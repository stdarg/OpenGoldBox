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
