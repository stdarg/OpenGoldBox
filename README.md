# OpenGoldBox

An open-source role-playing game engine for Gold Box games.

The project was previously named OpenGold. The display name and documentation
now use OpenGoldBox. Existing `opengold` C++ namespaces, build targets,
`OPENGOLD_GAME_DIR`, source-directory names, save-format identifiers, and the
GitHub repository URL keep their original names. Campaign saves continue to use
the original Godot user-data directory.

A modern Godot-based reimplementation of SSI's Gold Box engine that reads the
original game data and assets while adding a cleaner UI, improved rendering,
and modern quality-of-life features.

The current implementation targets **Pool of Radiance**. The game plays New
Phlan and the Slums; the full campaign is not yet playable. The engine uses
C++20 with Godot 4.x presentation and a shared GDExtension. Combat uses a bounded,
replaceable **SRD 5.2.1** rules module. Original game files must come from your
own installation and are decoded at runtime; they are not bundled.

## Current status

As of 2026-10-02. Each linked document holds the detail and its own evidence;
this section only summarizes.

### Play the game

The game is [`src/OpenGoldBox/`](src/OpenGoldBox/README.md). It starts at
character creation and needs your Pool of Radiance installation (the game asks
for the folder on first run, or set `OPENGOLD_GAME_DIR`).

- Windows (PowerShell): `.\build-opengoldbox.cmd`, then `.\win-package\opengoldbox.exe`.
- macOS (bash): `cmake --preset macos-universal`, `cmake --build --preset macos-universal`,
  then `open mac-package/OpenGoldBox.app`.

On 2026-10-02 (commit f43127d, macOS) the two macOS build commands above
succeeded, and with original files `ctest --preset default` passed 53 checks and
`ctest --preset macos-universal` passed 84. The Windows commands were not run for
this summary.

