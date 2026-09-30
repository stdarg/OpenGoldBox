# Fighter Champion — bounded batch #88

## Frozen outcome and scope

Deliver SRD 5.2.1 Champion at normally attained Fighter levels 3–4:
Improved Critical (weapon/Unarmed Strike attack rolls 19–20), Remarkable Athlete
(Advantage on Initiative and Strength/Athletics checks), and optional immediate
movement up to half Speed after a critical hit, without opportunity attacks.
Fighter levels 1–2 and other classes do not gain these benefits.

Authority: [SRD 5.2.1, pp. 48–49](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf).
The movement trigger says a critical hit, not only a weapon critical hit; keep
spell criticals distinct from the expanded weapon/Unarmed Strike threshold.

| Acceptance | Required evidence |
| --- | --- |
| Source and progression | Normal Fighter creation, level 3 acquisition and level 4 retention; reject wrong level/class/source; current and older saves preserve selected feats, training, wounds, inventory and expenditure. |
| Criticals | 19/20 weapon and Unarmed Strike criticals; other attacks retain normal thresholds; critical dice, automatic hits and existing adjacent-Unconscious behavior remain correct. |
| Advantage | Initiative and Strength/Athletics only; normal cancellation with Disadvantage and no extra stacking dice; no bonus to unrelated checks. |
| Movement | Immediately optional after critical resolution, including reactions; bounded by half current Speed, terrain/occupancy/posture rules; no opportunity attacks or normal movement spending; stop/decline, multiple triggers and interrupted movement resume correctly. |
| Decisions | Preserve Savage Attacker sequencing, original Action/Reaction cost, per-turn resources and RNG; other actions wait during free movement. |
| Persistence | Actual pre-change writer captures; legacy encounters retain exact behavior; current pending movement, targets, path and allowances survive internal checkpoints; campaign handoff/rest/save correct. |
| Player path | Approved controls in game/demo; keyboard/mouse/cancel, disabled illegal commands, main EN/ES and demo EN renders at 1120×800/1920×1080. No combat saving controls. |

