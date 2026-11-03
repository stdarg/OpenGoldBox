# Creature edits: from the editor to the game

How changes made in the
[Creature Stats Editor](creature-stats-editor-requirements.md) reach the game,
and how to use the tools that carry them. The
[interface specification](creature-stats-editor-interface.md) records the
design and each file's rules in full; this guide is the working reference.

Who builds what: Codex builds the editor (`dev_tools/creature_stats_editor/`).
The game's side, described here, is built: the data files the game reads, the
importer that writes them, and the build targets that put them in the game.

## How it fits together

```
Creature Stats Editor (Godot, dev_tools/)
  │  writes its pending changes
  ▼
dev_tools/creature_stats_editor/edits/edits.json
  │  opengold_import_edits: checks every edit with the game's own code,
  │  then applies all of them, or none
  ▼
data/rules/srd-5.2.1/combat.rules      stat lines: Hit Points, spellcasting
data/encounters/conversions.json       what each original record fights as
data/encounters/overrides.json         what a fight loads instead
data/encounters/chances.json           wandering and rest chances
data/companions/companions.json        the NPCs who join the party
docs/DEVIATIONS.md, srd5.cpp, SAVES.md the bookkeeping
  │  opengoldbox_game_data: copies data/ into the game project
  ▼
src/OpenGoldBox/godot/data/            what the game reads when it starts
```

- The game never reads `dev_tools/`. Each value has one home in `data/`, which
  the game and the importer both use.
- The editor never writes `data/` itself, and never works out a game result
  itself; the importer and the game's loaders decide what is valid.
- Monster gear is the exception: it is edited in the
  [Combat Sprite Catalog](combat-sprite-catalog.md), whose own export writes
  `data/monsters/`, so there is one gear editor.

## Quick start

From the repository root, with the original game at `$OPENGOLD_GAME_DIR`:

1. Make the changes in the editor. It writes
   `dev_tools/creature_stats_editor/edits/edits.json`.
2. Preview them. Nothing is written:

   ```sh
   build/macos-universal/opengold_import_edits --check
   ```

3. Import them and copy the result into the game:

   ```sh
   cmake --build build/macos-universal --target opengoldbox_import_edits
   ```

4. Start the game from its Godot project (`src/OpenGoldBox/godot`) and play
   the changed fight. A game already running keeps what it read when it
   started.
5. Clear the editor's edits list, run the full test suite, and commit the
   changed files (CLAUDE.md).

## The edits file

`dev_tools/creature_stats_editor/edits/edits.json`, written by the editor:

```json
{
  "format": "opengold-creature-edits-v1",
  "edits": [
    {"change": "hit_points", "definition": "castle-knight", "from": 52, "to": 60,
     "reason": "Sturdier guards"}
  ]
}
```

Every edit has:

- `change`: one of the seven kinds below.
- `reason`: one line saying why. It goes into `docs/DEVIATIONS.md` and, for a
  stat line, into a comment above it. An edit without one is refused.
- `from` and `to` (all kinds but `new_stat_line`): the value as the editor
  read it, and the new value.

The edits apply in order, so a later edit can change what an earlier one made,
such as the Hit Points of a stat line copied just before.

### `from`, `to` and conflicts

The importer compares `from` with what the data holds now:

| The data holds | Result |
| --- | --- |
| `from` | The edit is applied. |
| `to` | Nothing to do: reported as `already applied`. |
| Anything else | The whole import is refused, naming the edit. |

So importing the same file twice changes nothing, and an edit made before
someone else changed the same value is never written over theirs. If the
editor reports a conflict, it reads the files again and lets the user redo the
edit.

For `conversion`, `override`, `chance` and `companion`, `from` and `to` are
each a whole entry, exactly as its data file holds it, or `null` for none:

- `from: null` with an entry in `to` adds the entry;
- an entry in `from` with `to: null` removes it, which for an override or a
  chance restores the original's;
- two entries change one into the other.

The editor takes `from` straight from the file it read, so that the two are
equal member for member. Member order does not matter.

### The seven changes

**`hit_points`**: a stat line's Hit Points.

| Field | Value |
| --- | --- |
| `definition` | the `combat.rules` creature key, such as `castle-knight` |
| `from`, `to` | whole numbers; `to` from 1 to 10,000 |

