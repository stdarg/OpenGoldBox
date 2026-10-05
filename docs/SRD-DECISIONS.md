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
  [Removed: needs a DM](SPELL-INVENTORY.md#removed-needs-a-dm), plus ritual
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
- **Death saves outside combat:** when a fight is won, each dying character's
  remaining death saves are rolled at once until it is Stable or dead (a
  natural 20 restores 1 HP), and the combat log lists every roll and the
  outcome. Outside combat a dying character is resolved the same way the moment
  campaign time passes. Stable still recovers 1 HP after 1d4 hours. Combat
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
- **Leaders** are their base stat block wearing the armor their original record
  readies. A leader shoots a bow only when its combat art shows one; otherwise
  its better gear is loot. Of the Slums icons only the orc leader's (CPIC2 5)
  shows a bow.
- **Simplifications:** kobolds have no Sunlight Sensitivity and goblins no
  Nimble Escape. The Bugbear's Grab hits an adjacent character for its damage
  with no grapple (consistent with [SCOPE-2](#scope-2-2026-09-30-exploration-halves-and-marginal-features)).
  Aggressive adds the orc's speed as a bonus action; only monsters have it, so
  "toward a hostile creature" is left to the monster's policy.
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
