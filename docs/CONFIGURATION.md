# First-run setup and configuration

The packaged game saves `settings.cfg` in the player's user data folder, beside
the campaign saves: `%APPDATA%\Godot\app_userdata\OpenGold\settings.cfg` on
Windows and `~/Library/Application Support/Godot/app_userdata/OpenGold/settings.cfg`
on macOS. The game's own folder can be read-only. A `settings.cfg` left beside
`opengoldbox.exe` by an earlier build is no longer read; the first launch asks for
the language and the game folder again.

```ini
[game]
path="D:/Games/POOLRAD/GAME/POOLRAD"
difficulty="normal"

[interface]
language="es"

[combat]
combat_zoom=100
encounter_challenge=33

[monsters]
legendary="hard"
```

Use the actual folder containing `START.EXE`, `ITEMS`, and the original `.DAX`
archives, not the Steam launcher directory. Store an absolute folder path.
The supported language codes are `en` and `es`.

`difficulty` is one of `easy`, `normal`, `hard`, `nightmare` and `infernal`,
easiest first; the default, also used for an unknown value, is `normal`. On
`hard` and above monsters use the potions, wands and scrolls they carry. The
game reads it when exploration starts or a save is loaded, and new settings
files receive the default. See
[DIFF-1](SRD-DECISIONS.md#diff-1-2026-10-21-difficulty-and-monsters-that-use-their-items).

The in-game menu's **Options** screen (Escape while exploring) sets
`difficulty`, `encounter_challenge` and the `[monsters]` section below, and
writes them at once.

`[monsters]` switches monster abilities by difficulty. Each key names an
ability (`legendary`, `parry`, `breath`; the
[rules README](../data/rules/srd-5.2.1/README.md) lists them all) and its value
the lowest difficulty it is on at, or `never`. An ability without a key is on
at every difficulty, and a value that is neither a difficulty nor `never` keeps
the default. By default Legendary Actions (Tyranthraxus's and the vampire's)
are on from `hard`; new settings files receive that key. The combat screen
reads the section when a fight starts. See
[MONSTER-2](SRD-DECISIONS.md#monster-2-2026-10-25-legendary-actions-and-monster-riders).

The combat screen reads `combat_zoom` from `[combat]` when it opens. The value
is a percentage, clamped to 10–1000. The default is 100, also recorded in the
Godot project config. New settings files receive this value automatically.
It controls the battlefield canvas, input coordinates, and centering; menus and
text retain their normal size.

`encounter_challenge` sizes original monster encounters. It is a percentage of
the SRD 5.2.1 Moderate XP budget for the party, clamped to 0–200; the default is
33. An encounter whose monsters are worth more XP than that budget, or that has
more monsters than living characters, is scaled back proportionally, keeping at
least one of each kind. Encounters are never enlarged, and experience and loot
remain the original encounter's. The game reads the value when exploration
starts or a save is loaded, and new settings files receive the default. See
[MON-1](SRD-DECISIONS.md#mon-1-2026-10-02-original-monsters-as-srd-stat-blocks).

## Startup and reset controls

If the config is absent, the game asks for the language first, then the game folder.
If only one value is missing or invalid, it asks only for that value. A saved
folder that disappears or lacks required files must be selected again.
Setup completes before the optional splash screens.

From **cmd.exe**:

```bat
win-package\opengoldbox.exe --reset-lang
win-package\opengoldbox.exe --reset-game-path
win-package\opengoldbox.exe --reset-game-path --reset-lang --splash
```

`--lang` has been removed. `--reset-lang` forces language selection;
`--reset-game-path` forces folder selection. Neither deletes the previous value
on launch. Cancel, the dialog X, and Ctrl+X exit through normal shutdown.
Only a confirmed replacement is saved. If the language is confirmed and the user
then cancels folder selection, the confirmed language remains saved.

The folder dialog accepts a typed path or **Browse...**, followed by **Continue**.
The language dialog offers native names **English** and **Español**. Highlighting
a language previews its dialog translation; Enter or the translated Continue
button confirms it. Initial setup text uses the active language when available,
otherwise the system language with English fallback.

Writes preserve unrelated config entries and replace the destination through a
temporary file. An unreadable/malformed config or a write failure displays an
error and leaves the saved file unchanged. Correct its syntax or permissions
and retry. No config is silently written to a different folder.

## Environment overrides

```bat
set "OPENGOLD_GAME_DIR=D:\Games\POOLRAD\GAME\POOLRAD"
set "OPENGOLD_LANG=es"
win-package\opengoldbox.exe
```

An environment value overrides the saved value for that launch. Language
overrides accept `en` and `es` (case-insensitive); unsupported values are ignored.
Environment overrides are not automatically persisted. Missing config values
still prompt; overrides supply the initial path/language when applicable.

A reset flag supersedes its corresponding environment override for that run.
An explicit confirmed dialog selection also takes precedence for the remainder
of the run. On the next ordinary launch, environment overrides apply again.
Every game view and campaign-save identity check uses the same resolved path.

## MD5 compatibility warning

Startup compares 115 file fingerprints against
`src/OpenGoldBox/godot/config/por-pc13-md5.json`. This manifest records the local
Steam PC 1.3 reference installation used for current development. It contains
only filenames and MD5 hashes, not original game content. The covered files are
`START.EXE` (sound data), `ITEMS`, and all 113 `.DAX` archives (including archives
read by the campaign asset-identity check). The installation has no `.DAT` files.
Other executables/overlays that OpenGoldBox does not read are excluded.

Missing or unreadable required files return to folder selection. Different MD5
values show a scrollable filename list with **Quit to OS** and **Continue**.
Quit preserves the previous settings; Continue accepts that compatibility risk
for this launch and saves the folder if it was being selected. The warning
appears again on later launches while the files differ. It does not certify
other revisions as compatible or change the existing native decoder checks.

These hashes identify a known local revision; they are not a security or
authenticity guarantee. The reference is bundled and never relearned from an
arbitrary folder selected by a player. Adding a supported revision requires a
reviewed manifest change and decoder validation.

## Development and checks

Godot editor runs use `src/OpenGoldBox/godot/settings.cfg` rather than placing
project preferences beside the shared Godot executable. This development file
and its temporary file are ignored by Git and explicitly excluded from exports.

`tests/setup_config_tests.gd` exercises first run, partial settings, complete
settings, resets, environment precedence, invalid folders, synthetic checksum
mismatches, warning Continue/Quit, cancellation, preservation of unrelated keys,
and malformed-config save failure. It creates synthetic files rather than
copying original game assets. It requires `OPENGOLD_GAME_DIR` pointing to the
local reference installation and writes the ignored development config; back up
that file before running the test. Tests should use a workspace-local APPDATA.
`tests/language_dialog_tests.gd` checks keyboard progression, saved-language
restoration and the shared splash background with the new reset flag.
CTest runs both, and `tests/startup_tests.gd`, through `tests/settings_check.cmake`
(`opengold_godot_setup_*`, `opengold_godot_language*`, `opengold_godot_startup*`;
skipped without `OPENGOLD_GAME_DIR`), which keeps the development config aside
while each check runs and puts it back afterwards.

See [LOCALIZATION.md](LOCALIZATION.md) for catalogs and translated splash assets.
