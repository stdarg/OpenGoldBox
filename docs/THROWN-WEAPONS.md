# Thrown weapon inventory — batch #58

## Current behavior (SIMPLIFY-1, 2026-09-30)

[SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation)
treats Thrown weapons like [ammunition](AMMUNITION.md). A throw is an ordinary
ranged attack with the weapon's Thrown range and damage; the weapon stays where
it was, held or carried. Nothing lands on a square, becomes a ground item, is
spent from a stack or needs picking up, and a throw never stows a held weapon,
so hands are not checked for it. A Ranged attack with a held Thrown weapon
throws it; the **Thrown weapon** dropdown and **Throw** button remain so a
carried Thrown weapon can be thrown too, and list each weapon by name. Light,
Nick, Savage Attacker, Champion and mastery behavior are unchanged.
`opengold_thrown_weapon_tests` and `thrown_view_tests.gd` check that throws keep
the weapon, including a million-unit stack.

The sections below record the original #58 delivery. Its removal, landing,
pickup, stowing and quantity rules, Q43's stowing and Q44's landing policy are
superseded by SIMPLIFY-1.

## Frozen player outcome

Complete actual removal, battlefield location and recovery of thrown weapons,
including drawing a carried Thrown weapon as part of its attack. Use all seven
SRD Thrown weapons (Dagger, Handaxe, Javelin, Light Hammer, Spear, Dart, Trident),
for all currently supported class/level routes; do not substitute fixed demo
actors for ordinary campaign inventory. Preserve ability, range, critical,
Savage Attacker and Champion behavior, with physical quantities and identity.

Source: [SRD 5.2.1 pp.90 and 177, Thrown and Attack action](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf).
Thrown permits drawing as part of the attack and retains a Melee weapon's normal
ability modifier. The SRD does not prescribe a grid landing square for misses;
Q44 requests the project policy rather than presenting one as an SRD rule.

## Acceptance and boundaries

- Actual held and carried weapon units, including stacks, map to campaign item
  identity/provenance; each accepted throw removes exactly one available unit.
  No removal for canceled targeting or rejected commands. No unlimited copies.
- Drawing/hand eligibility and any necessary weapon stowing use SRD attack
  timing; shields retain their normal don/doff action requirements. All choices
  are visible before committing. No general inventory-management screen.
- Hit/miss, critical, damage defenses and optional Savage decisions preserve
  the original weapon/dice even after it leaves the hand. Champion's immediate
  movement follows critical resolution. Spent actions/reactions and RNG persist.
- Landed weapons use the existing Ground item/Pick up flow and its normal reach,
  line-of-sight, free object interaction/Utilize rules; safe collection follows
  approved Q37. Ownership transfer, quantities and unreachable items survive
  campaign handoff and camping/inn saves. No combat saving controls.
