# Level-one Sorcerer cantrips

Rules 0.6.40 / PC28 implements [#228](https://github.com/stdarg/OpenGold/issues/228),
a bounded child of [Sorcerer spellcasting #132](https://github.com/stdarg/OpenGold/issues/132).
Authority: [SRD 5.2.1 pp.64–65,67](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf):
four starting cantrips and Charisma spellcasting. The implemented eligible catalog
contains Fire Bolt, Poison Spray, Ray of Frost and Shocking Grasp. This is not the
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
No player combat saving controls were added; camp/inn saving remains the policy.

Evidence: [native checks](../tests/sorcerer_cantrip_tests.cpp),
[creator checks](../tests/sorcerer_cantrip_view_tests.gd) and
[combat checks](../tests/sorcerer_view_tests.gd). Native cases use independent
fixed hit/miss/critical rolls and distinguish Charisma 18 from Intelligence 15,
d8/d10/d12 damage, typed defenses, source validation, effect expiration, illegal
command atomicity, PC/recruited-NPC handoff and rest/save reconstruction.
Creator/combat checks cover keyboard selection,
Back, all four spells, ally targeting and English/Spanish layouts at 1120×800
and 1920×1080.

Remaining #132 scope includes the other Sorcerer cantrips, leveled spells and
slots, Innate Sorcery's activation/resources, level 2–4 advancement and replacement
choices. General components remain #39; higher progression remains #176–178.
Fire Bolt still uses the existing enemy-creature targeting path; ally/self
targeting, object targets and ignition remain #166. The other three cantrips
support legal allied creature targets. This increment closes no parent spell
or class conformance claim.

Verification: all 44 native/tool checks and 20 headless Godot checks plus eleven
native prerequisites pass. Asset-backed creator and graphical combat checks pass
in English/Spanish at both supported sizes. Main/demo extensions build, and all
827 localization messages validate. Logs: `/tmp/sorcerer-regression.log`,
`/tmp/sorcerer-creator.log`, `/tmp/sorcerer-combat-render.log`,
`/tmp/sorcerer-demo.log`; renders: `/tmp/opengold-sorcerer-renders`.