Older-save and legacy-encounter requirements above are superseded: saves now
have one pre-1.0 format and nothing is migrated; see
[SAVES.md](SAVES.md#pre-10-format-policy).

## Approved decisions

Q41 approves naming Champion and its fixed grants in the existing Fighter
level-three confirmation, with Confirm applying the sole SRD subclass. Existing
level-three/four Fighters would gain the same fixed grants through save replay;
prior choices/wounds/resources remain. User selected the fixed SRD Champion acquisition.

Q42 approves an optional free-movement phase after critical resolution. Reuse
battlefield highlights and arrow/click navigation; temporarily rename End Turn
to Finish free move; that control/Escape discards the remaining allowance.
Other actions wait; existing Action/Reaction costs remain and interrupted enemy
movement resumes afterward. The user approved this behavior on 2026-09-25.

## Architecture and exclusions

SRD library owns grants, advantage, critical thresholds, allowances, timing and
continuation validation. Public snapshots expose resolved state; Core orchestrates
transactions; Godot renders commands. No rule calculations in Core/UI.
No Fighter mastery/style completion, levels 5+, other subclasses, new general
Athletics campaign actions, spell-access expansion or automatic issue splitting.
Existing skill-query interfaces must expose Athlete advantage for their actual
consumers; do not invent an Athletics action solely for a passing test. Report any
required out-of-scope prerequisite and obtain approval before implementing it.

## Execution card

- Active full goal; original issue #88, one implementation owner, no agents.
- Recorded/requested Astra/high retained for reaction/critical timing and save
  migrations; no model switch. Escalate after two unsuccessful fixes of one
  failure or an architectural decision beyond the assigned tier.
- Start/preflight 2026-09-25 15:26 UTC; checkpoint 16:26, maximum 16:56 UTC.
- Q41/Q42 asked together with successful Glass audio and question widgets.
  Both were answered and approved before gameplay edits.
- Checks: focused grants/critical/advantage/movement/continuation tests, affected
  legacy regressions; one final native suite plus affected/full shared Godot
  checks, both app builds and required rendered views. Record exact revision.
- Prior turn: PROGRESS, delivered #32/#87 and pushed main. This preflight does
  not close #88 or complete the full SRD goal.

## Delivery and measured checkpoints

Runtime `6324a51` (module 0.6.46) implements this packet. PC31 identifies the
Champion entitlement; combat18 is written only during pending free movement.
Campaign replay grants the fixed subclass features at the character's attained
level under Q41.

Observed milestones on 2026-09-25: start/preflight 15:26 UTC; first focused rules,
control and render checks passed by 15:50; full 48-test native pass by 16:04:31;
25 Godot runtime checks plus prerequisites (40 total) and final graphical checks
passed before runtime commit at 16:06:21. About 40 minutes to verified runtime,
within the 16:26 checkpoint. Build/review phases overlapped; finer unique effort
and token/cost deltas are unavailable. One of one selected original requirements
is delivered; no new issues, extra prerequisites or expanded scope.

Verification found and corrected a killed-mover reaction continuation, the
training End visibility gate and Grip refresh overriding disabled state. Old
regressions needed the approved grant/profile changes and independently selected
seeds after the extra Initiative die. The expected-migration helper initially
skipped one of two color banks; its field offset was corrected. No historical
fixture was rewritten, acceptance dropped, or same-failure escalation needed.
Recorded/requested Astra/high was retained; no model switch or delegation.

All checks pass on runtime `6324a51`: 48 native/tool tests, 25 Godot runtime
checks (40 including fixture prerequisites), main EN/ES and demo EN graphical
checks at both supported sizes, plus 891 localized messages. Scope/architecture
review confirms rules remain in the STATIC SRD library; Core only follows the
rules-offered default while Godot presents resolved commands and state.

Full logs: `/tmp/champion-final-native-{build,tests}.log`,
`/tmp/champion-final-{main,demo}-build.log`,
`/tmp/champion-final-godot-tests.log`, and
`/tmp/champion-{main,demo,creator}-final-render.log`.
Final captures are in `/tmp/champion-{main,demo,creator}-final-captures`.
No live build/test handles remain.

## Verification detail and limits

`tests/champion_tests.cpp` supplies independent dice expectations for Initiative
and critical damage, ordinary Fighter advancement at every supported level,
exclusive free-movement budgets, Difficult Terrain, crawling, multiple criticals
in one turn, Savage Attacker ordering, actual opportunity attacks and interrupted
routes, campaign rest/handoff and canonical persistence.

`tests/champion_view_tests.gd` exercises the real level-three confirmation and
keyboard Confirm, plus battlefield arrows/clicks, Escape, keyboard Finish,
disabled unrelated actions/Grip and absence of combat-save controls. Game EN/ES
and demo EN are rendered at 1120×800 and 1920×1080.

Strength/Athletics Advantage is available through the existing public skill-check
query with sourced evidence. No currently implemented action rolls Athletics;
adding such actions remains outside #88. Supported Fighters have no spell-access
source: Magic Initiate/multiclass features must later connect their actual spell
routes and immediate per-hit movement, including multi-attack spells. This batch
does not claim those future player paths. The current spell critical calculation
retains natural 20, while weapon and Unarmed Strike thresholds expand to 19–20.

Verification commands (run after rebuilding the affected targets):

```bash
# Build every native target listed by ctest, then run the shared-rule regression.
ctest --test-dir build/mac-check --output-on-failure -E '^opengold_godot_' -j6
cmake --build build/mac-check --target opengoldbox_test_project -j6
cmake --build build/sprite-demo --target opengold_godot -j6
OPENGOLD_GAME_DIR=/Users/edmond/POOLRAD ctest --test-dir build/mac-check --output-on-failure -R '^opengold_godot_' -E '^opengold_godot_prepare$' --fixture-exclude-setup godot_project
python3 tools/localization.py --check
```

Graphical checks use `tests/run_godot_test.cmake`, `champion_view_tests.gd`,
`GRAPHICAL=ON`, the generated `build/mac-check/champion-fixtures`, and
`--champion-demo` or `--champion-creator` for the respective app paths.
