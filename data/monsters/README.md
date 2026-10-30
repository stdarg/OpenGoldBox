# Exported monsters

The [Combat Sprite Catalog](../../dev_tools/combat_sprite_catalog/README.md)
writes these folders with **Export to game** or `--export-to-game`; the game
reads them and nothing in `dev_tools/` ([#274](https://github.com/stdarg/OpenGoldBox/issues/274)).
Each folder is one creature, named like its catalog record (`mon2cha_dax_4` is
`MON2CHA.DAX` record 4, `custom_...` a copy the catalog made). It holds
`monster.json` and, for each weapon set with reviewed art, `ready.png` and
`action.png`, 128 × 128 pixels.

`monster.json` gives the creature's original record, its name, its sprite
size and humanoid height, its equipment and its weapon sets:

- **Name.** A creature the game stages fights and is logged under this name.
- **Art.** The game shows the weapon set of what the creature readies (an
  archer's bow), alternating its ready and action poses, at the catalog's
  fixed scale with both poses' feet on the square. A creature without art
  here keeps its original combat icon, with the icon's action pose.
- **Equipment.** Each original item names its `MONnITM` slot
  (`source_index`); one the catalog left out is not carried into a fight and
  is not left behind after it. Gear added in the catalog has `source_index`
  -1. With an original item type (`source_type`) it is left behind as a plain
  item of that type, copied from New Phlan's shop lists (a type they lack is
  not); without one it only picks the art.
- **Weapons.** A creature drawn at a humanoid height hits with the weapon it
  holds, original or added: its readied melee weapon (else its first), and a
  readied ranged weapon or an archer's bow. The weapon's dice and damage type
  replace its stat block's; the stat block keeps its bonuses.
- **Forms.** Wild Shape's and Polymorph's Beast forms and summoned creatures
  wear the art of the creature drawn with their original icon (a Wolf form,
  `mon4cha_dax_106`'s).
- **Copies** keep their own name, gear and art; no encounter places them yet.

A record flagged for regeneration exports its data but no art. Do not edit
these files; change the catalog and export again. The catalog's
`--catalog-check` fails when this folder is out of date, and
`opengold_monster_bundle_tests` reads it as the game does. Every image here
is new art; none is copied from the original game.
