import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def strip_c_style(content):
    content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
    lines = []
    for line in content.split('\n'):
        in_string = False
        in_char = False
        result = []
        i = 0
        while i < len(line):
            c = line[i]
            if not in_string and not in_char:
                if i + 1 < len(line) and c == '/' and line[i+1] == '/':
                    break
                if c == '"':
                    in_string = True
                elif c == "'":
                    in_char = True
            else:
                if c == '\\' and i + 1 < len(line):
                    result.append(c)
                    result.append(line[i+1])
                    i += 2
                    continue
                if in_string and c == '"':
                    in_string = False
                elif in_char and c == "'":
                    in_char = False
            result.append(c)
            i += 1
        lines.append(''.join(result))
    return '\n'.join(lines)

def strip_hash(content, preserve_shebang=True):
    lines = content.split('\n')
    result = []
    for idx, line in enumerate(lines):
        stripped = line.lstrip()
        if preserve_shebang and idx == 0 and stripped.startswith('#!'):
            result.append(line)
            continue
        in_string = False
        in_char = False
        out = []
        i = 0
        while i < len(line):
            c = line[i]
            if not in_string and not in_char:
                if c == '#':
                    break
                if c == '"':
                    in_string = True
                elif c == "'":
                    in_char = True
            else:
                if c == '\\' and i + 1 < len(line):
                    out.append(c)
                    out.append(line[i+1])
                    i += 2
                    continue
                if in_string and c == '"':
                    in_string = False
                elif in_char and c == "'":
                    in_char = False
            out.append(c)
            i += 1
        result.append(''.join(out))
    return '\n'.join(result)

def strip_xml_comments(content):
    return re.sub(r'<!--.*?-->', '', content, flags=re.DOTALL)

def strip_semicolon(content):
    lines = content.split('\n')
    result = []
    for line in lines:
        in_string = False
        out = []
        i = 0
        while i < len(line):
            c = line[i]
            if not in_string:
                if c == ';':
                    break
                if c == "'":
                    in_string = True
            else:
                if c == "'":
                    in_string = False
            out.append(c)
            i += 1
        result.append(''.join(out))
    return '\n'.join(result)

def clean_blank_lines(content):
    lines = content.split('\n')
    result = []
    prev_blank = False
    for line in lines:
        is_blank = line.strip() == ''
        if is_blank and prev_blank:
            continue
        result.append(line)
        prev_blank = is_blank
    while result and result[-1].strip() == '':
        result.pop()
    return '\n'.join(result)

EXT_HANDLERS = {
    '.cs': strip_c_style,
    '.cpp': strip_c_style,
    '.h': strip_c_style,
    '.hpp': strip_c_style,
    '.c': strip_c_style,
    '.py': lambda c: strip_hash(c, True),
    '.ps1': lambda c: strip_hash(c, False),
    '.cmake': lambda c: strip_hash(c, False),
    '.toml': lambda c: strip_hash(c, False),
    '.iss': strip_semicolon,
    '.xaml': strip_xml_comments,
}

SKIP_DIRS = {'generated', 'thirdparty', 'out', 'beta-release', 'alpha-release', '.git', 'node_modules'}

def get_tracked_files():
    import subprocess
    result = subprocess.run(['git', 'ls-files'], capture_output=True, text=True, cwd=ROOT)
    return result.stdout.strip().split('\n') if result.stdout.strip() else []

def main():
    tracked = get_tracked_files()
    modified = 0
    for rel in tracked:
        if not rel or rel.endswith('.md'):
            continue
        skip = False
        for sd in SKIP_DIRS:
            if rel.startswith(sd + '/') or rel.startswith(sd + '\\'):
                skip = True
                break
        if skip:
            continue
        ext = os.path.splitext(rel)[1].lower()
        if ext not in EXT_HANDLERS:
            continue
        fpath = os.path.join(ROOT, rel.replace('/', os.sep))
        if not os.path.isfile(fpath):
            continue
        try:
            with open(fpath, 'r', encoding='utf-8-sig') as f:
                content = f.read()
        except:
            continue
        handler = EXT_HANDLERS[ext]
        stripped = handler(content)
        cleaned = clean_blank_lines(stripped)
        if cleaned != content:
            with open(fpath, 'w', encoding='utf-8') as f:
                f.write(cleaned)
                if not cleaned.endswith('\n'):
                    f.write('\n')
            modified += 1
            print(f"  Stripped: {rel}")
    print(f"\nDone. {modified} files modified.")

if __name__ == '__main__':
    main()
