# Skill, tool and language grants

F02 is split into the [rules/persistence layer (#187)](https://github.com/stdarg/OpenGold/issues/187),
[creation controls (#188)](https://github.com/stdarg/OpenGold/issues/188), and
[completion of missing saved choices (#189)](https://github.com/stdarg/OpenGold/issues/189).
All three layers are now implemented, completing parent [#29](https://github.com/stdarg/OpenGold/issues/29).
The #189 party panel Review Training dialog, which completed training missing
from old saves, was later removed with the [pre-1.0 save cutoff](SAVES.md#pre-10-format-policy).
Training is selected at creation and level-up.

## First supported package

- All eighteen skills have their ordinary governing abilities and derived bonuses.
- All twelve classes choose their starting skills from their SRD lists; see
  [class skill coverage](CLASS-SKILLS.md). Rogue chooses four and two proficient skills for
  Expertise. Its fixed Thieves' Tools and Thieves' Cant grants retain class sources.
- Criminal grants Sleight of Hand, Stealth and Thieves' Tools with background sources.
- Every character knows Common and chooses two distinct other standard languages.
  Rogue chooses one additional distinct language from the standard or rare tables.
  Known Common and Thieves' Cant are excluded from the additional selections.
- Partial selections remain explicitly incomplete. Unknown groups/options,
  duplicate selections within a group, excessive selections and Expertise without
  proficiency reject. Both sources of an overlapping class/background proficiency
  persist; overlap supplies no second bonus or automatic replacement choice.

Starting class tools and the four supported backgrounds are covered by
[class skills](CLASS-SKILLS.md), [background training](BACKGROUND-TRAINING.md),
[Sage](SAGE-TRAINING.md), and the tool packages linked below. Later class proficiency features, feats (including Criminal's Alert),
starting equipment, higher-level Rogue features and campaign uses of skills remain
in their respective plan issues. A completed training selection means only that
the choices supported by this increment are filled, not that the class is complete.
The project target remains all twelve SRD classes.

The rules follow [SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
pp. 8–9 (proficiency and skills), 20 (languages), 61–62 (Rogue), 83 (Criminal),
and the Expertise glossary entry. Proficiency contributes once; Expertise doubles
it once. A check involving both a proficient skill and a proficient tool gains
Advantage without adding proficiency again. Tool checks can use the governing
ability required by the situation. The shared query reports the numeric modifier,
Expertise, tool-derived Advantage and all contributing sources; it does not roll
dice, advance time, consume resources or resolve campaign interactions.

## Creation and presets

The game and legacy demo share a Training step after Class and before Name.
Fixed grants appear above scrollable checkbox groups with selected/required
counts. Every required selection must be complete before Next is enabled.
Standard control states and keyboard focus remain visible; focusing a later
choice scrolls it into view. Expertise offers only currently proficient skills.

Back retains choices. Changing class or background retains legal selections and
removes only choices that become invalid, including dependent Expertise. Moving
an additional Rogue language into the starting-language group clears the now
duplicate additional choice. Re-selecting a class does not invent cleared choices.
Start over clears all selections. Completed sheets show all eighteen skill bonuses,
proficiency/Expertise and the sources of skill, tool and language grants.

All 48 presets (four per class) now come with deterministic, complete choices
for the supported training packages. Adding a preset keeps the existing direct
Add to party action; it does not open a training dialog. This follows the user's
approved Training layout and explicit requirement that preset choices be
pre-generated. Opening or loading saved characters does not apply preset
generation.

## Data and validation

`CharacterDraft::training` records named choice groups and ordered selections.
`CharacterRules::training_options` supplies valid options and required counts.
The implementation uses the F01 `FeatureGrant` records with `skill:`, `tool:`,
`expertise:` and `language:` IDs, distinct source IDs and acquisition levels.
`CharacterSheet::training` contains derived skill totals, known tools/languages,
their sources and a completeness flag. Advancement refreshes the derived totals
when an ability modifier changes. `CharacterRules::ability_check` calculates a
check from validated grants and scores rather than trusting cached display totals.

Rules stay in the C++20 SRD library. Core serialization stores the records and
asks the module to validate them; Core does not interpret SRD
skill names, proficiency math or grant prefixes. There is no new runtime or UI stack.

## Persistence

The campaign stores the original training selections as well as the acquired
grant records, which must agree on load. Character profile recipes validate grant
entitlements and choices before creating an actor. Loading never generates or
fills training choices, and combat restoration does not manufacture them. See
[saves](SAVES.md#pre-10-format-policy).

## Verification

`opengold_training_tests` verifies the Rogue/Criminal package, all ordinary skill
bonuses, source overlap, Expertise eligibility, proficiency-table boundaries,
tool/skill Advantage, language permissions, incomplete selections and invalid
choices. Campaign tests check exact round trips and transactional rejection;
combat tests retain source records and reject forged ones.
Existing advancement, feature-grant, character, party, save and combat regressions
remain required. Creator tests exercise transitions, invalid-choice atomicity,
dependent pruning, restart, and manual/preset party save round trips. All 48
presets are validated, covering all twelve classes.

`tests/training_view_tests.gd` drives the actual game and demo controls with
user-supplied original files. It checks keyboard Space/Tab, focus retention,
scrolling, counts, selection limits, dynamic Expertise, Back, class/background
changes, the sheet and party handoff at the minimum 1120×800 size. The existing
`--character-check` and `--party-check` paths also cover Training, and
`tests/localization_tests.gd` checks Spanish training labels and sources.
Example (replace the original-file path for your installation):

```sh
OPENGOLD_GAME_DIR=/path/to/POOLRAD OPENGOLD_LANG=en godot --headless \
  --path src/OpenGoldBox/godot --script "$PWD/tests/training_view_tests.gd"
```

Use `--path demos/godot` for the same check against the demo. Omit `--headless`
and append `-- --training-capture=/tmp/opengold-training` for rendered captures.

Rules 0.6.15 also persists Orc Adrenaline Rush uses and pending Temporary HP
replacement; see [Temporary HP](TEMPORARY-HP.md).

Bard starting instrument proficiencies use a choose-three group from all ten SRD
instruments. Presets generate selections. See
[Bard instruments](BARD-INSTRUMENTS.md) for sources, versioning and verification.

Monks choose one artisan tool or instrument from all 27 SRD options. Shared
continuity metadata preserves instruments across Bard/Monk changes; artisan
tools remain Monk-only choices. See [Monk tools](MONK-TOOLS.md).

Druids receive fixed Herbalism Kit proficiency from their class. See [Druid Herbalism Kit](DRUID-HERBALISM.md).

Soldiers choose one of four Gaming Set variants through Training, independently
of class. Class changes retain the choice; leaving Soldier removes it. Presets
generate choices. See
[Soldier Gaming Set](SOLDIER-GAMING.md).
