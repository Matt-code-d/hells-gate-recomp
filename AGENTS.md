# AGENTS.md - Project guide for AI agents

## Project

Static recompilation port of **Dante's Inferno** (Xbox 360) to native PC using
the ReXGlue SDK (v0.10.0). ReXGlue translates PowerPC XEX -> C++ ahead of time.

## Key paths

- `dantes_inferno_manifest.toml` - ReXGlue project manifest (SDK-managed; regen with `rexglue init --force`)
- `generated/rexglue.cmake` - SDK build boilerplate (auto-generated, DO NOT EDIT)
- `generated/default/` - codegen output (gitignored, produced during build)
- `src/dantes_inferno_app.h` - **user-owned** app class; override ReXApp hooks here
- `src/main.cpp` - entry point (SDK-managed, preserved on first init only)
- `game/` - extracted Xbox 360 game files (gitignored, copyrighted - never commit)
- `thirdparty/rexglue-sdk/` - SDK clone (gitignored, via setup.ps1)
- `docs/rexglue_notes.md` - ReXGlue workflow & command reference
- `docs/ultrawide_research.md` - Ultrawide support RE findings & implementation plan
- `tools/asset_tool.py` - asset extraction/packing tool (BIG/STR/TG4D/VP6)
- `tools/ASSET_TOOL_README.md` - asset tool documentation
- `tools/Gibbed.Visceral/` - format reference source (gitignored, Zlib license)
- `patches/` - local patches for SDK and generated code (tracked in git)
  - `patches/sdk/rexglue-sdk-v0.10.0.patch` - all SDK modifications
  - `patches/apply_sdk_patches.ps1` - applies SDK patches after clone
  - `patches/generated/apply_generated_patches.py` - applies fiber/setjmp/longjmp
    edits to generated code after codegen (must be re-run after each regen)

## Build commands (Windows)

```powershell
# One-time: build the rexglue CLI from the SDK
cmake --preset win-amd64-release -DREXSDK_DIR=thirdparty\rexglue-sdk
cmake --build out\build\win-amd64-release --target rexglue

# Regenerate SDK-managed files (requires game/default.xex present)
rexglue init --force --project_name dantes_inferno --project_root . --xex_path game\default.xex --game_root game

# Build the port (codegen runs automatically as a build dependency)
cmake --preset win-amd64-release -DREXSDK_DIR=thirdparty\rexglue-sdk
cmake --build out\build\win-amd64-release
```

Run: `out\win-amd64\Release\dantes_inferno.exe`

## Toolchain

- Clang 18+ required (NOT MSVC/GCC). Detected: Clang 22 at `C:\Program Files\LLVM\bin\clang.exe`
- CMake 3.25+, Ninja, Visual Studio 2022 (Windows SDK for D3D12)
- C++23, D3D12 graphics backend on Windows

## Conventions

- `src/dantes_inferno_app.h` is the ONLY place for custom app behavior. Do not
  edit `main.cpp` or `generated/rexglue.cmake` - they are SDK-managed and get
  overwritten by `rexglue init`/`rexglue migrate`.
- For per-instruction custom C++ injection, use `[[mid_asm_hooks]]` in the
  manifest (see docs/rexglue_notes.md).
- Game assets under `game/` are copyrighted and gitignored. Never commit them.
- The SDK under `thirdparty/rexglue-sdk/` is gitignored; re-clone via `setup.ps1`.

## Naming

Project name `dantes_inferno` -> snake_case `dantes_inferno`, PascalCase
`DantesInferno`, UPPER `DANTESINFERNO`. CMake target: `dantes_inferno`.

## Improvement plan

See `docs/improvements_plan.md` for full research findings. Summary:

1. **Graphics quality** (Phase 1, no RE): resolution_scale, anisotropic_override,
   swap_post_effect=fxaa cvars in OnPreSetup. Optionally FidelityFX FSR build.
2. **Input config** (Phase 2, no RE): SDL backend + MnK keybind defaults in
   OnPreSetup. DualShock/DualSense/Xbox all supported via SDL3.
3. **DLC auto-install** (Phase 3, no RE): OnPostSetup hook scans dlc/ folder,
   calls ContentManager::InstallContent() on each STFS package.
4. **Ultrawide** (Phase 4, requires RE): midasm_hook on projection matrix to
   patch aspect ratio. Hor+ anamorphic render strategy.
5. **Button glyphs** (Phase 5, requires RE): replace game's button prompt
   textures based on active input device. Needs SDK patch for device detection
   or glyph_family cvar. Glyph art in metadata/glyphs/.
