# Druid

Levels 1–4 of the SRD 5.2.1 Druid (pp. 41–46). Wisdom is the spellcasting
ability.

## Spellcasting, Primal Order and the Circle of the Land

- Druids prepare spells from the Druid list like Clerics: two cantrips (three
  from level 4) and 4/5/6/7 prepared spells, with full-caster slots, chosen at
  creation and on gaining a level; a Long Rest may change them. The
  implemented Druid spells through level 2 are on the list. Replacing a
  cantrip on gaining a level is not offered yet.
- **Primal Order** (level 1), chosen with the Training controls:
  **Magician** learns one more Druid cantrip; **Warden** trains Martial
  weapons and Medium armor.
- **Produce Flame** (cantrip, Bonus Action, 10 minutes): a flame in hand,
  shown as a condition. While it lasts, "Hurl flame" is an Action (a Magic
  action, not a casting) making a ranged spell attack for 1d8 Fire within 60
  feet; the flame stays. Casting it again replaces it.
- **Shillelagh** (cantrip, Bonus Action, 1 minute): offered only while holding
  a Club or Quarterstaff. Melee attacks with it use the spellcasting ability
  for the attack and damage rolls and a d8. The weapon keeps its Bludgeoning
  damage; the Force alternative is not offered.
- **Barkskin** (level 2, Bonus Action, touch, 1 hour): the creature's AC is 17
  if it was lower.
- **Circle of the Land** (level 3), the SRD's only Druid subclass: the land
  type is a Training choice at level-up. Its Circle Spells are always
  prepared: Arid (Blur, Burning Hands, Fire Bolt), Polar (Fog Cloud, Hold
  Person, Ray of Frost), Temperate (Misty Step, Shocking Grasp, Sleep) or
  Tropical (Acid Splash, Ray of Sickness, Web). The land is not changed on a
  Long Rest.
- Druidic and Wild Companion are removed (SRD-DECISIONS). Wild Shape and
  Land's Aid are below.
- Verification: `opengold_druid_tests` (Produce Flame and its hurl,
  Shillelagh's Wisdom attack bonus, Magician's cantrip and Warden's armor,
  the Arid Circle Spells and Fire Bolt, Barkskin's AC, a third cantrip at
  level 4), `opengold_spell_table_tests` (Druid probes).

## Flame Blade, Moonbeam, Spike Growth and Heat Metal

- **Flame Blade** (level 2, Bonus Action, Concentration): a fiery blade in hand,
  shown as a condition. While it lasts, "Flame Blade attack" is an Action (a
  Magic action) making a melee spell attack for 3d6 + the spellcasting modifier
  Fire against a creature within 5 feet. The free-hand requirement is not
  checked.
- **Moonbeam** (level 2, Concentration): an aimed 5-foot-radius beam within 120
  feet. A creature makes a Constitution save against 2d10 Radiant, half on a
  success, when the beam appears or moves onto it, when it enters the beam and
  when it ends its turn there, at most once a turn. On later turns "Move
  Moonbeam" is a Magic action that moves the beam onto a creature within 60
  feet of it. Shape-shifting is not affected yet.
- **Spike Growth** (level 2, Concentration): an aimed 20-foot-radius Sphere
  within 150 feet becomes Difficult Terrain; a creature takes 2d4 Piercing for
  each square it moves into or within it on its own move. Forced movement and
  the camouflage's Perception check are not modeled.
- **Heat Metal** (level 2, Druid and Bard, Concentration): offered only against
  a creature wearing metal armor (Medium or Heavy armor other than Hide), as
  the user decided on 2026-10-06. Player characters are judged from their
  equipment, monsters from their `equipment` rows in combat.rules
  ([creature equipment](CREATURE-EQUIPMENT.md)): the Slums goblin and orc
  leaders wear metal. The armor deals 2d8 Fire; a failed Constitution save gives
  Disadvantage on attack rolls until the start of the caster's next turn (worn
  armor cannot be dropped). "Heat Metal again" repeats it as a Bonus Action on
  the caster's later turns within 60 feet. Ability checks are not affected.
- Verification: `opengold_druid_tests` (the blade's Bonus Action and attack,
  the beam's saves on appearing, entering, ending a turn and moving, Difficult
  Terrain and spike damage, Heat Metal on a character in Chain Mail but not on
  a monster, and its later Bonus Action).

## Wild Shape and Land's Aid

- **Wild Shape** (level 2): two uses, one back on a Short Rest and all on a
  Long Rest, kept in the store Channel Divinity and Rage use. The Bonus Action
  dropdown offers "Wild Shape: Wolf", "...: Boar", "...: Giant Lizard" and
  "...: Giant Snake", and "Leave Wild Shape" while in a form (CLASS-10: the
  Beasts with game art that the SRD allows at levels 2-4; every Druid knows
  all four).
- In a form the Druid keeps its Hit Points, mental scores, proficiencies and
  features and gains Temporary Hit Points equal to its Druid level. The
  Beast's AC, Speed, size, Strength, Dexterity, physical saves and attack
  replace the Druid's; equipment merges into the form (so Heat Metal finds no
  armor); no spells are cast, but Concentration continues.
  - Wolf: AC 12, Speed 40, Bite +4 for 1d6 + 2 Piercing; Pack Tactics, and a
    hit knocks a Medium or smaller creature Prone.
  - Boar: AC 11, Speed 40, Gore +3 for 1d6 + 1 Piercing; Bloodied Fury
    (Advantage at half its Hit Points or fewer). The charge is not modeled.
  - Giant Lizard (Large): AC 12, Speed 40, Bite +4 for 1d8 + 2 Piercing.
  - Giant Snake (the SRD Constrictor Snake, Large): AC 13, Speed 30, Bite +4
    for 1d8 + 2 Piercing. Constrict is not offered: grappling is not planned
    (SCOPE-2), and its Swim Speed goes unused.
  - Large forms take one square until multi-square footprints
    ([#45](https://github.com/stdarg/OpenGold/issues/45)).
- The form ends on "Leave Wild Shape", another Wild Shape, 0 Hit Points, being
  Incapacitated at the start of the Druid's turn, or after an hour (half the
  Druid level in hours from level 4 is not distinguished).
- The combat view draws the Druid as the Beast's original combat icon (the
  art research's candidates: Wolf CPIC4 106, Wild Boar CPIC6 120, Giant
  Lizard CPIC8 59, Giant Snake CPIC5 60).
- **Land's Aid** (Circle of the Land, level 3): a Magic action spending a use
  of Wild Shape, aimed at a point within 60 feet. Each enemy in the 10-foot
  Sphere makes a Constitution save against the spell save DC, taking 2d6
  Necrotic, half on a success; the most wounded ally in it regains 2d6 Hit
  Points. The enemies and the ally are chosen automatically.
- Verification: `opengold_druid_tests` (the Wolf's AC, Temporary HP, no
  spells, a checkpoint, the bite and its Prone, leaving the form, and Land's
  Aid's thorns, healing and spent use).
