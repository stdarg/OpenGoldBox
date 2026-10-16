# C++ coding style

## Scope

Use this style for project-owned C++ source and headers. Preserve the C++20,
Godot 4.x, and GDExtension architecture described in [TECH.md](TECH.md).
Exclude third-party, generated, build, and packaged files from formatting.
For design and correctness mistakes to avoid, see
[CPP_ANTIPATTERNS.md](CPP_ANTIPATTERNS.md).

This document records the agreed style and the tools that apply it:
**clang-format** for layout and **clang-tidy** for the one rule a formatter
cannot apply safely, required braces. Both come from the same LLVM release;
pin one release in editors and automated checks.

Most of the existing tree does not yet match: in October 2026, 263 of 322
project-owned C++ files differed from clang-format's output, and one-statement
`if` and loop bodies without braces are common. New and changed code follows
this style; bringing the rest in line is a separate, formatting-only change
(see [Adoption](#adoption)).

## Layout and readability

- Indent with four spaces; do not use tabs.
- Use a 100-column line limit. Let the formatter wrap expressions and argument
  lists; indivisible tokens and strings may exceed the limit.
- Put opening and closing braces on their own lines (Allman style).
- Require braces around every `if`, `else`, and loop body, including one-statement
  bodies. An `else if` chain may remain a chain.
- Write one statement per line. Expand short functions and conditionals instead
  of compressing them onto one line.
- Put spaces around binary operators and after commas. Use a space between
  control keywords and their opening parentheses: `if (condition)`.
- Remove trailing whitespace.
- Keep existing naming conventions. Formatting is not a reason to rename symbols.
- Preserve include order; the formatter does not sort includes, to avoid
  introducing include-order problems.
- Put a blank line before and after every function, class, struct, enum and
  namespace definition.

For example:

```cpp
bool attack_hits(int natural, int bonus, int ac) noexcept
{
    if (natural == 20)
    {
        return true;
    }

    return natural != 1 && static_cast<std::int64_t>(natural) + bonus >= ac;
}
```

## Engineering rules

- Use C++20, not compiler-specific "latest" language modes.
- Use RAII for every owned resource.
- Prefer values and standard RAII containers. When pointer ownership is needed,
  use `std::unique_ptr` by default; reserve `std::shared_ptr` for genuine shared
  ownership. Raw pointers and references are non-owning views only.
- Preserve architectural boundaries: SRD mechanics belong in
  `opengold_rules_srd5`; Core depends on rules interfaces and owns campaign
  orchestration and transactions; the application injects the rules implementation.
- Keep formatting changes separate from behavioral changes and refactoring.

clang-format handles layout, and clang-tidy checks braces; neither checks
architectural correctness or ownership, which stay a matter for review.

## Tools

Use **clang-format** and **clang-tidy** from one LLVM release. The repository
root holds their configuration, and each file's comments give the reason for
its less obvious options:

- `.clang-format`: layout. It never sorts includes, never splits string
  literals (tools/localization.py reads message templates straight out of the
  sources and would lose a split one), and never adds or removes braces.
- `.clang-tidy`: enables only `readability-braces-around-statements`, with
  one-statement bodies included. clang-tidy works from the compiled code, so
  unlike a formatter it knows which identifiers are macros and can add braces
  safely.

Validated with LLVM 23.1.0. Install that release, or another 23.x, and make
`clang-format`, `git-clang-format` and `clang-tidy` available on `PATH`:

- macOS: `brew install llvm`, then add `$(brew --prefix llvm)/bin` to `PATH`.
- Debian/Ubuntu: the `clang-format-23` and `clang-tidy-23` packages from
  [apt.llvm.org](https://apt.llvm.org/).
- Windows: `winget install LLVM.LLVM`, or a release from
  [github.com/llvm/llvm-project/releases](https://github.com/llvm/llvm-project/releases).

Check `clang-format --version` before formatting. Do not rely on a version
bundled with an editor: clang-format's output changes between major releases.
Upgrade deliberately, in a formatting-only change.

## Formatting

Format only what a change touches. `git clang-format` reformats the lines that
differ from a commit, so new work follows the style without reformatting
untouched code:

```
git clang-format --diff HEAD     # preview the staged and unstaged changes
git clang-format HEAD            # apply it
git diff --check
```

To format a whole file, or check one without editing it (a nonzero exit code
means it differs):

```
clang-format -i src/OpenGold.Rules.Srd5/src/srd5.cpp
clang-format --dry-run -Werror src/OpenGold.Rules.Srd5/src/srd5.cpp
```

Name the files explicitly. Never run the formatter recursively over the working
tree: it would reach godot-cpp and other third-party, generated and build files.

Reformatting moves source lines, so run `python tools/localization.py` and
`python tools/localization.py --check` after formatting, as after any source
change.

## Checking braces

clang-tidy reads the compile commands of a configured build. The top-level
CMakeLists.txt asks CMake to write `compile_commands.json` into the build
directory (Ninja and Makefile generators; Visual Studio does not write one).
Check a file, or let clang-tidy add the braces:

```
clang-tidy -p build src/OpenGold.Core/src/training_hall.cpp
clang-tidy -p build --fix src/OpenGold.Core/src/training_hall.cpp
```

On macOS, Homebrew's clang-tidy does not find Apple's SDK headers by itself;
add `--extra-arg=-isysroot$(xcrun --show-sdk-path)`. `.clang-tidy` sets
`FormatStyle: file`, so inserted braces follow `.clang-format` (Allman). Review
brace fixes on their own, separately from a formatting pass.

## Editor integration

Point the editor at the pinned clang-format binary and the repository's
`.clang-format` rather than a bundled copy: in VS Code, set
`C_Cpp.clang_format_path` (C/C++ extension) or `clangd.path`; CLion and Xcode
plug-ins take the same binary. Format-on-save reformats the whole file, so
enable it only for files already in the style, or format just the changed
lines with `git clang-format`.

## Adoption

1. Pin the LLVM release and keep `.clang-format` and `.clang-tidy` at the
   repository root (done).
2. Format new and changed code with `git clang-format` (from now on).
3. Bring the rest of the tree in line in a separate formatting-only commit over
   an explicit list of project-owned files, excluding third-party and generated
   files.
4. Add the missing braces with `clang-tidy --fix` in another separate commit.
5. After each of those commits, review the diff, run `git diff --check`,
   rebuild, run the tests and the localization check.
6. Then add an automated check (`clang-format --dry-run -Werror` and
   clang-tidy) so the tree does not drift.

## References

- [clang-format style options](https://clang.llvm.org/docs/ClangFormatStyleOptions.html)
- [clang-tidy](https://clang.llvm.org/extra/clang-tidy/) and
  [readability-braces-around-statements](https://clang.llvm.org/extra/clang-tidy/checks/readability/braces-around-statements.html)
