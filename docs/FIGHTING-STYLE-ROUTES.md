# Fighting Style routes — approved controls, frozen acceptance

## Batch card

- Standing SRD goal; one owner, no agents/new tasks/issues. STYLE-1/2 approved; implementation and verification active.
- Player outcome: Archery, Defense and Great Weapon Fighting through actual
  Fighter, Paladin and Ranger entitlements at every supported level through4.
  Original feat owners#78/#79/#80. Existing #85/#140/#147 own class integration;
  they stay open for other styles, mastery and cantrip alternatives.
- Include normal Paladin/Ranger advancement1→4 with fixed HP and level4
  available feat/ASI choices, because actual level2 entitlements are required.
  Do not claim full Paladin/Ranger support. Show their unsupported features
  explicitly in the existing note. No forged high-level test-only route.
- Fighter starting selection and level4 choices include the new feat. Its
  class-granted starting style can optionally change when gaining levels2–4;
  an independently acquired level4 feat is a different entitlement.
- Exclude Two-Weapon Fighting/Light/Nick, mastery, Blessed/Druidic Warrior,
  spellcasting, other Paladin/Ranger features/subclasses, level5+, multiclassing.
  Their original acceptance stays required. No new spell/item/action systems.
- Requested `gpt-6-astra/high` for sourced replacement, progression/history
  migration and optional damage interaction; actual configuration unverified.
  No switch. Escalate after two failed fixes of the same issue; no new agents.
- Preflight observed2026-09-26 00:08:10 UTC; checkpoint01:08:10, maximum01:38:10.
  Preserve original start across approval wait. Implementation began after STYLE-1/2 approval.
- Reuse feature grants, Training/Review Training, advancement history, grip,
  shared damage roller, pending Savage hit and current-source public tests.
- Static SRD library owns choices/eligibility/damage/progression. Core handles
  generic advancement transactions/history; Godot presents rules-provided options.

## Acceptance

1. All three feats require the actual Fighting Style feature and cannot repeat.
   Fighter receives it at1, Paladin/Ranger at2. Source/acquisition provenance
   remains distinct from level4 feat selection; invalid/duplicate choices reject
   atomically without wounds, resources, equipment or history changes.
2. Archery applies only to Ranged weapons (+2), including Dart and excluding
   thrown Melee weapons/unarmed fallback. Defense grants+1 only while wearing
   Light/Medium/Heavy armor, never merely holding a shield.
3. Great Weapon Fighting affects eligible Melee weapons held in two hands with
   Two-Handed/Versatile. Each rolled damage die1/2 becomes3 without another RNG
   draw. Flat modifiers unchanged, critical dice each treated once; Savage's
   two rolls use the same policy. No spell/unarmed/thrown/Ranged-weapon benefit.
   Grip, Heavy penalties, defenses, reactions and saved pending rolls stay correct.
4. Ordinary creation/XP advancement/combat/rest/reload through levels1–4 for
   all three source classes, PC and recruited routes. Fighter replacements
   preserve valid prior choices and cannot duplicate the separately acquired feat.
5. Existing saves/history remain supported. Use actual prior-writer GWF fixtures
   and existing current module0.6.52 evidence; capture another writer only for a
   missing acceptance case. Version new grants/choices where validation requires.
6. Main EN/ES and demo EN controls/input/render at1120×800 and1920×1080;
   exact native comparison of saved choices. Relevant focused checks, final
   native regression for shared rules/history, affected Godot checks/builds.
   Coverage, tested revision, commit/push then only proven full issue closures.

## Approved decisions

STYLE-1 (replaces unresolved Q23): automatically apply beneficial Great Weapon
Fighting die replacement on every eligible attack. No new per-hit dialog. Savage
shows the modified totals; it still changes only weapon dice. This chooses the
optional benefit on the player's behalf under STYLE-1 approval.

