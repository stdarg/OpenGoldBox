# SRD coverage ledger

This ledger tracks the [audit](audits/srd-5.2.1-rules.md) against the
[incremental plan](SRD-IMPLEMENTATION.md). The
[GitHub issue index](https://github.com/stdarg/OpenGold/issues/186) links every
planned increment and recurring work queue. All use `SRD_improvements`.
The all-twelve-class level-four milestone is
[#8](https://github.com/stdarg/OpenGold/issues/8).

The [level-four spell inventory](SPELL-INVENTORY.md) records **139 required
spells**, including source discrepancies between class tables and descriptions.
Twelve have partial playable paths and 127 are missing. It links grant routes,
dependencies, current evidence and bounded child issues; inventory work alone
does not close [#165](https://github.com/stdarg/OpenGold/issues/165).

## Wizard spell learning and preparation controls

Runtime `db864c8` (SRD module 0.6.51) completes
[#37](https://github.com/stdarg/OpenGold/issues/37) for the implemented Wizard
catalog through level four. WIZCHOICE-1/2/3 deliver independent creation choices,
two-page advancement, pending old-save knowledge in Spellbook, and preparation
plus one optional cantrip replacement after each completed Long Rest. Presets
receive pre-generated selections. Existing knowledge/preparation, wounds, spent
resources and equipment survive chronological replay and Review Training.

The [delivery packet](WIZARD-SPELL-CHOICES.md) records SRD source, frozen scope,
actual 0.6.50 writer fixtures, conditional campaign16/PC34 compatibility and
reproduction commands. All 51 native/tool and 26 Godot checks pass on the final
integrated tree. New creator/advancement/book/rest flows, keyboard/cancellation,
sequential Wizard choices and whole UI-save comparisons pass in main EN/ES and
demo EN at both supported sizes. 922 translated messages validate. Measured
preflight-to-verified-runtime-commit: 50m16s including approval wait/builds.

This delivers one original control-workflow requirement. No new issues or spell
effects were added. [#97](https://github.com/stdarg/OpenGold/issues/97) remains
open for physical books, copying costs/time and book replacement/loss;
[#165](https://github.com/stdarg/OpenGold/issues/165) retains missing spells.
SRD counts remain intact with unsupported selections visibly pending. Other
classes, Ritual Adept, Evoker and higher levels remain their existing issues.

## Wizard Scholar

Runtime `4ce541a` (SRD module 0.6.50) completes
[#100](https://github.com/stdarg/OpenGold/issues/100) through Wizard levels 2–4:
choose exactly one proficient skill from the six SRD candidates, gain sourced
Expertise, and retain it through advancement, combat and persistence. SCHOLAR-1's
level-two dropdown and old-save Review Training path preserve prior selections,
wounds and expenditure. Missing historical choices remain pending.

The [delivery packet](SCHOLAR.md) records source/acceptance, actual 0.6.49 writer
fixtures, conditional campaign15/PC33 compatibility, commands and limitations.
51 native/tool checks and 26 Godot runtime checks pass; Scholar and existing
Review Training UI flows pass, as do complete UI-save comparisons. Main EN/ES and
demo EN renders/input pass at both supported sizes; 907 translations validate.
Measured freeze-to-verified-runtime-commit: 29m36s, including approval wait/builds.
Ritual Adept, Evoker and broader Wizard integration remain separate requirements.

## Wizard Arcane Recovery

Runtime `642c7a1` (SRD module 0.6.49) completes
[#99](https://github.com/stdarg/OpenGold/issues/99) through Wizard levels 1–4:
legal expended-slot allocations after Short Rest, one use per Long Rest, exact
transactional expenditure and current/historical save continuation. AR-1's
shared Rest dropdown/button is keyboard accessible and preserves camp/inn saves.

The [delivery packet](ARCANE-RECOVERY.md) contains the source, fixed acceptance,
prior-writer provenance, architecture and measured timing. Final verification:
51 native/tool tests; 26 Godot runtime checks (43 entries with prerequisites);
game EN/ES and demo EN renders at 1120×800 and 1920×1080; 902 translations valid.
Other Wizard features and later progression remain separate requirements.

## Approved exception: unlimited ranged ammunition

The user explicitly declined ammunition tracking on 2026-09-25. [#57](https://github.com/stdarg/OpenGold/issues/57)
is no longer planned: a ranged weapon has unlimited ammunition, with no inventory
prerequisite, expenditure or recovery. This retains existing behavior, not newly
implemented SRD expenditure/recovery. Existing saved ammunition items remain
readable; physical Thrown weapons remain distinct. [Policy and evidence](AMMUNITION.md)
record the decision, runtime `79558fd`, all-class weapon tests and compatibility.
Do not reopen tracking as a missing SRD feature without explicit approval.

## Thrown weapon inventory

Runtime `5d7813d` (module 0.6.47) completes [#58](https://github.com/stdarg/OpenGold/issues/58):
all seven Thrown weapons can be drawn from real carried stacks, thrown once,
located on the target square on hit/miss, and retrieved through pickup or approved
safe recovery. The Q43/Q44 game/demo dropdown shows quantities and necessary
stowing. Core preserves quantities, inventory identities and provenance; the
static SRD library owns hand timing, rule outcomes and physical item locations.
[Thrown packet](THROWN-WEAPONS.md) records acceptance, compatibility and evidence.

All 49 native/tool tests and 26 Godot runtime checks pass (42 with prerequisites).
The matrix covers 154 supported weapon/class/level routes, million-unit stacks,
companion transfer, recovery, canceled/rejected operations and pending Savage/
Champion continuation. Main EN/ES and demo EN controls render and pass keyboard/
mouse checks at 1120×800 and 1920×1080; 896 localized messages validate.
Actual 0.6.46 fixtures remain unchanged. New physical encounters use combat 19;
older encounters retain their previous continuation, and campaign 11 is unchanged.
No other issue or excluded equipment property is claimed complete.

## Fighter Champion through level four

Runtime `6324a51` (module 0.6.46) completes [#88](https://github.com/stdarg/OpenGold/issues/88).
Ordinary Fighter level-three confirmation grants Champion; level four retains it.
Weapon and Unarmed Strike attacks critically hit on 19–20, Initiative and
Strength/Athletics gain Advantage, and each critical permits immediate optional
half-Speed movement without opportunity attacks or normal movement expenditure.
The Q41/Q42 game/demo controls preserve spent actions and interrupted movement;
existing level-three/four campaign saves gain the fixed grants during replay.
[Champion packet](CHAMPION.md) records acceptance, compatibility, limitations,
verification commands and timing. No future Athletics actions or spell-access
sources were added to this batch.

All 48 native/tool tests and 25 Godot runtime checks pass (40 with prerequisites).
Main EN/ES and demo EN rendered controls pass at 1120×800 and 1920×1080;
891 localized messages validate. Actual 0.6.45 writer fixtures prove prior combat
continuation, while campaign comparisons allow only approved fixed-grant additions
and existing migrations. Historical fixture bytes remain unchanged. Rules remain
in the statically linked SRD library, with no SRD arithmetic in Core or UI.

Measured start 15:26 UTC, focused checks/render review by 15:50, final native pass
by 16:04:31 and runtime committed at 16:06:21 on 2026-09-25: about 40 minutes to
verified runtime. One of one selected original requirements delivered, no added
issues or scope. Recorded Astra/high retained; no switch or agents. Token/cost
deltas unavailable. The packet records corrections and limits; the full SRD goal
remains active and incomplete.

## Medicine stabilization and Tactical Mind

Runtime revision `ce04673` (module 0.6.45) completes [#32](https://github.com/stdarg/OpenGold/issues/32)
and [#87](https://github.com/stdarg/OpenGold/issues/87). All twelve classes at
currently supported levels can attempt DC 10 Wisdom (Medicine) stabilization;
Fighters gain Tactical Mind at level 2 and use the existing Second Wind pool
through level 4. Failed/declined checks spend the original Action, successful
boosts alone spend Second Wind, and stabilization never heals or wakes.
[Feature packet](MEDICINE-TACTICAL-MIND.md) records source/level coverage,
limitations, codecs, commands and the existing-check-path audit.

The approved Q38/Q39 controls work in game/demo with mouse, keyboard targeting,
Escape cancellation and exclusive failed-check decisions. Main EN/ES and demo EN
were rendered at 1120×800/1920×1080, including the combined Cunning/Wake/Stabilize
row. The main footer identifies the keyboard target; no combat-saving controls
were added. All calculations and resource decisions remain in the STATIC SRD
library. Core only selects the first rules-offered pending choice for AI.

Verification: all 47 native/tool tests, all 24 Godot runtime tests (38 with
prerequisites), focused graphical checks in both applications, and localization
of 886 messages pass on the delivered runtime tree. The last test-script change
only adds a screenshot and visible-footer assertion, also passing in both apps.
Actual 0.6.42/0.6.43 writer fixtures remain byte-identical; expected campaign
migrations add only the legitimate fixed Fighter grant, alongside existing
migrations. PC30 entitlement and pending combat17 states reject invalid old
identities and preserve RNG, spent Action type and targets. Existing checkpoint
shapes remain for states without a pending check. See the feature packet for logs.

Measured continuation: 14:46 UTC start, focused rules passing by 14:58, initial
renders by 15:04, full native pass by 15:20, final UI/render checks by 15:24:20
on 2026-09-25 (about 38 minutes since continuation). Implementation/review and
build phases overlap; no unique-effort sum is inferred. Original preflight start
04:41:34 and its approval wait remain recorded separately. Two of two selected
original requirements delivered; no new issues or expanded scope. Routing kept
the recorded Astra/high assignment, no model switch or delegation. Token/cost
deltas unavailable. Five old migration/profile expectations and two keyboard/
presentation gaps were corrected during verification; no repeated-fix escalation.

## Chill Touch class paths and healing prevention

Rules 0.6.44 completes the bounded class-cantrip increment under #165/#35:
Wizard levels 1–4 and level-one Sorcerer/Warlock choices and casting, melee
Necrotic damage, sourced healing prevention through the end of the caster's next
turn, and Q40's approved earned Stable recovery. [Mechanics, scope and evidence](CHILL-TOUCH.md)
include the actual grant routes, damage/action/component checks, all HP recovery
paths, death saves, overlapping effects, skipped caster turns, deterministic
campaign/combat continuation and old saves. SRD mechanics remain in the STATIC
library; Core and public interfaces have no changes. Existing Spell/Cast and
creator patterns are reused without new controls or combat save buttons.

Verification: tested runtime revision `0c76685`, delivered to `origin/main`,
passes all 46 native/tool checks and all 23 Godot runtime checks (36 with
prerequisites). The first regression run exposed a stale hard-coded module
identity in the Cunning Action test header; updating that assertion and rebuilding
made its native and dependent Godot checks pass. No runtime fix was required.
Actual frozen 0.6.43 saves were captured before the writer change. Prior EN/ES
combat and creator renders at 1120×800 and 1920×1080 remain valid because Q40
changes no layout. Both applications rebuilt; demo sleep and creator checks are
recorded in the feature packet. Localization validates 876 messages.

Commands/evidence: native targets and `opengoldbox_test_project` built with
`cmake --build build/mac-check --target ... -j6`; CTest native `-E '^opengold_godot_' -j6`
and Godot `-R '^opengold_godot_' -E '^opengold_godot_prepare$' --fixture-exclude-setup godot_project`.
Logs `/tmp/chill-final-native-{build,tests}.log`, `/tmp/chill-q40-godot.log`,
`/tmp/chill-q40-training-{build,test}.log`, `/tmp/chill-q40-cunning.log` and
`/tmp/chill-q40-demo-{build,sleep,creator}.log`. Focused checks also cover the
shared recovery timeline and legacy clocks (`/tmp/chill-earned-{build,tests}.log`).

Delivered one bounded spell increment, no new issues or issue closures. The
inventory now has twelve partial playable spells and 127 missing; #165 and #35
remain open for their full acceptance. Object targeting, feat/species access and
higher class progression are explicitly incomplete. Routing retained the recorded
Astra/high assignment, no agents/model switch. Original start 04:56:05 UTC;
independent verification ended 05:28:59, followed by the Q40 approval wait.
Q40 work resumed 14:23:53; focused implementation/build passed by 14:32,
final application verification completed by 14:36:55 (13m02s after resumption).
Documentation/delivery followed. Token/cost deltas unavailable.

## Workflow maintenance — not SRD functionality

2026-09-24: adopted batch execution in [AGENTS.md](../AGENTS.md) and
[workflow](SRD-WORKFLOW.md), with a compact [repository map](SRD-REPO-MAP.md),
[decision register](SRD-DECISIONS.md) and current batch card in the
[handoff](SRD-HANDOFF.md). Related requirements share implementation/evidence;
mandatory ticket splitting is superseded without reducing acceptance.
This ledger is the canonical completion record. Feature docs hold behavior and
durable evidence, the spell inventory holds source/coverage requirements, and
issue updates/handoff link here instead of reproducing delivery narratives.
Historical records are retained; new work follows this ownership convention.

Validation: local Markdown links, explicit map paths and named native test targets
resolve; conflicting old active-workflow phrases were removed; diff whitespace
checks pass. Documentation-only change: no game build or gameplay test rerun.
Observed implementation-to-verification interval: 21:52:55–21:56:28 UTC (3m33s);
earlier investigation timing is unknown, so this is not total task time.
No token saving or faster feature throughput is claimed yet. No SRD issues closed,
no compatibility reduction, no agents/new tasks/model changes; goal remains paused.

## Delivered batch B — rest workflow

#30/#193 are delivered with the reviewed #192 player rest controls. The existing campaign adapter's unverified high-chance
profiles now reject rather than masquerading as guaranteed city-watch events.
Independent regressions failed before the guard correction and pass afterward;
[scope and evidence](REST-RESOURCES.md#rest-batch-b-verified-profile-boundary).
Native resumable activity now records sleep/light/exertion, interruptions and Q32
fresh segments, rejects stale requests, and persists through format 12 while
retaining prior saves. Safe-camp/inn atomic callers consume the same engine.
[Mechanics, compatibility and tests](REST-RESOURCES.md#resumable-activity-persistence-and-evidence).
The complete combat-victory regression additionally preserves XP/loot across the
interrupted rest, save/load and completion; unrelated edits remain locked.
Original ECL damage and encounter requests now interrupt rests automatically;
earned recovery choices finish before combat starts, and committed dice survive
a rejected encounter. [Adapter scope and evidence](REST-RESOURCES.md#193-event-adapter-increment).
Natural sleep, waking and persistent Prone are implemented. Combat equipment drops, pickup, inventory transfer and saved detached items are implemented; natural-sleep camp drops now survive waking and interruption into combat; Q37 safe standing/collection and verified city-watch scheduling now complete #193/#30.
[Sleep/posture scope and evidence](REST-RESOURCES.md#natural-sleep-and-posture-increment-193-q3335).
[Combat equipment scope and evidence](REST-RESOURCES.md#combat-held-equipment-increment-193-q35).
[Camp equipment scope and evidence](REST-RESOURCES.md#natural-sleep-ground-equipment-at-camp-193-q35).
[Final safe recovery, city-watch behavior and complete verification](REST-RESOURCES.md#safe-recovery-and-city-watch-completion-193-q37).
Q29–35 and Q37 are approved; Q37 supersedes Q36. See the
[register](SRD-DECISIONS.md).

The shared game/demo Rest dialog now delivers Q29–31: Short/Long choice,
per-member eligibility/resources, sequential committed dice, Finish/Escape,
camp/inn saving, reload and retained-rest Resume/End. Original permissions and
inn payment remain authoritative. [Controls and acceptance evidence](REST-RESOURCES.md#player-rest-controls-192).
`opengold_godot_rest` exercises actual controls and keyboard actions; campaign
restart tests cover pending spending through the real save/load host.

## Delivered increments

| Increment / feature | Authority and supported scope | Implementation and verification | Persistence |
| --- | --- | --- | --- |
| [#29 / #189](https://github.com/stdarg/OpenGold/issues/29): complete training grants and saved-choice review | All twelve starting class skill packages, supported background/tool/language grants, duplicate provenance and Expertise; Q28 Review Training completes missing saved choices. | [Training scope and reproducible checks](TRAINING.md#review-training-verification-29189). Shared game/demo dialog locks old choices, rejects advancement conflicts, supports keyboard/cancel and blocks combat. Native comparison proves only training changes. 44 native/tool tests, 20 registered Godot tests, game/demo review checks and all-class creator check pass; English/Spanish layouts inspected at both sizes. | Rules 0.6.40 / PC28, campaign 11 and combat 13–15 unchanged. Migrated prior-save wounds, resources, equipment, advancement and history retained. No new save controls or compatibility reduction. |
| [#228](https://github.com/stdarg/OpenGold/issues/228): Sorcerer starting cantrips | SRD pp.64–65,67; four explicit starting choices and Charisma casting through the implemented catalog. | [Scope/evidence](SORCERER-CANTRIPS.md); source/attack/effect/persistence cases and creator/combat checks. Complete catalog, leveled spells, Innate Sorcery and later levels remain #132. | Rules 0.6.40 / PC28; campaign 11, combat 13–15 and FX1–3 unchanged. Actual 0.6.39 prior writer retains exact continuation. |
| [#227](https://github.com/stdarg/OpenGold/issues/227): Warlock Poison Spray | SRD pp.75,153; explicit level-one Pact Magic source and Charisma casting. New presets fill both supported cantrips; old choices remain. | [Scope/evidence](WARLOCK-POISON-SPRAY.md), native source/attack/persistence and actual creator/dropdown casting. Later levels #160 and other sources #202 remain. | Rules 0.6.39 / PC27; campaign 11, combat 13–15 and FX1–3 unchanged. Actual 0.6.38 prior-writer continuation retained. |
| [#226](https://github.com/stdarg/OpenGold/issues/226): Wizard Shocking Grasp | SRD p.162; sourced Wizard levels 1–4, melee spell Lightning damage and target-turn Opportunity Attack suppression without spending/refunding Reactions. | [Scope and evidence](SHOCKING-GRASP.md); actual spell selection, combat controls, interrupted movement and campaign continuation. Parent #223 remains open for other sources. | Rules 0.6.38 / PC26 / FX3; campaign 11 and combat 13–15 unchanged; real 0.6.37 prior-writer fixtures. |
| [#224](https://github.com/stdarg/OpenGold/issues/224): level-one Warlock cantrip path | SRD pp. 71, 127; explicit Pact Magic cantrip selection and Charisma-based Eldritch Blast against creatures. | [Scope and evidence](ELDRITCH-BLAST.md); native rules/persistence, creator and combat controls. Objects #225, later levels #160 and parent #204 remain open. | Rules 0.6.37 / PC25; campaign 11 and combat 13–15 unchanged. Real 0.6.36 fixtures preserve old missing choices and continuation. |
| [I01 / E5](https://github.com/stdarg/OpenGold/issues/20): starting-class weapon proficiency | SRD 5.2.1 pp. 49, 61, 91. Rogue gains martial Finesse or Light; Monk gains martial Light. Applies to the implemented catalog, including Shortsword/Scimitar. Other starting-class grants remain intact. | [weapons.h](../src/OpenGold.Rules.Srd5/src/weapons.h) records Light; [srd5.cpp](../src/OpenGold.Rules.Srd5/src/srd5.cpp) uses the same training query for profiles, equipment notes and combat. [party_tests.cpp](../tests/party_tests.cpp), `class_weapon_proficiency`, verifies all twelve classes, actual +5 attacks for Dexterity-16 Rogue/Monk, equipment explanations and deterministic campaign/combat reload. The regression failed before the fix. | Module 0.6.1; campaign schema unchanged. Existing 0.6.0 and earlier supported campaigns reconstruct the corrected bonuses without losing equipment or wounds. [save_tests.cpp](../tests/save_tests.cpp) verifies 0.6.0 campaign upgrade. Standalone combat checkpoints require exact module identity. |
| [I02 / E2–E3](https://github.com/stdarg/OpenGold/issues/21): death-save turn entry and stabilization | SRD 5.2.1 pp. 17–18. An unstable actor at 0 HP rolls once on turn entry, including the initial initiative slot. A natural 20 permits the recovered actor's turn. Stabilization clears both counters. | [srd5.cpp](../src/OpenGold.Rules.Srd5/src/srd5.cpp) shares one turn-entry path. [rules_tests.cpp](../tests/rules_tests.cpp), `death_save_turn_entry_tests`, covers initial/later entry, all four outcomes, stable/dead skipping and deterministic checkpoint continuation. The regression failed before the fix. [party_tests.cpp](../tests/party_tests.cpp), `stabilization_handoff`, verifies campaign handoff and reload with spent resources preserved. | Module 0.6.2; campaign schema unchanged. Supports 0.6.1 and earlier supported campaigns. Legacy stable counters normalize when rules hydrate vitals. [save_tests.cpp](../tests/save_tests.cpp) verifies 0.6.0/0.6.1 campaign upgrades. Checkpoint restoration performs no turn-entry roll; standalone combat checkpoints still require exact module identity. |
| [I03 / E1](https://github.com/stdarg/OpenGold/issues/22): Constitution and HP history | SRD 5.2.1 p. 23. Gain fixed HP with the previous modifier before applying the Constitution increase per attained level. Prior minimum-one gains persist; Dwarven Toughness remains additive. | [character_rules.cpp](../src/OpenGold.Rules.Srd5/src/character_rules.cpp) initializes modifier history; [srd5.cpp](../src/OpenGold.Rules.Srd5/src/srd5.cpp) replays and validates it for advancement and combat profiles. [advancement_tests.cpp](../tests/advancement_tests.cpp), `hp_history`, verifies the 9-HP Wizard, odd/even modifier boundaries, Dwarves, wounds, unconsciousness, resource expenditure and campaign/combat reconstruction. The regression failed before the fix. | Module 0.6.3 and PC4 combat profiles; campaign format remains 6. Existing saved choices reconstruct history. A rules-owned migration preserves living HP deficits and zero-HP/dead state. [save_tests.cpp](../tests/save_tests.cpp), `hp_migration`, uses a [frozen 0.6.2 save](../tests/fixtures/README.md) to verify correction and reload without double application. Prior-module standalone combat saves still reject. |
| [I04 / E4](https://github.com/stdarg/OpenGold/issues/23): ability-bonus provenance | SRD 5.2.1 pp. 23, 83, 87. Background allocations and level-four Ability Score Improvement are distinct grants. | [character_rules.h](../src/OpenGold.Rules/include/opengold/character_rules.h) exposes source IDs, acquisition levels and individual amounts. Creation/advancement emit separate records; game and demo modifier dialogs render each contribution. [advancement_tests.cpp](../tests/advancement_tests.cpp), `ability_sources`, verifies Soldier +2 and feat +2, grants outside the background list, totals, preview/rejection and reload. Both Godot advancement checks verify actual dialog text and reconstruction; the new game regression failed before the fix. | Module 0.6.3, PC4 and campaign format 6 unchanged. Sources are derived by replaying existing saved choices, including the frozen prior-module fixture in [save_tests.cpp](../tests/save_tests.cpp). Numeric scores, vitals and combat continuation are unchanged. |
| [I05 / G12](https://github.com/stdarg/OpenGold/issues/24): remaining turn resources | SRD 5.2.1 pp. 13–14. Existing melee/ranged attacks and damaging spells preserve unused movement and Bonus Actions. Explicit End Turn advances initiative; AI uses available recovery and then ends. | [srd5.cpp](../src/OpenGold.Rules.Srd5/src/srd5.cpp) retains the turn after attacks/reaction resolution. [rules_tests.cpp](../tests/rules_tests.cpp), `turn_budget_tests`, covers all six offensive verbs, pre/post-attack movement, Second Wind, spell slots, spent-action rejection, ordered reactions and enemy completion. The regression failed before the fix. [party_tests.cpp](../tests/party_tests.cpp), `remaining_turn_handoff`, checks campaign time and spent recovery through completed combat and reload. [combat_view_tests.gd](../tests/combat_view_tests.gd) exercises actual ranged input, keyboard movement, End Turn and AI return. | Module 0.6.4; checkpoint 5 and campaign 6 unchanged. Post-attack/reaction checkpoints continue deterministically with no resource refund. Supported 0.6.3 and earlier campaigns migrate; prior-module combat checkpoints require the exact identity. |
| [I06 / G12](https://github.com/stdarg/OpenGold/issues/25): opportunity triggers and presentation facing | [SRD 5.2.1 p. 15](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf). Turning toward a weapon/spell target creates no reaction. Leave-reach movement still pauses before the step, respecting sight, Disengage and the reaction budget. Incapacitating the mover stops movement and the queue. | [srd5.cpp](../src/OpenGold.Rules.Srd5/src/srd5.cpp) removes runtime facing queues. [rules_tests.cpp](../tests/rules_tests.cpp) verifies left/right turns, party/enemy actors, every targeted offensive verb, ordered movement reactions, spent-reaction limits, interruption, atomic rejection and deterministic migration. The no-reaction regression failed before the fix. Existing status-effect tests verify Blinded visibility. [opportunity_view_tests.gd](../tests/opportunity_view_tests.gd) uses real Load/Save and keyboard controls to verify migrated facing, remaining movement and pending queues. | Module 0.6.5 / combat format 6. [Frozen 0.6.4 fixtures](../tests/fixtures/README.md) prove migration cancels only valid obsolete facing queues, invalidates their command tickets and preserves resources/time/RNG. Real movement retains partial queue position and matches the prior writer after resolution. Unrelated combat identities reject; supported campaigns through 0.6.4 upgrade with campaign format 6 unchanged. |
| [I07 / G12](https://github.com/stdarg/OpenGold/issues/26): allied transit | [SRD 5.2.1 p. 14](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf). Allied spaces are transit cells at normal terrain cost; every selected endpoint must be free. Hostile, size-dependent and general Incapacitated transit remain F13. | Shared [combat_grid.cpp](../src/OpenGold.Rules.Srd5/src/combat_grid.cpp) governs planning, execution and restore. The corridor regression failed before the fix. [combat_grid_tests.cpp](../tests/combat_grid_tests.cpp) checks normal/difficult allied costs, budget boundaries, walls and 9,686 exhaustive layouts. [rules_tests.cpp](../tests/rules_tests.cpp) covers both sides, successive allies, atomic occupied-stop rejection, shared-cell reactions, malformed overlap, knockdown, healing, death and natural-20 recovery. [party_tests.cpp](../tests/party_tests.cpp) verifies a real campaign fight and save/reload; the rendered Godot demo clicks across two allies and rejects an occupied arrow destination. | Module 0.6.6 / combat format 7 retains an involuntary-overlap marker so interrupted movement and later recovery reload correctly. Accepted paused routes validate their remaining budget and free endpoint. Supported 0.6.4/0.6.5 combat and campaign saves upgrade without refilling resources. Prone and size-dependent co-occupancy effects remain explicitly tracked in #35/#44; the marker is persistence state, not a claim that Prone is implemented. |
| [I08 / G9](https://github.com/stdarg/OpenGold/issues/27): Versatile grip | [SRD 5.2.1 pp. 90–91](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf). Quarterstaff/Spear use 1d6/1d8; Battleaxe/Longsword/Trident/Warhammer/War Pick use 1d8/1d10. Two hands affect melee only and require no shield. | Rules supply grip choices and validated commands; campaign inventory and Godot combat dropdowns persist selection. [versatile_tests.cpp](../tests/versatile_tests.cpp) checks all seven weapons, fixed normal/critical/miss dice, thrown attacks, atomic rejection, resource preservation, pending reactions and campaign handoff. [grip_view_tests.gd](../tests/grip_view_tests.gd) exercises layout, keyboard, dice and save/load; inventory integration checks cover shields and both grip choices. | Module 0.6.7 / PC5 / combat 8 / campaign 7 store selected hands separately from resources. [Frozen 0.6.6 files](../tests/fixtures/README.md) preserve the old four forced grips and other one-handed grips; migrated and new saves resume deterministically. Weapon/shield artwork remains the existing catalog; new poses and remaining equipment properties are outside I08. |
| [F01](https://github.com/stdarg/OpenGold/issues/28): feature and feat provenance | [SRD 5.2.1 pp. 47, 83, 87–88](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf). Soldier's Origin feat and the implemented level-four choices have separate entitlements; prerequisites, repeatability and ability choices are validated. | [feature_grants.cpp](../src/OpenGold.Rules.Srd5/src/feature_grants.cpp) centralizes creation grants, advancement sources, choices and validation. [feature_grant_tests.cpp](../tests/feature_grant_tests.cpp) covers all twelve Soldier classes, distinct sources, malformed/duplicate/prerequisite rejection, totals and saved continuation. Existing advancement checks exercise both acquired feats in the normal game/demo sheet. | Module 0.6.8 / campaign 8 / PC6 persist grant records and choices. Formats 1–7 reconstruct grants from creation/history; frozen 0.6.7 files verify wounds/resources/totals and exact combat continuation against the previous writer. Older combat recipes retain validated effects without invented provenance. New feats and complete class features remain separate increments; the starting Fighting Style selector is recorded below. |

| [F02a](https://github.com/stdarg/OpenGold/issues/187): training rules and persistence | [SRD 5.2.1 pp. 8–9, 20, 61–62, 83](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf). Rogue/Criminal skills, Thieves’ Tools, Expertise and languages; Common plus two standard languages for every class. Duplicate proficiency sources contribute one bonus; skill/tool proficiency reports Advantage. | [training.cpp](../src/OpenGold.Rules.Srd5/src/training.cpp) supplies choice validation, all eighteen skill modifiers and source records. [training_tests.cpp](../tests/training_tests.cpp) covers eligibility, overlap, proficiency boundaries, pending choices, malformed records and advancement totals. | Module 0.6.9 / campaign 9 / PC7. Frozen 0.6.8 saves verify pending choices, preserved wounds/resources and exact combat continuation. Creation controls #188 are delivered below; [saved-choice completion #189](https://github.com/stdarg/OpenGold/issues/189) remains open, so parent F02 is not complete. Other class/background packages and campaign skill uses remain separate. |

| [F02b](https://github.com/stdarg/OpenGold/issues/188): training creation controls | Uses the F02a choices and grants with the approved Training step after Class. Presets come with complete training; older saves retain pending choices. | Shared [training_control.h](../src/OpenGoldBox/training_control.h) serves the game/demo, with fixed grants, counts, keyboard access and sourced sheets. [training_tests.cpp](../tests/training_tests.cpp) verifies creator validation/pruning and all 48 presets with campaign round trips; [training_view_tests.gd](../tests/training_view_tests.gd) exercises real controls, focus/scrolling, dependencies and party handoff. Existing creator, party and Spanish localization checks cover the new step. | No module or save-format change. #189 remains open for Review Training; other class/background packages remain separate. Completion describes only the supported choices. [Scope](TRAINING.md). |

| [F03a](https://github.com/stdarg/OpenGold/issues/190): Hit Dice and rest resource rules | [SRD 5.2.1 pp. 47–48, 185, 187](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf). Starting die sizes for all twelve classes; one die per attained level; Constitution healing with a minimum of 1 before the HP cap; one Second Wind use per Short Rest; full supported resource/Hit Dice Long Rest recovery. | The SRD module supplies typed queries and single-die commits. [rest_resource_tests.cpp](../tests/rest_resource_tests.cpp) covers golden rolls, Dwarven Toughness, negative Constitution, caps, rejection, advancement, training review, effects, healing and campaign/combat continuation. See [scope and caller obligations](REST-RESOURCES.md). | Module 0.6.10 / SRD4 / combat 9; campaign 9 and PC7 unchanged. Frozen 0.6.9 files preserve old state and the prior writer’s opportunity-attack result, adding unspent dice only. [Campaign flow #191](https://github.com/stdarg/OpenGold/issues/191) is delivered below; [controls #192](https://github.com/stdarg/OpenGold/issues/192) remain open, so F03 is not complete. |

| [F03b](https://github.com/stdarg/OpenGold/issues/191): campaign rest transactions | SRD 5.2.1 pp. 185, 187. Individual active-member eligibility, precise Long Rest cooldowns, a completed hour before Hit Dice spending, another decision after each die, and zero-die completion. | [campaign_rest.cpp](../src/OpenGold.Core/src/campaign_rest.cpp) stages time/effects/resources atomically. [campaign_rest_tests.cpp](../tests/campaign_rest_tests.cpp) verifies mixed parties, reserves, effect/RNG timing, duplicate/expired requests, original camp restrictions, inn rollback and next combat. No UI layout changed. | Campaign 10 persists the completed spending window and revision ticket; old formats gain no pending entitlement. Resources remain module 0.6.10 / SRD4 / combat 9 / PC7. Save/load between rolls and malformed continuation rejection are covered. [Rest controls #192](https://github.com/stdarg/OpenGold/issues/192), [late interruptions/resumption #193](https://github.com/stdarg/OpenGold/issues/193) remain outside this delivery. |

| [F04a](https://github.com/stdarg/OpenGold/issues/194): mortality recovery clocks | SRD 5.2.1 pp. 17–18. Shared death-save, stabilization, damage and healing transitions; one rolled 1d4-hour Stable delay, combat elapsed countdown and exact next-turn death-save timing. | [life_cycle.cpp](../src/OpenGold.Rules.Srd5/src/life_cycle.cpp) stores value-based recovery state. [recovery_clock_tests.cpp](../tests/recovery_clock_tests.cpp) verifies golden RNG/rolls, deadlines, damage/healing, migration, rejected timers, campaign/combat persistence, training/advancement and pending reactions. No new controls. | Module 0.6.11 / SRD5 / combat 10; campaign 10 and PC7 unchanged. Frozen 0.6.10 saves preserve spent dice/slots, Stable and unstable members, a dead member, a reserve and pending movement. Legacy delays initialize without inventing elapsed time or rolling on load. [Campaign scheduling #195](https://github.com/stdarg/OpenGold/issues/195) completes F04 separately. |

| [F04c](https://github.com/stdarg/OpenGold/issues/195): campaign recovery | SRD 5.2.1 pp. 17–18. Six-second death-save turns and one rolled Stable deadline continue through exploration, waits, rests and encounter boundaries. | [recovery_timeline.cpp](../src/OpenGold.Rules.Srd5/src/recovery_timeline.cpp) combines mortality and effect deadlines. [campaign_recovery_tests.cpp](../tests/campaign_recovery_tests.cpp) verifies fixed rolls, time partitions, reserves, saves, encounters, rest eligibility and failed-event rollback; the Godot route exercises a zero-HP companion and reserve through original camp/temple/inn callbacks. | Module 0.6.12; existing campaign 10 / combat 10 / SRD5 / PC7 schemas. No load-time rolls, double combat processing, rest benefits from natural recovery or UI changes. Completes #31; Help/Medicine remains #32. |

| [F05](https://github.com/stdarg/OpenGold/issues/33): typed damage and defenses | SRD 5.2.1 pp. 16–17, 84, 91, 146. Typed damage; nonstacking resistance/vulnerability/immunity; type-specific rounding; Dwarf Poison resistance. | [damage.cpp](../src/OpenGold.Rules.Srd5/src/damage.cpp) resolves mixed damage and sourced defenses. [damage_tests.cpp](../tests/damage_tests.cpp) runs Poison attacks against Dwarves in all twelve classes, every supported weapon, Fire Bolt/Scorching Ray/Magic Missile, migration and malformed data. Existing Godot Modifiers/log presentation is reused. | Module 0.6.13 / PC8; campaign/combat 10 and SRD1–5 retained. Frozen 0.6.12 writer fixtures preserve resources, RNG, clocks and pending reactions; newly supported fixed resistance is sourced to the saved species. Temporary HP #34, remaining Dwarf traits #66 and full spell conformance remain separate. |

| [F05b1](https://github.com/stdarg/OpenGold/issues/196): Temporary HP native foundation | SRD 5.2.1 pp. 17–18. Separate sourced pool, explicit keep/replace, damage absorption after defenses, zero-HP consequences and individual Long Rest expiry. | [life_cycle.cpp](../src/OpenGold.Rules.Srd5/src/life_cycle.cpp) and [temporary_hp_tests.cpp](../tests/temporary_hp_tests.cpp) cover independent arithmetic, actual Poison combat, healing, training, advancement, PC/NPC/reserve rest, malformed state and prior-writer continuation. | Module 0.6.14 / combat 11 / SRD6; campaign 10 and PC8 retained. This is a native API foundation. [Orc Adrenaline Rush and reviewed controls #197](https://github.com/stdarg/OpenGold/issues/197) are delivered below; parent #34 stays open. See [scope](TEMPORARY-HP.md). |

| [F05b2](https://github.com/stdarg/OpenGold/issues/197): Orc Adrenaline Rush and replacement UI | SRD 5.2.1 pp. 17–18, 86. Bonus Action Dash, PB uses and Temporary HP, full Short/Long Rest recharge, explicit nonstacking replacement. | [adrenaline_tests.cpp](../tests/adrenaline_tests.cpp) covers all twelve Orc classes, costs, damage-buffer source choices, stale tickets, migration, movement reactions and campaign continuation. [Godot controls](../tests/adrenaline_view_tests.gd) cover the labeled feature, keyboard decision and HP presentation. | Module 0.6.15 / combat 12 / PC9 / SRD7; campaign 10 retained. Frozen 0.6.14 fixtures retain actual previous-writer state. Exploration activation remains unavailable; player saving is restricted to camp/inn, #34 stays open. [Details](TEMPORARY-HP.md). |


| [EQ03](https://github.com/stdarg/OpenGold/issues/55): Heavy weapon requirements | SRD 5.2.1 pp. 89, 91. Strength 13 for Heavy melee weapons, Dexterity 13 for Heavy ranged weapons; lower scores impose attack Disadvantage. Applies to all nine Heavy weapons across all twelve classes (the last four added in EQ02). | [heavy_weapon_tests.cpp](../tests/heavy_weapon_tests.cpp) checks independent attack/damage rolls, stacking/cancellation, Small species, spell/unarmed exceptions, opportunity attacks, equipment changes, ASI, rejected commands and saved continuation. Existing Modifiers text explains the source in English/Spanish. | Module 0.6.16; PC9/combat 12/campaign 10/SRD7 retained. Frozen 0.6.15 writer files preserve existing grants, HP, resources, clocks, RNG and pending movement; future affected attacks use the corrected rule. The complete catalog is delivered in #54. [Details](HEAVY-WEAPONS.md). |


| [EQ02](https://github.com/stdarg/OpenGold/issues/54): complete weapon catalog | SRD 5.2.1 pp. 16, 89–91. All 38 weapon definitions and source properties, including Blowgun's fixed damage. | [weapon_catalog_tests.cpp](../tests/weapon_catalog_tests.cpp) checks the independent full table and actual normal/critical/miss attacks for every weapon/class combination, range, hands, Heavy, purchase/equip, rejection and campaign continuation. English/Spanish names and explanations use existing UI. | Module 0.6.17; existing schemas retained. Frozen 0.6.16 fixtures prove preservation of original weapon conversions, existing grants/resources and exact combat continuation. Loading/ammunition/Light/mastery actions and mounted Lance use remain #56–60/#173. [Scope](WEAPON-CATALOG.md). |

| [EQ01](https://github.com/stdarg/OpenGold/issues/53): armor catalog and training | SRD 5.2.1 p. 92 and all twelve core class tables. Twelve armor suits and Shield, AC/Dexterity rules, Strength/Stealth penalties and class training. | [armor_catalog_tests.cpp](../tests/armor_catalog_tests.cpp) checks the independent full table, all 156 class/equipment combat combinations, actual initiative/attack/spell restrictions, equipped ability checks, boundary values, shield/hands rejection and campaign/save continuation. Existing Modifiers text and names are localized. | Module 0.6.18; PC9/combat 12/campaign 10/SRD1–7 retained. Frozen 0.6.17 saves preserve existing state and prior-writer continuation. Don/doff and shield Utilize actions remain [#198](https://github.com/stdarg/OpenGold/issues/198); optional class grants, starting packages and unsupported original conversions remain open. [Scope](ARMOR.md). |

| [F07a](https://github.com/stdarg/OpenGold/issues/199): Wizard spell knowledge and preparation | SRD 5.2.1 pp. 77–78. Sourced cantrip/book entries, first acquisition levels, independent preparation and pending entitlements through level 4 for existing spells. | [spell_access_tests.cpp](../tests/spell_access_tests.cpp) verifies knowledge retention, actual casting/slot use, invalid grants, atomic advancement, campaign replay and frozen-writer continuation. Existing Modifiers text is checked through the Godot creation flow in Spanish. | Module 0.6.19 / PC10; campaign 10/combat 12/SRD1–7 retained. Old campaigns recover only preset/history-backed selections; old combat retains its recorded access. Controls #37 are delivered above; copying #97 and free casts #200 remain open; parent #36 stays open. [Scope](SPELL-ACCESS.md). |

| [FT03](https://github.com/stdarg/OpenGold/issues/76): Savage Attacker | SRD 5.2.1 p. 87. Nonrepeatable sourced feat, optional once-per-turn weapon damage reroll and either-result choice, including opportunity hits. | [savage_attacker_tests.cpp](../tests/savage_attacker_tests.cpp) checks independent all-class rolls, acquisition, both stages, exclusions, critical/Versatile dice, defenses, atomicity and movement continuation. [savage_view_tests.gd](../tests/savage_view_tests.gd) checks approved controls, keyboard focus and deferred HP. | Module 0.6.20 / combat 13; PC10/campaign 10/SRD1–7 retained. Frozen 0.6.19 saves retain existing state and reproduce prior automatic outcomes when the higher result is explicitly chosen. Human/starting-package choices remain their own issues. [Details](SAVAGE-ATTACKER.md). |

| [F09a](https://github.com/stdarg/OpenGold/issues/201): spell components and Somatic hands | SRD 5.2.1 pp. 90, 105 and the six existing spell descriptions. Explicit V/S requirements, weapon/wand plus shield blocking, verbal-only exceptions and attack-grip preservation. | [spell_component_tests.cpp](../tests/spell_component_tests.cpp) covers real spell commands, slots, rejected-command atomicity, inventory/equipment and frozen migration; [spell_component_view_tests.gd](../tests/spell_component_view_tests.gd) covers game/demo action controls and game keyboard casting. Existing Modifiers explanations are localized. | Module 0.6.21; campaign 10/combat 13/PC10/resource formats retained. Actual 0.6.20 fixtures preserve state and verbal-cast continuation. Parent #39 stays open for live speech-blocking sources; materials/foci remain #40. [Scope](SPELL-COMPONENTS.md). |
| [S08a](https://github.com/stdarg/OpenGold/issues/206): Poison Spray and explicit Wizard cantrip choices | SRD 5.2.1 pp. 77–78, 153, 187, 191. Sourced cantrip choices, level-1–4 Poison ranged attack, valid living targets, defenses and zero-HP attack modifiers. | [Native](../tests/poison_spray_tests.cpp), [creator UI](../tests/cantrip_view_tests.gd), [combat UI](../tests/poison_view_tests.gd); prior-writer campaign and Fire Bolt continuation. | Rules 0.6.22 / campaign 11 / PC11; combat 13 and SRD1–7 retained. Approved questions 12–14 add playable Wizard controls. Other sources, live speech blocking and full spell selection remain #202/#39/#37. [Scope](POISON-SPRAY.md). |
| [S09](https://github.com/stdarg/OpenGold/issues/203): Sacred Flame, partial Cleric path | SRD 5.2.1 pp. 36–37, 159, 191. Sourced cantrip choice, Dexterity save, Radiant damage, sight and zero-HP automatic physical-save failure. | [Native](../tests/sacred_flame_tests.cpp), [creator](../tests/cleric_cantrip_view_tests.gd), [combat UI](../tests/sacred_view_tests.gd), prior-writer campaign/Cure Wounds continuation. | Rules 0.6.23 / PC12; campaign 11/combat 13/SRD1–7 retained. Approved questions 15–16 add playable Cleric controls. Partial-cover exception, live speech blockers and other routes remain open; #203 and #91 are not complete. [Scope](SACRED-FLAME.md). |
| [S11 control preparation](https://github.com/stdarg/OpenGold/issues/205): shared known-cantrip selector | User-approved question 18; presentation consumes existing knowledge and legal commands. | Poison/Sacred/component Godot checks cover choice filtering, keyboard dropdown, Cast, ally targeting, A/Space and disabled states; English/Spanish at both sizes. | Rules and save formats unchanged. Replaces individual main-game cantrip buttons; does not implement Ray of Frost or close #205. [Scope](CANTRIP-CONTROLS.md). |
| [S11 Ray of Frost](https://github.com/stdarg/OpenGold/issues/205) | SRD 5.2.1 p. 157; sourced Wizard choice, ranged Cold attack and nonstacking caster-turn Speed reduction. | Independent level 1–4 damage/RNG, multiple casters, movement/Dash, effects, checkpoint/campaign/camp continuation; actual creation and Godot casting controls. | Partial: speech blockers #39 and additional grant integrations remain. PC14/FX2/combat 15; campaign 11. [Evidence and limits](RAY-OF-FROST.md). |
| [F08a concentration transitions](https://github.com/stdarg/OpenGold/issues/207) | SRD 5.2.1 p. 179; source replacement, damage saves, incapacity, release and expiry. | Independent state/RNG/DC/time-partition tests; existing effects and Ray regressions pass. | Foundation only: no live spell/save hooks yet. #208/#209 deliver combat/player and campaign integration; #38/#39 remain open. [Limits](CONCENTRATION.md). |
| [Dwarven Toughness explanation #210](https://github.com/stdarg/OpenGold/issues/210) | SRD 5.2.1 p. 84; current +1 per attained level in the existing racial section. | Independent HP/source checks: all 12 starting classes; Fighter/Cleric/Wizard through 4; Constitution increases and reloads. | Display correction only; arithmetic/formats unchanged. Dwarf #66 stays open. [Details](DAMAGE.md#dwarven-toughness-source-display). |
| [Sage fixed training #211](https://github.com/stdarg/OpenGold/issues/211) | SRD 5.2.1 p. 83: Arcana, History and Calligrapher's Supplies with background provenance. | All 12 starting classes, Rogue Expertise, tool/skill checks, ASI and actual 0.6.25 migration; existing Training display. | Rules 0.6.26 / PC15. No new choices, items or feat grant. Sage #63 remains open. [Evidence and limits](SAGE-TRAINING.md). |
| [Acolyte/Soldier fixed training #212](https://github.com/stdarg/OpenGold/issues/212) | SRD 5.2.1 p. 83: Acolyte Insight, Religion, Calligrapher's Supplies; Soldier Athletics, Intimidation. | All 12 starting classes, Rogue Expertise, ASI, actual 0.6.26 migration and existing translated Training display. | Rules 0.6.27 / PC16. #61/#64 remain open for remaining package choices. [Evidence and limits](BACKGROUND-TRAINING.md). |
| [Archery #78](https://github.com/stdarg/OpenGold/issues/78) | SRD 5.2.1 p. 87: +2 with Ranged weapons; prerequisite and nonrepeatability. | Actual Fighter/Paladin/Ranger grants, ordinary levels1–4, attack/category oracles, campaign/recruited/reload and main/demo controls. | Rules0.6.53 completes supported source routes. [Style-route evidence](FIGHTING-STYLE-ROUTES.md). |
| [Fighter styles #85](https://github.com/stdarg/OpenGold/issues/85) | Starting choice and replacement whenever gaining a Fighter level. | Training/Review Training and separate advancement control; initial and independently acquired feats retain distinct provenance. | All four SRD styles and mastery through4 delivered; see the Weapon Mastery integration completion record below. [Scope](FIGHTER-STYLES.md). |
| [Starting class skills #213](https://github.com/stdarg/OpenGold/issues/213) | SRD 5.2.1 Core Traits tables for all twelve classes; exact skill lists and counts. | Shared Training controls, generated presets, sourced bonuses, all-class/background native oracles, actual prior-writer migration and keyboard checks. | Rules 0.6.30 / PC19. Missing old choices remain pending; full class packages remain in their trackers. [Evidence and limits](CLASS-SKILLS.md). |
| [Bard instruments #214](https://github.com/stdarg/OpenGold/issues/214) | SRD 5.2.1 pp. 31, 94: three of ten instrument proficiencies. | Normal Training, generated presets, sourced sheet/check bonuses, all 120 triples, prior-writer campaign/combat and translated keyboard/render checks. | Rules 0.6.31 / PC20. Old choices stay pending; equipment, Utilize actions and remaining Bard features stay separate. [Evidence](BARD-INSTRUMENTS.md). |
| [Monk tools #215](https://github.com/stdarg/OpenGold/issues/215) | SRD 5.2.1 pp. 49, 93–94: one artisan tool or instrument from all 27 options. | Training, generated presets, sourced checks, overlap and class-change preservation, prior-writer continuation. | Rules 0.6.32 / PC21; old choices pending. Equipment, Utilize and other Monk features remain separate. [Evidence](MONK-TOOLS.md). |
| [Druid Herbalism Kit #216](https://github.com/stdarg/OpenGold/issues/216) | SRD 5.2.1 pp. 41, 94: fixed class tool proficiency. | Ordinary creation/presets, sourced checks and fixed display, validated old-ledger migration with exactly the owed grant. | Rules 0.6.33 / PC22. Equipment, crafting/Utilize and other Druid features remain separate. [Evidence](DRUID-HERBALISM.md). |
| [Soldier Gaming Set #217](https://github.com/stdarg/OpenGold/issues/217) | SRD 5.2.1 pp. 83–94: one of four Gaming Set variants. | Normal Training, presets, sourced checks, all 48 class/variant combinations, actual prior-save continuation and completion. | Rules 0.6.34 / PC23; old choices pending. Equipment/wealth and Utilize remain separate. [Evidence](SOLDIER-GAMING.md). |
| [Great Weapon Fighting #80](https://github.com/stdarg/OpenGold/issues/80) | SRD 5.2.1 p. 88: eligible two-hand Melee damage dice1/2 count as3. | Independent actual dice/RNG oracles, critical/Savage/reaction/grip/thrown/Ranged exclusions and sourced acquisition through Fighter/Paladin/Ranger. | Rules0.6.53, STYLE-1 automatic benefit approved; PC36/campaign17 and retained historical fixtures. [Evidence](GREAT-WEAPON-FIGHTING.md). |
| [FTR02](https://github.com/stdarg/OpenGold/issues/86): Action Surge through level four | SRD 5.2.1 p. 48. Fixed Fighter level-two grant, one restricted extra action, one use per Short/Long Rest through level 4. | [Native](../tests/action_surge_tests.cpp), [combat controls](../tests/action_surge_view_tests.gd), actual 0.6.23 writer campaign/Dash fixtures. | Rules 0.6.24 / PC13 / combat 14 when a Surge-capable Fighter participates / spent resource SRD8; campaign 11 retained. Historical campaigns gain their justified fixed grant; old combat retains recorded access. Approved mouse/keyboard button, remaining uses and disabled states verified in English/Spanish; #86 complete. [Scope](ACTION-SURGE.md). |

Light extra attacks, Monk Martial Arts, optional feature-granted proficiency,
multiclass-entry proficiency remain their own issues. EQ02 adds the complete
catalog and Rapier/Hand Crossbow proficiency examples; it does not establish
complete Rogue or Monk support.

Validation for I01: rebuilt and passed the five native suites for party, save,
rules, character and content behavior. No Godot layout or control code changed;
equipment explanation checks exercise the shared rules output.

Validation for I02: rebuilt and passed the rules, party, save, status-effect and
advancement native suites. No Godot layout or control code changed. Death saves
and natural recovery outside combat are now completed by [F04 #31](https://github.com/stdarg/OpenGold/issues/31);
the existing all-unconscious-party defeat policy remains unchanged.

Validation for I03: the rules, party, save, status-effect, advancement, character
and content native suites pass. The Godot GDExtension is rebuilt against the
updated shared character data. No UI layout or controls changed. Higher levels
and additional classes retain their separate advancement issues.

Validation for I04: advancement, save, character and party native suites pass;
game and demo Godot advancement checks pass with the installed original assets.
English/Spanish catalogs and the localization UI check pass. Existing dialogs
retain their layout and controls. The general feature/feat grant ledger is delivered in
[F01 #28](https://github.com/stdarg/OpenGold/issues/28).

## Audit finding status

Status refers to the current implementation. The audit retains its original
commit-specific findings as historical evidence.

| Finding | Status | Primary work |
| --- | --- | --- |
| E1: Constitution/HP history | Fixed | [I03 #22](https://github.com/stdarg/OpenGold/issues/22) |
| E2: initial death save | Fixed | [I02 #21](https://github.com/stdarg/OpenGold/issues/21) |
| E3: stabilization counters | Fixed | [I02 #21](https://github.com/stdarg/OpenGold/issues/21) |
| E4: bonus provenance | Fixed | [I04 #23](https://github.com/stdarg/OpenGold/issues/23) |
| E5: Rogue/Monk weapon proficiency | Fixed for implemented weapons | [I01 #20](https://github.com/stdarg/OpenGold/issues/20) |
| G1: complete class features/advancement | Open; all twelve required | [Level-four milestone #8](https://github.com/stdarg/OpenGold/issues/8) and its class issues |
| G2: species/background grants | Open | [Species #51](https://github.com/stdarg/OpenGold/issues/51), [backgrounds #50](https://github.com/stdarg/OpenGold/issues/50) |
| G3: feat entitlements/choices | Open | [Feats #49](https://github.com/stdarg/OpenGold/issues/49) |
| G4: spell access/learning/preparation | Partial: Wizard ordinary learning/preparation controls delivered; other source/class routes remain | [F07 #36](https://github.com/stdarg/OpenGold/issues/36) and class-specific integration |
| G5: spells/shared casting mechanics | Open | [Spell inventory #165](https://github.com/stdarg/OpenGold/issues/165) and named spell/mechanic issues |
| G6: full progression/multiclassing | Open | [Higher levels #176](https://github.com/stdarg/OpenGold/issues/176), [multiclassing #179](https://github.com/stdarg/OpenGold/issues/179) and successors |
| G7: rests/recharge | Open | [F03 #30](https://github.com/stdarg/OpenGold/issues/30) |
| G8: death/recovery outside combat | Fixed, including Help/Medicine #32 | [F04 #31](https://github.com/stdarg/OpenGold/issues/31) |
| G9: equipment/properties | Open; E5 and Versatile corrections delivered | [Equipment #48](https://github.com/stdarg/OpenGold/issues/48), [Versatile I08 #27](https://github.com/stdarg/OpenGold/issues/27) |
| G10: conditions/effects/creature state | Open | [Conditions #35](https://github.com/stdarg/OpenGold/issues/35), [damage #33](https://github.com/stdarg/OpenGold/issues/33), [size #44](https://github.com/stdarg/OpenGold/issues/44) |
| G11: precise capability/documentation coverage | In progress: ledger and issue index established | Update with every increment; reconcile remaining support claims at [final closure #185](https://github.com/stdarg/OpenGold/issues/185) |
| G12: turn/facing/transit policies | I05–I07 delivered; remaining creature-state rules are tracked separately | [I05 #24](https://github.com/stdarg/OpenGold/issues/24), [I06 #25](https://github.com/stdarg/OpenGold/issues/25), [I07 #26](https://github.com/stdarg/OpenGold/issues/26) |

Each subsequent delivery adds a feature row with the SRD source, actual class/
level/options supported, code/tests, save version and any reviewed adaptation.
Do not mark a tracking issue complete solely because its first child works.

SRD-derived rules use the attribution in
[NOTICE](../data/rules/srd-5.2.1/NOTICE.md); see the
[official SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf).

### Rogue level-two Cunning Action increment (#218)

Rogue ordinary advancement reaches level two (300 XP, fixed-average d8 HP,
source grant, preserved training/vitals). Bonus Action Dash/Disengage reuse the
shared movement and opportunity-attack lifecycle and have approved combat controls.
See [Cunning Action](CUNNING-ACTION.md) for acceptance evidence. Hide remains #219;
parent #113 and later Rogue advancement #116 remain open. Barbarian/Monk Unarmored
Defense was already present in character AC calculation and existing party tests;
no duplicate implementation was needed.

### Rogue Sneak Attack through level four (#112/#220/#221)

Rules0.6.52 connects sourced Sneak Attack to ordinary Rogue creation, advancement,
combat, reactions, rest and campaign reload through all four levels. The approved
optional dialog works in main EN/ES and demo EN. Extra dice, criticals, ally and
roll-mode eligibility, signed damage/resistance and separate Savage rolls remain
SRD-owned. Recruited Rogues and malformed pending-hit fields have explicit cases.
Old actual-writer fixtures and capabilities are preserved; new Rogues use
conditional PC35/combat21. No campaign-format bump or combat-saving controls.

Verified integrated runtime: all78 checks (51native/tool,27Godot), main/demo
asset-backed advancement and exact native-save comparison, both-size rendered
combat/advancement checks,934-message localization validation. The full training
suite passed again after the final recruited/malformed-save test additions.
Commands and limits: [batch evidence](ROGUE-ATTACKS.md#verified-implementation-and-remaining-work).
Mechanics and durable tests: [Sneak Attack](SNEAK-ATTACK.md).

### Rogue Steady Aim (#114)

Ordinary Rogue levels3/4 grant Steady Aim: a Bonus Action before any movement
this turn, Advantage on the next attack roll (consumed on hit or miss), and
Speed0 through turn end. Unused Advantage expires; opposing roll modifiers
cancel normally. Dash cannot restore movement; returning to the starting square
does not restore eligibility. No rest pool. Source/level, spent-budget and
pending-hit saves validate strictly; current and historical continuations pass.

Main and demo have approved Bonus Action dropdowns. All three commands—Dash,
Disengage and Aim—match independent native saved results under keyboard use;
spent budgets disable controls. Main EN/ES and demo EN render/input checks pass
at1120×800 and1920×1080. Demo neighbor controls and layout checks pass. Ordinary
advancement/rest/reload through4 and recruited routes use the same tested rules.
See [batch evidence](ROGUE-ATTACKS.md#approved-demo-completion--2026-09-26).
Rules runtime7f3ab30; final demo completion runtime8ee62fe.
#116 remains open for full Rogue completion; Thief, Hide and mastery are not
claimed complete. Conditional PC35/combat21 preserve released histories.

### Unconscious enemy transit (#222, bounded child of #44)

Existing zero-HP Unconscious creatures can be crossed by either side, with the
correct nonstacking Difficult Terrain cost and no occupied voluntary endpoints.
Search/execution share the occupancy model. Interrupted enemy overlap preserves
spent movement through recovery and reload. See [Unconscious transit](UNCONSCIOUS-TRANSIT.md).
Tiny/size differences, large footprints, other Incapacitated sources and the Prone
consequence remain tracked by #44/#45/#35; this increment does not close them.

### Fighting Style routes (#78/#79/#80)

Rules0.6.53 implements Archery, Defense and Great Weapon Fighting through actual
Fighter starting/replacement and Paladin/Ranger level-two grants, plus independent
level-four feats. Defense is armor-only; shields alone do not grant its bonus.
Fighter replacement history remains separate from starting Training and feat
choices. Campaign17/PC36 preserve source and acquisition level; no old fixture or
supported history was removed. Rules stay in the static SRD library.

[Packet and verification](FIGHTING-STYLE-ROUTES.md) records the final runtime
revision and commands. Main EN/ES and demo EN advancement controls pass at both
sizes; saved results match independent native advancement. Training and Review
Training exercise GWF; recruited routes, spent resources, wounds, rest and old
save continuations pass. #85/#140/#147 retain other styles, mastery and cantrip
alternatives. Whole Paladin/Ranger classes and later levels remain incomplete.

### Light attacks, Two-Weapon Fighting and Loading (#59/#81/#56)

Runtime/test commit `69771eb`, rules0.6.55, delivers the three original
requirements on `codex/srd-light-attacks`. All three issues are closed after push.
The [fixed packet](LIGHT-ATTACKS.md#integrated-implementation--rules0655) records
mechanics, scope, commands, compatibility and timings. Actual two-hand equipment
was delivered in`c539347`; genuine0.6.53/0.6.54 writer fixtures are retained.

An Attack action qualifies a later Bonus Action attack with a different physical
Light weapon, including misses, thrown units and identical weapon types. Actual
held weapon selection supports ordinary attacks and eligible reactions. TWF is
available through Fighter starting/replacement choices, Paladin/Ranger level-two
choices and independent level-four feats, with provenance and nonrepeatability.
Its modifier applies once; negative modifiers, Savage, Sneak and critical damage
remain correct. Loading respects separate Action/Surge/Bonus Action transactions.
PC38/Combat22 retain selected identities and pending Light damage; supported
historical saves retain their recorded state and continuation.

All78 integrated checks pass on fresh binaries (61.48s final run). Focused main
EN/ES + demoEN combat/advancement tests pass at both supported sizes, with exact
native combat-state and six campaign-save comparisons. Actual PC/recruited safe
recovery and rest/save paths are covered.960 localized messages validate. An
unchanged party test caught delayed initialization of non-thrown Light weapon
identities; initializing them at encounter creation fixed live/restored divergence.
Core gained no SRD calculations. No new issues, agents, ammunition tracking or
combat-save controls were added. Nick/mastery, level-five Extra Attack, other
styles and remaining class/spell requirements stay open under their existing scope.

### Ability Score Improvement conformance (#82)

Reconciled by test/build commit`c3a88b4` on`codex/srd-asi-conformance`; #82 closed.
The existing rules0.6.55 implementation was verified rather than duplicated.
[Acceptance, limits and evidence](ASI-CONFORMANCE.md):252 allocation/class/ownership
cases, all-ability caps, all12 early-entitlement checks, exact provenance,
modifiers/saves/skills/combat effects, Constitution HP, wounds/resources, rests
and saved histories. Duplicate use of one entitlement is rejected; ASI remains
repeatable when further legal entitlements become available. The current1–4 band
provides one; unavailable class progression and later levels remain open.

The extended advancement suite passes. Existing controls pass for six classes,
mainEN/ES + demoEN at both sizes, and all18 saved UI results match native
advancement exactly. Production code/formats are unchanged from the78-check
Light batch baseline. The obsolete embedded single-page Wizard check is noted
as deferred test maintenance; current two-page checks pass. No broader Fighter,
class integration or higher-level completion is claimed.

### Weapon Mastery acquisition checkpoint (batch incomplete)

The feature-branch commit containing this entry implements the acquisition part
of [Weapon Mastery](WEAPON-MASTERY.md): five starting class routes, all38 eligible
weapon kinds, Fighter4 additional selection, preset choices, pending historical
training, chosen-grant validation and conditional PC39 recipes. Rules remain in
`opengold_rules_srd5`; Core passes choice context and UI renders approved controls.
The branch rules identity is0.6.56; no main/release integration is claimed.

Fresh native regression51/51 passes (`/tmp/mastery-native-final.log`); mainEN/ES
and demoEN Training/Review Training/Fighter4 ASI controls pass at both sizes,
with five UI saves matching native expectations. Genuine0.6.55 campaign/combat
fixtures retain exact continuation.972 localized messages validate. The committed
acquisition tree is the tested revision; full batch verification is still due.

**No original issue closed:** eight combat properties and completed-Long-Rest
replacement transactions/UI remain outstanding. The pure replacement policy is
unit-tested, not yet a playable rest path. This entry is partial evidence, not a
claim that #60/#85 or the class issues are complete.

Weapon Mastery Long Rest checkpoint (commit containing this entry): five-class
PC/NPC replacement, one-use completed-rest windows, original source preservation,
level/rest history replay and conditional campaign18 are implemented. Interrupted,
ineligible and reserve members do not gain windows; applying or keeping a choice
preserves vitals, inventory and advancement. Review Training cannot undo earlier
replacements. Actual acquisition writer0.6.56/format15 remains byte-identical on
reload;1–17 compatibility is retained.

Fresh native51/51 passes (4.03s). MainEN/ES and demoEN rest controls pass at both
sizes, including keyboard/Cancel, sequential members, spell-before-training and
pending-codec reload; the existing full rest-control suite also passes.976 messages
validate. See [packet evidence](WEAPON-MASTERY.md#long-rest-implementation-evidence-wip).
Combat masteries remain unfinished. Saving pending choices is verified in the
codec, but an accessible Save button in the modal awaits MASTERY-7 approval.
No issue closure or completed full mastery implementation is claimed.

Weapon Mastery Sap/Vex checkpoint (commit containing this entry): chosen weapons
now apply sourced automatic combat effects, with next-roll consumption, exact
source-turn expiry, shared Advantage/Disadvantage, typed damage gating and
conditional FX6 persistence. Native coverage includes all applicable weapons,
PC/NPC physical/Light/thrown attacks, reactions, Surge, Sneak/Savage/Champion
continuation, bounds and legacy rejection. **51/51 native PASS**, mainEN/ES and
demoEN visible attack/log flows at both sizes pass;980 messages validate.
See [packet](WEAPON-MASTERY.md#sap-and-vex-combat-checkpoint) and
[tests](../tests/mastery_combat_checks.h). Six combat properties and pending
MASTERY-5/6/7 remain; #60/#85 are not closed. No added scope or new issues.


Weapon Mastery Nick checkpoint (commit containing this entry): chosen Nick kinds
now offer the approved physical-weapon selector and targeted extra attack, sharing
Light's once-per-turn allowance while preserving the Bonus Action. Current Attack
action timing, Surge, Two-Weapon Fighting, thrown items, Sneak/Savage/Vex/Champion
and conditional combat23 continuation have native evidence. Actual0.6.56 fixtures
retain old current-turn behavior; Nick activates at the next fresh turn. **51/51
native PASS**; mainEN/ES and demoEN Nick and neighboring Light UI/native checks
pass at both sizes;988 messages validate. See
[packet](WEAPON-MASTERY.md#nick-combat-checkpoint) and
[tests](../tests/nick_attack_checks.h). Three of eight properties are delivered;
Cleave/Graze/Push/Slow/Topple and MASTERY-5/6/7 remain. #60/#85 stay open.


Slow lifecycle checkpoint (commit containing this entry) supplies bounded sourced
Slow effects, stacking/expiry and shared movement/persistence consumers in SRD
rules0.6.58. **51/51 native PASS**,989 messages validate; actual0.6.57 continuation
is retained. [Evidence](WEAPON-MASTERY.md#slow-effect-lifecycle-checkpoint-combat-integration-incomplete).
This is partial rules support: the weapon-hit decision is not integrated and
Slow is not counted as a fourth playable property. No issue closure or new UI
verification is claimed. MASTERY-5/6/7 remain pending.

Weapon Mastery pending-save control (commit containing this entry): approved
MASTERY-7 Save game opens the existing campaign SaveSlots window and returns to
the same unapplied mastery selections after Cancel or saving. Reload retains
the entitlement and original grants; Apply then resolves it. Newly loaded town
views stay hidden while SaveSlots owns the exclusive window. MainEN/ES and
demoEN controls pass at1120×800/1920×1080; native grants/rest/save checks3/3 and
neighboring rest controls pass.989 localization messages validate.
[Evidence and commands](WEAPON-MASTERY.md#pending-mastery-save-control). MASTERY-5/6
are now approved but their combat implementation remains. No issue closed.

Graze checkpoint (commit containing this entry): both eligible weapons now offer
Use/Skip after a miss, retaining Action/Reaction costs and applying only the
attack ability modifier with the approved reduction/cap policy. Main/demo actual
controls and retained pending/reaction continuation work. All51 native checks,
mainEN/ES + demoEN at both sizes, neighboring Savage/Champion controls and993
localized messages pass. [Evidence](WEAPON-MASTERY.md#graze-playable-checkpoint)
and [native tests](../tests/graze_checks.h). Four of eight mastery properties are
playable. Slow/Topple choices, Cleave, Push and simultaneous ordering remain;
#60/#85 are still open. No added issue or scope.

### Weapon Mastery integration complete (#60/#85)

The commit containing this record completes all eight mastery properties in the
[approved batch](WEAPON-MASTERY.md#frozen-batch), including creation/review,
Fighter's fourth kind at4, Long Rest replacement and retained-save continuation.
Slow/Topple/Cleave/Push now use actual hits and the approved optional controls;
simultaneous Champion movement preserves separate entitlements and interrupted
routes. All calculations and decisions stay in the static SRD library.
[Mechanics, commands and evidence](WEAPON-MASTERY.md#optional-hit-mastery-integration).

Rules0.6.60/combat25 preserves prior supported formats and actual0.6.57–0.6.59
writer fixtures.51/51 native checks pass; mainEN/ES and demoEN rendered controls
pass at1120×800/1920×1080;1005 localized messages validate. This completes #60 and
Fighter #85's level1–4 style/mastery scope, with all four SRD styles supplied by
this and the preceding style/Light deliveries. #140/#147/#111/#103 retain their
other original requirements; Barbarian progression and all later-level/multiclass
requirements remain. No issue or feature scope was added. Source: [SRD Fighting
Style feats](https://www.dndbeyond.com/sources/dnd/br-2024/feats#FightingStyleFeats).

### Alert complete (#74)

Tested implementation **2320701**, pushed on`codex/srd-alert`; #74 closed
2026-09-26T16:39:33Z. GitHub GraphQL confirms141 open SRD issues. Criminal
characters from all12 creation/preset routes receive the fixed grant; existing
six level4 feat routes offer Alert without duplicate acquisition. Initiative
Proficiency and optional pre-turn swaps work through the approved main/demo
dialog, including multiple holders, incapacitation and exact pending saves.
Mechanics remain in the static SRD library. Rules0.6.61/PC40/combat26 retains all
previous supported formats; historical combats keep their rolls and current turn.

51/51 native tests pass in9.06s after affected rebuilds. MainEN/ES and demoEN
keyboard/mouse checks pass at1120×800/1920×1080; creation, Fighter advancement
with native save comparison and adjacent Savage/Champion controls pass.1014
localized messages validate. [Scope, mechanics, commands and timing](ALERT.md),
[native evidence](../tests/alert_checks.h), [rendered controls](../tests/alert_view_tests.gd).
One original requirement closed in approximately26 minutes after approval;
this excludes preflight and approval wait. No new issue or scope was added.
Human's independent extra Origin-feat source remains#71; background equipment
and other class progression remain their original issues. Main stays375f58f;
branch delivery is not a merge or release claim.
