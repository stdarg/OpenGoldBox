# OpenGoldBox

This project was built using Anthropic's Claude and OpenAI's Codex as a means to learn how to work with LLMs.

An open-source role-playing game engine for Gold Box games.

The project was previously named OpenGold. The display name, documentation
and the GitHub repository ([stdarg/OpenGoldBox](https://github.com/stdarg/OpenGoldBox))
now use OpenGoldBox. Existing `opengold` C++ namespaces, build targets,
`OPENGOLD_GAME_DIR`, source-directory names and save-format identifiers keep
their original names. Settings and campaign saves use the Godot user-data
folder `app_userdata/OpenGold`.

A modern Godot-based reimplementation of SSI's Gold Box engine that reads the
original game data and assets while adding a cleaner UI, improved rendering,
and modern quality-of-life features.

The current implementation targets **Pool of Radiance**, and the whole campaign
is playable: every area of the map, through Tyranthraxus's fall and the old town
after it. The engine uses C++20 with Godot 4.x presentation and a shared
GDExtension. Combat uses a bounded, replaceable **SRD 5.2.1** rules module.
Original game files must come from your own installation and are decoded at
runtime; they are not bundled.

## Current status

As of 2026-11-05. Each linked document holds the detail and its own evidence;
this section only summarizes.

### Play the game

The game starts at
character creation and needs your Pool of Radiance installation (the game asks
for the language and the folder on first run, or set `OPENGOLD_GAME_DIR`).
Settings and saves go to your user-data folder; see
[configuration](docs/CONFIGURATION.md).

- Windows (PowerShell): `.\build-opengoldbox.cmd`, then `.\win-package\opengoldbox.exe`.
  The build also makes the package for players,
  `build\game\OpenGoldBox-<version>-windows-x86_64.zip`, which runs from any folder,
  including a read-only one ([the package](docs/BUILD.md#the-package-for-players)).
- macOS (bash): `./build.sh`, then `open mac-package/OpenGoldBox.app`. `./build.sh`
  builds the game and re-exports the app without running tests.

The last full test run was on 2026-11-06 (commit b4d0aed5, macOS, with original
files): all 269 checks of the game build and all 130 of the demo build passed,
including the packaged game's smoke check. The last full Windows run was on
2026-10-25 (commit 1e46d8be), when all 239 checks passed.

See [building the game and each demo](docs/BUILD.md). The scenes under
[`demos/`](demos/README.md) are frozen references, described under
[Reference demos](#reference-demos) below; they are not the game.

### What you can do

- **Party:** create level-one characters (nine species with their lineage and
  ancestry choices, twelve classes, four backgrounds) with 972 portraits to
  choose from, or pick from a 48-character pool; up to six PCs and two NPCs.
  Every new character brings 3d6 × 10 gp, and the party shares one purse; any
  coin pays a price, with change. What a character can carry is limited by
  weight and Strength. Better weapons, armor and shields go on when bought or
  taken, unless Options turns that off. See
  [character creation](docs/CHARACTER-CREATION.md) and [party](docs/PARTY.md).
- **New Phlan:** Rolf's tour, then the civilized town with its original
  dialogue, shops, inn rest, temple healing, street camping and four training
  halls. Every armor the shops sell converts to SRD equipment. Shops draw in
  dark red what the chosen character cannot fully use (it can still be
  bought), in yellow what the party cannot afford, and in gray armor that
  would not raise the character's Armor Class.
  Any shop buys any item but a cursed
  one for half its price, and any item but a cursed one can be dropped, gone
  for good. See [New Phlan](docs/PHLAN.md),
  [recovery](docs/RECOVERY.md) and [training halls](docs/TRAINING-HALL.md).
- **The campaign:** every area the original's map leads to, from the Slums,
  Kuto's Well and Podol Plaza to Sokal Keep by boat, the Temple of Bane, the
  nomad camp, Kovel Mansion, the Pyramid, the kobold caves, the lizard men's
  keep, Valhingen Graveyard, the wilderness's sites, Stojanow Gate and Valjevo
  Castle, travelled on the overland map. One party plays it all from area to
  area in a single test run ([the whole map](docs/audits/whole-map.md)).
  Monsters are SRD stat blocks, and the original's groups are trimmed to an XP
  budget set by `encounter_challenge` and the difficulty level. Fights use the
  original battlefield geometry. Monsters give their SRD XP, and scripted
  rewards keep the original amounts. The original's loot is left in a pile
  after a fight, and the selected character takes items from it as from a
  shop. Locked doors can be bashed, picked or opened with Knock, Search mode
  finds what the original hides, and camping can be interrupted. See
  [the expedition](docs/EXPEDITION.md) and [outdoor areas](docs/OUTDOOR-AREAS.md).
- **Combat:** the original's placement, monster morale, parley through a party
  leader, Quick combat, and fleeing off the battlefield's edge (or with the
  **Flee** button). Trolls get back up unless fire or acid finishes them;
  Torches, Acid, Alchemist's Fire and Oil can be used in a fight. Five
  difficulty levels; on Hard and above monsters use their potions, wands and
  scrolls. See [rules](docs/RULES.md) and [configuration](docs/CONFIGURATION.md).
- **Quests:** Ohlo's potion delivery and City Hall's commissions, from
  acceptance to reward. See [quests](docs/QUESTS.md).
- **Advancement:** every class levels up to 15 from the roster's Advance
  button, with its SRD subclass. A character may take a level in another class
  as SRD 5.2.1 multiclassing allows, from the level-up dialog's Class row.
  Training halls sell experience sessions and skill lessons. See
  [advancement](docs/ADVANCEMENT.md#multiclassing),
  [levels 5–10](docs/LEVELS-5-10.md) and [training halls](docs/TRAINING-HALL.md).
- **Magic items:** enchanted weapons, armor and shields keep their bonus;
  potions, wands, rings, bracers, necklaces and scrolls work; items are
  identified at a shop or with Identify. No item needs attunement. The Flame
  Tongue lights for 2d6 Fire; the cursed Broad Sword binds its wielder until
  Remove Curse. See [magic items](docs/MAGIC_ITEMS.md#in-opengoldbox).
- **Saves:** named campaign slots from the party roster and idle exploration;
  not during combat or an unfinished event. Saves in an older format are
  refused. See [saves](docs/SAVES.md).
- Fog of war on the overhead map, timed status effects and saving throws, and
  English or Spanish text.

| Class | Subclass | Detail |
| --- | --- | --- |
| Barbarian | Path of the Berserker | [Barbarian](docs/BARBARIAN.md) |
| Bard | College of Lore | [Bard](docs/BARD.md) |
| Cleric | Life Domain | [Cleric](docs/CLERIC.md) |
| Druid | Circle of the Land | [Druid](docs/DRUID.md) |
| Fighter | Champion | [Fighter](docs/FIGHTER.md) |
| Monk | Warrior of the Open Hand | [Monk](docs/MONK.md) |
| Paladin | Oath of Devotion | [Paladin](docs/PALADIN.md) |
| Ranger | Hunter | [Ranger](docs/RANGER.md) |
| Rogue | Thief | [Rogue](docs/ROGUE.md) |
| Sorcerer | Draconic Sorcery | [Sorcerer](docs/SORCERER.md) |
| Warlock | Fiend Patron | [Warlock](docs/WARLOCK.md) |
| Wizard | Evoker | [Wizard](docs/WIZARD.md) |

All twelve classes reach level 15, the game's cap. Spells and features a CRPG
cannot resolve are cut, as [SRD-DECISIONS](docs/SRD-DECISIONS.md) records.
Per-feature status is in the
[SRD coverage ledger](docs/SRD-COVERAGE.md), and every deliberate departure from
the original is in [DEVIATIONS](docs/DEVIATIONS.md).

### Limits

- Two engine commands are not implemented (CALL 0x8000 and opcode 0x1F,
  which the original leaves undefined); an event that reaches one stops with
  an error and is undone. The whole-map run reaches neither. See
  [ECL coverage](docs/ECL-COVERAGE.md#gaps).
- No saving during combat, no importing original saves.

### Tracked work

Every tracked issue is closed, among them the campaign's script coverage
([#4](https://github.com/stdarg/OpenGoldBox/issues/4)), advanced combat
interactions ([#5](https://github.com/stdarg/OpenGoldBox/issues/5)), the Windows
package on a clean machine ([#7](https://github.com/stdarg/OpenGoldBox/issues/7)),
reward mappings ([#9](https://github.com/stdarg/OpenGoldBox/issues/9)), the SRD
plan ([#186](https://github.com/stdarg/OpenGoldBox/issues/186)), magic items
([#234](https://github.com/stdarg/OpenGoldBox/issues/234)) and the Combat Sprite
Catalog's export into the game
([#274](https://github.com/stdarg/OpenGoldBox/issues/274)).

## Reference demos

The scenes below and their launchers are preserved under [`demos/`](demos/README.md)
for reference. The sound board and the art, map and graphics tools run in the
demo project. The Rolf, combat and character launchers open the game's own
scenes, so build the game first. The launchers are Windows CMD scripts.

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

Run the original welcome through Rolf's farewell on the game's town screen,
with his encounter sprite, the original Phlan wall and door artwork, and a
synchronized party map. Launched on its own, without a party, the tour gives a
single fighter carrying 9,999 gold to explore New Phlan with: enter shops,
answer the original dialogue, and buy items.
See [New Phlan exploration](docs/PHLAN.md) for controls and current script limits,
and [keyboard shortcuts](docs/KEYBOARD.md) for every key.
From PowerShell:

```powershell
.\build-opengoldbox.cmd
.\demos\review-rolf.cmd
```

See [build prerequisites, controls and current limits](docs/ROLF.md).

## Turn-based combat

The first replaceable C++ rules module uses **SRD 5.2.1** with a native Godot
combat scene and an offline curated rules pack. The standalone modes use fixed
fixtures; fights in the game use your party. From PowerShell:

```powershell
.\build-opengoldbox.cmd
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
**Desired Class(es)** checklist records future goals and shows unmet ability
requirements; acquiring additional classes is not implemented.

Choose from 972 complete portraits with optional gender, class, and in-game race
filters. Portrait controls stay available throughout creation and sheet review;
selected artwork carries into the party and campaign saves. Customize ready/action combat sprites with separate head/body parts
and twelve region colors. The shared character sheet includes inventory, live
stats, a **Modifiers** dialog and a **Saving Throws** calculator.

From PowerShell:

```powershell
.\build-opengoldbox.cmd
.\demos\review-character.cmd
```

See [controls, rules and scope](docs/CHARACTER-CREATION.md).

Finish a character and **Add to party**, or open **Character Pool** to choose
from 48 level-one characters (four per class), each with a unique first name
and surname, selected portraits and matching sprite palettes. The roster holds
six PCs and two NPCs; members can go to reserve and rejoin. New PCs bring
3d6 × 10 gp to the party's purse.

**Explore New Phlan** shares the party with Rolf's tour and town scripts. Inspect
members from the town roster, buy equipment for the selected member, and equip
or unequip supported items with visible training penalties. HP, equipment,
spent resources and town position persist between scenes. Class support is
summarized under [Current status](#current-status); see
[party scope and controls](docs/PARTY.md).

## Recovery and advancement

A won fight gives each living party member a share of the defeated creatures'
experience. Characters advance manually as listed under
[Current status](#current-status), updating maximum HP and caster slots while
retaining spent resources. Training halls also sell experience. See
[advancement](docs/ADVANCEMENT.md).

The original inn can provide an eligible paid long rest, restoring HP and
supported resources. Street camping runs the original city-watch interruption
and grants no recovery. The temples of Sune, Tempus and Tyr in New Phlan, and
Ilmater's in Podol Plaza, heal, cure and raise the dead for a price: from
100 gp for Cure Light Wounds to 11,000 gp for Resurrection. Rest eligibility,
payment and unsupported-event rollback are enforced. See
[recovery, service limits and verification](docs/RECOVERY.md).

**Save game** and **Load game** support named campaign slots on the roster and idle exploration screens, with overwrite/load confirmation and previous-version recovery. See [save boundaries and restart verification](docs/SAVES.md). The whole campaign plays from exploration; see [the expedition](docs/EXPEDITION.md). The standalone combat Training mode has its own combat save/load.

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
script interpreter and resumable engine requests. The game's host runs every
area's location events, building transitions, dialogue, shops and fights;
[ECL coverage](docs/ECL-COVERAGE.md) tracks each command. After building,
run `.\build\opengold_scripts.exe --demo` for a self-contained arithmetic, text,
and menu script. The tool also lists, inspects, and runs installed ECL records;
missing gameplay host capabilities and unbound engine variables produce explicit faults.

## Map inspector

Run `.\build.cmd`, then `.\demos\review-maps.cmd` from PowerShell to browse the original
GEO maps as a top-down grid with walls, doors, and numbered event markers. Click cells or event
locations to inspect their raw data. The demo uses the configured game directory
and the reusable native `MapCatalog` loader. Markers expose potential script
triggers; this inspector does not execute ECL or provide collision-checked player
movement, which the game does.
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

- Filter combat icons, encounter sprites, character components, projectiles,
  or combat effects/miscellaneous, or show every category; use Left / Right to
  browse groups.
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

## Developer tools

The tools under [`dev_tools/`](dev_tools) read your installed DAX files and are
not part of the game; the game never reads them. Most are Godot tools: their
`run.sh` (bash; Git Bash on Windows) builds the tool's native extension and
imports its project on first run. The DOS probe and the AI lab are
command-line programs. The demos' build also builds the tile art viewer's and
the combat sprite catalog's extensions, and every build includes the AI lab.
Each tool's README has its build and self-check commands.

- [Combat sprite catalog](dev_tools/combat_sprite_catalog/README.md): every
  monster and NPC record with its combat art, weapon sets and record editing.
  Its Close-ups tab plays the 3D view's 74 animated close-ups beside a written
  description of each, and plays new art drawn only from that description.
  Its export is the game's `data/monsters` and `data/art/closeups`.
- [Creature Stats Editor](dev_tools/docs/creature-stats-editor.md): changes a
  monster's Hit Points, stats, spellcasting and fight ratings, the scripts'
  wandering and rest chances, and the companions who join the party, and
  publishes the changes into the game's data.
- [Combat tester](dev_tools/combat_tester/README.md): fights one of six Slums
  monster types on any of the game's 29 combat maps with a preset party, on the
  game's own combat screen.
- [Party tester](dev_tools/party_tester/README.md): a party of up to eight from
  the Character Pool, levelled to a chosen level with the game's level-up
  dialog, fights any monsters and NPCs the areas' scripts load on an area's
  battlefield, on the game's combat screen. Its level sweep takes every class,
  and every pair of classes, from level 1 to 15 through the dialog, checks each
  sheet against SRD 5.2.1's tables and fights at levels 1, 5, 10 and 15.
- [AI lab](dev_tools/ai_lab/README.md): replays real campaign fights through
  the game's combat and computer play, to measure a change to how the computer
  plays.
- [Tile art viewer and map browser](dev_tools/tile_art_viewer/README.md): the
  original first-person wall art and exploration maps.
- [Replacement art creator](dev_tools/replacement_art_creator/README.md):
  briefs, imports and previews for new wall art; see
  [the workflow](docs/REPLACEMENT-ART-CREATOR.md).
- [Shop & Temple Manager](dev_tools/shop_manager/README.md): the shops' and
  temples' offerings, prices, quantities and portraits.
- [DOS probe](dev_tools/dos_probe/README.md): a research-only headless PC that
  runs your copy of the original game and records screenshots and script
  traces to compare with OpenGoldBox.

`opengold_magic_items GAME_DIR` lists every magic item the original can give the
party; see [magic items](docs/MAGIC_ITEMS.md).

## Build setup

OpenGoldBox uses CMake to build a portable C++20 native core. The game's Godot
project is `src/OpenGoldBox/godot/`; the reference demos' project is `demos/godot/`.
This section covers the native core and the demos; the game's build is under
[Play the game](#play-the-game).

C++ follows [the coding style](docs/CODING_STYLE.md), kept by clang-format and
clang-tidy. Enable the project's hooks (code style, and no Claude co-author
lines in commit messages) once per checkout with
`git config core.hooksPath tools/git-hooks`.

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

Open `src/OpenGoldBox/godot/project.godot` in Godot 4.x for the game, or
`demos/godot/project.godot` for the reference demos. The native
core is deliberately testable without launching Godot. `.\build-opengoldbox.cmd`
builds the game's C++ GDExtension: the tour/town, character/party and combat
scenes. The native core builds and tests without it. The demos' research tools,
such as the sound board, are built separately with `.\demos\build-rolf.cmd`.
Both build helpers run every test in their build tree, the Godot checks
included; close running native demo scenes before rebuilding the DLL.
The helpers currently expect Visual Studio Build Tools under the hard-coded
`Microsoft Visual Studio\18\BuildTools` path. For other installations, use the
CMake commands in a configured developer terminal and the
[extension build options](docs/ROLF.md#build-boundary).

Set the game directory before launching a demo or running tests against original
data; without it, about 80 of the checks report Skipped. The environment
variable overrides the game's saved folder and `opengold/game_directory` in
`demos/godot/project.godot`; point it at the directory containing the DAX files:

```powershell
$env:OPENGOLD_GAME_DIR = 'C:\Games\POOLRAD'
```

The shared party acceptance check exercises creation, the original shop, combat,
level-two advancement, the temple, the inn and interrupted camping:

```powershell
godot --headless --path src/OpenGoldBox/godot res://scenes/character_creation.tscn -- --party-check
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