STYLE-2: Add Great Weapon Fighting to existing Fighter Training/Review Training
and eligible level4 feat dropdowns. In main/demo's existing level-up window,
reuse the unused spell-choice area for a labeled Fighting Style dropdown
(x24,y325,w652,h38; label x24,y292). For Fighters gaining levels2–4, default Keep
current; list eligible replacements. For Paladin/Ranger level2, require one of
Archery, Defense or Great Weapon Fighting. Preserve selections while reviewing;
Confirm applies atomically, Cancel discards, keyboard navigation retained.
At level4 the existing feat/ability controls remain independently usable.
The note explicitly identifies unsupported class features and cantrip alternatives;
these choices do not imply full class completion. Presets/legacy choices follow
existing pre-generated/pending policies; do not invent historical selections.

## Source

Official [SRD5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
pp47,54,59,87–88, checked2026-09-26. Fighter alone has the stated style-replacement
permission. Paladin/Ranger gain the feature at2 and have separate cantrip
alternatives; those alternatives are preserved under their class issues.

## Delivery and verification

Runtime/test commit `bf9ec73e26b3351b4cae31188170e616e402d1a8` (rules0.6.53).
All three original feat requirements are delivered through supported levels1–4;
class trackers#85/#140/#147 remain open for the exclusions above. No new issues,
agents, runtime/model setting changes or unrelated fixes.

- Native targets rebuilt before testing. Integrated CTest:78/79 initially passed;
  the Archery test still expected Paladin/Ranger advancement to be unsupported.
  Extended its actual level-four source matrix and reran Archery plus Training:
  2/2 pass. All79 integrated checks now pass across that run and targeted rerun.
- `opengold_training_tests` includes independent dice/modifier/RNG expectations
  for every source/attained level, critical/Savage/grip/Ranged exclusions, actual
  opportunity attacks and thrown spears, sourced level-four choices, rejected
  duplicates, PC/recruited combat handoff, wounds/resources/rest/reload, and
  Review Training after a class style replacement. Prior-writer fixtures unchanged.
- Main `opengoldbox_test_project` and demo `opengold_godot` build successfully.
  `style_advancement_view_tests.gd` passes for all three classes, main EN/ES and
  demo EN, at1120×800 and1920×1080; layouts inspected. Keyboard confirmation,
  Cancel and independently selectable Fighter replacement/level-four feat pass.
  Six saved outputs match native advancement via `--verify-style-ui CLASS FILE`.
- `training_view_tests.gd` passes main EN/ES and demo EN (`--training-demo`).
  `training_review_view_tests.gd --review-gwf` passes main EN/ES and demo EN
  (`--review-demo`); both saves match `--verify-review FILE`. The demo is English;
  its explicit test flag leaves main Spanish coverage intact.
- `python3 tools/localization.py --check`:941 complete EN/ES messages. Spanish
  class note shortened to fit the approved layout without omitting exclusions.
  `git diff --check` passes. Scope/static-library boundary reviewed.

Reproduce native checks with `cmake --build build/mac-check --target
opengold_training_tests opengold_archery_tests -j6`, then CTest with
`-R '^opengold_(training|archery)_tests$' --output-on-failure`. The integrated run
uses `ctest --test-dir build/mac-check --output-on-failure`. Native training with
`OPENGOLD_GAME_DIR=/Users/edmond/POOLRAD` generates `style-CLASS-ui.ogs` and
`training-review.ogs` under the build directory. Godot scripts take
`--style-class=CLASS --style-fixture=PATH --style-output=PATH --style-captures=DIR`
or the corresponding `--review-*` flags. Use `tests/run_godot_test.cmake` with
Godot `/Applications/Godot_mono.app/Contents/MacOS/Godot`; main project
`src/OpenGoldBox/godot`, demo `demos/godot`. Run Godot serially.

Preflight00:08:10 UTC2026-09-26; verified runtime committed00:44:42:36m32s elapsed,
including approval wait. Three original feature outcomes delivered. Implementation
was already underway at the observed00:26:21 continuation; exact earlier phase
boundaries were not recorded, so no fabricated phase durations. Two test expectation
corrections (Finesse modifier and newly supported classes); one Spanish layout
correction. No repeated production fix failure. Requested Astra/high retained;
actual configuration and tokens/cost unavailable. Delivery before the01:08:10
checkpoint; no speedup claim beyond this measured result.

STYLE-1/2 APPROVED2026-09-26: user replied “1. Yes. 2. yes.”
Implementation continues in the frozen batch; no repeated approval needed.
