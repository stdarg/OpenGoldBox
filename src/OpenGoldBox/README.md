# OpenGoldBox game source

This is the editable game application, copied from the character demo. It starts
at character creation and retains the connected party, town, combat, inventory,
save, and advancement flow. The reference demos remain under `demos/`.
This starting point does not claim a complete campaign.

The C++20 code builds as a Godot GDExtension. `godot/` contains the game's own
scenes and theme. Godot exports the Windows executable; no demo project or
installed Godot editor is needed when running the exported game folder.

## Build

### macOS (Apple silicon and Intel)

Install CMake 3.24+, Apple Command Line Tools, Python 3.8+, Godot 4.5+
and its matching macOS export templates. From a macOS bash shell at the repository root:

```bash
cmake --preset macos-universal
cmake --build --preset macos-universal
ctest --preset macos-universal
open mac-package/OpenGoldBox.app
```

The `macos-universal` preset compiles the game GDExtension for `arm64` and
`x86_64` and populates `mac-package/OpenGoldBox.app`. The executable, GDExtension,
packed scenes and artwork, and native rules files are all inside that app bundle.
The build checks for those files before reporting success. The app is ad hoc signed for local
testing. Settings are saved
in Godot's per-user data directory because app bundles are not writable after
installation. Distribution signing and notarization are separate release steps.

### Windows

Use the existing Visual Studio/CMake/Godot prerequisites. Install a Windows x64
debug export template matching the Godot editor. The default template location
is `build/export-templates/windows_debug_x86_64.exe`; alternatively pass
`-DOPENGOLDBOX_DEBUG_TEMPLATE=C:/path/to/windows_debug_x86_64.exe` to the build helper.
Templates come from the official Godot release's export-template archive and
are local build dependencies, not repository content.

From the repository root in PowerShell:

```powershell
.\build-opengoldbox.cmd
.\win-package\opengoldbox.exe
```

The game requests a **1920 x 1080** window (16:9, excluding window borders).
The minimum resizable game area remains 1120 x 800. To launch fullscreen at the
monitor's dimensions, run `./win-package/opengoldbox.exe --fullscreen`.

By default startup opens character generation directly. To show the two splash
screens first, run `./win-package/opengoldbox.exe --splash`. Any key advances
from the OpenGoldBox image to the Pool of Radiance image, then to character
generation. The background appears immediately and the first lettering fades
in over 0.6 seconds. Advancing clears it immediately and fades the second
lettering in over 0.6 seconds. Keys remain responsive during either fade;
Escape skips directly to character generation from either image.
Both splash screens use the same static background texture; only the transparent
lettering overlay changes. The background is identical to the
pixel throughout the transition. The lettering restores the reference
image's beveled metallic gold title and slender ivory serif supporting text.
The shared generator output is 1672 x 941 and fits the display proportionally;
other aspect ratios use black letterboxing. The two screen previews in `art/`
are 1920 x 1080 captures from Godot. The build packages the shared background and both transparent lettering layers.
See `art/OpenGoldBoxScreens.provenance.md` for sources and prompts.

Close the main window with its X button, or press Ctrl+X from any screen or
dialog, to exit through the same normal shutdown path. The scene tree and native
resources are released as Godot shuts down. Ctrl+X does not advance splash screens.

The CMake target is `OpenGoldBox`. Its output is
`win-package/opengoldbox.exe`. Every build creates and provisions `win-package/`
at the repository root with the PCK (scenes, theme, and portraits), GDExtension
DLL, rules data and notices, and Visual C++ runtime DLLs. Keep the whole package
folder together. Native code uses `RelWithDebInfo` and the redistributable runtime;
the Godot debug template retains development checks. Visual Studio is not needed
to run the package.
To build the executable target directly after configuring:

```powershell
cmake --build build/game --target OpenGoldBox
```

The game reads original assets from the path saved in `settings.cfg` beside the
executable, with `OPENGOLD_GAME_DIR` as a runtime override. Missing settings prompt
for the language, then the game folder. `--reset-game-path` and `--reset-lang` force
selection again. See [configuration](../../docs/CONFIGURATION.md) for portable
settings, environment overrides and checksum warnings. The game preserves the
existing per-user save directory. Rules and
authored portraits are copied from root `data/` and `art/` at build time.
No original game assets are copied or exported. Diagnostic captures use
`user://checks` so exported builds do not depend on the repository layout.

