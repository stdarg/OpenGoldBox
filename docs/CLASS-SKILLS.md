# Starting class skill choices

[#213](https://github.com/stdarg/OpenGold/issues/213) completes the starting skill
choices from the [SRD 5.2.1 Core Traits tables](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf).

| Class | Required choices | Printed page |
| --- | ---: | ---: |
| Barbarian | 2 | 28 |
| Bard | 3, from all eighteen skills | 31 |
| Cleric | 2 | 36 |
| Druid | 2 | 41 |
| Fighter | 2 | 47 |
| Monk | 2 | 49 |
| Paladin | 2 | 53 |
| Ranger | 3 | 57 |
| Rogue | 4 | 61 |
| Sorcerer | 2 | 64 |
| Warlock | 2 | 70 |
| Wizard | 2 | 77 |

Except Bard, each class uses its specified list. Fighter includes Persuasion;
Wizard includes Nature. The rules catalog and independent test expectations
record every permitted skill. Rogue's prior list and dependent Expertise remain
unchanged. A class choice records `skill:<id>` from `class:<id>` at level one.
Duplicate choices and out-of-list skills reject. Overlapping background/class
proficiency keeps both sources and contributes proficiency only once.

## Player path

The existing Training checkbox groups show each class's skill list, selection
count and limit. Next requires all supported choices. Keyboard selection keeps
focus; Back and background changes preserve valid selections. Changing class
keeps skills allowed by the new list, in selection order up to its limit, and
records them under the new class entitlement. Other invalid choices clear. Fighter retains the approved style
dropdown above languages. No new control type or independent layout is added.

The shared controls rebind callbacks when a row changes class and retain the
catalog's option order. All 48 presets use the existing deterministic generator
to fill the newly supported choices. Completed sheets list skill bonuses and
translated class sources through the existing Training display.

## Persistence and verification

Profiles accept the class sources. Campaign saves retain choices, grants,
levels, wounds and spent resources. Rogue's recorded choices remain valid.

Native checks cover every allowed skill for all twelve classes
across all four backgrounds, exact lists/counts, bonus/provenance correctness,
invalid choices and canonical current saves. Existing creator/preset tests retain
Back, pruning and deterministic generation checks.

The Godot Training test switches through every newly supported class list,
selects by keyboard, checks limits, preserves choices on Back and continues the
existing Rogue/Fighter flow. Wizard/Cleric cantrip creation and main/demo party
fixtures now make the required skill selections through their existing controls.

Class tools and Druidic were removed by [DM-1](SRD-DECISIONS.md#dm-1-2026-09-30-tools-languages-and-dm-adjudicated-spells). Further Expertise features, spell choices, equipment,
subclasses, higher levels and multiclass acquisition remain in their own issues.
This increment does not complete the full starting-class trackers or #29/#52.

Verification: all 41 native/tool regression checks pass, including every class
skill list and all 144 class-to-class preservation cases. All 16 Godot runtime
checks and seven native prerequisites pass. The all-class Training flow,
Wizard/Cleric Spell Choices and class-source displays, main/demo creation checks
and full main/demo party flows pass. English/Spanish Bard and Wizard skill lists
were rendered and inspected at 1120×800 and 1920×1080. Main/demo extensions build;
all 777 localization messages validate.
