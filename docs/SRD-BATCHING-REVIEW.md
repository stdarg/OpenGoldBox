# SRD backlog batching review

Reviewed 2026-09-24 against GitHub issue bodies, repository-wide issue comments,
`main` at `8429158`, the coverage ledger, handoff and relevant feature documents.
This is a planning review, not certification that any open issue is complete.
The implementation goal remains **paused**. No issue is closed, deleted or
re-scoped by this document. Workflow recommendations were adopted by the user on 2026-09-24 and are now
implemented in AGENTS.md and SRD-WORKFLOW.md. Issue consolidation/removal
recommendations remain proposals; no issue acceptance has been reduced.

## Baseline and findings

- 210 labeled issues: **168 open, 42 closed**. Of the original issues/index,
  16 are closed and 152 open; subsequent issues account for 26 closed and 16 open.
- Every open issue is assigned exactly once below: **158 in 24 work families,
  10 in the cross-cutting tracker group**. Cross-references are dependencies,
  not extra work counted again. Families are scheduling groups, not promises
  that a whole class or system fits one commit or two hours.
- The ledger records **139 required spells: 11 partial, 128 missing**. Most
  missing spells are rows under #165, not separate issues. Consequently neither
  168 open issues nor 42 closures measures functional completion or remaining hours.
- The plan/workflow encourages fragmentation: one issue at a time, mandatory
  child issues for many families, and repeated shared checks per small delivery.
  Batching must replace those rules explicitly rather than silently ignore them.
- Generic dependencies are often too broad. For example, all Monk items list
  Shield/reaction issue #41, though starting tools/Martial Arts do not require
  finished spell-reaction casting. Use the actual capability needed by the
  active acceptance case. Preserve real dependencies and test interactions.

## Grouped backlog

The appendix preserves every issue title and primary assignment. These groups
organize the current backlog; they do not create 24 new GitHub issues.

