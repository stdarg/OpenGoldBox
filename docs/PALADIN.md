# Paladin

Milestone [#8](https://github.com/stdarg/OpenGold/issues/8), delivered class by
class ([CLASS-1](SRD-DECISIONS.md#class-1-2026-10-04-finishing-levels-14-for-all-classes)).
SRD 5.2.1 pp. 52–56. This page grows with each Paladin increment.

| Level | Feature | Status |
| --- | --- | --- |
| 1 | Spellcasting | Delivered (rules 0.6.65) |
| 1 | Weapon Mastery | Delivered earlier ([Weapon Mastery](WEAPON-MASTERY.md)) |
| 1 | Lay On Hands | Delivered in combat (rules 0.6.66) and in camp (rules 0.6.74); curing Poisoned not yet |
| 2 | Fighting Style | Delivered earlier ([Fighting Style routes](FIGHTING-STYLE-ROUTES.md)); Blessed Warrior delivered (rules 0.6.72) |
| 2 | Paladin's Smite | Delivered with Divine Smite and Searing Smite (rules 0.6.69) |
| 3 | Channel Divinity, Oath of Devotion | Delivered (rules 0.6.73): Sacred Weapon and the oath spells; Divine Sense left out (CLASS-4) |
| 4 | Ability Score Improvement | Delivered earlier |

## Channel Divinity and the Oath of Devotion

- Level three grants Channel Divinity (two uses; a Short Rest restores one, a
  Long Rest all) and the Oath of Devotion, the SRD's only Paladin oath, so it is
  taken automatically.
- **Sacred Weapon** spends one use. It comes with the Attack action, so it is
  offered in the A cycle (Space uses it) while the Action is unspent, and costs
  no Action. For 10 minutes the Paladin adds its Charisma modifier (at least +1)
  to attack rolls with Melee weapons, and the weapon deals Radiant damage
  whenever the target resists its own damage type more than Radiant (the SRD
  leaves the choice to the Paladin; this takes the better one). It ends when
  the Paladin drops to 0 Hit Points. Its light is not modeled. The party AI uses
  it before its first melee attack.
- Divine Sense is left out: every creature in combat is already visible
  ([CLASS-4](SRD-DECISIONS.md#class-4-2026-10-05-divine-sense)).
- The oath spells, Protection from Evil and Good and Shield of Faith, are always
  prepared from level three and not counted. A spell that becomes always
  prepared (these, or Divine Smite at level two) frees its earlier place on the
  level-up, and can no longer be chosen as a prepared spell.

Verification: `opengold_paladin_spell_tests` (Sacred Weapon's cost, its attack
bonus, Radiant damage against a target resisting Slashing, a checkpoint, no
Sacred Weapon below level three), `opengold_spell_access_tests` (oath spells
freeing their place, a doubled preparation refused, Channel Divinity's two uses
and Short Rest recovery) and `opengold_godot_sacred_weapon` (A cycle and Space,
English and Spanish).

## Blessed Warrior

- At level two the Fighting Style dropdown also offers **Blessed Warrior**
  instead of a Fighting Style feat. The Paladin learns two Cleric cantrips,
  cast with Charisma, chosen on the level-up Spell Choices page ("Blessed
  Warrior cantrips"). Sacred Flame is the only Cleric cantrip in the game so
  far, so the second choice stays pending until another arrives; replacing one
  on later Paladin levels waits for it too.
- Changing the style on the first page drops the cantrip picks with it.
- A new rules call, `spell_choice_sheet`, gives the level-up page the sheet
  with the chosen style, so the page offers exactly what the level-up applies.

Verification: `opengold_spell_access_tests` (the option, two Cleric cantrips
only with Blessed Warrior, a non-Cleric cantrip refused atomically, Sacred
Flame in the profile and offered in combat, save round trip). With
`OPENGOLD_GAME_DIR` it writes `blessed-warrior-ui.ogs` for
`tests/blessed_warrior_view_tests.gd` (English and Spanish: choose Blessed
Warrior, learn Sacred Flame by keyboard, switch to Defense and back, confirm,
save). Run it like the Cleric check:

```bash
OPENGOLD_GAME_DIR=/path/to/POOLRAD cmake -DGODOT=godot -DPROJECT=$PWD/src/OpenGoldBox/godot \
    -DSCRIPT=$PWD/tests/blessed_warrior_view_tests.gd \
    "-DEXPECTED=Blessed Warrior view checks passed" \
    "-DARGS=--blessed-fixture=$PWD/build/blessed-warrior-ui.ogs" -P tests/run_godot_test.cmake
```

## Spellcasting

- Charisma is the spellcasting ability. Level-one slots: 2, 2, 3, 3 at Paladin
  levels 1–4; no level-two slots before level 5.
- Prepared spells: 2, 3, 4, 5 at levels 1–4, chosen from the Paladin list at
  creation (in the Spell Choices step, which now appears for Paladins) and added
  at each level-up, with earlier choices locked.
- After a Long Rest a Paladin may replace **one** prepared spell (a Cleric may
  replace any). The shared rest window enforces this.
- The Paladin list in the game holds Bless, Command, Cure Wounds, Divine Favor,
  Divine Smite, Heroism, Protection from Evil and Good, Searing Smite and Shield
  of Faith. Detect Magic waits for spellcasting outside combat
  ([CLASS-3](SRD-DECISIONS.md#class-3-2026-10-05-holy-water-and-casting-outside-combat)).
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
- Outside combat it is used from the Camp dialog's **Cast / Use** row, like the
  healing spells ([Camp actions](REST-RESOURCES.md#cast--use-in-camp)).
- Not yet: spending 5 points to remove Poisoned, because the game has no
  Poisoned condition yet.
- The pool is saved in the character's vital state (`SRD11`) and in combat
  checkpoints (`OGCOMBAT 32`).

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
  (`SRD11`); the open smite chance is saved in combat checkpoints (`OGCOMBAT 32`).

Verification: `opengold_smite_tests` (the window after own-turn melee hits only,
closing on other commands, free and slot casts, the extra die against a Fiend,
Searing Smite's burn and save with failing and succeeding saves, checkpoints,
Long Rest recovery) and `opengold_godot_smite` (the Bonus Action list after a
hit and Use, in English and Spanish). Critical doubling is applied but not yet
forced in a test.

## Buffs and Concentration

- **Shield of Faith** (Bonus Action, 60 feet, Concentration up to 10 minutes):
  +2 AC. **Heroism** (Action, touch, Concentration up to 1 minute): Temporary
  HP equal to the caster's spellcasting modifier at the start of each of the
  target's turns, kept only when higher than what it has (Frightened is not yet
  modeled). **Divine Favor** (Bonus Action, self, 1 minute): weapon hits deal an
  extra 1d4 Radiant damage, doubled on a critical hit.
- Concentration: a caster holds one Concentration spell; casting another ends
  the first and removes its effects. Damage calls for a Constitution save
  against half the damage (10–30); dropping to 0 Hit Points ends it.
  Concentration is tracked in combat only and ends when the combat ends (an
  adaptation; outside combat no spell needs it yet). It is saved in combat
  checkpoints (`OGCOMBAT 32`).
- **Bless** (Action, 30 feet, Concentration up to 1 minute; Cleric and
  Paladin): up to three creatures, four from a level-two slot, add 1d4 to their
  attack rolls and saving throws, Concentration saves and repeated saves
  included. Choosing creatures follows CLASS-2: the first choice starts it and
  spends nothing; each click on a creature adds or removes it; the spell is cast
  when the third is chosen, or earlier with **Cast spell** (the End button) or
  Space; Escape cancels. An open choice is saved in combat checkpoints
  (`OGCOMBAT 32`).
- **Protection from Evil and Good** (Action, touch, Concentration up to 10
  minutes; Cleric and Paladin): Aberrations, Celestials, Elementals, Fey, Fiends
  and Undead attack the warded creature with Disadvantage. The spell needs no
  holy water (CLASS-3). Its protection against being Charmed, Frightened or
  possessed waits until a creature can cause those. In the current content the
  Slums goblins and bugbears (Fey) are affected.
- **Command** (Action, 60 feet, Verbal only; Cleric and Paladin): a creature
  the caster can see makes a Wisdom save or obeys on its next turn. Each option
  is its own entry in the A cycle (Command: Approach, Flee, Grovel, Halt).
  Approach moves it to the reachable cell nearest the caster and ends its turn
  within 5 feet; Flee moves it to the reachable cell farthest away, Dashes and
  moves on; Grovel makes it Prone and ends its turn; Halt ends its turn without
  moving or acting (it keeps its Reaction). Drop is left out: the game has no
  dropped gear. A level-two slot commands two creatures, chosen as for Bless
  (CLASS-2), each saving separately. A newer Command replaces an older one.
- In the game: A cycles to the spell, then click the ally (a click on an ally
  now targets it whenever the selected action can) or press Space.

Verification: `opengold_godot_bless` chooses, cancels and casts Bless through
the game controls in English and Spanish. `opengold_paladin_spell_tests` (Bless's
choice, cancel, early and automatic casts, and its attack bonus; AC, a second Concentration spell
ending the first, Temporary HP at the target's turn, Divine Favor's damage, the
Concentration save after damage, ending with the combat, and Protection from
Evil and Good's Disadvantage against a Fiend but not a Humanoid, and each
Command option, its checkpoint, and a level-two Command on two creatures).
`opengold_godot_command` casts Command: Grovel from the A cycle in English and
Spanish and the shared
spell-table checks, which now also probe Paladins.

Cleric and Paladin share one table of class-list casters in
`spell_access.cpp` (`PreparedCaster`): cantrip and prepared counts, highest
slot level and the rest rule per class level.

Verification: `opengold_spell_access_tests` (`tests/paladin_choices_checks.h`)
checks counts, eligible and rejected preparations, Charisma, the Spell Choices
step, slots and prepared spells through level 4, locked level-up preparation
and a save round trip. `tests/training_view_tests.gd` confirms a Paladin reaches
Spell Choices after Training. The one-swap rest rule is tested once a second
Paladin spell exists.
