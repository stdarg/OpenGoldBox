# Native coverage and fuzz testing

## Running the suite: use `build.cmd`

Run verification through the repository script, not hand-written `cmake` and
`ctest` calls:

```
build.cmd          # Windows: vcvars, configure, build, then ctest
```

It checks `errorlevel` between every step, so a compile failure stops it before
any test runs.

**Why this is mandatory rather than a convenience.** `ctest` executes whatever
binary is already on disk. If a target fails to compile, the previous binary is
still there and `ctest` reports it as passing. On 2026-09-26 a hand-rolled
`cmake --build` / `ctest` pair reported "100% tests passed, 50/50" while one test
target had failed to compile. Any "all N checks pass" claim produced without
checking the build result first may be a stale-binary pass.

If you must run the steps separately — to build one target during development —
inspect the build output for `error` and `FAILED` before believing `ctest`:

```
cmake --build build --target <one_target> -j6
ctest --test-dir build --output-on-failure -R '^<one_target>$'
```

Two tools this document assumes are **not** present on every development
machine; check before relying on them, and record a check as unverified rather
than passed if they are missing:

- `python3` (needed by `tools/localization.py --check`)
- `clang-format`

## Coverage review (2026-09-18)

The review compared the native CTest suite before and after these additions,
using Apple Clang source coverage in a Debug build with `OPENGOLD_GAME_DIR`
unset. Original game files are not needed for the new tests.

| Source | Line coverage, before → after | Branch coverage, before → after |
| --- | --- | --- |
| `character_art.cpp` | 64.5% → 100% | 51.2% → 86.0% |
| `por_effects.cpp` | 21.2% → 100% | 51.6% → 100% |
| `formats.cpp` | 96.2% → 99.4% | 78.3% → 94.2% |

These are coverage of specific native source files, not the whole application.
Line coverage alone does not establish correctness. In particular, filesystem
failure branches, original campaign event variants, and rendered Godot behavior
still need integration and observation tests.

### Added checks

- **Synthetic character art:** temporary archives owned by a scoped fixture;
  duplicate/conflicting portrait IDs, missing/corrupt records, missing poses,
  file-size limits, case-independent DOS filenames, and use after the source
  files have been removed. All 1,792 head/body/size/pose combinations compose.
  Recolor properties check every palette choice for all six regions in both
  color banks, sizes, and poses, including occlusion, transparency, unchanged
  source components, and visible-pixel counts.
- **Effect metadata:** all 256 byte identifiers, reserved codes, aliases, and
  the difference between absent and explicitly zero quantities. Quantified
  resistance, regeneration, level drain, and saving throw modifiers are checked.
- **Deterministic mutations:** 3,840 mutations of 24 authored seeds, plus seed
  and empty-input checks. Bit flips, replacement, truncation, erasure, and
  insertion run in normal CTest, without a special compiler or original assets.
- **Combat regressions:** malformed extra movement with an unspent action;
  legitimate Dash budgets; command revision rollover; continuation past round
  100,000 and at the round counter's maximum.

Checkpoint fuzzing found two accepted states whose next command produced an
unloadable checkpoint: extra movement with an unused Dash, and a maximum
revision wrapping to reserved zero. Restore now checks movement against action
availability; revisions skip zero on wrap. Related boundary tests also exposed
the old round-100,000 restore limit. Round numbers now use the full unsigned
range and saturate at its maximum, allowing play and saves to continue.

## Normal tests

Use the existing CMake/CTest workflow:

```bash
cmake -S . -B build/native -DOPENGOLD_BUILD_TESTS=ON
cmake --build build/native -j 6
env -u OPENGOLD_GAME_DIR ctest --test-dir build/native --output-on-failure
```

