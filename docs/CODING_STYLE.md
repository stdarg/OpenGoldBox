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

The whole project-owned tree follows this style: every tracked C++ file was
formatted with clang-format 23.1.0 (5a7187c1) and every `if`, `else` and loop
body braced with clang-tidy (5b5c199b), each in a commit of its own. Keep it
that way: format every change, and keep large formatting passes in separate
commits.

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

Format what a change touches. `git clang-format` reformats the lines that
differ from a commit:

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

## Pre-commit hook

`tools/git-hooks/pre-commit` applies both tools to the C++ files a commit
stages. Turn it on once per clone; worktrees share the setting:

```
git config core.hooksPath tools/git-hooks
```

On each commit that stages C++ files it:

1. Stops if `clang-format` or `git-clang-format` is missing, or is not LLVM 23,
   since another major release would reformat lines nobody changed.
2. Formats the staged lines (`git clang-format --staged`) and stages the
   result. If a file to format also has unstaged changes, re-staging it would
   commit those too, so instead it shows the formatting and stops; run
   `git clang-format --staged`, stage the result and commit again.
3. Runs clang-tidy on each staged file and stops on a missing brace. It uses
   the first of `build`, `build-game`, `build/debug` and `build/macos-universal`
   that has a `compile_commands.json`. Without clang-tidy or such a build, and
   for a header that only compiles inside the file including it, it says the
   braces were not checked and lets the commit through.

`git commit --no-verify` skips the hook for one commit.

## Editor integration

Point the editor at the pinned clang-format binary and the repository's
`.clang-format` rather than a bundled copy: in VS Code, set
`C_Cpp.clang_format_path` (C/C++ extension) or `clangd.path`; CLion and Xcode
plug-ins take the same binary. Since the tree is already in the style,
format-on-save is safe.

## Whole-tree passes

A pass over every file, such as a clang-format upgrade, goes in a commit of its
own over the tracked files (`git ls-files '*.cpp' '*.h'`). Review the diff, run
`git diff --check`, rebuild the game, demo and dev-tool targets, and run the
tests and the localization check.

For a whole-tree clang-tidy fix, give it the compile commands of every target
(the game and demo builds together cover them all). Do not let
`run-clang-tidy -fix` fix headers in the same run as the source files: a
header reached through two include paths (`archive_io.h` and
`../src/OpenGold.Core/src/archive_io.h`) gets the same fix applied more than
once, nesting braces. Fix the source files with `-header-filter='^$'`, then
each header once, from one file that includes it, with a header filter naming
only that header. Afterwards, check that comments heading an `else` branch did
not move into the block before it.

The [pre-commit hook](#pre-commit-hook) keeps each commit in the style; there
is no CI check.

## References

- [clang-format style options](https://clang.llvm.org/docs/ClangFormatStyleOptions.html)
- [clang-tidy](https://clang.llvm.org/extra/clang-tidy/) and
  [readability-braces-around-statements](https://clang.llvm.org/extra/clang-tidy/checks/readability/braces-around-statements.html)
