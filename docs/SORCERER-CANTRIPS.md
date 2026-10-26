# Level-one Sorcerer cantrips

Rules 0.6.40 / PC28 implements [#228](https://github.com/stdarg/OpenGoldBox/issues/228),
a bounded child of [Sorcerer spellcasting #132](https://github.com/stdarg/OpenGoldBox/issues/132).
Authority: [SRD 5.2.1 pp.64–65,67](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf):
four starting cantrips and Charisma spellcasting. At 0.6.40 the eligible catalog
contained Fire Bolt, Poison Spray, Ray of Frost and Shocking Grasp. This is not the
complete sixteen-cantrip Sorcerer catalog.

Q27 approved the existing Spell Choices step, keyboard checkbox selection, counts,
Back preservation, pre-generated preset choices and shared Spell/Cast controls.
Choices record `class:sorcerer:spellcasting` at acquisition level one. Duplicates,
foreign spells/sources, wrong acquisition levels and mismatched spell lists reject.
Missing choices remain pending. The character sheet lists learned cantrips and Charisma provenance.

All four spells use Charisma plus proficiency for attacks. Shared rules retain
typed damage, criticals, range/line of effect, armor restrictions,
and adjacent hostile Disadvantage for ranged spells. Shocking Grasp uses a melee
spell attack. Its hit suppresses Opportunity Attacks until the target's next turn,
without spending the Reaction. Ray of Frost reduces Speed by 10 feet until the
caster's next turn. Both effects apply on a hit even through damage immunity.
Only the Magic action is spent. Other resource pools remain unchanged.

Profile recipes validate the new source and use Charisma for Sorcerers.
No player combat saving controls were added. Player saving follows [SAVE-1](SRD-DECISIONS.md#save-1-2026-10-25-save-wherever-exploration-is-idle): wherever exploration is idle, never in combat.

Evidence: [native checks](../tests/sorcerer_cantrip_tests.cpp),
[creator checks](../tests/sorcerer_cantrip_view_tests.gd) and
[combat checks](../tests/sorcerer_view_tests.gd). Native cases use independent
fixed hit/miss/critical rolls and distinguish Charisma 18 from Intelligence 15,
d8/d10/d12 damage, typed defenses, source validation, effect expiration, illegal
command atomicity, PC/recruited-NPC handoff and rest/save reconstruction.
Creator/combat checks cover keyboard selection,
Back, all four spells, ally targeting and English/Spanish layouts at 1120×800
and 1920×1080.

The rest of #132 (more Sorcerer cantrips, leveled spells and slots, Innate
Sorcery, advancement to level 15 and replacement choices) is in
[Sorcerer](SORCERER.md). Spells have no components
([CLASS-11](SRD-DECISIONS.md#class-11-2026-10-06-no-spell-components)).
All four cantrips can target allied creatures and the caster; Fire Bolt gained
this in rules 0.6.160 ([Fire Bolt](FIRE-BOLT.md)). This increment closes no parent spell
or class conformance claim.

Verification: all 44 native/tool checks and 20 headless Godot checks plus eleven
native prerequisites pass. Asset-backed creator and graphical combat checks pass
in English/Spanish at both supported sizes. Main/demo extensions build, and all
827 localization messages validate. Logs: `/tmp/sorcerer-regression.log`,
`/tmp/sorcerer-creator.log`, `/tmp/sorcerer-combat-render.log`,
`/tmp/sorcerer-demo.log`; renders: `/tmp/opengold-sorcerer-renders`.
