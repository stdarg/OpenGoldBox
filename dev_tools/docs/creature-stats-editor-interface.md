# Creature Stats Editor: interface with the game

How the [Creature Stats Editor](creature-stats-editor-requirements.md) gets its
edits into the game. For using the importer and its build targets, see
[Creature edits: from the editor to the game](creature-edits-import.md). Written 2026-11-03 for Codex, who builds the tool; Claude
builds the game's side. Everything here is built except the parts marked
**game work**.

## Principles

- **The tool writes its edits in `dev_tools/`; the importer puts them in the
  game.** The tool writes one file of changes,
  `dev_tools/creature_stats_editor/edits/edits.json`
  ([the edits file](#the-edits-file)). `opengold_import_edits`, built from the
  game's own code, applies them to `data/`, checks the result with the game's
  loaders, and writes nothing if any edit is refused. The game never reads
  `dev_tools/`.
- **Every value has one home** in `data/`, which the game and the importer
  use. There are no override copies of stat lines, gear or companions.
- **The tool never computes game results itself.** What a creature's AC is,
  how many guards a party meets, what a companion's Hit Points come to: the
  game's code answers ([Reading results](#7-reading-results-opengold_creature_data-game-work)).
- **Original data stays in the player's install.** Files in the repository hold
  OpenGoldBox's own values and references to original records, never copies of
  original records.

## The pieces

| What the user edits | The edit (`change`) | Where the importer writes it | Format |
| --- | --- | --- | --- |
| Hit Points | `hit_points` | `data/rules/srd-5.2.1/combat.rules` | the rules text format |
| Spellcasting profile | `spell_profile` | `combat.rules` | |
| A new stat line, copied from one | `new_stat_line` | `combat.rules` | |
| Which stat line a record uses, the XP that sizes a fight and the XP it awards | `conversion` | `data/encounters/conversions.json` | `opengold-conversions-v1` |
| A fight's creatures, count and icon | `override` | `data/encounters/overrides.json` | `opengold-encounter-overrides-v1` |
| The chance of a wandering encounter, and a rest's interruption | `chance` | `data/encounters/chances.json` | `opengold-encounter-chances-v1` |
| Companions | `companion` | `data/companions/companions.json` | `opengold-companions-v1` |
| Monster gear | (the catalog's own export) | `data/monsters/` | `opengold-monster-v1` |

The data formats are described beside the files, in
[data/encounters/README.md](../../data/encounters/README.md) and
[data/companions/README.md](../../data/companions/README.md). The tool reads
them, and `combat.rules`, directly to show the current values.

## 1. Stat lines: `combat.rules`

The importer edits the file in place. A creature is its `creature` row and
every row whose second field names it (`saves`, `parry`, `multiattack` and the
others). Row 2 of the file names the `creature` row's fields:

```
creature key AC HP initiative speed melee_bonus melee_count melee_sides melee_modifier ranged_bonus ranged_count ranged_sides ranged_modifier range long_range second_winds spell_slots casting_bonus level spell_mask
creature castle-knight 18 52 0 30 5 2 6 3 2 2 10 0 100 400 0 0 0 1 0
```

Three kinds of change:

1. **Hit Points** (`hit_points`): field 4 (`HP`) of a `creature` row, 1 to
   10,000.
2. **Spellcasting profile** (`spell_profile`): fields `spell_slots`,
   `casting_bonus` and `level`, set to a profile another `creature` row already
   has, such as the nomad shaman's level-4 Wizard profile. The importer refuses
   a profile no creature has.
3. **A new stat line** (`new_stat_line`): a copy of every row of a creature
   under a new key, inserted after the original's `creature` row and the rows
   that follow it, usually followed by a change of kind 1 or 2. Records then
   point at the new key through a `conversion` edit.

The importer keeps every other line of the file as it was, comments included,
and writes the edit's reason in a comment right above the changed `creature`
row (`# Creature Stats Editor (date): reason`), replacing the one an earlier
import wrote there.

**The rules version.** The rules identity includes the file's content hash, so a
changed file already makes older saves fail to load. By convention the version
number goes up too: when an import changes `combat.rules`, the importer raises
`"0.6.N"` in `src/OpenGold.Rules.Srd5/src/srd5.cpp` and
`` `opengold.srd5` 0.6.N `` in `docs/SAVES.md`, once per import, and refuses
the import if the two are missing or disagree. The new number shows in a game
built afterwards; the hash change applies at once. The files under
`data/encounters/` and `data/companions/` are not part of the rules identity.

## 2. Monster gear: the catalog's records

Gear stays where it is: `equipment` in the catalog record
`dev_tools/combat_sprite_catalog/json/` (such as `mon5cha_dax_85.json`), exported to
`data/monsters/<key>/monster.json` ([data/monsters](../../data/monsters/README.md)).
The editor either opens the catalog's own equipment editor or writes the same
`equipment` list and then runs the catalog's export:

```sh
"$GODOT" --headless --path dev_tools/combat_sprite_catalog/godot -- --export-to-game
```

`opengold_monster_export_check` fails until `data/monsters` matches the
records, so both are committed together. In a fight a monster uses only its
held weapons, potions, wands and scrolls; armor, rings and other worn gear are
loot. `opengold_creature_data creature` reports which item counts.

## 3. Fight ratings and stat lines per record: `conversions.json`

The conversion tables that were in `src/OpenGold.Core/src/rolf_tour.cpp` are
in this file, every value kept (188 records in 22 entries). The maps, banks and
`shared_with` lists stay in code. One entry per area:

```json
{
  "format": "opengold-conversions-v1",
  "areas": [
    {
      "scripts": [3, 4, 5, 7],
      "bank": 5,
      "name": "Valjevo Castle",
      "audit": "docs/audits/valjevo-castle.md",
      "creatures": [
        {"record": 85, "definition": "castle-knight", "fit_xp": 700, "award_xp": 700}
      ]
    }
  ]
}
```

- `scripts` are the area scripts the entry is for; the castle's four scripts
  share one entry, so a castle record has one rating. `bank` is the `MONnCHA`
  archive number.
- `definition` is a `combat.rules` key. `fit_xp` sizes the fight; `award_xp`
  is the experience the fight gives.
- `audit` names the area's audit, whose conversion table requirement 17 updates.

**The game** (`load_encounter_data`, `src/OpenGold.Core/src/encounter_data.cpp`)
refuses, naming the file, the entry and the field: a missing file, an unknown
format, a script in two entries, a record twice in one entry, a value that is
not a whole number in range (records 0-255, XP 0-1,000,000), and an empty
definition. When it loads the original's files it also refuses a script that
is no area of the entry's bank, and a record missing from the player's
`MONnCHA` archive. A record the scripts load that has no entry still stops the
game when it is loaded.

**Tests:** `opengold_encounter_data_tests` reads the shipped file (record 85 in
the castle is `castle-knight`, 700 and 700), checks that `combat.rules` defines
every `definition`, and refuses broken files. The route tests pass unchanged.

## 4. Fight changes: `overrides.json`

A fight is one `LOAD MONSTER` instruction in an original script, named by its
archive, record and address: `ECL5.DAX:7:0xA60D` is the instruction that loads
twelve of record 85, with icon 11, against Tyranthraxus.
`opengold_scripts --inspect GAME_DIR ECL5.DAX 7` lists a script's
instructions. An override replaces what that instruction loads:

```json
{
  "format": "opengold-encounter-overrides-v1",
  "overrides": [
    {
      "site": "ECL5.DAX:7:0xA60D",
      "original": {"record": 85, "count": 12},
      "count": 6,
      "reason": "Short reason, also written to docs/DEVIATIONS.md"
    }
  ]
}
```

- `record`, `count` and `icon` are what the instruction now loads; one left
  out keeps the original's, and at least one is given. A changed record keeps
  the instruction's icon unless `icon` names another; a creature with art in
  `data/monsters` shows its art either way.
- `count` is still sized: the game shrinks it to the party's XP budget and to
  one creature per living member, and then applies the difficulty. So `count`
  is the most the party can meet.
- `original` records what the script loads, as a check.
- Fights whose script reads the record or count from its variables, such as
  Podol Plaza's roaming groups, cannot be overridden in this version. The
  tool lists them as read-only.

**The game** applies the override where `LOAD MONSTER` stages creatures
(`RolfTourSession::handle_town_host`, `phlan_session.cpp`), before the
encounter is sized. The record's conversion, gear and art are the new
record's. A missing file means no overrides. The loader refuses, naming the
site: an unknown format, a malformed site, a site twice, a count outside 1-250,
an override that changes nothing, and one without a reason. When the game
loads the original's files it refuses a site whose script is not an area of
that bank, a site that is not a `LOAD MONSTER` of the `original` record and
count written in the script, and a record that a map of that script does not
convert. So a bad override stops the game as it starts, never in a fight.

**Tests:** `opengold_encounter_data_tests` refuses broken files.
`opengold_encounter_override_tests` (the original files, `OPENGOLD_GAME_DIR`)
overrides the Slums orcs at (13,1), `ECL2.DAX:20:0x9E65`, to goblins and checks
the fight the party meets, and that the game refuses a mismatched original, a
site at another instruction, an unconverted record and the wrong bank.

## 5. Companions: `companions.json`

The companions that were written in `src/OpenGold.Core/src/companions.cpp`
(Skullcrusher, the mad fighter, Dirten, Fatima, the mystic, Genheeris) are in
this file, each built exactly as before:

```json
{
  "format": "opengold-companions-v1",
  "companions": [
    {
      "bank": 5,
      "record": 88,
      "name": "Genheeris",
      "note": "Where the values come from",
      "race": "human",
      "gender": "male",
      "class": "wizard",
      "alignment": "neutral_evil",
      "background": "sage",
      "rolled": [10, 10, 9, 14, 10, 10],
      "background_increase": {"plus_two": "intelligence", "plus_one": "constitution"},
      "level": 6,
      "items": []
    }
  ]
}
```

- `bank` and `record` are the `ADD NPC` record the companion replaces.
- `rolled` are the six ability rolls before the background's increase, in the
  order Strength, Dexterity, Constitution, Intelligence, Wisdom, Charisma.
- The game builds the companion at level 1 and advances it to `level` with
  the rules' default choices. Training choices stay the rules' defaults.
- `items` are added after advancing, in order, each as
  `{"key": "mace", "name": "Mace", "quantity": 1, "original_type": 23}` (Dirten's
  mace). `quantity` is 1 when left out; `original_type` is the original's item
  type, or absent.

**The game** (`load_companions`, `build_companion`,
`src/OpenGold.Core/src/companions.cpp`) reads the file as it starts. The
loader refuses, naming the companion: an unknown format, an empty name, race,
class, background, alignment or gender, a level outside 1-15, a roll outside
3-18 or not six of them, an unknown ability or the same one twice in
`background_increase`, an item without a key or name, and a `bank` and
`record` given twice. The rules check the keys when the companion is built: an
unknown race, class, background or alignment, or a level the class cannot
reach, stops the game when a script adds that NPC. A companion who has already
joined keeps the character in the save; a change applies to the next time
someone joins.

**Tests:** `opengold_companion_data_tests` builds each shipped companion and
compares its level, Hit Points, scores, features, spells and items with what
the code built before the move, builds an edited one at a new level with new
gear, and refuses broken files.

## 6. Wandering encounter chances: `chances.json`

The original has no single wandering-encounter percentage. Each area's script
sets its own chances with numbers written in its instructions, and that is what
an edit changes:

- **On a step.** The script rolls a `RANDOM n` (0 to n) into a variable and
  meets a group when a `COMPARE` of that variable with a number succeeds, often
  with additions in Search mode. The Slums roll `RANDOM 13` (ECL2:20 0x9B3A)
  and meet a group above 12 (0x9B60): 1 in 14 a step.
- **On a rest.** The script `SAVE`s the rest check's interval into cell 0x6DD2
  (28114) and its chance in percent into 0x6DD3 (28115); the engine rolls d100
  every interval of 5-minute steps. The Slums save 24 and 24 (0x9A3C, 0x9A42):
  a 24% check every 2 hours.

An entry replaces one such number:

```json
{
  "format": "opengold-encounter-chances-v1",
  "chances": [
    {"site": "ECL2.DAX:20:0x9B60", "operand": 1, "original": 12, "value": 13,
     "reason": "Fewer street groups"}
  ]
}
```

- `site` is the instruction and `operand` the number's place in it (0 or 1);
  `original` is the number the player's script holds there, and `value` the new
  one, 0 to 255.
- Only a `RANDOM`'s range (operand 0), either side of a `COMPARE`, or the value
  of a `SAVE` into 0x6DD2 or 0x6DD3 can be changed; the game refuses anything
  else, and an `original` the script does not hold, when it loads.
- The game reads the new number where the script is decoded. The script's
  bytes, which saves keep, stay the original's, so removing the entry restores
  the original's chance in every save.

`opengold_scripts --inspect GAME_DIR ECLn.DAX SCRIPT` lists a script's
instructions with their addresses and operands (`tag=0` or `2` is a written
number). Each area's rest profile is its `SAVE n {tag=1,value=28114}` and
`{tag=1,value=28115}` lines; some areas have several, for different places
(an inn, a cleared camp). The step rolls traced so far:

| Area | Roll | Meets a group | Chance a step |
| --- | --- | --- | --- |
| The Slums (ECL2:20) | `RANDOM 13` at 0x9B3A | above 12 (0x9B60, operand 1); +4 in Search mode (0x9B47), +5 after the fortune teller's murder (0x9B57) | 1 in 14 |
| Podol Plaza (ECL1:18) | `RANDOM 25` at 0x9AEA | off a mission on 0 (0x9B32), above 18 searching (0x9B44); on one, 0-2 is a rumour (0x9AFB) and a group above 21 (0x9B06), above 16 searching (0x9B18); none after 9 wins (0x9B27) | 1 in 26; 4 in 26 on a mission |
| The central wilderness (ECL7:26) | `RANDOM 19` at 0xA0E7 | on 0 (0xA0F3) | 1 in 20 |
| The western wilderness (ECL6:25) | `RANDOM 19` at 0xA0B3 | on 0 (0xA0BF) | 1 in 20 |
| The eastern wilderness (ECL8:27) | `RANDOM 19` at 0x9EA7 | on 0 | 1 in 20 |

A higher Slums step chance also meets the party more often in
`opengold_godot_area_crossing`, which steps from New Phlan into the Slums and
loads its fixture again, up to 8 times, when monsters meet it; raise its
attempts or change its route with such an edit.

The other areas' step rolls are still to be traced (**game work**): a
`RANDOM`'s variable often feeds more than the encounter, such as Podol Plaza's
rumours, so each needs reading before its numbers are offered as a chance.

**Tests:** `opengold_encounter_data_tests` refuses broken files.
`opengold_encounter_override_tests` (the original files) changes the Slums
roll, its threshold and its rest chance, checks that the script machine rolls
and saves the new numbers while the script's bytes stay the original's, and
that the game refuses a wrong `original`, an `ADD` and a `RANDOM`'s destination.

## The edits file

The tool writes its pending changes to
`dev_tools/creature_stats_editor/edits/edits.json`:

```json
{
  "format": "opengold-creature-edits-v1",
  "edits": [
    {"change": "hit_points", "definition": "castle-knight", "from": 52, "to": 60,
     "reason": "Sturdier guards"},
    {"change": "new_stat_line", "definition": "tower-knight", "copy_of": "castle-knight",
     "reason": "The tower's guards alone"},
    {"change": "spell_profile", "definition": "castle-mage",
     "from": {"spell_slots": 4, "casting_bonus": 5, "level": 4},
     "to": {"spell_slots": 4, "casting_bonus": 5, "level": 4}, "reason": "..."},
    {"change": "conversion", "script": 7, "record": 85,
     "from": {"record": 85, "definition": "castle-knight", "fit_xp": 700, "award_xp": 700},
     "to": {"record": 85, "definition": "tower-knight", "fit_xp": 900, "award_xp": 700},
     "reason": "The guards fight as the tower's knights"},
    {"change": "override", "site": "ECL5.DAX:7:0xA60D", "from": null,
     "to": {"site": "ECL5.DAX:7:0xA60D", "original": {"record": 85, "count": 12},
            "count": 6, "reason": "Half the guards"},
     "reason": "Half the guards"},
    {"change": "chance", "site": "ECL2.DAX:20:0x9B60", "operand": 1, "from": null,
     "to": {"site": "ECL2.DAX:20:0x9B60", "operand": 1, "original": 12, "value": 13,
            "reason": "Fewer street groups"},
     "reason": "Fewer street groups"},
    {"change": "companion", "bank": 5, "record": 88, "from": {"...": "the entry now"},
     "to": {"...": "the entry with level 8"}, "reason": "A stronger turncoat"}
  ]
}
```

- Every edit has a `change` and a one-line `reason` (requirement 17).
- **`from` and `to`.** `from` is the value as the tool read it and `to` the new
  one. For `conversion`, `override`, `chance` and `companion` each is the whole
  entry exactly as its data file holds it, or `null` for none: `from: null` adds
  an entry, `to: null` removes it, so removing an override or a chance restores
  the original's. A `conversion` names the area by any of its `scripts`; the
  castle's four scripts share one entry.
- The edits apply in order, so a `new_stat_line` can be followed by edits of
  the new line.
- **Conflicts.** The importer refuses an edit whose `from` is not what the data
  holds, as when someone changed the value since the tool read it, and an edit
  whose `to` is already there counts as applied. So importing the same file
  twice changes nothing, and a stale edit is never applied on top of a newer
  value.

## Importing

```sh
cmake --build build/macos-universal --target opengoldbox_import_edits
```

runs `opengold_import_edits` and then copies `data/rules`, `data/encounters`,
`data/companions`, `data/monsters` and the new wall art into the game project
(`src/OpenGoldBox/godot/data/`), in a second or so and without building the
game. A game started afterwards plays with the edits; one already running keeps
the files it read when it started. The packaged app (`OpenGoldBox.app`) gets
them with the next full build. `opengoldbox_game_data` does the copy alone, as
after a catalog export of gear.

The importer can also be run alone:

```sh
build/macos-universal/opengold_import_edits [--check] [--repo DIR] [--edits FILE] [--game GAME_DIR]
```

- `--check` reports what an import would do and writes nothing: the tool runs
  it to show the user the result before saving (requirement 19).
- `GAME_DIR` (default `$OPENGOLD_GAME_DIR`) is the player's install. With it the
  importer also loads the game's areas from the original's files, so a fight
  override, a chance or a record the player's scripts do not match is refused
  at import. Without it those checks wait until the game starts.
- It prints a JSON report and exits 0, or prints `{"ok": false, "error": ...}`
  naming the edit and exits 1, having written nothing:

```json
{
  "ok": true,
  "edits": [
    {"change": "hit_points", "subject": "castle-knight", "status": "applied", "summary": "Hit Points 52 to 60"}
  ],
  "files_changed": ["data/rules/srd-5.2.1/combat.rules", "src/OpenGold.Rules.Srd5/src/srd5.cpp", "docs/SAVES.md", "docs/DEVIATIONS.md"],
  "rules_version": "0.6.276"
}
```

`status` is `applied` or `already applied`. Before writing, the importer loads
the edited `combat.rules` with the rules module, the encounter and companion
files with the game's loaders, checks that every conversion's stat line exists,
builds every companion, and with `GAME_DIR` loads the game's areas. It writes
each file beside itself and renames it over the old one.

After a successful import the tool clears its `edits` list. The changed files
are ordinary repository changes: they are committed with the full test suite
passing (CLAUDE.md), and after a `combat.rules` change the AI lab's scenario
packs are rebuilt.

## 7. Reading results: `opengold_creature_data` (game work)

Not built yet; the import does not need it. A command-line program in
`tools/`, built with the game's core and rules libraries and run by the tool
with `OS.execute`. It writes JSON to standard
output, and on error a message to standard error and exit code 1. `GAME_DIR` is
the player's Pool of Radiance install. It reads the repository's `data/` as the
game does.

| Command | Prints |
| --- | --- |
| `check GAME_DIR` | Loads `combat.rules`, `data/monsters`, and the three new files the way the game does. Prints `{"ok": true}` or exits 1 with every problem, one per line. |
| `creatures GAME_DIR` | Every converted record: area, script, bank, record, catalog name, `definition`, `fit_xp`, `award_xp`, the original's Hit Points, and the other records that share the `definition`. |
| `creature GAME_DIR SCRIPT RECORD` | The creature as a fight stages it: AC, Hit Points at each difficulty, initiative, speed, each attack with its to-hit, damage and weapon, saves, abilities, and each gear item with `"counts": true` or `false`. |
| `encounters GAME_DIR` | Every `LOAD MONSTER` site: site name, area, the map square where the party meets it when the script says, the record and count loaded, the override if any, and the text the script shows just before it. Table-rolled fights are listed with `"editable": false`. |
| `size GAME_DIR SITE --levels L1,L2,... [--challenge N]` | For a party with those levels (default encounter challenge 33): at each difficulty, the XP budget, each group's `fit_xp`, and the count met. Uses `fit_encounter_to_difficulty`. |
| `companions GAME_DIR` | Each companion built from the file: the fields above plus the built sheet's level, Hit Points, ability scores, AC, spells and items. |

For the castle's guards, `size GAME_DIR ECL5.DAX:7:0xA60D --levels
10,10,10,10,10,10,10,6` prints 8 met at Normal.

**Tests:** each command's output for a few known records and sites, checked
in the game suite with the original files, as the other original-data tests
are.

## 8. Documentation the importer writes

- **DEVIATIONS.** Each applied edit gets an entry at the end of "Rules and
  combat" in `docs/DEVIATIONS.md`: `- **Subject** (date). Reason. Creature
  Stats Editor: what changed.`, ending with a hidden marker such as
  `<!-- creature-edit hit_points castle-knight -->`. A later edit of the same
  value replaces that entry instead of adding a second one. An edit already
  applied writes nothing.
- **Comments** above changed stat lines in `combat.rules` (section 1).
- **Audits.** The importer does not change the area audits in `docs/audits/`;
  a conversion's `audit` names the one whose conversion table to update.

## Order of work

Built 2026-11-03: `conversions.json`, `overrides.json`, `companions.json`,
their loaders and checks, and `opengoldbox_game_data`. Also built
2026-11-03: `chances.json`, the edits file and `opengold_import_edits`, and
`opengoldbox_import_edits`. With them the tool can edit Hit Points,
spellcasting, stat lines, fight ratings, fight creatures and counts, wandering
and rest chances, and companions, and put the edits in the game. Gear goes
through the catalog's export.

Still to build (**game work**):

1. Tracing the step rolls of the areas not in section 6's table.
2. `opengold_creature_data` (section 7), if the tool is to show what the game
   makes of a creature or how many monsters a party meets.

Each step is pushed with the full test suite passing, and with no change to
the game's behavior until the tool writes an edit.
