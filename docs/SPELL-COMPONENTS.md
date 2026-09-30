# Spell components and occupied hands

[#201](https://github.com/stdarg/OpenGold/issues/201) delivers the first child of
[#39](https://github.com/stdarg/OpenGold/issues/39): explicit component definitions
for existing spells and Somatic hand eligibility. The parent remains open for
actual speech restrictions such as gagging and magical silence, including their
sources, persistence, removal and effects on casting. Recording a Verbal flag
alone does not implement those restrictions.

The source is [SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
p. 105 (components), p. 90 (Two-Handed/Versatile), and the named spell entries.

| Existing spell | Components |
| --- | --- |
| Fire Bolt | Verbal, Somatic |
| Cure Wounds | Verbal, Somatic |
| Magic Missile | Verbal, Somatic |
| Healing Word | Verbal |
| Scorching Ray | Verbal, Somatic |
| Blindness/Deafness (implemented Blindness option) | Verbal |

Higher-slot variants retain the same components. These six spells have no
Material components. Material/focus/cost/consumption mechanics remain #40.

An equipped weapon or wand together with a shield occupies both hands, making
Somatic spells unavailable. A shield alone, weapon alone or empty hands leaves
at least one hand usable. Two-Handed weapons require both hands when attacking;
Versatile damage likewise describes an attack grip. Casting can use one hand
while the other holds the weapon. It preserves the selected attack grip and
does not remove equipment, change shield AC or spend an extra action.

This uses equipment from actual character recipes for PCs and recruited NPCs.
The abstract standalone training creature profiles have no equipped weapon or
shield records; their attack dice are not treated as evidence of occupied hands.
Inventory-only items do not occupy hands. Untrained armor's separate casting
prohibition remains in force, including for Verbal-only spells.

The C++20 rules module filters legal commands before execution, so forged or
stale blocked casts cannot consume actions, slots, RNG, HP or game time. The
existing Godot game/demo action controls reflect those commands; the game's
keyboard action cycle excludes blocked spells. The existing Modifiers display
explains the weapon/wand-plus-shield restriction in English and Spanish. No new
controls or combat saving options are added.

## Persistence and verification

The hands rule adds no save data; loaded campaign and combat state uses the
corrected rule for subsequent spell eligibility.

`opengold_spell_component_tests` independently checks component flags, all live
spell commands and upcasts, weapon/wand/shield combinations, two-hand and
Versatile grips, normal casting costs, rejected-command atomicity, equipped
campaign members and unequipping, and repeated saves.

`tests/spell_component_view_tests.gd` uses normally created and advanced caster
fixtures to check the game and demo's existing spell controls and hidden internal
checkpoint continuation. The game additionally checks Blindness, unchanged grip,
the keyboard cycle and a Healing Word cast; the older demo lacks those grip and
Blindness controls.
CTest registers the game check without original assets. The demo can run the
same script with `--legacy` and the `--component-fixtures` directory written by
the native suite.

The remaining magical-silence path is split into concentration transitions #207,
Silence combat/area/player integration #208 and campaign/ritual integration #209.
See [concentration](CONCENTRATION.md). These do not imply gagging is supported.
