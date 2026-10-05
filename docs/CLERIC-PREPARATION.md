# Cleric preparation and Divine Order

Issue [#91](https://github.com/stdarg/OpenGold/issues/91), decided in
[CLERIC-1](SRD-DECISIONS.md#cleric-1-2026-10-02-cleric-preparation-and-divine-order).
Rules module 0.6.63. SRD 5.2.1 Cleric, levels 1–4; counts as recorded in the
[rules audit](audits/srd-5.2.1-rules.md).

## Rules

| Cleric level | Cantrips | Prepared spells | Highest slot |
| --- | --- | --- | --- |
| 1 | 3 (4 for a Thaumaturge) | 4 | 1 |
| 2 | 3 (4) | 5 | 1 |
| 3 | 3 (4) | 6 | 2 |
| 4 | 4 (5) | 7 | 2 |

- **Preparation** is from the whole Cleric list, any spell of a level for
  which the Cleric has slots. There is no spellbook. The implemented Cleric
  spells are Bless, Command, Cure Wounds, Healing Word, Inflict Wounds,
  Protection from Evil and Good and Shield of Faith (level 1) and
  Blindness/Deafness's blindness option (level 2). Places beyond those stay
  pending, so a Cleric prepares every implemented spell it can.
- **Changing preparation:** creation chooses the first list. Gaining a level
  keeps the earlier list and adds to it. After a completed Long Rest the whole
  list may be changed, once per rest, through the shared spell window.
- **Cantrips:** Sacred Flame is the only implemented Cleric cantrip. The
  level-four cantrip is a learning group at level-up and stays pending.
  Replacing a cantrip on gaining a Cleric level waits for a second implemented
  cantrip; it is not offered at a Long Rest.
- **Divine Order** (level 1, required):
  - Protector: training with Martial weapons and Heavy armor. Weapon
    proficiency, armor penalties and the shield bonus read this grant.
  - Thaumaturge: one extra Cleric cantrip, and Wisdom modifier (minimum +1)
    added to Intelligence (Arcana or Religion) checks.
- A Cleric no longer knows Cure Wounds implicitly; it casts what it prepared.

## Interface

- **Training step:** a "Divine Order" dropdown, sorted first like Fighting Style.
- **Spell Choices step:** the Cleric cantrip group, then "Prepared spells
  (n / 4)". Next stays disabled until every available spell is prepared.
- **Level up:** a second page with the prepared group. Earlier preparations are
  locked; at level four the cantrip group appears, pending.
- **Long Rest:** the window is titled "Prepared spells", lists known cantrips
  and prepared spells, and hides the cantrip replacement row.
- The character sheet reports "Pending Cleric choices: {cantrips} cantrips,
  {prepared} prepared spells."

## Persistence

The Divine Order is a training grant; prepared spells are the character's
prepared list. Both replay through creation and advancement history, and the
character profile records the cantrips and prepared spells. Saves from 0.6.62
are rejected by the rules identity, as the
[pre-1.0 policy](SAVES.md#pre-10-format-policy) requires.

## Verification

- `opengold_spell_access_tests` (`tests/cleric_choices_checks.h`): counts,
  eligible spells, rejected preparations, no implicit Cure Wounds, Thaumaturge
  cantrip, required Divine Order, Protector training, Thaumaturge checks,
  locked level-up preparation and atomic rejection, level four pending choices,
  the Long Rest window, rejected rest replacement, and identical commits after
  a reload. With `OPENGOLD_GAME_DIR` it writes `cleric-choices-ui.ogs`.
- `tests/cleric_preparation_view_tests.gd` (English and Spanish): level two
  and three through the real level-up controls, locked earlier choices,
  keyboard preparation, Back, save and reload. Run it like the Wizard check:

  ```bash
  OPENGOLD_GAME_DIR=/path/to/POOLRAD cmake -DGODOT=godot -DPROJECT=src/OpenGoldBox/godot \
      -DSCRIPT=tests/cleric_preparation_view_tests.gd \
      "-DEXPECTED=Cleric choices view checks passed" \
      "-DARGS=--cleric-fixture=$PWD/build/cleric-choices-ui.ogs" -P tests/run_godot_test.cmake
  ```

- `opengold_godot_rest` (CTest) opens a Cleric's Long Rest window after the
  Wizard checks: "Prepared spells" title, no replacement row, Apply closes it.
- `tests/cleric_cantrip_view_tests.gd` and `tests/training_view_tests.gd`
  cover the Training dropdown and the Spell Choices step.

## Not covered

Channel Divinity, the level-three subclass and its always-prepared spells, and
the missing Cleric spells and cantrips are separate issues. Inflict Wounds is
described in [its own page](INFLICT-WOUNDS.md).
