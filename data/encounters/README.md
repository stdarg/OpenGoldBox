# Encounter data

The [Creature Stats Editor](../../dev_tools/docs/creature-stats-editor-requirements.md)
writes these files and the game reads them
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
