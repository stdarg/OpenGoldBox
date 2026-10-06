# Rogue attacks — frozen delivery batch

- Authorization: active standing SRD goal. One owner; no agents, new tasks or
  new issues. Branch `codex/srd-rogue-attacks`.
- Outcome: playable Sneak Attack at Rogue levels 1–4 and Steady Aim at levels
  3–4, reached through ordinary creation and advancement in game and demo.
- Owners: #112 (Sneak Attack, including existing #220/#221); #114 (Steady Aim).
  #116 owns the necessary ordinary level-three/four advancement integration,
  but remains open for its broader class-completion acceptance.
- Exclusions: Thief (#115), Hide (#219), weapon mastery (#60), new feats,
  new spell effects, level five+, multiclassing, combat saving controls.
  None of these requirements is removed or claimed complete by this batch.
- Reuse: pure Sneak eligibility/dice helper, pending weapon-hit/Savage pipeline,
  existing Bonus Action row, level-up dialog, sourced grants and history.
  SRD decides all eligibility, damage, budgets, movement and progression;
  Core retains generic transactions/history and UI presents rules options.
- Requested model: `gpt-6-astra/high` for optional-hit/reaction/damage and
  migration interactions. Actual configuration unverified; no setting change.
  Escalate after two unsuccessful fixes of the same failure or a decision
  beyond this assignment. Failed fixes: zero at preflight.
- Timing: observed preflight start 2026-09-25 21:27:56 UTC. Checkpoint
  22:27:56, no later than 22:57:56. Preserve this start across approval waits.
- Source: [official SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
  pp.61–63; Rogue feature table, Sneak Attack, Steady Aim and level-four ASI.
  Rechecked 2026-09-25. See [existing Sneak foundation](SNEAK-ATTACK.md).

## Fixed acceptance

| Requirement | Required evidence |
| --- | --- |
| Sneak Attack | Optional once per combatant turn, 1d6 at Rogue1/2 and 2d6 at Rogue3/4; hit with Finesse/Ranged weapon and net Advantage, or capable ally within five feet and no net Disadvantage. Actual ally query excludes attacker; no extra ally sight/reach rule. |
| Damage | Matching weapon type; double extra dice on criticals; fixed Blowgun base stays fixed. Thrown Dagger qualifies, thrown Handaxe does not. Spells/unarmed excluded. Combine signed weapon and extra components before flooring/resistance. Savage changes only weapon dice. |
| Timing | Miss, decline and ineligible hit do not spend Sneak. Reset on every actor's turn, including reactions during another turn. Original Action/Reaction remains spent; interrupted movement resumes after all hit decisions. |
| Steady Aim | Level3+, Bonus Action only before movement that turn; next attack roll gains Advantage, consumed even on miss and canceled normally by Disadvantage. Speed becomes zero for the rest of the turn. Unused Advantage expires; no rest pool. |
| Ordinary progression | Real XP-driven Rogue1→2→3→4, existing fixed HP growth and level-four available feat/ASI choices, Cunning Action retained, correct feature provenance/dice at each level. Thief and broader Rogue completion remain pending. |
| Persistence | Actual prior-writer continuation, all released histories retained, strict source/level/pending-state validation, deterministic RNG at each decision stage, no invented legacy combat abilities. Campaign migration adds only justified fixed grants. |
| Transactions | Invalid choices leave state/RNG unchanged. Wounds, equipment, training, resources, party/recruited ownership and rest/campaign handoff remain correct. |
| UI | Approved controls only; standard visible buttons, keyboard/mouse, proper disabled state, game EN/ES and demo EN input/renders at 1120×800 and 1920×1080. |

The Sneak Attack decision (optional use, decline, dialog) is superseded by
[AUTO-1](SRD-DECISIONS.md#auto-1-2026-09-30-automatic-choices-with-logging):
Sneak Attack now applies automatically to the first eligible hit each turn and
is logged. Steady Aim and the Bonus Action row are unchanged.

The prior-writer, released-history and migration persistence requirements are
superseded: saves now have one pre-1.0 format and nothing is migrated; see
[SAVES.md](SAVES.md#pre-10-format-policy).

## Approved controls

ROGUE-1/2/3 approved 2026-09-25: user replied “1. Approved. 2. Approved.
3. Approved.” Implementation follows these recorded layouts and behaviors.

**ROGUE-1 (supersedes pending Q25; itself superseded by AUTO-1 on 2026-09-30,
the dialog was removed):** Centered 700×380 Sneak Attack dialog after
an eligible hit. Show target and extra dice; use two full-width, stacked buttons:
Use Sneak Attack and Keep hit; save Sneak Attack. Use spends the turn's use and
rolls the extra dice; decline/Escape preserves it. Savage Attacker follows if
eligible and changes only weapon dice. Damage and other actions wait until
decisions finish; the attack's Action/Reaction stays spent. Keyboard navigation
and normal control styling; no combat-save controls.

**ROGUE-2:** Reuse the existing Cunning Action dropdown/Use Bonus Action row,
relabeling its caption Bonus Action. Add Steady Aim at Rogue3+, alongside Dash
and Disengage. Keep the existing position and keyboard access. Disable illegal
options and disable Use for an unavailable selection. Steady Aim spends the
Bonus Action, grants Advantage on the next attack roll this turn and sets Speed
to zero until turn end; unavailable after movement. Other options retain their
normal eligibility. No new row or automatic activation.

**ROGUE-3:** Extend the existing level-up confirmation to Rogue levels three
and four. Level three lists Steady Aim and 2d6 Sneak Attack; level four reuses
the existing available feat/ability controls and fixed HP preview. Confirm
commits atomically; Cancel discards edits; existing wounds/resources remain.
The note explicitly says Thief features, Hide and weapon mastery are still
unavailable. This does not grant partial Thief or close #116.

## Evidence / current phase

### Demo placement discovery — ROGUE-DEMO-1 approved

The legacy demo has no Cunning Action row; it uses a sidebar action grid and
separate Wake/Stabilize controls below the battlefield. ROGUE-2's assumption of
an existing dropdown therefore applies to the main game only. Do not silently
choose a new demo layout. Its Sneak dialog remains covered by ROGUE-1.

Proposed demo adaptation: one 36-pixel-high Bonus Action row below the
battlefield, above Wake/Stabilize, reserving 44 vertical pixels when shown.
At the minimum window, label x24/w180, dropdown x214/w200, Use Bonus Action
x424/w240. Shift existing utility rows down by 44 and reduce the battlefield's
available height by 44; retain all existing controls and the footer. Use the
same Dash/Disengage/Steady Aim options, keyboard behavior and rules-driven
disabled states approved in ROGUE-2. No new mechanics or scope beyond the
already required demo player path. Await a visible numbered answer for this
specific placement before implementing it. Approval received 2026-09-26: “1. yes.”

### Verified implementation and remaining work

Module0.6.52 implements live Sneak through levels1–4, Steady Aim at3/4 and
ordinary Rogue advancement through4. Main and demo Sneak dialogs and advancement
pass keyboard, exact native-save comparisons and rendered checks at1120×800 and
1920×1080 (main EN/ES, demo EN). The main Bonus Action row passes the same
checks; demo Steady Aim awaits ROGUE-DEMO-1. No new runtime mechanics were added
outside the frozen acceptance. No historical fixtures were regenerated.

Tested runtime commit: `7f3ab30` (same production tree as the checks below).
Final added-test training run passed before this commit.

Final integrated runtime tree: 78/78 checks, 37.53s (`/tmp/rogue-final-tests.log`),
51native/tool and27Godot. Localization:934 complete EN/ES messages. Build/test
commands follow workflow, with the current project prepared before excluding
its fixture setup. Asset-backed advancement uses `rogue_advancement_view_tests.gd`
and `opengold_training_tests --verify-rogue-ui` on both saved outputs. Captures:
`/tmp/rogue-combat-main-final`, `/tmp/rogue-demo-captures`,
`/tmp/rogue-advance-main-final`, `/tmp/rogue-advance-demo-final`.

Native `rogue_attack_checks.h` covers real criticals, opposed roll cancellation,
incapacitated allies, throws, opportunity attacks/interrupted movement, separate
Savage dice, resistance, rejected commands, pending-state RNG, campaign/rest and
XP progression. Final extra tests add malformed fields and recruited ownership
through all four levels.

Observed phases: preflight21:27:56; runtime implementation and focused combat UI
were present by22:09; final advancement fit and integrated verification complete
by22:23. Intermediate phase boundaries were not recorded precisely; these are
observations, not exclusive effort totals. One wrap-only fit fix was insufficient;
font14 resolved it. Two distinct new test-setup mistakes (content identity and a
non-party ally in party handoff) were corrected without production changes.
Requested Astra/high retained; actual model/effort and token delta unavailable.

Preflight baseline (historical; not final feature evidence):
The 0.6.51 baseline at `eef99a3` passed
`ctest --test-dir build/mac-check --output-on-failure -R '^opengold_(damage|training)_tests$'`
(2/2, 0.70 seconds, 21:34 UTC). This is baseline evidence, not feature completion.

Remaining: obtain ROGUE-DEMO-1 approval, implement only that demo row, run its
input/render/native continuation checks and affected regression. Then deliver
#114. Full Rogue completion #116 remains separate. The approved Sneak controls
and advancement require no further permission.

### Delivery checkpoint — 2026-09-25 22:26 UTC

Runtime7f3ab30 and evidence0d7f803 pushed to `codex/srd-rogue-attacks`.
#220/#221/#112 verified CLOSED; #114/#116 OPEN. One of two original features
is delivered, three corresponding issues closed, zero added, about58 minutes
from original preflight. Main is unchanged while this batch's demo placement
remains pending. Goal stays active. No live build/test remains. ROGUE-DEMO-1 is
asked as visible question1 with Glass audio; continue the same batch after its
answer, without resetting its original clock.

### 60-minute follow-up

At22:28 UTC the demo placement answer remained pending. Independent remaining
Steady Aim acceptance was exercised: unused Advantage expires before the next
turn's actual hit, and an actual aimed miss clears the saved attack-roll benefit
while preserving spent Action/Bonus Action and zero Speed. This is verification
inside the original batch, without new mechanics or UI choices. Native training
was rebuilt and passed after the final assertion; see `/tmp/rogue-aim-expiry-tests.log`.

Approval received; implementation resumed at observed00:02:32 UTC on2026-09-26.
Original batch start/checkpoint retained; elapsed includes the approval wait.
Requested Astra/high unchanged, actual configuration unverified.

### Approved demo completion — 2026-09-26

ROGUE-DEMO-1 is implemented with the exact approved row and44px reservation.
Godot demo build `cmake --build build/sprite-demo --target opengold_godot -j6`
passes. The training target was rebuilt; `--rogue-attacks` passes and produces
independent Dash/Disengage/Aim UI result fixtures. Expanded
`rogue_attack_view_tests.gd` passes in demo EN and main EN/ES at both sizes,
including keyboard dropdown selection and Use activation, exact native command
results for all three choices, shared-budget disabled states and no row/log/footer
overlap. Demo neighboring thrown/pickup controls also pass keyboard/mouse and
both-size renders. No native rules or save format changed in this final step.

Logs: `/tmp/rogue-demo-bonus-build.log`, `/tmp/rogue-bonus-fixture-tests.log`,
`/tmp/rogue-demo-bonus-tests.log`, `/tmp/rogue-demo-neighbor-tests.log`,
`/tmp/rogue-main-bonus-tests.log`. Inspected captures:
`/tmp/rogue-demo-bonus-final/en-1120-aim.png` and `en-1920-aim.png`;
main captures in `/tmp/rogue-main-bonus-final`. The earlier78-check integrated
runtime pass and both actual advancement comparisons remain current: only demo
presentation and expanded test fixtures/assertions changed afterward.

Both original features (#112, #114) now meet this batch's full acceptance.
#116 remains open; Thief, Hide and mastery are still excluded and required.

Final demo runtime: `8ee62fe`; verified at00:06:47 UTC,4m15s after the
approval continuation was observed. Original elapsed2h38m51s includes the
approval wait; no claim of measured token savings. Both original requirements
are delivered, with four existing issue closures total and no new issues.

## Thief (level 3)

Rules 0.6.110 records the Thief, the SRD's only Rogue subclass, at Rogue level
three: `subclass:thief` with `feature:fast_hands` and
`feature:second_story_work`, granted on advancement and required by grant
validation. Neither feature has a use yet. Fast Hands' lock picking only
matters in combat, where there are no locks, and its Use an Object waits for
magic items (a wand or similar used as a Bonus Action is its future use).
Second-Story Work's climbing and jumping need vertical movement the battle grid
does not have. The level-up note says so. Verification: the Rogue progression
checks in `opengold_training_tests` (rogue_attack_checks.h) find the three grants from level three
only.
