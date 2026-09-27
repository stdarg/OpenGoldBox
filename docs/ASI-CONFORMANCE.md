# Ability Score Improvement conformance — #82

## Fixed batch card

- Goal ACTIVE; one owner, no agents/new tasks or settings changes. Preflight
  2026-09-26 03:41:40 UTC; checkpoint04:41:40, maximum05:11:40. No reset.
- Outcome: reconcile #82 with the actual Ability Score Improvement feat at the
  supported level-four entitlement. Existing implementation already handles
  allocations, score cap, modifier/HP effects, sourced grants and saved history.
  Close only after independent coverage verifies those claims. #84/#85/#89 and
  other class integration issues remain open; no class-completion claim.
- Acceptance: level4 prerequisite; +2 to one or +1 to two of all six abilities;
  maximum20; separate background/feat provenance; duplicate entitlement rejection;
  correct modifiers/saves/skill totals/combat profiles; retroactive Constitution
  HP with wounds/resources preserved; real PC/recruited advancement, previews,
  rejected-command atomicity and campaign/save/rest continuation. Check all six
  current ordinary advancement classes and below-level4 rejection across all12.
- ASI is repeatable, but levels1–4 supply only one eligible entitlement. Do not
  manufacture a second entitlement or broaden this batch to higher levels or
  incomplete class progression. Those remain in their original issues and goal.
- Requested Astra/high; configured model/effort unverified. Reason: provenance,
  derived effects and historical save evidence. Escalate after two failed fixes
  of the same failure. No delegation. Current production code is rules0.6.55.
- Reuse existing approved level-up controls; no layout/behavior changes proposed.
  Preserve existing correctness; add missing independent conformance coverage,
  repair a defect only if evidence identifies one inside this acceptance.
- Verification: focused advancement tests and existing advancement UI checks;
  reuse the just-passed78-test production baseline if runtime stays unchanged.
  No new writer/history fixture is needed unless a runtime recipe changes.

## Sources and baseline

[SRD5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf)
p.87 specifies General category, level4 prerequisite, the two allocation forms,
maximum20 and repeatability. p.23 governs advancement and Constitution HP effects.
The implemented validator exempts ASI from the nonrepeatable-feat set but enforces
one grant per source/level. `maximum_hit_points` already applies Constitution
increases retroactively, including low-modifier/minimum-growth cases.

Existing `advancement_tests.cpp` covers independent HP examples, caps, provenance,
preview atomicity, wounds, spent resources and reconstruction, mainly through
Fighter/Cleric/Wizard. The conformance addition extends allocations/source coverage
to all six current progression classes and recruited characters. Existing class
issues retain the six unavailable progression paths; all12 remain required.

## Verification evidence

The native matrix covers252 legal allocations:21 (+2 or two distinct +1) forms,
six actual class routes, PC/recruited ownership. It independently checks scores,
modifiers, saves, skill totals, combat AC/attacks/HP, Constitution's four-level HP
adjustment, wounds, equipment, provenance, previews, malformed allocations,
duplicate/early grants, exact campaign/combat continuation and rests. Separate
cases cover19/20 caps for every ability and early-grant rejection for all12 classes.
Existing low-Constitution, unconscious, spent-resource and historical cases remain.
No production defect has been identified; no runtime or save recipe was changed.

The old embedded `--advancement-check` assumes a single-page Wizard confirmation
and fails against the already delivered two-page Wizard flow. This is stale test
logic, not evidence of an ASI failure. `asi_view_tests.gd` uses the current flow,
including Wizard's second page, and compares actual UI saves to native results.
It reuses existing test helpers; no new product controls were added. Updating the
obsolete embedded helper is deferred maintenance, not a new issue or scope change.

## Delivery

Verified test/build commit **c3a88b4**, pushed on `codex/srd-asi-conformance`.
#82 is CLOSED (GitHub direct issue state,2026-09-26T03:53:16Z).144 open SRD
issues remain, verified through the repository issues API; the immediately
queried search listing was stale and still included#82. Snapshot:
`/tmp/srd-open-after-asi.json`. No new issues and no gameplay-code changes.

- Extended native advancement suite: passed after rebuild; final CTest0.59s,
  `/tmp/asi-delivery-tests.log`. Build `/tmp/asi-fixture-build.log`.
- Main EN/ES + demo EN, all six progression classes,1120×800/1920×1080:
  `asi_view_tests.gd` passes actual controls, invalid-point prevention, window
  close/Cancel preservation, keyboard confirmation and camp/inn-compatible
  out-of-combat save/reload. All18 output saves exactly match native advancement.
  Logs `/tmp/asi-ui-SURFACE-CLASS.log`; minimum-size Spanish Wizard capture
  visually inspected. Uses the approved current two-page Wizard flow.
- Existing78 integrated checks on unchanged production runtime69771eb remain
  applicable; only this test target, test helpers and its output-directory
  build definition changed. No stale binary or new runtime/history claim.
- `git diff --check` passes. #84/#85/#89 and all other incomplete class work
  stay open; higher-level entitlements remain in their original scope.

Reproduction (Bash, from repository root):

```bash
cmake --build build/mac-check --target opengold_advancement_tests -j6
OPENGOLD_GAME_DIR=/Users/edmond/POOLRAD build/mac-check/opengold_advancement_tests
ctest --test-dir build/mac-check -R '^opengold_advancement_tests$' --output-on-failure
```

Use `tests/run_godot_test.cmake` with `asi_view_tests.gd`, the corresponding
`build/mac-check/asi-CLASS-ui.ogs` fixture, and existing `--style-class`,
`--style-fixture`, `--style-output`, `--style-captures` flags; add `--style-demo`
for the demo. Validate each output with
`opengold_advancement_tests --verify-asi-ui CLASS FILE` under the same asset env.

Observed preflight03:41:40 to verified commit03:52:42 UTC:11m02s; closure03:53:16.
UI verification started03:51:14 and completed by03:52:17. Native final test0.59s;
implementation/build subphases were not separately timed. One original issue
reconciled with existing behavior, not one newly invented gameplay feature.
Requested Astra/high retained; actual configuration/token attribution unverified.
