# Building OpenGoldBox and the demos

This guide covers the three separate builds in this repository: the native core
and its tests, the OpenGoldBox game, and the reference demos. Install the
prerequisites in [INSTALL.md](INSTALL.md) first.

| Build | Command (Windows) | Build tree | Output |
| --- | --- | --- | --- |
| Native core and tests | `.\build.cmd` | `build/` | libraries, tests, `opengold_maps` |
| Game | `.\build-opengoldbox.cmd` | `build/game/` | `win-package/opengoldbox.exe` |
| Demos | `.\demos\build-rolf.cmd` | `build/godot/` | `demos/godot/bin/opengold_godot.dll` |

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
./build.sh
```

Both scripts configure the `default` preset (RelWithDebInfo, Ninja), build, then
run the tests in parallel, and stop at the first failure. They always build the
checkout they live in, whatever the current directory. The equivalent manual
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

### macOS

Install the Godot macOS export templates matching your editor, then:

```bash
cmake --preset macos-universal
cmake --build --preset macos-universal
ctest --preset macos-universal
open mac-package/OpenGoldBox.app
```

This builds a universal (`arm64` and `x86_64`) app bundle, ad hoc signed for
local testing. The preset uses Ninja, which compiles on every core; a tree
configured earlier with Unix Makefiles must be deleted once
(`rm -rf build/macos-universal`) before it configures again.

## The demos

The demos are reference scenes in `demos/godot/`, backed by the C++ extension in
`demos/src/OpenGold.Godot/`. They are separate from the game. See
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
Most require the demo extension; the map inspector instead requires
`build/opengold_maps.exe` from `.\build.cmd`.

| Demo | Windows launcher | Scene | Requires | Documentation |
| --- | --- | --- | --- | --- |
| Character creation and party | `demos\review-character.cmd` | `character_creation.tscn` | extension | [CHARACTER-CREATION.md](CHARACTER-CREATION.md) |
| Combat | `demos\review-combat.cmd` | `combat_demo.tscn` | extension | [RULES.md](RULES.md), [COMBAT-DEMO.md](COMBAT-DEMO.md) |
| Rolf's tour and New Phlan | `demos\review-rolf.cmd` | `rolf_tour.tscn` | extension | [ROLF.md](ROLF.md), [PHLAN.md](PHLAN.md) |
| Sound board | `demos\review-sounds.cmd` | `sound_board.tscn` | extension | [sound-format.md](sound-format.md) |
| Equipment sprites | `demos\review-equipment.cmd` | `equipment_sprite_demo.tscn` | extension | [demos/README.md](../demos/README.md#equipment-sprite-demo) |
| Combat sprite scale | `demos\review-sprites.cmd` | `combat_sprite_demo.tscn` | builds extension itself | [SPRITE-DEMO.md](SPRITE-DEMO.md) |
| Combat body review | `demos\review-combat-bodies.cmd` | `combat_body_review.tscn` | builds extension itself | [COMBAT-BODY-ASSIGNMENTS.md](COMBAT-BODY-ASSIGNMENTS.md) |
| Map inspector | `demos\review-maps.cmd` | `map_inspector.tscn` | `opengold_maps` | [MAPS.md](MAPS.md) |
| Monster art review | `demos\review-art.cmd` | `monster_art_review.tscn` | — | [monster-art-mapping.md](monster-art-mapping.md) |

A typical session from PowerShell at the repository root:

```powershell
$env:OPENGOLD_GAME_DIR = 'C:\Games\POOLRAD'
.\demos\build-rolf.cmd
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
- **Tests needing original data** are not part of `ctest`. See
  [TESTING.md](TESTING.md) and the game README.
