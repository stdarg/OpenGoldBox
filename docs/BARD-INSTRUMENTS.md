# Bard starting instrument proficiencies

Tracked in [#214](https://github.com/stdarg/OpenGold/issues/214), a child of
[Bard starting choices #125](https://github.com/stdarg/OpenGold/issues/125).
Rules 0.6.31 implements the starting proficiency choices; profile recipes validate
their grants.

SRD 5.2.1 Core Bard Traits (p. 31) grants three Musical Instrument
proficiencies. The instrument variants (p. 94) are bagpipes, drum, dulcimer,
flute, horn, lute, lyre, pan flute, shawm and viol. The proficiency entitlement
is separate from choosing/owning starting equipment. Multiclass entry grants
one instrument instead and remains outside this starting-character increment.
[Official source](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf).

## Delivered behavior

- The ten instrument IDs are added to the rules-owned tool catalog. Bards require three
  distinct selections in a Bard Training checkbox group, reusing the approved
  controls, keyboard access, Back preservation and class-change pruning.
- Choices emit level-one `tool:` grants from `class:bard:instruments`, show their sources
  on the sheet and presets receive selections through the existing generator.
- Checks apply proficiency once to instrument checks and the existing tool/skill
  Advantage when both are proficient. This grants proficiency, not equipment.
- Verification covers all 120 distinct triples, rejected choices, normal creation,
  sourced sheet display and presets. Runtime/render and
  regression checks are recorded below.

Full Bard spellcasting, Inspiration, starting equipment and instrument Utilize
actions remain separate issues.

## Verification

All 41 native/tool checks and all 16 Godot runtime checks, with seven native
prerequisites, pass. Main/demo extensions build; 788 English/Spanish messages
validate. The graphical main Training test verifies keyboard focus, limits,
Back preservation, the completed Bard sheet's instrument sources, and existing
class/Expertise flows. Instrument controls were rendered and inspected in both
languages at 1120×800 and 1920×1080. The demo lacks locale catalogs; localization
verification uses the main game. Current combat round trips and forged-source
rejections also pass in the final training test run. No production changes
followed the full regression run.
