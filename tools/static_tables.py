#!/usr/bin/env python3
"""Find function-local constexpr tables that are not static.

MSVC gives a lambda its own copy of a plain constexpr local array, so code
such as `find(table, x) != table.end()` inside the lambda compares iterators
into two different arrays and gets the wrong answer (docs/TESTING.md,
"Windows verification"). Clang and GCC share one array, so the bug only shows
on Windows. Declaring the table `static constexpr` gives it one address on
every compiler.

It also finds a test file that defines its own check() or rejects(): every
test takes them from tests/test_support.h, so a better failure message or a
counter there reaches every test.

    static_tables.py [--fix] PATH...

Without --fix it lists each offending declaration and exits 1 if there is
one; with --fix it inserts `static` in place. A test's own check() or
rejects() is always listed and fails the run; --fix leaves it alone.
"""

import pathlib
import re
import sys

SOURCE_SUFFIXES = {".cpp", ".h", ".hpp", ".cc"}

# A constexpr declaration of a std::array, a C array, or an auto deduced from
# std::array or std::to_array, starting its own line.
TABLE = re.compile(
    r"^(?P<indent>[ \t]*)constexpr\s+"
    r"(?:std::array\b"
    r"|auto\s+\w+\s*=\s*std::(?:array|to_array)\b"
    r"|[\w:<>, ]+?\s+\w+\s*\[)"
)

NON_FUNCTION_SCOPE = re.compile(r"\b(?:namespace|class|struct|union|enum)\b|extern\s*$")
CONSTANT_FUNCTION = re.compile(r"\b(?:constexpr|consteval)\b")
IF_CONSTEXPR = re.compile(r"\bif\s+constexpr\b")

# A definition of check() or rejects() in a test: a function with a return
# type (not a call after return or throw), or a lambda bound to the name.
LOCAL_ASSERTION = re.compile(
    r"^[ \t]*(?:template\s*<[^>]*>\s*)?(?:(?:static|inline|constexpr|\[\[nodiscard\]\])\s+)*"
    r"(?!(?:return|throw|co_return|else|case|new|delete)\b)[\w:<>]+[ \t]*[&*]*[ \t]+(?:check|rejects)\s*\("
    r"|\b(?:auto|std::function<[^>]*>)[ \t&]+(?:check|rejects)\s*=",
    re.M,
)
ASSERTION_HOME = "test_support.h"


def strip_comments_and_literals(text: str) -> str:
    """Blank out comments, strings and characters, keeping every newline."""
    out = []
    i = 0
    length = len(text)

    def blank(segment: str) -> str:
        return "".join("\n" if c == "\n" else " " for c in segment)

    while i < length:
        c = text[i]
        if text.startswith("//", i):
            end = text.find("\n", i)
            end = length if end < 0 else end
            out.append(blank(text[i:end]))
            i = end
        elif text.startswith("/*", i):
            end = text.find("*/", i + 2)
            end = length if end < 0 else end + 2
            out.append(blank(text[i:end]))
            i = end
        elif c == "R" and text.startswith('R"', i) and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == "_")):
            open_paren = text.find("(", i + 2)
            delimiter = text[i + 2:open_paren]
            end = text.find(")" + delimiter + '"', open_paren)
            end = length if end < 0 else end + len(delimiter) + 2
            out.append(blank(text[i:end]))
            i = end
        elif c == '"' or (c == "'" and not (i > 0 and text[i - 1].isalnum())):
            j = i + 1
            while j < length and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            out.append(blank(text[i:j + 1]))
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def function_local_lines(text: str) -> set[int]:
    """The 0-based lines that start inside a function, lambda or block, but
    not inside a constexpr function: C++20 forbids static locals there."""
    code = strip_comments_and_literals(text)
    scopes = []  # one per open brace: "block", "constexpr" or "other"
    statement_start = 0
    local = set()
    line = 0
    for at, c in enumerate(code):
        if c == "\n":
            line += 1
            if "block" in scopes and "constexpr" not in scopes:
                local.add(line)
        elif c == "{":
            head = code[statement_start:at]
            # A constexpr function, or a lambda run to initialize a constexpr
            # value, is evaluated at compile time. A function head has a
            # parameter list; `template <class T>` before one must not make it
            # look like a class.
            if CONSTANT_FUNCTION.search(IF_CONSTEXPR.sub("", head)):
                scopes.append("constexpr")
            elif ")" in head or not NON_FUNCTION_SCOPE.search(head):
                scopes.append("block")
            else:
                scopes.append("other")
            statement_start = at + 1
        elif c == "}":
            if scopes:
                scopes.pop()
            statement_start = at + 1
        elif c == ";":
            statement_start = at + 1
    return local


def offending_lines(text: str) -> list[int]:
    lines = text.split("\n")
    local = function_local_lines(text)
    return [n for n, source in enumerate(lines) if n in local and TABLE.match(source)]


def local_assertion_lines(path: pathlib.Path, text: str) -> list[int]:
    """The 0-based lines where a test file defines its own check() or
    rejects()."""
    if "tests" not in path.parts or path.name == ASSERTION_HOME:
        return []
    code = strip_comments_and_literals(text)
    return [code.count("\n", 0, match.start()) for match in LOCAL_ASSERTION.finditer(code)]


def source_files(paths: list[str]):
    for path in map(pathlib.Path, paths):
        if path.is_file():
            yield path
        else:
            yield from sorted(p for p in path.rglob("*") if p.suffix in SOURCE_SUFFIXES and p.is_file())


def main(arguments: list[str]) -> int:
    fix = "--fix" in arguments
    paths = [a for a in arguments if a != "--fix"]
    if not paths:
        print(__doc__)
        return 2
    found = 0
    assertions = 0
    for path in source_files(paths):
        text = path.read_bytes().decode("utf-8")
        for n in local_assertion_lines(path, text):
            found += 1
            assertions += 1
            print(f"{path}:{n + 1}: test defines its own check() or rejects(); use tests/{ASSERTION_HOME}")
        offending = offending_lines(text)
        if not offending:
            continue
        found += len(offending)
        lines = text.split("\n")
        for n in offending:
            if fix:
                lines[n] = TABLE.sub(lambda m: m.group(0).replace("constexpr", "static constexpr", 1), lines[n], count=1)
            else:
                print(f"{path}:{n + 1}: function-local constexpr table should be static constexpr")
        if fix:
            path.write_bytes("\n".join(lines).encode("utf-8"))
    if fix:
        print(f"made {found - assertions} tables static")
        return 1 if assertions else 0
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
