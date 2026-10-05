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
- The Ranger list in the game holds Cure Wounds, Goodberry, Hunter's Mark and
  Longstrider so far. Ensnaring Strike, Entangle and Fog Cloud come next; Detect
  Magic waits for magic items.

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
- Combat checkpoints become `OGCOMBAT 33` (each actor's once-per-turn Hunter's
  Prey state and Horde Breaker's first target).
- Verification: `opengold_ranger_spell_tests` (the level-three choice and an
  unknown option refused, Colossus Slayer against unwounded and wounded
  creatures, Horde Breaker's target rules, once per turn and checkpoint,
  Hunter's Lore with and without the subclass) and `opengold_godot_hunters_mark`
  (attack, then Horde Breaker on the second enemy).

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
