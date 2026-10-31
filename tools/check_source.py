from __future__ import annotations

import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE_DIRS = (ROOT / "engine", ROOT / "apps", ROOT / "tests")
CPP_SUFFIXES = {".h", ".hpp", ".inl", ".cpp", ".c", ".cc"}
DEFAULT_LIMITS = {".cpp": 600, ".c": 600, ".h": 400, ".hpp": 400, ".inl": 400}
CMAKE_LIMIT = 400
BANNED = re.compile(r"\b(?:TODO|FIXME|XXX|HACK|stub|placeholder)\b", re.IGNORECASE)
INCLUDE = re.compile(r'#\s*include\s*[<"]caligo/([A-Za-z0-9_]+)/[^>"]+[>"]')
MODULE_DEPENDENCIES = {
    "core": set(),
    "platform": {"core"},
    "assets": {"core", "platform"},
    "render": {"core", "platform"},
    "scene": {"core", "assets"},
    "physics": {"core", "scene"},
    "audio": {"core", "assets"},
    "animation": {"core", "scene"},
    "script": {"core", "scene", "physics", "audio"},
    "game": {
        "core",
        "platform",
        "render",
        "assets",
        "scene",
        "physics",
        "audio",
        "animation",
        "script",
    },
    "editor": {
        "core",
        "platform",
        "render",
        "assets",
        "scene",
        "physics",
        "audio",
        "animation",
        "script",
        "game",
    },
}


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def line_count(text: str) -> int:
    if not text:
        return 0
    return text.count("\n") + (0 if text.endswith("\n") else 1)


def blank(chars: list[str], start: int, end: int) -> None:
    for index in range(start, end):
        if chars[index] != "\n":
            chars[index] = " "


def mask_cpp_strings(text: str) -> str:
    chars = list(text)
    length = len(text)
    index = 0

    while index < length:
        if text.startswith('R"', index):
            cursor = index + 2
            while cursor < length and cursor - (index + 2) <= 16 and text[cursor] not in '()\\ \t\r\n':
                cursor += 1
            if cursor < length and text[cursor] == "(":
                delimiter = text[index + 2 : cursor]
                closing = ")" + delimiter + '"'
                end = text.find(closing, cursor + 1)
                if end < 0:
                    blank(chars, index, length)
                    break
                blank(chars, index, end + len(closing))
                index = end + len(closing)
                continue

        if text[index] == '"' or text[index] == "'":
            quote = text[index]
            cursor = index + 1
            while cursor < length:
                if text[cursor] == "\\":
                    cursor += 2
                    continue
                if text[cursor] == quote:
                    cursor += 1
                    break
                cursor += 1
            blank(chars, index, min(cursor, length))
            index = cursor
            continue

        index += 1

    return "".join(chars)


def cpp_comment_lines(text: str) -> list[int]:
    masked = mask_cpp_strings(text)
    return [masked.count("\n", 0, match.start()) + 1 for match in re.finditer(r"//|/\*", masked)]


def cmake_comment_lines(text: str) -> list[int]:
    lines: list[int] = []
    for number, line in enumerate(text.splitlines(), 1):
        in_string = False
        escaped = False
        for character in line:
            if escaped:
                escaped = False
                continue
            if character == "\\":
                escaped = True
                continue
            if character == '"':
                in_string = not in_string
                continue
            if character == "#" and not in_string:
                lines.append(number)
                break
    return lines


def source_files() -> list[Path]:
    files: list[Path] = []
    for base in SOURCE_DIRS:
        if not base.exists():
            continue
        for path in sorted(base.rglob("*")):
            if path.is_file() and path.suffix in CPP_SUFFIXES:
                files.append(path)
    return files


def cmake_files() -> list[Path]:
    files: list[Path] = [ROOT / "CMakeLists.txt"]
    cmake_dir = ROOT / "cmake"
    if cmake_dir.exists():
        files.extend(sorted(cmake_dir.glob("*.cmake")))
    for base in SOURCE_DIRS:
        if base.exists():
            files.extend(sorted(base.rglob("CMakeLists.txt")))
    return [path for path in files if path.is_file()]


def module_of(path: Path) -> str | None:
    try:
        relative = path.relative_to(ROOT / "engine")
    except ValueError:
        return None
    return relative.parts[0] if relative.parts else None


