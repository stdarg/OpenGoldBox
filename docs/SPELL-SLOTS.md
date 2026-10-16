# Spell slots by level

Part of [#176](https://github.com/stdarg/OpenGold/issues/176) (levels 5–15).

## Slots

- Each character keeps spell slots of levels 1 to 9, filled from SRD 5.2.1's
  class tables through class level 15: the full casters (Bard, Cleric, Druid,
  Sorcerer, Wizard), the Paladin and Ranger, and the Warlock's Pact Magic,
  whose slots are all of one level (level 2 from Warlock 3, 3 from 5, 4 from 7,
  5 from 9; three slots from 11). Only the classes whose cap allows it reach
  the higher rows.
- The vital state (`SRD13`) and the combat checkpoint (`OGCOMBAT 56`) store
  all nine counts. The rest screen lists a pool for each level the character
  has ("Level-three spell slots").
- One slot a turn, as before, whatever its level.

## Casting from a higher slot

- A spell is offered at its own level and, beside it, at each higher slot
  level that changes it: more dice, more creatures, a larger area, or a rider
  that grows (Aid +5 Hit Points a level, False Life +5, Ice Knife's burst,
  Magic Weapon +2 from a level-4 slot, Dispel Magic's automatic level).
  "Cure Wounds (level 3 slot)" is the A-cycle entry, verb `cure_wounds_3`.
- A spell a higher slot does not change (Shield of Faith, Protection from Evil
  and Good) is offered only at its own level while those slots last, then at
  the lowest level with a slot left. Before this, a level-1 spell was always
  also offered from a level-2 slot.
- The camp's Cast / Use list follows the same rule.
- The smites follow it after a hit: "Divine Smite (level 3 slot)" deals 4d8.
  Searing Smite stays at level 1: its burn from a higher slot is not built.
  Ensnaring Strike's vines deal 1d6 more a turn for each level above 1; the
  caster carries the count (an `ensnaring` effect) while it concentrates.
- Shield and Hellish Rebuke, cast as Reactions, take the lowest slot left;
  Hellish Rebuke deals 1d10 more for each level above 1.
- Upcast rules added with the Paladin: Heroism takes one more creature per
  level above 1, Prayer of Healing heals 1d8 more per level above 2.
- A label "X (level N slot)" with no Spanish entry of its own is translated as
  the spell's name plus "(espacio de nivel N)".

## Spellcasting modifier

Healing, Heroism's Temporary Hit Points and the other features that add the
spellcasting ability modifier used the spell attack bonus minus 2, which was
right only while the Proficiency Bonus was +2. They now subtract the
Proficiency Bonus of the character's level (rules 0.6.205).

Verification: `opengold_paladin_levels_tests` (offers at each level, a
level-3 Cure Wounds and Divine Smite, Protection from Evil and Good at its own
level only), `opengold_spell_table_tests` (the offer baseline) and the save
format tests.

## Cantrip damage by level

A cantrip's damage dice grow with the caster's character level (SRD 5.2.1's
Cantrip Upgrade): two dice from level 5, three from 11, four from 17 (Fire
Bolt 2d10 at 5). Eldritch Blast fires a beam for each step instead, each its
own attack roll, aimed one creature at a time as Scorching Ray's rays are
([SPLIT-1](SRD-DECISIONS.md#split-1-2026-10-13-aiming-darts-and-rays)); a beam
whose creature has fallen turns to the nearest foe. True Strike adds 1d6
Radiant damage from level 5 (2d6 from 11), and Shillelagh's die becomes a d10
at 5 and a d12 at 11. A Dragonborn's Breath Weapon grows the same way (2d10 at
5, 3d10 at 11). Rules 0.6.215.

