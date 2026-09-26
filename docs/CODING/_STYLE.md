# C++ coding style

## Scope

Use this style for project-owned C++ source and headers. Preserve the C++20,
Godot 4.x, and GDExtension architecture described in [TECH.md](../TECH.md).
Exclude third-party, generated, build, and packaged files from formatting.

This document records the agreed style and proposed formatter setup. It does
not install a formatter, add a root `.clang-format`, change editor settings,
or reformat source. Select and record an exact clang-format release when the
setup is adopted; use that same release in editors and automated checks.

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
- Preserve include order during the initial formatting pass to avoid introducing
  include-order problems.

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

clang-format handles layout, not architectural correctness or ownership.
Check required braces in review or a separately configured static-analysis check.
Do not enable automatic brace insertion as part of the initial formatting pass.

## Formatter configuration

Use LLVM's **clang-format**. For adoption, save the following YAML as
`.clang-format` in the repository root. LLVM supplies defaults for unspecified
options; the explicit overrides express this project's layout choices.

```yaml
BasedOnStyle: LLVM
Language: Cpp
Standard: c++20
IndentWidth: 4
ContinuationIndentWidth: 4
UseTab: Never
ColumnLimit: 100
BreakBeforeBraces: Allman
AllowShortFunctionsOnASingleLine: None
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
AllowShortBlocksOnASingleLine: Never
AllowShortLambdasOnASingleLine: None
SpaceBeforeParens: ControlStatements
SortIncludes: Never
IncludeBlocks: Preserve
```

Install the selected LLVM release with clang-format, or use an existing binary
of that exact release. Make it available on `PATH`. Record its full
`clang-format --version` output alongside the adopted configuration and pin the
same release in automated checks. Avoid silently using an editor's different
bundled version. Upgrade the formatter deliberately and review resulting diffs.

## Running the formatter in PowerShell

Run these commands from the repository root, after installing the selected
release and creating the root configuration above:

```powershell
clang-format --version
if (-not (Test-Path -LiteralPath .clang-format)) {
    throw 'Create the agreed root .clang-format before formatting.'
}
```

Preview formatted output without changing the source file:

```powershell
clang-format --style=file .\src\OpenGold.Rules.Srd5\src\srd5.cpp
```

Apply formatting to that file, then review the changes:

```powershell
clang-format --style=file -i .\src\OpenGold.Rules.Srd5\src\srd5.cpp
git diff -- src/OpenGold.Rules.Srd5/src/srd5.cpp
git diff --check
```

Check formatting without editing files; a nonzero exit code means the check
failed (for example, formatting differences or an invalid configuration):

```powershell
clang-format --style=file --dry-run --Werror .\src\OpenGold.Rules.Srd5\src\srd5.cpp
if ($LASTEXITCODE -ne 0) {
    throw 'C++ formatting check failed.'
}
```

For additional files, supply an explicitly reviewed list of project-owned
source/header paths to the same command. Do not recursively format the entire
working tree. Automated checks should use the pinned release, require the root
configuration, and run the dry-run check on the agreed file set.

## VS Code integration

With Microsoft's C/C++ extension installed, merge these settings into
`.vscode/settings.json` when adopting the setup; preserve existing settings:

```json
{
    "C_Cpp.formatting": "clangFormat",
    "C_Cpp.clang_format_style": "file",
    "C_Cpp.clang_format_fallbackStyle": "none",
    "[cpp]": {
        "editor.defaultFormatter": "ms-vscode.cpptools",
        "editor.formatOnSave": true,
        "files.trimTrailingWhitespace": true
    }
}
```

Set `C_Cpp.clang_format_path` to the full path of the pinned executable in local
user settings. Do not commit a developer-specific absolute path. The repository
already associates `.h` files with C++, so the C++ settings cover those headers.
Use **Format Document** for a manual run. Enable format-on-save after reviewing
the initial sample, since saving an existing file can reformat the whole file.

## Adoption and validation

1. Pin the formatter release and add the shared configuration.
2. Format `srd5.cpp` first and review it as a small sample.
3. After sample approval, format the agreed project-owned source/header set in
   a separate formatting-only commit, excluding third-party and generated files.
4. Review the diff, run `git diff --check`, rebuild affected targets, and run
   relevant tests before committing source changes. Review missing-brace fixes
   separately from the mechanical formatting pass.
5. Enable editor format-on-save and an automated formatting check to prevent drift.

## References

- [clang-format usage](https://clang.llvm.org/docs/ClangFormat.html)
- [clang-format style options](https://clang.llvm.org/docs/ClangFormatStyleOptions.html)
- [VS Code C++ formatting](https://code.visualstudio.com/docs/cpp/cpp-ide)
