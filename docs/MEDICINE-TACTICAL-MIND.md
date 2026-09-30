# Medicine stabilization and Tactical Mind

Existing issues: [#32](https://github.com/stdarg/OpenGold/issues/32) and
[#87](https://github.com/stdarg/OpenGold/issues/87). Gameplay implementation is authorized. Q38/Q39 are approved; see [decisions](SRD-DECISIONS.md). The active goal
continuation selected this prepared batch on 2026-09-25 at 14:46 UTC after the
Chill Touch delivery. Existing acceptance and original clock are retained.

## Player outcome and authority

A conscious character can spend a Help Action to stabilize a nearby dying
creature. The check uses the character's actual Wisdom and Medicine training.
A Fighter from level 2 can use Tactical Mind on a failed ability check, rolling
1d10 and spending Second Wind only when the revised check succeeds.

Authority: [SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
p. 18 (Help, DC 10 Wisdom/Medicine, Stable and natural recovery), p. 48
(Tactical Mind, Fighter level 2 and Second Wind), pp. 8–9 (ability checks/training).
The five-foot clear-path aid policy is included explicitly in Q38; it is not
claimed as a range printed in the stabilization paragraph.

## Immutable acceptance

| Route / source | Required behavior and evidence |
| --- | --- |
| All twelve classes' supported levels | Correct untrained/proficient/Expertise Medicine modifier, legal physical targeting, Help Action cost on success and failure, no healing or waking. No automatic success/failure from a natural 20/1 on an ability check. |
| Fighter level 1 | Stabilization works; no Tactical Mind entitlement. |
| Fighter levels 2–4 | Ordinary advancement grants Tactical Mind; it applies to eligible failed ability checks, adds one d10, consumes Second Wind only on success, and never heals. Non-Fighters lack the grant. |
| Turn budgets | Normal Action and eligible Action Surge work; Bonus Action is not consumed. Reaction/off-turn/disabled/pending-choice commands reject without state or RNG changes. |
| Mortality | Success clears death-save counters, stops death saves and starts the existing Stable recovery clock. Damage resumes the existing dying rules; natural recovery keeps Prone. Failed/declined checks change no target state. |
| Resource choices | Decline consumes no d10 or Second Wind. A still-failing boosted check retains Second Wind. Success consumes exactly one use. Short/Long Rest recharge uses the existing service. |
| Continuation | Pending choice, original check, target, action expenditure, resources and RNG survive internal combat checkpoints. Campaign handoff/rest/save preserve final states. No combat-save UI. |
| Player controls | Q38/Q39, game and demo, keyboard/cancel, clear labels and unavailable states; inspect main-game English/Spanish and the demo’s existing English presentation at 1120×800 and 1920×1080. |
| Completion | Inventory all existing ability-check execution paths; do not close #87 while another eligible path bypasses Tactical Mind. Full issue acceptance governs closure. |

## Reuse and boundary

Use the SRD static library's character training/provenance, action budgets,
recovery timeline and the existing Second Wind pool. Introduce a shared check
resolution boundary only with the immediate Medicine/Tactical Mind consumer.
Core owns transactions/persistence; Godot renders legal commands and pending
choices. Do not put SRD thresholds or resource decisions into either layer.

Likely files: `OpenGold.Rules/include/opengold/rules.h`,
`OpenGold.Rules.Srd5/src/srd5.cpp`, character/feature grant validation,
`life_cycle`/`recovery_timeline`, the existing combat views and shared presentation,
plus focused native and Godot tests. Use the current campaign/combat handoff;
no independent health system, UI stack or new GitHub children.

Healer's Kit automation, the Healer feat, general Help advantage, Light attacks,
draw/stow inventory, other Fighting Styles and other class abilities retain their
own existing issues. If an uncovered prerequisite is required for these two
issues, report it before expanding this batch.

## Verification and execution

Run focused checks for independent DC boundaries/modifiers, grant levels,
resource spending, all failure/decline branches, action budgets, Stable timing,
forged/stale requests and exact checkpoint/RNG continuation. Then one relevant
native regression run, game/demo runtime and rendered controls, and actual
campaign/rest/save continuation. Preserve old writer evidence; update coverage,
commit/push and close only fully verified original issues.

Routing: gpt-6-astra/high, retained from the verified preceding batch, for the
shared pending-check decision and save/provenance integration. No model switch,
new task or agents. Escalate/report two failed fixes of the same failure.

Observed batch/preflight start: 2026-09-25 04:41:34 UTC. Review checkpoint
05:41:34; maximum 06:11:34. Do not reset on approval waits or implementation
substeps. Q38/Q39 audio completed successfully; approval widget is visible.


## Verified pre-implementation audit

The `ability_check` path currently derives modifiers only:
`training.cpp::check_modifier` → `CharacterRules::ability_check` →
`RulesModule::ability_check` → `CampaignParty::ability_check`. It preserves
training/provenance, Expertise, sourced advantage and equipment penalties. No game
or demo action currently executes a rolled SRD skill check. Combat's other d20
sites are attack rolls, saving throws/death saves and initiative; initiative has
no failed-check DC, so Tactical Mind cannot boost it. Do not apply Tactical Mind
to attacks/saves or reinterpret original ECL random branches as SRD skill checks.
The original surprise adapter in `phlan_session.cpp` uses authored d6 probability
thresholds, not Wisdom/Medicine or a failed SRD ability check.

Medicine will be the first rolled SRD skill-check action. Reuse
`status_effects.cpp::d20` with the proper ability-check modifier and separate
success comparison; do not use attack natural-1/20 rules. Reuse
`life_cycle.cpp::stabilize`: it clears death-save counters, stops their clock and
rolls one d4 for natural Stable recovery without healing. No second stabilization
roll or new recovery timeline is needed. Source/level validation must add only
legitimately attained Tactical Mind grants.

Q38/Q39 were approved on 2026-09-25 after the 14:46 UTC continuation. Implementation resumes within this fixed acceptance; prior preparation is not issue completion.


## Implementation and verification

Q38/Q39 were approved on 2026-09-25. The rules module now resolves Medicine
through the validated training profile, grants Tactical Mind through ordinary
Fighter advancement and suspends an eligible failed check for the user's choice.
PC42 recipes record the Fighter entitlement, added through the existing replay
path. The combat checkpoint preserves the original failed roll, target and spent
Action type along with resource/RNG state.

The public snapshot exposes check-choice display data. Core's only new branch
selects the first rule-offered choice for AI; it contains no SRD calculation,
DC, resource cost or feature decision. New controls are confined to the approved
game/demo views. Player saving remains camp/inn only.

Focused native checks pass for all twelve starting classes, Fighters 1–4,
proficient Medicine sources, actual roll extremes, both d10 outcomes, declined
boosts, conditional resource spending, Action Surge, malformed/stale commands,
exact pending-check continuation and ordinary campaign handoff/rest/save at each
eligible Fighter level. Game/demo runtime checks and main EN/ES renders at
1120×800 and 1920×1080 pass. The first UI check found the newly visible aid row
needed a full layout refresh; the correction passes. All 47 native/tool regressions subsequently pass.
The native run initially exposed five stale grant/profile assertions, corrected
without weakening the old-save comparisons.

Medicine skill totals come from the same validated training profile as other
skills, including its Expertise arithmetic. Currently supported grants provide
Medicine proficiency to Bard, Cleric, Druid and Paladin; there is no legal
Medicine Expertise acquisition route yet (Rogue class skills exclude Medicine,
and the relevant Skilled/Bard progression remains in its existing backlog).
Tests do not fabricate a supported route or mark those owning issues complete.
Generic fixed monster demo profiles lack ability scores/training and do not gain
an invented Medicine modifier; all actual PC/NPC character profiles can attempt
stabilization, including on dying opponents.

Observed implementation/build/check progression: 14:46 UTC goal continuation;
focused native pass by 14:58; both game/demo control checks and rendered views
by 15:04. New target registration regenerated Godot bindings; those build inputs
were left unchanged during the build. Original 04:41:34 batch/preflight clock
is retained rather than restarted after the approval wait. Astra/high routing
retained; no agents, issue splitting, new scope or model switch.


Keyboard stabilization uses Left/Right to select rule-offered targets, Space to
attempt the check and Escape to cancel. The current target has stronger board
highlighting. The game displays the target/instructions in its existing footer;
the demo uses its existing prompt. These complete Q38's keyboard path, including
when a mouse is unavailable. The modal retains standard keyboard-focus controls.
Combined Cunning Action/Wake ally/Stabilize rows fit in the supported minimum size.

Final verification commands and logs:

- Build all native targets and `opengoldbox_test_project` in `build/mac-check`;
  build `opengold_godot` in `build/sprite-demo`.
- `ctest --test-dir build/mac-check --output-on-failure -E '^opengold_godot_' -j6`:
  47 pass (`/tmp/medicine-native-tests.log`).
- Godot suite: `-R '^opengold_godot_' -E '^opengold_godot_prepare$'
  --fixture-exclude-setup godot_project`, after current project preparation:
  24 runtime checks plus 14 fixture prerequisites (`/tmp/medicine-godot-regression.log`).
- `tests/run_godot_test.cmake` with `tests/medicine_view_tests.gd`, graphical main
  EN/ES and demo EN at 1120×800/1920×1080, captures in
  `/tmp/medicine-{main,demo}-captures`, logs `/tmp/medicine-{main,demo}-final-render.log`.
- `python3 tools/localization.py --check`: 886 complete EN/ES messages.
- Scope/architecture/diff review: no new issues, unchanged frozen saves, mechanics
  in `opengold_rules_srd5`, no player combat saving, no new runtime or framework.

The only rolled ability-check action currently supported is Medicine; future
ability-check implementations must use the same failed-check decision boundary.
Attack rolls, saving throws and death saves are excluded. Unsupported Medicine
Expertise acquisition and generic monster ability-score authoring remain under
their existing owners, not silently implemented or counted as complete here.


Tested runtime revision: `ce04673`. Final native regression passed by 15:20 UTC;
all 24 Godot checks and both graphical paths finished by 15:24:20 UTC on
2026-09-25. This is approximately 38 minutes after the authorized continuation,
separate from the original preparation/approval interval. Two original feature
requirements delivered, no added issues, no scope changes. The completion record
is [coverage](SRD-COVERAGE.md#medicine-stabilization-and-tactical-mind).
