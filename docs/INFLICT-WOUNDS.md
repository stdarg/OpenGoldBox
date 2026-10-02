# Inflict Wounds

Issue [#230](https://github.com/stdarg/OpenGold/issues/230). Cleric level-one
spell, SRD 5.2.1 p. 143. Rules module 0.6.64.

## Rules

- Action; Touch (an enemy within 5 feet); Verbal and Somatic components.
- The target makes a Constitution saving throw against the caster's spell save
  DC. It takes 2d10 Necrotic damage on a failure, or half the same roll,
  rounded down, on a success. Halving comes before resistance or vulnerability.
- From a level-two slot (`inflict_wounds_2`) the damage is 3d10.

The earlier row was the 2014 version: a melee spell attack for 3d10. The
generic `save_damage` pattern gains a `half_on_success` flag; Sacred Flame keeps
dealing nothing on a success.

## In the game

A Cleric with Inflict Wounds prepared reaches it like the other leveled spells:
A cycles to it, then click a highlighted enemy or press Space. The Spell
dropdown still lists cantrips only. The automatic combat policy, used for
enemies, checks and the balance tool, now casts it before a melee attack when
no ally needs healing.

## Verification

- `opengold_inflict_wounds_tests`: with a forced failing and succeeding save
  and the same dice, full and half damage on level-one and level-two slots, and
  Touch range.
- `opengold_godot_inflict`: in English and Spanish, the A cycle selects
  Inflict Wounds beside an enemy, a click casts it, and the spent Action ends
  further casting.
