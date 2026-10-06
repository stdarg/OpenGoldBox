# Monk

Levels 1–4 of the SRD 5.2.1 Monk (pp. 48–50). Unarmored Defense came earlier;
see [Armor](ARMOR.md). Slow Fall is dropped (SRD-DECISIONS).

## Martial Arts (level 1)

- While the Monk wears no armor, holds no Shield and wields nothing or only
  Monk weapons (Simple Melee weapons and Martial Melee weapons with the Light
  property), its Unarmed Strikes and Monk weapons use the better of Strength
  and Dexterity for attack and damage rolls, and deal at least the Martial
  Arts die, a d6 through level 4. An Unarmed Strike deals 1d6 + that modifier
  Bludgeoning damage.
- **Unarmed Strike** (Bonus Action): one more Unarmed Strike against an enemy
  within 5 feet, from the Bonus Action list, then a click on the target. It
  carries no weapon's mastery or feat.
- Grapple and Shove are dropped (SRD-DECISIONS).
- Verification: `opengold_monk_tests` (Dexterity-based Unarmed Strikes and
  their damage, the Bonus Action strike, a dagger as a Monk weapon, no Martial
  Arts in armor).

## Levels 2–4

- Monks now advance to level 4.
- **Monk's Focus** (level 2): Focus Points equal to the Monk level, all back
  on a Short or Long Rest. They share the Action Surge store, which no Monk
  has, so vitals and checkpoints keep their format. From the Bonus Action list:
  - **Flurry of Blows** (1 Focus): two Unarmed Strikes at the chosen enemy
    within 5 feet; the second only if the first leaves it standing.
  - **Patient Defense: Disengage** (free), or **Disengage and Dodge** (1 Focus).
  - **Step of the Wind: Dash** (free), or **Disengage and Dash** (1 Focus). The
    doubled jump has no use.
- **Unarmored Movement** (level 2): 10 feet more Speed without armor or a
  Shield.
- **Uncanny Metabolism** (level 2, once per Long Rest; it shares Arcane
  Recovery's store): when Initiative is rolled and the Monk has spent Focus or
  lost Hit Points, the Initiative window (shared with Alert, now titled
  "Initiative") offers **Uncanny Metabolism**: all Focus Points back and 1d6 +
  the Monk level in Hit Points. A Monk with Alert picks either the swap or
  Uncanny Metabolism.
- Level 3 records **Deflect Attacks** and the **Warrior of the Open Hand**
  (Open Hand Technique); their effects arrive in the next increment. Level 4
  brings a feat or ability points. Slow Fall is dropped (SRD-DECISIONS).
- Verification: `opengold_monk_tests` (the grants, four Focus and 40 feet at
  level 4, Flurry of Blows, Step of the Wind, Patient Defense's Dodge, Uncanny
  Metabolism at Initiative), `opengold_archery_tests` (Monks advance).