**`spell_profile`**: a stat line's spellcasting, chosen from the profiles the
rules already have (such as the nomad shaman's level-4 Wizard profile).

| Field | Value |
| --- | --- |
| `definition` | the creature key |
| `from`, `to` | `{"spell_slots": 4, "casting_bonus": 5, "level": 4}` |

A `to` no creature in `combat.rules` has is refused.

**`new_stat_line`**: a copy of a creature's rows under a new key, for a change
that should affect only some records (requirement 9).

| Field | Value |
| --- | --- |
| `definition` | the new key: one word, not already used |
| `copy_of` | the creature to copy |

Every row of `copy_of` (`creature`, `saves`, `parry`, `multiattack` and the
others) is copied under the new key, after the original's rows. A key that
already exists counts as already applied. Records then use it through a
`conversion` edit.

**`conversion`**: what an original creature record fights as in an area: its
stat line, the XP that sizes its fights, and the experience it awards.

| Field | Value |
| --- | --- |
| `script` | any script of the area's entry in `conversions.json`; Valjevo Castle's 3, 4, 5 and 7 share one entry |
| `record` | the `MONnCHA` record |
| `from`, `to` | `{"record": 85, "definition": "castle-knight", "fit_xp": 700, "award_xp": 700}`, or `null` |

**`override`**: what one of the original's `LOAD MONSTER` instructions loads.

| Field | Value |
| --- | --- |
| `site` | the instruction, such as `ECL5.DAX:7:0xA60D` |
| `from`, `to` | an `overrides.json` entry, or `null` |

```json
{"site": "ECL5.DAX:7:0xA60D", "original": {"record": 85, "count": 12},
 "count": 6, "reason": "Half the guards"}
```

`original` is what the player's script loads there. `record`, `count` (1 to
250) and `icon` are what it loads now; one left out stays the original's. The
game still sizes the count to the party, so `count` is the most it can meet.

**`chance`**: one number an original script sets a wandering encounter's or a
rest's chance with ([Wandering and rest chances](#wandering-and-rest-chances)).

| Field | Value |
| --- | --- |
| `site`, `operand` | the instruction and the number's place in it (0 or 1) |
| `from`, `to` | a `chances.json` entry, or `null` |

```json
{"site": "ECL2.DAX:20:0x9B60", "operand": 1, "original": 12, "value": 13,
 "reason": "Fewer street groups"}
```

**`companion`**: an NPC who joins the party.

| Field | Value |
| --- | --- |
| `bank`, `record` | the `MONnCHA` record the script's `ADD NPC` names |
| `from`, `to` | a `companions.json` entry, or `null` |

An entry gives the name, race, gender, class, alignment, background, the six
scores rolled, the background's +2 and +1, the level (1 to 15) and the items
([data/companions](../../data/companions/README.md)). The game builds the
companion at level 1 and levels it up with the rules' default choices.

## The importer: `opengold_import_edits`

Built with the game (`build/macos-universal/`, and `build/` for the native
build):

```sh
opengold_import_edits [--check] [--repo DIR] [--edits FILE] [--game GAME_DIR]
```

| Option | Meaning |
| --- | --- |
| `--check` | Do everything except write: the preview. |
| `--repo DIR` | The repository to change; by default the one it was built from. |
| `--edits FILE` | The edits file; by default `dev_tools/creature_stats_editor/edits/edits.json`. |
| `--game GAME_DIR` | The original game; by default `$OPENGOLD_GAME_DIR`. |

Without an edits file, or with an empty list, it changes nothing.

### What it does

1. Reads the edits and the current files.
2. Applies the edits in order, in memory, refusing any whose `from` does not
   match.
3. For each applied edit, adds an entry to `docs/DEVIATIONS.md`, or replaces
   the one an earlier import wrote for the same value; for a stat line, writes
   the reason in a comment above its `creature` row.
4. If `combat.rules` changed, raises the rules version: `"0.6.N"` in
   `src/OpenGold.Rules.Srd5/src/srd5.cpp` and in `docs/SAVES.md`, once per
   import.
