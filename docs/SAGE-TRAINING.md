# Sage fixed training

[#211](https://github.com/stdarg/OpenGold/issues/211) supplies the Sage background's
Arcana, History and Calligrapher's Supplies proficiency grants. Authority:
[SRD 5.2.1 p. 83](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=83).
Each fixed grant records `background:sage` as its source at level 1.

All twelve starting classes receive these grants. The existing Training fixed
section and character sheet list their names, bonuses and sources. No new control
or layout is introduced. Rogue Expertise may select either proficient Sage skill
through its existing eligibility-driven choice group. Proficiency applies once;
Expertise doubles it. When a tool and skill both apply to a check, the shared
modifier query grants Advantage rather than adding proficiency twice. Determining
which tools apply to a particular campaign action remains that action's job.

Profiles require the fixed package. Campaign saves reconstruct the fixed grants
from the saved Sage background while preserving selected choices, wounds,
resource expenditure and advancement.

The [training tests](../tests/training_tests.cpp) exercise new grants, all-class
creation, Rogue eligibility, modifier totals, invalid grants and canonical
current saves.
The [creation test](../tests/cantrip_view_tests.gd) checks the fixed Training display.

The Sage parent #63 remains open for Magic Initiate (#75), starting equipment/
wealth choices and their full integration. Tool-specific campaign actions remain
separate. This change does not grant the Magic Initiate spell package or invent
items in inventory.

Verification: 40 native/tool checks pass in aggregate after updating nine frozen
migration expectations for the exact owed grants. Eleven affected migration,
profile and spell-continuation tests pass after the final old-version guard.
All 16 Godot runtime checks and seven fixture prerequisites pass. The creation
check verifies fixed Training and final character-sheet text; rendered Training fits
1120×800 and 1920×1080 in English and Spanish. Main/demo extensions build;
745 localization messages validate. New profiles cannot be smuggled into a
checkpoint labeled with an older rules version.