def load_allowlist() -> tuple[dict[str, tuple[int, str]], list[str]]:
    path = ROOT / "tools" / "limite_arquivos.txt"
    entries: dict[str, tuple[int, str]] = {}
    errors: list[str] = []
    if not path.exists():
        return entries, errors

    for number, line in enumerate(read_text(path).splitlines(), 1):
        stripped = line.strip()
        if not stripped:
            continue
        parts = [part.strip() for part in stripped.split("|")]
        if len(parts) != 3 or not parts[0] or not parts[2]:
            errors.append(f"tools/limite_arquivos.txt:{number}: expected path|limit|reason")
            continue
        try:
            limit = int(parts[1])
        except ValueError:
            errors.append(f"tools/limite_arquivos.txt:{number}: limit is not an integer")
            continue
        if limit <= 0:
            errors.append(f"tools/limite_arquivos.txt:{number}: limit must be positive")
            continue
        entries[parts[0]] = (limit, parts[2])
    return entries, errors


def check_format(files: list[Path]) -> list[str]:
    executable = shutil.which("clang-format")
    if executable is None:
        return ["clang-format was not found on PATH"]
    result = subprocess.run(
        [executable, "--dry-run", "--Werror", *[str(path) for path in files]],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode == 0:
        return []
    output = (result.stdout + result.stderr).strip()
    return output.splitlines() if output else ["clang-format rejected the source files"]


def main() -> int:
    errors: list[str] = []
    allowlist, allowlist_errors = load_allowlist()
    errors.extend(allowlist_errors)

    files = source_files()
    for path in files:
        try:
            text = read_text(path)
        except UnicodeDecodeError:
            errors.append(f"{path.relative_to(ROOT)}: file is not valid UTF-8")
            continue

        relative = path.relative_to(ROOT).as_posix()
        lines = line_count(text)
        suffix = path.suffix
        default_limit = DEFAULT_LIMITS[suffix]
        limit = allowlist.get(relative, (default_limit, ""))[0]

        if lines == 0:
            errors.append(f"{relative}: source file is empty")
        if lines > limit:
            errors.append(f"{relative}: {lines} lines exceeds the limit of {limit}")
        if relative in allowlist and lines <= default_limit:
            errors.append(f"{relative}: allowlist entry is no longer needed")

        for line in cpp_comment_lines(text):
            errors.append(f"{relative}:{line}: comments are forbidden")
        for match in BANNED.finditer(text):
            line = text.count("\n", 0, match.start()) + 1
            errors.append(f"{relative}:{line}: forbidden marker {match.group(0)}")

        module = module_of(path)
        if module is not None:
            dependencies = MODULE_DEPENDENCIES.get(module)
            if dependencies is None:
                errors.append(f"{relative}: unknown engine module {module}")
            else:
                allowed = dependencies | {module}
                for match in INCLUDE.finditer(text):
                    included = match.group(1)
                    if included not in allowed:
                        errors.append(f"{relative}: module {module} cannot include {included}")
                    if "/src/" in match.group(0):
                        errors.append(f"{relative}: private src include is forbidden")

    for path in cmake_files():
        relative = path.relative_to(ROOT).as_posix()
        text = read_text(path)
        lines = line_count(text)
        if lines == 0:
            errors.append(f"{relative}: build file is empty")
        if lines > CMAKE_LIMIT:
            errors.append(f"{relative}: {lines} lines exceeds the limit of {CMAKE_LIMIT}")
        for line in cmake_comment_lines(text):
            errors.append(f"{relative}:{line}: comments are forbidden")
        for match in BANNED.finditer(text):
            line = text.count("\n", 0, match.start()) + 1
            errors.append(f"{relative}:{line}: forbidden marker {match.group(0)}")

    for relative in allowlist:
        if not (ROOT / relative).is_file():
            errors.append(f"tools/limite_arquivos.txt: obsolete entry for {relative}")

    errors.extend(check_format(files))

    if errors:
        for error in errors:
            print(f"error: {error}")
        print(f"{len(errors)} problem(s) found")
        return 1

    print(f"source check passed: {len(files)} source files and {len(cmake_files())} build files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
