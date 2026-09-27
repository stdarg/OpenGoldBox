# SRD decision register

Authoritative working register of user decisions; updated 2026-09-24 from the
conversation and feature/handoff records. These entries summarize approved scope,
not verbatim transcripts. Preserve question IDs. If exact missing wording matters,
retrieve it before coding; do not reconstruct an approval from a summary.
A reply resolves only its question and does not resume a paused goal.

## Current execution agreement

The SRD goal resumed on 2026-09-25 for #193. Earlier workflow maintenance did
not itself authorize resumption. The user requires no silent scope expansion: defer new
work, or obtain explicit approval if an out-of-scope dependency blocks delivery.
Use fixed batches, approved controls, targeted reads, edits completed before
builds, focused checks and one appropriate final regression pass. Report actual
player outcomes and elapsed time at a 60-minute checkpoint (90-minute maximum).
See [workflow](SRD-WORKFLOW.md) for the batch card and enforcement steps.
Questions remain numbered, visible in conversation and preceded by sound.

## Model routing adoption

The user explicitly requested a durable model routing policy for this goal.
[SRD-MODEL-ROUTING.md](SRD-MODEL-ROUTING.md) defines assignments, mandatory visible
selection, compact work packets, escalation after two unsuccessful fixes and
measurement of actual delivery cost. Creating the policy does not resume the goal
or authorize spawning agents/new tasks. Preserve the existing scope and approval
rules; do not silently change model settings.

## Rest workflow approval

Q29–31 are approved by the user's “Yes to all. Get to work.” This authorizes the
rest picker, sequential Hit Die controls and resumption dialog. Current run status
is recorded in the handoff; these control approvals remain valid.
Future questions must be written directly in the conversation, not only in a
question widget, and preceded by the audible Glass alert.

| ID | Decision | Approved scope |
| --- | --- | --- |
| Q29 | #30/#192/#193 Rest picker | Camp [C] opens centered Short/Long dropdown, party eligibility/HP/dice/recharge list, Start/Cancel; preserve camping restrictions and paid inn Long Rest flow; keyboard. Approved; user confirmed all three controls. |
| Q30 | #192 sequential spending | Character list, Spend 1 Hit Die, committed roll/healing result, Finish; camp/inn-only Save game through existing dialog; close finishes without undo. Approved; user confirmed all three controls. |
| Q31 | #193 resumption | Same dialog after interruption resolves: retained progress, extra time, Resume/End; recheck permission; retain earned benefits/time/resources; camp/inn save preserves decision, no combat saving. Approved; user confirmed all three controls. |
| Q33 | #193 natural sleep wake-up policy | Damage wakes recipient; adjacent ally may spend an Action to wake; explicit campaign loud-noise event wakes affected sleepers; initiative alone does not wake. Approved by the user's “33. Yes.” on 2026-09-25. |
| Q34 | #193 Wake ally control | Standard button at right end of existing Cunning Action row; visible for natural sleepers, keyboard/action cycle, highlight adjacent sleeping allies, Action to wake, Escape cancels; disabled off-turn or without Action. Approved by the user's “34. Yes.” on 2026-09-25. |
| Q35 | #193 missing Unconscious prerequisites | Approved by the user on 2026-09-25: persistent Prone and held-item dropping/recovery as one prerequisite; new row below Cunning Action/Wake ally with Stand up (half Speed), Ground item dropdown and Pick up (shown object-interaction/action cost), keyboard access and disabled illegal actions. Requires shared combat/save-state changes. Q33–34 remain approved. User reply: “35. Approved.” |

| Q36 | #193 safe cleanup | Superseded by Q37. |
| Q37 | #193 safe recovery | Approved: after victory or safe camping completion, cancellation, or obeying the city watch, able characters stand and collect reachable dropped party equipment. Return items to original owner, or a surviving companion if dead. Waking during combat retains Prone and ground equipment; normal standing/pickup costs remain. |

## Chill Touch recovery approval

| ID | Issue / decision | Approved scope |
| --- | --- | --- |
| Q40 | #165 Chill Touch natural recovery | APPROVED by “40. Yes”: if the already-rolled Stable recovery deadline arrives during healing prevention, defer its 1 HP recovery until prevention expires without another d4-hour roll; damage still cancels Stable/recovery. |

## Medicine and Tactical Mind controls