| Group | Work family | Open issues |
| --- | --- | --- |
| T | Cross-cutting acceptance and inventory trackers | [#8](https://github.com/stdarg/OpenGold/issues/8), [#35](https://github.com/stdarg/OpenGold/issues/35), [#48](https://github.com/stdarg/OpenGold/issues/48), [#49](https://github.com/stdarg/OpenGold/issues/49), [#50](https://github.com/stdarg/OpenGold/issues/50), [#51](https://github.com/stdarg/OpenGold/issues/51), [#52](https://github.com/stdarg/OpenGold/issues/52), [#165](https://github.com/stdarg/OpenGold/issues/165), [#185](https://github.com/stdarg/OpenGold/issues/185), [#186](https://github.com/stdarg/OpenGold/issues/186) |
| B01 | Origin packages and training completion | [#29](https://github.com/stdarg/OpenGold/issues/29), [#61](https://github.com/stdarg/OpenGold/issues/61), [#62](https://github.com/stdarg/OpenGold/issues/62), [#63](https://github.com/stdarg/OpenGold/issues/63), [#64](https://github.com/stdarg/OpenGold/issues/64), [#71](https://github.com/stdarg/OpenGold/issues/71), [#74](https://github.com/stdarg/OpenGold/issues/74), [#77](https://github.com/stdarg/OpenGold/issues/77), [#189](https://github.com/stdarg/OpenGold/issues/189) |
| B02 | Rest, stabilization and campaign HP | [#30](https://github.com/stdarg/OpenGold/issues/30), [#32](https://github.com/stdarg/OpenGold/issues/32), [#34](https://github.com/stdarg/OpenGold/issues/34), [#192](https://github.com/stdarg/OpenGold/issues/192), [#193](https://github.com/stdarg/OpenGold/issues/193) |
| B03 | Weapon actions, equipment and fighting styles | [#56](https://github.com/stdarg/OpenGold/issues/56), [#57](https://github.com/stdarg/OpenGold/issues/57), [#58](https://github.com/stdarg/OpenGold/issues/58), [#59](https://github.com/stdarg/OpenGold/issues/59), [#60](https://github.com/stdarg/OpenGold/issues/60), [#78](https://github.com/stdarg/OpenGold/issues/78), [#79](https://github.com/stdarg/OpenGold/issues/79), [#80](https://github.com/stdarg/OpenGold/issues/80), [#81](https://github.com/stdarg/OpenGold/issues/81), [#83](https://github.com/stdarg/OpenGold/issues/83), [#198](https://github.com/stdarg/OpenGold/issues/198) |
| B04 | Fighter progression and Champion | [#82](https://github.com/stdarg/OpenGold/issues/82), [#84](https://github.com/stdarg/OpenGold/issues/84), [#85](https://github.com/stdarg/OpenGold/issues/85), [#87](https://github.com/stdarg/OpenGold/issues/87), [#88](https://github.com/stdarg/OpenGold/issues/88), [#89](https://github.com/stdarg/OpenGold/issues/89) |
| B05 | Spatial rules, senses and hiding | [#44](https://github.com/stdarg/OpenGold/issues/44), [#45](https://github.com/stdarg/OpenGold/issues/45), [#46](https://github.com/stdarg/OpenGold/issues/46), [#47](https://github.com/stdarg/OpenGold/issues/47), [#219](https://github.com/stdarg/OpenGold/issues/219) |
| B06 | Species traits and lineages | [#65](https://github.com/stdarg/OpenGold/issues/65), [#66](https://github.com/stdarg/OpenGold/issues/66), [#67](https://github.com/stdarg/OpenGold/issues/67), [#68](https://github.com/stdarg/OpenGold/issues/68), [#69](https://github.com/stdarg/OpenGold/issues/69), [#70](https://github.com/stdarg/OpenGold/issues/70), [#72](https://github.com/stdarg/OpenGold/issues/72), [#73](https://github.com/stdarg/OpenGold/issues/73) |
| B07 | Spell access, preparation and free casts | [#36](https://github.com/stdarg/OpenGold/issues/36), [#37](https://github.com/stdarg/OpenGold/issues/37), [#75](https://github.com/stdarg/OpenGold/issues/75), [#97](https://github.com/stdarg/OpenGold/issues/97), [#200](https://github.com/stdarg/OpenGold/issues/200) |
| B08 | Concentration, components and persistent areas | [#38](https://github.com/stdarg/OpenGold/issues/38), [#39](https://github.com/stdarg/OpenGold/issues/39), [#40](https://github.com/stdarg/OpenGold/issues/40), [#43](https://github.com/stdarg/OpenGold/issues/43), [#171](https://github.com/stdarg/OpenGold/issues/171), [#208](https://github.com/stdarg/OpenGold/issues/208), [#209](https://github.com/stdarg/OpenGold/issues/209) |
| B09 | Reaction casting and split targets | [#41](https://github.com/stdarg/OpenGold/issues/41), [#42](https://github.com/stdarg/OpenGold/issues/42), [#168](https://github.com/stdarg/OpenGold/issues/168), [#170](https://github.com/stdarg/OpenGold/issues/170) |
| B10 | Complete existing spell families and object targets | [#166](https://github.com/stdarg/OpenGold/issues/166), [#167](https://github.com/stdarg/OpenGold/issues/167), [#169](https://github.com/stdarg/OpenGold/issues/169), [#202](https://github.com/stdarg/OpenGold/issues/202), [#203](https://github.com/stdarg/OpenGold/issues/203), [#204](https://github.com/stdarg/OpenGold/issues/204), [#205](https://github.com/stdarg/OpenGold/issues/205), [#223](https://github.com/stdarg/OpenGold/issues/223), [#225](https://github.com/stdarg/OpenGold/issues/225) |
| B11 | Wizard class completion | [#96](https://github.com/stdarg/OpenGold/issues/96), [#98](https://github.com/stdarg/OpenGold/issues/98), [#99](https://github.com/stdarg/OpenGold/issues/99), [#100](https://github.com/stdarg/OpenGold/issues/100), [#101](https://github.com/stdarg/OpenGold/issues/101), [#102](https://github.com/stdarg/OpenGold/issues/102) |
| B12 | Cleric class completion | [#90](https://github.com/stdarg/OpenGold/issues/90), [#91](https://github.com/stdarg/OpenGold/issues/91), [#92](https://github.com/stdarg/OpenGold/issues/92), [#93](https://github.com/stdarg/OpenGold/issues/93), [#94](https://github.com/stdarg/OpenGold/issues/94), [#95](https://github.com/stdarg/OpenGold/issues/95) |
| B13 | Barbarian class completion | [#103](https://github.com/stdarg/OpenGold/issues/103), [#104](https://github.com/stdarg/OpenGold/issues/104), [#105](https://github.com/stdarg/OpenGold/issues/105), [#106](https://github.com/stdarg/OpenGold/issues/106), [#107](https://github.com/stdarg/OpenGold/issues/107), [#108](https://github.com/stdarg/OpenGold/issues/108), [#109](https://github.com/stdarg/OpenGold/issues/109) |
| B14 | Rogue class completion | [#110](https://github.com/stdarg/OpenGold/issues/110), [#111](https://github.com/stdarg/OpenGold/issues/111), [#112](https://github.com/stdarg/OpenGold/issues/112), [#113](https://github.com/stdarg/OpenGold/issues/113), [#114](https://github.com/stdarg/OpenGold/issues/114), [#115](https://github.com/stdarg/OpenGold/issues/115), [#116](https://github.com/stdarg/OpenGold/issues/116), [#220](https://github.com/stdarg/OpenGold/issues/220), [#221](https://github.com/stdarg/OpenGold/issues/221) |
| B15 | Monk class completion | [#117](https://github.com/stdarg/OpenGold/issues/117), [#118](https://github.com/stdarg/OpenGold/issues/118), [#119](https://github.com/stdarg/OpenGold/issues/119), [#120](https://github.com/stdarg/OpenGold/issues/120), [#121](https://github.com/stdarg/OpenGold/issues/121), [#122](https://github.com/stdarg/OpenGold/issues/122), [#123](https://github.com/stdarg/OpenGold/issues/123), [#124](https://github.com/stdarg/OpenGold/issues/124) |
| B16 | Bard class completion | [#125](https://github.com/stdarg/OpenGold/issues/125), [#126](https://github.com/stdarg/OpenGold/issues/126), [#127](https://github.com/stdarg/OpenGold/issues/127), [#128](https://github.com/stdarg/OpenGold/issues/128), [#129](https://github.com/stdarg/OpenGold/issues/129), [#130](https://github.com/stdarg/OpenGold/issues/130) |
| B17 | Sorcerer class completion | [#131](https://github.com/stdarg/OpenGold/issues/131), [#132](https://github.com/stdarg/OpenGold/issues/132), [#133](https://github.com/stdarg/OpenGold/issues/133), [#134](https://github.com/stdarg/OpenGold/issues/134), [#135](https://github.com/stdarg/OpenGold/issues/135), [#136](https://github.com/stdarg/OpenGold/issues/136) |
| B18 | Paladin class completion | [#137](https://github.com/stdarg/OpenGold/issues/137), [#138](https://github.com/stdarg/OpenGold/issues/138), [#139](https://github.com/stdarg/OpenGold/issues/139), [#140](https://github.com/stdarg/OpenGold/issues/140), [#141](https://github.com/stdarg/OpenGold/issues/141), [#142](https://github.com/stdarg/OpenGold/issues/142), [#143](https://github.com/stdarg/OpenGold/issues/143) |
| B19 | Ranger class completion | [#144](https://github.com/stdarg/OpenGold/issues/144), [#145](https://github.com/stdarg/OpenGold/issues/145), [#146](https://github.com/stdarg/OpenGold/issues/146), [#147](https://github.com/stdarg/OpenGold/issues/147), [#148](https://github.com/stdarg/OpenGold/issues/148), [#149](https://github.com/stdarg/OpenGold/issues/149), [#150](https://github.com/stdarg/OpenGold/issues/150) |
| B20 | Druid class completion | [#151](https://github.com/stdarg/OpenGold/issues/151), [#152](https://github.com/stdarg/OpenGold/issues/152), [#153](https://github.com/stdarg/OpenGold/issues/153), [#154](https://github.com/stdarg/OpenGold/issues/154), [#155](https://github.com/stdarg/OpenGold/issues/155), [#156](https://github.com/stdarg/OpenGold/issues/156), [#157](https://github.com/stdarg/OpenGold/issues/157), [#158](https://github.com/stdarg/OpenGold/issues/158) |
| B21 | Warlock class completion | [#159](https://github.com/stdarg/OpenGold/issues/159), [#160](https://github.com/stdarg/OpenGold/issues/160), [#161](https://github.com/stdarg/OpenGold/issues/161), [#162](https://github.com/stdarg/OpenGold/issues/162), [#163](https://github.com/stdarg/OpenGold/issues/163), [#164](https://github.com/stdarg/OpenGold/issues/164) |
| B22 | Campaign checks, companions and exploration magic | [#172](https://github.com/stdarg/OpenGold/issues/172), [#173](https://github.com/stdarg/OpenGold/issues/173), [#174](https://github.com/stdarg/OpenGold/issues/174) |
| B23 | Later single-class levels and recovery magic | [#175](https://github.com/stdarg/OpenGold/issues/175), [#176](https://github.com/stdarg/OpenGold/issues/176), [#177](https://github.com/stdarg/OpenGold/issues/177), [#178](https://github.com/stdarg/OpenGold/issues/178) |
| B24 | Multiclassing | [#179](https://github.com/stdarg/OpenGold/issues/179), [#180](https://github.com/stdarg/OpenGold/issues/180), [#181](https://github.com/stdarg/OpenGold/issues/181), [#182](https://github.com/stdarg/OpenGold/issues/182), [#183](https://github.com/stdarg/OpenGold/issues/183), [#184](https://github.com/stdarg/OpenGold/issues/184) |

## Delivery boundaries and dependencies

### B01: Origin packages and training completion

Reuse one choice/validation flow for Alert, Skilled, Human choices and the four backgrounds. First package: #29/#189. Then package feats with their consuming backgrounds. Preserve equipment/wealth policy as a decision, not an invented default.

Prerequisites / coordination: Q11; B03 equipment; B07 Magic Initiate for Acolyte/Sage; B05/B06 where applicable.

### B02: Rest, stabilization and campaign HP

Deliver #30/#192/#193 as a rest workflow, with sequential Hit Dice and interruption/resumption. Deliver #32 via real Medicine checks, and the remaining #34 exploration Temporary HP path with Orc #72. Reuse recharge/time state across class resources.

Prerequisites / coordination: Rest-control decisions; B01 skill grants are already partly usable; B22 campaign actions.

### B03: Weapon actions, equipment and fighting styles

Batch #59/#81 with Nick from #60; #56/#58 share attack/inventory accounting; #57 tracking was explicitly declined in favor of [unlimited ammunition](AMMUNITION.md); #78/#79/#80 share style grants with #85/#140/#147. Keep Grappler and armor timing as separate packages within this family. Never implement all masteries as one undifferentiated change.

Prerequisites / coordination: Q23 for Great Weapon Fighting; B05 displacement/Prone/grapple geometry; B02 elapsed-time hooks for #198.

### B04: Fighter progression and Champion

Finish Fighter-specific starting/style/mastery access, Tactical Mind, Champion and level-four/ASI integration together with required B03/B22 capabilities. #89 owns attained-level acceptance, not a second implementation.

Prerequisites / coordination: B01/B02/B03; actual ability checks from B22 for Tactical Mind. No need to wait for unrelated spell catalogs.

### B05: Spatial rules, senses and hiding

Share occupancy/reach/cover/sight queries for #44/#45, light and senses #46/#47, and actual Hide #219. Hide is a separate runnable package once its sight/cover dependencies exist. Include Prone/other required conditions from #35.

Prerequisites / coordination: Reviewed geometry/visibility controls; B06 supplies real sense grants; B14 consumes Hide.

### B06: Species traits and lineages

Batch passive traits using the same modifiers, active resource traits using the same lifecycle, and spell lineages with B07/B10. Keep a per-species acceptance checklist. Do not duplicate already delivered Dwarf HP/resistance or Orc combat Adrenaline Rush.

Prerequisites / coordination: B02 recovery/exploration traits; B05 senses/size; B07 spells; B08 areas for breath effects; higher-level traits remain B23.

### B07: Spell access, preparation and free casts

One implementation owner for Wizard #37/#97; finish #36/#200 through real Magic Initiate #75 and a consuming background. Share source/access validation across class policies, but preserve prepared, learned, book and Pact differences.

Prerequisites / coordination: Selection/preparation/free-cast control review; class policies from B11–B21; spell effects from B08–B10.

### B08: Concentration, components and persistent areas

Deliver #38/#208/#209 through one real Silence lifecycle across combat and campaign. Pair #39 verbal restrictions with that path; #43 supplies reviewed geometry. #40 materials and #171 Deafness are adjacent packages, not excuses to defer finishing Silence.

Prerequisites / coordination: Pending Q19–21; explicit source/preparation from B07/B12; B22 ritual/campaign use. Silence alone does not close every condition or area shape.

### B09: Reaction casting and split targets

Share damage/reaction decision sequencing for Shield #41 and Magic Missile #42/#168, then Scorching Ray #170. Keep spell-specific dice/allocation rules distinct. Reuse applicable reaction machinery for Monk/Bard, not their spell-slot policy.

Prerequisites / coordination: B07 source/slot access; B08 components; targeting/reaction controls reviewed before coding.

### B10: Complete existing spell families and object targets

Batch #166/#204/#225 for legal creature/object targets; #167/#169 for healing; #202/#203/#205/#223 for remaining source/component/cover integration. Implement each shared source adapter across its eligible existing spells in one pass.

Prerequisites / coordination: B05 cover; B07 source policies; B08 components; owning class/species/feat group for each route. No claim of all-source completion from one class.

### B11: Wizard class completion

Reuse B07 spellbook/preparation instead of reimplementing it; package Ritual Adept/Arcane Recovery with rest/campaign casting, Scholar with skill choices, and Evoker with its actual spell grants. #102 owns final lifecycle acceptance.

Prerequisites / coordination: B01/B02/B07–B10/B22; applicable full spell inventory remains required.

### B12: Cleric class completion

One class package for creation, preparation/Divine Order, Channel Divinity/Divine Spark, Turn Undead and Life features. Subpackages share grant/resource/effect infrastructure; #95 is the acceptance owner.

Prerequisites / coordination: B02/B07/B08; relevant condition/area mechanics and full Cleric/domain spell inventory.

### B13: Barbarian class completion

Implement Rage lifecycle and effects #104/#105 together; add Reckless/Danger Sense, Primal Knowledge and Berserker through the same real progression path. #109 owns final acceptance.

Prerequisites / coordination: B02 recovery; B03 weapons/mastery; B05 conditions; B22 ability checks.

### B14: Rogue class completion

One Sneak Attack implementation and attained-level matrix for #112/#220/#221, coordinated with #116. Reuse delivered Dash/Disengage; #219 supplies Hide. Complete Steady Aim, Thief and starting/Expertise acceptance without separate level-only migrations.

Prerequisites / coordination: Pending Q25; B01 training; B03 weapon properties; B05 Hide; B22 climbing/item/skill actions.

### B15: Monk class completion

Martial Arts/Focus/action options share the attack-budget path; movement/recovery share class resource progression. Deflect Attacks and Slow Fall consume reaction/falling capabilities; Open Hand consumes forced-movement/condition rules.

Prerequisites / coordination: B02/B03/B05 and reaction capability from B09 as needed, not blanket completion of Shield or all spellcasting.

### B16: Bard class completion

Class access plus Inspiration/Expertise/Jack of All Trades/Lore share selected grants and check/reaction modifiers. Integrate actual instruments already delivered; #130 owns final acceptance.

Prerequisites / coordination: B01/B02/B07; B09 reaction windows for Cutting Words; B22 checks; full Bard/Lore spell inventory.

### B17: Sorcerer class completion

Extend delivered cantrips to real progression and leveled access. Innate Sorcery/points/conversion share resource rules; Metamagic uses explicit per-option cases; Draconic grants join the same source matrix.

Prerequisites / coordination: B02/B07–B10; Metamagic must exercise actual applicable spells, not a universal toggle.

### B18: Paladin class completion

Spellcasting, Lay On Hands, style/mastery choices, Smite and Devotion share class advancement/resources. Deliver style acquisition with B03 and free/prepared spell sources with B07.

Prerequisites / coordination: B02/B03/B07–B09; complete Paladin/Devotion spell inventory; hit-decision timing for Smite.

### B19: Ranger class completion

Spellcasting/Hunter’s Mark/free casts, mastery/styles, Deft Explorer and Hunter share source/progression and resource queries. Use the same style implementation as Fighter/Paladin.

Prerequisites / coordination: B01/B02/B03/B07/B08; complete Ranger spell inventory; real skill/exploration path from B22.

### B20: Druid class completion

One Wild Shape package owns catalog eligibility, transform/retained state and actions/reversion (#153–155). Companion infrastructure is shared with B22; Primal Order/Land spell grants use B07.

Prerequisites / coordination: B02/B05 footprints/B07/B08/B22; complete Druid/Land spells. Transformations remain a substantial package.

### B21: Warlock class completion

Pact slots/known spells, Magical Cunning, invocations and Fiend grants integrate through one attained-level resource path. Reuse delivered cantrips; route Tome and other spell entitlements through B07.

Prerequisites / coordination: B02/B07/B10; invocation-specific behavior and complete Warlock/Fiend spell inventory.

### B22: Campaign checks, companions and exploration magic

Implement concrete uses needed by the active feature: checks with Tactical Mind/Thief, rituals with Silence/Wizard, companions with Wild Companion/Find Familiar. Do not build an open-ended simulation before named acceptance cases.

Prerequisites / coordination: Consuming class/spell/source group, plus existing campaign APIs; authored adaptations require review.

### B23: Later single-class levels and recovery magic

Retain level bands 5–10, 11–16 and 17–20; deliver each through shared progression plus class/spell integrations and ordinary attained-level evidence. Resurrection costs/time remain here.

Prerequisites / coordination: Level-four milestone #8 and its real prerequisites. Deferred from the first delivery milestone, not deleted.

### B24: Multiclassing

Batch class history/entry grants, then spell-slot/access interactions, then feature overlap and ordinary advancement acceptance. Keep #184 as integration acceptance rather than a separate duplicate implementation.

Prerequisites / coordination: Single-class/resource/progression foundations; preserve full multiclass scope after earlier milestones.

## What is redundant, unnecessary, or should wait

No reviewed evidence justifies dropping required classes, species, spells,
rest behavior, later levels or multiclassing. The avoidable work is primarily
tracking duplication, repeated integration and scope expansion. Recommendations
below preserve the requirements; they are not instructions to close issues now.

| Finding | Recommended treatment | Evidence / safeguard |
| --- | --- | --- |
| #37 and #97 both own Wizard learning/preparation | Use #97 as implementation owner; carry #37 UI/selection/save criteria as its explicit checklist. After reconciling both bodies, #37 could be closed as superseded, never reported as functionality delivered. | #37 calls for learning/book/preparation controls; #97 includes those plus copying. Copying must remain. |
| #112, #220 and #221 divide one Sneak Attack feature partly by level | Execute one coordinated package across supported progression; retain children as acceptance rows and link the same evidence. Consolidate tracking only after preserving the attained-level tests and Q25 decision. | The pure helper already covers progression; live hit decisions and real levels remain missing. #116 owns the ordinary advancement prerequisite. |
| #104/#105 split Rage state from its effects | Implement in one package, with distinct lifecycle/effect tests. Separate deliveries of inactive state have no player value. | Both belong to the same activation, duration, cost and removal paths. Neither scope is dispensable. |
| #153/#154/#155 split Wild Shape catalog, transformation and actions | One feature package with reviewable commits and a shared acceptance matrix. | A form list without transformation/reversion is not playable completion. |
| Twelve class-start issues plus twelve final-integration issues | Retain their criteria as per-class acceptance gates; avoid twelve standalone plumbing implementations and twelve repeated integration projects. | Existing common Training/preset services already handle many starting grants. Individual class entitlements still differ. |
| #48–52, #8, #35, #165, #185/#186 are overlapping tracking layers | Keep them as cross-cutting coverage gates, report them separately from executable work, and give each requirement one implementation owner. | They still protect full scope. #165 has real unimplemented inventory behind it; it cannot simply be closed as “inventory done.” |
| #165 title says inventory, but it is used as the whole spell queue | Clarify its title/role after review; keep the 139-row inventory authoritative and schedule named batches beneath it without one ticket per source route. | Current ledger has 128 missing spells. Renaming is administrative, not completion. |
| #34 already has native/combat delivery but requires exploration Adrenaline Rush | Assign that remaining implementation once, coordinated with Orc #72 and campaign #174. Share the evidence; do not build three exploration action paths. | TEMPORARY-HP.md explicitly says exploration activation is missing. Do not close #34 merely because its two children are closed. |
| #29/#189, #78/#85/#140/#147, #111/#110/#116, #79 and #82 | Reconcile acceptance against existing delivered code first; implement only the uncovered choices/routes/levels. | Training/Archery are materially delivered but retain missing paths. This review does not prove any of these issues fully closable. |
| Boilerplate “spell choices” in noncasting class-start tickets | Remove the implication that those classes need intrinsic spellcasting; retain any spell choices actually granted by species/background/feats. | The class-start wording is templated. It must not generate fictional class mechanics. |
| Mandatory new child for every property, class grant or level-only extension | Replace with a named checklist in the batch unless a separate owner, distinct decision/state machine or genuinely independent deliverable warrants an issue. | Existing #60/species/higher-level templates and the workflow require splits; the adopted workflow now replaces that mechanical splitting policy. |
| Blanket dependence on an entire tracker | Replace execution blockers with named capabilities and evidence. | Monk starting work need not await Shield. This does not waive the final acceptance gate. |
| New free-form simulations for every tabletop possibility | Require an existing SRD/campaign requirement and a concrete player use before building one. For example, enforce cannot-speak casting eligibility, but do not invent a general gagging/restraint game merely to exercise it. Any excluded required interaction needs an explicit reviewed adaptation. | #39/#174 are broad; source/removal behavior must remain explicit. No blanket exemption from Verbal rules is proposed. |
| #175–184 higher levels/resurrection/multiclassing | Keep in the full goal, outside the immediate level-four delivery queue. | These were explicitly requested. Deferral is sequencing, not removal. |

**No confirmed “unnecessary SRD feature” was found.** The strongest consolidation
candidate is #37/#97; most other apparent duplicates are parent/child acceptance
or shared implementation opportunities. I should not inflate completed counts
by marking superseded tracking as delivered functionality.

## Concrete batches instead of another vague grouping

Select a package from a family, name its original requirements, and preflight
its dependencies before starting. Initial candidates:

| Package | Existing issues worked together | Delivery / stop boundary |
| --- | --- | --- |
| A: Finish training review | #29/#189 | Delivered under Q28; see [canonical coverage](SRD-COVERAGE.md) and [Training evidence](TRAINING.md). Existing native preview/commit and shared controls complete the saved-character path. |
| B: Finish rest workflow | #30/#192/#193 | Sequential Hit Die choices, interruption/resumption and camp/inn continuation. Resolve controls and exact supported encounter schedules before coding; this is substantial, not a promised two-hour task. |
| C: Fighting styles and Light attacks | #59/#78–81, style/mastery portions of #60/#85/#140/#147 | Shared eligible-source matrix and attack decisions, ordinary Fighter/Paladin/Ranger acquisition. If ordinary progression is unavailable for a route, record that prerequisite before choosing this as a closure batch. Q23 required. |
| D: Wizard access plus a real free-cast source | #36/#37/#97/#200/#75, consuming #61/#63 | One source-aware selection/preparation/free-cast path with known/book/prepared distinctions. Retain unsupported spell catalog gaps; this package does not magically complete every class or Magic Initiate list. |
| E: Silence end to end | #38/#208/#209, relevant #39/#43 | Concentration, area, V restrictions, actual preparation, combat/campaign casting, ritual/time and save continuation. Requires Q19–21; defer unrelated material-component implementation. |
| F: Complete Fighter class mechanics | #84/#85/#87/#88/#89/#82 with required shared packages | One real 1–4 progression path and acceptance matrix. Report class-mechanics completion separately from all origin combinations and the all-class milestone. Do not call it a complete character ecosystem while those requirements remain. |

**Recommended next sequence:** A is delivered. Continue with the necessary
B/C/B22 capabilities and F, resolving each batch's outstanding controls together.
Do not rotate through unrelated partial features merely to stay busy. The full
all-class goal stays intact. The 168-issue inventory above remains a dated review
snapshot; live status and completed work belong in GitHub and the coverage ledger.

For the unimplemented spells under #165, batch by semantics after the needed
capability is proven: healing (Cure Wounds/Healing Word), targeted attacks and
source routes (existing cantrips), area damage (e.g. Burning Hands/Thunderwave),
concentration modifiers (e.g. Bless/Bane), then separate transformation,
companion, teleportation and exploration packages. Examples are scheduling
candidates, not declarations of identical mechanics or new implementation
approvals. Name the exact spells, all applicable routes and independent expected
results before coding; do not hide 128 spells inside a nominally “one issue” batch.

## Durable execution proposal

The workflow changes below are adopted; the operational rules now live in
[SRD-WORKFLOW.md](SRD-WORKFLOW.md), the navigation map in
[SRD-REPO-MAP.md](SRD-REPO-MAP.md), and approvals in
[SRD-DECISIONS.md](SRD-DECISIONS.md). This table retains the rationale, not a
second operating procedure. The implementation goal remains paused.

| Change after approval | Durable location / enforcement | Acceleration and proof |
| --- | --- | --- |
| Replace one-issue work with one active delivery batch | Update AGENTS.md and SRD-WORKFLOW.md together; reconcile conflicting “split before coding” language in SRD-IMPLEMENTATION.md and relevant issue bodies. | Removes repeated source-by-source starts while retaining one owner and bounded work in progress. |
| Freeze batch requirements before implementation | Put a short batch card in the feature's own doc: issue IDs, requirement IDs, player outcome, supported levels/routes, dependencies, decisions, expected checks and stop boundary. | Prevents repeatedly redefining success around whichever small piece passed. |
| Use a stable decision register | Record each UI/policy question once with exact decision, scope and status; link from the card. Preserve Q11, Q19–21, Q23, Q25 as pending and Q27 as approved. Review new decisions together before the trial. | Avoids repeated permission questions and implementing a foundation that cannot reach the player. No widening old approvals by assumption. |
| Deliver a shared source family once | Track a source × feature × level matrix in the existing coverage/inventory documents; class/feat/species issues link to its evidence. | Avoids separate save changes and render passes for every cantrip/class pairing. Correct casting ability and acquisition rules still get individual cases. |
| Plan persistence before changing it | On the card record whether the batch changes schema, interpretation, or just catalog data. Reuse existing source records when valid; otherwise change the current format in place with no migration ([policy](SAVES.md#pre-10-format-policy)). | Fewer unnecessary format changes; older pre-1.0 saves are rejected rather than migrated. |
| Test in layers | Focused native tests during development; shared regressions once on the final integrated diff. UI tests/rendering on affected flows; repeat only after relevant changes/failures. Record tested commit/tree and exact commands. | Fewer full rebuilds. Preserve all required behavior, localization, ownership and save gates; no stale-binary pass claims. |
| Keep builds bounded | Use existing build directories/tools; avoid unnecessary CMake reconfiguration and concurrent builds in one directory; run Godot checks serially. | Reduces observed binding regeneration/build overhead without introducing another stack or new tooling project. |
| Limit work in progress | One active batch; at most one necessary prerequisite with an explicit return path. Record live tool handles. Escalate a material blocker rather than starting a chain of unrelated partially delivered features. | Reduces unfinished work and context switching. Do not resume work while the goal is paused. |
| Run a measured trial | After authorization, record a two-hour checkpoint and actual elapsed active work. At the checkpoint report delivered requirements, original issue closures, build/test effort and blockers; agree on continuation. If a verification process is live, preserve its handle and report it accurately. | Establishes evidence of improved throughput. Two hours is a review interval, not a promise a family finishes or permission to skip checks. |
| Use meaningful metrics | On the handoff card report original issues closed, fixed acceptance rows delivered, fully verified player paths, new scope and blocked work separately. Mark administrative consolidation separately. | Stops ticket splitting or supersession from appearing as functional progress. Rebaseline only with an explicit explanation. |
| Bound context and documentation | Keep one short handoff, one coverage matrix and feature-specific evidence. Update the canonical record and link it instead of reproducing the same history in several documents/comments. | Less repeated investigation and fewer stale status copies. |
| Carry the plan across tasks | On every start/resume read AGENTS → handoff → active card/decision links. At a user-authorized task boundary transfer this compact state; do not create a new task or change models automatically. | Makes the method recoverable from repository state rather than relying on this conversation. |

Suggested batch card (plain documentation, no new software):

- Status: proposed / ready / active / waiting on named decision / verified / delivered.
- Batch and original issues; measurable player outcome and explicit route/level bounds.
- Fixed acceptance checklist with per-item owner, evidence and remaining dependency.
- Approved/pending decisions; no inferred approvals.
- Files/services, existing implementation reused, and persistence policy.
- Focused checks; final integration checks; last tested revision.
- Start time, checkpoint, elapsed effort and live process handles.
- Original requirements delivered versus added scope; next concrete action.

Before starting another batch, reconcile the entire card. Commit/push and close
issues only against their preserved acceptance. A passed helper, a native-only
path, a superseded ticket or a green regression suite alone is not delivery.

## Complete issue-to-group index

Snapshot of all 168 open labeled issues. Each has one primary group above;
this index is a completeness check, not another independently maintained backlog.

| Issue | Current title | Group |
| --- | --- | --- |
| [#8](https://github.com/stdarg/OpenGold/issues/8) | Complete all twelve SRD classes through level 4 | T |
| [#29](https://github.com/stdarg/OpenGold/issues/29) | SRD F02: Represent skill, tool and language grants | B01 |
| [#30](https://github.com/stdarg/OpenGold/issues/30) | SRD F03: Implement Short Rests, Hit Dice and resource recharge | B02 |
| [#32](https://github.com/stdarg/OpenGold/issues/32) | SRD F04b: Add Help/Medicine stabilization | B02 |
| [#34](https://github.com/stdarg/OpenGold/issues/34) | SRD F05b: Implement Temporary HP and replacement choices | B02 |
| [#35](https://github.com/stdarg/OpenGold/issues/35) | SRD F06: Complete conditions in bounded feature-driven increments | T |
| [#36](https://github.com/stdarg/OpenGold/issues/36) | SRD F07: Separate spell grants, spellbooks and prepared lists | B07 |
| [#37](https://github.com/stdarg/OpenGold/issues/37) | SRD F07b: Integrate Wizard spell learning and preparation controls | B07 |
| [#38](https://github.com/stdarg/OpenGold/issues/38) | SRD F08: Implement concentration lifecycle | B08 |
| [#39](https://github.com/stdarg/OpenGold/issues/39) | SRD F09: Enforce verbal and somatic casting requirements | B08 |
| [#40](https://github.com/stdarg/OpenGold/issues/40) | SRD F09b: Enforce spell materials, foci and consumed costs | B08 |
| [#41](https://github.com/stdarg/OpenGold/issues/41) | SRD F10: Implement spell reactions starting with Shield | B09 |
| [#42](https://github.com/stdarg/OpenGold/issues/42) | SRD F11: Support split-target Magic Missile and Scorching Ray | B09 |
| [#43](https://github.com/stdarg/OpenGold/issues/43) | SRD F12: Implement area targeting with one representative spell | B08 |
| [#44](https://github.com/stdarg/OpenGold/issues/44) | SRD F13: Complete creature-size and occupied-space transit rules | B05 |
| [#45](https://github.com/stdarg/OpenGold/issues/45) | SRD F13b: Implement multi-square creature footprints | B05 |
| [#46](https://github.com/stdarg/OpenGold/issues/46) | SRD F14: Implement light and visual senses | B05 |
| [#47](https://github.com/stdarg/OpenGold/issues/47) | SRD F14b: Implement nonvisual senses through a real trait | B05 |
| [#48](https://github.com/stdarg/OpenGold/issues/48) | SRD Q01: Complete equipment coverage | T |
| [#49](https://github.com/stdarg/OpenGold/issues/49) | SRD Q02: Complete feats coverage | T |
| [#50](https://github.com/stdarg/OpenGold/issues/50) | SRD Q03: Complete backgrounds coverage | T |
| [#51](https://github.com/stdarg/OpenGold/issues/51) | SRD Q04: Complete species coverage | T |
| [#52](https://github.com/stdarg/OpenGold/issues/52) | SRD Q05: Complete starting class choices coverage | T |
| [#56](https://github.com/stdarg/OpenGold/issues/56) | SRD EQ04: Loading weapon limits | B03 |
| [#57](https://github.com/stdarg/OpenGold/issues/57) | SRD EQ05: Ammunition use and recovery | B03 |
| [#58](https://github.com/stdarg/OpenGold/issues/58) | SRD EQ06: Thrown weapon inventory | B03 |
| [#59](https://github.com/stdarg/OpenGold/issues/59) | SRD EQ07: Light weapon extra attacks | B03 |
| [#60](https://github.com/stdarg/OpenGold/issues/60) | SRD EQ08: Weapon mastery properties | B03 |
| [#61](https://github.com/stdarg/OpenGold/issues/61) | SRD BG01: Complete Acolyte background package | B01 |
| [#62](https://github.com/stdarg/OpenGold/issues/62) | SRD BG02: Complete Criminal background package | B01 |
| [#63](https://github.com/stdarg/OpenGold/issues/63) | SRD BG03: Complete Sage background package | B01 |
| [#64](https://github.com/stdarg/OpenGold/issues/64) | SRD BG04: Complete Soldier background package | B01 |
| [#65](https://github.com/stdarg/OpenGold/issues/65) | SRD SP01: Complete Dragonborn species traits and choices | B06 |
| [#66](https://github.com/stdarg/OpenGold/issues/66) | SRD SP02: Complete Dwarf species traits and choices | B06 |
| [#67](https://github.com/stdarg/OpenGold/issues/67) | SRD SP03: Complete Elf species traits and choices | B06 |
| [#68](https://github.com/stdarg/OpenGold/issues/68) | SRD SP04: Complete Gnome species traits and choices | B06 |
| [#69](https://github.com/stdarg/OpenGold/issues/69) | SRD SP05: Complete Goliath species traits and choices | B06 |
| [#70](https://github.com/stdarg/OpenGold/issues/70) | SRD SP06: Complete Halfling species traits and choices | B06 |
| [#71](https://github.com/stdarg/OpenGold/issues/71) | SRD SP07: Complete Human species traits and choices | B01 |
| [#72](https://github.com/stdarg/OpenGold/issues/72) | SRD SP08: Complete Orc species traits and choices | B06 |
| [#73](https://github.com/stdarg/OpenGold/issues/73) | SRD SP09: Complete Tiefling species traits and choices | B06 |
| [#74](https://github.com/stdarg/OpenGold/issues/74) | SRD FT01: Complete Alert feat | B01 |
| [#75](https://github.com/stdarg/OpenGold/issues/75) | SRD FT02: Complete Magic Initiate feat | B07 |
| [#77](https://github.com/stdarg/OpenGold/issues/77) | SRD FT04: Complete Skilled feat | B01 |
| [#78](https://github.com/stdarg/OpenGold/issues/78) | SRD FT05: Complete Archery feat | B03 |
| [#79](https://github.com/stdarg/OpenGold/issues/79) | SRD FT06: Complete Defense feat | B03 |
| [#80](https://github.com/stdarg/OpenGold/issues/80) | SRD FT07: Complete Great Weapon Fighting feat | B03 |
| [#81](https://github.com/stdarg/OpenGold/issues/81) | SRD FT08: Complete Two-Weapon Fighting feat | B03 |
| [#82](https://github.com/stdarg/OpenGold/issues/82) | SRD FT09: Complete Ability Score Improvement feat | B04 |
| [#83](https://github.com/stdarg/OpenGold/issues/83) | SRD FT10: Complete Grappler feat | B03 |
| [#84](https://github.com/stdarg/OpenGold/issues/84) | SRD FTR00: Integrate Fighter starting choices | B04 |
| [#85](https://github.com/stdarg/OpenGold/issues/85) | SRD FTR01: Fighter — Level-one styles and mastery grants | B04 |
| [#87](https://github.com/stdarg/OpenGold/issues/87) | SRD FTR03: Fighter — Tactical Mind | B04 |
| [#88](https://github.com/stdarg/OpenGold/issues/88) | SRD FTR04: Fighter — Champion features | B04 |
| [#89](https://github.com/stdarg/OpenGold/issues/89) | SRD FTR05: Fighter — level-four choices/resource integration | B04 |
| [#90](https://github.com/stdarg/OpenGold/issues/90) | SRD CLR00: Integrate Cleric starting choices | B12 |
| [#91](https://github.com/stdarg/OpenGold/issues/91) | SRD CLR01: Cleric — Preparation/cantrip rules and Divine Order choices | B12 |
| [#92](https://github.com/stdarg/OpenGold/issues/92) | SRD CLR02: Cleric — Channel Divinity/Divine Spark | B12 |
| [#93](https://github.com/stdarg/OpenGold/issues/93) | SRD CLR03: Cleric — Turn Undead | B12 |
| [#94](https://github.com/stdarg/OpenGold/issues/94) | SRD CLR04: Cleric — Life Domain healing features and domain spells | B12 |
| [#95](https://github.com/stdarg/OpenGold/issues/95) | SRD CLR05: Cleric — level-four integration | B12 |
| [#96](https://github.com/stdarg/OpenGold/issues/96) | SRD WIZ00: Integrate Wizard starting choices | B11 |
| [#97](https://github.com/stdarg/OpenGold/issues/97) | SRD WIZ01: Wizard — Spellbook learning/copying and preparation | B07 |
| [#98](https://github.com/stdarg/OpenGold/issues/98) | SRD WIZ02: Wizard — Ritual Adept | B11 |
| [#99](https://github.com/stdarg/OpenGold/issues/99) | SRD WIZ03: Wizard — Arcane Recovery | B11 |
| [#100](https://github.com/stdarg/OpenGold/issues/100) | SRD WIZ04: Wizard — Scholar | B11 |
| [#101](https://github.com/stdarg/OpenGold/issues/101) | SRD WIZ05: Wizard — Evoker features and spell grants | B11 |
| [#102](https://github.com/stdarg/OpenGold/issues/102) | SRD WIZ06: Wizard — level-four integration | B11 |
| [#103](https://github.com/stdarg/OpenGold/issues/103) | SRD BBN00: Integrate Barbarian starting choices | B13 |
| [#104](https://github.com/stdarg/OpenGold/issues/104) | SRD BBN01: Barbarian — Rage activation/duration/recharge | B13 |
| [#105](https://github.com/stdarg/OpenGold/issues/105) | SRD BBN02: Barbarian — Rage effects | B13 |
| [#106](https://github.com/stdarg/OpenGold/issues/106) | SRD BBN03: Barbarian — Reckless Attack/Danger Sense | B13 |
| [#107](https://github.com/stdarg/OpenGold/issues/107) | SRD BBN04: Barbarian — Primal Knowledge | B13 |
| [#108](https://github.com/stdarg/OpenGold/issues/108) | SRD BBN05: Barbarian — Berserker features | B13 |
| [#109](https://github.com/stdarg/OpenGold/issues/109) | SRD BBN06: Barbarian — level-four integration | B13 |
| [#110](https://github.com/stdarg/OpenGold/issues/110) | SRD ROG00: Integrate Rogue starting choices | B14 |
| [#111](https://github.com/stdarg/OpenGold/issues/111) | SRD ROG01: Rogue — Expertise and starting choices | B14 |
| [#112](https://github.com/stdarg/OpenGold/issues/112) | SRD ROG02: Rogue — Sneak Attack and its timing | B14 |
| [#113](https://github.com/stdarg/OpenGold/issues/113) | SRD ROG03: Rogue — Cunning Action | B14 |
| [#114](https://github.com/stdarg/OpenGold/issues/114) | SRD ROG04: Rogue — Steady Aim | B14 |
| [#115](https://github.com/stdarg/OpenGold/issues/115) | SRD ROG05: Rogue — Thief features, separated where exploration/item actions need support | B14 |
| [#116](https://github.com/stdarg/OpenGold/issues/116) | SRD ROG06: Rogue — level-four integration | B14 |
| [#117](https://github.com/stdarg/OpenGold/issues/117) | SRD MNK00: Integrate Monk starting choices | B15 |
| [#118](https://github.com/stdarg/OpenGold/issues/118) | SRD MNK01: Monk — Martial Arts and eligible weapon/unarmed use | B15 |
| [#119](https://github.com/stdarg/OpenGold/issues/119) | SRD MNK02: Monk — Focus and its individual action options | B15 |
| [#120](https://github.com/stdarg/OpenGold/issues/120) | SRD MNK03: Monk — movement/recovery features | B15 |
| [#121](https://github.com/stdarg/OpenGold/issues/121) | SRD MNK04: Monk — Deflect Attacks | B15 |
| [#122](https://github.com/stdarg/OpenGold/issues/122) | SRD MNK05: Monk — Slow Fall | B15 |
| [#123](https://github.com/stdarg/OpenGold/issues/123) | SRD MNK06: Monk — Open Hand techniques | B15 |
| [#124](https://github.com/stdarg/OpenGold/issues/124) | SRD MNK07: Monk — level-four integration | B15 |
| [#125](https://github.com/stdarg/OpenGold/issues/125) | SRD BRD00: Integrate Bard starting choices | B16 |
| [#126](https://github.com/stdarg/OpenGold/issues/126) | SRD BRD01: Bard — Spell access/preparation policy | B16 |
| [#127](https://github.com/stdarg/OpenGold/issues/127) | SRD BRD02: Bard — Bardic Inspiration | B16 |
| [#128](https://github.com/stdarg/OpenGold/issues/128) | SRD BRD03: Bard — Expertise/Jack of All Trades | B16 |
| [#129](https://github.com/stdarg/OpenGold/issues/129) | SRD BRD04: Bard — Lore features and Cutting Words | B16 |
| [#130](https://github.com/stdarg/OpenGold/issues/130) | SRD BRD05: Bard — level-four integration | B16 |
| [#131](https://github.com/stdarg/OpenGold/issues/131) | SRD SOR00: Integrate Sorcerer starting choices | B17 |
| [#132](https://github.com/stdarg/OpenGold/issues/132) | SRD SOR01: Sorcerer — Spell access and Innate Sorcery | B17 |
| [#133](https://github.com/stdarg/OpenGold/issues/133) | SRD SOR02: Sorcerer — Sorcery Points and slot conversion | B17 |
| [#134](https://github.com/stdarg/OpenGold/issues/134) | SRD SOR03: Sorcerer — individual Metamagic options | B17 |
| [#135](https://github.com/stdarg/OpenGold/issues/135) | SRD SOR04: Sorcerer — Draconic features/spells | B17 |
| [#136](https://github.com/stdarg/OpenGold/issues/136) | SRD SOR05: Sorcerer — level-four integration | B17 |
| [#137](https://github.com/stdarg/OpenGold/issues/137) | SRD PAL00: Integrate Paladin starting choices | B18 |
| [#138](https://github.com/stdarg/OpenGold/issues/138) | SRD PAL01: Paladin — Level-one spellcasting | B18 |
| [#139](https://github.com/stdarg/OpenGold/issues/139) | SRD PAL02: Paladin — Lay On Hands | B18 |
| [#140](https://github.com/stdarg/OpenGold/issues/140) | SRD PAL03: Paladin — mastery/style choices | B18 |
| [#141](https://github.com/stdarg/OpenGold/issues/141) | SRD PAL04: Paladin — Smite trigger and casting | B18 |
| [#142](https://github.com/stdarg/OpenGold/issues/142) | SRD PAL05: Paladin — Channel Divinity/Devotion features and spells | B18 |
| [#143](https://github.com/stdarg/OpenGold/issues/143) | SRD PAL06: Paladin — level-four integration | B18 |
| [#144](https://github.com/stdarg/OpenGold/issues/144) | SRD RNG00: Integrate Ranger starting choices | B19 |
| [#145](https://github.com/stdarg/OpenGold/issues/145) | SRD RNG01: Ranger — Level-one spellcasting | B19 |
| [#146](https://github.com/stdarg/OpenGold/issues/146) | SRD RNG02: Ranger — Hunter's Mark/free-cast grants | B19 |
| [#147](https://github.com/stdarg/OpenGold/issues/147) | SRD RNG03: Ranger — mastery/style choices | B19 |
| [#148](https://github.com/stdarg/OpenGold/issues/148) | SRD RNG04: Ranger — Deft Explorer | B19 |
| [#149](https://github.com/stdarg/OpenGold/issues/149) | SRD RNG05: Ranger — Hunter features | B19 |
| [#150](https://github.com/stdarg/OpenGold/issues/150) | SRD RNG06: Ranger — level-four integration | B19 |
| [#151](https://github.com/stdarg/OpenGold/issues/151) | SRD DRU00: Integrate Druid starting choices | B20 |
| [#152](https://github.com/stdarg/OpenGold/issues/152) | SRD DRU01: Druid — Spell access/Druidic/Primal Order choices | B20 |
| [#153](https://github.com/stdarg/OpenGold/issues/153) | SRD DRU02: Druid — Wild Shape form catalog | B20 |
| [#154](https://github.com/stdarg/OpenGold/issues/154) | SRD DRU03: Druid — transformation and retained statistics | B20 |
| [#155](https://github.com/stdarg/OpenGold/issues/155) | SRD DRU04: Druid — form actions/reversion | B20 |
| [#156](https://github.com/stdarg/OpenGold/issues/156) | SRD DRU05: Druid — Wild Companion | B20 |
| [#157](https://github.com/stdarg/OpenGold/issues/157) | SRD DRU06: Druid — Land features/spells | B20 |
| [#158](https://github.com/stdarg/OpenGold/issues/158) | SRD DRU07: Druid — level-four integration | B20 |
| [#159](https://github.com/stdarg/OpenGold/issues/159) | SRD WLK00: Integrate Warlock starting choices | B21 |
| [#160](https://github.com/stdarg/OpenGold/issues/160) | SRD WLK01: Warlock — Pact Magic | B21 |
| [#161](https://github.com/stdarg/OpenGold/issues/161) | SRD WLK02: Warlock — individual invocations, including their prerequisites | B21 |
| [#162](https://github.com/stdarg/OpenGold/issues/162) | SRD WLK03: Warlock — Magical Cunning | B21 |
| [#163](https://github.com/stdarg/OpenGold/issues/163) | SRD WLK04: Warlock — Fiend features/spells | B21 |
| [#164](https://github.com/stdarg/OpenGold/issues/164) | SRD WLK05: Warlock — level-four integration | B21 |
| [#165](https://github.com/stdarg/OpenGold/issues/165) | SRD S01: Inventory every spell needed through character level 4 | T |
| [#166](https://github.com/stdarg/OpenGold/issues/166) | SRD S02: Complete existing Fire Bolt spell path | B10 |
| [#167](https://github.com/stdarg/OpenGold/issues/167) | SRD S03: Complete existing Cure Wounds spell path | B10 |
| [#168](https://github.com/stdarg/OpenGold/issues/168) | SRD S04: Complete existing Magic Missile spell path | B09 |
| [#169](https://github.com/stdarg/OpenGold/issues/169) | SRD S05: Complete existing Healing Word spell path | B10 |
| [#170](https://github.com/stdarg/OpenGold/issues/170) | SRD S06: Complete existing Scorching Ray spell path | B09 |
| [#171](https://github.com/stdarg/OpenGold/issues/171) | SRD S07: Complete existing Blindness/Deafness spell path | B08 |
| [#172](https://github.com/stdarg/OpenGold/issues/172) | SRD X01: Integrate campaign uses of skills | B22 |
| [#173](https://github.com/stdarg/OpenGold/issues/173) | SRD X02: Integrate companions and summons | B22 |
| [#174](https://github.com/stdarg/OpenGold/issues/174) | SRD X03: Integrate exploration and open-ended magic | B22 |
| [#175](https://github.com/stdarg/OpenGold/issues/175) | SRD X04: Integrate resurrection and later recovery magic | B23 |
| [#176](https://github.com/stdarg/OpenGold/issues/176) | SRD H01: Complete all-class levels 5–10 | B23 |
| [#177](https://github.com/stdarg/OpenGold/issues/177) | SRD H02: Complete all-class levels 11–16 | B23 |
| [#178](https://github.com/stdarg/OpenGold/issues/178) | SRD H03: Complete all-class levels 17–20 | B23 |
| [#179](https://github.com/stdarg/OpenGold/issues/179) | SRD M01: Acquired class levels/history and entry prerequisites | B24 |
| [#180](https://github.com/stdarg/OpenGold/issues/180) | SRD M02: Entry proficiencies, mixed Hit Dice and source grants | B24 |
| [#181](https://github.com/stdarg/OpenGold/issues/181) | SRD M03: Combined Spellcasting slots versus per-class access | B24 |
| [#182](https://github.com/stdarg/OpenGold/issues/182) | SRD M04: Pact Magic, free casts and later special spell resources | B24 |
| [#183](https://github.com/stdarg/OpenGold/issues/183) | SRD M05: Nonstacking and competing class features | B24 |
| [#184](https://github.com/stdarg/OpenGold/issues/184) | SRD M06: Normal creation/advancement controls and combination acceptance | B24 |
| [#185](https://github.com/stdarg/OpenGold/issues/185) | SRD CLOSE: Reconcile final SRD coverage and acceptance | T |
| [#186](https://github.com/stdarg/OpenGold/issues/186) | SRD implementation plan: issue index and completion tracker | T |
| [#189](https://github.com/stdarg/OpenGold/issues/189) | SRD F02c: Complete missing training choices in existing characters | B01 |
| [#192](https://github.com/stdarg/OpenGold/issues/192) | SRD F03c: Integrate Short Rest and Hit Dice controls | B02 |
| [#193](https://github.com/stdarg/OpenGold/issues/193) | SRD F03d: Implement rest interruptions and Long Rest resumption | B02 |
| [#198](https://github.com/stdarg/OpenGold/issues/198) | SRD EQ09: Armor donning and shield Utilize actions | B03 |
| [#200](https://github.com/stdarg/OpenGold/issues/200) | SRD F07c: Persist and spend source-specific free spell casts | B07 |
| [#202](https://github.com/stdarg/OpenGold/issues/202) | SRD S08: Implement Poison Spray through level 4 | B10 |
| [#203](https://github.com/stdarg/OpenGold/issues/203) | SRD S09: Implement Sacred Flame through level 4 | B10 |
| [#204](https://github.com/stdarg/OpenGold/issues/204) | SRD S10: Implement Eldritch Blast through level 4 | B10 |
| [#205](https://github.com/stdarg/OpenGold/issues/205) | SRD S11: Implement Ray of Frost through level 4 | B10 |
| [#208](https://github.com/stdarg/OpenGold/issues/208) | SRD F08b/F09c: Integrate Silence areas, concentration and player casting | B08 |
| [#209](https://github.com/stdarg/OpenGold/issues/209) | SRD F08c/X03: Carry Silence and concentration through campaign play | B08 |
| [#219](https://github.com/stdarg/OpenGold/issues/219) | SRD ROG03b: Cunning Action Hide and hidden-state lifecycle | B05 |
| [#220](https://github.com/stdarg/OpenGold/issues/220) | SRD ROG02a: Sneak Attack for playable Rogue levels one and two | B14 |
| [#221](https://github.com/stdarg/OpenGold/issues/221) | SRD ROG02b: Sneak Attack level-three/four progression and integration | B14 |
| [#223](https://github.com/stdarg/OpenGold/issues/223) | SRD S12: Implement Shocking Grasp through level 4 | B10 |
| [#225](https://github.com/stdarg/OpenGold/issues/225) | SRD: Targetable objects for Eldritch Blast and other spells | B10 |
