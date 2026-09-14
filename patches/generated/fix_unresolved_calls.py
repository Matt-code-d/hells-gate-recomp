#!/usr/bin/env python3
"""
Patch generated code to replace REX_FATAL unresolved call/branch traps
with proper function calls to the target functions.

This is needed because the codegen's validation phase reports some calls
as unresolved even though the target functions exist in the register file.
The --force flag lets codegen proceed, but the generated code contains
REX_FATAL traps that crash at runtime.

This script replaces those traps with proper calls.
"""

import os
import re
import sys
import glob

def main():
    project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    gen_dir = os.path.join(project_root, 'generated', 'default')

    if not os.path.isdir(gen_dir):
        print(f"ERROR: Generated directory not found: {gen_dir}", file=sys.stderr)
        sys.exit(1)

    # Read the register file to find all registered function addresses
    register_file = os.path.join(gen_dir, 'dantes_inferno_register.cpp')
    if not os.path.exists(register_file):
        print(f"ERROR: Register file not found: {register_file}", file=sys.stderr)
        sys.exit(1)

    with open(register_file, 'r', encoding='utf-8') as f:
        reg_content = f.read()

    # Extract all registered function addresses and names
    # Pattern: registrar->SetFunction(0xADDRESS, func_name);
    registered_funcs = {}
    for match in re.finditer(r'registrar->SetFunction\(0x([0-9A-Fa-f]+),\s*(\w+)\)', reg_content):
        addr = match.group(1).upper()
        name = match.group(2)
        registered_funcs[addr] = name

    print(f"Found {len(registered_funcs)} registered functions")

    # Patterns to fix:
    # 1. Unconditional branch (tail call):
    #    // b 0xTARGET
    #    // FATAL: unresolved function 0xTARGET (no CallTarget in FunctionNode)
    #    REX_FATAL("Unresolved call from 0xSITE to 0xTARGET");
    #    return;
    #
    # 2. Conditional branch:
    #    // beq 0xTARGET
    #    if (ctx.cr0.eq) REX_FATAL("Unresolved branch from 0xSITE to 0xTARGET");

    patched_count = 0
    skipped_count = 0
    # Track which files need which forward declarations
    file_decls_needed = {}  # filepath -> set of function names

    for filepath in sorted(glob.glob(os.path.join(gen_dir, 'dantes_inferno_recomp.*.cpp'))):
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        original_content = content
        decls_needed = set()

        # Fix unconditional branches (tail calls)
        # Pattern: // b 0xTARGET\n// FATAL: unresolved function 0xTARGET...\nREX_FATAL("Unresolved call from 0xSITE to 0xTARGET");\nreturn;
        # Note: addresses may be different cases (lowercase in b, uppercase in FATAL)
        # Note: lines may have leading tabs/whitespace
        pattern1 = re.compile(
            r'// b 0x([0-9A-Fa-f]+)\n'
            r'\s*// FATAL: unresolved function 0x[0-9A-Fa-f]+[^\n]*\n'
            r'\s*REX_FATAL\("Unresolved call from 0x[0-9A-Fa-f]+ to 0x([0-9A-Fa-f]+)"\);\n'
            r'\s*return;'
        )
        def replace1(m):
            nonlocal patched_count, skipped_count
            target = m.group(1).upper()
            target2 = m.group(2).upper()
            if target != target2:
                skipped_count += 1
                return m.group(0)
            if target in registered_funcs:
                func_name = registered_funcs[target]
                patched_count += 1
                decls_needed.add(func_name)
                return f'// b 0x{target}\n\t{func_name}(ctx, base);\n\treturn;'
            else:
                skipped_count += 1
                return m.group(0)
        content = pattern1.sub(replace1, content)

        # Fix conditional branches
        # Pattern: if (ctx.cr0.eq) REX_FATAL("Unresolved branch from 0xSITE to 0xTARGET");
        # Also handle other condition registers (cr6, cr7, etc.)
        # First check if the target is a label in the same file (loc_TARGET)
        # If not, check if it's a registered function
        pattern2 = r'if \(ctx\.cr(\d)\.(\w+)\) REX_FATAL\("Unresolved branch from 0x[0-9A-Fa-f]+ to 0x([0-9A-Fa-f]+)"\);'
        def replace2(m):
            nonlocal patched_count, skipped_count
            cr_num = m.group(1)
            cr_cond = m.group(2)
            target = m.group(3).upper()
            # Check if loc_TARGET exists in the current content
            loc_name = f'loc_{target}'
            if re.search(rf'\b{re.escape(loc_name)}\s*:', content):
                patched_count += 1
                return f'if (ctx.cr{cr_num}.{cr_cond}) goto {loc_name};'
            elif target in registered_funcs:
                func_name = registered_funcs[target]
                patched_count += 1
                decls_needed.add(func_name)
                return f'if (ctx.cr{cr_num}.{cr_cond}) {{ {func_name}(ctx, base); return; }}'
            else:
                skipped_count += 1
                return m.group(0)
        content = re.sub(pattern2, replace2, content)

        # Also handle negated conditions: if (!ctx.cr0.eq) REX_FATAL(...)
        pattern3 = r'if \(!ctx\.cr(\d)\.(\w+)\) REX_FATAL\("Unresolved branch from 0x[0-9A-Fa-f]+ to 0x([0-9A-Fa-f]+)"\);'
        def replace3(m):
            nonlocal patched_count, skipped_count
            cr_num = m.group(1)
            cr_cond = m.group(2)
            target = m.group(3).upper()
            # Check if loc_TARGET exists in the current content
            loc_name = f'loc_{target}'
            if re.search(rf'\b{re.escape(loc_name)}\s*:', content):
                patched_count += 1
                return f'if (!ctx.cr{cr_num}.{cr_cond}) goto {loc_name};'
            elif target in registered_funcs:
                func_name = registered_funcs[target]
                patched_count += 1
                decls_needed.add(func_name)
                return f'if (!ctx.cr{cr_num}.{cr_cond}) {{ {func_name}(ctx, base); return; }}'
            else:
                skipped_count += 1
                return m.group(0)
        content = re.sub(pattern3, replace3, content)

        # Add forward declarations for functions that aren't already declared
        if decls_needed:
            # Only check for existing DECLARE_REX_FUNC in the content (from the header include)
            # The .cpp file itself doesn't have DECLARE_REX_FUNC lines - those are in the .h file
            # Just add declarations for all patched functions; duplicate declarations are harmless
            new_decls = decls_needed
            if new_decls:
                # Add declarations after the include line
                decl_text = '\n// Forward declarations for unresolved call patches\n'
                for name in sorted(new_decls):
                    decl_text += f'DECLARE_REX_FUNC({name});\n'
                content = re.sub(r'(#include "[^"]+")', r'\1' + decl_text, content, count=1)

        if content != original_content:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)

    print(f"Patched {patched_count} unresolved calls/branches")
    print(f"Skipped {skipped_count} (target not registered)")

if __name__ == '__main__':
    main()