The game copy delays resize handling until the town view's dynamic controls
exist, avoiding the `SaveGame` startup crash observed in the reference demo.

Validation: the game build and all 12 native test suites pass. The exported
`opengoldbox.exe` passes headless character and party integration checks,
including tour, shops, combat, recovery, and XP awards, from the output folder.
These automated checks do not establish full campaign completion.

Startup input checks (run once without `--splash` and once with it):

```powershell
godot_console --headless --path src/OpenGoldBox/godot --script ../../../tests/startup_tests.gd
godot_console --headless --path src/OpenGoldBox/godot --script ../../../tests/startup_tests.gd -- --splash
```

These cover key progression, Escape from either splash, and ignored mouse,
key-release, and held-key repeat events. Both screens were also visually checked
at the 1920 x 1080 target size; the rendered check verifies identical background
pixels across the text transition.

Shutdown checks use `tests/shutdown_tests.gd` with `--shutdown-close` or
`--shutdown-key`; optional `--splash`, `--shutdown-second`, `--shutdown-dialog`,
and `--shutdown-paused` cover other active views. Tests verify one shutdown
request and scene teardown. The packaged executable also passed a native
Windows `WM_CLOSE` check with exit code 0.

Combat terrain, figures, and battlefield overlays render at 3x magnification.
Use the mouse wheel to scroll vertically, Shift+wheel to scroll horizontally,
or hold the middle mouse button and drag to pan. Both scrollbars are always
visible and support keyboard focus; arrow keys pan one square when the view or
a scrollbar has focus. The camera centers on the active combatant
at the start of a turn and after they move; manual panning persists otherwise.
Menus and the combat log retain their normal size.

Combat viewport checks:

```powershell
godot_console --headless --path src/OpenGoldBox/godot --script ../../../tests/combat_view_tests.gd
```

### Overhead map and fog

The overhead map remembers where the party has walked and what it has seen in
the 3D exploration view. Walls and opaque doors hide cells behind them;
transparent openings reveal the visible areas beyond. Unknown areas are solid
black. Revealed areas remain visible after moving away, changing districts, and
saving/loading. The full-map toggle has been removed.

Launch with `--no-fog` to inspect the entire overhead map. This override does
not change saved exploration history. On macOS:

```bash
open -n mac-package/OpenGoldBox.app --args --no-fog
```

For an editor run, put custom arguments after Godot's separator:

```bash
/Applications/Godot_mono.app/Contents/MacOS/Godot --path src/OpenGoldBox/godot -- --no-fog
```

On Windows, run `.\win-package\opengoldbox.exe --no-fog`. Launch normally to
restore fog. Existing saves remain readable; their visited history is retained.

Graphical integration checks can run the tour with `--tour-check --capture`,
once normally and once with `--no-fog`. Each captured pause verifies all overhead
cell centers against the actual framebuffer, excluding the party arrow.

### Goliath combat rendering

