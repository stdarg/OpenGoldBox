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
  an enemy on its turn extends it, so does a thrown Acid, Alchemist's Fire or
  Oil that makes an enemy save, and so does **Extend Rage** (a Bonus Action
  offered on a later turn). It ends early when the Barbarian is Incapacitated
  or drops to 0 Hit Points.
- Advantage on Strength checks applies to the Athletics check that escapes
  Entangle, Web or Ensnaring Strike, the only Strength check the game rolls.
- Not modeled: the 10-minute limit (no fight lasts that long) and donning armor
  mid-Rage.
- The computer rages before it fights. Rage and Extend Rage sit in the Bonus
  Action list.
- Verification: `opengold_barbarian_tests` (the Bonus Action and its use, the
  Rage Damage, a checkpoint, Resistance, extension by attack, by a thrown
  flask's save and expiry, the escape check's Advantage, no Rage in Heavy armor).

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
  Rages, Reckless Attack's Advantage both ways, Reckless lasting until the next
  turn and through a checkpoint, Danger Sense's Advantage on Dexterity saves and
  its loss while Incapacitated, Primal Knowledge refusing a held, foreign or
  unknown skill without change, Frenzy's damage),
  `opengold_archery_tests` (Barbarians advance).

## Levels 5–15

In the campaign a Barbarian stops at level 8, the Fighters' hall's limit
([CAP-1](SRD-DECISIONS.md#cap-1-2026-10-14-the-original-training-halls-limits)),
until [LEVELS-2](SRD-DECISIONS.md#levels-2-2026-10-15-level-cap-15)'s one limit
for every class is settled; the rules go to 15. Reckless Attack is offered only with the turn's first
attack roll, as the SRD has it, now that a turn can hold two.

| Level | Gains |
| ---: | --- |
| 5 | Extra Attack; Fast Movement (Speed +10 feet without Heavy armor); Proficiency Bonus +3 |
| 6 | Berserker: Mindless Rage; a fourth Rage |
| 7 | Feral Instinct (Advantage on Initiative); Instinctive Pounce |
| 8 | A feat or ability points |
| 9 | Brutal Strike; Rage Damage +3; Proficiency Bonus +4 |
| 10 | Berserker: Retaliation; a fourth Weapon Mastery |
| 11 | Relentless Rage |
| 12 | A feat or ability points; a fifth Rage |
| 13 | Improved Brutal Strike; Proficiency Bonus +5 |
| 14 | Berserker: Intimidating Presence |
| 15 | Persistent Rage |

- **Extra Attack:** as the [Fighter's](FIGHTER.md).
- **Mindless Rage:** while raging, Charm Person does not take, and entering a
  Rage ends Charmed and Frightened. No enemy in the game charms or frightens a
  party member yet; a raging Berserker cannot be Frightened.
- **Instinctive Pounce:** entering a Rage adds half the Speed to the turn's
  movement, to be used at once: any other command first gives up what is left.
  It is ordinary movement and provokes Opportunity Attacks.
- **Brutal Strike:** after Reckless Attack, once a turn, the A cycle offers
  "Brutal Strike (Forceful Blow)" and "Brutal Strike (Hamstring Blow)" for a
  Strength melee attack. It gives up the roll's Advantage (a roll with
  Disadvantage stays an ordinary attack). On a hit it deals 1d10 more damage
  and either pushes the target 15 feet, after which the Barbarian may move up to
  half its Speed straight toward it without provoking (the free-move controls),
  or lowers the target's Speed by 15 feet until the Barbarian's next turn (one
  Hamstring Blow at a time). A push into a wall deals damage
  ([PUSH-1](SRD-DECISIONS.md#push-1-2026-10-14-pushed-into-a-wall)).
- **Retaliation:** when a creature within 5 feet damages the Berserker, React
  makes one melee attack against it with the Reaction; Decline keeps it.
- **Relentless Rage:** a raging Barbarian dropped to 0 Hit Points but not
  killed outright makes a Constitution save, DC 10 and 5 more for each earlier
  try since its last Short or Long Rest; on a success it has twice its level in
  Hit Points. It is always tried, before an Orc's Relentless Endurance. The
  rest window and combat resources show the tries left (five: no save passes a
  DC above 30).
- **Improved Brutal Strike:** the A cycle also offers "Brutal Strike
  (Staggering Blow)": the target has Disadvantage on its next saving throw and
  makes no Opportunity Attacks until the start of the Barbarian's next turn;
  and "Brutal Strike (Sundering Blow)": until then, the next attack roll another
  creature makes against it has +5.
- **Intimidating Presence:** a Bonus Action. Each enemy within 30 feet in line
  of sight makes a Wisdom save (DC 8 + Strength modifier + Proficiency Bonus)
  or is Frightened for up to a minute, repeating the save at the end of each of
  its turns. The SRD lets the Barbarian choose the creatures; every enemy is the
  only sensible choice, so no allies are asked about. Once per Long Rest; with
  that use spent, "Intimidating Presence (1 Rage)" spends a Rage use instead.
- **Frightened** (new with Intimidating Presence): while the creature that
  frightens it is alive and in its line of sight, a Frightened creature has
  Disadvantage on attack rolls and ability checks and cannot move to a square
  closer to it. A Halfling's Brave gives Advantage on the saves.
- **Persistent Rage:** when Initiative is rolled while a Rage use is spent, the
  Initiative dialog offers Persistent Rage, as it offers Uncanny Metabolism: all
  Rage uses come back, once per Long Rest. The Rage needs no extending, so
  "Extend Rage" is not offered.
- Verification: `opengold_barbarian_levels_tests` (the campaign's stop at 8,
  four Rages and the fifth at 12, Fast Movement, the level 11–15 grants and
  resources, Reckless Attack only first, Extra Attack, Instinctive Pounce and
  its checkpoint, both Brutal Strike blows and the follow-up move, Retaliation,
  Relentless Rage, Staggering and Sundering Blows, Intimidating Presence and
  Frightened movement, Persistent Rage).
