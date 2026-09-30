# Wizard Scholar — #100 frozen batch

- Authorization: standing SRD goal, explicitly continued by the user. One owner,
  existing #100 only, branch `codex/srd-scholar`; no new issues/agents/tasks.
- Player outcome: Wizard level 2 grants Expertise in one proficient skill from
  Arcana, History, Investigation, Medicine, Nature or Religion. Levels 3–4 retain
  that selection; level 1 and other classes receive no Scholar entitlement.
- Acceptance: all six eligible skills with real proficiency provenance; exactly
  one choice; doubled proficiency once, correct modifiers/source display,
  invalid/untrained/replaced/forged choice rejection, ordinary advancement,
  actual Medicine use, pending old-save choices, existing selection locks,
  current/previous campaign and combat continuation. Preserve wounds, equipment,
  spells, Arcane Recovery and all other expenditure. Main/demo player paths and
  game EN/ES/demo EN visual checks at both supported sizes.
- Exclusions: Ritual Adept, Evoker, additional spells, other class Expertise,
  new skill/source catalogs, levels above four and multiclassing. No unrelated
  training redesign or reduced compatibility history.
- Reuse: existing sourced Expertise arithmetic and skill display, transactional
  advancement, Review Training controls, save replay and static SRD library.
  Keep entitlements and eligibility in SRD; Core owns generic history/transactions.
- Source: [SRD 5.2.1 p.78](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
  Scholar, verified at PDF lines 6615–6618. All six candidate skills are already
  in the existing Wizard class skill catalog; no new proficiency route is needed.
- Model: requested `gpt-6-astra` / `high`, retained for advancement-history and
  legacy pending-choice migrations. Actual configuration not independently
  verified; no switch. Escalate out-of-tier decisions or two unsuccessful fixes
  of the same failure. This does not authorize delegation.
- Verification: capture real 0.6.49 writer evidence before changing persistence;
  focused choice/bonus/advancement tests, final native regression and affected
  game/demo checks. No stale binaries or editing active build inputs.
- Timing: frozen 2026-09-25 19:57:47 UTC; checkpoint 20:57:47, maximum 21:27:47.
  Record actual phase endpoints; no invented durations or token savings.

## Approved SCHOLAR-1 — visible question 1

Approved: reuse the disabled ability-points area of the Wizard level-two Level Up
window for a labeled Scholar Expertise dropdown. It lists eligible proficient
skills; Confirm requires a valid selection and grants it with the new level.
Existing level-two through level-four Wizards keep missing choices pending and
complete them in the existing Review Training checkbox groups. Previously chosen
training stays locked. Cancel discards edits; keyboard access and wounds/resources
are preserved. The existing windows retain their dimensions. The user replied “1. Approved.” Implement within this recorded scope.
Review Training was later removed with the [pre-1.0 save cutoff](SAVES.md#pre-10-format-policy);
Scholar is chosen only at Wizard level two.

## Implementation

Scholar eligibility, source validation and Expertise modifiers live in the static
SRD library. Core replays generic advancement training choices transactionally;
the UI renders rules-provided options. A Scholar grant uses
`expertise:<skill>`, source `class:wizard:scholar`, acquired level 2. It is never
recorded as starting training. Campaign saves record advancement training, and
profiles admit Scholar proficiency provenance.

Native conformance in `tests/scholar_checks.h` covers all six skills, proficiency
eligibility, exact level/source, double proficiency once, rejected transactions,
ordinary levels 2–4, current campaign round trips and actual Medicine/Stabilize
rolls.

`tests/scholar_view_tests.gd` exercises deliberate keyboard selection at level
two, Cancel, saving and reloading. `--verify-scholar <output>` compares the complete
UI-produced campaign against the native expected transaction. It adds no player
save control; the existing safe campaign save controls are used.

## Verified delivery — runtime `4ce541a`

- Focused training/Scholar and Arcane Recovery native checks pass.
- Main game EN/ES and demo EN Scholar flows and rendered controls pass at both
  1120×800 and 1920×1080. Captures: `/tmp/scholar-main-captures` and
  `/tmp/scholar-demo-captures`; logs `/tmp/scholar-*-render.log`.
- Localization validates 907 messages. Existing Spanish resource-pool strings
  remain untranslated; Scholar labels/options/source are translated. Demo retains
  its existing English-only presentation.
- All 51 native/tool tests and 26 Godot runtime checks pass. The combined run
  found three older test assumptions (untrained Wizard fixtures and pre-15 save
  expectations); those were corrected, rebuilt and all four affected/dependent
  checks passed. No supported historical fixtures or acceptance were removed.
- Existing Review Training UI regression passes, and both Scholar UI output
  saves match their complete native expected campaigns. Scope, static-library
  boundaries, ownership and `git diff --check` reviewed.

Reproduction commands (from repository root):

```bash
ctest --test-dir build/mac-check --output-on-failure -E '^opengold_godot_prepare$' --fixture-exclude-setup godot_project -j6
python3 tools/localization.py --check
OPENGOLD_GAME_DIR=/Users/edmond/POOLRAD build/mac-check/opengold_training_tests
OPENGOLD_GAME_DIR=/Users/edmond/POOLRAD /Applications/Godot_mono.app/Contents/MacOS/Godot --path src/OpenGoldBox/godot --resolution 1120x800 --script /Users/edmond/src/OpenGold/tests/scholar_view_tests.gd -- --scholar-fixture=/Users/edmond/src/OpenGold/build/mac-check/scholar-ui.ogs --scholar-capture=/tmp/scholar-main-captures --scholar-output=/tmp/scholar-ui-result.ogs
OPENGOLD_GAME_DIR=/Users/edmond/POOLRAD build/mac-check/opengold_training_tests --verify-scholar /tmp/scholar-ui-result.ogs
```

For the demo, use `--path demos/godot`, add `--scholar-demo` and distinct output/
capture paths. The script checks both resolutions; main checks both locales.
Build native targets and `opengoldbox_test_project` before tests; demo uses
`cmake --build build/sprite-demo --target opengold_godot -j6`.
Logs: `/tmp/scholar-integrated-tests.log`, `/tmp/scholar-regression-rerun.log`,
`/tmp/scholar-final-build.log`, `/tmp/scholar-final-demo-build.log`,
`/tmp/scholar-existing-review.log` and the render logs above. Godot editor import
emitted shutdown/window/debugger warnings; the actual game/demo checks completed
with their required success markers and exit 0.

## Measured timing and boundaries

Frozen 19:57:47 UTC; prior-writer capture preceded all writer changes. Focused
integration/visual evidence was observed by 20:18:14; the full native rebuild was
running then. Final verified runtime commit was recorded at 20:27:23: **29m36s**
from the original freeze, including approval wait and builds. Individual earlier
phase start timestamps were not captured, so no separate durations are invented.
The 20:57:47 checkpoint was not reached or reset. One original requirement
(#100) delivered; no new issue, agent, task, scope or compatibility reduction.
Requested Astra/high retained, actual configuration unverified; no model switch.
Token/cost totals unavailable. Other Wizard requirements and the full SRD goal
remain incomplete.
