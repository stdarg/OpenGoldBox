# Acolyte and Soldier fixed training

[#212](https://github.com/stdarg/OpenGold/issues/212) supplies Acolyte Insight and
Religion, and Soldier Athletics and Intimidation. Acolyte's Calligrapher's Supplies
and Soldier's Gaming Set were removed with all tool proficiencies
([DM-1](SRD-DECISIONS.md#dm-1-2026-09-30-tools-languages-and-dm-adjudicated-spells)).
The source is [SRD 5.2.1 p. 83](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=83).
Each grant records its background source and acquisition at level one.

All twelve starting classes receive the fixed grants. Rogue Expertise can select
these skills through its existing choice group. Overlapping class/background
proficiency applies once; Expertise doubles proficiency.
Changing backgrounds retains valid choices and clears Expertise that loses its
prerequisite proficiency. Presets use the existing deterministic training choices.

Existing Training and character-sheet controls show names, modifiers and sources
in English and Spanish. No new layout or control behavior is introduced.

Profiles reject missing or forged background grants. Campaign saves reconstruct
the fixed grants alongside explicit choices, wounds, resources, equipment and
advancement.

[Training tests](../tests/training_tests.cpp) cover all classes,
Expertise, independent check totals, profile validation and level-four
ability improvements. [Godot checks](../tests/training_view_tests.gd) cover the
actual choice groups, background switching, keyboard access and final party sheet.

Acolyte #61 remains open for Magic Initiate and starting equipment/wealth.
Soldier #64 retains Savage Attacker. Starting equipment/wealth remains open. This
original increment completed only the fixed skill grants.

Verification: all 40 native/tool regression checks pass; the final focused
training check also verifies valid preceding PC15 Sage profiles. All 16 Godot
runtime checks and seven native fixture prerequisites pass. The actual Training
creation/party flow passes, with both backgrounds rendered and inspected at
1120×800 and 1920×1080 in English and Spanish. Main and demo extensions build;
747 localization messages validate. Scope/architecture review and diff checks pass.
