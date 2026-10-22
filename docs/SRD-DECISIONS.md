# SRD decision register

Authoritative working register of user decisions; updated 2026-09-24 from the
conversation and feature/handoff records. These entries summarize approved scope,
not verbatim transcripts. Preserve question IDs. If exact missing wording matters,
retrieve it before coding; do not reconstruct an approval from a summary.
A reply resolves only its question and does not resume a paused goal.

## 2026-09-30 simplification

On 2026-09-30 the user reviewed which SRD 5.2.1 mechanics are worth their cost
in this scripted, DM-less computer RPG and approved six decisions. They
supersede the older entries they name below.

- [DM-1](#dm-1-2026-09-30-tools-languages-and-dm-adjudicated-spells): tool
  proficiencies, languages and spells that need a DM are removed.
- [AUTO-1](#auto-1-2026-09-30-automatic-choices-with-logging): Savage Attacker,
  Sneak Attack, Hit Dice spending and Versatile grip are automatic and logged.
- [SIMPLIFY-1](#simplify-1-2026-09-30-tabletop-time-and-body-simulation): no
  natural sleep in combat, no dropped gear, no rest resumption, death saves
  outside combat resolved at once, and Thrown weapons that work like ammunition.
- [SCOPE-1](#scope-1-2026-09-30-level-cap-deferrals-and-rare-situations): level
  cap 15, multiclassing deferred, the pre-1.0 save cutoff, and rare-situation
  features not pursued.
- [DM-2](#dm-2-2026-09-30-dm-adjudicated-spells-at-levels-38): DM-1 extends to
  the spell levels a level-15 cap brings in.
- [SCOPE-2](#scope-2-2026-09-30-exploration-halves-and-marginal-features):
  grappling dropped, exploration-only feature halves cut, marginal issues
  closed; Hide and multi-square creatures stay.

On 2026-10-02 [MON-1](#mon-1-2026-10-02-original-monsters-as-srd-stat-blocks)
made original monsters SRD stat blocks and sized their encounters by an XP budget.

## Current execution agreement

The SRD goal resumed on 2026-09-25 for #193. Earlier workflow maintenance did
not itself authorize resumption. The user requires no silent scope expansion: defer new
work, or obtain explicit approval if an out-of-scope dependency blocks delivery.
Use fixed batches, approved controls, targeted reads, edits completed before
builds, focused checks and one appropriate final regression pass. Report actual
player outcomes and elapsed time at a 60-minute checkpoint (90-minute maximum).
See [workflow](SRD-WORKFLOW.md) for the batch card and enforcement steps.
Questions remain numbered, visible in conversation and preceded by sound.

The [pre-1.0 save cutoff](SAVES.md#pre-10-format-policy) supersedes every
old-save clause below: older saves are rejected, not migrated. Old-save training
choices (Q22 and the old-save entry), Q28's Review Training dialog, Q41's
old-Fighter replay, SCHOLAR-1's Review Training completion and WIZCHOICE's
Spellbook button/pending knowledge dialog no longer apply; the Workflow adoption
row's save-compatibility limit is lifted. Their gameplay scope still stands.

## Model routing adoption

The user explicitly requested a durable model routing policy for this goal.
[SRD-MODEL-ROUTING.md](SRD-MODEL-ROUTING.md) defines assignments, mandatory visible
selection, compact work packets, escalation after two unsuccessful fixes and
measurement of actual delivery cost. Creating the policy does not resume the goal
or authorize spawning agents/new tasks. Preserve the existing scope and approval
rules; do not silently change model settings.

## Rest workflow approval

Q29–31 are approved by the user's “Yes to all. Get to work.” This authorizes the
rest picker, sequential Hit Die controls and resumption dialog. The resumption
dialog and Q31–Q35/Q37 below are superseded by [SIMPLIFY-1](#simplify-1-2026-09-30-tabletop-time-and-body-simulation) as marked. Current run status
is recorded in the handoff; these control approvals remain valid.
Future questions must be written directly in the conversation, not only in a
question widget, and preceded by the audible Glass alert.

| ID | Decision | Approved scope |
| --- | --- | --- |
| Q29 | #30/#192/#193 Rest picker | Camp [C] opens centered Short/Long dropdown, party eligibility/HP/dice/recharge list, Start/Cancel; preserve camping restrictions and paid inn Long Rest flow; keyboard. Approved; user confirmed all three controls. |
| Q30 | #192 sequential spending | **Per-die spending superseded by [AUTO-1](#auto-1-2026-09-30-automatic-choices-with-logging).** Character list, Spend 1 Hit Die, committed roll/healing result, Finish; camp/inn-only Save game through existing dialog; close finishes without undo. Approved; user confirmed all three controls. |
| Q31 | #193 resumption | **Superseded by [SIMPLIFY-1](#simplify-1-2026-09-30-tabletop-time-and-body-simulation).** Same dialog after interruption resolves: retained progress, extra time, Resume/End; recheck permission; retain earned benefits/time/resources; camp/inn save preserves decision, no combat saving. Approved; user confirmed all three controls. |
| Q33 | #193 natural sleep wake-up policy | **Superseded by [SIMPLIFY-1](#simplify-1-2026-09-30-tabletop-time-and-body-simulation).** Damage wakes recipient; adjacent ally may spend an Action to wake; explicit campaign loud-noise event wakes affected sleepers; initiative alone does not wake. Approved by the user's “33. Yes.” on 2026-09-25. |
| Q34 | #193 Wake ally control | **Superseded by [SIMPLIFY-1](#simplify-1-2026-09-30-tabletop-time-and-body-simulation).** Standard button at right end of existing Cunning Action row; visible for natural sleepers, keyboard/action cycle, highlight adjacent sleeping allies, Action to wake, Escape cancels; disabled off-turn or without Action. Approved by the user's “34. Yes.” on 2026-09-25. |
| Q35 | #193 missing Unconscious prerequisites | **Held-item dropping/recovery, Ground item and Pick up superseded by [SIMPLIFY-1](#simplify-1-2026-09-30-tabletop-time-and-body-simulation); Prone and Stand up stand.** Approved by the user on 2026-09-25: persistent Prone and held-item dropping/recovery as one prerequisite; new row below Cunning Action/Wake ally with Stand up (half Speed), Ground item dropdown and Pick up (shown object-interaction/action cost), keyboard access and disabled illegal actions. Requires shared combat/save-state changes. Q33–34 remain approved. User reply: “35. Approved.” |

| Q36 | #193 safe cleanup | Superseded by Q37. |
| Q37 | #193 safe recovery | **Superseded by [SIMPLIFY-1](#simplify-1-2026-09-30-tabletop-time-and-body-simulation).** Approved: after victory or safe camping completion, cancellation, or obeying the city watch, able characters stand and collect reachable dropped party equipment. Return items to original owner, or a surviving companion if dead. Waking during combat retains Prone and ground equipment; normal standing/pickup costs remain. |

## Chill Touch recovery approval

| ID | Issue / decision | Approved scope |
| --- | --- | --- |
| Q40 | #165 Chill Touch natural recovery | **Exploration part moot under [SIMPLIFY-1](#simplify-1-2026-09-30-tabletop-time-and-body-simulation): Chill Touch expires within seconds, long before a Stable deadline; the deferral remains for combat.** APPROVED by “40. Yes”: if the already-rolled Stable recovery deadline arrives during healing prevention, defer its 1 HP recovery until prevention expires without another d4-hour roll; damage still cancels Stable/recovery. |

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
[SIMPLIFY-1](#simplify-1-2026-09-30-tabletop-time-and-body-simulation) later extends
the same treatment to Thrown weapons: throwing no longer spends the weapon.

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
The Spellbook button/pending knowledge dialog was removed by the 2026-09-30 save cutoff.


## Rogue attacks

**ROGUE-1/2/3 — APPROVED, #112/#114/#116, 2026-09-25, visible questions 1–3:**
Sneak Attack hit dialog (replaces Q25 proposal), Steady Aim in the existing
Bonus Action row, and ordinary Rogue3/4 advancement using the existing dialog.
[Exact controls and preserved exclusions](ROGUE-ATTACKS.md#approved-controls).
User answered the new Rogue questions “1. Approved. 2. Approved. 3. Approved.”
This supersedes Q25; earlier Wizard approvals remain independently recorded.
The ROGUE-1 Sneak Attack dialog is itself superseded by
[AUTO-1](#auto-1-2026-09-30-automatic-choices-with-logging); ROGUE-2/3 stand.

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
| Q21 | #208/#209 Cleric preparation | **Superseded by [CLERIC-1](#cleric-1-2026-10-02-cleric-preparation-and-divine-order).** Proposed current level 3–4 limits/confirmation, explicit Silence selection, existing saved preparations unchanged. |
| Q43 | #58 Thrown controls | **Stowing and quantities superseded by [SIMPLIFY-1](#simplify-1-2026-09-30-tabletop-time-and-body-simulation); the dropdown and Throw button stand.** Proposed Thrown weapon dropdown/Throw row below Ground item/Pick up; held/carried quantities, legal target highlighting, keyboard/mouse, explicit necessary stowing before confirmation, free cancellation; proper SRD hand/action costs. APPROVED 2026-09-25 by “43. Approved.”. |
| Q44 | #58 landing policy | **Superseded by [SIMPLIFY-1](#simplify-1-2026-09-30-tabletop-time-and-body-simulation).** Proposed target square on hit/miss, no embedding/breakage/scatter; ground item, ordinary pickup and approved Q37 safe recovery. SRD-unspecified policy. APPROVED 2026-09-25 by “44. Approved.”. |

## Approved patterns and policies

| Reference | Scope that may be reused | Limit / evidence |
| --- | --- | --- |
| Initial four decisions | Preserve remaining turn resources; SRD opportunity triggers without facing reactions; allied transit; Versatile grip/damage/shield behavior | [Implementation plan](SRD-IMPLEMENTATION.md); these do not settle unrelated campaign policies. |
| Q1 / Grip | **Superseded by [AUTO-1](#auto-1-2026-09-30-automatic-choices-with-logging).** Labeled one/two-hand dropdown with actual dice, keyboard, immediate persistence; two hands unavailable with shield | [Implementation plan](SRD-IMPLEMENTATION.md). |
| Old-save training / Q9–10 | Keep missing choices pending; approved Training step after Class and before Name; Back keeps valid choices, invalid choices pruned; presets pre-generate their training | [Training](TRAINING.md). Q11 superseded by approved Q28. |
| Q5–7 / HP, Adrenaline and saving | HP colors/source tooltips and separate Temporary HP; Adrenaline Rush beside Dash; **player saves only at camp/inn** | [Temporary HP](TEMPORARY-HP.md). Internal combat checkpoint tests permitted; no player combat Save/Load controls. |
| Q8 | **Superseded by [AUTO-1](#auto-1-2026-09-30-automatic-choices-with-logging).** Two-stage Savage Attacker hit/damage-choice dialog, keyboard, action/reaction retained as spent | [Savage Attacker](SAVAGE-ATTACKER.md). Does not approve Sneak Attack's new decision order. |
| Q12–16 | Wizard/Cleric Spell Choices; supported selections, counts, Back, pending catalog choices, preset choices; approved casting row | [Cantrip controls](CANTRIP-CONTROLS.md), [Sacred Flame](SACRED-FLAME.md). |
| Q17 | Action Surge beside Adrenaline Rush, remaining uses, disabled when unavailable, keyboard | [Action Surge](ACTION-SURGE.md). |
| Q18 | Shared labeled Spell dropdown and Cast in existing combat row; known cantrips, legal target preview, keyboard and A/Space cycle | [Cantrip controls](CANTRIP-CONTROLS.md). Replaces individual spell buttons; adding fixed data in this pattern needs no repeated layout question. |
| Q22 | Fighter starting Fighting Style dropdown in Training, initially Archery/Defense; required selection, Back, keyboard, presets, old missing choice pending | [Fighter styles](FIGHTER-STYLES.md). Not blanket approval of future independent selectors. |
| Q24 | Rogue level-two Cunning Action row below combat buttons: dropdown and Use Bonus Action; Dash/Disengage initially, keyboard, turn/budget restrictions | [Cunning Action](CUNNING-ACTION.md). Hide requires its actual rule/target behavior. |
| Q26 | Warlock existing Spell Choices pattern and Spell/Cast; presets, retained old selections | [Eldritch Blast](ELDRITCH-BLAST.md), [Warlock Poison Spray](WARLOCK-POISON-SPRAY.md). |
| Q32 | **Superseded by [SIMPLIFY-1](#simplify-1-2026-09-30-tabletop-time-and-body-simulation).** Repeated Long Rest interruptions grant Short Rest benefits only for a fresh uninterrupted segment of at least one hour; earlier credited time cannot qualify again. Each interruption adds one required hour. | Approved for #193; 70-minute/10-minute/60-minute example in user reply. |
| Q28 | #29/#189 Review Training button beside Grip below inventory, visible for missing training; centered dialog reuses checkbox groups and Fighting Style dropdown, fixed grants, counts and keyboard; locks prior choices; Apply requires all supported choices; Cancel/Escape discards; combat blocks edits; preserve wounds/resources/equipment/advancement | Approved; supersedes pending Q11. The dialog was removed by the 2026-09-30 save cutoff. |
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

## DM-1 (2026-09-30): tools, languages and DM-adjudicated spells

Tools, languages and DM-adjudicated spells are removed as intentional SRD
exceptions. The user approved this on 2026-09-30 for this scripted campaign,
which has no DM to adjudicate them. Skills and Expertise stay.

- **Tool proficiencies:** all of them, including Rogue and Criminal Thieves'
  Tools, the Bard's three instruments, the Monk's tool or instrument, the Druid's
  Herbalism Kit, the Soldier's Gaming Set and Sage/Acolyte Calligrapher's
  Supplies. Skilled offers skills only, and ability checks take no tool.
- **Languages:** Common, the two chosen standard languages, the Rogue's Thieves'
  Cant and extra language, and Druidic.
- **Spells:** the 33 narrative-only spells listed in
  [Removed: needs a DM](SPELL-INVENTORY.md#removed-needs-a-dm) (Guidance was
  later restored by [GUIDE-1](#guide-1-2026-10-12-guidance-restored)), plus ritual
  casting (Ritual Adept) and the features that exist only to cast removed spells:
  Mask of Many Faces, Misty Visions, Pact of the Chain, Wild Companion, the Forest
  and Rock Gnome lineage spells, Tiefling Thaumaturgy and High Elf
  Prestidigitation. The High Elf takes a Wizard cantrip instead.

## AUTO-1 (2026-09-30): automatic choices with logging

The user approved on 2026-09-30 that four choices with only one sensible answer
are made automatically, and that the result is shown to the player in the
combat log or the rest dialog. This supersedes Q8, the ROGUE-1 Sneak Attack
dialog, Q30's per-die Hit Die spending and Q1 / Grip.

- **Savage Attacker:** on the once-per-turn eligible weapon hit, weapon damage
  is rolled twice and the higher total is kept. The log shows both totals and
  the kept one. See [Savage Attacker](SAVAGE-ATTACKER.md).
- **Sneak Attack:** applied to the first eligible hit each turn, under the
  unchanged eligibility rules. The log shows the extra dice and damage. Steady
  Aim and Cunning Action are unchanged. See [Sneak Attack](SNEAK-ATTACK.md).
- **Hit Dice:** after a Short Rest, one **Heal with Hit Dice** action per
  character spends dice one at a time until the character is at full HP or out
  of dice; no die is spent at full HP. The rest dialog lists every roll and its
  healing. See [Rest resources](REST-RESOURCES.md).
- **Versatile grip:** there is no grip control and no stored grip. A Versatile
  melee weapon is wielded two-handed when the other hand is empty (no shield or
  second weapon), otherwise one-handed. The damage line of a Versatile melee hit
  names the grip used. A two-handed Versatile grip frees the other hand between
  attacks, so it never blocks a Somatic component; a shield or second weapon
  still does. See [Party](PARTY.md#versatile-grip) and
  [Spell components](SPELL-COMPONENTS.md).

## SIMPLIFY-1 (2026-09-30): tabletop time and body simulation

The user approved on 2026-09-30 five simplifications of tabletop time and body
bookkeeping that a computer game resolves better in the background. What
happens automatically is shown in the combat log or the exploration text.
This supersedes Q31, Q32, Q33, Q34, Q35's held-item part, Q37, the exploration
part of Q40, and Q43's stowing and Q44's landing policy. Q35's Stand up and
Prone, Q38/Q39 and the rest of Q43 stand.

- **Sleep in combat:** there is no sleeping combatant and no Wake ally action.
  A character resting when an encounter interrupts the rest starts the combat
  awake and Prone, and the log says "{name} wakes up prone." Prone and Stand up
  (half Speed) are unchanged. Prone does not outlast combat: everyone stands
  when a fight is won. See [Status effects](STATUS-EFFECTS.md).
- **Downed characters keep their gear:** falling to 0 HP drops nothing, and
  there is no ground item, Ground item dropdown, Pick up control or post-combat
  and camp recovery of dropped gear. Resting never unequips gear. See
  [Unconscious transit](UNCONSCIOUS-TRANSIT.md).
- **Rest interruption:** an interrupted rest simply ends. It grants nothing,
  earns no partial Short Rest, adds no extra hour and leaves nothing to resume;
  the party rests again. The city watch's interruption says "The rest was
  interrupted. Rest again to recover." Camp and inn restrictions and the paid
  inn Long Rest are unchanged. See [Rest resources](REST-RESOURCES.md).
- **Death saves outside combat:** when a fight is won, each character still
  dying is bandaged and becomes Stable, as in the original game, where anyone
  who survived a fight while dying was assumed bandaged afterward (the user,
  2026-10-09, replacing the first rule, which rolled the remaining death saves
  at once). Outside combat a dying character rolls its remaining death saves at
  once the moment campaign time passes. Stable still recovers 1 HP after 1d4 hours. Combat
  death saves are unchanged. See [Recovery clocks](RECOVERY-CLOCKS.md).
- **Thrown weapons work like ammunition:** throwing never removes or drops the
  weapon, which stays held or carried. There is no landing square, ground item,
  quantity spent, pickup or stowing. The Thrown weapon dropdown and Throw
  button remain so a carried Thrown weapon can still be thrown. See
  [Thrown weapons](THROWN-WEAPONS.md) and [Ammunition](AMMUNITION.md).

## SCOPE-1 (2026-09-30): level cap, deferrals and rare situations

The user approved on 2026-09-30 the following scope limits.

- **Level cap 15:** level 15 is both the rules-library target and the campaign's
  playable cap. Levels 16–20, Epic Boons and the level 17–20 capstones are not
  planned. Progression currently stops at level 4; the next milestones are
  levels 5–10 and 11–15. See [the plan](SRD-IMPLEMENTATION.md#higher-levels-and-multiclassing).
- **Multiclassing deferred:** M01–M06 are not part of the current plan.
- **Pre-1.0 save compatibility:** a hard cutoff. Each save kind has one current
  format; older saves are rejected with a clear message and are not migrated.
  See [Saves](SAVES.md#pre-10-format-policy).
- **Rare-situation features not pursued:** Wizard spellbook copying and book
  loss (the copying part of #97; learning and preparation stay), the mounted
  Lance rules, armor don/doff timing (#198), non-costly material components and
  focus hands (#40; costly or consumed material components still count), and
  speech-blocking sources (#39).

## DM-2 (2026-09-30): DM-adjudicated spells at levels 3–8

The user approved on 2026-09-30 extending DM-1 to the spell levels reachable
under the level-15 cap. When the levels 5–15 spell inventory is made, leave out
spells whose effect needs a DM, including Tongues, Sending, Speak with Dead,
Clairvoyance, Scrying, Legend Lore, Commune, Contact Other Plane, Divination,
Find the Path, Major Image, Seeming, Dream, Geas, Modify Memory, Mirage Arcane,
Programmed Illusion, Hallucinatory Terrain and Magnificent Mansion. Features
that exist only to cast such spells (for example the Warlock's Contact Patron)
go with them. #174 is narrowed to kept spells with a campaign use: Light,
Detect Magic, Identify, Knock and Find Traps.

## SCOPE-2 (2026-09-30): exploration halves and marginal features

The user approved on 2026-09-30 the following, after reviewing the open SRD
issues for features with little value in this computer RPG.

- **Dropped:** grappling and the Grappler feat (#83; removed from the
  level-four feat list), campaign skill checks (#172; at most, original script
  ability checks may map to SRD checks), Silence as the representative
  concentration/area spell (#208/#209; concentration #38 and areas #43 are
  delivered through commonly cast spells, Silence later as an ordinary spell),
  object targeting (#225), Monk Slow Fall (#122), nonvisual senses (#47) and the
  remaining size-difference transit cases (#44).
- **Kept, exploration half cut:**
  - Rogue Thief (#115): Fast Hands and Use Magic Device stay; Second-Story Work
    goes.
  - Ranger Deft Explorer (#148): only Tireless stays; Expertise, languages and
    climb/swim speeds go.
  - Monk movement (#120): Unarmored Movement stays; wall and water running goes.
  - Barbarian Primal Knowledge (#107): the extra skill stays; Strength-based
    skill checks while raging go.
  - Rogue and Bard Expertise (#111/#128), Reliable Talent: no further work;
    already implemented Expertise stays. Jack of All Trades stays because it
    adds to Initiative.
  - Wizard Evoker (#101): Potent Cantrip and Sculpt Spells stay; Evocation
    Savant goes.
  - Druid Wild Shape (#153–#155): uses as a resource (Land's Aid, Wild
    Resurgence) plus a small set of combat forms; no full beast catalog, no
    swim or fly forms.
  - Companions and summons (#173): only spells that actually create creatures.
  - Species (#65–#73): combat traits stay; Elf Trance, Goliath Powerful Build
    and Large Form, Dragonborn Draconic Flight, Dwarf Stonecunning and the
    emptied Gnome lineages go.
  - Light and sight (#46): Darkvision only matters where the campaign marks
    dark areas.
- **Kept:** Hide (#219), multi-square creature footprints (#45; the campaign
  has trolls, giants, ettins and a small dragon), resurrection (#175), free
  casts (#200), spell reactions (#41), split targets (#42), area targeting
  (#43) and all class combat features.

## MON-1 (2026-10-02): original monsters as SRD stat blocks

The user decided on 2026-10-01 and 2026-10-02 how original Pool of Radiance
monsters become SRD creatures and how their encounters are sized. It replaces
the earlier authored Slums conversions.

- **Stat blocks:** monsters are SRD stat blocks, unchanged. In the Slums: Kobold
  Warrior, Goblin Warrior and Bugbear Warrior (SRD 5.2.1). The 2024 SRD has no
  orc, so orcs are the SRD 5.1 Orc (the user: orcs are tougher than goblins).
  In Kuto's Well (2026-10-08): Giant Lizard and Gnoll Warrior (SRD 5.2.1) and,
  as the 2024 SRD has none, the SRD 5.1 Lizardfolk, unchanged at the user's
  choice. Lizardfolk Multiattack is two melee attacks. Norris the Gray is the
  SRD 5.2.1 Bandit Captain at its own AC 15, matching his portrait's leather
  rather than his record's chain mail and shield (the user, 2026-10-08).
  The Slums' set encounters (2026-10-08): Hobgoblin Warrior (its leaders the
  same, as their records ready no armor), Ogre and Troll (SRD 5.2.1). The user
  chose the Troll's full Regeneration, then (the same day) to kill trolls as the
  original game does: the fight ends once every enemy is down and downed
  trolls stay dead. On 2026-10-10 the user chose the original's timing too: a
  downed troll gets back up, with all its Hit Points, only once 3d6 rounds have
  passed since it fell, and Acid or Fire on it keeps it down for good. As in
  the original engine, someone standing on its square keeps it down; it gets up
  on its first turn after the delay with its square clear. Standing, it
  regenerates as the SRD says. The Hobgoblin's poisoned arrows as
  written, and, as SRD 5.2.1 has no apprentice mage, a level-3 Wizard profile
  for the magic-user defending Ohlo. The Troll's Charge and Loathsome Limbs
  are not modeled.
- **Script hazards and robbery (Kuto's Well, the user, 2026-10-08):** an
  original `DAMAGE` arrow volley is one SRD attack roll per arrow, at +4 (the
  hidden archers are Norris's kobolds, as the SRD Kobold Warrior; corrected
  from +3 for bandits after checking the original cluebook), against a random conscious
  member, for the original dice as Piercing damage. A member it drops rolls
  death saves at once, as any dying member outside combat does. Surrendering to Norris (`ROB`) takes
  the money the script names but no items.
- **Leaders** are their base stat block wearing the armor their original record
  readies. A leader shoots a bow only when its combat art shows one; otherwise
  its better gear is loot. Of the Slums icons only the orc leader's (CPIC2 5)
  shows a bow.
- **Simplifications:** kobolds have no Sunlight Sensitivity and goblins no
  Nimble Escape. The Bugbear's Grab hits an adjacent character for its damage
  with no grapple (consistent with [SCOPE-2](#scope-2-2026-09-30-exploration-halves-and-marginal-features)).
  Aggressive adds the orc's speed as a bonus action; only monsters have it, so
  "toward a hostile creature" is left to the monster's policy. The Lizardfolk's
  bite and club both deal piercing damage, and the Gnoll Warrior has no Rampage.
  Norris has no Pistol (out of place in Phlan) and no Parry reaction.
- **Encounter size:** when an original encounter is too strong, it is scaled
  back. The `[combat] encounter_challenge` setting in `settings.cfg` (0–200,
  default 33) is a percentage of the SRD 5.2.1 Moderate XP budget (p. 202). An
  encounter over its budget, or with more creatures than living characters,
  shrinks every group by one factor, keeping at least one of each. Encounters
  never grow. The user left the rest to the implementation's discretion; the
  default and the one-creature-per-character limit were then chosen by
  measurement: the SRD budget alone left mobs untrimmed, and the party
  currently has no leveled cleric healing (#91). See the
  [balance audit](audits/slums-encounter-balance.md).
- **Rewards:** experience and loot remain those of the original encounter,
  however many monsters fought. The user will later tie challenge to difficulty
  and to experience and treasure gain, and add a visible slider.

## CLERIC-1 (2026-10-02): Cleric preparation and Divine Order

The user answered four questions for [#91](https://github.com/stdarg/OpenGold/issues/91),
each with the recommended option.

1. **Divine Order** is chosen at creation in the Training step, with the same
   single-selection dropdown as Fighting Style. It is a training grant
   (`order:protector` or `order:thaumaturge`, source `class:cleric:divine_order`)
   and cannot be changed later.
2. **Preparation** reuses the Wizard controls: a counted "Prepared spells" group
   in the Spell Choices step, the level-up spell page (earlier preparations
   locked, the level-four cantrip as a learning group) instead of the old
   four-checkbox block, and the shared Long Rest dialog titled "Prepared
   spells", with no spellbook and no cantrip replacement at rest.
3. **Spells not yet implemented** leave their places pending, as Cleric cantrips
   already did. Nothing is invented; the missing spells come with
   [#165](https://github.com/stdarg/OpenGold/issues/165).
4. **Inflict Wounds**' SRD conformance and combat button are a separate issue,
   [#230](https://github.com/stdarg/OpenGold/issues/230).

Replacing a cantrip on gaining a Cleric level waits for a second implemented
Cleric cantrip; Sacred Flame is the only one, so there is nothing to replace it
with. The Slums [balance audit](audits/slums-encounter-balance.md) was measured
again with prepared Cleric healing; the `encounter_challenge` default is
unchanged.

## SHOP-1 (2026-10-02): shop armor and unusable stock

For [#18](https://github.com/stdarg/OpenGold/issues/18) the user chose the
recommended answers:

1. Original armor converts to the SRD armor of the same name: Padded, Studded
   Leather, Ring Mail, Scale Mail, Splint Mail and Plate Mail. Banded Mail, which
   SRD 5.2.1 lacks, converts to Splint (same AD&D AC 4). Magic or cursed items
   stay unsupported.
2. Stock that still has no conversion stays for sale and is marked "cannot be
   equipped" in the shop list. No new controls.

## DM-3 (2026-10-04): spells a CRPG cannot use

The user set the spell policy for finishing levels 1–4 for every class
([#8](https://github.com/stdarg/OpenGold/issues/8)): every spell relevant to a
computer role-playing game like Pool of Radiance is in the game, and anything
that needs a DM is not. Applying it, the user removed 17 spells and asked that
they not be supported: Suggestion, Phantasmal Force, Enthrall, Feather Fall,
Jump, Spider Climb, Pass without Trace, Levitate, Floating Disk, Light, Dancing
Lights, Continual Flame, Darkvision, Animal Friendship, Calm Emotions, Find
Traps and Alter Self. The Otherworldly Leap invocation, which only casts Jump,
goes with them. They are not learnable, preparable or granted by any route; see
the [spell inventory](SPELL-INVENTORY.md#removed-nothing-to-act-on-in-this-game).

## CLASS-1 (2026-10-04): finishing levels 1–4 for all classes

For [#8](https://github.com/stdarg/OpenGold/issues/8) the user approved:

1. **One control pattern** instead of a question per feature. In combat, each
   class action joins the A cycle and a class-action dropdown with a Use button,
   like Cunning Action; reactions use the existing React/Decline prompt; targets
   are clicked as now. Creation and level-up choices use the existing Training
   dropdowns and checkboxes and the Spell Choices groups. A question is still
   asked when a feature does not fit, such as Wild Shape's form picker.
2. **Class by class**, each finished to level 4 before the next: Paladin,
   Ranger, then the Cleric, Wizard and Rogue subclasses, Barbarian, Monk, Bard,
   Sorcerer, Warlock and Druid. Spells come with the class that first needs them.
3. **Sequential work**, committed and pushed per class when its tests pass.

## CLASS-2 (2026-10-04): choosing several targets

For spells that affect several creatures, such as Bless, the user chose
click-to-select: after choosing the spell, each click on a legal creature adds
or removes it, and the spell is cast when the maximum is reached or earlier with
the existing Use button or Space; Escape cancels without spending anything.

Concentration is tracked in combat only and ends when the combat ends, an
implementation adaptation recorded with the first Concentration spells (Shield
of Faith, Heroism).

## CLASS-3 (2026-10-05): holy water and casting outside combat

Protection from Evil and Good needs no holy water: like arrows, the consumed
component is not tracked, so the spell costs only its slot.

Features and spells that matter outside combat (Lay On Hands, healing spells,
later Detect Magic and Identify) will be used from the Camp/Rest dialog, in a
"Cast / Use" section: choose a party member, then the spell or feature, then the
member it affects.

## CLASS-4 (2026-10-05): Divine Sense

The Paladin's Divine Sense is left out, as DM-3 left out spells: in combat every
creature is already visible, so it has no effect in this game. Channel Divinity's
uses go to Sacred Weapon.

## CLASS-5 (2026-10-05): aiming area spells

Area spells (Entangle, Fog Cloud, later Burning Hands, Thunderwave and others)
are aimed on the battlefield after choosing the spell: a left click on a cell,
or the arrow keys, moves a preview of the affected squares; a right click, Space
or Enter casts at the previewed spot; Escape cancels without spending anything.

## CLASS-6 (2026-10-05): Goodberry

Goodberry is a camp heal: cast from the Camp dialog's Cast / Use row, the chosen
member eats the ten berries at once and regains up to 10 Hit Points. No berries
are carried, so none reach combat.

## CLASS-7 (2026-10-05): choosing Resistance's damage type

The Resistance cantrip is offered once per damage type in the A cycle
("Resistance: Fire", "Resistance: Slashing", ...), like Command's options; the
player then clicks the creature.

## CLASS-8 (2026-10-05): costly components that are not consumed

A spell's costly component that the spell does not consume, such as Warding
Bond's two 50 gp rings or Identify's 100 gp pearl, is not required: like
CLASS-3's holy water, the spell costs only its slot.

## CLASS-9 (2026-10-05): Sanctuary

A creature that targets a creature warded by Sanctuary with an attack or a
harmful spell and fails its Wisdom save loses that attack or spell; it does not
choose another target.

## CLASS-10 (2026-10-06): Wild Shape forms and Heat Metal

- **Wild Shape forms** are the Beasts the game has combat art for that the SRD
  allows a Druid of levels 2-4 (CR 1/4, or 1/2 from level 4, and no Fly
  Speed): Wolf, Boar (the game's Wild Boar), Giant Lizard and Giant Snake (the
  SRD Constrictor Snake). Giant Snake is kept although it has a Swim Speed,
  which goes unused; this replaces the earlier "no swim forms" for it. Every
  Druid knows all of them, and the form is picked from the Bonus Action
  dropdown.
- **Heat Metal** targets only a creature wearing metal armor. Monsters' armor
  comes from `equipment` rows in combat.rules, derived from the armor their
  original records ready ([#231](https://github.com/stdarg/OpenGold/issues/231);
  [creature equipment](CREATURE-EQUIPMENT.md)). The user chose rows in the
  rules file over a separate JSON file on 2026-10-07.

## CLASS-11 (2026-10-06): no spell components

The user removed spell components for all casters. Spells have no Verbal,
Somatic or Material components and need no spellcasting focus: a weapon, wand or
shield in hand never blocks casting, and Silence no longer stops spells (it
still protects from Thunder damage). Subtle Spell, which only removed
components, is no longer offered. Untrained armor, Rage and Wild Shape still
prevent casting. See [spell components](SPELL-COMPONENTS.md).

## GEAR-1 (2026-10-08): Fire and Acid for every party

SRD trolls stay down only after Acid or Fire, and the user chose to give every
party those rather than design the trolls fight around standing on a downed
troll (a rule kept from the original game that players cannot easily know).

- **Torch:** a Simple Melee weapon that can be equipped; a hit deals 1 Fire
  damage (SRD 5.2.1 adventuring gear). Every starting kit carries one, as the
  SRD's starting packs do.
- **Acid, Alchemist's Fire and Oil:** thrown in place of one attack at a
  creature within 20 feet, which makes a Dexterity save (DC 8 + Dexterity
  modifier + Proficiency Bonus). Acid deals 2d6 Acid, Alchemist's Fire 1d4 Fire
  and Burning, and Oil covers the target so its next Fire damage deals 5 more.
  Each throw uses one up. Oil is the original's Flask of Oil; Acid and
  Alchemist's Fire are added to New Phlan's general store at SRD prices, and
  Torches are sold ten for 1 gp.
- **Simulated parties** set a standing troll Burning with Alchemist's Fire
  (its Fire each turn stops Regeneration), oiling it first when anyone on the
  side can follow with Alchemist's Fire (before a Fire spell or a Torch, an
  action spent on Oil is worth less than a second attack). At a standing troll
  whose Regeneration nothing has stopped this round, a character casts a Fire
  spell (Fire Bolt rather than Magic Missile) or, beside it, strikes with its
  Torch. They finish a downed troll with a Fire or Acid spell, a Torch or a
  flask, in that order. A character that sees no enemy steps to the nearest
  square with a clear line to one; one with a ranged attack cantrip does so
  before walking up to the enemy. A Wild Shape form uses no gear. A character who cannot get closer to an
  enemy draws a carried bow or crossbow and shoots, taking off its shield
  first (an action) when the bow needs both hands and no enemy is beside it.

## MELEE-1 (2026-10-08): adjacent-only melee

The user chose to keep melee as the original game had it: a melee attack
reaches adjacent squares only. SRD 5.2.1's 10-foot reach (the Glaive, Halberd,
Lance, Pike and Whip, and monsters such as the Troll) is not used; the weapon
catalog keeps the SRD's Reach property as data. This restores the original's
tactic, which the cluebook recommends against trolls, of a front rank in melee
with archers one square behind it. The Troll's `reach` row is removed.

## LEADER-1 (2026-10-08): a party leader speaks for the party

The original asks which character speaks each time the party parleys. The user
chose a standing leader instead: the first member until another is made leader
with **Make leader** on a character's sheet (exploration) or under the party
roster. The leader's name shows with a gold star. The leader parleys: twice its
Charisma goes into the scripts' reaction rolls (variable `0x6DCF`). The
original's formula is unknown; plain Charisma left the scripts' better outcomes
out of reach, New Phlan's own script doubles the value, and the user chose
twice Charisma (Charisma 12 avoids a Slums kobold fight 30% of the time
parleying Abusive, Charisma 18 70%). The leader is the
first buyer in a shop unless the script has just asked which member acts. A
leader who is down hands the parley to the next conscious member. As in the
original, Parley replaces Advance once the monsters are adjacent, and a meeting
that ends without a fight says so ("The monsters go on their way.", "You get
away.").

## FLEE-1 (2026-10-08): fleeing a fight, as the original game did

Fleeing follows the original manual. A party member flees by moving off the
edge of the battlefield (an arrow key from an open edge square; there is no
separate command). Faster than every conscious enemy, it gets away; as fast as
the fastest enemy, it has an even chance, and on a failure it must stay until
the fight ends; slower than any enemy, it cannot leave. Leaving an enemy's
reach provokes an opportunity attack as any move does. One who got away is out
of the fight and rejoins the party after it. When no party member is left on
the field and someone got away, the party has fled: everyone left on the field
is lost for good (dead and out of the party), the treasure stays, and the
original script receives its "fled" result (128; a party wiped out is 129).
Experience comes from the monsters the party killed (the user's ruling): a
flight earns the experience of those it killed, and members who fled share
the experience of a fight the rest went on to win.

The user added a **Flee** button beside End turn (not in the original): it
hands every party member to the game AI until the fight ends. A member who can
flee (no conscious enemy is faster and it has not failed a flight) runs for
the nearest open edge, around walls and Dashing when it must, whatever
opportunity attacks it provokes, and steps off; the rest fight on as the AI
would. Keys and clicks no longer act for the party; the prompt says "Your party
is fleeing." Once no member on the field can still flee (each has failed a
flight or is outpaced), the player takes the party back and the prompt says
"No one else can get away. Your party fights on." (the user, 2026-10-09).

## MORALE-1 (2026-10-09): monster morale, as the original game had it

Monsters check morale at the start of each of their turns, following the Gold
Box engine (read from the original data, the Curse of the Azure Bonds engine
reimplementation as reference, and the original's own messages; no code
copied). The encounter's morale is what the original script stores in `0x6DC6`
before COMBAT (0 to 101 across the game, mostly 50 to 100; 100 never breaks). A creature's own
morale is its record byte `0x84` (`(byte & 0x7F) * 2` from `0x80` up; above
102 it has none): most Pool of Radiance monsters have none, and nine never
break. A creature with its own morale holds while it has lost no more of its
Hit Points, in percent, than that morale; otherwise it holds while its side
keeps at least 100 less the encounter's morale percent of its Hit Points
(rounded down to 5). Broken, it flees in panic when no living opponent is
faster: it runs for an edge away from the party and off the field by the same
rule as the party (FLEE-1), and may rally on a later turn; cornered (it failed
to get away), it fights on.
When an opponent is faster, a creature with Intelligence above 5 surrenders,
leaving the fight counted as defeated; a witless one fights on. A monster that
gets away is worth neither experience nor treasure; the fight is won when none
is left on the field. Hovering a panicked monster lists "Fleeing in panic"
among its conditions (the user, 2026-10-09). Not adopted: Curse's
Bless/Curse/charm morale modifiers
(unconfirmed for Pool) and morale for allied NPCs (ours are player-controlled).


## QUICK-1 (2026-10-09): Quick combat, as the original game had it

The original let the computer play any party member in combat (Quick), and
kept playing it in later fights until the player took over; a key set the
whole party to Quick, Space while the computer was moving a member returned
every member to the player, and a Magic toggle decided whether Quick members
cast spells (read from the original manual and cluebook; no text copied).
OpenGold follows it, with the layout the user approved on 2026-10-09:

- On a member's turn, **Quick** (beside Flee) hands it to the computer at once.
  Q puts the whole party on Quick.
- While the computer plays a member, its action controls are hidden, **Take
  control** stands where End turn does and the prompt says so; Take control
  or Space returns every member to the player. The highlight follows the
  member acting. Each Quick member's card carries a gold QUICK tag.
- **Quick magic** (Off unless switched on with M, as the original's Magic
  appears to start) decides whether the computer casts spells, cantrips
  included, for Quick members; the button shows while the computer plays,
  where Flee stands, and M works on any turn.
- The campaign party keeps who is on Quick and Quick magic between fights and
  in saves (campaign format 25). The computer plays a Quick member as the
  automated demo policy does, described in [AI.md](AI.md).

## PLACE-1 (2026-10-10): starting positions, as the original game had them

The user chose (2026-10-10) to start every scripted fight with the monsters
placed as the original Gold Box engine placed them, instead of the authored
stand-in that always stood them one exploration square ahead of the party. The
rules were read from the Curse of the Azure Bonds engine reimplementation (coab)
as reference; Pool of Radiance's START.EXE carries the same placement tables. No
code was copied.

- The encounter distance is the script's. SETUP MONSTER and ENCOUNTER MENU set
  it to their maximum, but no further than the party can see ahead (at most two
  squares, stopping at a wall or door); APPROACH and Advance bring it one square
  closer, and COMBAT caps it by the view ahead once more. The approaching
  monster's picture and the menu follow the same distance.
- Each side is one group facing the other, standing in the back half of its
  exploration square: the party in its own square, the monsters in the square
  that many squares ahead. At distance 0 they share the party's square and the
  two front ranks touch.
- A group forms ranks from its front centre backwards, each rank filling
  outwards from the centre, alternately to the group's right and left. The first
  rank holds half the group (rounded up); later ranks run to the square's edge.
  A party facing east or west leaves the rank behind its first empty when it has
  a square to spill into. Obstacles and taken squares are skipped.
- A group too big for its square spills into the square behind it, then into
  the squares to its sides, each filled from the edge it shares with the
  group's square. A solid wall (a doorway is not one) closes a square off. The
  groups never surround each other.
- Where the original found no room at all for a monster, it left the monster
  out, and the user chose (2026-10-10) to do the same. A monster left out never
  fights: it gives no experience or treasure, and the band's morale counts only
  the monsters that fight. It is not held back for later; whether the band
  meets the party again is the script's, as for any fight. The original could
  leave out a whole band and start an empty fight that the party won at once
  for nothing; OpenGold keeps the band's first monster instead, on the nearest
  free square the party can reach. A party member with no room takes that
  nearest free square too.

## START-1 (2026-10-11): starting gold, as the original game gave it

The user chose (2026-10-11) that player characters start with gold and buy
their starting equipment, as in Pool of Radiance, instead of receiving the SRD
class and background equipment packages or their gold alternatives.

- A newly created character gets 3d6 × 10 gp (30 to 180 gp), the range the
  original manual gives for every class. It is rolled together with the ability
  scores, so rerolling all six rolls the gold again. It replaces the flat 250 gp
  the party preview used to grant.
- The original's START.EXE also holds AD&D's per-class starting-money dice
  (Cleric and Druid 3d6, Fighter, Paladin, Ranger and Monk 5d4, Thief 2d6,
  Magic-User 2d4, each × 10 gp); the coab reimplementation of the next Gold Box
  game keeps the same table unused. Its use in Pool of Radiance was not
  confirmed, and four SRD classes have no original counterpart, so the user
  chose the manual's single 3d6 × 10 for everyone.
- Gear comes from the shops at the original prices; the SRD's starting
  packages and their 50 gp background alternative are not granted.
- The Character Pool's ready-made characters follow the same rule, since the
  original game had only the characters a player created (2026-10-12): each
  brings 3d6 x 10 gp, rolled from the party's saved random state when it joins,
  and no equipment. The combat demo and the campaign simulator still give their
  parties a class kit so they can fight at once.

## GUIDE-1 (2026-10-12): Guidance restored

Clerics owe more cantrips than the three DM-1 and DM-3 left them (Sacred Flame,
Spare the Dying and Resistance): four at level 4, and a Thaumaturge one more.
The user restored Guidance, the cut cantrip with a use in this game.

- SRD 5.2.1 (p. 138): an Action, touch, Concentration up to 1 minute; the
  creature adds 1d4 to ability checks with the chosen skill.
- In a fight it is offered, like Resistance (CLASS-7), once for each skill the
  game checks there: "Guidance: Athletics" (escaping Ensnaring Strike, Entangle
  and Web) and "Guidance: Medicine" (stabilizing a dying creature). The 1d4 is
  rolled with the check and logged.
- It is a Cleric and Druid cantrip, so Blessed and Druidic Warriors may learn
  it too. A level-four Thaumaturge still has one cantrip choice left pending.
- Bashing or picking a locked door, the other checks the game makes, happen
  outside combat at the door menu. The user chose (2026-10-12) to keep
  Guidance to fights; the door menu does not offer it.

## SACRED-1 (2026-10-12): Sacred Weapon's damage is the player's choice

SRD 5.2.1 lets the Paladin deal the weapon's own damage or Radiant on each hit.
The game used to choose Radiant only when the target resisted the weapon's type
more, reading resistances the player cannot see. The user chose instead that
the player picks when imbuing the weapon: the A cycle offers "Sacred Weapon"
(the weapon's own damage) and "Sacred Weapon (Radiant)", and the choice holds
for its 10 minutes. The party's computer-played members take the weapon's own
damage.


## DEAF-1 (2026-10-13): Blindness/Deafness without Deafness

Deafened makes a creature fail ability checks that need hearing (SRD 5.2.1
p. 178). Nothing in this game asks for hearing, so the Deafness option would
do nothing. The user agreed to drop it, as
[DM-3](#dm-3-2026-10-04-spells-a-crpg-cannot-use) dropped spells with nothing
to act on: Blindness/Deafness always blinds. The SRD gives the spell no early
dismissal, and its extra targets from higher slots arrive with level-three
slots at character level 5 ([#176](https://github.com/stdarg/OpenGold/issues/176)).

## SPLIT-1 (2026-10-13): aiming darts and rays

The user approved, from screenshots of the real combat screen, aiming Magic
Missile's darts and Scorching Ray's rays one click at a time
([#42](https://github.com/stdarg/OpenGold/issues/42)). It extends
[CLASS-2](#class-2-2026-10-04-choosing-several-targets): each click on a foe
aims the next dart or ray, the same foe again if wanted; the spell is cast once
all are aimed (Cast spell stays greyed out until then); Escape cancels for
nothing. Only foes the spell could hit alone are offered. Each foe takes its
darts as one simultaneous hit, with one Shield question; rays are separate
attacks in the order aimed. A ray aimed at a foe already down turns to the
next aimed foe still standing, else to the nearest foe it could have been aimed
at, and is lost only when there is none (the user's choice on 2026-10-14; the
SRD leaves it open; rules 0.6.173). A ray may also be aimed at a downed but
living creature, such as a party member at 0 Hit Points: each hit is a failed
death save (the user's choice on 2026-10-14, as the SRD allows; rules 0.6.174).
Darts and other attacks still pass over the downed, and the computer's casters
aim at standing foes first. The
battlefield does not mark aimed foes (the prompt lists them), and there is no
shortcut that sends the remaining darts at one foe.

## WILD-1 (2026-10-13): Wild Shape lasts until the fight ends

A departure from SRD 5.2.1, approved by the user for simplicity. The SRD's form
lasts hours equal to half the Druid level, but fights last minutes, and the
game offers Wild Shape only in fights. A form now ends when the fight ends
(victory, defeat or flight) and when its Druid flees the battlefield, as
Concentration does ([CLASS-2](#class-2-2026-10-04-choosing-several-targets));
it is never carried into the next fight. It still ends earlier on Leave Wild
Shape, another Wild Shape, 0 Hit Points, or a turn begun Incapacitated.

## BARD-1 (2026-10-13): Bardic Inspiration and Cutting Words without asking

Approved by the user, replacing the React/Decline question that both used to
ask. Asking after every failed roll interrupted play, and could not reach the
saves rolled outside a command, such as the repeated save at the end of a turn.
Both now apply on their own, and the combat log shows the die and its result:

- An inspired creature adds its die to the first failed attack roll, save or
  check that the die could turn into a success. A roll short by more than the
  die's sides keeps the die.
- A Lore Bard's Cutting Words subtracts the die from an enemy's attack roll
  that hit by less than the die's sides, so that it could miss. Only the first
  Bard able to react does so.

As with [AUTO-1](#auto-1-2026-09-30-automatic-choices-with-logging), neither
uses information the player lacks: the roll, its bonus and the target number
are already in the log.

## TOME-1 (2026-10-13): Pact of the Tome without rituals

Approved by the user. The SRD's Pact of the Tome grants three cantrips and two
level-1 spells with the Ritual tag. Every ritual spell was removed with DM-3 as
needing a Dungeon Master, so the two level-1 spells are any the game has, from
any class's list, none of which needs one. They are cast as prepared Warlock
spells with Pact Magic slots, as the cantrips are cast with Charisma; nothing
is cast as a ritual. The Book of Shadows as an object is not modeled.

## COVER-1 (2026-10-14): no half or three-quarters cover

The user decided that the game has no partial cover, as Pool of Radiance had
none. The battlefield knows open, difficult and blocking squares only: a wall
or obstacle between two creatures is total cover (no line of sight, so no
attack or targeted spell), and nothing grants half (+2) or three-quarters (+5)
cover to AC or Dexterity saves. Rules that refer to partial cover, such as
Sacred Flame ignoring it, have nothing to act on.

## HIDE-1 (2026-10-14): finding hidden creatures

SRD 5.2.1 leaves finding a hidden creature to the DM, apart from the Search
action. The user decided:

- **Passive Perception finds.** Whenever a hidden creature is in an enemy's
  clear view (line of sight, neither square Heavily Obscured, the enemy
  conscious and not Blinded), an enemy whose Passive Perception (10 + its
  Perception bonus) reaches the Stealth total finds it at once, without a
  roll. The Search action (a Wisdom (Perception) check against the Stealth
  total) finds each hidden enemy in the searcher's clear view.
- **Monsters never guess.** The AI does not target or approach a hidden
  creature; with no other enemy left it takes the Search action.
- **Any spell ends hiding.** Spells have no components (CLASS-11), so
  "casting a spell with a Verbal component" becomes casting any spell. An attack
  roll ends hiding as the SRD says, and so does a Breath Weapon (a sound louder
  than a whisper).

See [Hide](HIDE.md).

## ADAPT-1 (2026-10-14): Sorcerer and Ranger adaptations approved

The user approved five adaptations already in the game:

- **Empowered Spell:** the rerolled damage dice are chosen for the player: the
  lowest dice below their average, up to the Charisma modifier (at least one).
- **Careful Spell:** the protected allies are chosen for the player.
- **Hunter's Mark:** the free cast (Favored Enemy) is not offered while a
  living creature still carries the Ranger's mark; Move Hunter's Mark moves it.
- **Horde Breaker:** only the turn's first weapon target is excluded, not
  every creature attacked that turn.
- **Ensnaring Strike:** only the Restrained creature may try the Strength
  (Athletics) check to escape, not a creature beside it.

See [Sorcerer](SORCERER.md) and [Ranger](RANGER.md).

## DROW-1 (2026-10-14): no Drow lineage for now

The user decided not to offer the Drow elven lineage at this time. An Elf
chooses High Elf or Wood Elf. (The Drow's Dancing Lights was removed by DM-3,
its Darkvision matters only where the campaign marks dark areas, and only its
level-3 Faerie Fire would have had an effect.)

## HEROIC-1 (2026-10-14): Heroic Inspiration without asking

The user approved that a Human's Heroic Inspiration (Resourceful) is used
without asking, as Bardic Inspiration is (BARD-1): the first failed attack
roll, saving throw or ability check (escaping Restraints, hiding) that a 20
could turn into a success has its d20 rolled again, and the new roll stands;
the log says so. A Human gains it on finishing a Long Rest, so a new character
has none until its first. The new roll replaces the d20 that counted, also
under Advantage or Disadvantage. Repeated saves at the end of a turn and death
saves (made unconscious) do not use it.

Versatile (the same day): a Human picks an Origin feat other than its
background's on the race page; Skilled's three skills are checked there too,
and Magic Initiate's spells are chosen on the spell page. Skilled is offered
first, so it is the creator's suggestion.

## SPARK-1 (2026-10-14): Divine Spark always Radiant

The user decided that Divine Spark always deals Radiant damage to an enemy;
the SRD's choice of Necrotic is not offered. The game used to pick whichever
type the target took more of after its resistances, which the player cannot
see. This may change later (a choice in the A cycle, as True Strike offers).

## SKILL-1 (2026-10-14): no repeated skill picks

The user chose that a later skill list leaves out the skills the character
already has, as the level-4 Skilled feat already did, so no pick is wasted on
a repeat. Creation lists them in this order, each leaving out what comes
before: the background's two skills, Keen Senses, Skillful, Versatile's
Skilled, then the class's list. Rogue Expertise may take any of them.

- If the class's list has fewer skills left than it chooses (a Human Acolyte
  Cleric whose Skillful and Skilled took History, Medicine and Persuasion),
  it offers every skill not held instead, so creation can always finish.
- [Class skills](CLASS-SKILLS.md), [Species](SPECIES.md#choices-at-creation).

## INITIATE-1 (2026-10-14): Magic Initiate learns the cantrips left

The game has four Cleric cantrips (Guidance, Resistance, Sacred Flame and
Spare the Dying; DM-1 and DM-3 removed Light, Mending and Thaumaturgy). A Cleric
learns three at level 1, so an Acolyte Cleric's Magic Initiate (Cleric) has one
left of the two it should learn. The user agreed that it learns as many as
remain. Likewise, when the feat's picks are made first, a Thaumaturge's class
cantrips are the ones left. Either way the character knows every Cleric cantrip
in the game. The level-up lab, which filled every group to its full count,
could not make an Acolyte Cleric before this.

## CUT-1 (2026-10-14): Cutting Words against lethal damage

Cutting Words may also lessen a damage roll (SRD 5.2.1). As with attack rolls
and checks ([BARD-1](#bard-1-2026-10-13-bardic-inspiration-and-cutting-words-without-asking)),
the user chose that it is used without asking, and only when the roll would
drop a member of the Bard's side to 0 Hit Points and the d6 could keep it
standing. The damage roll is shown before it is dealt, as at a table, so the
choice reads nothing hidden. See [Bard](BARD.md#bardic-inspiration-and-cutting-words).

## AID-1 (2026-10-14): Aid in camp chooses its targets

Pool of Radiance asks for one target when a spell needs one, and never for
several: its group spells pick their own, as Bless affects every member not
in melee (Manual pp. 20, 23). The user chose (2026-10-14) to follow that, so
Aid cast from the Camp dialog picks its three targets as Prayer of Healing
picks its five: the members with the lowest Hit Point maximum who are not
already aided, the caster included. In a fight the player
still clicks Aid's targets ([CLASS-2](#class-2-2026-10-04-choosing-several-targets)).
See [Cleric](CLERIC.md#aid).

## LEVELS-1 (2026-10-14): levels 5–10

The user approved the [levels 5–10 plan](LEVELS-5-10.md): shared progression,
then Extra Attack, then the classes one at a time (Fighter, Barbarian, Rogue,
Monk, Paladin, Ranger, Cleric, Wizard, Sorcerer, Warlock, Bard, Druid). A
class's cap rises to 10 only when its features work. Decisions on the
[inventory](LEVELS-5-10-INVENTORY.md):

- **Spell triage:** the proposed KEEP and CUT lists stand, and all nine
  UNSURE spells are kept, working as below.
- **Daylight** is cut, as Light was; Dispel Magic also ends Darkness.
- **Revivify and Raise Dead** need an offering of 300 gp × the revived
  character's level, paid from the party's money.
- **Long buffs** without Concentration (Death Ward) last into later fights, as
  Aid does; Concentration still ends with each fight.
- **Summoned and controlled creatures** (Giant Insect, Summon Dragon, Dominate
  Beast, Dominate Person, Animate Dead, Animate Objects) share one system: the
  creature acts right after its caster, controlled by the player.
- **Banishment:** an extraplanar creature banished for the full minute counts
  as defeated, for XP and loot.
- **Expertise:** the new picks at Rogue 6, Bard 9 and Ranger 9 are
  implemented, replacing SCOPE-2's "no further Expertise work" for them.
- **Reliable Talent** (Rogue 7) is implemented too, replacing SCOPE-2's cut.
- **Cunning Strike's Poison** is offered to a Rogue carrying a Poisoner's Kit
  (#233); the user decided on 2026-10-22 that carrying it is enough, with no
  tool proficiency. A Rogue's shop will sell it
  ([#271](https://github.com/stdarg/OpenGold/issues/271)).
- **Supreme Sneak:** the game has no cover, so a Thief's Stealth Attack keeps
  it hidden whenever it is hidden.
- **Adrenaline Rush** is also offered in the Camp dialog's Cast / Use list
  (#34), besides the combat button.
- **Magic Circle** works both ways: cast in a fight over 10 rounds of
  Concentration, or in camp, where those creature types cannot interrupt the
  rest.
- **Tiny Hut:** a rest inside it cannot be interrupted.
- **Animate Objects:** every battlefield has enough loose debris; the caster
  animates as many objects as its spellcasting modifier.
- **Mislead:** a monster that cannot see the caster attacks the double as if
  it were the caster; the attack passes through, revealing it, and the monster
  ignores it after.
- **Reincarnate:** the species and its traits change, the player makes the new
  species' choices and may change the portrait; class, level, ability scores,
  other skills and items stay.
- **Telekinesis:** moves a creature along the ground only; object uses are
  left out. A creature driven into a wall takes damage (amount still open).
- **Plant Growth's Overgrowth** affects only battle squares that come from map
  squares open to the sky (the original's per-square interior flag; see
  [outdoor areas](OUTDOOR-AREAS.md)). The Scoured Lands are barren, with no
  Overgrowth, until Yarash is defeated.
- **Corpses** of fallen enemies are cleaned up when the fight ends, so
  Animate Dead raises them only during the fight. A dead party member keeps
  its time of death for Revivify and Raise Dead.
- **Indomitable** (Fighter 9) is used without asking, like Heroic Inspiration
  ([HEROIC-1](#heroic-1-2026-10-14-heroic-inspiration-without-asking)).
- **Faithful Hound** moves 50 feet, half again a human's 30 feet, rounded
  up; how it is moved is settled when the Wizard is built.
- **Slow's spell failure:** the user does not want a random 25% chance; a
  slowed caster's spells fail through a saving throw instead, settled by
  [LEVELS-5](#levels-5-2026-10-16-the-last-open-spells).

## CAP-1 (2026-10-14): the original Training Hall's limits

Pool of Radiance's manual (p. 6) says Phlan's Training Hall trains up to "8th
level fighter, 9th level thief, and 6th level cleric and magic-user". The
user chose to keep those limits. A character stops at its class group's
limit (the hall that trains it, [TRAINING-HALL](TRAINING-HALL.md)): Fighter,
Barbarian, Paladin and Ranger at 8; Rogue, Bard and Monk at 9; Cleric, Druid,
Wizard, Sorcerer and Warlock at 6. The rules still carry each class to its own
cap ([LEVELS-1](#levels-1-2026-10-14-levels-510)); this limits the campaign,
in place of [SCOPE-1](#scope-1-2026-09-30-level-cap-deferrals-and-rare-situations)'s
playable cap of 15. [LEVELS-2](#levels-2-2026-10-15-level-cap-15) replaces the
three limits with level 15 for every class.

## LEVELS-2 (2026-10-15): level cap 15

The user raised the level cap to 15 for every class: each class is built to
15 (the facts are in the [levels 11–15 inventory](LEVELS-11-15-INVENTORY.md)).
The Training Hall trains every class to 15, the game's maximum, in place of
[CAP-1](#cap-1-2026-10-14-the-original-training-halls-limits)'s three limits.
At first the hall stopped at 10; the user then set the game's maximum at 15.
A class reaches only the levels whose features are built, so a class still
being built stops at its own cap until its work is done. Decisions taken with
it:

- **Empowered Strikes** (Monk 6): Unarmed Strikes always deal Force damage.
- **Stunning Strike** is also offered on Flurry of Blows.
- **Rolls:** a feature that changes a roll is used automatically when it can
  change the result, and the combat log shows it with the roll (Disciplined
  Survivor's Focus reroll, as Indomitable and Heroic Inspiration).
- **Improved Cunning Strike** (Rogue 11): the A cycle offers each pair of
  Cunning Strike effects the Sneak Attack dice can pay for, besides the single
  effects.
- **Heightened Focus** (Monk 10): Step of the Wind with Focus can carry an
  adjacent ally, offered as more A-cycle entries.
- **Use Magic Device** (Thief 13) is granted and waits for magic items
  ([#234](https://github.com/stdarg/OpenGold/issues/234)), as Fast Hands does.
- **Aura of Protection** in the combat log: each save line names the aura's
  share of the bonus ("+ 5 (Aura of Protection +3)").
- **Revivify** reaches 10 feet, the adjacent square and one more (the
  original's squares are 10 feet, see [DEVIATIONS](DEVIATIONS.md#rules-and-combat)),
  in place of the SRD's touch. It is cast in a fight, or in camp right after
  one while the death is less than a minute old.
- **Magic Circle**: one A-cycle entry per creature type, aimed at a square,
  standing after 10 rounds of Concentration; in camp, the same choice from
  the Cast / Use list wards the rest.
- **Remove Curse** is built now; Hex is its curse until others come.
- **Smite of Protection** (Devotion 15): the game has no cover (COVER-1), so
  in place of Half Cover the Paladin and its allies in the Aura of Protection
  gain what Half Cover gives: +2 AC and +2 to Dexterity saves, until the start
  of the Paladin's next turn.
- **Controlled creatures** come with the Ranger: LEVELS-1's shared system for
  summoned and controlled creatures arrives with Dominate Beast (Ranger 13),
  and the Druid builds on it. The Ranger's Conjure Animals and Conjure
  Woodland Beings conjure spirits that only deal damage around a point or the
  caster, not creatures.
- **Superior Hunter's Prey** (Ranger 11): the second creature is picked by
  click, as Horde Breaker's is. **Superior Hunter's Defense** (Ranger 15) is a
  React/Decline prompt when the Ranger takes damage, as Uncanny Dodge is.
- **Ranger prepared spells** follow the SRD table (p. 58), 5 at level 4.
- **Expertise** at Rogue 6, Bard 9 and Ranger 9 stands as LEVELS-1 decided.

## LEVELS-4 (2026-10-16): Wizard spells, Antimagic Field and summons

The user answered the questions the Wizard's levels 5–8 raised:

- **Faithful Hound:** a Magic action moves it up to 50 feet (LEVELS-1) beside
  an enemy the Wizard can see; it bites at the start of the Wizard's turns
  and cannot be harmed.
- **Conjure Minor Elementals:** the extra damage type is chosen once, when the
  spell is cast (one entry per type, as Protection from Energy has); the SRD
  chooses it at each hit.
- **Antimagic Field** stops every spell and every Magic action in it, and
  magic items do not work there.
- **Polymorph:** creature stat blocks carry no Challenge Rating, so the limit
  to a Beast no stronger than the creature is not checked.
- **Summoned creatures** (Summon Dragon's Draconic Spirit, Animate Objects'
  objects, Mislead's double) join the fight as new combatants that act right
  after their caster, controlled by the player (LEVELS-1), drawn with the
  closest existing art (the game's dragon for the spirit).
- **Confirmed:** Confusion's wandering provokes no Opportunity Attacks; Wall of
  Fire hides creatures in it as fog does without blocking sight beyond it;
  Divine Word, Holy Aura and Conjure Celestial choose foes or allies
  automatically; Fire Storm's cubes lie on a 10-foot grid.

## LEVELS-5 (2026-10-16): the last open spells

The user answered the questions left after every class reached level 15:

- **Slow:** the SRD gives no save for its spell failure, only a 25 percent
  chance for spells with a Somatic component. A slowed caster instead makes
  a Wisdom save, Slow's own save, against Slow's DC; on a failure the spell
  fails and its slot is spent. Slow's "creatures of your choice" are the
  caster's enemies in the Cube, up to six, the nearest to its centre first.
- **Summon Dragon:** one damage type chosen at the casting sets both the
  spirit's breath and the Resistance it shares; the SRD chooses them
  separately. The spirit wears Tyranthraxus's combat icon.
- **Wall of Force and Wall of Ice:** a creature in the wall's line is pushed
  to the nearest free square; the SRD lets the caster pick its side.
- **Befuddlement** lasts until the fight ends, with no 30-day saves.
- **Simulacrum:** the copy carries none of the original's gear, and it is
  repaired from the Camp dialog at any time, not only during a Long Rest.
- **Gift of the Protectors:** every party member counts as written in the
  Book of Shadows, which is not modeled (TOME-1).
- **One with Shadows** is cut: battle maps have no light levels.
- **Tsunami** is cast with an Action instead of in a minute, and its wall is
  10 feet thick instead of 50.
- **Reincarnate** reuses the character-creation species page for the new
  body, and takes the same offering as Raise Dead, 300 gp × the character's
  level. The SRD leaves its Hit Points open: the new body has all of them,
  without Raise Dead's penalty. A Human body whose Versatile feat is Magic
  Initiate picks its spells on the next page. A species skill the character
  built Expertise on stays with the new body, though the SRD takes the old
  species' traits away.

## LEVELS-3 (2026-10-15): the open decisions of levels 11–15

The user answered the [levels 11–15 inventory](LEVELS-11-15-INVENTORY.md)'s
open decisions, with two questions the work on every class to 15 raised:

- **Walls** (Wind Wall; later Wall of Fire, Wall of Force, Wall of Stone, Wall
  of Ice, Wall of Thorns and Blade Barrier) are straight lines of squares
  centred on the aimed square. More A-cycle entries pick the direction:
  horizontal, vertical or either diagonal. A spell that allows a ring also
  gets a ring entry. The SRD lets the caster shape a wall freely.
- **Bard Expertise at level 2** is built too, replacing SCOPE-2's skip for it.
- **Costly components:** Resurrection takes the same offering as Raise Dead,
  300 gp × the revived character's level. Heroes' Feast, Forcecage, True
  Seeing and Simulacrum stay free, as CLASS-11 made components.
- **Animal Shapes** and the undead of Create Undead and Finger of Death end
  with the fight, as Wild Shape (WILD-1) and Animate Dead's corpses do.
- **Disintegrate:** Resurrection can revive a creature turned to dust; Raise
  Dead and Revivify cannot. The SRD needs level-9 magic, beyond the cap.
- **Dragon Wings** (Draconic Sorcerer 14): flight is cut, so while the wings
  are out the Sorcerer ignores Difficult Terrain and may move through
  enemies' squares, for the hour the wings last (once per Long Rest, or for 3
  Sorcery Points).
- **The UNSURE spells:** kept, working as below; Clone is cut.
  - **Contingency:** the stored spell fires on a trigger picked from a fixed
    menu (dropping to half Hit Points, a fight starting, dropping to 0).
  - **Magic Jar:** works as Dominate Person on a Humanoid while the caster's
    body lies helpless on the battlefield.
  - **Word of Recall:** the party returns from anywhere to a temple in New
    Phlan prepared earlier with the spell.
  - **Teleport:** fast travel, without mishaps, to towns and areas the party
    has visited.
  - **Reverse Gravity:** falling damage on the PUSH-1 scale; lifted creatures
    are out of melee reach until it ends, then take falling damage again.
  - **Simulacrum:** a lasting copy of a Beast or Humanoid with half its Hit
    Points that joins the party as a controlled creature.
  - **Tsunami:** a wall of water that moves across the battlefield, with
    Strength saves, damage and a push each round.
- **Fire Storm** is aimed cube by cube: the player clicks up to ten 10-foot
  cubes, each touching another, then casts, as the SRD lets the caster
  arrange them.
- **Create Undead** works at any hour: the SRD's night-only limit is dropped.
- **Word of Recall** is cast outside fights, from the Camp dialog's Cast / Use
  list, and takes the whole party back to the prepared temple; the SRD takes
  the caster and five others.

## PUSH-1 (2026-10-14): pushed into a wall

The SRD gives no damage for a push that a wall stops. The user approved one,
on the scale of the SRD's falling damage: a creature pushed into a wall, an
obstacle or another creature takes 1d6 Bludgeoning damage for each 10 feet, or
part of it, the push could not cover, up to 3d6. It applies to every push in
the game: Thunderwave, Repelling Blast, Gust of Wind, the Open Hand's push,
the Push mastery (when the creature is pushed as far as it can go) and, when
built, Telekinesis. The battlefield's edge is open ground, not a wall.

## MAGIC-1 (2026-10-17): the original's enchanted weapons and armor

The user chose how the original's enchanted weapons, armor and shields
([MAGIC_ITEMS.md](MAGIC_ITEMS.md#weapons), issue
[#261](https://github.com/stdarg/OpenGold/issues/261)) work:

- **+N as the original has it.** A weapon's bonus adds to its attack and
  damage rolls; armor's and a shield's add to Armor Class. The original's +4
  and +5 items (Tyranthraxus's Long Sword +5, the buccaneer captain's Long
  Sword +4, the Leather Armor +4 in Kovel Mansion) keep their bonus, although
  SRD 5.2.1 stops at +3.
- **Long Sword +2 Flame Tongue** works as the SRD's Flame Tongue on top of its
  +2: a Bonus Action lights or douses the blade, and while lit it deals an
  extra 2d6 Fire damage on a hit. It needed attunement until 2026-10-20, when
  attunement left the game
  ([MAGIC-4](#magic-4-2026-10-20-no-attunement)).
- **Two-Handed Sword +1, +3 vs Undead** has +1, or +3 against an Undead
  target, to attack and damage rolls.
- **Cursed Broad Sword -2** follows the original: -2 to attack and damage
  rolls, and a character wielding it cannot put it down until Remove Curse is
  cast on them. Put down, it is lost: Remove Curse cast in camp takes it away
  at once, and cast in a fight, when the fight ends (the user, 2026-10-18).

## MAGIC-2 (2026-10-18): identifying magic items

The user chose how found magic items are identified (issue
[#262](https://github.com/stdarg/OpenGold/issues/262),
[MAGIC_ITEMS.md](MAGIC_ITEMS.md#in-opengoldbox)):

- **Hidden names as the original has them.** Until it is identified, an item
  shows only the name words its record does not hide ("Long Sword", "Chain
  Mail", "Potion"). Its bonus and powers work regardless, and its numbers on
  the character sheet already count them.
- **The shop's ID**, 200 gp per item, as in the original. Any item can be
  submitted; one with nothing hidden still costs the fee.
- **The SRD's Identify spell** reveals one item's name. As CLASS-8 says, it
  needs no pearl. When any member can cast it, the inventory offers it on the
  selected item (the user, 2026-10-18), on every item, so the offer does not
  give away which items are magic.
- **A Cast menu** on the exploration screen casts spells and uses features
  outside combat, the same ones Camp's Cast / Use offers (the user,
  2026-10-18).
- **No Short Rest study.** The SRD's way of learning an item over a Short Rest
  is left out.
- **Detect Magic** can be cast in camp or at any time outside combat. For ten
  minutes of game time, item lists mark each magic item with "* ".
- **Flame Tongue unidentified.** Its wielder can light it before it is
  identified. (Attuning to it revealed its name until attunement left the game,
  [MAGIC-4](#magic-4-2026-10-20-no-attunement).)
- **Cursed items** give no warning before they are equipped. Once one is, the
  refusal to unequip it says that it is cursed.

## MAGIC-3 (2026-10-19): wands, potions and other charged items

The user chose how the original's charged and single-use items work (issue
[#263](https://github.com/stdarg/OpenGold/issues/263),
[MAGIC_ITEMS.md](MAGIC_ITEMS.md#wands)):

- **SRD 5.2.1 effects, the original's charges.** Each item does what its SRD
  counterpart does, but keeps the charges or uses its record holds (a Wand of
  Magic Missiles has 20 to 67), and nothing recharges.
  - Wand of Magic Missiles: Magic Missile for each charge.
  - Wand of Lightning and Wand of Lightning Bolt: the SRD's Wand of Lightning
    Bolts, 8d6 Lightning in a line, Dexterity save DC 15 for half.
  - Wand of Paralyzation: the SRD's Wand of Paralysis, a Constitution save DC
    15 or Paralyzed for up to a minute, saving again at the end of each turn.
  - Potion of Healing: 2d4 + 2.
  - Potion Extra Healing: its three doses are three Potions of Greater Healing,
    4d4 + 4 each.
  - Potion of Speed: Haste for a minute, without Concentration.
  - Potion of Giant Strength: Strength 21 for an hour.
  - Dust of Disappearance: the party is Invisible for 2d4 minutes.
- **Necklace of Missiles** keeps its original name and its six missiles; each
  explodes as the SRD's Fireball, 8d6 Fire damage, Dexterity save DC 15 for
  half (the SRD's Necklace of Fireballs is the same item).
- **No attunement.** No wand or other charged item needs attunement; anyone
  can use them.
- **Javelin of Lightning** works as the SRD's: once a day its throw becomes a
  bolt of lightning, and the javelin is kept.
- **Empty items are gone.** A wand or potion disappears when its last charge or
  dose is used, as in the original.
- **Manual of Bodily Health**: +2 Constitution and +2 to its maximum, with Hit
  Points adjusted to match. It is read in camp from the inventory, without the
  SRD's 48 hours of study, and its magic is then spent.
- **Efreeti Bottle** stays the original's scripted quest item.
- **Using items.** Potions are drunk from the inventory, without equipping
  them, as a Bonus Action. A wand must be equipped, and using it takes the
  Magic action. A Thief's Fast Hands uses either as a Bonus Action.
- **Quick combat** follows the original: members on Quick always use their
  equipped magic items, whether or not Quick magic is on, when the item makes
  sense (an attack needs a target in range, healing needs a wounded ally, and
  an area effect is skipped when it could catch an ally). The full
  specification of the AI is [#266](https://github.com/stdarg/OpenGold/issues/266).

## MAGIC-4 (2026-10-20): no attunement

The user decided that OpenGoldBox has no attunement at all (issue
[#264](https://github.com/stdarg/OpenGold/issues/264)). This changes SRD 5.2.1,
where many magic items need a creature attuned to them and a creature is
attuned to at most three items:

- **No item needs attunement.** Every magic item works for whoever wields,
  wears or uses it, as in the original, which had no attunement.
- **No limit.** A character may use any number of magic items, within the
  slots each kind of item has.
- **Flame Tongue** lights for anyone who wields it, and a rest no longer
  reveals its name.
- **Use Magic Device** keeps only its charge rule; its fourth attunement slot
  has nothing to do.

## MAGIC-5 (2026-10-20): rings, bracers and other worn items

The user chose how the original's worn items
([MAGIC_ITEMS.md](MAGIC_ITEMS.md#rings-and-worn-items), issue
[#264](https://github.com/stdarg/OpenGold/issues/264)) work:

- **Slots as the original has them.** A character wears two rings and one
  each of bracers, a cloak, gauntlets and a necklace. Worn items need no hand.
- **Ring of Protection +1 to +3** adds its bonus to Armor Class and to saving
  throws. Only the better of two counts, as in the original, and it stacks
  with magic armor, as in the SRD.
- **Bracers**, as the original's: Armor Class is 10 + the bracers' improvement
  + the Dexterity modifier, worn instead of armor (the better of the two
  counts), and a shield still adds. The nomad shaman's "Bracers AC 2" have the
  same improvement as AC 4 bracers, as in the original.
- **Cloak of Displacement**, as the original's: +2 Armor Class, and the first
  attack roll against the wearer in each fight misses. A natural 1 misses
  anyway and leaves the cloak's miss for a later attack.
- **Gauntlets of Ogre Power**, as the SRD's: Strength 19 unless it is higher.
- **Ring of Fire Resistance**, as the original's: ordinary fire (a Torch,
  Alchemist's Fire, burning oil) deals no damage; magical fire deals 2 less
  per damage die, but at least 1 per die, and saves against it gain +4.
- **Ring of Feather Falling** can be worn and does nothing, as in the original,
  which has no falls.
- **Cursed Necklace**, as the original's: -5 to Armor Class and to every saving
  throw. Once worn it cannot be taken off until Remove Curse; then it is lost,
  like the cursed sword (MAGIC-1).
- **The alignment-bound swords** the same research found wait for
  [#267](https://github.com/stdarg/OpenGold/issues/267).

## MAGIC-6 (2026-10-20): spell scrolls

The user chose how the original's scrolls
([MAGIC_ITEMS.md](MAGIC_ITEMS.md#scrolls), issue
[#265](https://github.com/stdarg/OpenGold/issues/265)) work:

- **Spells as SRD 5.2.1 spells.** Each of the original's 56 spells becomes an
  SRD spell: Restoration is Greater Restoration, Cause Disease is Contagion,
  Cure Blindness and Cure Disease are Lesser Restoration, Cause Blindness is
  Blindness, Detect Invisibility is See Invisibility, Spiritual Hammer is
  Spiritual Weapon, Enlarge and Reduce are Enlarge/Reduce, Curse is Bane, Cure
  and Cause Light Wounds are Cure and Inflict Wounds, Resist Fire and Resist
  Cold are Protection from Energy, Slow Poison is Protection from Poison,
  Prayer is Bless, Invisibility 10' Radius is Invisibility from a 3rd-level
  slot (two creatures), the 10' Radius protections are Magic Circle, and
  Strength is Enhance Ability for Strength. The others keep their names.
- **Stand-ins** for the five spells SRD 5.2.1 lacks, working as the original's:
  - **Friends**: cast in camp, 2d4 more Charisma for the caster, to at most
    25, for six minutes; the spokesman's Charisma sets the reaction to a parley.
  - **Read Magic**: cast in camp, it reveals the caster's scrolls.
  - **Find Traps**: cast in camp, it lasts 30 minutes. The original's trap
    scripts in Kovel Mansion and the Kobold Caves ask for it; until those areas
    run, it has nothing to find.
  - **Snake Charm**: the caster's current Hit Points are a pool, and each snake
    or scorpion, in turn order, whose Hit Points still fit is Incapacitated for
    the fight, its Hit Points leaving the pool; a hit slays one. No save.
  - **Protection From Normal Missiles**: for an hour, cast in camp or a fight
    on a creature within 5 feet, an attack from more than 5 feet away with
    ammunition, or a thrown weapon, without magic misses.
  The stand-ins are on the class lists of the original's casters: Friends,
  Read Magic and Protection From Normal Missiles are Wizard spells, Find Traps
  and Snake Charm Cleric spells, which Wizards learn and copy and Clerics
  prepare (the user, 2026-10-20). Enhance Ability stays on no class list, as
  DM-1 removed it; only scrolls hold it.
- **Readers as the original's**: arcane scrolls (the original's magic-user
  scrolls) for Bards, Sorcerers, Warlocks and Wizards; divine scrolls (cleric
  scrolls) for Clerics, Druids, Paladins and Rangers (the user, 2026-10-20). A
  reader casts any spell
  on a scroll of its kind, whatever the spell's level, with no check, as in
  the original.
- **The SRD's scroll DC**: 13 for spells of level 1 or 2, 15 for 3 or 4, 17 for
  5 or 6; a spell attack adds the DC less 8.
- **Scrolls must be identified** (the shop's ID, Identify or Read Magic) before
  their spells can be read or copied.
- **Multi-spell scrolls stay one item**, as in the original; reading a spell
  erases only that spell, and an empty scroll is gone.
- **In a fight**, a scroll's spell is read from the Items menu, without
  equipping the scroll, with the spell's own casting time. Outside combat the
  inventory's Use opens the Cast menu with the scroll's spells that work
  outside combat.
- **Copying**: a Wizard copies an arcane scroll's spell of a level it can
  prepare into its spellbook, beyond the book's usual choices: 1000 gp and two
  hours for each spell level (the user, 2026-10-21; the SRD asks 50 gp). The
  gold is **Magic Ink**, a new item (the user, 2026-10-20). Each bottle has a
  level, from 1 to the highest spell level the game has (8), and costs 1000 gp
  a level; it serves a spell of its level or lower (the user, 2026-10-21). The
  general store sells every level until a magic shop does. Copying erases the spell from the scroll, as in the
  original.
- **Writing a scroll** (the user, 2026-10-21): any caster writes a scroll of
  one spell it can cast, in camp (the Camp dialog), with a bottle of Magic Ink
  of the spell's level or higher, in two hours a spell level, spending no
  spell slot. The SRD's scribing takes days and gold instead; the original
  had no scroll writing. No cantrip scrolls: ink has no level 0.
- **Use Magic Device** (Thief 13), as the SRD's: the Thief reads any scroll,
  with Intelligence as its spellcasting ability. A spell above level 1 needs an
  Intelligence (Arcana) check, DC 10 + the spell's level; on a failure the
  spell is lost from the scroll (the SRD's single-spell scroll disintegrates).
- **Quick members read scrolls** (the user, 2026-10-20), whether or not Quick
  magic is on, as they use their other magic items: a healing spell for a
  creature at half its Hit Points or lower, a spell at an enemy, an area only
  where no ally can be caught. Spells that help an ally otherwise wait for the
  player. Monsters carry no items into a fight yet
  ([#268](https://github.com/stdarg/OpenGold/issues/268)).



## MAGIC-7 (2026-10-22): alignment-bound swords

The original binds three swords to an alignment: the Long Sword +2 in
Valhingen Graveyard's knight's tomb to Lawful Good (15 damage) and the two Long
Swords +3 behind the altar in Valjevo Castle SW to Lawful Evil (5 damage). A
wielder of another alignment who readied one lost it from the hand and took
magic damage. The user decided on 2026-10-22 (#267):

- **The original's damage**, as Force damage, each time a member of another
  alignment equips the sword, which stays in the pack.
- **Good or evil only**: the Lawful Good sword serves any Good member, the
  Lawful Evil swords any Evil member; a member Neutral on that half is burned.
- **Its note says so once identified**; until then it reads as its plain +N.
## AI-1 (2026-10-21): computer play, as the original game had it

How the computer plays monsters and Quick members is described in
[AI.md](AI.md); the user decided its open questions on 2026-10-21 (#266),
following the original Gold Box engine (read from coab; no code copied):

- **Targets**: a creature keeps attacking its target while it can reach it,
  otherwise an enemy at random, not the weakest.
- **Sight**: a creature that sees no enemy holds its ground until one comes
  into view, even when no one on the field sees anyone (the user,
  2026-10-21); a fight the computer plays alone then waits for the player.
- **Magic-users stay back**: an unarmored Wizard or Sorcerer, or a monster
  magic-user, never closes in.
- **Spells by priority**: the original's 1d7 roll over each spell's priority
  (1 to 7), the strongest first; area spells are cast where they catch no ally;
  a spell that needs Concentration waits while the caster holds one.
- **Ranged weapons**: with no enemy beside it, a creature shoots when its
  ranged weapon rates more than half its melee weapon (the original's rating,
  counting attacks a turn).
- **Healing** goes to the most wounded ally at half its Hit Points or lower,
  the same line for spells, features and items.
- **Shield** is cast only when +5 AC turns the hit into a miss; the prompt
  shows the attack's total and the AC to the player too.
- **A Champion's free move** is taken toward the enemy.
- **Flee mode** obeys Quick magic for members who cannot flee.
- **Features it had never used** (the user, 2026-10-21, #269): Light and Nick
  extra attacks follow the Attack action at the same target; a thrown weapon is
  rated and thrown as the original's were; Action Surge once the Action is
  spent with an enemy in view; a smite that costs a slot only on a Critical
  Hit, from the lowest slot; Steady Aim when it can attack without moving;
  Cunning Action's Dash or Adrenaline Rush when its target is out of reach even
  with all its movement; Disengage before running from an enemy beside it;
  Dodge instead of ending its turn with the Action unused beside an enemy;
  Breath Weapon and Dragon's Breath's exhale where they catch an enemy and no
  ally; Dragon's Breath cast as a helpful spell of priority 3; putting out
  flames at half Hit Points or lower; Wholeness of Body at half Hit Points or
  lower. Hide is left to the player.

## DIFF-1 (2026-10-21): difficulty, and monsters that use their items

The original had no difficulty setting: its Alter menu offers only Order, Drop,
Speed, Icon and Pics. The user decided on 2026-10-21 (#268):

- **Five levels**, easiest first: Easy, Normal, Hard, Nightmare, Infernal.
  Normal is the default. The setting is `[game] difficulty` in `settings.cfg`
  ([CONFIGURATION.md](CONFIGURATION.md)); the Settings screen will show it
  later. For now it decides only whether monsters use their items.
- **Monsters use their potions, wands and scrolls on Hard and above**, not on
  Normal and below. A monster brings the ones its original record carries into
  the fight and uses them by the same rules as Quick members (AI.md §5):
  healing at half its Hit Points or lower, Speed and Giant Strength on itself,
  a wand or scroll at an enemy. A monster needs no hand free for its wand.
  The original's monsters used their readied wands at every level and never a
  potion or scroll.
- **Only a monster that casts spells reads a scroll**, of either kind: a
  monster has no class list.
- **What a monster uses is gone from the loot**, as in the original: spent
  charges, drunk potions and read spells.

So far the one such monster the game meets is the Slums magic-user, whose
Wand of Magic Missiles it zaps on Hard.

## TEMPLE-1 (2026-10-22): temple services

The original's temples cast nine clerical spells for a price, the same at every
temple; only Cure Light Wounds was in the game, as Cure Wounds for 100 gp. The
user decided on 2026-10-22:

- **Each service casts an SRD 5.2.1 spell** under the original's name, as a
  temple cleric with Wisdom +3:

  | Service | Spell | Helps |
  |---|---|---|
  | Cure Light Wounds | Cure Wounds, level-1 slot (2d8 + 3) | a wounded living member |
  | Cure Serious Wounds | Cure Wounds, level-3 slot (6d8 + 3) | a wounded living member |
  | Cure Critical Wounds | Cure Wounds, level-5 slot (10d8 + 3) | a wounded living member |
  | Cure Blindness | Lesser Restoration | a Blinded member |
  | Cure Disease | Lesser Restoration | a member with Contagion's disease |
  | Neutralize Poison | Lesser Restoration | a Poisoned member, other than by disease |
  | Remove Curse | Remove Curse | a cursed member, or one holding a cursed item, which is lost |
  | Stone to Flesh | Greater Restoration | a Petrified member, or one turning to stone |
  | Raise Dead | Raise Dead (1 Hit Point, −4 to D20 Tests easing by 1 a Long Rest) | a member dead up to ten days, not turned to dust |
  | Resurrection | Resurrection (all its Hit Points) | a member Raise Dead cannot help |

- **The price is the whole cost**: Raise Dead's 5,500 gp includes the offering
  camp revival takes (300 gp × level).
- **Resurrection is offered only on Easy and Normal**, at 11,000 gp. On Hard
  and above, the Raise Dead list shows a member it cannot help with the reason,
  and the temple cannot bring that member back.
- **Prices come from the Shop & Temple Manager's files** in
  [`data/shops/`](../data/shops/README.md), which start at the original's
  prices; each temple's prices and **Offered** switches apply where its service
  opens. A temple the files do not name charges the original's prices.
- **The Heal list** works like the training hall's: choosing a service lists
  the members it would help, and choosing a member asks to confirm the price, as
  the original did. The party may buy several services before leaving.
- **The party's money is always pooled**, so the original's Take, Pool and
  Share commands are not needed (#232).
