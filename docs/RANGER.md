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
| 2 | Fighting Style | Delivered earlier ([Fighting Style routes](FIGHTING-STYLE-ROUTES.md)); Druidic Warrior not yet |
| 3 | Ranger Subclass (Hunter) | Not yet |
| 4 | Ability Score Improvement | Delivered earlier |

## Spellcasting

- Wisdom is the spellcasting ability. Level-one slots: 2, 2, 3, 3 at Ranger
  levels 1–4.
- Prepared spells: 2, 3, 4, 4 at levels 1–4, chosen from the Ranger list at
  creation (Spell Choices step), added at each level-up with earlier choices
  locked, and after a Long Rest one prepared spell may be replaced, as for a
  Paladin.
- The Ranger list in the game holds Cure Wounds, Hunter's Mark and Longstrider
  so far. Ensnaring Strike, Entangle, Fog Cloud and Goodberry come next; Detect
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
