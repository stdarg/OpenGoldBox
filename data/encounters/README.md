# Encounter data

The game reads these files. Edits made in the
[Creature Stats Editor](../../dev_tools/docs/creature-stats-editor-requirements.md)
reach them through `opengold_import_edits`
([interface](../../dev_tools/docs/creature-stats-editor-interface.md)). They hold
OpenGoldBox's own values and references to the original's records, never
copies of those records, which stay in the player's install.

## conversions.json

What each area's original creature records fight as. Format
`opengold-conversions-v1`; one entry per area:

- `scripts`: the area scripts the entry is for. Valjevo Castle's four scripts
  (3, 4, 5 and 7) share one entry. A script is in one entry at most, and each
  is an area of the game's area table in the entry's bank.
- `bank`: the `MONnCHA.DAX` archive the records are in.
- `name`, and `audit`, the area's audit in `docs/audits/` where it has one.
- `creatures`: for each `record`, its `combat.rules` stat line
  (`definition`), the XP that sizes the fight (`fit_xp`) and the experience it
  awards (`award_xp`), each a whole number up to 1,000,000. A record appears
  once per entry.

A map holds its own script's creatures and those of the scripts that share it
(the castle's quarters, and the old town's map that the castle's script 5 also
loads); a record keeps its first conversion. A record the scripts load that has
no entry stops the game ("Encounter needs an explicit supported creature
conversion"). The Slums creatures' experience follows
[EXPEDITION.md](../../docs/EXPEDITION.md); the set-encounter creatures added
later and Kuto's Well award each stat block's XP, a leader double
([kutos-well.md](../../docs/audits/kutos-well.md)).

## overrides.json

What a script's `LOAD MONSTER` loads instead of the original's. Optional:
without the file every fight is the original's. Format
`opengold-encounter-overrides-v1`; one entry per instruction:

```json
{"site": "ECL5.DAX:7:0xA60D", "original": {"record": 85, "count": 12},
 "count": 6, "reason": "Why, also written to docs/DEVIATIONS.md"}
```

- `site`: the script's archive, its record and the instruction's address in
  hex, as `opengold_scripts --inspect GAME_DIR ECL5.DAX 7` lists it.
- `original`: the record and count the instruction loads. The game checks them
  against the player's script when it loads, and refuses an override at an
  instruction that is not a `LOAD MONSTER` of them. An instruction that reads
  its record or count from the script's variables, as table-rolled fights do,
  cannot be overridden.
- `record`, `count` and `icon`: what the instruction loads now; one left out
  stays the original's. The record must be converted on every map its script
  loads. A changed record keeps the instruction's icon unless `icon` names
  another; a creature with art in `data/monsters` shows that art either way.
- The count is still sized to the party, as the original's is: it is the most
  the party can meet.
- `reason`: why, in one line.

## chances.json

The numbers an original script sets a wandering encounter's chance with, read
in place of the original's. Optional: without the file every chance is the
original's. Format `opengold-encounter-chances-v1`; one entry per number:

```json
{"site": "ECL2.DAX:20:0x9B60", "operand": 1, "original": 12, "value": 13,
 "reason": "Fewer street groups"}
```

- `site` and `operand` (0 or 1): the instruction and the number's place in it.
  It must be a `RANDOM`'s range (operand 0), either side of a `COMPARE`, or the
  value of a `SAVE` into the rest check's interval (0x6DD2) or chance in
  percent (0x6DD3).
- `original`: the number the player's script holds there, checked when the
  game loads; `value`: the new one, 0 to 255.
- The script's bytes, which saves keep, stay the original's: the game reads the
  new number as it decodes the instruction.

Which numbers make up each area's chance is in the
[interface](../../dev_tools/docs/creature-stats-editor-interface.md#6-wandering-encounter-chances-chancesjson).
