# Cure Wounds and Healing Word

Both healing spells are complete through character level 4 (2026-10-13,
[#167](https://github.com/stdarg/OpenGold/issues/167) and
[#169](https://github.com/stdarg/OpenGold/issues/169)). Authority:
[SRD 5.2.1 pp. 121 and 139](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=121).

## The spells

| | Cure Wounds | Healing Word |
| --- | --- | --- |
| Casting | Action | Bonus Action |
| Range | Touch (an adjacent creature or the caster) | 60 feet, a creature the caster can see |
| Level-1 slot | 2d8 + casting ability modifier | 2d4 + casting ability modifier |
| Level-2 slot | 4d8 + modifier | 4d4 + modifier |

Healing raises an unconscious creature, which stays Prone, and is capped at the
creature's Hit Point maximum. Effects that prevent healing, such as Chill Touch,
stop it. Spells have no components
([CLASS-11](SRD-DECISIONS.md#class-11-2026-10-06-no-spell-components)), so full
hands, a shield or silence never stop a cast.

**Life Domain.** From level 3 a Life Cleric's Disciple of Life adds 2 + the
slot's level to either spell, and Cure Wounds is always prepared
([Cleric](CLERIC.md)).

**Out of combat.** Both are offered in the Camp dialog's Cast / Use list
([CLASS-3](SRD-DECISIONS.md#class-3-2026-10-05-holy-water-and-casting-outside-combat)).

## Routes

| Route | Cure Wounds | Healing Word | Ability |
| --- | --- | --- | --- |
| Cleric | yes | yes | Wisdom |
| Bard | yes | yes | Charisma |
| Druid | yes | yes | Wisdom |
| Paladin | yes | — | Charisma |
| Ranger | yes | — | Wisdom |
| Magic Initiate (Cleric or Druid list), slot or free cast | yes | yes | The feat's ability |

## Verification

`opengold_healing_spell_tests` casts each class route with two casters who
differ only in the casting ability: the healing differs by exactly the
modifiers' difference. It also checks Touch against the 60-foot reach, the
Action against the Bonus Action, the two extra dice of a level-2 slot, Disciple
of Life on Healing Word, and that a refused cast leaves the fight unchanged.
Other checks: Disciple of Life on Cure Wounds (`opengold_cleric_channel_tests`),
Magic Initiate's casts and camp healing (`opengold_magic_initiate_tests`,
`opengold_camp_action_tests`), the Paladin's and Ranger's casts
(`opengold_paladin_spell_tests`, `opengold_ranger_spell_tests`), and camp
healing kept across a save file (`opengold_save_tests`).

## Beyond level 4

Slots above level 2 arrive with character levels 5 and up
([#176](https://github.com/stdarg/OpenGold/issues/176)–178); each slot level
adds 2d8 or 2d4.