- Historical saves/recipes retain their supported continuation. Capture actual
  module 0.6.46 writer evidence before runtime changes; do not rewrite old fixtures.
  (Superseded: saves now have one pre-1.0 format, nothing is migrated and those
  fixtures were deleted; see [SAVES.md](SAVES.md#pre-10-format-policy).)
- Main/demo keyboard/mouse controls; main EN/ES and demo EN at both supported
  sizes; rules remain in static SRD library, Core owns inventory transactions.
- Excluded: #57 ammunition, #59 Light bonus attacks, #60 mastery effects,
  #80/#81 style feats, later class advancement, unrelated item/equipment fixes.
  Any additional prerequisite outside this boundary requires explicit approval.

## Approved decisions

Q43 (approved 2026-09-25) specifies a labeled Thrown weapon dropdown and Throw button in a new row
immediately below Ground item/Pick up in game/demo. List held/carried Thrown
weapons with quantities. Throw highlights legal targets; click/keyboard target
confirmation draws if needed and throws one unit. Where stowing a held weapon
is needed, show that change before confirmation and apply its proper cost/timing.
Cancel/Escape before target confirmation spends nothing; illegal actions disable.
Reuse standard styling, keyboard access, existing ground pickup and Q37 recovery.

Q44 (approved 2026-09-25) specifies placing the thrown weapon on the target's battlefield square on a
hit or miss, without embedding/breakage/scatter. It remains a ground item rather
than automatically joining the target's inventory. Existing pickup and safe
collection rules decide when it can be retrieved. This is a game policy for an
SRD-unspecified detail, requiring explicit approval.

## Execution card

- Full goal active. Previous turn: PROGRESS, #88 implemented/closed and main clean
  at `74dfb32`, verified before this preflight. One owner; no agents/new tasks.
- Original issue #58, package B03/C. No other issue closure claimed or new issue
  created. No gameplay implementation before Q43/Q44 are answered.
- Requested/recorded Astra/high retained for physical inventory transactions,
  pending-hit state and legacy continuation. No configured-model change claimed.
  Escalate after two failed fixes of the same failure or decisions beyond tier.
- Observed preflight start 2026-09-25 16:08:35 UTC; checkpoint 17:08:35, latest
  17:38:35. Approval wait will be recorded separately; do not silently reset clock.
- Verification: all weapon/level/source rows through real inventory, focused
  combat/inventory/save tests, final native regression and affected Godot checks,
  both apps and required renders/localization. Exact targets after code preflight.
- Current state: requirements/control proposal prepared; implementation pending
  answers. No live build/test process. No compatibility reduction authorized.

## Independent compatibility preparation — 2026-09-25

Before either pending gameplay decision, added `opengold_thrown_weapon_tests`
and captured the actual 0.6.46 writer's campaign and combat continuations. They
recorded the ordinary character, all seven physical stacks, equipped/ground items,
spent Second Wind, critical Savage decision, Champion phase and second throw.
Replay passes byte-exactly for loaded old combat, and campaign bodies/quantities
remain unchanged except any future module identity. No production code changed.

The initial capture used unsaved constructor state; the old format-16 implicit
movement flag then differed from the state produced by actual loading. Capture
now reloads the genuine prior save before continuation. This preserves the old
codec's actual behavior and does not normalize test output or change the writer.

Commands passed:

```bash
cmake -S . -B build/mac-check
cmake --build build/mac-check --target opengold_thrown_weapon_tests -j6
build/mac-check/opengold_thrown_weapon_tests --capture-prior-writer
ctest --test-dir build/mac-check --output-on-failure -R '^opengold_thrown_weapon_tests$'
```

Configure was required for the added test target. Logs are
`/tmp/thrown-baseline-configure.log` and
`/tmp/thrown-baseline-final-{build,capture,tests}.log`.
No build/test remains live. Q43/Q44 remain unanswered; these tests establish
compatibility preparation, not completion of #58 or permission to implement its
pending controls/landing policy. The batch clock remains 16:08:35 UTC.

## Blocking-input checkpoint — 16:19:15 UTC

Goal status confirmed BLOCKED after Q43/Q44 remained unanswered across three
consecutive goal turns (proposal, completed independent baseline, revalidation).
The baseline turn was PROGRESS; this audit found no remaining independent work
inside the frozen batch before those decisions. No process is still running.
Preparation is committed/pushed in `15d5e09`, with focused checks passing. The
full SRD objective remains unchanged and incomplete. Await user decisions before
runtime/control implementation; do not rotate into unrelated batches.

## Authorized continuation — 17:11:00 UTC

User approved Q43 and Q44. Implementation may now proceed within the frozen
scope. The prior blocker is resolved; the goal tracker still reports blocked
(it cannot be resumed through `update_goal`), but the user has directly authorized
this work. Original start/checkpoint remain recorded. Approval-blocked interval:
16:19:15–17:11:00, 51m45s. The 17:08:35 checkpoint fell during that wait; this
continuation reports preparation complete, one focused compatibility check green,
zero gameplay requirements delivered yet. No scope or model change. New visible
question sets restart at 1 under the user's instruction.

## Implementation and verification in progress

Module 0.6.47 introduces physical combat inventory only for new encounters with
held/carried Thrown weapons. Combat format 19 carries item source identities,
stack quantities, held/stowed/ground locations and optional pending damage,
ability-check and Champion movement states.

The SRD library decides hand availability, attack-time drawing/stowing, damage,
landing and pickup. Core reconciles conserved quantities and immutable source
identities transactionally, preserving remaining stack IDs and original item
provenance. It does not inspect weapon properties. Splitting a stack creates a
single ground-unit record, without expanding large carried stacks into units.

Focused checks have passed for all seven weapon types across twelve starting
classes, campaign quantities/pickup/save continuation, and critical Savage/
Champion continuation. Expanded supported-level checks, all native regressions,
main/demo builds and rendered controls are in progress. No issue is yet closed.

## Delivery checkpoint — 17:38 UTC

Original preflight was 16:08:35; the approval wait was 51m45s. This report is at
the original 90-minute outer boundary, without resetting the clock. Since the
17:11 approval continuation, implementation and verification have taken about
27 minutes. No original requirement is marked delivered or issue closed yet.
One batch remains active; no added scope, child tickets, model switch or agents.

All 49 native/tool checks pass, including 154 weapon/class/level routes, large
stacks, companion pickup, rejected handoff atomicity, victory recovery and the
frozen historical continuations. Both applications build. New control tests pass
in main EN/ES and demo EN; graphical runs at both sizes pass. A final sizing
refinement keeps the new Throw button inside the minimum-width combat column;
the affected main build and full Godot regression remain before delivery.

Two existing tests needed semantic updates: fresh throws now leave no held grip
to change, and fresh Rogue/Dagger encounters write format 19. Their original
resource/damage and malformed-version assertions remain; frozen bytes were not
changed. The UI harness now explicitly focuses the first popup entry before
pressing Down; its first attempt assumed a platform-dependent initial focus.
No production rule failure or compatibility reduction was concealed.

## Verified delivery — runtime `5d7813d`

All frozen acceptance is implemented for the currently supported class/level
routes. Q43/Q44 are implemented, with no combat saving controls. The tests cover
154 weapon/class/level combinations; the existing Versatile fixed-seed damage
oracles also prove one-handed thrown dice for both grip choices, ordinary hits,
critical hits and misses. Critical thrown hits retain their original weapon
through Savage Attacker and Champion movement. Normal ranged commands also spend
a held Thrown weapon. Optional targeting cancellation spends nothing.

The focused test additionally covers a million-unit carried stack without unit
expansion, drawing while keeping another weapon in a free hand, necessary stowing
with a shield, one-unit pickup by another party member, metadata/ownership save
continuation, rejected quantity handoffs and reachable victory recovery. Ground
items use the existing persistent detached-item representation and pickup/recovery
rules; uncollected items are not silently returned or deleted.

Final evidence for runtime `5d7813d`:

- All 49 native/tool checks pass: `/tmp/thrown-native-final.log`.
- All 26 Godot runtime checks pass, 42 with native prerequisites:
  `/tmp/thrown-godot-final.log`.
- Both applications build: `/tmp/thrown-integrated-build.log`,
  `/tmp/thrown-main-final-build.log`, `/tmp/thrown-demo-final-build.log`.
- Main EN/ES and demo EN rendered controls and mouse/keyboard/cancel/pickup checks
  pass at both supported sizes: `/tmp/thrown-{main,demo}-final-render.log`.
  Captures: `/tmp/thrown-{main,demo}-final-captures/`. Visual review corrected the
  new main button width and the demo log height; layout assertions guard both.
- `python3 tools/localization.py --check`: 896 complete EN/ES messages.
- `git diff --check` passes; actual historical fixture bytes are unchanged.
- Scope/architecture review: SRD remains STATIC and depends only on the rules
  interface; Core has no new SRD property calculation, and Godot consumes offered
  commands and rule-owned labels. No compatibility reduction or new framework.

Commands after rebuilding affected targets:

```bash
ctest --test-dir build/mac-check --output-on-failure -E '^opengold_godot_' -j6
cmake --build build/mac-check --target opengoldbox_test_project -j6
cmake --build build/sprite-demo --target opengold_godot -j6
OPENGOLD_GAME_DIR=/Users/edmond/POOLRAD ctest --test-dir build/mac-check --output-on-failure -R '^opengold_godot_' -E '^opengold_godot_prepare$' --fixture-exclude-setup godot_project
python3 tools/localization.py --check
```

Graphical checks use `tests/run_godot_test.cmake`, `thrown_view_tests.gd`,
`GRAPHICAL=ON`, `--thrown-fixtures=.../build/mac-check/thrown-fixtures` and
`--thrown-capture=...`; add `--thrown-demo` for the demo project. The main project
is `src/OpenGoldBox/godot`; the demo project is `demos/godot`.

Timing: baseline/decisions 16:08:35–16:19:15 (10m40s), approval wait
16:19:15–17:11:00 (51m45s), authorized implementation/build/verification through
runtime commit 17:11:00–17:41:31 (30m31s). Total wall time 92m56s, excluding wait
41m11s. Build and verification subphases were not separately timed; logs hold
commands/results. Requested/recorded Astra/high retained, no model switch or
agents; token/cost deltas unavailable. One original equipment requirement delivered,
no added issues or expanded scope. The full SRD goal remains incomplete.

Deferred observation: existing demo help text mentions training combat saves,
although saving controls remain hidden. That unrelated wording was not changed.
