# Ranger

Milestone [#8](https://github.com/stdarg/OpenGoldBox/issues/8), delivered class by
class ([CLASS-1](SRD-DECISIONS.md#class-1-2026-10-04-finishing-levels-14-for-all-classes)).
SRD 5.2.1 pp. 57–60. This page grows with each Ranger increment.

| Level | Feature | Status |
| --- | --- | --- |
| 1 | Spellcasting | Delivered (rules 0.6.75) |
| 1 | Favored Enemy | Delivered (rules 0.6.75) |
| 1 | Weapon Mastery | Delivered earlier ([Weapon Mastery](WEAPON-MASTERY.md)) |
| 2 | Deft Explorer | Nothing to deliver in levels 1–4 ([2026-09-30 simplification](SRD-DECISIONS.md#2026-09-30-simplification)) |
| 2 | Fighting Style | Delivered earlier ([Fighting Style routes](FIGHTING-STYLE-ROUTES.md)); Druidic Warrior delivered (rules 0.6.76) |
| 3 | Ranger Subclass (Hunter) | Delivered (rules 0.6.77); Hunter's Prey changes after a rest (rules 0.6.175) |
| 4 | Ability Score Improvement | Delivered earlier |
| 5 | Extra Attack, level-2 spells | Delivered (rules 0.6.210) |
| 6 | Roving | Speed delivered (rules 0.6.210); Climb and Swim Speeds cut (SCOPE-2) |
| 7 | Defensive Tactics (Hunter) | Delivered (rules 0.6.210) |
| 8 | Ability Score Improvement | Delivered (rules 0.6.210) |
| 9 | Expertise, level-3 spells | Delivered (rules 0.6.211) |
| 10 | Tireless | Temporary Hit Points delivered (rules 0.6.211); the Exhaustion half has nothing to act on |
| 11 | Superior Hunter's Prey (Hunter) | Delivered (rules 0.6.211) |
| 12 | Ability Score Improvement | Delivered (rules 0.6.211) |
| 13 | Relentless Hunter, level-4 spells | Delivered (rules 0.6.212) |
| 14 | Nature's Veil | Delivered (rules 0.6.212) |
| 15 | Superior Hunter's Defense (Hunter) | Delivered (rules 0.6.212) |

## Levels 5–15

The Ranger advances to level 15, the game's cap
([LEVELS-1](SRD-DECISIONS.md#levels-1-2026-10-14-levels-510),
[LEVELS-2](SRD-DECISIONS.md#levels-2-2026-10-15-level-cap-15)).

- **Extra Attack** (5): two attacks with each Attack action.
- **Spellcasting:** four level-1 slots from level 5; two level-2 slots at 5
  and 6, three from 7. Prepared spells 6, 6, 7, 7 (SRD p. 58). The Ranger
  list gains its level-2 spells: Aid, Barkskin, Gust of Wind, Lesser
  Restoration, Magic Weapon, Protection from Poison, Silence and Spike Growth.
  Animal Messenger, Darkvision, Enhance Ability, Find Traps, Locate Animals or
  Plants, Locate Object and Pass without Trace are cut (DM-1, DM-3).
- **Favored Enemy:** three free Hunter's Marks per Long Rest from level 5,
  four from 9, five from 13.
- **Ensnaring Strike** from a level-2 slot: "Ensnaring Strike (level 2 slot)"
  in the Bonus Action list after a hit; its vines deal 2d6 at the start of
  each of the target's turns.
- **Roving** (6): Speed +10 feet without Heavy armor. Its Climb and Swim
  Speeds are cut (SCOPE-2).
- **Defensive Tactics** (Hunter 7): chosen in the level-up Training dropdown,
  and changed after any rest in the rest window's own section, as the Hunter's
  Prey is.
  - **Escape the Horde:** Opportunity Attacks against the Ranger have
    Disadvantage.
  - **Multiattack Defense:** a creature that hits the Ranger with an attack
    roll has Disadvantage on its other attack rolls against the Ranger until
    that turn ends.
- **Ability Score Improvement** (8 and 12): a feat or ability points.
- **Expertise** (9): two skills the Ranger is proficient in, chosen in the
  level-up Training dropdown ("Ranger Expertise"), as the Rogue's at 6.
- **Level-3 spells** (9): two level-3 slots at 9 and 10, three from 11;
  prepared spells 9, 9, 10, 10 at levels 9–12. The list gains Conjure
  Animals, Dispel Magic, Plant Growth, Protection from Energy, Revivify and
  Wind Wall (below). Daylight, Meld into Stone, Nondetection, Speak with
  Plants, Water Breathing and Water Walk are cut (LEVELS-1).
- **Tireless** (10): a Magic action gives the Ranger 1d8 + its Wisdom
  modifier (at least 1) Temporary Hit Points, as many times as its Wisdom
  modifier (at least once) per Long Rest ("Tireless" in the resources). The
  game has no Exhaustion, so the Short Rest's lowering of it does nothing.
- **Superior Hunter's Prey** (Hunter 11): once a turn, after the Ranger
  damages the creature its Hunter's Mark marks on its own turn, the A cycle
  offers **Superior Hunter's Prey** against each other enemy it can see within
  30 feet of that creature; the click deals the mark's 1d6 Force damage to it
  (LEVELS-2). The party AI uses it whenever it is offered.
- **Relentless Hunter** (13): damage never forces a Concentration save while
  the Ranger's Concentration holds Hunter's Mark; dropping to 0 Hit Points
  still ends it.
- **Level-4 spells** (13): one level-4 slot at 13 and 14, two at 15;
  prepared spells 11, 11, 12 at levels 13–15; five free Hunter's Marks from
  13. The list gains Conjure Woodland Beings, Dominate Beast, Freedom of
  Movement and Stoneskin (below); Locate Creature is cut (LEVELS-1).
- **Nature's Veil** (14): a Bonus Action makes the Ranger Invisible until the
  end of its next turn, as many times as its Wisdom modifier (at least once)
  per Long Rest ("Nature's Veil" in the Bonus Action list and resources).
  Attacking does not end it.
- **Superior Hunter's Defense** (Hunter 15): when an attack hits the Ranger,
  the React/Decline prompt offers it (LEVELS-2); the Reaction gives
  Resistance to the hit's damage type, this hit included, until the turn
  ends. Like Uncanny Dodge it answers attack hits only.
- Verification: `opengold_ranger_levels_tests` (the campaign's stop at 15,
  features by level, prepared counts 5 at level 4 to 12 at 15, the level-2
  to level-4 spells preparable, slots, Favored Enemy's count, Roving, Extra
  Attack, Escape the Horde on an Opportunity Attack, Multiattack Defense
  after a hit and not after a miss, the rest swap, Ensnaring Strike from a
  level-2 slot and its checkpoint, a campaign round trip, Expertise at 9,
  Tireless, Superior Hunter's Prey's targets and checkpoint, Relentless
  Hunter against a Concentration save at 12, Nature's Veil's Disadvantage,
  Superior Hunter's Defense's prompt, and each level-3 and level-4 spell
  below).

### The level-3 spells

- **Protection from Energy** (Action, touch, Concentration up to 1 hour): the
  A cycle offers one entry per type, Acid, Cold, Fire, Lightning or Thunder;
  the creature resists it.
- **Conjure Animals** (Action, 60 feet, Concentration up to 10 minutes): aimed
  like Entangle, a Large pack of spirits fills two squares by two. Each enemy
  the Ranger can see makes a Dexterity save the first time in a turn it comes
  within 10 feet of the pack (moving there, or the pack moving near it) or
  ends its turn there, taking 3d10 Slashing damage on a failure (1d10 more for
  each slot level above 3). The Ranger has Advantage on Strength saves within
  5 feet of its pack. After the Ranger moves on its turn, **Move spirit pack**
  in the A cycle moves the pack up to 30 feet; adaptation: as with Moonbeam,
  the click picks a creature, and the pack goes to the free space it can
  reach nearest that creature.
- **Wind Wall** (Action, 120 feet, Concentration up to 1 minute): a straight
  wall 50 feet long through the aimed square; the A cycle offers it
  horizontal, vertical or along either diagonal (LEVELS-3). Each creature in
  it makes a Strength save against 4d8 Bludgeoning damage, half on a success.
  Ranged weapon attacks shot across the wall miss ("The wind deflects the
  shot."). Its wall is drawn on the map. Small flying creatures and gases are
  not modeled.
- **Plant Growth** (Action, 150 feet, Overgrowth only): every square with
  ordinary plants within 100 feet of the aimed square becomes overgrown for
  the rest of the fight: moving into one costs 20 feet (4 feet for each foot).
  A square has plants when the original's map square it comes from is open to
  the sky ([outdoor areas](OUTDOOR-AREAS.md), LEVELS-1); indoors the spell
  does nothing. The caster cannot exclude areas. The Scoured Lands' barren
  ground, which grows plants only after Yarash is defeated, is not modeled.
  Enrichment has nothing to act on.
- Combat checkpoints became `OGCOMBAT 56` (the battlefield's plants, each
  zone's slot level, Superior Hunter's Prey's state, a dominated creature's
  side).

### The level-4 spells

- **Stoneskin** (Action, touch, Concentration up to 1 hour): Resistance to
  Bludgeoning, Piercing and Slashing damage.
- **Conjure Woodland Beings** (Action, Concentration up to 10 minutes):
  spirits fill a 10-foot Emanation around the Ranger, drawn on the map. Each
  enemy the Ranger can see makes a Wisdom save the first time in a turn the
  Emanation reaches it (the Ranger moving near it, or it moving into the
  Emanation) or it ends its turn there, taking 5d8 Force damage, half on a
  success (1d8 more for each slot level above 4). While it lasts, the Bonus
  Action list offers **Disengage (Conjure Woodland Beings)**.
- **Dominate Beast** (Action, 60 feet, Concentration up to 1 minute): offered
  only against a Beast. On a failed Wisdom save (with Advantage, since the
  party is fighting it) the creature is Dominated: it fights on the party's
  side and takes its turns right after the Ranger, under the player's
  control ([LEVELS-1](SRD-DECISIONS.md#levels-1-2026-10-14-levels-510)'s
  shared system for summoned and controlled creatures). One that has already
  acted in the round it was dominated waits for the next round. Whenever it
  takes damage it saves again, and a success ends the spell. It makes an
  Opportunity Attack only with the Ranger's Reaction too, spending both. It
  still counts as an enemy for the fight's outcome: the fight is not won
  while it stands. When the spell ends it returns to its own side.

## Spellcasting

- Wisdom is the spellcasting ability. Level-one slots: 2, 2, 3, 3 at Ranger
  levels 1–4.
- Prepared spells: 2, 3, 4, 5 at levels 1–4, chosen from the Ranger list at
  creation (Spell Choices step), added at each level-up with earlier choices
  locked, and after a Long Rest one prepared spell may be replaced, as for a
  Paladin.
- The Ranger list in the game holds Cure Wounds, Detect Magic, Ensnaring Strike,
  Entangle, Fog Cloud, Goodberry, Hunter's Mark and Longstrider: every
  CRPG-relevant level-one Ranger spell.

## Favored Enemy and Hunter's Mark

- Hunter's Mark is always prepared and not counted. Favored Enemy casts it twice
  without a slot; a Long Rest restores both uses ("Favored Enemy" in the rest
  and combat resources). The count is stored where Paladin's Smite keeps its own:
  a character has only one of the two while multiclassing is deferred.
- **Hunter's Mark** (Bonus Action, 90 feet, Concentration up to 1 hour):
  every attack-roll hit by the caster on the marked creature deals an extra 1d6
  Force damage, doubled on a critical hit. When the marked creature drops to 0
  Hit Points, **Move Hunter's Mark** (a Bonus Action, no slot) marks another
  creature the caster can see within range. Its Perception and Survival
  Advantage has no use in the game.
- In the game: A cycles to **Hunter's Mark (Favored Enemy)**, **Hunter's Mark**
  (from a slot) or **Move Hunter's Mark**, then click the enemy.
- Adaptation: the free cast is not offered while a living creature still carries
  the Ranger's mark. The party AI marks the first enemy it can, and moves the
  mark when its quarry drops.

## Druidic Warrior

- At level two the Fighting Style dropdown also offers **Druidic Warrior**
  instead of a Fighting Style feat: two Druid cantrips, cast with Wisdom, chosen
  on the level-up Spell Choices page ("Druidic Warrior cantrips"). One may be
  replaced with another Druid cantrip on each later Ranger level
  ([Replacing choices](LEVEL-SWAPS.md)). It works like the Paladin's
  [Blessed Warrior](PALADIN.md#blessed-warrior), through the same level-up page,
  whose Godot check covers both.
- Verification: `opengold_spell_access_tests` (the offer, Druid cantrips only, a
  Cleric cantrip refused atomically, the profile, save round trip).

## Hunter (level 3)

- The Hunter is the SRD's only Ranger subclass, taken automatically at level
  three. **Hunter's Prey** is chosen in the level-up Training dropdown:
  - **Colossus Slayer**: once per turn, a weapon hit deals 1d8 more damage
    (doubled on a critical hit) to a creature already missing Hit Points.
  - **Horde Breaker**: once per turn, after a weapon attack, the A cycle offers
    **Horde Breaker** against a different creature within 5 feet of the first
    target and within the weapon's reach (or range). Adaptation: only the
    turn's first weapon target is excluded, not every creature attacked. The
    party AI uses it whenever it is offered.
- **Hunter's Lore**: marking a creature with Hunter's Mark logs its damage
  Immunities, Resistances and Vulnerabilities (or that it has none).
- After a Short or Long Rest the rest window offers the **Hunter's Prey** as
  its own section, picked like a radio button: Keep current or Apply training
  (rules 0.6.175). After a Long Rest the window shows Weapon Mastery first,
  then the Hunter's Prey; after a Short Rest only the Hunter's Prey. A choice
  left as it was keeps its picks. Verification: `opengold_ranger_spell_tests`
  (`hunters_prey_rest_checks`, including a save with the choice still open).
- Combat checkpoints became `OGCOMBAT 33` (each actor's once-per-turn Hunter's
  Prey state and Horde Breaker's first target).
- Verification: `opengold_ranger_spell_tests` (the level-three choice and an
  unknown option refused, Colossus Slayer against unwounded and wounded
  creatures, Horde Breaker's target rules, once per turn and checkpoint,
  Hunter's Lore with and without the subclass) and `opengold_godot_hunters_mark`
  (attack, then Horde Breaker on the second enemy).

## Ensnaring Strike and Restrained

- **Ensnaring Strike** (Bonus Action right after a weapon hit, Melee or Ranged;
  Concentration up to 1 minute): it appears in the Bonus Action list with the
  smites. The creature hit makes a Strength save (Advantage if Large or larger)
  or is **Restrained**; it takes 1d6 Piercing damage at the start of each of its
  turns. A successful save ends the spell; the slot is spent either way.
- **Escape the vines**: the Restrained creature may spend its Action on a
  Strength (Athletics) check against the spell's DC (a monster uses its
  Strength save bonus); success ends the spell and the caster's Concentration.
  Adaptation: only the creature itself may try, not a creature beside it. The
  AI tries to escape whenever it is caught.
- **Restrained** (shown as a condition): Speed 0, attacks against it have
  Advantage, its attacks have Disadvantage, and its Dexterity saves have
  Disadvantage.
- The smite window now opens on any weapon hit on the caster's own turn and
  records whether it was a Melee hit; Divine and Searing Smite still need one.
  Combat checkpoints became `OGCOMBAT 34`.
- From a level-two slot its vines deal 2d6 (see [Levels 5–15](#levels-515)).
- Verification: `opengold_ranger_spell_tests` (offered only after a hit, slot
  and Bonus Action, Restrained, checkpoint, turn damage and Speed 0,
  Disadvantage and Advantage, escaping, a longbow hit opening the window) and
  `opengold_godot_ensnaring` (Bonus Action list and Use, English and Spanish).

## Entangle and aiming area spells

- Area spells are aimed as recorded in
  [CLASS-5](SRD-DECISIONS.md#class-5-2026-10-05-aiming-area-spells): choose the
  spell in the A cycle, then a left click on a square (or Space or Enter) starts
  aiming; a left click or the arrow keys move the highlighted area, a right
  click, Space, Enter or **Cast spell** casts it there, and Escape cancels
  without spending anything. The preview starts on the nearest enemy in range.
  An open aim is kept in combat checkpoints.
- **Entangle** (Action, 90 feet, Concentration up to 1 minute): a 20-foot square
  (4 by 4 squares, clipped to the battlefield) becomes Difficult Terrain, drawn
  as such on the map. Each creature in it when it is cast, other than the
  caster, makes a Strength save or is Restrained until the spell ends. A
  Restrained creature may use **Escape the vines** (an Athletics check) to free
  itself; the spell goes on for the others. The plants vanish when the caster's
  Concentration ends.
- Combat checkpoints became `OGCOMBAT 35` (the open aim and spell zones).
- Verification: `opengold_ranger_spell_tests` (one aimed offer, the preview,
  moving and cancelling, a checkpoint, casting, the caster spared, Difficult
  Terrain, Concentration ending the plants) and `opengold_godot_entangle`
  (Space, arrows and Escape; left click to aim and right click to cast; English
  and Spanish).

## Fog Cloud

- **Fog Cloud** (Action, 120 feet, Concentration up to 1 hour), aimed like
  Entangle: a 20-foot-radius sphere of squares, 20 feet more from a level-two
  slot, is Heavily Obscured and drawn grey on the map. Sight into and out of it
  is blocked: spells that need sight (such as Hunter's Mark) and Opportunity
  Attacks cannot cross it, and an attack into, out of or within the fog is made
  as if neither creature could see the other, so Advantage and Disadvantage
  cancel. The fog ends with the caster's Concentration. Combat checkpoints
  become `OGCOMBAT 36`.
- Verification: `opengold_ranger_spell_tests` (aiming and casting, the sphere,
  sight blocked, cancelling modifiers, a checkpoint).

## Goodberry

- Cast from the Camp dialog's Cast / Use row only, never in combat
  ([CLASS-6](SRD-DECISIONS.md#class-6-2026-10-05-goodberry)): the ten berries are
  eaten at once and the chosen member regains up to 10 Hit Points. It spends a
  level-one slot; a higher slot adds nothing. Verified by
  `opengold_camp_action_tests`.

## Longstrider

- Action, touch, 1 hour, no Concentration: the creature's Speed increases by 10
  feet at once (for a Dash too). A level-two slot touches one more creature,
  chosen as for Bless ([CLASS-2](SRD-DECISIONS.md#class-2-2026-10-04-choosing-several-targets)).

Verification: `opengold_ranger_spell_tests` (free and slot casts, the mark's
damage, a checkpoint, no free recast while the quarry stands, moving the mark,
Longstrider's Speed), `opengold_spell_access_tests` (Wisdom casting, preparation
counts, Hunter's Mark always prepared and not choosable, slots, Favored Enemy's
pool, the one-spell Long Rest window, save round trip),
`opengold_spell_table_tests` (Ranger probes) and `opengold_godot_hunters_mark`
(A cycle and click, English and Spanish).
