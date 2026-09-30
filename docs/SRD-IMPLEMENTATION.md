# Incremental SRD implementation plan

Date: 2026-09-23. Status: issue backlog created; incremental implementation started.

Execution policy updated 2026-09-24: [SRD-WORKFLOW.md](SRD-WORKFLOW.md) and
[AGENTS.md](../AGENTS.md) now define one active **delivery batch**. Related
requirements may share implementation and verification; historical “one per
increment” and child-splitting language below describes review boundaries,
not mandatory new tickets or separate releases. Preserve all acceptance criteria.
Use the [batch grouping](SRD-BATCHING-REVIEW.md) and [decision register](SRD-DECISIONS.md).

The [GitHub issue index](https://github.com/stdarg/OpenGold/issues/186) links all
steps, labeled `SRD_improvements`. The [coverage ledger](SRD-COVERAGE.md) records
completed work and verification. I01–I08 are delivered in
[#20](https://github.com/stdarg/OpenGold/issues/20),
[#21](https://github.com/stdarg/OpenGold/issues/21),
[#22](https://github.com/stdarg/OpenGold/issues/22),
[#23](https://github.com/stdarg/OpenGold/issues/23),
[#24](https://github.com/stdarg/OpenGold/issues/24),
[#25](https://github.com/stdarg/OpenGold/issues/25),
[#26](https://github.com/stdarg/OpenGold/issues/26) and
[#27](https://github.com/stdarg/OpenGold/issues/27).
[F01, feature and feat provenance](https://github.com/stdarg/OpenGold/issues/28) is also delivered;
F02 is split into [rules and persistence (#187)](https://github.com/stdarg/OpenGold/issues/187),
[creation controls (#188)](https://github.com/stdarg/OpenGold/issues/188), and
[missing-choice review (#189)](https://github.com/stdarg/OpenGold/issues/189).
The rules/persistence and creation-control children are delivered. See [training
support](TRAINING.md) for the first Rogue/Criminal package and boundaries.
F03 is split into [resource rules and persistence (#190)](https://github.com/stdarg/OpenGold/issues/190),
[campaign rest transactions (#191)](https://github.com/stdarg/OpenGold/issues/191),
[rest controls (#192)](https://github.com/stdarg/OpenGold/issues/192), and
[interruption/resumption (#193)](https://github.com/stdarg/OpenGold/issues/193).
All four children are delivered, including reviewed controls and interruption/
resumption with Q37 safe recovery. Hit Dice spending must allow a decision after each
roll, rather than requiring every die to be committed beforehand. The first
resource rules are independent of the remaining F02 saved-choice controls.

F04 is split into [recovery clocks (#194)](https://github.com/stdarg/OpenGold/issues/194)
and [campaign scheduling (#195)](https://github.com/stdarg/OpenGold/issues/195).
Both children are delivered. The separate
[Help/Medicine follow-up (#32)](https://github.com/stdarg/OpenGold/issues/32) remains
open. See [recovery clock support](RECOVERY-CLOCKS.md).

This breaks the [SRD audit](audits/srd-5.2.1-rules.md) into bounded changes.
The target includes all twelve SRD classes, all nine species, backgrounds,
feats, spells, progression and recovery. The first complete class milestone is
levels 1–4 across all twelve classes. Later milestones cover single-class
progression through 20 and multiclassing. This does not select a campaign cap.

The implementation follows [TECH](TECH.md): C++20 rules and campaign code,
Godot 4.x/GDExtension presentation, existing build tools, value ownership and
RAII. No replacement framework or general-purpose rules engine is proposed.

## Decisions already approved

The user explicitly approved all four changes. These decisions supersede the
corresponding current behavior and the audit's pending policy questions:

1. An attack preserves any remaining legal movement, Bonus Actions and actions.
2. Opportunity attacks use SRD triggers; turning to face a target adds no trigger.
3. Movement through allies follows the SRD, including restrictions on stopping
   in occupied spaces.
4. Versatile weapons allow one-handed and two-handed use with the appropriate
   damage. Remove the forced two-hand exceptions identified by the audit.

Other policies, including starting ability-score restrictions, campaign
equipment/wealth and all-unconscious party defeat, remain separate decisions.
Approval of these four changes does not decide those policies or UI layouts.

## Size and completion of an increment

An increment changes one principal behavior or a small group with the same
state transition. It includes the necessary rules, campaign integration,
persistence, relevant presentation and checks. A larger milestone contains
multiple increments; it is not a promise of completion in one execution.

Before starting an increment, inspect the current branch and confirm its exact
scope, prerequisites and independent expected results. Reconcile work already
completed by other contributors. Bound oversized work with named acceptance substeps before coding; create a
separate issue only for a genuine independent dependency/deliverable. A feature
that changes saved state changes the current format in place, with no migration;
see [saves](SAVES.md#pre-10-format-policy).

Each completed increment must:

- Meet named SRD examples and applicable audit acceptance criteria, including
  failure paths and resource limits.
- Preserve deterministic continuation and rejected-command atomicity.
- Pass relevant native tests and save/reload checks. Run Godot acceptance when
  controls or presentation change; report unavailable checks explicitly.
- Update the coverage ledger and affected support documentation.
- Leave the repository buildable, commit only the increment's own changes and
  push the current branch to `origin`.
- Report the behavior delivered, tests run, remaining limitations and next item.

One delivery batch is the default execution unit; it may close several related
issues. Existing authorization for rules does not need to be repeated. New UI
choices require the numbered layout/control questions specified by
[AGENTS.md](../AGENTS.md). Gather those questions before the affected increment;
reuse established scenes and controls. This plan selects no new layouts.

The audit's eight passing test suites are a starting baseline, not evidence
that the new features work. Use expectations from the pinned SRD rather than
copying the implementation into tests.

## First milestone: correct the existing implementation

These eight increments are concrete enough to schedule now, in this order.
The first increment also opens the coverage ledger with the audit finding IDs;
later increments update it as part of their own work.

| ID | Bounded change | Completion check |
| --- | --- | --- |
| I01 | Correct Rogue/Monk weapon proficiency using weapon properties and class grants (E5). Add the Light property needed by this calculation; other Light combat mechanics come later. | Shortsword/Scimitar attacks include the owed proficiency bonus; equipment notes agree; unrelated classes remain unchanged; save/reload works. |
| I02 | Correct initial death-save processing and reset counters on stabilization (E2/E3). Use one turn-entry path. | An unstable first actor rolls exactly once; natural-20 recovery permits its turn; Stable clears both counters; checkpoint restore adds no roll. |
| I03 | Correct HP history and retroactive Constitution increases (E1), including profile validation and migration. | The audit's low-Constitution example reaches 9 maximum HP; normal growth and Dwarf HP still work; current deficits and unconscious state survive migration/reload. |
| I04 | Separate background and level-up ability adjustments (E4). Reconstruct provenance where existing history permits and correct current modifier explanations. | Background +2 and level-4 feat +2 remain separate, total correctly, and survive saves; no new layout is required for the text correction. |
| I05 | Preserve remaining turn resources after attacks and damaging spells. Update combat flow and AI turn completion to respect remaining choices. | Attack then move and attack then Second Wind work; a spent action cannot be reused; explicit End Turn still works; enemies finish their turns; pending reactions remain ordered. |
| I06 | Remove facing-based opportunity reactions while preserving actual SRD leave-reach reactions. Keep facing as presentation state. | Turning in place causes no reaction; leaving reach interrupts at the correct point; Disengage, visibility and spent reactions still apply; saved pending reactions migrate consistently. |
| I07 | Permit movement through allied occupied spaces. Update path planning, execution and checkpoint validation together. | Allied transit has the correct cost, stopping in an occupied cell stays restricted, terrain and diagonal walls still apply, and saved movement resumes correctly. Other size/incapacitation transit cases join the later creature-state increment. |
| I08 | Implement Versatile grip and damage, removing the four forced two-hand exceptions. Confirm the grip control before implementation. | Legal one-handed use works with a shield; two-handed melee uses the alternate die and requires available hands; grip, equipment and save state agree; incompatible changes are atomic. |

**Milestone exit:** E1–E5 are closed, the four approved policy changes work
through normal game flows, and documentation no longer describes the old
behavior as current. This milestone does not claim complete class support.

## Shared mechanics: deliver each through a real feature

These are subsequent increment templates, with explicit scope limits. They
are interleaved with the character/class work below as prerequisites permit.
Rows are acceptance boundaries that may share a delivery batch. Do not build
every abstraction before delivering additional character behavior.

| ID | Scope and first use | Acceptance boundary / split rule |
| --- | --- | --- |
| F01 | Persist acquired feature/feat IDs, source and choices; migrate the existing Soldier and advancement feats. | Creation grants no longer depend on the level-four feat count; duplicate/prerequisite rules are validated; totals and saves remain stable. Adding new feats is separate. |
| F02 | Skills, tools and languages with source grants and choice validation. | Exercise one real class/background package, a duplicate grant and Expertise. Campaign uses of individual skills are separate increments. |
| F03 | Resource pools and Short Rests, using Fighter Second Wind and spendable Hit Dice. | Selected Hit Dice spending, partial recharge, Long Rest recovery and individual eligibility persist. Other class pools are added with their own feature. |
| F04 | Death saves and stable natural recovery continue outside combat. | Time partitioning, reserves, save/reload and encounter transitions agree. Add stabilization/Medicine as its own small follow-up if its controls or check flow are not yet available. |
| F05 | Typed damage and resistance/vulnerability/immunity, verified with an actual species resistance. | Mixed damage, rounding, overlapping grants and zero damage resolve correctly. Temporary HP and its replacement choice form F05b, a separate increment. |
| F06 | Extend conditions and source-based modifiers only for the next concrete feature. | One condition or closely coupled condition family per increment, including duration, removal, stacking and saves. Repeat until the coverage ledger is complete. |
| F07 | Persist spell grants, cantrips, books and prepared lists, starting with the existing Wizard spells. | [F07a #199](https://github.com/stdarg/OpenGold/issues/199) separates Wizard cantrips/book/preparation; [F07c #200](https://github.com/stdarg/OpenGold/issues/200) adds source-specific free casts with a real granting feature. Parent #36 remains open until both are complete. Wizard learning/preparation controls are #37; adapting Cleric and each other preparation policy is separate. |
| F08 | Concentration, exercised by one representative low-level spell. | Replacement, damage saves, incapacitation, voluntary release and save/reload work; one spell is enough to prove the mechanism. |
| F09 | Spell components and casting eligibility. | Existing spells exercise V/S and hand availability; material/focus/cost/consumption cases form a separate F09b increment using named spells. |
| F10 | Spell reaction windows, starting with Shield. | Trigger selection, decline, resource expenditure, one-slot-per-turn handling and checkpoint resumption are correct. Other reaction features are separate work. |
| F11 | Multiple targets using Magic Missile and Scorching Ray. | Legal allocation, dead/invalid targets, resource use and deterministic damage work through the reviewed targeting controls. |
| F12 | Area targeting and effect resolution using one simple spell. | Bounds, target eligibility, cover/line of effect, saves and persistence work. New shapes or ongoing areas are separate increments when they add distinct mechanics. |
| F13 | Creature size and occupied-space rules beyond I07. | Implement remaining Tiny/incapacitated/size-difference transit cases; introduce large footprints with one actual creature fixture in a separate follow-up if needed. |
| F14 | Senses, light and visibility for actual species/feature grants. | Split sight/light rules and nonvisual senses into separate increments; verify attacks, targeting and reactions against the same visibility queries. |

The coverage ledger records dependencies between instantiated increments. For
example, Shield follows spell grants and reaction support; Rage follows typed
damage/resources; lineage spells follow spell grants; Wild Shape follows size,
forms, resources and Temporary HP. Add prerequisite work before exposing a
choice that cannot yet function.

## Creation, origins and equipment

These queues close the creation portions of G2/G3/G9. Each entry below is a
series of small increments, not one combined delivery.

| Queue | Increment boundary | Completion gate |
| --- | --- | --- |
| Equipment | Armor catalog/training; missing weapon definitions; Heavy/Loading behavior; unlimited ranged ammunition (approved exception; see [policy](AMMUNITION.md)); thrown inventory; Light attacks; individual mastery properties. Split any new control flow from unrelated properties. | Every ordinary SRD equipment option required by class/background packages is usable, with correct prerequisites, hands, damage, resources and persistent inventory. |
| Feats | One feat per increment, or a small group sharing already-proven mechanics. Start with Origin feats and Fighter level-one Fighting Styles, then General feats. | Actual entitlement, choice, prerequisites, repeatability and effects work; a label in the sheet is insufficient. Epic Boons follow their level milestone. |
| Backgrounds | One complete background package at a time, after its feat and proficiency dependencies. | All four packages grant their skills/tools/feat and reviewed starting equipment/wealth policy; duplicate choices are handled. |
| Species | One species' passive traits, then one active trait or lineage per increment. | All nine species and their SRD choices work through creation, derived statistics, actions, recovery and saves. Include Human's extra Origin feat and level-dependent traits in the ledger. |
| Starting class choices | One class's proficiency/equipment/spell-choice integration at a time. | A normally created character receives every level-one entitlement; fixed fixtures cannot substitute for the ordinary creation path. |

Before equipment package integration, resolve how SRD starting entitlements
interact with the campaign's preview wealth and original-item conversion.
Before species/feat/class selectors are implemented, obtain the required UI
layout and control decisions. Those questions do not reopen the four already
approved rules changes.

## Class features through level 4

For each row, treat the semicolon-separated groups as candidate increments.
Split a group further when it introduces separate state machines, targeting
flows or adaptations. The audit's class table and the SRD tables remain the
complete inventory: these groups organize the work rather than omit unlisted
entitlements. The level-four integration check is its own final increment for
each class.

| Class | Proposed feature increments, after shared prerequisites |
| --- | --- |
| Fighter | Level-one styles and mastery grants; Action Surge; Tactical Mind; Champion features; level-four choices/resource integration. |
| Cleric | Preparation/cantrip rules and Divine Order choices; Channel Divinity/Divine Spark; Turn Undead; Life Domain healing features and domain spells; level-four integration. |
| Wizard | Spellbook learning/copying and preparation; Ritual Adept; Arcane Recovery; Scholar; Evoker features and spell grants; level-four integration. |
| Barbarian | Rage activation/duration/recharge; Rage effects; Reckless Attack/Danger Sense; Primal Knowledge; Berserker features; level-four integration. |
| Rogue | Expertise and starting choices; Sneak Attack and its timing; Cunning Action; Steady Aim; Thief features, separated where exploration/item actions need support; level-four integration. |
| Monk | Martial Arts and eligible weapon/unarmed use; Focus and its individual action options; movement/recovery features; Deflect Attacks; Slow Fall; Open Hand techniques; level-four integration. |
| Bard | Spell access/preparation policy; Bardic Inspiration; Expertise/Jack of All Trades; Lore features and Cutting Words; level-four integration. |
| Sorcerer | Spell access and Innate Sorcery; Sorcery Points and slot conversion; individual Metamagic options; Draconic features/spells; level-four integration. |
| Paladin | Level-one spellcasting; Lay On Hands; mastery/style choices; Smite trigger and casting; Channel Divinity/Devotion features and spells; level-four integration. |
| Ranger | Level-one spellcasting; Hunter's Mark/free-cast grants; mastery/style choices; Deft Explorer; Hunter features; level-four integration. |
| Druid | Spell access/Primal Order choices; Wild Shape form catalog; transformation and retained statistics; form actions/reversion; Wild Companion; Land features/spells; level-four integration. |
| Warlock | Pact Magic; individual invocations, including their prerequisites; Magical Cunning; Fiend features/spells; level-four integration. |

Suggested order is Fighter, Cleric and Wizard to exercise existing paths, then
Barbarian/Rogue/Monk, Bard/Sorcerer, Paladin/Ranger and Druid/Warlock. Prerequisites
can justify reordering. Every class is mandatory for the milestone; completing
the first three does not close it.

## Spells and exploration: a continuing queue

Spell implementation runs alongside class features and is required before a
class/level is marked complete. First inventory every SRD spell accessible in
the target tier, including species, feats and subclass grants. The inventory
must link each spell to its implementation, tests and any reviewed adaptation.

The [level-four spell inventory](SPELL-INVENTORY.md) records 106 distinct spells
(139 before DM-1 removed 33 DM-adjudicated spells),
their class and additional grant routes, dependencies and current evidence.
It reconciles the printed class lists with spell descriptions, including
Phantasmal Force and Sorcerer access to Mind Spike. Current counts and delivery evidence live in the coverage ledger; do not
maintain another status snapshot here. #165 remains open for implementation and source-route
integration. The first new single-spell children are #202–205; rows still marked
as queued require a named bounded batch checklist before coding, linked to #165;
a new issue is needed only for a genuinely separate deliverable.

- A batch contains at most three to five straightforward spells using already
  verified mechanics. Name the spells and expected behaviors before starting.
- A spell introducing a new mechanic gets its own increment and any explicit
  prerequisites. Summoning, forms and open-ended effects are not grouped into
  a routine spell batch.
- Complete the six existing spell paths first as dependencies become ready,
  including split targeting, components and Blindness/Deafness choices.
- Include exploration uses, duration, concentration, upcasting, targeting,
  material costs and recovery as applicable. A combat button alone is not
  completion.
- Divide campaign skills, companions, noncombat magic and later resurrection
  into one capability per increment. Propose any authored adaptation before
  implementation; expose its actual supported behavior in the ledger.

**All-class level-four milestone:** each class passes ordinary creation,
equipment, combat, rewards, confirmed advancement, rest, another encounter and
save/reload at every level through 4. Required species/background/feat choices
and all available SRD spells/features for that tier are functional, or have a
specifically reviewed adaptation. Run mixed parties to verify interactions.

## Higher levels and multiclassing

Use levels 5–10, 11–16 and 17–20 as review milestones. Inside them, implement one
level transition and its features in separate bounded increments:

1. Add the shared progression change, such as proficiency or cantrip scaling,
   with independent boundary tests. Do not expose incomplete higher levels.
2. Add each class's newly acquired feature/resource/subclass changes, one
   feature family per increment. A complex feature is split further.
3. Add the spells/feats/species progression required at that level through the
   same small queues above.
4. Integrate and verify that class's level transition, then close the level
   across all twelve classes before declaring it complete.

This keeps Extra Attack, additional feats, subclass schedules, Epic Boons and
capstones tied to their actual grants. It does not apply one generic table to
all classes. The rules-library target and campaign's playable cap remain
distinct decisions.

Multiclassing follows as six increments, split further if necessary:

| ID | Scope | Completion check |
| --- | --- | --- |
| M01 | Acquired class levels/history and entry prerequisites. | Total/class levels remain distinct; single-class saves round-trip; illegal entry is rejected atomically. |
| M02 | Entry proficiencies, mixed Hit Dice and source grants. | Starting-class benefits differ from later entry; duplicate grants and rest spending remain correct. |
| M03 | Combined Spellcasting slots versus per-class access. | Slot level does not grant unauthorized known/prepared spells; multiple casting abilities remain correct. |
| M04 | Pact Magic, free casts and later special spell resources. | Their independent recharge and casting interactions match explicit conformance cases. |
| M05 | Nonstacking and competing class features. | AC formulas, Extra Attack and other overlapping grants resolve without double counting. |
| M06 | Normal creation/advancement controls and combination acceptance. | Representative martial/caster/Pact combinations work through the full campaign lifecycle and saves; remaining interaction gaps stay explicit. |

Final closure reconciles every audit finding and every class/species/feat/spell
ledger entry. Passing a representative party test does not prove all spell or
multiclass interactions; targeted checks remain attached to their rules.

F09 is split into [existing spell component definitions and Somatic hands
(#201)](https://github.com/stdarg/OpenGold/issues/201), and the remaining live
speech-blocking sources tracked by [#39](https://github.com/stdarg/OpenGold/issues/39).
The equipment eligibility child is delivered; it does not close verbal casting
restrictions or material/focus mechanics (#40). See [spell components](SPELL-COMPONENTS.md).

## Recommended next execution

I01–I08, F01, F02a and F02b are complete. #189's Review Training dialog, which
only completed training missing from old saves, was removed at the
[pre-1.0 save cutoff](SAVES.md#pre-10-format-policy).
F03a–d are complete: #192's reviewed rest controls and #193's interruption/
resumption, sleep/equipment handoff and Q37 safe recovery complete #30. See [rest resources](REST-RESOURCES.md)
for the implemented boundary and verification.
F04a and F04c complete #31. Help/Medicine controls and checks remain #32.
F05 #33 supplies [typed damage and Dwarf Poison resistance](DAMAGE.md).
F05b #34 is split into #196 (native Temporary HP state, absorption, expiry and
persistence) and #197 (Orc Adrenaline Rush and the reviewed HP/replacement controls).
Keep #34 open until its ordinary gameplay and campaign acceptance are complete.
See [Temporary HP](TEMPORARY-HP.md).
The later queues are refined
into named increments from the verified state after each milestone, without
changing the all-class completion target.

This document records decisions and work boundaries. Implementation status and
evidence are maintained in the coverage ledger and linked issues.

Rules 0.6.22 adds [Poison Spray and explicit Wizard cantrip choices](POISON-SPRAY.md),
with the approved creation and main combat controls. Campaign saves store choices;
the character profile validates their grants.
Full spell selection, speech blocking and other granting sources remain open.
