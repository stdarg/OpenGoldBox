# Spell components

**Spells have no components** ([CLASS-11](SRD-DECISIONS.md#class-11-2026-10-06-no-spell-components)).
Verbal, Somatic and Material components and spellcasting focuses are not part
of the rules. A caster with a weapon, wand or shield in hand casts any spell it
can otherwise cast; standing in Silence does not stop a spell; no material
needs to be carried, paid for or consumed.

What still limits casting:

- Untrained armor prevents all spellcasting.
- A raging Barbarian casts no spells, and a Druid in Wild Shape casts none.
- Slots, prepared spells, the action economy, range and sight work as before.

Consequences:

- **Silence** still makes Thunder damage harmless inside its sphere and is
  still shown on the battlefield, but it no longer stops smites, Hunter's
  Mark, Armor of Shadows, Shield, Spiritual Weapon's attack or any other spell.
- **Subtle Spell** is no longer offered as a Metamagic option, since it only
  removed components. A Sorcerer who chose it before keeps a valid save; the
  option does nothing and cannot be readied.
- The character sheet no longer notes that a weapon and shield occupy both
  hands.

## History

From rules 0.6.21 ([#201](https://github.com/stdarg/OpenGold/issues/201)) to
0.6.128 each spell recorded its Verbal and Somatic components: a weapon or wand
together with a shield blocked Somatic spells, and Silence blocked Verbal ones.
Material components were never enforced. Rules 0.6.129 removes all of it. No
save data changes: campaign and combat saves load and play under the new rule.

## Verification

`opengold_spell_component_tests` checks that every combination of weapon, wand
and shield leaves every live spell command available with its normal costs,
that untrained armor still blocks casting, and that a Cleric with mace and
shield casts in a campaign encounter. `opengold_spell_table_tests` checks every
spell row with a quarterstaff and shield in hand. `opengold_cleric_channel_tests`
casts Sacred Flame from inside Silence. `tests/spell_component_view_tests.gd`
checks that the game's spell controls and keyboard cycle offer casting with
full hands.