| ID | Issue / decision | Approved scope |
| --- | --- | --- |
| Q38 | #32 Medicine stabilization control | Stabilize beside Wake ally in a compacted Cunning Action row; highlight living unstable creatures at 0 HP within 5 feet/clear path, including enemies; Help Action and DC 10 Wisdom (Medicine), training applies; success stabilizes without healing/waking, failure still spends Action; keyboard, Escape cancel and disabled illegal use. APPROVED 2026-09-25: user answered Yes / Approve Tactical Mind dialog. |
| Q39 | #87 Tactical Mind decision | centered eligible-failed-check dialog shows roll/modifier/total/DC/Second Wind uses; Use Tactical Mind or Keep failed check; add 1d10, no healing, spend Second Wind only on resulting success; original Action stays spent, other actions wait, keyboard, no combat-save controls. APPROVED 2026-09-25: user answered Yes / Approve Tactical Mind dialog. |

## Champion approvals

| ID | Issue / decision | Approved scope |
| --- | --- | --- |
| Q41 | #88 Champion acquisition | APPROVED 2026-09-25: existing level-three Fighter confirmation names Champion and grants features on Confirm; old level-three/four Fighters gain the same fixed grants through replay, preserving choices/wounds/equipment/resources. Alternative is explicit subclass selection, including old saves. |
| Q42 | #88 critical free movement | APPROVED 2026-09-25: battlefield highlights, arrows/clicks, up to half Speed without opportunity attacks or normal movement cost; temporary Finish free move label replaces End Turn, that button/Escape declines remainder; other actions wait, original budgets stay spent and interrupted enemy movement resumes. Game/demo, no combat saving. |

## Unlimited ranged ammunition — approved exception

**AMMO-UNLIMITED, 2026-09-25:** user answered AMMO-1: “Let's do away with
ammunition for ranged weapons and assume, if they have the weapon, they also
have the ammunition.” AMMO-2 refers to that answer. Ranged ammunition supply is
unlimited: no required inventory stock, selection, expenditure, recovery or
recovery time/dialog. This supersedes #57's original expenditure/recovery scope
and withdraws AMMO-1/AMMO-2. Preserve current ranged behavior and existing saved
inventory records. This is an intentional SRD exception, not an unfinished
ammunition simulation. Thrown weapons still use their actual weapon inventory;
other weapon properties remain separate. See [ammunition](AMMUNITION.md).

**AR-1 — APPROVED, #99, 2026-09-25, visible question 1:** add Arcane Recovery
dropdown and Recover slots in existing Rest dialog above Result, shortening the
scrollable Info area. Selected eligible Wizard sees legal combinations after
completed Short Rest. Use immediately commits recovered slots and spends the
once-per-Long-Rest use; Finish/Escape without use preserves it. Keyboard support,
existing camp/inn saves preserve committed state. Applies to shared main/demo
rest dialog. User approved visible question 1 and instructed continuation on 2026-09-25 at 19:42 UTC. [Frozen packet](ARCANE-RECOVERY.md).

**SCHOLAR-1 — APPROVED, #100, 2026-09-25, visible question 1:** reuse the disabled
ability-points area at Wizard level-two advancement for a labeled Scholar
Expertise dropdown; require one eligible proficient skill before Confirm.
Existing level-two through level-four Wizards retain missing choices pending
in Review Training's existing checkbox groups. Earlier selections remain locked;
Cancel discards edits; keyboard access, wounds/resources and window dimensions
remain. User replied “1. Approved.” [Frozen scope](SCHOLAR.md).

## Wizard spell choices

