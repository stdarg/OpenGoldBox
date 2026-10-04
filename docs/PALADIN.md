# Paladin

Milestone [#8](https://github.com/stdarg/OpenGold/issues/8), delivered class by
class ([CLASS-1](SRD-DECISIONS.md#class-1-2026-10-04-finishing-levels-14-for-all-classes)).
SRD 5.2.1 pp. 52–56. This page grows with each Paladin increment.

| Level | Feature | Status |
| --- | --- | --- |
| 1 | Spellcasting | Delivered (rules 0.6.65) |
| 1 | Weapon Mastery | Delivered earlier ([Weapon Mastery](WEAPON-MASTERY.md)) |
| 1 | Lay On Hands | Delivered in combat (rules 0.6.66); outside combat and curing Poisoned not yet |
| 2 | Fighting Style | Delivered earlier ([Fighting Style routes](FIGHTING-STYLE-ROUTES.md)); Blessed Warrior not yet |
| 2 | Paladin's Smite | Delivered with Divine Smite and Searing Smite (rules 0.6.67) |
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
- The Paladin list in the game holds Cure Wounds, Divine Smite and Searing
  Smite; Bless, Command, Divine Favor, Heroism, Protection from Evil and Good and
  Shield of Faith arrive in later increments, and the other places stay pending.
- Spells a feature keeps prepared (Divine Smite from level 2) are listed apart
  and not counted against the prepared spells.
  Detect Magic waits for spellcasting outside combat.

## Lay On Hands

- A pool of five Hit Points per Paladin level, refilled by a Long Rest (not a
  Short Rest) and growing by five at each level-up.
- In combat, as a Bonus Action, the Paladin touches itself or an ally within 5
  feet whose healing can take effect, including one at 0 Hit Points. It restores
  what the target is missing, up to the pool, and the pool loses only the Hit
  Points actually restored. (The SRD lets the player choose a smaller amount;
  healing exactly what is missing is the adaptation used here.)
- In the game it is the **Lay On Hands** entry of the Bonus Action list: choose
  it, press **Use Bonus Action**, then click the ally. The automatic combat
  policy uses it, before any healing spell, on an ally below half Hit Points.
- Not yet: using it outside combat (that needs a control outside combat, asked
  together with Detect Magic), and spending 5 points to remove Poisoned, because
  the game has no Poisoned condition yet.
- The pool is saved in the character's vital state (`SRD11`) and in combat
  checkpoints (`OGCOMBAT 30`).

Verification: `opengold_lay_on_hands_tests` (offers, healing amount, spending,
dying ally, checkpoint, rest and level growth, campaign round trip) and
`opengold_godot_lay_on_hands` (the Bonus Action list, Use and a click on the
ally, in English and Spanish).

## Smites

- Divine Smite and Searing Smite are cast as a Bonus Action immediately after
  the Paladin's own hit with a Melee weapon or an Unarmed Strike, against the
  creature hit, while it still stands. Any other command, or the next turn, ends
  the chance; resolving that hit's weapon-mastery choice does not.
- Divine Smite: 2d8 Radiant damage, 3d8 against a Fiend or an Undead. Searing
  Smite: 1d6 Fire damage, then at the start of each of the target's turns 1d6
  Fire damage and a Constitution save against the Paladin's spell save DC; a
  success ends it, and it lasts at most 1 minute. The damage is part of the
  attack, so a critical hit doubles the smite's dice. A slot cast spends one
  level-one slot (one slot per turn, as for every spell).
- Paladin's Smite (level 2): Divine Smite is always prepared and can be cast
  once without a slot, restored by a Long Rest.
- In the game the smites appear in the Bonus Action list right after the hit:
  "Divine Smite (Paladin's Smite)", "Divine Smite" and "Searing Smite"; Use
  casts it on the creature just hit. The automatic combat policy uses the free
  smite at its first chance and keeps slots for healing.
- Monsters now carry an SRD creature type in the rules content (`type` rows):
  the Slums kobolds are Dragons, goblins and bugbears Fey, and the rest
  Humanoid. Characters are Humanoid.
- The free use and Channel Divinity (not yet used) are saved in the vital state
  (`SRD11`); the open smite chance is saved in combat checkpoints (`OGCOMBAT 30`).

Verification: `opengold_smite_tests` (the window after own-turn melee hits only,
closing on other commands, free and slot casts, the extra die against a Fiend,
Searing Smite's burn and save with failing and succeeding saves, checkpoints,
Long Rest recovery) and `opengold_godot_smite` (the Bonus Action list after a
hit and Use, in English and Spanish). Critical doubling is applied but not yet
forced in a test.

Cleric and Paladin share one table of class-list casters in
`spell_access.cpp` (`PreparedCaster`): cantrip and prepared counts, highest
slot level and the rest rule per class level.

Verification: `opengold_spell_access_tests` (`tests/paladin_choices_checks.h`)
checks counts, eligible and rejected preparations, Charisma, the Spell Choices
step, slots and prepared spells through level 4, locked level-up preparation
and a save round trip. `tests/training_view_tests.gd` confirms a Paladin reaches
Spell Choices after Training. The one-swap rest rule is tested once a second
Paladin spell exists.
