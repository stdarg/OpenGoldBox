# Creature Stats Editor

A Godot tool for changing a monster's Hit Points, stat line, spellcasting and
fight ratings, what a fight loads, the scripts' wandering and rest chances, and
the companions who join the party. It shows what the game makes of each
value and publishes changes straight into the game's data.

Decided 2026-11-04 (the user): Claude builds the editor, and it runs the game's
own code instead of a copy. The [requirements](creature-stats-editor-requirements.md)
list what it is for; the [interface](creature-stats-editor-interface.md) has
each data file's rules.

## Running it

From the repository root:

```sh
OPENGOLD_GAME_DIR="$HOME/POOLRAD" dev_tools/creature_stats_editor/run.sh
```

`OPENGOLD_GAME_DIR` is the original game's files. Set `GODOT_BIN` if Godot 4.5
or newer is not at `/Applications/Godot_mono.app/Contents/MacOS/Godot`.

The script builds the editor's extension in `build/creature-stats-editor`
from the game's own libraries, stages the editor's Godot project in
`build/creature-stats-editor/project` and opens it. The first run builds
godot-cpp and takes a few minutes; later runs take seconds.

## How it works

```
Creature Stats Editor (Godot, dev_tools/creature_stats_editor/godot)
  │  CreatureEditor, the editor's extension (native/creature_editor.cpp)
  ▼
tools/creature_data.cpp      reads data/ and the original's files with the game's
                             loaders; computes values with the game's rules,
                             staging and encounter sizing
tools/creature_edits.cpp     the importer: checks the edits with the game's code
                             and writes data/ and the bookkeeping, all or nothing
  │  Publish
  ▼
data/, srd5.cpp, SAVES.md, DEVIATIONS.md
  │  opengoldbox_game_data, run by Publish
  ▼
src/OpenGoldBox/godot/data/  what a game started afterwards reads
```

- **One code path.** The editor computes nothing itself. A monster's AC,
  attacks and Hit Points come from the rules module
  (`RulesModule::creature_stats`) for the creature `staged_fighter` builds, with
  the difficulty `apply_difficulty` applies, exactly as a fight fields it. A
  fight's size comes from `staged_groups` and `fit_encounter_to_difficulty`, as
  the game sizes it. A companion is built with `build_companion`.
- **The importer is the same one** the command line uses
  (`opengold_import_edits`, [guide](creature-edits-import.md)); the editor calls
  it in process, with the edits it holds.
- **Pending edits** are kept in
  `dev_tools/creature_stats_editor/edits/edits.json` (the edits file's format),
  so they survive closing the editor. Git ignores that folder. Publishing
  clears it.
- **Monster gear** is shown, with what a fight uses each item for, but edited
  only in the [Combat Sprite Catalog](combat-sprite-catalog.md), so there is one
  gear editor.

## The tabs

**Creatures.** Every original record an area converts, searchable by name,
stat line, area or record. A record shows:

- its combat art, the original's Hit Points and AC, and the records that share
  its stat line;
- AC, Hit Points, initiative, speed, melee and ranged attacks at each
  difficulty, met by a party of average level 2 and up (below that the harder
  levels come on gradually);
- saves and spellcasting at Normal;
- its gear: what it fights with, what it uses on Hard and above (potions,
  wands, scrolls), and what is loot only.

It can change the stat line's Hit Points (with the original's value one click
away), the two fight ratings, which stat line the record fights as, a copy of
the stat line for this record alone, and the spellcasting profile, choosing
from the profiles the rules already have.

**Fights.** Every `LOAD MONSTER` in the areas' scripts. A fight is the
`LOAD MONSTER`s that follow one another. For a party's levels and the encounter
challenge, it shows each difficulty's XP budget and the creatures met from
each group. It can override the record, the most met and the icon, or remove
an override. Fights whose script reads the record or count from a variable
are read only.

**Chances.** Every number an area's script sets a chance with that
`chances.json` may change: a `RANDOM`'s range, the number a `COMPARE` tests a
rolled variable against, and the rest check's interval and chance. Filter by
area and kind. A `RANDOM`'s variable can feed more than the encounter, so read
the script before changing one ([traced rolls](creature-stats-editor-interface.md#6-wandering-encounter-chances-chancesjson)).

**Companions.** Each companion built as the game builds them: Hit Points,
scores, saves, features, spells and items. It can change the level and the
items added after advancing.

**Pending.** The edits in order. **Preview** runs the importer without writing
and shows each edit's result, the new rules version, and a diff of every file
it would change. **Publish** asks first, then writes the files, copies `data/`
into the game project with `opengoldbox_game_data`, and reads the data again.

A second change to the same thing replaces the pending edit and keeps its
`from`; a record's conversion or a companion builds on its pending change, so
fight ratings and a new stat line can be one edit. Every edit needs a
one-line reason, which goes to `docs/DEVIATIONS.md`.

**Start the game** opens the game's Godot project
(`src/OpenGoldBox/godot`), which plays with published edits. A game already
running keeps the data it read. The packaged app gets the edits with the next
`./build.sh`.

## After publishing

The changed files are ordinary repository changes. Run the full test suite and
commit them (CLAUDE.md). After a `combat.rules` change, rebuild the AI lab's
scenario packs ([AI lab](ai-lab.md)).

## Checks

- `opengold_creature_data_tests` (in the game's test suite, with
  `OPENGOLD_GAME_DIR`): what the editor shows against the game: the castle's
  guards at each difficulty, eight of twelve met by the reference party, the
  Slums' chances, the companions.
- `opengold_creature_edits_tests`: the importer, including edits held in memory
  and the diffs Preview shows.
- `dev_tools/creature_stats_editor/check.sh`: drives the editor's screen
  headless: the lists, each tab, combining edits, Preview, a stale edit's
  refusal, and Publish into a scratch copy of the repository's files.

## Not built yet

- Replaying a changed fight in the [AI lab](ai-lab.md) before and after
  (requirements 20 and 27).
- A fight's map square and the text that announces it (requirement 21).
- Showing a creature's values with pending edits before they are published;
  Preview shows the files instead.
