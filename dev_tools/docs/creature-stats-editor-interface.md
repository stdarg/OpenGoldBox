# Creature Stats Editor: interface with the game

How the [Creature Stats Editor](creature-stats-editor-requirements.md) gets its
edits into the game, and how it learns what the game makes of them. Written
2026-11-03 for Codex, who builds the tool; Claude builds the game's side. The
data files and their loaders are built (2026-11-03). Parts marked **game work**
do not exist yet.

## Principles

- **The tool writes data files; the game reads them.** The game never reads
  `dev_tools/`. Every value has one home, which both the game and the tool use.
  There are no override copies of stat lines, gear or companions.
- **The tool never computes game results itself.** What a creature's AC is,
  how many guards a party meets, what a companion's Hit Points come to: the
  tool asks `opengold_creature_data`, which runs the game's own code.
- **Original data stays in the player's install.** Files in the repository hold
  OpenGoldBox's own values and references to original records, never copies of
  original records.

## The pieces

| Edit | Written to | Format | Exists |
| --- | --- | --- | --- |
| Hit Points, spellcasting profile, a new stat line | `data/rules/srd-5.2.1/combat.rules` | the rules text format | yes |
| Monster gear | the Combat Sprite Catalog's `json/` record, then its export to `data/monsters/` | `opengold-monster-v1` | yes |
| Which stat line a record uses, the XP that sizes a fight and the XP it awards | `data/encounters/conversions.json` | `opengold-conversions-v1` | yes |
| A fight's creatures and counts | `data/encounters/overrides.json` | `opengold-encounter-overrides-v1` | yes |
| Companions | `data/companions/companions.json` | `opengold-companions-v1` | yes |
| Putting the edits in the game | `opengoldbox_game_data`, a build target ([Publishing](#publishing-to-the-game)) | | yes |
| Reading what the game makes of all of these | `opengold_creature_data`, a command-line program | JSON on standard output | **game work** |
| Map encounter triggers and the chance of a wandering encounter (asked for 2026-11-03) | not designed yet | | **game work** |

The formats are described beside the files, in
[data/encounters/README.md](../../data/encounters/README.md) and
[data/companions/README.md](../../data/companions/README.md). The tool can read
the three JSON files directly; it needs `opengold_creature_data` only for what
the game computes from them.

## 1. Stat lines: `combat.rules`

The tool edits the file in place. A creature is its `creature` row and the rows
after it that name the same key (`saves`, `parry`, `multiattack` and the others).
Row 2 of the file names the `creature` row's fields:

```
creature key AC HP initiative speed melee_bonus melee_count melee_sides melee_modifier ranged_bonus ranged_count ranged_sides ranged_modifier range long_range second_winds spell_slots casting_bonus level spell_mask
creature castle-knight 18 52 0 30 5 2 6 3 2 2 10 0 100 400 0 0 0 1 0
```

The tool may make three kinds of change:

1. **Hit Points:** field 4 (`HP`) of a `creature` row.
2. **Spellcasting profile:** fields `spell_slots`, `casting_bonus` and `level`,
   set to the values of a profile another `creature` row already has, such as
   the nomad shaman's level-4 Wizard profile. It does not write new profiles.
3. **A new stat line:** a copy of a creature's rows under a new key, inserted
   right after the original's rows, followed by a change of kind 1 or 2. Records
   then point at the new key through `conversions.json`.

It must keep every other byte of the file as it was, comments included, and add
or update the comment above a changed creature with its reason in one line.

**The rules version.** The rules identity includes the file's content hash, so a
changed file already makes older saves fail to load. By convention the version
number goes up too. After writing `combat.rules`, the tool raises the last
number of `"0.6.N"` in the line `content.identity = {"opengold.srd5", "0.6.N", ...`
of `src/OpenGold.Rules.Srd5/src/srd5.cpp`, and the same number in
`docs/SAVES.md` (`` `opengold.srd5` 0.6.N, ``). It raises the number once per
session of edits, not once per edit, and stops with an error if either pattern
is missing. The files under `data/encounters/` and `data/companions/` are not
part of the rules identity and need no version change.

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

## Publishing to the game

The tool writes only the repository's `data/` (and `combat.rules`'s version
lines). The game, started from its Godot project, reads a copy of `data/` in
`src/OpenGoldBox/godot/data/`, made by the build. After writing, the tool runs

```sh
cmake --build build/macos-universal --target opengoldbox_game_data
```

which copies `data/rules`, `data/encounters`, `data/companions`,
`data/monsters` and the new wall art into the game project in a second or so,
without building the game. A game started afterwards plays with the edits; one
already running keeps the files it read when it started. The packaged app
(`OpenGoldBox.app`) gets them with the next full build.

An edit is published when, in this order:

1. the tool has written the files and shown their diff (requirement 19);
2. `opengoldbox_game_data` has copied them;
3. these tests pass: `ctest -R "encounter_data|companion_data|encounter_override|slums_creatures"`
   in `build/macos-universal` with `OPENGOLD_GAME_DIR` set to the player's
   install, and, after a `combat.rules` change, the full suite (CLAUDE.md).

A file the game refuses stops it as it starts, with the file, the entry and the
field in the message, so the tool runs the tests in step 3 before the user
starts the game.

## 6. Reading results: `opengold_creature_data` (game work)

A command-line program in `tools/`, built with the game's core and rules
libraries and run by the tool with `OS.execute`. It writes JSON to standard
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

## 7. Documentation the tool writes

- **DEVIATIONS.** Each change that departs from the original gets an entry in
  `docs/DEVIATIONS.md` under "Rules and combat", in the file's style:
  `- **Name** (date). The original ... OpenGoldBox ...`. The tool writes the
  user's reason and the values before and after, and updates an existing entry
  for the same creature or site rather than adding a second one.
- **Audits.** Where an area's audit in `docs/audits/` has a conversion table
  row for the record, the tool updates that row's description.
- The tool shows every file change as a diff and writes nothing until the user
  confirms.

## Order of work

Built 2026-11-03: `conversions.json`, `overrides.json`, `companions.json`, their
loaders and checks, and `opengoldbox_game_data`. With them the tool can edit
Hit Points, gear, fight ratings, fight creatures and counts, and companions,
and put the edits in the game.

Still to build, in this order:

1. `opengold_creature_data` with `check`, `creatures`, `creature` and
   `encounters`, so the tool shows what the game makes of a creature
   (requirements 4 to 7 and 21).
2. `size` (requirement 22).
3. `companions` (requirement 10's preview).
4. Map encounter triggers and wandering chances, which need the original's
   triggers traced per area first.

Each step is pushed with the full test suite passing, and with no change to
the game's behavior until the tool writes an edit.