5. Checks the result as the game would load it:
   - the rules module parses the new `combat.rules`;
   - the game's loaders read the new encounter and companion files;
   - every conversion fights as a stat line `combat.rules` defines;
   - every companion builds;
   - with the original game, the game's areas load. That checks each
     override and chance against the player's scripts and each record against
     the player's `MONnCHA` archives.
6. Unless `--check`, writes each changed file beside itself and renames it into
   place.

If any step fails, nothing is written.

### Its report

On success it prints JSON and exits 0:

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

- `status`: `applied` or `already applied`.
- `files_changed`: relative to the repository; with `--check`, the files it
  would change.
- `rules_version`: the new version when `combat.rules` changed, else `null`.

On a refusal it prints `{"ok": false, "error": "..."}`, writes the message to
standard error as well, and exits 1. Bad options print the usage and exit 2.

### What it writes in the docs

A DEVIATIONS entry goes at the end of "Rules and combat":

```markdown
- **castle-knight** (2026-11-03). Sturdier guards. Creature Stats Editor:
  Hit Points 52 to 60. <!-- creature-edit hit_points castle-knight -->
```

The hidden marker lets a later import of the same value replace the entry.
Edit the entry's wording freely, but keep the marker. The importer does not
update the area audits in `docs/audits/`: a conversion's `audit` names the
audit whose conversion table to bring up to date by hand.

Above a changed stat line in `combat.rules`:

```
# Creature Stats Editor (2026-11-03): Sturdier guards
creature castle-knight 18 60 0 30 5 2 6 3 2 2 10 0 100 400 0 0 0 1 0
```

## Getting the edits into the game

| Target (`cmake --build build/macos-universal --target ...`) | Does |
| --- | --- |
| `opengoldbox_import_edits` | Runs the importer, then the copy below. A refused edit fails the build and copies nothing. |
| `opengoldbox_game_data` | Copies `data/rules`, `data/encounters`, `data/companions`, `data/monsters` and the new wall art into the game project, in a second or so, without building the game. Use it after a catalog export of gear. |

- The game reads these files once, when it starts. A game started from the
  Godot project (`src/OpenGoldBox/godot`) after the copy plays with the edits.
- The packaged app (`mac-package/OpenGoldBox.app`) gets them with the next
  full build (`./build.sh`).
- A changed `combat.rules` changes the rules identity: saves made before it no
  longer load (pre-1.0 policy, [SAVES.md](../../docs/SAVES.md)), and AI lab
  scenario packs must be rebuilt ([AI lab](ai-lab.md)). The raised version
  number shows in a game built afterwards; the change itself applies at once.
- Encounter, chance and companion changes do not touch saves. A companion who
  has already joined keeps the character in the save; a change applies the next
  time someone joins.

### What the game checks when it starts

The game checks the same files again as it loads, and stops with a message
naming the file, the entry and the field rather than play with a bad one:

- `conversions.json` must exist; each script must be an area of its entry's
  bank, and each record must be in the player's `MONnCHA` archive.
- An override's site must be a `LOAD MONSTER` of the `original` record and
  count written in the player's script, and every map of that script must
  convert the record it now loads.
- A chance's site must hold its `original` number in a place a chance can be
  changed (below).
- A companion that the rules cannot build stops the game when a script adds
  that NPC.

## Finding what to edit

**Stat lines.** `data/rules/srd-5.2.1/combat.rules`: a creature's `creature`
row and the rows that name it. Row 2 of the file names the `creature` row's
fields; Hit Points are the fourth.

**Which records use a stat line.** `data/encounters/conversions.json`, one
entry per area, each record with its `definition`. A stat line used by several
records changes all of them: `castle-knight` is both Tyranthraxus's guards and
the old town's 8th level fighters.

**Fight sites.** The original's scripts, listed with

```sh
build/opengold_scripts --inspect "$OPENGOLD_GAME_DIR" ECL5.DAX 7
```

Each line is `ADDRESS NAME operands`, an operand `{tag=0,value=85}` (tag 0 or
2) being a number written in the script and `{tag=1,value=...}` a variable.
`0xa60d LOAD MONSTER {tag=0,value=85} {tag=0,value=12} {tag=0,value=11}` loads
12 of record 85 with icon 11, so its site is `ECL5.DAX:7:0xA60D`. A
`LOAD MONSTER` whose record or count is a variable, as in Podol Plaza's
roaming groups, cannot be overridden.