`opengold_fuzz_smoke_tests` is registered with a 60-second timeout and the
`fuzz` and `native` labels. Assertions remain active in release builds.
Installed-data tests remain optional through `OPENGOLD_GAME_DIR`. Game builds
also include the [Godot integration checks](../src/OpenGoldBox/README.md#tests).

## Sanitizer fuzzing

Fuzzing is appropriate at byte-oriented decoder and serialized-state boundaries.
It complements the existing exhaustive small-grid tests and rules examples.

`OPENGOLD_BUILD_FUZZERS` defaults to `OFF`. Enabling it requires Clang with
libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer. Use a separate
native build directory; CMake rejects combining this option with Godot or game
builds. Both the harnesses and their native libraries receive instrumentation.
Undefined behavior is fatal so it produces a reproducible failure artifact.

On this Apple Silicon development machine, the full LLVM installation is at
`/opt/homebrew/opt/llvm`; use your installed Clang path on other systems.

```bash
cmake -S . -B build/fuzz \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DOPENGOLD_BUILD_FUZZERS=ON -DOPENGOLD_BUILD_TESTS=ON \
  -DOPENGOLD_BUILD_GAME=OFF -DOPENGOLD_BUILD_GODOT=OFF
cmake --build build/fuzz -j 6
env -u OPENGOLD_GAME_DIR ctest --test-dir build/fuzz --output-on-failure
build/fuzz/opengold_fuzz_smoke_tests --write-corpus build/fuzz/corpus
mkdir -p build/fuzz/artifacts/formats build/fuzz/artifacts/checkpoint

build/fuzz/opengold_formats_fuzzer build/fuzz/corpus/formats \
  -max_total_time=45 -timeout=5 -rss_limit_mb=2048 -max_len=4096 \
  -seed=20260918 -artifact_prefix=build/fuzz/artifacts/formats/
build/fuzz/opengold_checkpoint_fuzzer build/fuzz/corpus/checkpoint \
  -max_total_time=45 -timeout=5 -rss_limit_mb=2048 -max_len=65537 \
  -seed=20260920 -artifact_prefix=build/fuzz/artifacts/checkpoint/
```

| Target | Invariants beyond crash detection |
| --- | --- |
| Formats | DAX failure is atomic; record IDs are unique; decoded dimensions match owned buffers; EGA alpha is binary; indexed art stays within its palette; sound expansion and sequencing remain bounded; copied ECL machines execute deterministically within 64 instructions. |
| Checkpoint | Accepted saves round-trip exactly; rejected commands preserve state and RNG; offered commands succeed; two restored sessions continue identically through three commands and can be saved/restored again. |

The first formats byte selects DAX/images, picture/character components, ECL,
sound executable unpacking, or sound sequencing. Checkpoint inputs are the
unchanged textual checkpoint format, bound to the synthetic rules module in
`tests/fuzz/fuzz_cases.cpp`. Decoder runtime errors are expected rejections;
invariant failures and exceptions after an accepted restore escape the harness.

Seeds are generated from authored C++ fixtures. Keep discovered corpora and
artifacts under ignored `build/`; do not include original game assets. Fuzzing
extends the on-disk corpus, so a fixed seed alone does not reproduce an entire
campaign across different corpora, compilers, or time limits. Replay the failure
file instead:

```bash
build/native/opengold_fuzz_smoke_tests --replay-checkpoint path/to/crash-file
build/native/opengold_fuzz_smoke_tests --replay-formats path/to/crash-file
```

Minimize a failure, identify its violated contract, and add an ordinary focused
regression test before fixing it. The same harness runs under CTest and
libFuzzer, so replay does not require sanitizer tooling.

### Verification performed (initial coverage review)

- All 16 native tests passed without original assets, including under ASan/UBSan.
- All 25 tests in the macOS game build passed, including Godot integration and
  installed-data checks with the local original files.
- Formats: 1,433,172 inputs in a bounded campaign; no failure found.
- Checkpoint: after fixing and replaying the discovered failures, a fresh
  campaign completed 547,370 inputs without another failure.

These short campaigns establish useful regression coverage, not an exhaustive
proof over all possible inputs. The harness bounds input sizes, VM steps, sound
ticks, and continuation length deliberately. Broader campaign-state and content
parser fuzz targets remain future opportunities.

### Saving throws and conditions verification

- All 27 tests in the macOS build passed with the local original files.
- Status effects, combat rules, party state, campaign saves and mutation smoke
  tests passed under ASan/UBSan (five suites).
- A bounded checkpoint campaign completed 329,718 inputs without a failure, using
  fresh seeds that include active and overlapping blindness applications.
- The native effect suite checks maximum-size effect collections, precise rest
  eligibility, campaign encounter source identities and time-update equivalence.
- The graphical condition check exercises the actual spell button, target click,
  roster/log updates and save/load controls at the supported window sizes.

See [status effects](STATUS-EFFECTS.md) for the implementation and review command.
