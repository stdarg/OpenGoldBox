# Building OpenGoldBox and the demos

This guide covers the three separate builds in this repository: the native core
and its tests, the OpenGoldBox game, and the reference demos. Install the
prerequisites in [INSTALL.md](INSTALL.md) first.

| Build | Windows | macOS and Linux | Build tree | Output |
| --- | --- | --- | --- | --- |
| Native core and tests | `.\build.cmd` | `./build.sh default` | `build/` | libraries, tests, `opengold_maps` |
| Game | `.\build-opengoldbox.cmd` | `./build.sh` | `build/game/` (Windows), `build/macos-universal/` (macOS), `build/linux/` (Linux) | `win-package/opengoldbox.exe`, `mac-package/OpenGoldBox.app`, `linux-package/opengoldbox.x86_64` |
| Demos | `.\demos\build-rolf.cmd` | see [the demos](#the-demos) | `build/godot/` | `demos/godot/bin/opengold_godot.dll` |

On macOS and Linux, `./build.sh` with no options builds the game and re-exports
it, and runs no tests. Run it after every change you want to see in the app: the
app carries its own copy of the scenes, so it does not change until it is
exported again.

Keep the game and demo trees separate. The demos' `build_profile.json` trims the
generated Godot bindings and removes classes the game needs, so one tree cannot
serve both.

## Before you build

On Windows, the helper scripts expect Visual Studio Build Tools at
`C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools`. They load its
developer environment and use its bundled CMake, so they run from a plain
PowerShell or VS Code terminal. With another installation, run the equivalent
`cmake` commands below from a Visual Studio Developer PowerShell.

The Godot builds also need Git and Python 3.8+ (the first configure fetches and
generates [godot-cpp](https://github.com/godotengine/godot-cpp)), and Godot 4.5+
on `PATH`. On Windows the `python` on `PATH` may be a Microsoft Store stub; install
a real interpreter with `winget install --id Python.Python.3.12 -e`, or pass
`-DPython3_EXECUTABLE=C:/path/to/python.exe` when configuring.

Optionally install [ccache](https://ccache.dev) (`winget install --id Ccache.Ccache -e`
on Windows, `brew install ccache` on macOS). When CMake finds it, compiled objects
are cached across build directories and worktrees, so a fresh checkout reuses
what another one already compiled. Open a new terminal after installing so
`ccache` is on `PATH`. Test sources that embed their checkout path
(`OPENGOLD_SOURCE_DIR`) still recompile in each worktree.

Original game files are never built or copied into the repository. Anything that
reads them uses the `OPENGOLD_GAME_DIR` environment variable, pointed at the
directory containing the DAX files:

```powershell
$env:OPENGOLD_GAME_DIR = 'C:\Games\POOLRAD'
```

## Native core and tests

The native build compiles the formats, core, rules interface and SRD module as
C++20, plus the native test suite and the `opengold_maps` exporter. It needs
neither Godot nor Python.

Windows:

```powershell
.\build.cmd
```

macOS and Linux:

```bash
./build.sh default --test
```

`build.cmd` configures the `default` preset (RelWithDebInfo, Ninja), builds,
then runs the tests in parallel, and stops at the first failure. `build.sh`
builds the preset it is given, or the platform's game preset with none, and
runs tests only when asked:

| `build.sh` option | Effect |
| --- | --- |
| *(none)* | Build the game and re-export it: `mac-package/OpenGoldBox.app` on macOS; `linux-package/` and [the package for players](#the-package-for-players-2) on Linux. No tests. |
| `--test` | After building, run the native tests (everything except the Godot runtime checks). |
| `--test-all` | After building, run every test, including the Godot runtime checks. |
| `--package` | macOS: also write [the disk image players download](#the-package-for-players-1). |
| `default`, `debug`, `macos-universal`, `linux` | Build that preset instead of the platform default. |
| `linux-release` | The Linux package for players, built against glibc 2.28. Run it through `tools/linux_release_build.sh`, which builds it in its container (see [the package for players](#the-package-for-players-2)). |

Both scripts always build the checkout they live in, whatever the current
directory, and never run tests after a failed build. The equivalent manual
commands are:

```powershell
cmake --preset default
cmake --build --preset default
ctest --preset default
```

The `debug` preset builds an unoptimized tree in `build/debug`. Its tests take
15 to 20 minutes rather than under one, mostly because MSVC's debug library
checks every container access, so use it for occasional deeper checks rather
than every change:

```powershell
.\build.cmd debug
```

```bash
./build.sh debug --test
```

The project's own targets build at a high warning level: `-Wall -Wextra` with
Clang and GCC (plus `-Wshorten-64-to-32` on Clang, which reports the narrowing
MSVC warns about) and `/W4 /permissive-` with MSVC. Every warning in them is an
error, in the native core, the game, the demo and the dev tools alike. Fetched
code such as godot-cpp keeps its own flags. To build past a warning while
investigating it, configure with `--compile-no-warning-as-error`.

When running these by hand, check the build output for errors before trusting
`ctest`: a target that fails to relink leaves its previous binary in place, and
`ctest` then reports a pass against stale code.

## The game

The game is a Godot GDExtension plus a Godot export. See
[src/OpenGoldBox/README.md](../src/OpenGoldBox/README.md) for launch options,
settings and game-specific checks.

### Windows

Install the Windows x64 debug export template that matches your Godot editor at
`build/export-templates/windows_debug_x86_64.exe`, with
`windows_debug_x86_64_console.exe` from the same archive beside it, or pass its
location to the helper:

```powershell
.\build-opengoldbox.cmd
.\build-opengoldbox.cmd -DOPENGOLDBOX_DEBUG_TEMPLATE=C:/path/to/windows_debug_x86_64.exe
```

The helper configures `build/game` with `OPENGOLD_BUILD_GAME=ON` in
`RelWithDebInfo`, builds, runs the native and headless Godot tests, and fills
`win-package/` with the executable, CMD launcher, PCK, GDExtension DLL, rules
data and runtime DLLs. Keep that folder together. Run the game with:

```powershell
.\win-package\opengoldbox.exe
```

From `cmd.exe`, use `win-package\opengoldbox.console.exe`, which waits for the
game, returns its exit code and then restores the prompt.

To rebuild only the game target after configuring:

```powershell
cmake --build build/game --target OpenGoldBox
```

#### The package for players

`win-package/` also collects development leftovers, so every build stages the
package players get in `build/game/package/OpenGoldBox/` and zips it as
`build/game/OpenGoldBox-<version>-windows-x86_64.zip`. It holds only the executables, the
PCK, the DLLs, `data/`, `LICENSE.txt` and `THIRD-PARTY-NOTICES.txt`; the
notices for the Godot Engine and its third-party components are written by the
Godot that built the export, so they match it. The build fails if the package
holds any file the original game's MD5 manifest names, any `.DAX`, a
`settings.cfg` or a `.TMP` file. Players unzip it anywhere, including a
read-only folder: settings and saves go to
`%APPDATA%\Godot\app_userdata\OpenGold` (see
[configuration](CONFIGURATION.md)). The package uses the debug export
template, so `opengoldbox.console.exe` shows the engine's messages when a
player reports a problem. The executable carries the app icon and the version,
which comes from `project()` in the top-level `CMakeLists.txt`.

The build also writes `build/game/OpenGoldBox-<version>-windows-symbols.zip`, for
you: keep it with the release. It reads
[crash reports](CONFIGURATION.md#crash-reports) down to functions.

The executables and DLLs import only each other, the bundled Visual C++ runtime
and libraries that ship with Windows 10 and later, so the package needs no
Redistributable installed. Windows 10 is the oldest version it expects.

`opengold_windows_package_smoke`, a ctest check that needs `OPENGOLD_GAME_DIR`,
installs the package under `Program Files Prüfung 冒険` and a copy of the
original files under `Pool of Radiance Daten ü 冒険`, makes both read-only, and
runs the packaged game with a fresh profile, a `PATH` of only the Windows
folders and no `OPENGOLD_*` overrides. It checks that the first launch waits in
setup without writing settings, then plays character creation, shops, combat
and recovery, saves, restarts and restores the saves. It also runs two copies at
once and crashes the game on purpose to check the report, and that nothing was
written into the install. Run it alone with:

```powershell
ctest --test-dir build/game -R windows_package_smoke --output-on-failure
```

On a clean Windows machine or VM, which that check cannot stand in for, unzip
the package and check by hand:

1. `opengoldbox.exe` starts with no Visual C++ Redistributable or Godot
   installed, and asks for the language, then the game folder.
2. A wrong folder, an empty one and one missing a `.DAX` return to folder
   selection with a message; a copy whose files differ shows the MD5 warning.
3. After **Continue**, quit and start again: setup is not asked again, and a
   saved campaign loads.

Last run on 2026-10-30: Windows 10 Home 10.0.19043, Godot 4.7.2-stable
(official) with its debug export template, MSVC 19.51.36256, godot-cpp
e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77. The smoke check passed; the clean
machine steps have not been run yet.

### macOS

Install the Godot macOS export templates matching your editor, then:

```bash
./build.sh
open mac-package/OpenGoldBox.app
```

Add `--test` for the native tests or `--test-all` for every test, including the
Godot checks. The script runs these commands:

```bash
cmake --preset macos-universal
cmake --build --preset macos-universal
ctest --preset macos-universal
```

This builds a universal (`arm64` and `x86_64`) app bundle, ad hoc signed for
local testing. The preset uses Ninja, which compiles on every core; a tree
configured earlier with Unix Makefiles must be deleted once
(`rm -rf build/macos-universal`) before it configures again.

#### The package for players

```bash
./build.sh --package
```

This also writes two files in `build/macos-universal/`:

- `OpenGoldBox-<version>-macos.dmg`, about 1.4 GB, for players: the app and a
  link to Applications to drag it onto. The build opens the finished image,
  verifies the app's signature and starts it once.
- `OpenGoldBox-<version>-macos-symbols.zip`, for you: keep it with the release.
  It reads [crash reports](CONFIGURATION.md#crash-reports) down to source lines.

Each build of the app adds `LICENSE.txt` and `THIRD-PARTY-NOTICES.txt` to its
`Contents/Resources`, and fails if the bundle holds any file the original game's
MD5 manifest names, any `.DAX`, a `settings.cfg` or a `.TMP` file. As on Windows,
the app uses the debug export template, so a crash leaves a full backtrace. The
version comes from `project()` in the top-level `CMakeLists.txt`; raise it for a
release, since it is the only thing in a crash report that names the build.

The app is ad hoc signed, not notarized, because that takes an Apple Developer ID
certificate. macOS therefore refuses to open a downloaded copy at first. The
player opens it once, sees the refusal, then chooses **Open Anyway** in System
Settings > Privacy & Security.

`opengold_macos_package_smoke`, a ctest check that needs `OPENGOLD_GAME_DIR`,
installs the app under `Programme Prüfung 冒険` and a copy of the original files
under `Pool of Radiance Daten ü 冒険`, makes both read-only, and runs the app with
a fresh home folder, a `PATH` of only the system's tools and no `OPENGOLD_*`
overrides. It checks that the first launch waits in setup without writing
settings, then plays character creation, shops, combat and recovery, saves,
restarts and restores the saves. It also runs two copies at once and crashes the
game on purpose to check the report; that crash exits quietly once the engine has
logged it, so it leaves macOS no crash report and no "quit unexpectedly" box.
Nothing may be written into the app. Run it
alone with:

```bash
OPENGOLD_GAME_DIR=/path/to/POOLRAD ctest --preset macos-universal -R macos_package_smoke --output-on-failure
```

On another Mac, or a clean user account, which that check cannot stand in for,
download the image from a browser and check by hand:

1. Drag the app to Applications. Opening it is refused once, and **Open Anyway**
   starts it, with no Godot or developer tools installed.
2. It asks for the language, then the game folder. A folder in Documents or on an
   external drive brings up macOS's permission request, and the game reads it.
3. After **Continue**, quit and start again: setup is not asked again, and a saved
   campaign loads.

Checked automatically on 2026-10-30: macOS 26.6.2 on Apple silicon, and the Intel
build started under Rosetta, Godot 4.7.2-stable (official) with its debug export
template. The hand steps have not been run yet.

### Linux

Install the Godot Linux export templates matching your editor (see
[INSTALL.md](INSTALL.md#linux-bazzite-and-ubuntu)). The first configure needs a
GCC that has the static libstdc++, which godot-cpp links into the game library
so that it does not depend on the player's libstdc++. Bazzite's own GCC lacks
it, so use Homebrew's there; CMake remembers the compiler afterwards:

```bash
CC=gcc-16 CXX=g++-16 ./build.sh
./linux-package/opengoldbox.x86_64
```

The script runs `cmake --preset linux` and `cmake --build --preset linux`, and
`ctest --preset linux` with `--test` or `--test-all`. The export is laid out as
on Windows: the executable, `opengoldbox.pck` and `opengoldbox_game.so` side by
side, with `data/` next to them.

#### The package for players

Every build also writes two files in `build/linux/`:

- `OpenGoldBox-<version>-linux-x86_64.tar.gz`, about 1.4 GB, for players: an
  `OpenGoldBox` folder to unpack anywhere and start with `opengoldbox.x86_64`.
  Its game library has its debug information stripped.
- `OpenGoldBox-<version>-linux-symbols.tar.gz`, for you: the unstripped library,
  to keep with the release. It reads [crash reports](CONFIGURATION.md#crash-reports)
  down to source lines.

As on the other systems, the package gets `LICENSE.txt` and
`THIRD-PARTY-NOTICES.txt`, uses the debug export template, and the build fails
if it holds any original game file, a `settings.cfg` or a `.TMP` file.

##### Which Linux systems it runs on

glibc tags each function it exports with the glibc version that introduced it,
and the linker records the newest tag the build machine's glibc offers. A
system whose glibc lacks one of those tags refuses to load the game. So a
package runs only on systems whose glibc is at least as new as the build
machine's: one built on Bazzite 44 needs glibc 2.43, newer than Ubuntu 24.04's.

The package for players is therefore built in an AlmaLinux 8 container, whose
glibc is 2.28, the same Godot's own Linux export templates need:

```bash
tools/linux_release_build.sh --test-all
```

The first run creates the [distrobox](https://distrobox.it) container
`opengold-alma8` (Bazzite ships distrobox; elsewhere install it and podman) and
installs GCC 14 from `gcc-toolset-14`, CMake, Ninja, Python 3.11 and the
libraries Godot needs. Every run then builds the `linux-release` preset into
`build/linux-release/` and `linux-package/`, with the same options as
`./build.sh`. The container shares your home folder, so Godot, its export
templates and `OPENGOLD_GAME_DIR` are the host's, and the package smoke check
runs on glibc 2.28. The preset sets `OPENGOLD_MAX_GLIBC` to 2.28, and the
package step then fails if the executable or the game library needs a newer
glibc, naming the functions that do.

Such a package runs on x86_64 distributions with glibc 2.28 or newer: RHEL,
AlmaLinux and Rocky 8, Debian 10, Ubuntu 20.04, Fedora 29, SteamOS 3 and
anything later. It does not run on musl distributions such as Alpine, and
there is no ARM build. godot-cpp links libstdc++ into the game library, so the
player's libstdc++ version does not matter.

GCC 14 warns about a few things GCC 16 does not; the build turns its warnings
into errors as everywhere, except `maybe-uninitialized`, which GCC before 15
reports falsely for `std::optional` and which GCC 16 still checks.

`opengold_linux_package_smoke`, a ctest check that needs `OPENGOLD_GAME_DIR`,
runs the same steps as the macOS check: the package and a copy of the original
files under non-ASCII folder names, both read-only, a fresh home folder, a `PATH`
of only the system's tools and no `OPENGOLD_*` or `XDG_*` overrides. It also
starts the game once in a real window, which needs a desktop session: the
graphics driver then loads the system's libstdc++ into the game, which headless
runs never do. Run it alone with:

```bash
ctest --preset linux -R linux_package_smoke --output-on-failure
```

Checked automatically on 2026-11-07: Bazzite 44 (x86_64), GCC 16.2 from
Homebrew, Godot 4.7.2-stable (official) with its debug export template. It has
not been run on another distribution yet.

## Releasing on GitHub

`tools/release.py` builds this platform's package for players and uploads it to
a draft GitHub release, on macOS, Linux and Windows. Run it from a clean
checkout of `main` that matches `origin/main`, with the GitHub CLI logged in
(`gh auth login`) and `OPENGOLD_GAME_DIR` set:

```bash
tools/release.py
```

On Windows, from PowerShell:

```powershell
python tools\release.py
```

It runs `./build.sh --test-all --package` on macOS,
`tools/linux_release_build.sh --test-all` on Linux, or
`build-opengoldbox.cmd` on Windows, so nothing untested is uploaded and the
Linux package runs on glibc 2.28 and newer ([which systems](#which-linux-systems-it-runs-on)).
It then uploads the disk image, the `.tar.gz` or the `.zip` to the draft release
`v<version>`, the version being `project()`'s in `CMakeLists.txt`. The first
platform creates the draft at the current commit; the next one, built from the
same commit, adds its package to it. It refuses a published release and a draft
made from another commit, so raise the version for each release.

It never publishes. A draft is visible only to people who can push to the
repository: check its files on GitHub, write the notes, then publish it there,
which creates the tag. The debug symbols archive stays with you; the script
prints where it is.

## The demos

The demos are reference scenes in `demos/godot/`, backed by the C++ extension in
`demos/src/OpenGold.Godot/`. That extension holds only research tools (sprites,
equipment, sound board); the character, combat and Rolf launchers open the
game's own scenes in `src/OpenGoldBox/godot`, so they need the game build. See
[demos/README.md](../demos/README.md) for what each one shows.

### Build the demo extension

Windows:

```powershell
.\demos\build-rolf.cmd
```

This configures `build/godot` (RelWithDebInfo, `OPENGOLD_BUILD_GODOT=ON`) on first use,
builds it, runs its tests, and writes the extension to `demos/godot/bin/`. Later
runs reuse the existing configuration; pass CMake options (for example
`-DPython3_EXECUTABLE=...`) to force a reconfigure. Close any running demo
before rebuilding, or Windows will not let the DLL be replaced.

The equivalent commands in a developer environment are:

```powershell
cmake -S . -B build/godot -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENGOLD_BUILD_GODOT=ON
cmake --build build/godot
ctest --test-dir build/godot --output-on-failure -j 8
```

macOS (see [SPRITE-DEMO.md](SPRITE-DEMO.md); only the sprite demo has been
checked on macOS):

```bash
cmake -S . -B build/sprite-demo -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DOPENGOLD_BUILD_GODOT=ON -DOPENGOLD_BUILD_GAME=OFF
cmake --build build/sprite-demo --target opengold_godot -j 8
ctest --test-dir build/sprite-demo -L demo --output-on-failure
```

### Launch a demo

Each Windows launcher refreshes the Godot import cache, then opens its scene.
Each requires the game build or the demo extension, as listed; the map inspector instead requires
`build/opengold_maps.exe` from `.\build.cmd`.

| Demo | Windows launcher | Scene | Requires | Documentation |
| --- | --- | --- | --- | --- |
| Character creation and party | `demos\review-character.cmd` | `character_creation.tscn` | game | [CHARACTER-CREATION.md](CHARACTER-CREATION.md) |
| Combat | `demos\review-combat.cmd` | `combat_demo.tscn` | game | [RULES.md](RULES.md), [COMBAT-DEMO.md](COMBAT-DEMO.md) |
| Rolf's tour and New Phlan | `demos\review-rolf.cmd` | `rolf_tour.tscn` | game | [ROLF.md](ROLF.md), [PHLAN.md](PHLAN.md) |
| Sound board | `demos\review-sounds.cmd` | `sound_board.tscn` | extension | [sound-format.md](sound-format.md) |
| Equipment sprites | `demos\review-equipment.cmd` | `equipment_sprite_demo.tscn` | extension | [demos/README.md](../demos/README.md#equipment-sprite-demo) |
| Combat sprite scale | `demos\review-sprites.cmd` | `combat_sprite_demo.tscn` | builds extension itself | [SPRITE-DEMO.md](SPRITE-DEMO.md) |
| Combat body review | `demos\review-combat-bodies.cmd` | `combat_body_review.tscn` | builds extension itself | [COMBAT-BODY-ASSIGNMENTS.md](COMBAT-BODY-ASSIGNMENTS.md) |
| Map inspector | `demos\review-maps.cmd` | `map_inspector.tscn` | `opengold_maps` | [MAPS.md](MAPS.md) |
| Monster art review | `demos\review-art.cmd` | `monster_art_review.tscn` | — | [monster-art-mapping.md](monster-art-mapping.md) |

A typical session from PowerShell at the repository root:

```powershell
$env:OPENGOLD_GAME_DIR = 'C:\Games\POOLRAD'
.\build-opengoldbox.cmd
.\demos\review-combat.cmd
```

On macOS, the sprite demo has a bash launcher, `bash demos/review-sprites.sh`.
Any other scene can be opened directly with Godot after building the extension:

```bash
godot --headless --editor --path demos/godot --import --quit
godot --path demos/godot res://scenes/combat_demo.tscn
```

Opening `demos/godot/project.godot` in the Godot editor runs the graphics
comparison scene by default.

## Other checks

- **Godot tests registered?** The Godot checks are added only when the tree was
  configured with a Godot option *and* CMake found Godot. Missing checks look
  like a clean run; confirm with `ctest -N` in the build tree.
- **Localization:** `python tools/localization.py --check` is a separate gate
  that no build runs. See [LOCALIZATION.md](LOCALIZATION.md).
- **Tests needing original data** use it only when `OPENGOLD_GAME_DIR` is set. See
  [TESTING.md](TESTING.md) and the game README.