Goliaths occupy one logical square. Their visible sprite is stretched to one
square wide and 1.25 squares tall, with feet aligned to the occupied square's
bottom edge. Monsters may stand in the square above. Combat sprites draw from
lower rows to upper rows, keeping occupancy and targeting independent of the
artwork. See the [technical rendering rules](../../docs/TECH.md#goliath-combat-sprites-and-draw-order).

The native `opengold_combat_sprite_layout_tests` checks sizing and ordering;
`opengold_party_tests` checks movement into the square above a Goliath.
For a graphical check using original assets and the real character pool:

```bash
OPENGOLD_GAME_DIR=/path/to/POOLRAD OPENGOLD_LANG=en godot \
  --path src/OpenGoldBox/godot --script ../../../tests/goliath_render_tests.gd \
  -- --capture
```

When a packaged portrait cannot be loaded, combat composes one from the
original art. To check that this works for a Goliath's additional head:

```bash
OPENGOLD_GAME_DIR=/path/to/POOLRAD OPENGOLD_LANG=en godot --headless \
  --path src/OpenGoldBox/godot --script ../../../tests/portrait_fallback_tests.gd
```

## Screenshots

Press **Ctrl+S** at any time to save timestamped PNGs in **user://screenshots**.
This works in setup, game screens, focused dialogs, and while paused. Each capture
saves the main game view and a separate image for every visible game dialog;
embedded dialogs are also visible in the main image. Earlier captures are kept.
A brief notice at the bottom left confirms the folder or explains a failure.
The notice does not take focus and is excluded from the next capture.

Default folders:

- macOS: `~/Library/Application Support/Godot/app_userdata/OpenGold/screenshots`
- Windows: `%APPDATA%\Godot\app_userdata\OpenGold\screenshots`

For agent inspection while the game is running, from the repository root:

```bash
python3 tools/screenshot.py
```

The command requests a new capture, waits for completion, and prints its image
paths. These paths can be opened directly by an image viewer or agent image tool.
It does not change scenes or restart the game. It reports a timeout if the game
is not running. PNG capture requires a graphical run; `--headless` reports that
rendering is unavailable.

For development, choose an absolute folder when launching Godot. In macOS bash:

```bash
export OPENGOLD_SCREENSHOT_DIR="$PWD/build/screenshots"
godot --path src/OpenGoldBox/godot
```

In another terminal, run `python3 tools/screenshot.py --directory build/screenshots`.
Use a different folder for each simultaneously running instance.
The service watches `capture.request` in that folder, and publishes `latest.json`
after capture with the request ID, success/error, and image paths/dimensions.
The helper publishes a complete request atomically, preserves other pending
requests, and removes its own request on timeout. Godot callers can also call
`/root/Screenshots.request_capture()` and listen for `capture_completed(files, error)`.

## Tests

Native CTest checks run in both Debug and release configurations. With
`OPENGOLD_BUILD_GAME=ON` and `OPENGOLD_BUILD_TESTS=ON`, CTest also runs the combat
canvas, native node ownership, and keyboard/window/dialog shutdown checks in
headless Godot. Screenshot tests also cover input, pause, request/report handling,
unavailable rendering, unwritable output, and the external helper's request cleanup.
The setup fixture builds and imports the extension automatically;
these checks require neither export templates nor original game files.

To run only these Godot checks from a macOS bash shell:

```bash
ctest --preset macos-universal -L godot
```

The shutdown cases force first-run setup and do not save configuration. Test
processes have timeouts; script errors and missing completion markers fail the run.
Original-data integration still requires `OPENGOLD_GAME_DIR` and runs separately
from these synthetic checks.

Combat handoff regression checks require the installed original assets. From
macOS bash (replace `/path/to/POOLRAD` with the installation folder):

```bash
OPENGOLD_GAME_DIR=/path/to/POOLRAD OPENGOLD_LANG=en godot --headless \
  --path src/OpenGoldBox/godot --script ../../../tests/combat_handoff_tests.gd
```

This checks that rejected party profiles keep the party screen and diagnostic
visible, release combat ownership, and permit a corrected party to start combat.
Omit `--headless` and append `-- --capture` to save both screens as PNGs.
Native party tests also cover failed encounter rollback of map, script, HP,
resources and reward state, and navigation after acknowledging the error.

To verify actual screenshot pixels, run `tests/screenshot_tests.gd` in graphical
Godot with `OPENGOLD_SCREENSHOT_DIR` set to an isolated output folder. It checks
saved PNG dimensions and colors, native dialog capture, unique filenames,
notification placement, and failure handling. `tests/run_godot_test.cmake`
accepts `-DGRAPHICAL=ON` for the same timeout/error/completion checks used by CTest.

## Language selection

English and Spanish are available. Launch `win-package\opengoldbox.exe --reset-lang`
from CMD to select **English** or **Español**. The selection is saved for later
launches. Combine with `--splash` to show the translated splash lettering after
selection. See [translation setup and coverage](../../docs/LOCALIZATION.md).

## Saving throws and conditions

Level 3–4 Clerics and Wizards can select **Blindness** during advancement. In
combat, select its button and click a highlighted target. The spell uses a
level-two slot; the roster and log show its condition and saving throws. Effects
persist through combat checkpoints and campaign handoff. See
[status effects](../../docs/STATUS-EFFECTS.md) for scope and the `--conditions`
fixture in the existing combat scene.
