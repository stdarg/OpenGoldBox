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

## Ray of Sickness, Ice Knife, Chromatic Orb and Acid Splash

- **Ray of Sickness** (level 1, 60 feet): a spell attack for 2d8 Poison; a hit
  leaves the target **Poisoned** (Disadvantage on attack rolls, shown as a
  condition) until the end of the Wizard's next turn. Protection from Poison
  ends it. Disadvantage on ability checks is not modeled yet.
- **Ice Knife** (level 1, 60 feet): a spell attack for 1d10 Piercing; hit or
  miss, the target and every creature within 5 feet make a Dexterity save or
  take 2d6 Cold (3d6 from a level-two slot).
- **Chromatic Orb** (level 1, 90 feet): a spell attack for 3d8 of the chosen
  type, offered once per type ("Chromatic Orb: Fire" and so on). The leap to
  another creature on matching dice is not modeled.
- **Acid Splash** (cantrip, Wizard and Sorcerer): a 5-foot-radius sphere aimed
  within 60 feet; each creature makes a Dexterity save or takes 1d6 Acid (an
  Evoker's Potent Cantrip deals half on a success). Area cantrips spend no slot.
- Area spells of level two now spend a level-two slot (Shatter and Silence spent
  a level-one slot before).
- Verification: `opengold_wizard_spell_tests` (Poisoned and its Disadvantage,
  the burst on two creatures, six orb entries and a level-one slot, Acid
  Splash's sphere without a slot, Shatter's level-two slot), the Sorcerer's six
  cantrip options.

## Sleep, Hideous Laughter and Color Spray

- **Sleep** (level 1, 60 feet, Concentration up to 1 minute): aimed like other
  areas, a 5-foot-radius sphere. Each enemy in it (allies are never chosen)
  makes a Wisdom save or is **Incapacitated** while drowsy; at the end of its
  next turn it saves again and on a failure falls **Unconscious** (Prone, Str
  and Dex saves fail, attacks against it have Advantage). Damage, the end of
  Concentration or a companion's **Shake awake** Action (within 5 feet) ends
  it. Elves, Undead and Constructs are unaffected.
- **Hideous Laughter** (level 1, 30 feet, Concentration up to 1 minute): an
  enemy makes a Wisdom save or falls **Prone and Incapacitated**, repeating the
  save at the end of each of its turns and, with Advantage, whenever it takes
  damage. The extra target from a higher slot is not modeled.
- **Color Spray** (level 1, 15-foot cone): each creature in the cone makes a
  Constitution save or is **Blinded** until the end of the Wizard's next turn.
- An Incapacitated creature can only end its turn and takes no Reactions. The
  computer wakes its sleeping companions and casts Hideous Laughter at an
  enemy without conditions.
- Verification: `opengold_wizard_spell_tests` (Sleep passes over allies and
  is shaken off, a laughing creature can only end its turn, the cone's Blinded
  creature, checkpoints of both effects).

## Grease and Web

- **Grease** (level 1, 60 feet, 1 minute, no Concentration): a 10-foot square
  of Difficult Terrain. Each creature in it when it appears, entering it or
  ending its turn in it makes a Dexterity save or falls **Prone**. The grease
  stays when the Wizard's Concentration on another spell ends and vanishes
  after a minute of combat time.
- **Web** (level 2, from Wizard level 3, 60 feet, Concentration): a 20-foot
  cube on the ground of Difficult Terrain. Each creature in it when it appears,
  entering it or starting its turn in it makes a Dexterity save or is
  **Restrained**; **Escape the webs** spends the Action on an Athletics check
  against the spell DC. A creature caught while moving stops where it entered.
  The webs' Light Obscurement, anchoring and burning are not modeled.
- Checkpoints become `OGCOMBAT 38`: each zone records when it ends (0 while
  Concentration holds it).
- Verification: `opengold_wizard_spell_tests` (Grease's square, Prone, a
  checkpoint and its expiry; Web's level-two slot and cube, the save on entry,
  the stop and Escape the webs).

## Shield

- **Shield** (level 1, Reaction): when an attack roll hits a Wizard who has
  Shield prepared, a Reaction and a slot, play stops and the Wizard is asked
  "You are hit. Cast Shield (+5 AC) or decline." The attack total is not shown,
  as at a table where the DM only says the attack hits. **React** casts it:
  +5 AC until the start of the Wizard's next turn, including against the
  triggering attack, which may now miss. Being targeted by Magic Missile asks
  the same way, and a shielded Wizard takes no Magic Missile damage.
- A critical hit is not asked about, since +5 AC cannot stop it. A Wizard who
  declines is not asked again during the same command (the later rays of one
  Scorching Ray, for example).
- How it works: the attack sits deep inside the command, so the command is
  undone at the hit, the question is asked, and the answer replays it with the
  same dice. The log shows the attack once, after the answer.
- Computer-controlled casters always cast Shield when asked.
- Checkpoints become `OGCOMBAT 39`: a pending Shield question records the
  asked creature, the command to replay and who has declined.
- Verification: `opengold_wizard_spell_tests` (the question and its
  checkpoint, React's +5 AC and slot with one resolved attack, Decline, and
  Shield blocking Magic Missile).

## Misty Step

- **Misty Step** (level 2, from Wizard level 3, Bonus Action, Verbal only):
  chosen from the A cycle, it is aimed like an area spell, but the preview
  starts on the Wizard and only unoccupied squares within 30 feet that the
  Wizard can see (not Heavily Obscured, not walls) can be chosen; Cast is
  unavailable until one is. The Wizard teleports there without provoking
  Opportunity Attacks, keeping the Action.
- Aimed spells may now be Bonus Actions: casting spends the Bonus Action
  instead of the Action.
- Verification: `opengold_wizard_spell_tests` (occupied and out-of-range
  squares refused, the teleport, the Bonus Action and level-two slot, no
  Opportunity Attack).

## Acid Arrow, Mind Spike and Ray of Enfeeblement

- **Acid Arrow** (level 2, 90 feet): a spell attack for 4d4 Acid, and 2d4 Acid
  more at the end of the target's next turn. A miss splashes half the initial
  damage and nothing later.
- **Mind Spike** (level 2, 120 feet, Somatic only, Concentration): a Wisdom
  save against 3d8 Psychic, half on a success. Knowing the target's location
  has no use here yet.
- **Ray of Enfeeblement** (level 2, 60 feet, Concentration): on a failed
  Constitution save the target is **Enfeebled**: Disadvantage on Strength
  saves and on melee weapon attacks (taken as its Strength-based attacks), and
  1d8 less damage on each attack that hits. It repeats the save at the end of
  each of its turns. On a success it still has Disadvantage on its next attack
  roll before the start of the Wizard's next turn. Strength checks and the
  damage of its spells and saves are not reduced yet.
- Fix: single-target save spells with Concentration now start it, so a second
  Hideous Laughter ends the first.
- The computer casts Acid Arrow, Mind Spike and Ray of Enfeeblement.
- Verification: `opengold_wizard_spell_tests` (the hit and the later burn,
  the splash on a miss, Mind Spike's save, both outcomes of the ray and their
  Disadvantage, a checkpoint, Concentration replacing Hideous Laughter).

## Blur, Mirror Image and Magic Weapon

- **Blur** (level 2, self, Verbal only, Concentration up to 1 minute): attack
  rolls against the Wizard have Disadvantage. No creature has Blindsight or
  Truesight yet, so none ignores it.
- **Mirror Image** (level 2, self, 1 minute): three duplicates, shown as
  "Mirror Image (3 duplicates)". Each hit rolls a d6 per duplicate left; any 3
  or higher and a duplicate takes the hit instead and vanishes. A Blinded
  attacker is not fooled. Shield is asked about before the duplicates roll.
- **Magic Weapon** (level 2, Bonus Action, touch): the creature's weapon
  attacks gain +1 to attack and damage rolls for an hour; casting it again
  ends the earlier one. Adaptation: the bonus follows the creature's weapon
  attacks rather than one particular weapon.
- Verification: `opengold_wizard_spell_tests` (Disadvantage against a blurred
  Wizard, a duplicate taking a hit and its checkpoint, Magic Weapon's Bonus
  Action and +1 to the attack roll).

## Invisibility, See Invisibility and Darkness

- **Invisibility** (level 2, touch, Concentration up to 1 hour): the creature
  is **Invisible**: attack rolls against it have Disadvantage, its own have
  Advantage, spells that need sight cannot target it, and it can leave an
  enemy's reach without an Opportunity Attack. It ends right after the
  creature makes an attack roll (which still has Advantage) or casts a spell.
  The extra targets from higher slots are beyond levels 1-4.
- **See Invisibility** (level 2, self, 1 hour): Invisible creatures are seen as
  if visible.
- **Darkness** (level 2, 60 feet, Verbal only, Concentration up to 10 minutes):
  aimed like other areas, a 15-foot-radius sphere that is Heavily Obscured,
  like Fog Cloud: creatures in it can neither see nor be seen. Casting it on
  an object and dispelling light spells are not modeled.
- Verification: `opengold_wizard_spell_tests` (Disadvantage against and
  Advantage for an Invisible Wizard, no Opportunity Attack, the attack ending
  it; a foe that can't be targeted until See Invisibility; Darkness's sphere
  hiding a creature).

## Flaming Sphere

- **Flaming Sphere** (level 2, 60 feet, Concentration up to 1 minute): aimed
  at an unoccupied square (the preview starts beside the nearest enemy), it is
  drawn as an orange ball. Any creature ending its turn within 5 feet makes a
  Dexterity save against 2d6 Fire, half on a success.
- **Roll Flaming Sphere** (Bonus Action on later turns): choose a creature
  within 30 feet of the sphere; it rolls into that creature, which saves at
  once, and stops in the open square beside it. Rolling it to an empty square,
  jumping pits and its light are not modeled.
- Verification: `opengold_wizard_spell_tests` (the open square, the burn at
  the end of a turn, the Bonus Action roll, a checkpoint).

## Knock

- **Knock** (level 2, Verbal only): at a locked door the menu offers
  **Knock** beside Bash and Pick when an active member has it prepared and a
  level-two slot. The first such member casts it, the slot is spent, and the
  door opens: "Wizard casts Knock, and the lock opens." It is never offered in
  combat. The loud knock it makes draws no attention yet, and wizard-locked
  doors (Arcane Lock) are not modeled.
- Rules modules gain `can_cast_exploration_spell` and `cast_exploration_spell`
  for spells cast while exploring.
- Verification: `opengold_wizard_spell_tests` (no Knock without the spell, the
  cast with its level-two slot, never offered in combat).
