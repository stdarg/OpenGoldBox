# Paladin

Milestone [#8](https://github.com/stdarg/OpenGold/issues/8), delivered class by
class ([CLASS-1](SRD-DECISIONS.md#class-1-2026-10-04-finishing-levels-14-for-all-classes)).
SRD 5.2.1 pp. 52–56. This page grows with each Paladin increment.

| Level | Feature | Status |
| --- | --- | --- |
| 1 | Spellcasting | Delivered (rules 0.6.65) |
| 1 | Weapon Mastery | Delivered earlier ([Weapon Mastery](WEAPON-MASTERY.md)) |
| 1 | Lay On Hands | Not yet |
| 2 | Fighting Style | Delivered earlier ([Fighting Style routes](FIGHTING-STYLE-ROUTES.md)); Blessed Warrior not yet |
| 2 | Paladin's Smite | Not yet |
| 3 | Channel Divinity, Oath of Devotion | Not yet |
| 4 | Ability Score Improvement | Delivered earlier |

## Spellcasting

- Charisma is the spellcasting ability. Level-one slots: 2, 2, 3, 3 at Paladin
  levels 1–4; no level-two slots before level 5.
- Prepared spells: 2, 3, 4, 5 at levels 1–4, chosen from the Paladin list at
  creation (in the Spell Choices step, which now appears for Paladins) and added
  at each level-up, with earlier choices locked.
- After a Long Rest a Paladin may replace **one** prepared spell (a Cleric may
  replace any). The shared rest window enforces this.
- The Paladin list in the game currently holds only Cure Wounds; Bless, Command,
  Divine Favor, Heroism, Protection from Evil and Good, Searing Smite and Shield
  of Faith arrive in later increments, and the other places stay pending.
  Detect Magic waits for spellcasting outside combat.

Cleric and Paladin share one table of class-list casters in
`spell_access.cpp` (`PreparedCaster`): cantrip and prepared counts, highest
slot level and the rest rule per class level.

Verification: `opengold_spell_access_tests` (`tests/paladin_choices_checks.h`)
checks counts, eligible and rejected preparations, Charisma, the Spell Choices
step, slots and prepared spells through level 4, locked level-up preparation
and a save round trip. `tests/training_view_tests.gd` confirms a Paladin reaches
Spell Choices after Training. The one-swap rest rule is tested once a second
Paladin spell exists.