**WIZCHOICE-1/2/3 — APPROVED, #37/#97, 2026-09-25, visible questions 1–3:**
User replied “1. Approved. 2. Approved. 3. Approved.” Creation/advancement spell
choice groups, Spellbook button/pending knowledge dialog, and completed Long Rest
preparation/one-cantrip replacement are approved exactly as described in the
[packet](WIZARD-SPELL-CHOICES.md#approved-layoutcontrol-behavior).


## Rogue attacks

**ROGUE-1/2/3 — APPROVED, #112/#114/#116, 2026-09-25, visible questions 1–3:**
Sneak Attack hit dialog (replaces Q25 proposal), Steady Aim in the existing
Bonus Action row, and ordinary Rogue3/4 advancement using the existing dialog.
[Exact controls and preserved exclusions](ROGUE-ATTACKS.md#approved-controls).
User answered the new Rogue questions “1. Approved. 2. Approved. 3. Approved.”
This supersedes Q25; earlier Wizard approvals remain independently recorded.

**ROGUE-DEMO-1 — APPROVED 2026-09-26:** User answered “1. yes.” New demo Bonus Action row below battlefield and above
Wake/Stabilize, as specified in [the packet](ROGUE-ATTACKS.md#demo-placement-discovery--rogue-demo-1-approved).
This separate approval authorizes the new placement and shared keyboard/disabled behavior.

**STYLE-1/2 — APPROVED 2026-09-26:** User replied “1. Yes. 2. yes.”
[Fighting Style routes](FIGHTING-STYLE-ROUTES.md#approved-decisions) records the
exact main/demo controls and source routes. STYLE-1 supersedes Q23 and selects
automatic beneficial die replacement; STYLE-2 approves the Training/Review
Training additions and the separate advancement dropdown.

LIGHT-1/2/3 APPROVED2026-09-26: the user approved2/3 with “2. Yes. 3. Yes”
and then1 with “1. Yes.” [Exact scope](LIGHT-ATTACKS.md#approved-controls)
covers the equipment hand-choice dialog, Weapon/Light combat controls, and
TWF selectors/automatic benefit. Do not ask these again.

## Pending — do not implement dependent choices


| ID | Issue / decision | Recorded scope |
| --- | --- | --- |
| Q19 | #208 Silence geometry | Proposed flat grid, square center within 120 feet, circular 20-foot radius; whole occupied square determines full containment, walls block spread, preview distinguishes partial/full containment; no height model. Pending. |
| Q20 | #208 Silence controls | Proposed prepared level-two Silence in Spell dropdown; area preview, arrows/Enter/click, free Escape cancel; new End concentration row with duration, free release for selected owner outside its turn. Pending. |
| Q21 | #208/#209 Cleric preparation | Proposed current level 3–4 limits/confirmation, explicit Silence selection, existing saved preparations unchanged. Pending. |
| Q43 | #58 Thrown controls | Proposed Thrown weapon dropdown/Throw row below Ground item/Pick up; held/carried quantities, legal target highlighting, keyboard/mouse, explicit necessary stowing before confirmation, free cancellation; proper SRD hand/action costs. APPROVED 2026-09-25 by “43. Approved.”. |
| Q44 | #58 landing policy | Proposed target square on hit/miss, no embedding/breakage/scatter; ground item, ordinary pickup and approved Q37 safe recovery. SRD-unspecified policy. APPROVED 2026-09-25 by “44. Approved.”. |

## Approved patterns and policies

| Reference | Scope that may be reused | Limit / evidence |
| --- | --- | --- |
| Initial four decisions | Preserve remaining turn resources; SRD opportunity triggers without facing reactions; allied transit; Versatile grip/damage/shield behavior | [Implementation plan](SRD-IMPLEMENTATION.md); these do not settle unrelated campaign policies. |
| Q1 / Grip | Labeled one/two-hand dropdown with actual dice, keyboard, immediate persistence; two hands unavailable with shield | [Implementation plan](SRD-IMPLEMENTATION.md). |
| Old-save training / Q9–10 | Keep missing choices pending; approved Training step after Class and before Name; Back keeps valid choices, invalid choices pruned; presets pre-generate their training | [Training](TRAINING.md). Q11 superseded by approved Q28. |
| Q5–7 / HP, Adrenaline and saving | HP colors/source tooltips and separate Temporary HP; Adrenaline Rush beside Dash; **player saves only at camp/inn** | [Temporary HP](TEMPORARY-HP.md). Internal combat checkpoint tests permitted; no player combat Save/Load controls. |
| Q8 | Two-stage Savage Attacker hit/damage-choice dialog, keyboard, action/reaction retained as spent | [Savage Attacker](SAVAGE-ATTACKER.md). Does not approve Sneak Attack's new decision order. |
| Q12–16 | Wizard/Cleric Spell Choices; supported selections, counts, Back, pending catalog choices, preset choices; approved casting row | [Cantrip controls](CANTRIP-CONTROLS.md), [Sacred Flame](SACRED-FLAME.md). |
| Q17 | Action Surge beside Adrenaline Rush, remaining uses, disabled when unavailable, keyboard | [Action Surge](ACTION-SURGE.md). |
| Q18 | Shared labeled Spell dropdown and Cast in existing combat row; known cantrips, legal target preview, keyboard and A/Space cycle | [Cantrip controls](CANTRIP-CONTROLS.md). Replaces individual spell buttons; adding fixed data in this pattern needs no repeated layout question. |
| Q22 | Fighter starting Fighting Style dropdown in Training, initially Archery/Defense; required selection, Back, keyboard, presets, old missing choice pending | [Fighter styles](FIGHTER-STYLES.md). Not blanket approval of future independent selectors. |
| Q24 | Rogue level-two Cunning Action row below combat buttons: dropdown and Use Bonus Action; Dash/Disengage initially, keyboard, turn/budget restrictions | [Cunning Action](CUNNING-ACTION.md). Hide requires its actual rule/target behavior. |
| Q26 | Warlock existing Spell Choices pattern and Spell/Cast; presets, retained old selections | [Eldritch Blast](ELDRITCH-BLAST.md), [Warlock Poison Spray](WARLOCK-POISON-SPRAY.md). |
| Q32 | Repeated Long Rest interruptions grant Short Rest benefits only for a fresh uninterrupted segment of at least one hour; earlier credited time cannot qualify again. Each interruption adds one required hour. | Approved for #193; 70-minute/10-minute/60-minute example in user reply. |
| Q28 | #29/#189 Review Training button beside Grip below inventory, visible for missing training; centered dialog reuses checkbox groups and Fighting Style dropdown, fixed grants, counts and keyboard; locks prior choices; Apply requires all supported choices; Cancel/Escape discards; combat blocks edits; preserve wounds/resources/equipment/advancement | Approved; supersedes pending Q11. |
| Q27 | Sorcerer existing Spell Choices and Spell/Cast with four supported cantrips, Charisma, presets, old selections pending | [Sorcerer cantrips](SORCERER-CANTRIPS.md). |
| Workflow adoption | User authorized efficiency implementation after backlog review | Batch workflow adopted; later pause takes precedence. #192 is closed; #30/#193 remain open. No authorization to discard save compatibility, create a fresh task or spawn agents. |

Before a new question, check this register and the relevant feature doc. Ask a
numbered question only for a material undecided layout/control/policy choice,
with concrete options. Play `/usr/bin/afplay /System/Library/Sounds/Glass.aiff`.
Append new decisions here once; other documents should link to this register.

User instruction 2026-09-25: restart visible question numbering at 1 for each new set. Existing Q identifiers remain historical references; new durable entries use topic-specific identifiers to avoid collisions.

STYLE-1/2 APPROVED2026-09-26: user replied “1. Yes. 2. yes.”
Implementation continues in the frozen batch; no repeated approval needed.

MASTERY-1–4 APPROVED2026-09-26: user replied “1. Yes. 2. Yes. 3. Yes. 4. Yes.” [exact proposed controls](WEAPON-MASTERY.md#approved-controls)
cover Training/fourth Fighter choice, completed Long Rest replacement, optional
combat mastery decisions and Nick attack. All four proposals are explicitly approved.

MASTERY-5–7 APPROVED2026-09-26: user replied “1. Approved. 2. Approved.
3. Approved.” MASTERY-5 approves the simultaneous Champion/mastery Resolve next
selector, separate Cleave critical movement entitlements, and enemy-turn ordering.
MASTERY-6 approves Graze's ability-modifier cap after typed defenses, including
Vulnerability. MASTERY-7 approves the rest-window Save game button and return to
the same pending choice. See [combat proposals](WEAPON-MASTERY.md#additional-combat-decisions-approved)
and [rest proposal](WEAPON-MASTERY.md#long-rest-implementation-evidence-wip).
These decisions resolve the recorded approval blockers; do not ask again.

## ALERT-1 — approved2026-09-26

User reply: “1. Approved”. Approves the centered640×360 pre-turn Alert dialog
in main/demo, Ally and conditional Resolve next dropdowns, current totals,
Swap initiative / Keep initiative, Escape decline, keyboard/mouse and action
blocking. Enemy AI keeps rolls. See [frozen scope](ALERT.md). No other scope added.
