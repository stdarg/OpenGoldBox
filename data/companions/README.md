# Companions

The original's NPCs who join the party through `ADD NPC`, as SRD characters.
The game reads `companions.json`. Edits made in the
[Creature Stats Editor](../../dev_tools/docs/creature-stats-editor-requirements.md)
reach it through `opengold_import_edits`
([guide](../../dev_tools/docs/creature-edits-import.md),
[interface](../../dev_tools/docs/creature-stats-editor-interface.md)).

Format `opengold-companions-v1`; one entry per companion:

- `bank` and `record`: the `MONnCHA.DAX` record `ADD NPC` names. A record has
  one companion at most.
- `name`, `race`, `gender`, `class`, `alignment` and `background`: the
  character creation rules' keys, such as `human`, `fighter`, `lawful_good` and
  `soldier`.
- `rolled`: the six scores rolled, 3 to 18, from Strength to Charisma, before
  the background's increase.
- `background_increase`: the abilities the background raises by 2
  (`plus_two`) and by 1 (`plus_one`).
- `level`: 1 to 15. The companion is made at level 1 and reaches it with the
  rules' default choice at each level, as the scores in each `note` assume.
- `items`: added in order after advancing; each a `key`, a `name`, a
  `quantity` (1 when left out) and the original's item type
  (`original_type`) where it has one. The companion joins with them on.
- `note`: where the values come from.

The game builds a companion when a script adds the NPC. A companion who has
joined is the character in the save; a change applies to the next one who
joins.
