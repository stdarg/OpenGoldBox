# Wizard

Milestone [#8](https://github.com/stdarg/OpenGold/issues/8), delivered class by
class ([CLASS-1](SRD-DECISIONS.md#class-1-2026-10-04-finishing-levels-14-for-all-classes)).
SRD 5.2.1 pp. 77–80. The spellbook, preparation and cantrip replacement are
described in [Wizard spell choices](WIZARD-SPELL-CHOICES.md).

| Level | Feature | Status |
| --- | --- | --- |
| 1 | Spellcasting, Ritual Adept, Arcane Recovery | Delivered earlier (Ritual Adept removed by DM-1) |
| 2 | Scholar | Delivered earlier ([Scholar](SCHOLAR.md)) |
| 3 | Wizard Subclass (Evoker) | Delivered (rules 0.6.93); Evocation Savant removed by the 2026-09-30 simplification |
| 4 | Ability Score Improvement | Delivered earlier |

## Spells

- The Wizard list in the game: the cantrips Chill Touch, Fire Bolt, Poison
  Spray, Ray of Frost and Shocking Grasp; level 1 Fog Cloud, Longstrider, Magic
  Missile and Protection from Evil and Good; level 2 Blindness, Hold Person and
  Scorching Ray. The shared spells work as described for the
  [Paladin](PALADIN.md#buffs-and-concentration), [Ranger](RANGER.md) and
  [Cleric](CLERIC.md#hold-person-and-paralyzed).
- Level-up and rest defaults learn and prepare the first available spells, so a
  default Wizard now learns more than Magic Missile.
- The other CRPG-relevant Wizard spells come in later increments.

## Burning Hands, Thunderwave and Shatter

- Area spells are aimed as recorded in
  [CLASS-5](SRD-DECISIONS.md#class-5-2026-10-05-aiming-area-spells). Cones and
  cubes extend from the caster toward the aimed square.
- **Burning Hands** (level 1, Action): a 15-foot cone (seven squares, those
  within 15 feet and about 30 degrees of the aim); each creature in it makes a
  Dexterity save, 3d6 Fire damage, half on a success (+1d6 from a level-two
  slot).
- **Thunderwave** (level 1, Action): a 15-foot cube with a face against the
  Wizard, on the aimed side (diagonals take the corner); Constitution save, 2d8
  Thunder damage, half on a success, and a failed save also pushes the creature
  10 feet straight away while the way is open (+1d8 from a level-two slot). Its
  sound is not modeled.
- **Shatter** (level 2, Action, 60 feet): a 10-foot-radius sphere; Constitution
  save, 3d8 Thunder damage, half on a success. Its Disadvantage for creatures of
  inorganic material is not modeled.
- The caster is never caught in its own area; allies are, unless an Evoker
  sculpts the spell.
- Verification: `opengold_wizard_spell_tests` (the cone's seven squares and who
  is hit, the cube and a 10-foot push, the sphere and who is hit).

Verification: the Wizard spellbook, preparation and advancement checks in
`opengold_spell_access_tests`, `opengold_advancement_tests`,
`opengold_rules_tests`, `opengold_spell_component_tests`,
`opengold_poison_spray_tests` and `opengold_spell_table_tests` (offer
baseline), and `tests/wizard_choices_view_tests.gd`.

## Evoker (level 3)

- The Evoker is the SRD's only Wizard subclass, taken automatically at level
  three.
- **Potent Cantrip**: when a damaging cantrip misses, or its target succeeds on
  the save, the target still takes half the damage, without the cantrip's other
  effects.
- **Sculpt Spells**: in an Evocation area spell (Burning Hands, Thunderwave,
  Shatter) up to 1 + the spell's level allies the Evoker can see are spared:
  they take no damage and no other effect. Adaptation: the allies are chosen
  automatically.
- Verification: `opengold_wizard_spell_tests` (a missed Fire Bolt at levels one
  and three, an ally in Burning Hands at levels one and three). The cantrip
  tests' exact-damage checks no longer pin a level-three miss.

## Mage Armor, False Life and Expeditious Retreat

- **Mage Armor** (level 1, Action, touch, 8 hours): a willing creature wearing
  no armor has base AC 13 + Dexterity modifier (a shield still adds). It is
  offered only on an unarmored character not already warded. Casting it from
  the Camp dialog is not available yet.
- **False Life** (level 1, Action, self): 2d4 + 4 Temporary Hit Points, 5 more
  from a level-two slot; when the Wizard already has some, the usual
  keep-or-replace choice appears.
- **Expeditious Retreat** (level 1, Bonus Action, self, Concentration up to 10
  minutes): the Wizard Dashes at once, and on later turns **Expeditious
  Retreat: Dash** is a Bonus Action.
- Verification: `opengold_wizard_spell_tests` (AC 13 + Dexterity and no second
  cast, 6–12 Temporary Hit Points, the first and a later Dash, a checkpoint).
  The generic spell table probes now also swap a Wizard's book entry.
