# Weapon mastery delivery packet

## Frozen batch

- Active user goal; one owner, no agents or new issues.
- Deliver all eight mastery properties through actual chosen weapon kinds, with
  creation, advancement, Long Rest replacement, combat and retained saves.
- Original issues: #60 and mastery acceptance of #85/#140/#147/#111/#103.
  Target closure #60/#85; other parent issues close only if their remaining
  original acceptance is independently complete. Do not equate mastery with
  whole-class completion or add missing unrelated class features.
- Current source routes: Fighter/Rogue/Paladin/Ranger levels1–4, Barbarian level1.
  Fighter gains its fourth kind at4. Barbarian's third kind at4 remains part of
  its existing unavailable progression (#104–109), not a new progression batch.
  Higher levels and multiclass remain required in the original goal.
- No starting-equipment packages, ammunition tracking, other conditions/classes,
  unrelated fixes, new UI framework or reduced historical compatibility.
- Requested Astra/high for combat trigger interactions and persistence; actual
  configured model/effort unverified. No model/runtime setting changes. Escalate
  unresolved architecture/rule interpretation and two failed fixes; attempts0.
- Preflight began03:56:03 UTC2026-09-26. Checkpoint04:56:03, latest05:26:03.
  Earlier context-reset boundary is retained; no timing reset.

## Source and acceptance

Official [SRD5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
printed pp28–29,47–48,54,58–59,62 and90, checked2026-09-26.

| Source | Chosen kinds in this batch | Eligible weapons | Long Rest replacement |
| --- | --- | --- | --- |
| Barbarian1 |2|Simple/Martial Melee|At most one kind|
| Fighter1–3 /4 |3 /4|Simple/Martial|At most one kind|
| Paladin1–4 |2|Proficient kinds|Either/both kinds|
| Ranger1–4 |2|Proficient kinds|Either/both kinds|
| Rogue1–4 |2|Proficient kinds|Either/both kinds|

1. Validate counts, duplicate/unknown/unproficient kinds, source and acquisition
   levels in SRD. All38 ordinary catalog weapons are eligible where class rules
   allow; the compatibility Wand is not a weapon choice. Presets pre-generate
   choices; prior saves leave missing selections pending. Existing training,
   wounds, resources, equipment and advancement survive completion/replacement.
2. Cleave: after a melee hit, one additional same-weapon melee attack against a
   different creature within5 feet of the first and the attacker's reach, once
   per turn. Retain negative ability damage; omit positive modifier.
3. Graze: optional miss damage equal to the attack ability modifier, weapon damage
   type; no added weapon/Sneak/style dice or other damage increases. Apply relevant
   typed defenses and zero-damage behavior.
4. Nick: the Light extra attack can use its qualifying Attack action instead of
   the Bonus Action. Share the one extra-attack-per-turn limit with Light; retain
   different physical weapon identity and Two-Weapon Fighting behavior.
5. Push: optional straight-away forced movement up to10 feet, Large or smaller;
   legal reachable destinations, collision and no Opportunity Attack. Declining
   does not refund the triggering attack. Preserve actual occupied cells.
6. Sap: next attack roll has Disadvantage before the source's next turn starts.
   Slow: optional damage-triggered10-foot Speed penalty until that same boundary;
   multiple Slow sources cannot increase its penalty; compose with other effects.
7. Topple: optional Constitution save, DC8+attack ability modifier+proficiency;
   failure Prone. Vex: on damage, source's next attack against this target has
   Advantage before the end of the source's next turn. Consume/expire effects at
   exact boundaries; use shared Advantage/Disadvantage cancellation.
8. Cover misses/hits/zero damage/critical, melee/ranged/thrown/reaction, Action
   Surge, Savage Attacker, Sneak Attack, Champion movement, sizes/obstacles,
   incapacitation/death, PC/NPC ownership, rejected-command atomicity and all
   pending-decision continuation. No mastery from catalog metadata alone.
9. SRD owns grants, outcomes, expiry, targeting and codecs. Core orchestrates
   transactions through generic rules interfaces; UI only presents queries.
   Preserve all released save formats; capture actual0.6.55 writer before edits.
10. Verify normal creation, Review Training, fourth Fighter choice, camp/inn rest
    replacement/save/reload and combat. MainEN/ES and demoEN at1120×800/1920×1080,
    keyboard/mouse, focus, disabled states and Cancel/Escape. Focused native tests,
    then final integrated regression on freshly built targets. Commit/push and
    update coverage before closing only fully proven original issues.

## Approved controls

**MASTERY-1.** Reuse scrollable Training checkbox groups for Weapon Mastery in
creation and Review Training, listing weapon plus mastery name and selection
count. Require class-appropriate counts; preserve Back, lock prior selections in
Review Training, pre-generate presets, keep missing old-save choices pending.
For Fighter4 add a labeled fourth-weapon dropdown below Fighting Style in the
existing Level Up dialog (label y374, selector y406, width652, height38); Confirm
requires it. Cancel/Escape changes nothing; sheet lists selected weapon kinds.

**MASTERY-2.** After a completed Long Rest, use a centered700×670 Weapon Mastery
window for each eligible member, following existing spell choices. Reuse a
scrollable labeled checkbox list, current selections and count/replacement limit.
Keep current/Apply buttons; Escape keeps current; Apply requires a legal complete
set. Choices finish before exploration resumes; camp/inn saving preserves a
pending choice. No combat saving. Interrupted/ineligible rests do not grant a
replacement window; each completed rest grants only one.

**MASTERY-3.** Reuse a centered combat decision dialog for optional Graze, Slow,
Topple, Cleave and Push: show the property/effect and Use/Skip buttons. Cleave's
Use enters existing highlighted creature targeting; Push's Use highlights legal
landing cells up to10 feet directly away. Click/arrows confirm; Escape or Skip
(or the temporarily relabeled End Turn button) declines. Other actions wait;
original Action/Reaction stays spent. Sap/Vex apply automatically as specified.
The dialog is shown only when an optional effect can change the result. All
controls retain keyboard access and standard button styling. No combat saves.

**MASTERY-4.** Add Nick attack beside End Turn, in the otherwise unused ordinary
turn position occupied by Decline during reactions (x208,w174,h36). Show for an
actor with Nick; disable until legal. It opens a centered640×300 selector naming
eligible weapon and melee/ranged/thrown attack, followed by Target/Cancel.
Target uses existing highlighted targeting and keyboard cycle; Escape cancels
before submission. It spends no Bonus Action and shares Light's one-extra-attack
limit. Existing Light Bonus Action choices remain available where legal.

## Evidence and phase record

Preflight confirms mastery currently exists only as weapon catalog metadata;
no runtime mastery grants/effects are implemented. The existing Training,
advancement, rest-spell and combat weapon controls provide reuse routes above.
No production source or UI change is authorized by these proposals alone.

Compatibility preparation verified04:05:58 UTC. Production remains69771eb /
rules0.6.55. Five genuine fixtures and exact replay checks are recorded in
[fixture provenance](../tests/fixtures/README.md#weapon-mastery-baseline--actual-0655-writer).
`opengold_training_tests --mastery-baseline` and the full rebuilt training target
pass; logs `/tmp/mastery-baseline-build.log` and `/tmp/mastery-baseline-tests.log`.
Only tests/documents changed, so no game rebuild or repeated integrated run.
One compile correction (unique ownership) and one fixture correction (two-NPC
party limit); neither failure repeated. No original issue closed by preparation.
All four controls subsequently approved by “1. Yes. 2. Yes. 3. Yes. 4. Yes.”
Implementation now proceeds within those boundaries.

### Acquisition implementation checkpoint (WIP)

By04:39 UTC the acquisition paths and pure replacement policy were implemented.
Focused grant/training/advancement/weapon checks pass. MainEN/ES and demoEN
Training, Review Training and Fighter4 ASI checks pass at both supported sizes;
five UI saves match native expectations. Artifacts/logs use `/tmp/mastery-*`.
All50 native targets rebuilt; first regression41/51 passed. Ten older tests need
updated complete-training inputs or conditional profile assertions; compact old
rest-format fixtures retain explicitly pending training. Corrections in progress.
Combat effects and actual Long Rest replacement remain outstanding. This is not
batch delivery, and no original issue has been closed. Timing remains03:56:03
start,04:56:03 checkpoint; no reset. Token/cost measurement unavailable.

Acquisition verification recorded04:44:04 UTC (observed clock; final log records
4.05s test duration): all51 native checks pass after rebuilding50 targets.
`ctest --test-dir build/mac-check --output-on-failure -E '^opengold_godot_' -j6`
produced `/tmp/mastery-native-final.log`; targeted correction builds are in
`/tmp/mastery-regression-*-build.log`. Archery legacy rejection tests deliberately
omit mastery grants so the original Archery boundary remains the cause of failure.
Review found no SRD calculation in Core/UI. Localization972 and diff checks pass.
This acquisition checkpoint is committed on the feature branch; full combat/rest
acceptance and issue closures remain pending.

### Additional combat decisions (approved)

**MASTERY-5.** When mastery and Champion movement trigger simultaneously on a
player-controlled turn, add a labeled Resolve next dropdown above the approved
Use/Skip buttons in the same centered decision dialog. Each entry names an
eligible pending effect. Use/Skip handles only the selected entry, retaining the
others; existing movement/targeting controls resolve the chosen effect. Retain
separate critical-hit movement entitlements from a Cleave critical. Keyboard
access, no action refund, other actions waiting and no combat saves are unchanged.
On enemy turns the game chooses a fixed mastery-before-movement order. Required
UI confirmation under AGENTS.md; simultaneous effects are explicitly within
acceptance row8, so this does not add unrelated trigger systems.

**MASTERY-6.** Proposed Graze interpretation: its ability modifier is the upper
limit on damage. Resistance/Immunity can reduce it; Vulnerability cannot increase
it. For modifier+3, ordinary/vulnerable damage3, resistant1, immune0. This resolves
the wording that Graze damage increases only with its ability modifier against
the general Vulnerability doubling rule. Approved by MASTERY-6 on2026-09-26.

Sources: SRD5.2.1 p90 (Graze) and official
[Simultaneous Effects](https://www.dndbeyond.com/sources/dnd/br-2024/rules-glossary#SimultaneousEffects).

### 60-minute checkpoint —04:56:24 UTC

Elapsed60m21s from03:56:03. Acquisition committed/pushed as `5b978df`; no original
issue closed. Rest replacement transaction, history replay, conditional campaign18
and approved dialog implemented in WIP. Native five-class PC/NPC tests pass,
including exact prior acquisition-writer replay and later training/advancement.
Combat effects remain unimplemented; MASTERY-5/6 await answers. No scope added.
Live dialog verification exposed checkbox focus loss during list refresh; one
test-timing adjustment did not fix it. The identified cause is hiding/re-showing
all checkboxes during refresh; fix only hides obsolete options. Rebuild and
keyboard verification next. Native API change necessitates final integrated
rebuild; current rest build logs `/tmp/mastery-rest-*-build.log`. No measured
token/cost savings claimed; no new agents/tasks or changed model settings.

### Long Rest implementation evidence (WIP)

Generic RulesModule replacement options/outcomes now drive per-member completed
rest windows. SRD owns eligible kinds, capacity, source preservation and limits.
Core records level/rest-session edits, replays them between advancement records,
and blocks exploration until choices finish. Campaign18 is conditional on a
pending training window or replacement history; older1–17 formats remain accepted.
Review Training adds missing unrelated choices without reverting replacements.
Keeping current consumes the same one-use window and is recorded in history.

Native `mastery_rest_checks.h` passes for all five class routes and PC/NPC owners:
Short/interrupted/ineligible/dead/reserve exclusions, stale/duplicate/invalid
commands, atomic previews, preservation, repeated rests, level1-to4 replay and
Review Training after replacement. The actual prior acquisition writer fixture
round-trips byte-for-byte; see fixture provenance. Final native regression awaits
the50-target rebuild, `/tmp/mastery-rest-native-build.log`.

The shared700×670 dialog and `mastery_rest_view_tests.gd` pass mainEN/ES and demoEN
at1120×800 and1920×1080. Five eligible members follow Wizard spell choices;
keyboard selection/Apply/Escape, sequential windows and pending-codec reload pass.
Screenshots are `/tmp/mastery-rest-{main,demo}-{en,es}-{1120,1920}/`; only main has
Spanish runs. Initial runs exposed a checkbox focus bug (fixed by retaining
visible controls) and test-startup overrides of locale/size (fixed in the script).
Corrected minimum-size Spanish screenshot was inspected; dimensions verified.
Existing full rest-control acceptance passes (`/tmp/mastery-rest-neighbor.log`).
976 localized messages and diff validation pass. These tests verify pending save
serialization, not yet an accessible Save button inside the modal.

**MASTERY-7 (approved2026-09-26).** The approved exclusive modal covers the
existing Save game control. To make unresolved choices saveable through the
player UI, place a standard Save game button at its bottom left (x24,y614,w234,
h40), beside Keep current/Apply. Reuse the existing camp/inn save dialog; return
to the same pending mastery choice after saving or Cancel, with keyboard access.
Saving must not apply or discard the choice; loading retains its entitlement.
No combat saving. This is needed to complete acceptance row10 and MASTERY-2's
pending-save behavior; approved for implementation.

Rest verification recorded05:04:27 UTC. Final rebuilt native suite **51/51 PASS**,
4.03s, `/tmp/mastery-rest-native-final.log`. The only initial native regression
failure was the Temporary HP test attempting combat while a new rest choice was
pending; it now verifies rejection, explicitly keeps training, then completes its
original pool-preservation assertions. No production behavior weakened. Corrected
UI runs and existing rest controls pass as described above. Scope/architecture
review confirms rule outcomes stay in the static SRD implementation and Core/UI
only orchestrate/present them. Checkpoint commits do not close #60/#85; combat and
pending Save control approval remain outstanding.

### Sap and Vex combat checkpoint

The static SRD library now applies chosen Sap/Vex properties through the shared
weapon-hit resolver, including selected physical weapons, thrown/Light attacks
and reactions. Sap needs a hit; Vex needs positive damage after typed defenses.
Shared attack modifiers apply/consume Sap on any next attack roll and Vex only
on its scoped source's next roll against that target, including attack spells.
Timers expire at the source's next start (Sap) or next end (Vex), even when
applied during another creature's turn. Reapplication refreshes a source's effect;
other sources retain independent provenance. No mechanic moved into Core/UI.

A pending Savage/Sneak damage choice retains the original effects until its hit
resolves, preserving independently validated roll mode on reload. Resolution
consumes those effects before applying the new hit's mastery. Champion movement
continues afterward. This checkpoint introduces no optional simultaneous-effect
control and does not resolve pending MASTERY-5/6/7.

Conditional FX6 records these two effects; older codecs remain canonical when
neither is present. Legacy module identities reject FX6 in combat and campaign
state. A regression test caught and fixed the combat identity check initially
running before effects were read. Capacity checks reject before spending actions
or RNG; same-source refresh and consumed Vex slots remain usable at the limit.

`tests/mastery_combat_checks.h` exercises every Sap/Vex weapon and supported
attack mode, hits/misses/criticals/immunity, chosen-kind gating, unarmed fallback,
source/scope separation, exact expiry, cancellation, spell attacks, PC/NPC Light
and Surge, reactions, Sneak/Savage/Champion continuation, codec rejection and
bounded storage. The existing status-effect target runs it. Set
`OPENGOLD_MASTERY_FIXTURES=/tmp/mastery-combat-fixtures` when running that target
to export actual production-content before/after checkpoints for the shared UI
harness `tests/mastery_combat_view_tests.gd`. UI uses existing attack controls and
localized sourced conditions; no new layout. Six mastery properties remain.

Verification at05:31 UTC2026-09-26: freshly rebuilt native **51/51 PASS** (13.01s),
`/tmp/mastery-sap-vex-native-final.log`; build50 native targets plus game test
project `/tmp/mastery-sap-vex-native-build.log`, standalone demo
`/tmp/mastery-sap-vex-demo-build.log`.980 localized messages and diff checks pass.
MainEN/ES and demoEN UI/native equality passes at1120×800/1920×1080 using
`mastery_combat_view_tests.gd -- --mastery-fixtures=/tmp/mastery-combat-fixtures
--mastery-captures=/tmp/mastery-combat-main` (add `--mastery-demo` for demo).
Logs `/tmp/mastery-sap-vex-{main,demo}-ui.log`; rendered captures
`/tmp/mastery-combat-{main,demo}`. Small Spanish/main and English/demo captures
were inspected. Main uses the real A action cycle; demo uses its visible Melee
button's Space activation. Both use battlefield target clicks and verify the
visible combat log, exact native state and retained checkpoint continuation.

The harness initially assumed Enter activated an ordinary combat button and
that the main legacy roster was visible. Source/renderer inspection corrected
those assumptions: Enter ends the turn; main status is in the scrollable Log.
No unrelated input/layout behavior was changed. The demo lacks main's A cycle,
so its existing standard button keyboard path is tested instead. Detailed sourced
condition data is supplied to snapshots; this checkpoint does not claim a new
visible condition panel. All mechanics remain inside `opengold_rules_srd5`.

Elapsed batch time at this checkpoint is about95 minutes from03:56:03; the
required60-minute checkpoint above remains the same. Two of eight combat
properties are implemented and tested, acquisition/rest paths were delivered
previously, and zero original issues are closed in this batch. Intermediate
implementation/build phase durations were not recorded separately; native suite
runtime is measured above. No scope, new issue, agent or setting change. Goal
remains active; next independent work is the remaining approved properties.
Final review strengthened the campaign legacy-effect test by first proving that
the identical state is valid under the current identity; rejection cannot be
caused by mismatched HP. Rebuilt status-effect target passes after that test-only
change (`/tmp/mastery-sap-vex-final-test-build.log`). Production sources remain
the same tree that passed the full51 native checks and both UI runs.

### Nick combat checkpoint

Nick uses the actual extra attack's selected weapon kind and physical identity.
Daggers, Light Hammers, Sickles and Scimitars have this property; the source must
have chosen that kind. The SRD resolver shares Light's damage treatment, thrown
ledger, hit choices and once-per-turn allowance. Nick does not spend or restore a
Bonus Action. Taking ordinary Light prevents Nick, taking Nick prevents ordinary
Light, and Action Surge never supplies a second shared extra attack.

Nick remains part of the current Attack action: movement and unrelated Bonus
Actions preserve it; a later non-Attack Action closes it. The ordinary Light Bonus
Action may still be taken later that turn. A subsequent qualifying Attack action
records its own weapon, so Nick cannot use the same weapon merely because a
different weapon qualified during an earlier Action Surge action. These outcomes
stay in `opengold_rules_srd5`; UI consumes supplied options and legal commands.
Source checked: [Nick](https://www.dndbeyond.com/sources/dnd/br-2024/equipment#Nick)
and [Attack action](https://www.dndbeyond.com/sources/dnd/br-2024/rules-glossary#AttackAction).

Rules identity0.6.57 introduces conditional combat23 for Nick's explicit shared
allowance and qualifying physical weapon. Formats1–22 remain accepted. Actual
pre-Nick writer0.6.56 fixtures are documented in
[fixture provenance](../tests/fixtures/README.md#nick-shared-budget-baseline--actual0656-writer).
Old in-progress turns remain exact: their Bonus Action flag cannot distinguish
Light from Second Wind. Nick activates at the first fresh turn boundary, after
those historical budgets and pending damage choices have resolved normally.

Approved MASTERY-4 controls use the existing unused ordinary-turn Decline slot
(x208 in main; the corresponding slot in demo), a640×300 labeled selector,
Target/Cancel, mouse and keyboard targeting and Escape cancellation. The modal
presents rules-provided eligible physical weapons and attack modes. The Nick
button stays visible and disabled after expenditure; no combat saving control.
MainEN/ES and demoEN pass at1120×800/1920×1080, including exact UI/native
state, disabled/spent controls, keyboard/mouse, cancellation, thrown Nick and
Nick after an unrelated Bonus Action. The existing Light UI/native checks also
pass on both surfaces at both sizes. Captures `/tmp/nick-{main,demo}` and
`/tmp/nick-light-{main,demo}`; logs `/tmp/mastery-nick-{main,demo}-ui.log` and
`/tmp/mastery-nick-light-{main,demo}.log`.

The final integrated native suite passes **51/51 in8.78s**, with all50 targets
rebuilt after production edits and affected targets rebuilt after test corrections.
Commands: `cmake --build build/mac-check --target <native targets> -j6`, then
`ctest --test-dir build/mac-check --output-on-failure -E '^opengold_godot_' -j6`.
Logs `/tmp/mastery-nick-native-final-build.log`,
`/tmp/mastery-nick-interactions-build.log`, `/tmp/mastery-nick-native-final.log`.
Game/demo builds are `/tmp/mastery-nick-{selection,demo-selection}-build.log`.
UI scripts are [Nick](../tests/nick_view_tests.gd) and
[existing Light](../tests/light_attack_view_tests.gd); use their fixture/capture
arguments with the corresponding main/demo project and `OPENGOLD_GAME_DIR`.
Localization validates988 messages; diff/scope/architecture review passes.

[Native tests](../tests/nick_attack_checks.h) exercise768 weapon/style/ability/seed
combinations plus all five PC/NPC class routes, actual historical continuation,
invalid/stale inputs, Surge timing, physical thrown items, Sneak/Savage choices,
Vex consumption and Champion movement. Regression corrections retained malformed
save rejection and exact historical-body comparisons while accounting for the new
identity/checksum and appended fields. The initial integrated run passed48/51;
further focused checks exposed one additional old tail-field assumption and one
new test that expected Vex consumption before pending damage resolved. Both were
test corrections; no production change was needed in this final verification phase.

Observed verified checkpoint06:06:09 UTC2026-09-26, **2h10m06s** after the original
batch preflight. The60-minute checkpoint above was not reset. Individual earlier
Nick implementation/build phase times were not recorded separately; tokens/cost
unavailable. This commit completes the third of eight combat properties, with
acquisition/rest transactions already delivered. Five properties and pending
MASTERY-5/6/7 remain; zero original issues closed. No scope expansion, new issues,
settings changes or delegation. The goal remains active.


### Optional choice compatibility preparation

Observed06:10:34 UTC2026-09-26. Production remains0bba07e/rules0.6.57. Actual
Longbow/Maul Champion critical checkpoints now cover the attack, Savage decision,
free movement and settled state; all eight round trips and three-step replays
match byte-for-byte. Rebuilt `opengold_status_effect_tests` passes; logs
`/tmp/mastery-choice-baseline-{build,capture,test}.log`. This changes tests and
provenance only, with no new playable property or issue closure. See
[fixture provenance](../tests/fixtures/README.md#optional-mastery-choices-baseline--actual0657-writer).

MASTERY-5/6/7 were re-presented as visible questions1–3 with the Glass alert
(exit0); no answer has arrived. Integration of simultaneous mastery/Champion
choices needs MASTERY-5. Graze policy and rest Save control need6/7. Keep those
pending; do not infer their approval from the earlier four-item response.


### Slow effect lifecycle checkpoint (combat integration incomplete)

Rules0.6.58 adds sourced Slow state and conditionalFX7 in the existing SRD
library. Repeated Slow sources cap at10 feet; Ray of Frost contributes its
separate10 feet. Shared movement, Action/Bonus Dash, standing, Steady Aim,
chronological expiry and campaign/combat persistence consume this state. Native
snapshot messages distinguish Slow from Frost and retain sources. No Core
calculation or new UI control was added. Optional hit choices are **not connected**;
this does not complete Slow or increase the three-of-eight playable count.

Source: [Slow](https://www.dndbeyond.com/sources/dnd/br-2024/equipment#Slow), checked
2026-09-26. Existing source-turn timing supplies the future trigger's duration;
this checkpoint proves the effect clock and consumers, not that trigger.
Actual0.6.57 fixtures load and replay with only module identity changed. Older
identities reject forged Slow state; all supported older formats remain accepted.

[Focused tests](../tests/slow_mastery_checks.h) prove nonstacking, independent
source expiry, refresh/storage bounds, malformed durations/codecs, exact replay,
combined Speed/Dash/posture behavior and campaign time chunking. All50 native
targets rebuilt; **51/51 native PASS in12.43s**. Logs
`/tmp/mastery-slow-lifecycle-{build,test}.log` and
`/tmp/mastery-slow-native{,-build}.log`. Verification command remains
`ctest --test-dir build/mac-check --output-on-failure -E '^opengold_godot_' -j6`.
989 messages/diff checks pass. No new Godot control or UI verification is claimed;
main/demo binaries still correspond to the prior completed Nick checkpoint and
must be rebuilt for the forthcoming integrated optional-choice work.

Observed verified06:19:44 UTC2026-09-26,2h23m41s since original preflight; original
60-minute checkpoint retained. This continuation began after compatibility
preparation06:10:34; exact phase subintervals/token costs unavailable. No failed
fixes in this checkpoint. MASTERY-5/6/7 remain unanswered. Goal remains active,
zero new issues or original closures, with the same frozen acceptance.

### Approval continuation —2026-09-26 13:59:56 UTC

The user approved MASTERY-5/6/7 together. All seven mastery decisions now have
approval; historical pending statements above describe earlier checkpoints.
Continuation retains Astra/high assignment (actual configuration unverified),
fixed acceptance, one owner and no delegation. The blocked waiting interval
06:29:28–13:59:56 was7h30m28s, not implementation time. Original batch timing
and its recorded60-minute checkpoint are retained.

### Pending mastery Save control

MASTERY-7 implemented in the shared rest dialog; main and demo use the same
existing save callbacks. Draft selections stay in the dialog, committed training
and the pending entitlement stay in the campaign. Saving/Cancel returns to the
same draft; loading restores the committed selections and pending entitlement.
The button is hidden outside a hosted campaign and disabled at unsafe boundaries.
No combat saving, mechanics, save schema or compatibility changes.

Verification on the commit containing this entry (base5b0e9ed):

- `cmake --build build/mac-check --target opengold_feature_grant_tests opengoldbox_test_project -j6`
  and `cmake --build build/sprite-demo --target opengold_godot -j6`.
- `ctest --test-dir build/mac-check --output-on-failure -R '^opengold_(feature_grant|campaign_rest|save)_tests$'`:3/3 PASS,0.47s.
- Generate a current UI fixture with `OPENGOLD_GAME_DIR=/Users/edmond/POOLRAD OPENGOLD_MASTERY_REST_FIXTURE=/tmp/mastery-rest-pending.ogs build/mac-check/opengold_feature_grant_tests`.
  It completes the original town introduction, grants a completed rest and validates
  decoding against the original town resources. It is not a historical fixture.
- `tests/mastery_rest_save_view_tests.gd` via `tests/run_godot_test.cmake`,
  `--style-fixture=/tmp/mastery-rest-pending.ogs`; demo adds `--style-demo`.
  MainEN/ES and demoEN at both supported sizes PASS. Keyboard Save, Cancel,
  overwrite, reload, unapplied replacement preservation and single Apply covered.
  Logs `/tmp/mastery-save-{main,demo}-ui-final.log`, rendered captures
  `/tmp/mastery-save-{main,demo}/`. No engine errors in final runs.
- Existing `opengold_godot_rest` and `mastery_rest_view_tests.gd` pass; existing
  tests retain actual camp flow and sequential per-member replacement coverage.
-989 messages/localization and diff checks pass. Review confirms the change is
  presentation and test fixture setup only; SRD outcomes remain in the library.

Continuation implementation/build/verification13:59:56–14:10:52 UTC,10m56s. No new scope/issues. Remaining
combat acceptance is unchanged; playable property count remains3/8.

### Graze playable checkpoint

MASTERY-3/6 implemented for Glaive and Greatsword misses, ordinary attacks and
Opportunity Attacks. Use/Skip and Escape retain the spent Action/Reaction; other
actions wait. Damage uses the actual attack ability modifier, floored at zero,
with the weapon's damage type. Resistance and Immunity reduce it; Vulnerability
is ignored even when Resistance is also present, so it cannot undo a reduction.
No weapon/style/Savage/Sneak/critical dice are added. Rules supply the preview,
commands, damage and continuation; Core follows generic optional-choice ordering,
and the shared Godot dialog renders module messages.

Rules0.6.59 uses conditional combat24 only while Graze is pending. It retains
Light/Nick activation flags instead of activating those budgets merely because
of the newer container. Old formats1–23 and module0.6.58 remain supported.
A real0.6.58 writer fixture with Slow is captured and retained, in addition to the
existing prior-writer mastery fixtures; see the fixture README.

Verification on the commit containing this entry (basea0514a9):

- `graze_checks.h` runs80 weapon/class/ownership/defense combinations, including
  combined Resistance/Vulnerability; negative/zero/positive modifiers and Great
  Weapon Fighting; physical PC weapons and Fighter/Paladin/Ranger levels1–4;
  Action Surge, missed reactions/interrupted movement, stale/malformed commands,
  absent grants, no hit-only features, exact pending/settled persistence and
  actual previous-writer continuation.
- All50 native executable targets rebuilt. Final relink pass40.05s;
  `ctest --test-dir build/mac-check --output-on-failure -E '^opengold_godot_' -j6`
  **51/51 PASS**,18.21s. `/tmp/graze-native-regression.log`.
- Main and demo rebuilt. `graze_view_tests.gd` via the standard CMake wrapper,
  `--nick-fixtures=/tmp/graze-fixtures` (demo also `--nick-demo`), passes mainEN/ES
  and demoEN at1120×800/1920×1080. Actual attack, keyboard/mouse Use, Escape,
  pending reload, focus/disabled controls and exact native/UI continuation pass.
  Fixtures are generated by `OPENGOLD_GRAZE_FIXTURES=/tmp/graze-fixtures build/mac-check/opengold_status_effect_tests`.
  Logs `/tmp/graze-{main,demo}-ui-final.log`; rendered captures `/tmp/graze-{main,demo}/`.
- Neighboring `opengold_godot_savage` and `opengold_godot_champion`, excluding the
  already prepared Godot fixture, pass with their native prerequisites4/4.
-993 messages/localization and diff checks pass. Scope/architecture reviewed:
  no SRD calculation in Core/UI, no new combat saving controls, no new issue.

Observed delivery checkpoint14:35:13 UTC:35m17s since approval continuation,
including the earlier rest Save control delivery. Per-build times not separately
measured are unavailable. Playable mastery count is4/8. Slow/Topple hit choices,
Cleave, Push and MASTERY-5 simultaneous ordering remain; no full issue closure.

### Approval continuation checkpoint

Observed2026-09-26 14:58:14 UTC:58m18s since13:59:56 continuation;
original batch timings above remain unchanged. Rest Save and Graze were committed
and pushed (a0514a9,a038d91); four of eight properties verified playable, zero
original issue closures. Slow/Topple/Cleave/Push and simultaneous-effect controls
are implemented locally; reaction/save/UI acceptance remains in progress. The
first optional weapon matrix and both Slow/Topple versus Champion orders pass.
No added scope/issues; no token/cost measurements available. Requested Astra/high,
actual configuration unverified; one owner, no delegation. This checkpoint does
not pause work or claim the remaining properties delivered.

## Optional hit mastery integration

Rules0.6.60 connects Slow, Topple, Cleave and Push to actual weapon hits. Slow
requires damage and expires at the source's next turn; Topple rolls Constitution
against8 + attack ability modifier + proficiency. Cleave uses the same weapon,
keeps negative damage modifiers, omits positive ability damage and shares one
use across the turn, including reactions. Push offers unobstructed, directly-away
landing cells within10 feet, for Large or smaller targets, without Opportunity
Attacks or normal movement expenditure. Supplemental creature size metadata only
supports that eligibility check; general footprints/transit remain #44/#45.

The approved Use/Skip dialog suppresses futile effects. Multiple pending effects
use Resolve next, preserving each unchosen entitlement; Cleave criticals can
produce a second separate Champion movement. Position-dependent effects are
re-evaluated after movement. Enemy-turn ordering is mastery before movement.
Cleave targets include allies; automatic combat declines if only allies remain.
Push and Cleave reuse highlighted mouse/keyboard targeting with Escape/Skip effect.
No combat save controls are introduced.

The SRD session owns the choices, geometry, damage/save resolution and interrupted
movement. Core consumes generic effect choices/targeting; shared Godot controls
present them. Conditional combat25 retains pending hit permissions, mastery
choices, separate Champion entitlements, Cleave expenditure and interrupted-route
origins/budgets. Prior supported versions remain readable; actual0.6.57 pending
hits retain their prior continuation, while subsequent fresh attacks can offer
new properties. Actual0.6.59 positive/zero Graze fixtures remain valid. New
zero-damage Graze prompts are suppressed without discarding historical choices.

Tests: [optional_mastery_checks.h](../tests/optional_mastery_checks.h) and
[optional_mastery_view_tests.gd](../tests/optional_mastery_view_tests.gd), alongside
existing grant/rest/mastery/Graze/Nick/lifecycle tests. The new cases cover every
eligible weapon/range route, immunity, Push size/collision boundaries, deterministic
Topple saves, thrown provenance, signed Cleave damage, allies, Savage rerolls,
critical ordering, movement enabling effects, dead interrupted movers, retained
formats and malformed/rejected choices. UI fixtures are real native outcomes;
keyboard/mouse/dialog/targeting outcomes are compared byte-for-byte against them.

Final verification commands (logs below are local build artifacts):

```bash
cmake --build build/mac-check --target opengold_status_effect_tests -j6
OPENGOLD_OPTIONAL_MASTERY_FIXTURES=/tmp/optional-mastery-fixtures \
  ctest --test-dir build/mac-check --output-on-failure -E '^opengold_godot_' -j6
cmake --build build/mac-check --target opengoldbox_test_project -j6
cmake --build build/sprite-demo --target opengold_godot -j6
OPENGOLD_GAME_DIR=/Users/edmond/POOLRAD OPENGOLD_LANG=en cmake \
  -DGODOT=/Applications/Godot_mono.app/Contents/MacOS/Godot \
  -DPROJECT=/Users/edmond/src/OpenGold/src/OpenGoldBox/godot \
  -DSCRIPT=/Users/edmond/src/OpenGold/tests/optional_mastery_view_tests.gd \
  '-DARGS=--nick-fixtures=/tmp/optional-mastery-fixtures;--nick-captures=/tmp/optional-mastery-main' \
  '-DEXPECTED=Optional mastery controls passed' -DGRAPHICAL=ON -DTEST_TIMEOUT=120 \
  -P tests/run_godot_test.cmake
python3 tools/localization.py --check
git diff --check
```

All native executables must be rebuilt before the regression command. The demo
uses the same wrapper with project `demos/godot` and argument `--nick-demo`.
English/Spanish main and English demo checks exercise1120×800 and1920×1080.

Final integrated verification (commit containing this record):50 native test
executables rebuilt, **51/51 native checks PASS**,16.84s;
`/tmp/mastery-native-regression-final.log`. MainEN/ES and demoEN controls pass
both sizes against final native fixtures; logs `/tmp/mastery-main-ui-final.log`
and `/tmp/mastery-demo-ui-final.log`, captures `/tmp/optional-mastery-main-final`
and `/tmp/optional-mastery-demo-final`. Keyboard Resolve next, Use, target cycling,
Escape, mouse ally/landing selection and historical pending continuation match
native outcomes.1005 messages validate; diff checks pass. Main's import step
emits the same Godot Mono editor diagnostics already present in the Graze build
(invalid saved editor-window position/debugger timer); runtime UI logs are clean.
No main-branch merge or release integration is claimed.

Neighboring Savage Attacker/Champion controls and their native prerequisites also
pass4/4 in3.45s (`/tmp/mastery-neighbor-ui-final.log`). Main Spanish ordering and
demo Push targeting captures were visually inspected. Final scope/architecture
review found no mechanics moved into Core/UI and no added issue or feature scope.

Delivery: implementation80ce54b pushed; #60/#85 closed. GitHub GraphQL verifies
142 open SRD issues after closure (the immediate issue-search result lagged by
one closure). No new issues. Final delivery observed2026-09-26 15:27:56 UTC,
88 minutes after the13:59:56 approval continuation; original batch timings
and the7h30m28s blocked approval wait remain recorded separately.
