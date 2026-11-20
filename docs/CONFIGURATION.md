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

[games]
CURSE="D:/Games/CURSE"

[interface]
language="es"

[combat]
combat_zoom=100
encounter_challenge=33
keep_quick="off"

[monsters]
legendary="hard"

[original_shops]
POOLRAD="on"

[darkness_gazes]
DARKNESS="off"

[command_every_round]
buck-rogers-countdown-to-doomsday="off"

[empty_helm_dodges]
buck-rogers-countdown-to-doomsday="off"

[boarding_fuel]
buck-rogers-countdown-to-doomsday="on"

[design_difficulty]
UA="both"

[art]
directory="/Users/you/OpenGoldBox-new-art"
walls="new"
combat="new"
portraits="new"

[gear]
auto_equip="on"
unidentified_magic="skip"

[sound]
effects=100
music=100
```

Use the actual folder containing `START.EXE`, `ITEMS`, and the original `.DAX`
archives, not the Steam launcher directory. Store an absolute folder path.
The supported language codes are `en` and `es`.

After the language, the startup menu offers Options, a button for each game
and Exit to desktop. Choosing a game asks for its folder the first time:
`[game] path` holds *Pool of Radiance*'s, and `[games]` the others' by their
folder name (`CURSE` for *Curse of the Azure Bonds*, `SECRET` for *Secret of the
Silver Blades*, `GATEWAY` for *Gateway to the Savage Frontier*, `DARKNESS` for
*Pools of Darkness*, `TREASURE` for *Treasures of the Savage Frontier*). A game
whose folder is not saved, or whose saved folder no longer holds its files, is
looked for by its files, whatever its folder's name: in the folders
`OPENGOLD_GAME_DIR` and `OPENGOLD_GOLDBOX_DIR` name and their children and
grandchildren, then upward from the game's own folder to the home folder
([TECH.md](TECH.md#finding-the-games-folders)). When it is found, the game
shows the folder and asks whether to use it: **Use this folder** saves it, so
the question comes once; **Choose another folder** opens the folder dialog for
the player to pick one, which is saved. When it is not found, the folder
dialog opens. Loading another game's save in play asks the same way before
that game plays; going back returns to the load screen. Bringing characters
in from an earlier game's save asks the same way before the save is read;
going back returns to the import. A party still takes its portraits and combat
art from *Pool of Radiance*'s files, so choosing another game asks for that
folder too when it is not set ([GAMES.md](GAMES.md)). `--game=POOLRAD`, `--game=CURSE`, `--game=SECRET`, `--game=GATEWAY`, `--game=DARKNESS` or `--game=TREASURE`
on the command line picks the game without the menu, as the packaged game's own checks
do.

*Unlimited Adventures*' folder (`[games] UA`) is the construction kit's, the
one holding `CKIT.EXE` and `DISK1` to `DISK3`. Choosing it then asks each time
for the design to play, a folder the kit names `NAME.DSN`, offering the last
one, which `[games] UA_DESIGN` keeps; `--game=UA` plays that one without
asking ([games/ua.md](games/ua.md), From the startup menu).

`difficulty` is one of `easy`, `normal`, `hard`, `nightmare` and `infernal`,
easiest first; the default, also used for an unknown value, is `normal`. The
Buck Rogers games add `novice`, below `easy`, and show the levels under
their own names: Novice, Cadet, Trooper, Veteran, Ace and Admiral
([BUCK-ROGERS.md](BUCK-ROGERS.md), Difficulty). Any other game plays `novice`
as `easy`. On
`hard` and above monsters use the potions, wands and scrolls they carry. Each level
also sets how many monsters a fight has and how strong they are ([DIFF-2](SRD-DECISIONS.md#diff-2-2026-11-02-each-level-changes-the-fights-not-only-the-tactics)). The
game reads it when exploration starts or a save is loaded, and new settings
files receive the default. See
[DIFF-1](SRD-DECISIONS.md#diff-1-2026-10-21-difficulty-and-monsters-that-use-their-items).

The in-game menu's **Options** screen (Escape) sets these settings and writes
them at once: under Gameplay `difficulty`, `encounter_challenge`, the
`[gear]` section, `keep_quick`, `[original_shops]`, `[darkness_gazes]`, the ship-combat
options, `[design_difficulty]` and the `[monsters]` section below; under Interface `language`, which takes effect the
next time the game starts, `combat_zoom`, read when a fight opens, the
`[art]` section below, which takes effect at once, and `[sound]`.

`[sound]` holds the Options screen's two volume sliders (Interface), the same
for every game: `effects` for the games' sound effects (a fight's sounds, the
storm's thunder, footsteps, a title's sound) and `music` for a title's music.
Each is a percentage, 0 (silent) to 100, the default, which is the loudness
the games always played at. A change applies at once to the sounds of the
exploration screen and a fight; the startup menu's title sounds and music take
it the next time they start.

`[art]` chooses between the new art and the original game's (#274): `walls`
for the walls and animated close-ups of the 3D view, `combat` for the creatures in a fight (monsters
and Wild Shape's and summoned forms; party members keep theirs), `portraits`
for the characters' portraits (`original` shows their original head-and-body
portraits). Each is `new` or `original`; the default, also used for any other value, is `new`, and new
settings files receive it. The Options screen's 3D view art, Combat art and
Portraits switch them, redrawing the view or the fight on screen at once. With `new`,
any piece of new art the game cannot load (a wall look, a close-up, a
creature, or the party's combat looks) shows the original art it replaces,
and the game's log names the file it could not load.

The new art is a separate, optional download (`new-art-pack` in the
repository). `directory` names its folder, absolute; the Options screen's
**New art folder** (Interface) chooses it, accepting only a folder holding
the download's `new-art-pack.json`, and the game shows the new art at once,
loading the game in progress again with it. It can change whenever the menu
could load a game, not during a fight or an event. Without a folder, or with
one that is not the new art, the game shows only the original art, whatever
`walls`, `combat` and `portraits` say. The environment variable
`OPENGOLD_NEW_ART_DIR` overrides `directory`; the game's checks and developer tools set it to the
repository's `new-art-pack`.

`[gear]` is auto-equip ([EQUIP-1](SRD-DECISIONS.md#equip-1-2026-11-05-auto-equip-better-gear)):
`auto_equip` is `on` or `off`, and `unidentified_magic` is `skip`, which
leaves an unidentified magic item in the pack, or `equip`. The defaults, also
used for any other value, are `on` and `skip`. The Options screen shows
"Do not equip unidentified magic items" only while auto-equip is on; both take
effect when the menu closes, for the next item a character gains.

`keep_quick` is the Options screen's Keep Quick after a fight. `off`, the
default, also used for any other value, returns every character on Quick to
the player when a fight ends, win, lose or flee; `on` keeps them on Quick for
the next fight, as the original did. The combat screen reads it when a fight
starts.

`[original_shops]` is the Options screen's Original shops, one key per game,
named by its folder (`POOLRAD`). `on`, the default, also used for any value
but `off`, keeps the game's shops as they were in the original: the original's
items at their records' values. `off` lets the game's modified shops sell:
Pool of Radiance's New Phlan adds the Rogue's Shop, behind its secret door,
and OpenGoldBox's gear (magic ink, the rogue's kits and poisons) at
OpenGoldBox's prices. Only Pool of Radiance has modified shops so far. It is
read when a game starts or loads.

`[darkness_gazes] DARKNESS` is the Options screen's Darkness's gazes
(the user, 2026-11-15). *Pools of Darkness*'s paralyzing gaze (its effect
114, the gaze spider's) and charming gaze (its 124, the vampires') have
handlers its original registers but no fight runs. `on` plays them as those
handlers describe: the paralyzing gaze holds its target for 10 rounds unless
it saves against paralysis at +3, and the charming gaze casts Charm Person.
`off`, the default, also used for any value but `on`, leaves them doing
nothing, as in the original. The combat screen reads it when a fight
starts.

The ship-combat options are the Buck Rogers games' (the user, 2026-11-20;
[games/countdown.md](games/countdown.md), "Ship combat"), one key per game
named by its folder (`buck-rogers-countdown-to-doomsday`,
`buck-rogers-matrix-cubed`). The Options screen shows them, in the place of
Original shops, Gateway's money and Darkness's gazes, only while a Buck
Rogers game is the game, and sets that game's. Each default is the Rule
Book's or OpenGoldBox's, and the other value plays the original's way
([DEVIATIONS.md](DEVIATIONS.md)):

- `[command_every_round]`, COMMAND every round: `off`, the default, also
  used for any value but `on`, offers a crew member COMMAND only while the
  pilot is dead or incapacitated, as the Rule Book says. `on` offers it once
  a round even with the pilot well, as the original does, so a second member
  may ram again in the same round.
- `[empty_helm_dodges]`, Empty helm dodges: `off`, the default, also used
  for any value but `on`, lets a ship with no one at the helm dodge a ram
  with 0, and the enemy weighs that. `on` dodges by the ship's own pilot
  value, 100 for the Maelstrom Rider, as the original does.
- `[boarding_fuel]`, Fuel from a boarded ship: `on`, the default, also used
  for any value but `off`, transfers the captured ship's fuel after a
  boarding, as the Rule Book says. `off` takes none, as the original does.

A ship fight takes them as it starts (`ShipCombatOptions`,
`buck_rogers_ship_combat.h`); the game plays ship fights from step B3 of
[BUCK-ROGERS.md](BUCK-ROGERS.md).

`[design_difficulty] UA` is the Options screen's Design difficulty: how
`difficulty` plays in an *Unlimited Adventures* design (the user, 2026-11-15;
[games/ua.md](games/ua.md), Fights). `design` plays the kit's own five
levels: the party's side +2, +1, 0, -1 or -2 to hit and the monsters' Hit
Points x (level + 1) / 4, Easy the kit's level 1, Normal its 3, Hard its 4,
Nightmare and Infernal its 5. `table` plays OpenGoldBox's difficulty table
for the monsters only: Easy -2 to hit, 75% Hit Points and Armor Class 2
worse; Normal as recorded; Hard +1, 125% and 1 better; Nightmare +2, 150%
and 2 better; Infernal +3, 200% and 3 better. `both`, the default, also used
for any other value, plays the kit's levels with the table's to-hit and
Armor Class for the monsters. It is read when a design starts or loads and
when the menu closes, for the next fight.

`[monsters]` switches monster abilities by difficulty. Each key names an
ability (`legendary`, `parry`, `breath`; the
[rules README](../data/rules/srd-5.2.1/README.md) lists them all) and its value
the lowest difficulty it is on at, or `never`. An ability without a key is on
at every difficulty, and a value that is neither a difficulty nor `never` keeps
the default. By default Legendary Actions and Legendary Resistance
(Tyranthraxus's and the vampire's) are on from `hard`, and the abilities that
take a character out of the fight for long, leave a lasting harm or add foes
from `normal` ([MONSTER-4](SRD-DECISIONS.md#monster-4-2026-10-28-monster-abilities-by-difficulty));
new settings files receive those keys. The combat screen
reads the section when a fight starts. See
[MONSTER-2](SRD-DECISIONS.md#monster-2-2026-10-25-legendary-actions-and-monster-riders)
and [MONSTER-3](SRD-DECISIONS.md#monster-3-2026-10-28-the-last-monster-abilities).

The combat screen reads `combat_zoom` from `[combat]` when it opens. The value
is a percentage, clamped to 10–1000. The default is 100, also recorded in the
Godot project config. New settings files receive this value automatically.
It controls the battlefield canvas, input coordinates, and centering; menus and
text retain their normal size.

`encounter_challenge` sizes original monster encounters, before the difficulty
scales it. It is a percentage of
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
folder that disappears or no longer holds the game's files is looked for again
as if none were saved: a folder found is offered to confirm, else the folder
dialog opens. One that still holds the game but lacks a file *Pool of
Radiance* needs opens the folder dialog, naming the missing files.
Setup completes before the chosen game's title.

From **cmd.exe**:

```bat
win-package\opengoldbox.exe --reset-lang
win-package\opengoldbox.exe --reset-game-path
win-package\opengoldbox.exe --reset-game-path --reset-lang
```

`--lang` has been removed. `--reset-lang` forces language selection;
`--reset-game-path` forces folder selection, skipping the question about a
folder found. Neither deletes the previous value
on launch. In the language dialog Cancel, the dialog X and Ctrl+X exit through
normal shutdown; in a game's folder dialog Cancel, the X and Escape go back to
the games, as the X and Escape do from the question about a folder found.
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

`OPENGOLD_LANG` overrides the saved language for that launch; it accepts `en`
and `es` (case-insensitive), and unsupported values are ignored.
`OPENGOLD_GAME_DIR` and `OPENGOLD_GOLDBOX_DIR` name folders where a game whose
folder is not saved is looked for first, by its files: each folder, its
children and its grandchildren ([TECH.md](TECH.md#finding-the-games-folders)).
A saved folder that still holds its game comes before them. Environment values
are not saved until the player confirms the folder found in them. Missing
config values still prompt.

A reset flag supersedes its corresponding environment override for that run.
An explicit confirmed dialog selection also takes precedence for the remainder
of the run. On the next ordinary launch, environment overrides apply again.

`OPENGOLD_SETTINGS_FILE` names the `settings.cfg` to read and write instead of
the usual one, for that launch. The Godot checks give each check its own, so
they run at the same time without sharing settings.
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

## Crash reports

When a session does not shut down normally, whether from a crash, a force quit or
a power loss, the next launch writes a report to `crash-reports/` in the user data
folder, beside `settings.cfg`. Each is a text file named
`crash-<date>-<process id>.txt`, holding the game version, engine, system, and the
last 400 lines of that session's engine log and game log, with the backtrace when the
game crashed.
The ten newest are kept. The log is also in `logs/` in the same folder, where the
engine keeps the last five. Beside them the game keeps its own log of each session,
`opengold-<date>.log`, the newest five: the game's version and settings, every
action the player takes, every die rolled and what it was for, every command and
line of each fight, and the errors met, the engine's included
([TECH.md](TECH.md#diagnostic-log)). A crash report holds the end of both logs.

Nothing is sent anywhere; a player attaches the file to a bug report. Logs can
hold folder names, such as the player's account name in the path to the game files.

To read a macOS backtrace down to source lines, unzip the release's
`-macos-symbols.zip` and pass the `opengoldbox_game.dylib+<offset>` offsets from the
report (`arm64`, or `x86_64` on an Intel Mac):

```bash
atos -arch arm64 -l 0 -o opengoldbox_game.dylib.dSYM/Contents/Resources/DWARF/opengoldbox_game.dylib 0x10aa1c
```

To read a Windows backtrace, unzip the release's `-windows-symbols.zip` and pass
the `opengoldbox_game.dll+<offset>` offsets from the report:

```powershell
python tools\decode_windows_crash.py opengoldbox_game.pdb 22996e 95826
```

A running session marks itself in `sessions/` and clears the mark on a clean exit.
A second copy of the game leaves the first one's mark alone while it is running.

## Development and checks

Godot editor runs use `src/OpenGoldBox/godot/settings.cfg` rather than placing
project preferences beside the shared Godot executable. This development file
and its temporary file are ignored by Git and explicitly excluded from exports.

`tests/setup_config_tests.gd` exercises first run, partial settings, complete
settings, resets, environment precedence, invalid folders, synthetic checksum
mismatches, warning Continue/Quit, cancellation, preservation of unrelated keys,
malformed-config save failure, and a folder found: confirmed and not asked
again in a second process, declined for the folder dialog, found after a
saved folder lost its files, and *Curse of the Azure Bonds*' confirmed the
same way (`opengold_godot_setup_found_*`). Its other cases clear
`OPENGOLD_GOLDBOX_DIR`, so the game finds nothing. (`tests/saves_by_game_tests.gd`
loads a *Curse* save in *Pool of Radiance* from a folder found, confirmed and
declined, `opengold_godot_saves_by_game_found_*`.) It creates synthetic files rather than
copying original game assets. It requires the local reference installation of
*Pool of Radiance*, found by its files (`tests/find_games.cmake`) or named by
`OPENGOLD_GAME_DIR` when run by hand. Run by hand, it writes the ignored development
config, so back up that file first; under CTest it writes its own copy
(`OPENGOLD_SETTINGS_FILE`). Tests should use a workspace-local APPDATA.
`tests/language_dialog_tests.gd` checks keyboard progression, saved-language
restoration and the shared splash background with the new reset flag.
CTest runs both, and `tests/startup_tests.gd`, through `tests/settings_check.cmake`
(`opengold_godot_setup_*`, `opengold_godot_language*`, `opengold_godot_startup*`;
skipped without *Pool of Radiance*'s files), which gives each check a `settings.cfg` of
its own in its work folder, so the development config is never touched.

See [LOCALIZATION.md](LOCALIZATION.md) for catalogs and translated splash assets.