6. **Installer** (Phase 6): asks user for ISO + DLC folder, extracts to game/
   and dlc/. No STFS logic in installer.

~~Known blocker: ReXGlue issue #75 — Dante's Inferno crashes at startup due to
unimplemented VMX/Altivec PPC instructions (v0.1.1).~~ **Resolved in v0.10.0.**
Game boots and runs. VMX builder bugs causing FMV corruption were found and
fixed (see `docs/vp6_fmv_corruption_fix.md`).

## SDK patches

The SDK under `thirdparty/rexglue-sdk/` has local patches to
`src/codegen/builders/vector.cpp` that fix VMX instruction builder bugs.
Full technical documentation: `docs/vp6_fmv_corruption_fix.md`.

If the SDK is re-cloned, these patches must be re-applied. The fixes are:

1. **`vpkuwus` / `vpkuhus` in-place aliasing** (root cause of FMV corruption):
   Element-by-element packing loops aliased the destination's narrowed array
   with the source's wider array. Replaced with SSE intrinsics.
2. **`vmsum3fp128` dot product mask** (`0x7F` → `0xEF`): A previous "fix"
   incorrectly changed the mask from `0xEF` to `0x7F`, which excluded PPC
   element 0 (X component) from 3-element dot products instead of excluding
   PPC element 3 (W). This broke physics/collision code, causing the
   character to fall through the map after the opening cutscene. Reverted
   to `0xEF` after the `ppc_tests` test suite caught the regression.
3. **Pack builder `unpackhi_epi64` removal**: Pack builders discarded half
   the packed elements via `unpackhi_epi64`. Removed and operand-swapped for
   byte reversal.

## Build & run notes

- The executable loads `rexgpu-xenos.dll` from its own directory, not from
  the SDK output. After rebuilding the SDK, copy:
  `thirdparty\rexglue-sdk\out\win-amd64\rexgpu-xenos.dll` →
  `out\build\win-amd64-release\rexgpu-xenos.dll`
- Always launch with `--game_data_root=game` from the project root.
- Runtime logs are in `out\build\win-amd64-release\logs\`.
- After changing SDK codegen builders, delete the stale generated files to
  force regeneration. The codegen stamp does NOT track `rexglue.exe` as a
  dependency, so you must delete the stamps AND the generated files, then
  reconfigure CMake and rebuild:
  ```powershell
  Remove-Item generated\default\codegen.* -Force
  Remove-Item generated\default\dantes_inferno_recomp.*.cpp -Force
  Remove-Item generated\default\dantes_inferno_recomp.*.h -Force
  Remove-Item generated\default\sources.cmake -Force
  cmake --preset win-amd64-release -DREXSDK_DIR=thirdparty\rexglue-sdk
  cmake --build out\build\win-amd64-release
  python patches\generated\apply_generated_patches.py
  ```
- The SDK's PPC instruction test suite (`ppc_tests`) can be built and run
  to verify codegen builder correctness:
  ```powershell
  cd thirdparty\rexglue-sdk
  cmake --preset win-amd64 -DREXGLUE_BUILD_TESTS=ON
  cmake --build out\build\win-amd64 --target ppc_tests --config Release
  .\out\win-amd64\Release\ppc_tests.exe
  ```

## Save system

The save system uses a setjmp/longjmp pair that requires C setjmp/longjmp
to unwind the C++ call stack. See `src/dantes_inferno_hooks.h` for the
fiber support functions and `docs/code_changes.md` for full details.

Key components:
- `[[midasm_hook]]` in manifest injects `ZeroFiberSwitchCallback()` at
  `sub_82701240` (guest setjmp)
- `OnPreLaunchModule` zeroes the fiber-switch callback at `0x82B101E4`
- Manual generated-code edits in `.24`, `.38`, `.45`, `.70` add C
  setjmp/longjmp calls (lost on codegen regen, must be re-applied)
- `XUserFindUsers` handler in `xlivebase_app.cpp` returns success to
  prevent null-pointer crash when loading saves

## Title Update 2 (TU2) and playable DLC

The game requires Title Update 2 (TU2) to enable playable DLC content
like Trials of Saint Lucia. The TU2 patch is applied at runtime via
`game/default.xexp` (the patched XEX). The runtime reports:
`XEX patch applied successfully: base version: 0.0.0.3, new version: 0.0.2.3`

### TU2-patched entry point

The TU2 patch changes the XEX entry point from `0x826A6790` (original) to
`0x8281DAC8` (TU2). The codegen explicitly registers this entry point via
`registerEntryPoints()` in `thirdparty/rexglue-sdk/src/codegen/phase_register.cpp`.

### TU2-patched functions in manifest

The TU2 patch introduces new function entry points that are called through
indirect calls but not discovered by PDATA or function pointer scanning.
These are registered in the manifest under `[entrypoint.functions.0xADDR]`:

```toml
[entrypoint.functions.0x8236E3C0]
[entrypoint.functions.0x825D2C30]
# ... (see dantes_inferno_manifest.toml for full list)
```

### Unresolved call patching

The codegen may emit `REX_FATAL("Unresolved call from 0xSITE to 0xTARGET")`
for branches/calls that are not connected to a `CallTarget` in the
`FunctionNode` graph. The `patches/generated/fix_unresolved_calls.py` script
post-processes generated code to replace these fatal traps with:
- Direct function calls (if the target is registered)
- `goto` labels (if the target is a label in the same function)

### Full TU2 rebuild workflow

```powershell
# 1. Ensure game/default.xexp is present (TU2 patch)
# 2. Delete generated files
Remove-Item generated\default\* -Force
# 3. Run codegen
rexglue --force codegen dantes_inferno_manifest.toml --ignore-stamp
# 4. Apply fiber/setjmp/longjmp patches
python patches\generated\apply_generated_patches.py
# 5. Apply unresolved-call patches
python patches\generated\fix_unresolved_calls.py
# 6. Reconfigure and build
cmake --preset win-amd64-release -DREXSDK_DIR=thirdparty\rexglue-sdk
cmake --build out\build\win-amd64-release
# 7. Run with TU2 enabled (default.xexp must be in game/)
out\build\win-amd64-release\dantes_inferno.exe --game_data_root=game --dlc_trace=true
```

### DLC verification

- Item DLC (costumes/relics/souls): accessed via `\Device\Content\1-18`
- Trials of Saint Lucia: accessed via `\Device\Content\33` (fe_arena.vp6)
- The game runs at 240 FPS with 0 unresolved function calls when all
  TU2 functions are properly registered

## Asset extraction tool

`tools/asset_tool.py` extracts and repacks game assets for upscaling. See
`tools/ASSET_TOOL_README.md` for full documentation.

```powershell
# List archive contents
python tools/asset_tool.py list game/bigfile0.viv

