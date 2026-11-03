# Creature Stats Editor: interface with the game

How the [Creature Stats Editor](creature-stats-editor-requirements.md) gets its
edits into the game, and how it learns what the game makes of them. Written
2026-11-03 for Codex, who builds the tool. Parts marked **game work** do not
exist yet; they are changes to `src/OpenGold.Core` and `tools/`, each with its
own tests.

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
| Which stat line a record uses, the XP that sizes a fight and the XP it awards | `data/encounters/conversions.json` | `opengold-conversions-v1` | **game work** |
| A fight's creatures and counts | `data/encounters/overrides.json` | `opengold-encounter-overrides-v1` | **game work** |
| Companions | `data/companions/companions.json` | `opengold-companions-v1` | **game work** |
| Reading what the game makes of all of these | `opengold_creature_data`, a command-line program | JSON on standard output | **game work** |

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

## 3. Fight ratings and stat lines per record: `conversions.json` (game work)

The conversion tables in `src/OpenGold.Core/src/rolf_tour.cpp` (the `creatures`
of each `AreaSource`, and `valjevo_castle`) move to this file, every value kept.
The maps, banks and `shared_with` lists stay in code. One entry per script:

```json
{
  "format": "opengold-conversions-v1",
  "areas": [
    {
      "script": 7,
      "bank": 5,
      "name": "Valjevo Castle",
      "creatures": [
        {"record": 85, "definition": "castle-knight", "fit_xp": 700, "award_xp": 700}
      ]
    }
  ]
}
```

- `script` is the area's script number in the game's area table, and `bank`
  the `MONnCHA` archive number. A record's entry applies to that area and to the
  areas that share its creatures, as today.
- `definition` is a `combat.rules` key. `fit_xp` sizes the fight; `award_xp`
  is the experience the fight gives.
- An area's `creatures` replace that area's code table. The castle's shared
  list appears under each castle script that uses it, so the file has no
  references between entries.

**The game:** loads the file when it loads the area resources. It rejects, with
the file, the area and the field named: a missing or unknown format, an unknown
script, a record twice in one area, a record missing from the player's
`MONnCHA` archive, an unknown `definition`, and an XP above 1,000,000. A record
the scripts load that has no entry still fails as now ("Missing original
creature").

**Tests:** every area loads, and staged fights are as before. The route test
(`opengold_expedition_tests`) and the encounter tests pass unchanged, and a check
reads record 85 in Valjevo Castle as `castle-knight`, 700 and 700.

## 4. Fight changes: `overrides.json` (game work)

A fight is one `LOAD MONSTER` instruction in an original script, named by its
archive, record and address: `ECL5.DAX:7:0xA60D` is the instruction that loads
twelve of record 85 against Tyranthraxus. An override replaces what that
instruction loads:

```json
{
  "format": "opengold-encounter-overrides-v1",
  "overrides": [
    {
      "site": "ECL5.DAX:7:0xA60D",
      "original": {"record": 85, "count": 12},
      "record": 85,
      "count": 6,
      "reason": "Short reason, also written to docs/DEVIATIONS.md"
    }
  ]
}
```

- `record` and `count` are what the instruction now loads. Either may be left
  out to keep the original's.
- `count` is still sized: the game shrinks it to the party's XP budget and to
  one creature per living member, and then applies the difficulty. So `count`
  is the most the party can meet.
- `original` records what the script loads, as a check. If the player's script
  at that address loads something else, the game rejects the override and names
  the site.
- Fights a script rolls from tables, such as Podol Plaza's roaming groups, have
  no override in this version. The tool lists them as read-only.

**The game:** applies the override where `LOAD MONSTER` stages creatures, in
`RolfTourSession` (`phlan_session.cpp`), before the encounter is sized. The
record's conversion, art and gear are the new record's. It rejects, with the
site named: an unknown format, a malformed site, a site that is not a
`LOAD MONSTER` instruction, a mismatched `original`, a count of 0 or above 250,
and a record with no conversion in that area. An empty or missing file means no
overrides.

**Tests:** with no file, every fight stages as before. With an override for the
castle's site, the guards' fight stages the override's count, sized as usual.
An override whose `original` does not match is rejected.

## 5. Companions: `companions.json` (game work)

The companions written in `src/OpenGold.Core/src/companions.cpp` (Skullcrusher,
the mad fighter, Dirten, Fatima, the mystic, Genheeris) move to this file, each
built exactly as now:

```json
{
  "format": "opengold-companions-v1",
  "companions": [
    {
      "bank": 5,
      "record": 88,
      "name": "Genheeris",
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
  the rules' default choices, as `advanced()` does now. Training choices stay
  the rules' defaults.
- `items` are added after advancing, in order, each as
  `{"key": "mace", "name": "Mace", "quantity": 1, "original_type": 23}` (Dirten's
  mace). `original_type` is the original's item type, as today, or absent.

**The game:** reads the file when a script adds an NPC. It rejects, with the
companion named: an unknown race, class, background, alignment or item, a
level outside 1-15, a score roll outside 3-18, and a `bank` and `record` given
twice. A companion who has already joined keeps the character in the save; a
change applies to the next time someone joins.

**Tests:** each companion built from the file has the same sheet, Hit Points
and items as the code builds now, checked before the code is removed.

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

1. `opengold_creature_data` with `check`, `creatures`, `creature` and
   `encounters`, reading today's files. With it the tool can already show
   values and edit Hit Points and gear.
2. `conversions.json`, then `size`. Fight ratings become editable.
3. `overrides.json`. Fight creatures and counts become editable.
4. `companions.json`, then `companions`. Companions become editable.

Each step is pushed with the full test suite passing, and with no change to
the game's behavior until the tool writes an edit.
