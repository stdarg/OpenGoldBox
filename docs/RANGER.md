# Ranger

Milestone [#8](https://github.com/stdarg/OpenGold/issues/8), delivered class by
class ([CLASS-1](SRD-DECISIONS.md#class-1-2026-10-04-finishing-levels-14-for-all-classes)).
SRD 5.2.1 pp. 57–60. This page grows with each Ranger increment.

| Level | Feature | Status |
| --- | --- | --- |
| 1 | Spellcasting | Delivered (rules 0.6.75) |
| 1 | Favored Enemy | Delivered (rules 0.6.75) |
| 1 | Weapon Mastery | Delivered earlier ([Weapon Mastery](WEAPON-MASTERY.md)) |
| 2 | Deft Explorer | Nothing to deliver in levels 1–4 ([2026-09-30 simplification](SRD-DECISIONS.md#2026-09-30-simplification)) |
| 2 | Fighting Style | Delivered earlier ([Fighting Style routes](FIGHTING-STYLE-ROUTES.md)); Druidic Warrior delivered (rules 0.6.76) |
| 3 | Ranger Subclass (Hunter) | Delivered (rules 0.6.77); changing Hunter's Prey after a rest not yet |
| 4 | Ability Score Improvement | Delivered earlier |

## Spellcasting

- Wisdom is the spellcasting ability. Level-one slots: 2, 2, 3, 3 at Ranger
  levels 1–4.
- Prepared spells: 2, 3, 4, 4 at levels 1–4, chosen from the Ranger list at
  creation (Spell Choices step), added at each level-up with earlier choices
  locked, and after a Long Rest one prepared spell may be replaced, as for a
  Paladin.
- The Ranger list in the game holds Cure Wounds, Ensnaring Strike, Entangle, Fog
  Cloud, Goodberry, Hunter's Mark and Longstrider: every CRPG-relevant level-one
  Ranger spell except Detect Magic, which waits for magic items.

## Favored Enemy and Hunter's Mark

- Hunter's Mark is always prepared and not counted. Favored Enemy casts it twice
  without a slot; a Long Rest restores both uses ("Favored Enemy" in the rest
  and combat resources). The count is stored where Paladin's Smite keeps its own:
  a character has only one of the two while multiclassing is deferred.
- **Hunter's Mark** (Bonus Action, 90 feet, Verbal, Concentration up to 1 hour):
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
  on the level-up Spell Choices page ("Druidic Warrior cantrips"). Poison Spray
  is the only Druid cantrip in the game so far, so the second choice stays
  pending. It works like the Paladin's
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
- Not yet: replacing the Hunter's Prey option after a Short or Long Rest; the
  rest window replaces only Weapon Mastery so far.
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
- A level-two slot's extra die waits for Rangers of level five.
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