# Full pipeline: extract + unpack STR + convert textures to PNG
python tools/asset_tool.py extract game/bigfile0.viv output --full-pipeline

# Extract just videos
python tools/asset_tool.py extract game/bigfile0.viv output --type videos

# Unpack/repack STR files
python tools/asset_tool.py unpack-str input.str output_dir/
python tools/asset_tool.py pack-str input_dir/ output.str

# Convert textures (TG4D <-> DDS/PNG)
python tools/asset_tool.py convert-texture tex.tg4d tex.png --tg4h tex.tg4h
python tools/asset_tool.py make-texture upscaled.png out.tg4d --format dxt5

# Pack BIG archive
python tools/asset_tool.py pack-big input_dir/ output.viv
```

Dependencies: `pip install pillow texture2ddecoder`

Formats supported: BIG/VIV (BIGH), STR (StreamSet), TG4D/TG4H (DXT1/DXT5),
VP6 video, EAGM mesh (raw extraction). RefPack compression/decompression
implemented in pure Python.

## Current status

- [x] SDK cloned at v0.10.0
- [x] Project scaffolding created from ReXGlue v0.10.0 init templates
- [x] GitHub repo created: https://github.com/florinp93/dantes-inferno (private)
- [x] Game ISO + DLC file placed in disc/ (ISO 7.8GB, DLC is STFS LIVE package)
- [x] Improvement research completed (docs/improvements_plan.md)
- [x] SDK submodules initialized & CLI built
- [x] Game ISO extracted into `game/` with `default.xex` entrypoint
- [x] `rexglue init --force` run to stamp SDK-managed files
- [x] First successful codegen + build
- [x] VMX/AltiVec issue #75 resolved (v0.10.0 has full VMX support)
- [x] VP6/Bink FMV corruption diagnosed and fixed (docs/vp6_fmv_corruption_fix.md)
- [x] SDK patches submitted upstream (PR #426)
- [x] Physics/collision bug fixed: vmsum3fp128 dot product mask reverted
      from 0x7F to 0xEF (character was falling through map after cutscene)
- [x] Save system fixed: fiber/setjmp/longjmp support, midasm_hook,
      XUserFindUsers handler, OnPreLaunchModule patch
- [x] Asset extraction tool built (tools/asset_tool.py): BIG/VIV parsing,
      STR unpacking/packing, RefPack, TG4D/DXT texture conversion, VP6
      extraction, EAGM mesh extraction
- [x] Graphics quality cvars configured in OnPreSetup
- [x] MnK keybind defaults configured in OnPreSetup
- [x] TU2 patch applied: entry point registration, manifest functions,
      unresolved-call patching, fiber patches for TU2 patterns
- [x] Trials of Saint Lucia DLC detected: fe_arena.vp6 accessed from
      \Device\Content\33, game runs at 240 FPS with 0 unresolved calls
- [x] Item DLC (costumes/relics/souls) verified working alongside Trials DLC
- [x] DLC auto-install hook in OnPostSetup (scans dlc/ folder, calls
      ContentManager::InstallContent on each STFS package)
- [x] TU2 patch bundled with installer (default.xexp copied to game/ folder)
- [x] Launcher DLC tab: Open DLC Folder / Open TU Folder buttons,
      DLC count and TU status display
- [ ] Ultrawide projection hook (requires RE of generated code)
- [ ] Button glyph replacement (requires RE of generated code)

## DLC dispatch investigation

- `0x821671F0` is inside the guest `.pdata` section. Its pair
  `(0x8266E508, 0x40001204)` is unwind metadata, not a class/method registration.
- The earlier `CModule::makeCurrent` identification of `sub_8266E508` was
  incorrect. Live `.rdata` inspection resolves the associated name at
  `0x8200CEF8` to `{EndCaptureFileCreation}`. `sub_8266E5A8` builds eight
  68-byte name/function records for capture commands and dispatches at
  `0x8266E840`; it is not evidence of a missing DLC activation path.
  Existing diagnostic labels `set-current` and `activation-gate` came from
  that disproven interpretation and must not be used to diagnose DLC.
- `--dlc_trace=true` enables bounded, read-only probes. The authoritative DLC
  read probe is `DLC-IO-TRACE` in the SDK; older manifest probes include the
  unrelated capture-command path. Default is off.
- The playable-content reproduction in `dantes_inferno_199.log` issues reads
  for `dia_core.dlm` and `dia_core_xen.dlm`, but not `dia_core.lu2`. The latter
  contains `SetupEditorGameFlow()` registering the editor/arena episodes.
  These reads do not hit the `sub_826AB7C0` probe, so that wrapper does not
  cover this scan path. `DLC-IO-TRACE` in the SDK's `NtReadFile_entry` now
  captures completion status, byte count, and guest stack links for up to 32
  `.dlm`/`.lu2` reads under the same `--dlc_trace=true` flag. This diagnostic
  is persisted in `patches/sdk/rexglue-sdk-v0.10.0.patch`.
- `dantes_inferno_200.log` confirms successful `.dlm` reads (106 and 1462
  bytes, status zero). The stack is `82507B58 -> 82505A58 -> 8250A1A0 ->
  825065B8 -> 82505B38 -> 82509610 -> 825070D0 -> 826A7D50 -> NtReadFile`.
- The live DLC manager at `*[0x82AF89A0]` has seven handlers at `manager+8`,
  count at `manager+40`: `.rep`, `.add`, `.key`, `.ltx`, `.lua`, `.dlm`, `.lds`.
  `sub_82284E18` registers them at `0x82286864..0x822868AC` through
  `sub_825057A8`. No `.lu2` handler is registered. `sub_82298628` returns
  `.lua` at `0x82124514`; its handler vtable is `0x8212C3A8`, with read
  callback `sub_82509280` at slot +32.
- The actual `.dlm` handler vtable is `0x8213F070`. Getter `sub_82507CA0`
  returns `.dlm` at `0x8213EFD4`; read callback `sub_82507EF0` parses the
  version header and file mappings. It does not register playable episodes.
- The missing `.lu2` loading support is a verified mismatch with the installed
  Saint Lucia package. A title-update comparison is the next investigation,
  not yet proof of which update or additional engine changes are required.
- `generated/default/codegen.partition.json` maps guest function starts to
  recomp file numbers. Use it to locate functions in ignored generated files;
  the nearest preceding start is only a candidate until the function body is read.
- Guest-image dumping is separately opt-in with `--dlc_dump_image=true`.
- After regenerating hooks, run `python patches/generated/apply_generated_patches.py`
  before compiling the executable to retain the save-system fiber patches.
