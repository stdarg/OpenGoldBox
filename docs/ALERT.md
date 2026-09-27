# Alert feat delivery packet

## Frozen batch

- Standing SRD goal; original issue #74. One owner, no agents or new issues.
- Player outcome: Alert adds proficiency to Initiative and permits one optional
  swap with a willing eligible ally after rolling, before the first turn.
- Sources: automatic Criminal background grant for all current class creation
  routes; selection through existing level-four feat entitlements. Nonrepeatable;
  reject duplicate acquisition. Human's extra Origin-feat selector remains #71;
  this batch does not implement that separate species feature.
- Exclusions: starting equipment/wealth (#84 and background packages), other
  feats, new class progression, initiative tie UI, general AI improvements and
  combat saving controls. Existing tie adjudication stays unchanged.
- Reuse: feature-grant provenance, existing feat selectors, initiative sorting,
  shared modal/control styling and rules-owned pending decisions. Core performs
  orchestration; the statically linked SRD library owns bonuses and legality.
- Requested Astra/high for combat-start sequencing and save migrations; actual
  runtime selection unverified. No settings change or delegation. Escalate after
  two unsuccessful fixes of the same failure or a new out-of-scope dependency.
- Preflight observed2026-09-26 15:34:23 UTC. Approval continuation began2026-09-26 16:13:12 UTC. ALERT-1
  approved by the user; implementation authorized. Checkpoint due17:13:12 UTC.

## Acceptance

1. The feat's proficiency bonus is added once, scales from the actual character,
   and combines with existing Initiative Advantage/Disadvantage. Incapacitation
   blocks swapping, not the Initiative proficiency bonus.
2. Roll Initiative once. Each eligible holder may swap current Initiative totals
   with one willing ally in the same combat, or keep the result. Both creatures
   must lack Incapacitated. Re-sort after a swap without rerolling or spending
   actions, movement, time, spell slots or resources.
3. Preserve other holders' pending decisions. Resolve all choices before starting
   any turn or applying turn-start recovery/effects. Reject stale/illegal commands
   atomically. Enemy AI keeps its Initiative by default.
4. Criminal creation/presets and current level-four feat selection use real
   entitlements, with source/provenance checks and no duplicate feat acquisition.
   Fixed background grants migrate without replacing chosen training or feats,
   healing wounds, replenishing resources or changing wealth/equipment.
5. Capture genuine0.6.60 campaign/combat fixtures before changing the writer.
   Existing combats retain their rolled order and do not reopen initial choices.
   Retain every supported save version; new pending decisions round-trip exactly.
6. Verify main/demo actual controls, keyboard and mouse, English/Spanish main and
   English demo at1120×800/1920×1080. Focused native conformance plus final affected
   native regression, existing creation/advancement and adjacent combat controls.
7. Record completion in SRD-COVERAGE, commit/push owned checked changes, then close
   #74 only after its acceptance is proven. No background/species issue closure
   is implied by delivery of its Alert dependency.

## Source and implementation entry points

[SRD5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
feat Alert; [official rules text](https://www.dndbeyond.com/sources/dnd/br-2024/feats#Alert).
Alert is an Origin feat with Initiative Proficiency and Initiative Swap; it is
not repeatable. Current `feature_grants.cpp` lacks Alert and `srd5.cpp` rolls
Initiative using Dexterity and existing Advantage/Disadvantage, then immediately
starts combat. Feature grants, character profile/migration, that initialization
boundary, generic pending-choice interfaces and shared combat views are the
bounded source routes. Capture old-writer evidence before editing them.

## Approved control: ALERT-1

In both game and demo, use a centered640×360 Alert dialog before the first combat
turn. Show the selected holder's Initiative total and a labeled Ally dropdown
containing eligible party allies and their current totals. Standard styled
“Swap initiative” and “Keep initiative” buttons resolve only that holder's choice.
Swap is disabled until an eligible ally is selected. Escape keeps that holder's
Initiative. Other combat actions wait.

When several party Alert holders remain, a labeled Resolve next dropdown selects
which holder to resolve; otherwise omit that selector. Swaps update displayed
totals, and every holder chooses once. Support normal keyboard focus/selection
and mouse input. Enemy AI keeps its rolls automatically. Existing feat-selection
controls gain Alert as an ordinary eligible option; no separate creation layout.

## Preflight findings for subsequent Fighter integration

#84 explicitly requires starting equipment choices, still absent from normal
creation's250gp policy. #89 must be reconciled with the remaining feat catalog;
do not close it merely because Weapon Mastery and Fighting Styles are complete.
Those issues remain open. Alert is a bounded original feat requirement and a
Criminal-background dependency; no new issue or silent prerequisite is added.

## Implementation and compatibility

Rules0.6.61 introduces PC40 only for characters who have Alert. Existing profile
versions remain unchanged. Combat26 is used while the opening choices are
pending; it records the remaining holders and preserves RNG, totals and order.
After the final choice, the ordinary current-turn format applies. Each command
carries the current revision, holder and selected ally; stale or illegal commands
fail before mutation. The module computes the bonus, eligible allies, swaps and
final order. Core consumes generic commands; the shared Godot dialog renders
those commands and totals.

The first turn, blindness recovery scheduling and death saves wait until every
party choice is resolved. Enemy holders keep their totals automatically. Campaign
reconstruction adds Criminal's fixed grant; old combat reconstruction retains its
historical profiles/totals and starts no new choice phase. Actual0.6.60 evidence
and hashes are in [the fixture record](../tests/fixtures/README.md#alert-baseline--actual0660-writer).

## Verification

Final integrated runtime: main and demo builds passed;50 native executables
rebuilt, then51/51 non-Godot CTests passed in9.06s. Alert acceptance is part of
`opengold_training_tests` (`--alert` also runs it alone). It covers all12 Criminal
creation routes, six current level4 entitlements, duplicate/provenance rejection,
independent Initiative bonus/cancelled Advantage evidence, multiple holders,
updated totals, enemy/self/incapacitated targets, sleeping holders, stale commands,
malformed pending records,32 seeds including an incapacitated first slot, exact
pending continuation and genuine prior-writer migration. Existing preset tests
also check Criminal Alert grants.

`alert_view_tests.gd` passes mainEN/ES and demoEN at1120×800/1920×1080. It uses
actual keyboard/mouse dropdowns/buttons, exchanges unequal totals, selects the
second resolver, declines with Escape, and compares complete saves with native
oracles. Screenshots were inspected at the minimum size. Creation controls pass
all12 class routes; Fighter ASI UI passesEN/ES and both outputs match the native
oracle. Adjacent Savage/Champion controls and their native prerequisites pass4/4
in4.16s.1014 localized messages and `git diff --check` pass.

Three older regressions needed explicitly updated expectations: decline the new
opening decision before weapon-catalog attacks, and add the fixed Criminal grant
to old rest/mastery campaign expectations. Full comparisons are preserved.
An early test-only compile error used a nonexistent vitality field; corrected
before verification. Existing Godot Mono editor-import diagnostics (saved window
position/debugger timer) remain; actual runtime UI logs are clean. No source
changes followed successful runtime UI verification; subsequent edits were tests
and delivery records. No packaging/export or main-branch integration claimed.

Reproduce from the repository root in Bash:

```bash
cmake --build build/mac-check --target opengold_training_tests -j6
build/mac-check/opengold_training_tests --alert
ctest --test-dir build/mac-check --output-on-failure -E '^opengold_godot_' -j6
python3 tools/localization.py --check
```

Rebuild all affected native targets before the full CTest run. Main preparation:
`opengoldbox_test_project`; demo: `build/sprite-demo` target`opengold_godot`.
Run `tests/run_godot_test.cmake` graphically with `alert_view_tests.gd`,
`--nick-fixtures=/Users/edmond/src/OpenGold/build/mac-check/alert-fixtures`, and
optionally `--nick-captures=/tmp/alert-captures`; add`--nick-demo` for the demo.
Expected marker: `Alert UI tests passed`. These reuse hidden test checkpoint hooks,
not player combat-save controls.

Logs: `/tmp/alert-final-build.log`, `/tmp/alert-native-verified.log`,
`/tmp/alert-{main,demo}-final.log`, `/tmp/alert-creation-ui.log`,
`/tmp/alert-advancement-ui.log`, `/tmp/alert-neighbor-ui.log`.
Screenshots: `/tmp/alert-main-final/`, `/tmp/alert-demo-final/`.

Timing evidence (UTC2026-09-26; observed log timestamps): approval continuation
16:13:12; actual prior-writer build16:16:56–16:17:18; integrated training pass by
16:31:58; final broad build16:32:14–16:35:55. Final native verification completed
before16:39. One original feat requirement delivered; no new issues or scope.
Requested Astra/high; actual runtime selection and token/cost metrics unavailable.

Delivery: tested code2320701 pushed on`codex/srd-alert`; #74 closed at
2026-09-26T16:39:33Z,26m21s after approval continuation. GitHub GraphQL count141.
Completion is recorded once in [SRD-COVERAGE](SRD-COVERAGE.md#alert-complete-74).