See [building the game and each demo](docs/BUILD.md). The scenes under
[`demos/`](demos/README.md) are frozen references, described under
[Reference demos](#reference-demos) below; they are not the game.

### What you can do

- **Party:** create level-one characters (nine species, twelve classes, four
  backgrounds) or choose from a 48-character pool; up to six PCs and two NPCs.
  See [character creation](docs/CHARACTER-CREATION.md) and [party](docs/PARTY.md).
- **New Phlan:** Rolf's tour, then the civilized town with its original
  dialogue, shops, inn rest, temple healing and street camping. Every armor the
  shops sell converts to SRD equipment; stock that cannot be equipped is marked
  before purchase. See [New Phlan](docs/PHLAN.md) and [recovery](docs/RECOVERY.md).
- **Slums:** leave by the west gate and meet the original roaming and scripted
  encounters. Monsters are SRD stat blocks, and encounters are sized to an XP
  budget set by `encounter_challenge`. Fights use the original battlefield
  geometry and award original loot and XP; locked doors can be bashed or
  picked, and camping can be interrupted. See [the expedition](docs/EXPEDITION.md).
- **Quest:** Ohlo's potion delivery can be completed from acceptance to reward.
  See [quests](docs/QUESTS.md).
- **Advancement:** manual level-up to level 4 for Fighters, Clerics, Wizards,
  Rogues, Paladins and Rangers. See [advancement](docs/ADVANCEMENT.md).
- **Saves:** named campaign slots from the party roster and idle exploration in
  New Phlan or the Slums; not during combat or an unfinished event. Older
  pre-release saves are refused. See [saves](docs/SAVES.md).
- Fog of war on the overhead map, timed status effects and saving throws, and
  English or Spanish text.

| Class | Levels | Class features in play |
| --- | --- | --- |
| Fighter | 1–4 | Fighting Style, Second Wind, Weapon Mastery, Action Surge, Tactical Mind, Champion, Ability Score Improvement or feat |
| Cleric | 1–4 | Sacred Flame; prepared Cure Wounds, Healing Word, Inflict Wounds and Blindness; Divine Order; Ability Score Improvement or feat. No Channel Divinity or subclass |
| Wizard | 1–4 | Five cantrips; spellbook and preparation (Magic Missile, Scorching Ray, Blindness); Arcane Recovery; Scholar; Ability Score Improvement or feat. No subclass |
| Rogue | 1–4 | Expertise, Sneak Attack, Weapon Mastery, Cunning Action (Dash, Disengage), Steady Aim, Ability Score Improvement or feat. No Hide or subclass |
| Paladin | 1–4 | Spellcasting (Charisma; Cure Wounds so far), Lay On Hands in combat, Weapon Mastery, Fighting Style, Ability Score Improvement or feat. Other features in progress; see [Paladin](docs/PALADIN.md) |
| Ranger | 1–4 | Weapon Mastery, Fighting Style, Ability Score Improvement or feat. No spells or other class features |
| Sorcerer, Warlock | 1 | Starting cantrips |
| Barbarian, Bard, Druid, Monk | 1 | Weapon attacks; Unarmored Defense (Barbarian, Monk); Weapon Mastery (Barbarian) |

Every class fights with weapons. Per-feature status is in the
[SRD coverage ledger](docs/SRD-COVERAGE.md) and the [spell inventory](docs/SPELL-INVENTORY.md).

### Limits

- Only New Phlan's civilized area and the Slums. Other districts and boat
  destinations are not reachable, and other Slums quests and hostile branches
  report a diagnostic and roll back.
- Most SRD spells and many class features are missing; see the ledger above.
- Magic items keep their original record but have no effect. Items cannot be sold.
- No saving during combat, no retreat once combat starts, no full campaign.

### Next work

The first-adventure milestone [#11](https://github.com/stdarg/OpenGold/issues/11)
is complete, with [#13](https://github.com/stdarg/OpenGold/issues/13),
[#14](https://github.com/stdarg/OpenGold/issues/14) and
[#15](https://github.com/stdarg/OpenGold/issues/15). Candidates for the next
milestone are broader original-script coverage beyond the town and the Slums
encounters ([#4](https://github.com/stdarg/OpenGold/issues/4)) and advanced combat
interactions ([#5](https://github.com/stdarg/OpenGold/issues/5)). The first
expedition loop with saves ([#1](https://github.com/stdarg/OpenGold/issues/1)) is
done; reward and training mappings stay with
[#9](https://github.com/stdarg/OpenGold/issues/9) and broader persistence with
[#10](https://github.com/stdarg/OpenGold/issues/10). Class and spell work is
indexed in [#186](https://github.com/stdarg/OpenGold/issues/186).

## Reference demos

The scenes below and their launchers are preserved under [`demos/`](demos/README.md)
for reference. They use the same native libraries as the game but have their
own scenes, fixtures and limits, and their launchers are Windows CMD scripts.

## Sound board

Play the 19 original PC-speaker effects from your installed `START.EXE`, including
unused effects, with a button for every sound-directory entry. The native demo
decodes everything in memory and includes stop, volume and mute controls.
Its reusable `SoundBank` and `SoundPlayer` classes are independent of Godot;
the scene uses a separate Godot audio adapter.
From PowerShell:

```powershell
.\demos\build-rolf.cmd
.\demos\review-sounds.cmd
```

See [sound data, supported release and verification](docs/sound-format.md).

## Rolf's tour

Run the original welcome through Rolf's farewell in a C++/Godot scene, with his
encounter sprite, the original Phlan wall and door artwork, and a synchronized party map.
After the farewell, explore New Phlan and its buildings with a single fighter
carrying 9,999 gold. Enter shops, answer the original dialogue, and buy items.
See [New Phlan exploration](docs/PHLAN.md) for controls and current script limits.
From PowerShell:

```powershell
.\demos\build-rolf.cmd
.\demos\review-rolf.cmd
```

See [build prerequisites, controls and current limits](docs/ROLF.md).

## Turn-based combat

The first replaceable C++ rules module uses **SRD 5.2.1** with a native Godot
combat scene and an offline curated rules pack. The standalone modes use fixed
fixtures; the shared party preview uses your created characters. From PowerShell:

```powershell
.\demos\build-rolf.cmd
.\demos\review-combat.cmd
```

Training works without original files. **Slums event** runs the original four-orc
encounter through actual combat and returns its result to ECL. The arena is
authored; original orc icons and dialogue load from your installed game.
See [controls, library boundaries, supported rules, and remaining work](docs/RULES.md).

## Character creation and shared party

Create level-one characters with nine species, twelve classes, four SRD
backgrounds, 4d6-drop-lowest rolls, drag-and-drop ability assignment and score
swapping. Background bonuses update the scores and class eligibility immediately.
Starting-class prerequisite checks are an OpenGoldBox house rule. A scrollable
**Target class(es)** checklist records future goals and shows unmet ability
requirements; acquiring additional classes is not implemented.

Choose from 72 complete portraits with optional gender, class, and in-game race
filters. Portrait controls stay available throughout creation and sheet review;
selected artwork carries into the party and campaign saves. Customize ready/action combat sprites with separate head/body parts
and twelve region colors. The shared character sheet includes inventory, live
stats, a **Modifiers** dialog and a **Saving Throws** calculator.

From PowerShell:

```powershell
.\demos\build-rolf.cmd
.\demos\review-character.cmd
```

See [controls, rules and scope](docs/CHARACTER-CREATION.md).

Finish a character and **Add to party**, or open **Character Pool** to choose
from 48 level-one characters (four per class), each with a unique first name
and surname, selected portraits and matching sprite palettes. The roster supports
six PCs and two NPCs, reserve/rejoin, and an authored preview guard. New PCs
receive 250 gp.

**Explore New Phlan** shares the party with Rolf's tour and town scripts. Inspect
members from the town roster, buy equipment for the selected member, and equip
or unequip supported items with visible training penalties. **Party combat**
opens the tactical Bandit preview. HP, equipment, spent resources and town
position persist between scenes within the running session. Class support is
summarized under [Current status](#current-status); see
[party scope and controls](docs/PARTY.md).

## Recovery and advancement

The party Bandit preview grants a one-time **300 XP per living active member**
on victory. A separate original Slums encounter mapping grants the same authored
award when a campaign party is attached. Characters advance manually as listed
under [Current status](#current-status), updating maximum HP and caster slots
while retaining spent resources. See [advancement](docs/ADVANCEMENT.md).

The original inn can provide an eligible paid long rest, restoring HP and
supported resources. Street camping runs the original city-watch interruption
and grants no recovery. The temple offers a bounded **Cure Wounds** service for
100 gp. Rest eligibility, payment and unsupported-event rollback are enforced.
See [recovery, service limits and verification](docs/RECOVERY.md).

**Save game** and **Load game** support named campaign slots on the roster and idle exploration screens, with overwrite/load confirmation and previous-version recovery. See [save boundaries and restart verification](docs/SAVES.md). The game also travels to the Slums and starts combat from exploration; see [the expedition](docs/EXPEDITION.md). The standalone combat Training mode has its own combat save/load.

## Native monster/NPC statistics

`opengold::por::CreatureCatalog` loads all original Pool of Radiance monster/NPC
records, their equipment, and special effects into reusable C++ values. It
provides stored HP, base AC/THAC0, both attack slots, saves, abilities, modifier
contributions and effect descriptions with unknown bytes retained.

`opengold::por::CreatureFactory` reuses that catalog to create independent monster
and NPC instances with mutable HP and consumable attack/movement turn budgets.
Instances share immutable stats and can outlive their factory. See the
[runtime instance API and example](docs/creature-catalog.md#creating-runtime-monsters-and-npcs)
for the boundary between action tracking and combat resolution.

Build and inspect a creature from the repository root:

```powershell
.\build.cmd
.\build\opengold_creatures.exe "D:\path\to\POOLRAD" "TROLL"
```

Omit `"TROLL"` to inspect all records. See [the C++ API and interpretation
limits](docs/creature-catalog.md) for integration, required assets and tests.

## ECL script tools

The [ECL runtime and engine design](docs/SCRIPTS.md) documents the native
script interpreter and resumable engine requests. The bounded New Phlan host
already runs location events, building transitions, dialogue and shops; the
Slums combat adapter returns encounter outcomes to ECL. General campaign
integration remains incomplete. After building,
run `.\build\opengold_scripts.exe --demo` for a self-contained arithmetic, text,
and menu script. The tool also lists, inspects, and runs installed ECL records;
missing gameplay host capabilities and unbound engine variables produce explicit faults.

## Map inspector

Run `.\build.cmd`, then `.\demos\review-maps.cmd` from PowerShell to browse the original
GEO maps as a top-down grid with walls, doors, and numbered event markers. Click cells or event
locations to inspect their raw data. The demo uses the configured game directory
and the reusable native `MapCatalog` loader. Markers expose potential script
triggers; this inspector does not execute ECL or provide collision-checked player
movement. Those bounded gameplay capabilities are available in the New Phlan demo.
See [map loading, demo controls, and tests](docs/MAPS.md).

## Monster art review tool

Run `.\demos\review-art.cmd` from PowerShell at the repository root, or:

```powershell
godot --path demos/godot res://scenes/monster_art_review.tscn
```

The review tool is organized around unique art groups rather than monster records.
It groups identical decoded images (including dimensions, category and ordered
poses), retains all archive references, and shows combat pose pairs and encounter
distance variants together. The inspected installation has 214 unique groups
from 302 source groups.

- Filter combat icons, encounter sprites, character components, or combat
  effects/miscellaneous; use Left / Right to browse groups.
- Search all monster records, then Confirm selected or mark the association
  Incorrect. Confirm saves and advances to the next unresolved group in the
  current category, skipping confirmed and Unknown groups and wrapping at the end.
  Use Previous to return and confirm additional monsters for the same group.
- Customizable player character **body** and **head** are separate searchable
  assignment targets, suggested first for CBODY and CHEAD art respectively.
  Confirm these components without assigning them to a named monster. Saved
  associations use stable `PLAYER:BODY` and `PLAYER:HEAD` identifiers.
- Arrow, Hatchet, and Flask projectile are searchable assignment targets. Select
  one and use the assignment-name field to rename it. Flask is available for
  manual assignment; no unverified source record is automatically labeled flask.
- To rename a category filter, choose it and click Rename category. Display
  names are saved in `user://art-category-names.json`; stable category IDs and
  existing review decisions are retained.
- Click Add category and enter a unique name to create a category. Choose it
  directly in the searchable assignment list and Confirm selected, or choose it
  in the destination dropdown and click Move current group to category to
  categorize the displayed art. Categories and group membership persist across
  restarts, preserving existing assignments and review decisions.
- Suggested, confirmed, and rejected links remain distinct. Incorrect links
  leave identity unresolved unless another confirmed assignment exists.
- Unconfirmed art preselects the closest available name to a suggested identity
  in the assignment list. Rejected entries are excluded; selection alone does
  not confirm an association.
- The Script evidence tab shows ECL instructions, dialogue on possible encounter
  paths, NPC recruitment references, and bounded character/icon table lookups.
  Script-based candidates take priority over same-ID guesses. Rolf, Scribe,
  Tavern brawler, Priest of Bane / acolytes, City watch, and Trader / wagon seller
  become searchable when their dialogue is found. One sprite can have several
  identities. Archive banks are inferred; these are not gameplay confirmations.
- Next unreviewed skips confirmed and explicitly Unknown groups. Unknown / skip
  clears active confirmations for that group without erasing rejections.
- Edit a selected monster's name separately; saving a name never confirms art.
- Images use nearest-neighbor integer enlargement to a minimum 48 x 48, with
  no smoothing. Source pixels are unchanged.

Names remain in `user://monster-art-names.json`. Existing per-image Incorrect
marks are read from `user://monster-art-decisions.json` and conservatively applied
to the corresponding grouped association; those files are not rewritten by
migration. Explicit group review can supersede a legacy rejection.
New decisions are saved in `user://art-group-review.json`. A complete regenerated
inventory, source references, names, and effective links/statuses is saved in
`user://art-group-inventory.json`. Save files use temporary-file replacement.
The previous monster-art-associations.json is historical and is not updated by
this new view. Original DAX files are unchanged.

Script evidence is rebuilt from the selected game installation on startup and
saved separately in `user://art-script-evidence.json`. For an offline research
report, run `godot_console --headless --path demos/godot --script ../../tools/research_ecl_art.gd`.
This writes the decoded ECL index, evidence JSON, and a report for the saved
unresolved groups under ignored `user-data/`. See [NPC art findings](docs/npc-art-identification.md)
for the discoveries and remaining validation. Test with
`godot_console --headless --path demos/godot --script ../../tests/ecl_art_tests.gd`.

This review tool covers CPIC, SPRIT, CHEAD, CBODY and COMSPR. It does not
categorize portraits, scene illustrations, walls/terrain or non-image assets.
Other native demos already decode supported portraits, pictures, Phlan wall
art and PC-speaker sounds. Automatic name suggestions use script evidence where
available and fall back to provisional
matching-ID guesses; search allows manual assignment beyond those suggestions.
Save names before closing.

## Build setup

OpenGoldBox uses CMake to build a portable C++20 native core. The game's Godot
project is `src/OpenGoldBox/godot/`; the reference demos' project is `demos/godot/`.
This section covers the native core and the demos; the game's build is under
[Play the game](#play-the-game).

Install the prerequisites listed in [docs/INSTALL.md](docs/INSTALL.md), then
configure and build from the repository root:

```powershell
cmake --preset default
cmake --build --preset default
ctest --test-dir build --output-on-failure
```

From a regular VS Code terminal, run the repository-local helper instead. It
initializes Visual Studio and uses its installed CMake directly:

```powershell
.\build.cmd
```

Open `demos/godot/project.godot` in Godot 4.x for the presentation shell. The native
core is deliberately testable without launching Godot. The optional C++
GDExtension contains the tour/town, character/party, combat and sound-board
scenes and is built separately with `.\demos\build-rolf.cmd`. Both build helpers run
the native tests; close running native demo scenes before rebuilding the DLL.
The helpers currently expect Visual Studio Build Tools under the hard-coded
`Microsoft Visual Studio\18\BuildTools` path. For other installations, use the
CMake commands in a configured developer terminal and the
[extension build options](docs/ROLF.md#build-boundary).

Set the game directory before launching a demo or running tests against original
data. The environment variable overrides `opengold/game_directory` in
`demos/godot/project.godot`; point it at the directory containing the DAX files:

```powershell
$env:OPENGOLD_GAME_DIR = 'C:\Games\POOLRAD'
```

The shared party acceptance check exercises creation, the original shop, combat,
level-two advancement, the temple, the inn and interrupted camping:

```powershell
godot --headless --path demos/godot res://scenes/character_creation.tscn -- --party-check
```

It requires the built extension and original game data. Individual feature docs
above describe their additional checks and supported data profiles.

## Graphics comparison

The default Godot scene compares nearest-neighbor, xBR level-2, and Maxim Stepin's HQx
(HQ4x) side by side at 4x. The dropdown defaults to **Combat sprites** (`CPIC*.DAX`,
`COMSPR.DAX`, `CHEAD.DAX`, and `CBODY.DAX`); **Encounter sprites** browses
`SPRIT*.DAX`. Combat starts at `CPIC1.DAX` record 2 when available.
Press Left / Right to browse every stored image across the selected category's
archives, wrapping at either end. Each category remembers its browsing position.
Archives and records are sorted numerically; the status line identifies the
archive, record, image, and overall position. All three panels stay synchronized.
Combat head/body components are shown individually, and combat poses stored in
separate records are browsable individually rather than automatically animated.
All views use the same background-composited image. The xBR shader retains
Hyllian's MIT license notice; the HQ4x shader and lookup table retain their
LGPL-2.1-or-later license and credits in [the HQx folder](demos/godot/shaders/hqx/README.md).
