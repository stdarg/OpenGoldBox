# C++ coding style

## Scope

Use this style for project-owned C++ source and headers. Preserve the C++20,
Godot 4.x, and GDExtension architecture described in [TECH.md](TECH.md).
Exclude third-party, generated, build, and packaged files from formatting.

This document records the agreed style and the pinned formatter setup. The
project-owned C++ tree is formatted to this style. Select and record an exact
Artistic Style (astyle) release when upgrading; use that same release in
editors and automated checks.

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

astyle handles layout, not architectural correctness or ownership.
Check required braces in review or a separately configured static-analysis check.
Do not enable automatic brace insertion as part of the initial formatting pass.

## Formatter configuration

Use **Artistic Style (astyle)**. The pinned configuration is `.astylerc` in the
repository root; it is the source of truth for the exact options, including the
reasoning behind each one and the limits of what astyle can express relative to
this style (see that file's header comment). Validated with Artistic Style
3.6.18.

Install the selected astyle release, or use an existing binary of that exact
release, and make it available on `PATH`:

- macOS: `brew install astyle`
- Debian/Ubuntu: `apt install astyle`
- Windows: `choco install astyle`, or download a release from
  [astyle.sourceforge.net](https://astyle.sourceforge.net/install.html)

Record its full `astyle --version` output alongside the adopted configuration
and pin the same release in automated checks. Avoid silently using an editor's
different bundled version. Upgrade the formatter deliberately and review
resulting diffs.

## Running the formatter in PowerShell

Run these commands from the repository root, after installing the selected
release; `.astylerc` already exists there:

```powershell
astyle --version
if (-not (Test-Path -LiteralPath .astylerc)) {
    throw 'Missing the repository .astylerc configuration.'
}
```

Preview formatted output without changing the source file:

```powershell
Get-Content .\src\OpenGold.Rules.Srd5\src\srd5.cpp | astyle --options=.astylerc
```

Apply formatting to that file, then review the changes. `--suffix=none` skips
the `.orig` backup astyle otherwise leaves next to the file; that backup must
never be committed:

```powershell
astyle --options=.astylerc --suffix=none .\src\OpenGold.Rules.Srd5\src\srd5.cpp
git diff -- src/OpenGold.Rules.Srd5/src/srd5.cpp
git diff --check
```

Check formatting without editing files; a nonzero exit code means the check
failed (for example, formatting differences or an invalid configuration):

```powershell
astyle --options=.astylerc --dry-run --error-on-changes --formatted .\src\OpenGold.Rules.Srd5\src\srd5.cpp
if ($LASTEXITCODE -ne 0) {
    throw 'C++ formatting check failed.'
}
```

For additional files, supply an explicitly reviewed list of project-owned
source/header paths to the same command. Do not recursively format the entire
working tree. Automated checks should use the pinned release, require the root
configuration, and run the dry-run check on the agreed file set.

astyle's brace style does not distinguish control/function/class braces from
aggregate-initializer braces the way clang-format does, so it also breaks
multi-field struct-literal initializers onto their own line. That changed the
layout of some data tables enough to need a fix in
[tools/localization.py](../tools/localization.py)'s extraction regex (its
structured-data pattern required the opening brace and the first field to be
adjacent) — check `python tools/localization.py --check` after any
large-scale reformat, not just the build and tests.

## Editor integration

Point the editor at the pinned binary and `.astylerc` rather than trusting a
bundled formatter's defaults or an unpinned marketplace extension: define a
task or external tool that runs `astyle --options=.astylerc --suffix=none` (or
the stdin/stdout form above for a preview) against the active file, and bind
it to the format command. If a marketplace astyle extension is used instead,
point its options-file setting at the repository's `.astylerc` and pin the same
astyle release the extension calls into. Enable format-on-save only after
reviewing the initial sample, since saving an existing file can reformat the
whole file.

`.clang-format` also remains in the repository from an earlier phase, for
editors that only integrate clang-format. It is not the pinned formatter and
is not guaranteed to match astyle's output byte for byte, particularly for
wrapped expressions and aggregate initializers (see the caveat above);
`.astylerc` is authoritative.

## Adoption and validation

1. Pin the formatter release and add the shared configuration.
2. Format `srd5.cpp` first and review it as a small sample.
3. After sample approval, format the agreed project-owned source/header set in
   a separate formatting-only commit, excluding third-party and generated files.
4. Review the diff, run `git diff --check`, rebuild affected targets, and run
   relevant tests before committing source changes. Review missing-brace fixes
   separately from the mechanical formatting pass, and re-run
   `python tools/localization.py --check` since reformatting shifts source-line
   references and, per the caveat above, can change what its extraction
   patterns match.
5. Enable editor format-on-save and an automated formatting check to prevent drift.

## References

- [Artistic Style documentation](https://astyle.sourceforge.net/astyle.html)
- [Artistic Style installation](https://astyle.sourceforge.net/install.html)
