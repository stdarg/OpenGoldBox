# Barbarian

Levels 1–4 of the SRD 5.2.1 Barbarian (pp. 28–30). Weapon Mastery and
Unarmored Defense came earlier; see [Armor](ARMOR.md).

## Rage (level 1)

- **Rage** (Bonus Action, not in Heavy armor): two uses at levels 1–2, three
  from level 3; one returns on a Short Rest, all on a Long Rest. The uses share
  the character's Channel Divinity store, which no Barbarian has, so vitals and
  checkpoints keep their format.
- While **Raging** (shown as a condition): Resistance to Bludgeoning, Piercing
  and Slashing damage; Strength-based weapon and unarmed hits deal 2 more
  ("Rage adds 2 damage."); Advantage on Strength saves; no spells (none are
  offered) and no Concentration (entering a Rage ends it).
- It lasts until the end of the Barbarian's next turn. An attack roll against
  an enemy on its turn extends it, and so does **Extend Rage** (a Bonus Action
  offered on a later turn). It ends early when the Barbarian is Incapacitated
  or drops to 0 Hit Points.
- Not modeled: Advantage on Strength checks, the 10-minute limit (no fight
  lasts that long), extending by forcing a save, and donning armor mid-Rage.
- The computer rages before it fights. Rage and Extend Rage sit in the Bonus
  Action list.
- Verification: `opengold_barbarian_tests` (the Bonus Action and its use, the
  Rage Damage, a checkpoint, Resistance, extension by attack and expiry, no
  Rage in Heavy armor).

## Levels 2–4

- Barbarians now advance to level 4 with the usual fixed Hit Point growth.
- **Danger Sense** (level 2): Advantage on Dexterity saves unless
  Incapacitated.
- **Reckless Attack** (level 2): offered as **Reckless attack** beside each
  melee target while the Attack action's attack (through level 4 the turn's
  first and only one) is Strength-based. Until the start of the Barbarian's
  next turn its Strength attack rolls have Advantage and attack rolls against
  it have Advantage; it shows as **Reckless**.
- **Berserker: Frenzy** (level 3): while raging and reckless, the turn's first
  Strength-based hit deals 2d6 more (a d6 per point of Rage Damage).
- **Primal Knowledge** (level 3): one more Barbarian skill, chosen at level-up
  from those not yet held. Making checks with Strength while raging is cut
  (SRD-DECISIONS).
- A third Rage at level 3; a third Weapon Mastery and a feat or ability points
  at level 4.
- Verification: `opengold_barbarian_tests` (the grants at each level, three
  Rages, Reckless Attack's Advantage both ways, Frenzy's damage),
  `opengold_archery_tests` (Barbarians advance).
