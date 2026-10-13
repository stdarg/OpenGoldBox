# Fire Bolt through level 4

Fire Bolt is complete through character level 4 (2026-10-13,
[#166](https://github.com/stdarg/OpenGold/issues/166)). Authority:
[SRD 5.2.1 p. 132](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=132).

## The spell

An Action: a ranged spell attack against a creature within 120 feet that the
caster has a clear line to. A hit deals 1d10 Fire damage; a critical hit
doubles the die. Fire resistance, vulnerability and immunity apply. Like the
other attack cantrips, it may target any creature, an ally or the caster
included (rules 0.6.160; it used to target enemies only). A Sorcerer's
Quickened Spell casts it as a Bonus Action.

Not in the game: objects as targets and setting objects alight, both dropped
([SCOPE-2](SRD-DECISIONS.md#scope-2-2026-09-30-exploration-halves-and-marginal-features)).
Spells have no components
([CLASS-11](SRD-DECISIONS.md#class-11-2026-10-06-no-spell-components)). The
damage grows at character levels 5, 11 and 17
([#176](https://github.com/stdarg/OpenGold/issues/176)–178).

## Routes

| Route | Ability | Evidence |
| --- | --- | --- |
| Wizard | Intelligence | `opengold_rules_tests`, `opengold_spell_access_tests` |
| Sorcerer | Charisma | `opengold_sorcerer_cantrip_tests`, `opengold_sorcerer_tests` |
| Arid Land Druid, level 3, always prepared | Wisdom | `opengold_druid_tests` |
| Magic Initiate (Wizard list) | The feat's ability | `opengold_magic_initiate_tests` |

Still waiting on content the game does not have yet: High Elf
([#67](https://github.com/stdarg/OpenGold/issues/67)), Infernal Tiefling
([#73](https://github.com/stdarg/OpenGold/issues/73)) and Pact of the Tome
([#161](https://github.com/stdarg/OpenGold/issues/161)).

## Verification

Typed Fire damage and defenses: `opengold_damage_tests`. Targeting the caster
and enemies, with no slot spent: `opengold_spell_access_tests`. Obstacles that
block the line: `opengold_rules_tests`. The offered targets for every class and
level: the spell-offer baseline in `opengold_spell_table_tests`.