### Wandering and rest chances

The original has no single wandering-encounter percentage; each area's script
writes its own numbers:

- **A step.** `RANDOM n` rolls 0 to n into a variable, and a `COMPARE` of that
  variable with a number decides whether a group appears. The Slums roll
  `RANDOM 13` (ECL2:20 0x9B3A) and meet a group above 12 (0x9B60): 1 in 14.
- **A rest.** The script saves the check's interval, in 5-minute steps, into
  cell 28114 (0x6DD2) and its chance in percent into 28115 (0x6DD3):
  `SAVE {tag=0,value=24} {tag=1,value=28115}` is a 24% chance. Some areas set
  several profiles for different places.

A chance edit can change only a `RANDOM`'s range (operand 0), either number of
a `COMPARE` (operand 0 or 1), or the number a `SAVE` writes into 28114 or
28115 (operand 0), each to 0-255. The game reads the new number as it decodes
the instruction. The script's bytes, which saves keep, stay the original's, so
removing the entry restores the original's chance everywhere.

The step rolls traced so far are listed in the
[interface](creature-stats-editor-interface.md#6-wandering-encounter-chances-chancesjson):
the Slums, Podol Plaza and the three wilderness maps. In other areas a roll's
variable can decide more than the encounter, such as Podol Plaza's rumours, so
read the script before offering its numbers as a chance. A higher Slums step
chance also makes `opengold_godot_area_crossing` meet monsters more often.

## When the importer refuses

| Message | Meaning and fix |
| --- | --- |
| `edit 2 (hit_points): the data holds 60, not the edit's from 52` | The value changed since the editor read it. Reload and redo the edit. |
| `edit 1 (hit_points): every edit gives its reason (requirement 17)` | Add a `reason`. |
| `combat.rules: no creature tower-knight` | The key is wrong, or the `new_stat_line` that makes it comes later in the list. |
| `edit 3 (spell_profile): no creature in combat.rules casts at level 9 (...)` | Choose a profile some creature has. |
| `edit 4 (conversion): the edit's to is for another entry` | `to` names another record, site or companion than the edit's own fields. |
| `The game would refuse the result (data/encounters): ...` | An entry breaks the file's rules; the rest of the message names it. |
| `The game would refuse the result: Valjevo Castle, record 85 fights as nobody, which combat.rules does not define` | A conversion names a stat line that does not exist. |
| `The game would refuse the result (the original's files): data/encounters/overrides.json: ECL5.DAX:7:0xA60D: the player's script loads no 11 of record 85 here` | The `original` does not match the player's script. Read the site again with `opengold_scripts --inspect`. |
| `... chances.json: ECL2.DAX:20:0x9B47: operand 0 is no RANDOM range, COMPARE number or SAVE of the rest check` | That number is not a chance. |
| `The rules version is not where the importer looks for it ...` | `srd5.cpp` and `SAVES.md` must both have the same `0.6.N`. |

## Tests

| Test | Covers |
| --- | --- |
| `opengold_creature_edits_tests` | The importer on a copy of the repository: every change kind, previews, repeated imports, conflicts, refusals that write nothing, DEVIATIONS entries, the rules version, and files that come back byte for byte. |
| `opengold_encounter_data_tests` | `conversions.json`, `overrides.json` and `chances.json`: the shipped values and every refusal. |
| `opengold_companion_data_tests` | `companions.json`: each companion built as before the move to data, and every refusal. |
| `opengold_encounter_override_tests` | With the original game: an override met in a Slums fight, chances read by the script machine, and the game's refusals at load. Skipped without `OPENGOLD_GAME_DIR`. |

After an import, run at least these and `opengold_slums_creatures_tests`
(`ctest -R "creature_edits|encounter_data|companion_data|encounter_override|slums_creatures"`
in `build/macos-universal`, with `OPENGOLD_GAME_DIR` set), and the full suite
before committing.

## Not built yet

- A program the editor can ask for what the game computes from the data: a
  monster's effective combat values, or how many monsters a party meets
  ([interface, section 7](creature-stats-editor-interface.md#7-reading-results-opengold_creature_data-game-work)).
  Imports do not need it.
- The step rolls of the areas other than the Slums, Podol Plaza and the three
  wilderness maps.
