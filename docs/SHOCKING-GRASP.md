# Wizard Shocking Grasp through level four

Rules 0.6.38 delivers the ordinary Wizard path in
[#226](https://github.com/stdarg/OpenGold/issues/226), a child of
[#223](https://github.com/stdarg/OpenGold/issues/223).
Authority: [SRD 5.2.1 p.162](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=162).

The approved shared Spell Choices step includes Shocking Grasp. It requires an
explicit Wizard cantrip grant and uses Intelligence plus proficiency. The existing
Spell dropdown and Cast controls target living creatures at Touch range (five
feet on this grid), including allies/self. Weapon reach does not extend Touch.
The Magic action deals 1d8 Lightning on a melee spell hit through levels 1–4;
critical hits double dice. There is no damage ability modifier, metal-armor
Advantage, adjacent-hostile ranged Disadvantage, saving throw or Concentration.
Existing typed defenses and V/S hand/armor restrictions apply. Speech blockers
remain #39.

A hit prevents only Opportunity Attacks until the **target's** next turn, even
if Lightning immunity prevents damage. It neither consumes nor refunds a Reaction.
Other enemies still interrupt movement. Reaction decisions and interrupted routes
preserve the effect's remaining time without advancing the turn clock. Multiple
applications keep source identities and expiry times; suppression is Boolean.
Turn entry also clears an imported suppression on the acting target. Outside
combat the existing six-second-round timeline expires the remaining duration;
camping/inn rests expire it before normal saving.

Profile recipes validate the new spell and grant. Effect state (FX7)
represents sourced suppression alongside existing blindness/Ray of Frost effects.
No player combat saving controls were added.

Evidence: [native](../tests/shocking_grasp_tests.cpp),
[creator](../tests/shocking_cantrip_view_tests.gd),
[combat controls](../tests/shocking_view_tests.gd). Checks cover ordinary attained
Wizard levels 1–4, independent damage/RNG/Intelligence values, typed defenses,
critical/miss outcomes, actual metal armor, previously spent Reactions,
interrupted movement, exact target-turn expiration, mixed codec validation,
atomic rejection, PC/NPC handoff and rest-save-reload.

Parent #223 stays open for [Sorcerer #132](https://github.com/stdarg/OpenGold/issues/132),
[High Elf #67](https://github.com/stdarg/OpenGold/issues/67),
[Magic Initiate #75](https://github.com/stdarg/OpenGold/issues/75),
[Tome #161](https://github.com/stdarg/OpenGold/issues/161), and
[Temperate Land #157](https://github.com/stdarg/OpenGold/issues/157), as well as
remaining component enforcement #39. Full spell selection remains #37;
higher damage scaling remains #176–178. Native effects do not certify these routes.

Verification: all 43 native/tool checks and all 19 headless Godot runtime checks
plus ten native prerequisites pass. The new creator and existing Wizard creator
checks pass with local original assets. English/Spanish creation and combat
renders were inspected at 1120×800 and 1920×1080. Main/demo extensions build;
825 localization messages validate. Logs: `/tmp/shocking-regression.log`,
`/tmp/shocking-creator.log`, `/tmp/shocking-existing-creator.log`, and
`/tmp/shocking-combat-render.log`; renders `/tmp/opengold-shocking-renders`.

Rules 0.6.40 adds the explicit [level-one Sorcerer source](SORCERER-CANTRIPS.md)
with Charisma casting in #228. References above to remaining Sorcerer access
now concern later levels and the rest of #132, rather than this delivered route.
