# Native coverage and fuzz testing

## Windows verification (2026-11-23)

On Windows (MSVC 14.51, Godot 4.7.2), with every Gold Box game in
`C:\Users\imper\GoldBox`, at `f3f71172`:

- Game tree: 383 of 386 checks pass. The 3 failures are the earned
  campaign routes listed below.
- Native tree: every check except those routes passes. Two Death Knights
  earned checks skip until the Champions winners are saved.
- Demo tree 9/9, DOS probe 2/2, 38 Python tests, localization (4,358
  messages), and every developer tool's headless check pass.
- The Countdown solar map check still skips, waiting for Buck Rogers
  character creation (#312).

Main did not build on Windows. MSVC's /W4 warnings, all errors here, are now
fixed. MSVC also gives a lambda its own copy of a plain `constexpr` local array
it uses. An iterator from one copy then meets the other's `begin()` or `end()`:
`find(table, x) != table.end()` is true for every `x`. Make such a table
`static constexpr`. This broke the route players' spell choices; the fix lets
Secret's and Curse's milestones pass.

Every function-local `constexpr` table is now `static constexpr`, except in
constexpr functions and in lambdas run to initialize a constexpr value, where
C++20 forbids it. `tools/static_tables.py` finds them; the
`opengold_static_tables` check fails on a new one.

Still failing on Windows: `opengold_curse_earned_ending`,
`opengold_treasure_earned_ending` (loses at Luskan, map 31, first reached at
beat 725) and `opengold_darkness_route`, which runs several times longer than
on Linux. Each Windows run is deterministic.

- The Curse ending now matches the recorded Linux run
  ([PLAYABILITY.md](PLAYABILITY.md): 187 beats, 84 fights, warriors and
  clerics level 8, the mage 9) through beat 186. Run alone, it loses only the
  final battle with Tyranthraxus, at map 67 (6,1), in 1,890 s. That battle's
  tries are seeded the same on every platform (7 + try), so the difference
  lies inside one battle's play.
- Ruled out: dice order across call arguments (no call has two arguments that
  roll), floating point (the rules use none), sorts with ties, the game files
  (byte-identical to Steam's), and other function-local `constexpr` objects
  (all scalars or addresses).
- To narrow it: save just before the battle with `OPENGOLD_ROUTE_BEATS=186
  OPENGOLD_ROUTE_SAVE=FILE`, then play only the battle with
  `OPENGOLD_ROUTE_RESUME=FILE OPENGOLD_FIGHT_LOG=1 OPENGOLD_FIGHT_SCORES=1`,
  on Windows and on macOS, and compare the logs. The same save can be played
  under an MSVC AddressSanitizer build (`/fsanitize=address`) or a checked
  build (`/MDd /O2`, debug iterators on). Either runs a whole campaign more
  than ten times slower, too slow for the full route.

Run on a busy machine, the Secret route (820 s alone, limit 1,500 s), the
Curse earned ending, the DOS probe experiments (about 1,450 s alone) and the
combat demo scene's footstep check can fail. Run them alone before taking a
failure as real. The DOS probe experiments now have a 3,600 s limit.

## Earned Original Rules campaign (2026-11-17)

All 337 Linux checks pass across the complete run and the corrected route
rerun, with no skips. The two final Original Rules checks pass in 84.11 seconds;
the fresh campaign takes 83.96 seconds. Nine developer-tool checks, both DOS
checks, 21 Python tests and localization validation also pass. The tested tree
is based on `334099c8` with the accompanying changes; detailed evidence and
timings are in [PLAYABILITY.md](PLAYABILITY.md).

`opengold_original_campaign_route` creates six human characters with zero XP
and their actual creation gold, uses the original's legal creation Modify
controls, buys equipment, earns quest and combat rewards, pays the real guilds,
and plays through Tyranthraxus's defeat and the ending. Arena practice earns XP
through actual mirror duels; losses, paid lodging, healing spells and save/reload
remain normal game actions. It supplies no levels, money, equipment, HP, quest
flags, relocations or combat results. The actual dragon record must be defeated;
a living boss at a ceasefire cannot pass. It plays with the Options screen's
defaults, Original movement and Original shops on; a reloaded stage gets the
town's shops and temples as the game gives them to an opened save.

```sh
OPENGOLD_GOLDBOX_DIR=/home/edmond/GoldBox \
OPENGOLD_GAME_DIR=/home/edmond/GoldBox/POOLRAD \
ctest --test-dir build/linux --output-on-failure -R '^opengold_original_campaign_route$'
```

Missing private game files explicitly skip this installed-data check. With the
files installed, the fresh test is a start-to-ending route, not a claim of
unassisted completion of every optional side quest. Other expedition audits
retain their separate assisted coverage. See [PLAYABILITY.md](PLAYABILITY.md)
for verification results and limitations.

For diagnostics, `opengold_expedition_tests --original-campaign FOLDER [STAGE]`
retains real campaign checkpoints. A named-stage resume is useful for debugging;
the registered test always starts a fresh party. `OPENGOLD_TRACE=1` adds travel
and casualty diagnostics; `OPENGOLD_TRACE_COMBAT=1` includes full combat logs.

## Darkness follow-up (2026-11-17)

The current [playability record](PLAYABILITY.md#2026-11-17-darkness-defenses-and-finale)
supersedes the historical forced-finale limitation below. Its checkpoint replay
now kills Gothmenes through actual combat, with no forced results or ceasefire,
and reaches the ending with all six members recovered. The fresh Darkness
whole route passes in 1,307.62 seconds. All 336 checks pass across the complete
run (335/336 in 1,328.04 seconds, no skips) and Secret's corrected whole-route
rerun: that single failure caught obsolete forced-win markers, now removed.
The rebuilt nine demo/catalog checks, both DOS checks, 21 Python tests and
localization validation also pass. Commercial progression and movement
assistance remain explicit limitations. The tested tree is based on `742f7840`,
with the changes accompanying this record.

## Linux verification (2026-11-16)

The playability follow-up based on `e3b2ae9c`, with the changes accompanying
this record, passed **all 336 game-build checks, with no failures and no skips**
in one final integrated run (1,211.51 seconds), including 171 Godot checks:

```bash
OPENGOLD_GOLDBOX_DIR=/home/edmond/GoldBox \
OPENGOLD_GAME_DIR=/home/edmond/GoldBox/POOLRAD \
JOBS=8 ./build.sh --test-all
```

All nine rebuilt demo/catalog checks (10.88 seconds), both DOS-probe checks
(469.96 seconds) and all 21 Python tests also passed. Localization validates
all 4,143 messages. The final full route times include Secret (626.13 seconds),
Darkness (1,194.74 seconds) and the Treasures map walk (1,089.19 seconds).

New regressions cover camp availability and persistence of the globes and
Mass Invisibility, Darkness's palace selector after exhausting every wandering
group, and the generated UA design's save/reload before its actual winning
event. Secret's complete route now wins every battle without forced victories.
See [PLAYABILITY.md](PLAYABILITY.md) for exact evidence and remaining gaps:
commercial routes retain progression and movement assistance, and Darkness
still has forced combat outcomes. These passing tests do **not** certify every
game as an unassisted end-to-end completion. Krynn and Buck Rogers remain future
work.

The earlier full installed-data run on `9a197ba9` also passed all 336 checks
(1,383 seconds). At `e3b2ae9c`, the six exported commercial campaigns launched
with their original-rules default parties both headlessly and in rendered
windows under `cage`, exiting without script or native failures. Checks use
isolated settings and user-data folders, leaving player configuration unchanged.

### Initial fixes and verification

Verified the changes based on `ee0a6b28` with GCC 16.2, Godot 4.7.2 and the
installed Pool of Radiance files (`OPENGOLD_GAME_DIR`). Build warnings remain
errors. The game build, export and packaged-game smoke check passed.

- `JOBS=8 ./build.sh --test-all`: 306 passed, 30 skipped, no failures out of 336.
  This initial run omitted `OPENGOLD_GOLDBOX_DIR`, leaving the installed
  additional games unused. They are present at `/home/edmond/GoldBox`.
- Demo/catalog targets built with the same compiler, `OPENGOLD_BUILD_GODOT=ON`
  and `OPENGOLD_BUILD_GAME=OFF` in `build/linux-extras`.
  `ctest --test-dir build/linux-extras --output-on-failure -j 4 -L demo`:
  all nine checks passed. The map inspector also passed after rebuilding its
  exporter with `cmake --build build --target opengold_maps --parallel 8`.
- `python3 -m unittest discover -s tests -p '*_tests.py'`: all 21 passed.
- `python3 tools/localization.py --check`: all 4,143 messages validated.
- With `OPENGOLD_BUILD_DOS_PROBE=ON`,
  `ctest --test-dir build/linux-extras --output-on-failure -j 2 -R '^opengold_dos_probe_'`:
  both checks passed, including the original game's VM, dungeon placement,
  wilderness placement and wilderness-field comparisons (516 seconds).

The encounter-formation regression checks cover empty encounters, party-only
and enemy-only formations in all four directions. The fix avoids division by
zero when there are no enemy quarters. Original-combat test lookups now copy
views obtained from temporary snapshots instead of retaining dangling references.
The Linux archive stages monster JSON records without the separately distributed
combat images.

One initial demo import aborted inside Godot. The suite rerun passed, as did a
cold-cache import under GDB and three cold-cache imports without GDB; no reliable
reproduction or extension-code failure was found.

## Running the suite: use `build.cmd`

Run verification through the repository script, not hand-written `cmake` and
`ctest` calls:

```
build.cmd          # Windows: vcvars, configure, build, then ctest
build.cmd debug    # the same with the unoptimized Debug preset
```

It checks `errorlevel` between every step, so a compile failure stops it before
any test runs. The default preset is optimized (RelWithDebInfo), and tests run
in parallel: the suite takes under a minute. The Debug preset takes 15 to 20
minutes, almost all of it `opengold_status_effect_tests` under MSVC's checked
iterators; run it occasionally, not for every change. No test relies on
`assert()`, so the optimized build loses no checks.

While iterating, build and run only the affected targets (below), then run the
whole suite once before pushing.

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

## The full suite, including Godot and localization

`build.cmd` and `./build.sh default --test` use the `default` preset, which leaves
`OPENGOLD_BUILD_GODOT` **off**. That is the fast native gate, not the whole
suite. Running only that and reporting "all checks passed" hides every Godot
runtime check — the failure mode is silence, not an error.

For the demos' Godot checks, configure a second build directory with them enabled:

```
cmake -S . -B build-godot -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
      -DOPENGOLD_BUILD_TESTS=ON -DOPENGOLD_BUILD_GODOT=ON
cmake --build build-godot -j6
ctest --test-dir build-godot -R "^opengold_(demo_prepare|godot_)"
```

The game's Godot checks run in `build/game`, which `build-opengoldbox.cmd`
configures with `OPENGOLD_BUILD_GAME=ON`; see [BUILD.md](BUILD.md).

For the demos, Godot is located by `find_program`, which searches `PATH` and the usual install
locations on each platform. If it is not found, CMake prints a status line and
the checks are simply **not registered** — confirm they exist with
`ctest -N` rather than assuming a clean run covered them. Set
`OPENGOLD_DEMO_GODOT` to override. The demo checks share a project and
user-data path, so each holds a CTest `RESOURCE_LOCK`; `ctest -j` runs them one
at a time while native tests run alongside. The game's Godot checks run side by
side: each gets its own user data and `settings.cfg` (`OPENGOLD_SETTINGS_FILE`,
see [CONFIGURATION.md](CONFIGURATION.md#environment-overrides)), and they share
only the imported project, which `opengold_godot_prepare` makes before any of
them starts. A check that changes files in the project folder runs with
`RUN_SERIAL`, alone, as the portrait fallback and Slums encounter checks do.
Checks drawn in a real window (`GRAPHICAL=ON`, and the package smoke check's
windowed run) draw in an invisible display of their own
([virtual_display.cmake](../tests/virtual_display.cmake)), so they pass with
the screen asleep or no monitor attached, and no test window appears on the
desktop. On Linux that display comes from `cage`, a small Wayland compositor
drawing with the graphics card, and each check gets its own, so they run side by
side. Where there is no `cage`, as in the AlmaLinux 8 release container, they
use `xvfb-run`, which draws in software and can give two checks started together
the same display, so there they share `RESOURCE_LOCK godot_window` and run one
at a time. With neither, they draw on the desktop, which must then stay awake:
run the suite under `kde-inhibit --power --screenSaver` on KDE. A new windowed
check is listed with the others in `src/OpenGoldBox/CMakeLists.txt` for that
lock.

On macOS the checks run a copy of the installed Godot that stays out of the
player's way, made at configure time in the build folder's `godot-out-of-dock`
([godot_out_of_dock.cmake](../cmake/godot_out_of_dock.cmake)). It starts with
`tools/godot_out_of_dock/no_dock.c` loaded: Godot never shows in the Dock,
never takes focus, and its windows go behind the player's with the app hidden,
as if minimized but with no thumbnail in the Dock. A hidden Godot still draws
every frame, so the checks that read the screen see real images. Exports,
packaging and the developer tools' own windows use the installed Godot.

### Known pre-existing Godot failures

As of 2026-09-26, on Windows, two game-side Godot checks fail at the commit that
first ran them here, and at the earlier baseline `375f58f`. They are recorded so
a future change is not blamed for them:

| Check | Assertion |
| --- | --- |
| `opengold_godot_adrenaline` | Saving pending choice does not refund or reroll |
| `opengold_godot_rogue_attack` | UI use matches native continuation exactly |

Both compare a UI-driven save against a native one, so both are candidates for a
platform difference in how the checkpoint is written or read rather than a rules
defect. Neither has been diagnosed. Both have since passed: on 2026-10-25 all 239
Windows ctest checks passed with original game data.

Localization is a separate gate and needs Python:

```
python tools/localization.py --check
```

It scrapes string literals out of the C++ sources, so it catches two things the
compiler cannot: a message template built by concatenation (the extractor sees
the fragments, not the message) and a missing Spanish translation. On Windows the
`python` on `PATH` may be a Microsoft Store stub that cannot run or install
anything; `winget install --id Python.Python.3.12 -e` provides a real one.
clang-format and clang-tidy, the pinned C++ formatter and brace check (see
[CODING_STYLE.md](CODING_STYLE.md)), come from `winget install LLVM.LLVM` or a
release at [github.com/llvm/llvm-project/releases](https://github.com/llvm/llvm-project/releases).

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
ctest --test-dir build/native --output-on-failure
```

`opengold_fuzz_smoke_tests` is registered with a 60-second timeout and the
`fuzz` and `native` labels. Assertions remain active in release builds.
Installed-data tests find each game's folder by its files, as the game does
([TECH.md](TECH.md#finding-the-games-folders)): in `OPENGOLD_GAME_DIR` and
`OPENGOLD_GOLDBOX_DIR` when set, else upward from the checkout to the home
folder. A test whose game is not found skips. To run without the original
files, unset both and give ctest an empty home:
`env -u OPENGOLD_GAME_DIR -u OPENGOLD_GOLDBOX_DIR HOME="$(mktemp -d)" ctest ...`. Game builds
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
