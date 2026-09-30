# Rest resources

[F03a #190](https://github.com/stdarg/OpenGold/issues/190) implements Hit Dice and
supported rest recharge in the rules layer. [F03b #191](https://github.com/stdarg/OpenGold/issues/191)
adds individual campaign eligibility and a persistent spending session. The game and demo provide the [reviewed rest controls #192](https://github.com/stdarg/OpenGold/issues/192).
The [#193 interruption/resumption](https://github.com/stdarg/OpenGold/issues/193)
implementation included natural sleep, combat equipment handoff and Q37 safe
recovery; [SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation) removed all three. The current behavior is
summarized first; the later sections are the delivery history.

## Current behavior (SIMPLIFY-1, 2026-09-30)

- A rest completes in one step: `CampaignParty::rest(kind)` captures the eligible
  members, advances the rest's time once and grants its benefits. There is no
  rest activity, sleep/light/exertion accounting, partial Short Rest credit,
  extra hour per interruption or resumption.
- An encounter or event that interrupts a rest simply interrupts it: nothing is
  granted and the party rests again. New Phlan's city watch advances five
  minutes and the exploration text says "The rest was interrupted. Rest again to
  recover." Camp restrictions and the paid inn Long Rest are unchanged.
- Resting never puts anyone to sleep and never unequips or drops gear. A
  character resting when an encounter interrupts its rest starts that combat
  awake and Prone (`Participant::resting`); see
  [Recovery clocks](RECOVERY-CLOCKS.md) for death saves after combat.
- The rest dialog has no Resume Long Rest or End Rest; it offers Start, then
  Heal with Hit Dice and Finish after a Short Rest.
- Campaign format 20 stores no rest activity and no detached (ground) items.
- Evidence: `alternate_rules_boundary`, `campaign_services` and
  `watch_interruption_and_rollback` in `opengold_campaign_rest_tests`, the
  rest-never-unequips check in `opengold_champion_tests`, and the Long Rest step
  of the Godot rest check.

## Rules and scope

Authority: [SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
pp. 47–48 (Second Wind), 185 (Long Rest) and 187 (Short Rest).

- Characters have one Hit Die per attained level: d12 for Barbarian; d10 for
  Fighter, Paladin and Ranger; d6 for Sorcerer and Wizard; d8 for the other six
  classes. Supported progression remains levels 1–4 for Fighter, Cleric and
  Wizard, and level 1 for the others.
- A single-die spend rolls that die and adds current Constitution, restoring at
  least 1 HP before the maximum-HP cap, with one RNG draw per die. Dwarven
  Toughness does not add to the healing roll. The player does not choose per
  die ([AUTO-1](SRD-DECISIONS.md#auto-1-2026-09-30-automatic-choices-with-logging)):
  one **Heal with Hit Dice** action spends dice until full HP or none remain,
  and never spends a die at full HP.
- A completed Short Rest restores one spent Second Wind use, up to capacity.
  It restores neither Hit Dice nor ordinary Cleric/Wizard slots. Spending zero
  dice still allows feature recharge.
- A completed Long Rest restores all spent dice, HP and supported slots/Second
  Wind uses. Existing campaign timing remains eight hours with a sixteen-hour
  delay after the previous completion before another Long Rest can begin.
- Recovery requires a living character with at least 1 HP. Invalid state and
  exhausted dice reject without changing vitals or RNG. Advancement adds the new
  level's die without replenishing earlier expenditure.

Other class pools, multiclass die mixtures, exhaustion, maximum-HP reductions,
species-specific rest features remain their named issues. An interrupted rest
grants nothing and cannot be resumed. Unverified original probabilistic
encounter schedules remain unsupported.

## Architecture and API

`RulesModule::recovery_info` reads die size, remaining/maximum dice, vitality
eligibility and named supported resource pools. Each pool reports its capacity,
remaining uses and Short Rest recharge amount. `can_rest` checks vitality only;
activity and cooldown checks belong to the campaign.

`recover_short_rest` applies recharge after the caller establishes completed-rest
eligibility. The rules module's `spend_hit_die` commits one die and returns its
roll, Constitution modifier, actual healing and remaining dice. These resource operations do not
independently authorize resting or spending at any time. `recover` remains the
Long Rest resource operation.

The statically linked SRD module owns arithmetic, capacities, validation and codecs,
plus rest timing and completion benefits.
Its rest transitions use engine-independent values; Core applies the outcomes
and owns campaign membership, tickets, clocks and save transactions. Core and Godot
consume typed queries and preserve the opaque continuation, without interpreting
SRD resource strings. The C++20/RAII and Godot GDExtension boundaries are unchanged.

## Campaign transactions

`CampaignParty::rest_info(kind)` returns each active member's recovery resources,
eligibility denial and precise remaining cooldown. Dead/zero-HP members are
ineligible. Reserves are absent. Eligibility is captured at the start: becoming
eligible during another member's rest does not grant benefits retrospectively.

`rest(kind)` requires the host to establish a safe location and interruption
profile. It stages elapsed time, effect recovery rolls and individual benefits
in one owned state copy. All roster effects advance once; only eligible active
members receive recharge and Long Rest completion timestamps. No eligible member
means no elapsed time or other changes. The existing `rest()` wrapper selects
Long Rest. Removal/rejoin and save/load preserve each member's cooldown.

A successful Short Rest creates a `ShortRestSession` at the completed hour, with
eligible member IDs and a session/revision ticket.
`CampaignParty::heal_with_hit_dice(ticket, member)` spends that member's dice
one at a time until full HP or out of dice, commits every roll, healing and
expenditure at once, returns each roll and increments the revision. It rejects,
changing nothing, when no die can heal (full HP or none left); a die whose
healing is blocked stops the loop. Repeated callbacks with the previous ticket
reject. The window stays open for the other eligible members and for rest
choices such as Arcane Recovery. `finish_short_rest(ticket)` closes the window,
including with zero dice spent, without reverting healing, dice, recharge or time.

The rest dialog offers **Heal with Hit Dice** for the selected member while
healing is possible, and lists every die rolled with its healing and the dice
remaining (Q30's **Spend 1 Hit Die** is superseded).

Ordinary party mutations and town events are barred while spending is pending;
selection and save/load are allowed. Positive elapsed time or a successful combat
handoff closes the window. Failed time updates or combat initialization preserve
it. Further rests require finishing the current window. Trusted native checkpoint
restoration remains available for load and campaign event rollback; the spending
flow never uses it for cancellation.

`RolfTourSession::camp(kind)` runs original ECL entry 2 for both kinds. Forbidden
camping grants nothing. The supported guaranteed city-watch interruption advances
five minutes and runs entry 3, granting no recovery or spending session. Other
probabilistic profiles remain explicitly unsupported and roll back. Safe camp
completes the requested kind. Camp [C] offers the approved Short/Long Rest picker; the original paid inn PROGRAM 9 still requests Long
Rest. Failed inn continuations restore the entire event, including payment,
resources, cooldowns, time and RNG. See [recovery mappings](RECOVERY.md).

A rest has no persistent activity: the camp and inn services call the one-step
`rest(kind)`, and an interruption advances only its own time and grants nothing
([SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation)).
Unknown probabilistic profiles never silently substitute an uninterrupted rest.

## Persistence

The **SRD9** vital continuation stores slots, Second Wind and death-save state,
the remaining die count, and an **FX8** effects record.

**OGCOMBAT 28** stores each actor's remaining dice; authored combat definitions
without a character recipe receive zero, without inventing monster Hit Dice
mechanics. **PC42** recipes derive capacity from class and level.

The campaign save stores the next rest-session ID and optional completed
Short Rest continuation after the party/town payload. It stores the
session/revision ticket, completion time and eligible member IDs. Malformed,
duplicate, inactive or ineligible members and mismatched completion times reject. The writer retains opaque spent-resource continuations;
loading a pending session repeats neither the hour nor feature recharge.

Spent dice survive healing, effects, training completion, advancement, combat
turns/checkpoints and campaign handoff.

Mortality clocks for living zero-HP characters retain the same Hit Dice/pool
fields; ordinary healing cancels mortality timing without restoring
expenditure. [Recovery clocks](RECOVERY-CLOCKS.md) describes that later increment
and the still-pending outside-combat scheduler.

## Verification

`opengold_rest_resource_tests` covers all twelve starting classes, fixed RNG
results, negative Constitution, HP caps, Dwarven Toughness, recharge limits,
caster slots, invalid/dead/unconscious state and rejection atomicity. It checks
campaign round trips, training review, advancement, healing, effects, combat
handoff and malformed checkpoint counts.

`opengold_campaign_rest_tests` covers mixed eligibility/cooldowns, active PC/NPC
and reserve boundaries, exact clock thresholds, healing until full or out of
dice with known rolls, no spending at full HP, zero spending,
duplicate/expired requests, save/load of the spending window, next combat, overflow and
malformed continuation rejection. It compares effect/RNG processing against one
independent elapsed interval and tests the original safe/denied/interrupted/
unsupported camp paths plus inn rollback. Existing save/party regressions retain
per-member cooldown behavior.

The reviewed game/demo rest controls and their campaign acceptance are delivered below.

F03a delivery validation: eleven relevant native suites, six Godot CTest entries and
the existing game/demo party-and-recovery integration routes pass. The demo's
old inn timing assertion was aligned with the game's persisted-rest-timestamp
check so travel time is included. Both GDExtensions rebuild, and localization
and diff checks pass. The first headless game route reported five ObjectDB
instances at shutdown; a verbose rerun passed without that warning. No shutdown
leak fix is claimed by this increment.

F03b delivery validation: ten relevant native suites, six Godot CTest entries,
and the game/demo party-and-recovery routes pass. Both GDExtensions rebuild;
localization and diff checks pass. The game route again reports the previously
observed five ObjectDB instances at shutdown; no shutdown fix is included here.

Rules 0.6.12 advances mortality alongside effects during rest time. A zero-HP
companion or reserve may wake naturally with one HP or die; eligibility captured
at the start still prevents unearned recharge, Hit Dice or completion timestamps.
See [recovery scheduling](RECOVERY-CLOCKS.md#campaign-time).

Rules 0.6.24 adds Fighter Action Surge at level 2: one use through level 4,
fully recharged by either rest kind. Advancement preserves expenditure. SRD9
stores the spent use alongside existing resources. See [Action Surge](ACTION-SURGE.md).

## Rest batch B: verified profile boundary

The campaign adapter now accepts only the documented guaranteed city-watch
profiles (`6DD2=1`, `6DD3=100/101`), alongside already-supported safe/forbidden
profiles. Previously it treated other chance values above 101 as city-watch
interruptions without evidence. A regression covering 102, 200 and 254 failed
before the correction; these profiles now reject without advancing time, changing vitals
or RNG, or granting a spending entitlement. Both rest kinds retain their verified
100/101 behavior. This increment closed the boundary error only; the later
control delivery is recorded below. Q29–32 approve the shared rest controls and
fresh qualifying segments. See the
[decision register](SRD-DECISIONS.md).

## Resumable activity persistence and evidence

*History: superseded by [SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation).*

The campaign save stores an optional activity record after the
rest-spending continuation. It records the session/revision, kind, start clock,
resting/fresh-segment/sleep/light/exertion/extension milliseconds, interruption
state/cause, current work and eligible IDs. Readers validate activity
structure, elapsed-clock bounds and rules timing before replacing live state.

`tests/rest_activity_checks.h`, run by `opengold_campaign_rest_tests`, verifies
fresh-segment recovery, millisecond boundaries, light/sleep and exertion limits,
stale/duplicate/overflow rejection, interrupted combat handoff, abandonment and
correct-checksum malformed records. These native checks do
not constitute completion of the remaining player workflow.

Final native-activity tree verification: all 44 native/tool checks and all 20
registered Godot checks passed (31 CTest entries including fixtures). Rebuilt
actual game/demo party routes passed original camping, temple, inn and recovery
checks. Scope review confirms C++20 Core/rules boundaries and value-owned state;
no new UI, runtime, save location or combat-saving control was introduced.

### Interrupted encounter rewards

*History: superseded by [SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation).*

XP and original encounter loot may commit while a Long Rest is interrupted and
its earned Hit Dice choices are resolved. The rest retains elapsed progress and
its extension; a committed reward advances the activity revision. Duplicate
reward IDs remain idempotent, and failed validation/overflow preserves all state.
This exception does not unlock roster, equipment, training or leveling changes.
Running rests and pending Hit Dice choices still reject reward changes. The
post-combat ECL readback may synchronize identical HP/wealth without mutation;
changed script values still reject while activity is retained.

`party_tests.cpp::interrupted_rest_victory` drives a complete real combat to
victory, retains XP/loot, checks duplicate and overflow rollback, saves/reloads,
and resumes through Long Rest completion. Before the fix the victory threw
“Finish or abandon the rest before changing the party.” This closes the native
reward handoff gap; automatic encounter scheduling and effectful ECL script
adapters remain part of #193. The reviewed controls are delivered below.

Reward-handoff final verification passed all 44 native/tool and 20 registered
Godot checks, plus actual game/demo party/recovery routes. Existing formats and
module identity are unchanged; this introduces no control or layout changes.

## Player rest controls (#192)

*The resumption controls (Resume/End Rest) below are superseded by [SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation).*

Approved Q29–31 are implemented once in `src/OpenGoldBox/rest_dialog_impl.h` and
used by the game and demo. Camp opens a centered, keyboard-accessible Short/Long
Rest picker. Each member has current HP and Hit Dice, with eligibility, cooldown
and resource recovery details for the selected member. Start runs the existing
original camping checks. Paid inns retain their original payment and Long Rest.

After a completed Short Rest, select a member and spend one die at a time. Each
roll commits immediately and displays its roll, Constitution modifier, actual
healing and remaining dice. Another die is a separate decision. Ineligible
members and exhausted dice disable spending. Finish or Escape closes spending
without undoing rolls. If a Long Rest remains interrupted, the same dialog shows
progress, extension and remaining time with Resume/End Rest. Resume invokes
`RolfTourSession::resume_camp`, rechecking original permissions; forbidden or
unsupported profiles preserve progress/resources, and failed events roll back.

Save game opens the existing save dialog from this camp/inn rest workflow.
Returning from it restores the pending rest controls. Loading clears stale roll
messages and reopens pending choices when returning to the town view. There are
no combat-saving controls. Closing an interrupted rest keeps earned healing,
spent dice and elapsed time. These controls do not claim new original
probabilistic profiles or automatic rest-interruption scheduling (#193).

Evidence: `opengold_godot_rest` presses the real controls, activates spending by
Space, finishes with Escape, checks ineligible members, sequential dice, save
continuation, resumption and cooldown. `resumption_services` in
`campaign_rest_tests.cpp` covers safe, forbidden, unknown and city-watch profiles.
The actual game/demo save restart routes include a ninth `short-rest-spending`
state: reload through the real campaign host, reopen rest controls, route Save to
the existing dialog, and spend the next die. English/Spanish catalogs are complete.


## #193 event adapter increment

*History: superseded by [SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation).*

The original ECL combat request now prepares the rest interruption before combat
participants are constructed. Short Rest ends without benefits. Long Rest retains
progress and, when earned, opens the approved sequential Hit Dice dialog. Game
and demo wait for Finish before creating combat; the pending-encounter dialog
never offers Save. Polling the encounter cannot add another extension or recharge.

Original ECL HP decreases with unchanged wealth now interrupt an active rest
transactionally. Repeated identical readback is a no-op; healing/wealth edits do
not gain a general bypass of the rest lock. A victim at zero HP does not gain
recovery. Further damage during the same interruption does not add another hour.

A player spending a Hit Die or finishing the recovery decision commits that
choice to the encounter rollback baseline. Later combat initialization failure
therefore preserves spent dice, healing and RNG. An event failing before those
choices still rolls back its original transaction.

Evidence is in `host_interruptions` (`tests/rest_activity_checks.h`), the real ECL
fixture in `rejected_combat_handoff` (`tests/party_tests.cpp`) and the shared
Godot rest-control scenario's pending-encounter stages. These exercise automatic
adapter calls, persistence, duplicate polling, rejection and actual UI signals.
This increment does not close #193: natural sleeping actors/wake-up decisions
(Q33–35 approved; dropped-item recovery remains unfinished) and
final rest-scheduling acceptance remain outstanding. Unknown original
probabilistic profiles remain explicitly unsupported.

Verification of the final event-adapter tree: all 44 native/tool checks and 22
Godot checks passed (33 CTest entries including prerequisites), plus the shared
demo `--rest-check`. Native targets were rebuilt before regression; game and demo
extensions were rebuilt before runtime checks. Commands: native CTest excluding
`^opengold_godot_`; Godot CTest selecting that prefix with the already-prepared
`godot_project` fixture excluded; headless demo `rolf_tour.tscn -- --rest-check`.
No layout, messages, save formats or module identity changed. Core reports events
and owns rollback; the static SRD library continues to decide rest outcomes.


## Natural sleep and posture increment (#193, Q33–35)

*History: superseded by [SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation).*

The static SRD library records natural sleep and persistent Prone in FX4, under
module 0.6.41. Sleep applies Unconscious without reducing HP: no actions,
movement or perception, disadvantaged initiative, automatic Strength/Dexterity
save failures and critical hits from attackers within five feet. Damage wakes
even when Temporary HP absorbs it. Explicit campaign loud noise affects only
its listed recipients; initiative alone does not wake sleepers. An adjacent ally
can spend an Action through the approved Wake ally control. Waking retains Prone.
Stand up spends half Speed; crawling adds its movement cost to Difficult Terrain.
Healing from zero HP also retains Prone.

Core applies generic rest-work hooks on the injected RulesModule; SRD mechanics
remain in the static library. Game and demo use the same legal command interface.
The approved controls support keyboard activation, canceling target selection,
translated labels and unavailable states. Internal continuation fixtures do not
add player combat-save controls.

Evidence: `natural_sleep_tests.cpp`, `natural_sleep_view_tests.gd`, recovery-clock
and campaign-rest checks. English/Spanish rendering at
1120×800 and 1920×1080 exercises the approved rows.

This is partial delivery: held-item dropping/recovery and normal rest scheduling
are still unfinished. Q35 authorizes that prerequisite; no further approval is
pending. #193 and parent #30 must remain open until their full acceptance passes.


Verification for this increment: all 45 native/tool CTest entries and all 23
Godot checks passed (35 entries including required native fixtures). Rebuilt
native targets and `opengoldbox_test_project` in `build/mac-check`; rebuilt
`opengold_godot` in `build/sprite-demo`. The shared sleep-control script also
passes in the demo. English/Spanish game and English demo captures were reviewed
at both supported sizes; demo board space was adjusted to preserve readable logs.
Localization validates 867 complete messages. Logs: `/tmp/sleep-verified-native.log`,
`/tmp/sleep-game-layout-tests.log`, `/tmp/sleep-demo-render.log`,
`/tmp/sleep-game-final-render.log`. The tested source is
this increment's commit; final documentation does not change executable inputs.

Timing: original batch began 01:27:34 UTC; Q35 implementation began 01:59:50 UTC.
The 60-minute report was late (02:49); the 90-minute overrun was reported at
02:58. No original issue was closed in this increment. Inventory ownership
investigation and several build/test corrections extended this work. Token/cost
savings are unmeasured; this record does not claim acceleration.


## Combat held-equipment increment (#193, Q35)

*History: superseded by [SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation).*

Module 0.6.42 drops character-profile weapons and shields on natural sleep or
zero HP. The SRD static library owns the held-item ledger, resulting attack/AC
changes, reach and hand-capacity checks, and pickup costs. Weapons use the free
object interaction, then an Action; recovering and donning a Shield uses an
Action. The approved Ground item dropdown and Pick up button show the offered
cost and support keyboard use in both game and demo. Ground markers in the main
game indicate item location. Equipment sprites refresh after ownership changes.

Core maps the generic encounter item identity to actual inventory identities.
Drops remove exactly one physical item; cross-character pickup preserves its
name and original-item provenance. Repeated snapshots cannot duplicate transfers,
and rejected manifests preserve campaign state and RNG. Uncollected equipment
is persisted separately, including when carried by an opponent. The combat
checkpoint stores holders, ground locations and interaction budgets; the campaign
save stores detached inventory.

Evidence: `natural_sleep_tests.cpp` covers sleep/drop, AC removal, actual lethal
hits, pickup and budgets, stale commands, malformed checkpoints, ownership
transfer, idempotence and campaign save/load. `natural_sleep_view_tests.gd` exercises
keyboard pickup, translated costs, disabled controls and both window sizes.

All 45 native/tool checks and all 23 Godot checks passed before the final legacy
ledger invariant correction. That correction has its own passing regression;
eight affected native checks and the Godot sleep check passed after rebuilding.
The rebuilt demo passed its runtime/visual checks, including a final label-spacing
correction. All 871 English/Spanish messages validate. No live verification remains.
Current integrated logs are `/tmp/held-integrated-native.log`,
`/tmp/held-integrated-godot.log`, `/tmp/held-integrated-demo-ui.log` and
`/tmp/held-integrated-render.log`. English/Spanish game captures at 1120×800 and
1920×1080 were inspected in `/tmp/held-integrated-captures`.

This is an incomplete #193 increment. Rest-time drops outside combat and their
encounter import remain unfinished. Q36 automatic collection after victory/safe
rest is pending; waking alone does not return gear, and no automatic cleanup is
implemented. Detached inventory is retained, but outside-combat recovery is not
yet playable. Static authored monster profiles lack equipment exchange metadata;
character recipes retain the existing one-weapon/one-shield restriction. Neither
#193 nor parent #30 is closed, and the full SRD goal is unchanged.


Final evidence: `/tmp/held-final-tests.log` (9/9),
`/tmp/held-demo-spacing-ui.log`, `/tmp/held-final-demo-captures`. Scope review
confirmed static-library rules ownership, generic Core inventory persistence,
existing Godot controls, RAII values and no new player saving controls. The held
increment ran from 03:08 to approximately 03:50 UTC; original batch clock remains
01:27:34 UTC. No original issue was closed. Token/cost delta was not captured.


## Natural-sleep ground equipment at camp (#193, Q35)

*History: superseded by [SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation).*

Natural sleep now releases held equipment immediately in the campaign. The
module's generic `released_equipment` query decides which equipment leaves the
character; Core transfers physical inventory and preserves stack quantities,
names and original-item provenance. The SRD implementation leaves worn armor
in place. An alternate-rules test deliberately releases armor during an awake
Short Rest to prove that Core does not substitute SRD condition/equipment rules.

Unplaced ground equipment is scoped to its rest session. Loud noise or script
damage waking a sleeper does not return it to inventory. The encounter adapter
passes initial ground-equipment ordinals alongside the original equipment recipe;
the SRD library places those items at the character's starting cell, removes
their bonuses, and uses the existing combat ledger and pickup commands.
Applying the first snapshot replaces camp records with positioned encounter
records exactly once. Camp records from an abandoned rest are retained without
being teleported into an unrelated battle.

The campaign save stores the rest-session location marker. Focused checks
cover a stack losing only its held member,
remaining stack provenance, worn armor, waking a saved sleeping record,
malformed rest IDs/ground ordinals, and ally pickup after camp interruption.
The native regression passed 44 checks and found one outdated Warlock equipment
assertion. That test now verifies physical-item conservation across held and ground
inventory; its casting/wealth expectations remain intact. The corrected test and
affected save/rest/Godot checks pass (7/7). Remaining registered Godot checks passed.
A proposed initiative correction was removed after its test disproved the premise
about untrained shields. Final sleep/native and game checks pass (2/2); rebuilt
demo sleep and rest checks pass. All 871 localized messages validate.
Logs: `/tmp/rest-ground-verified-tests.log`,
`/tmp/rest-ground-final-restore-tests.log`, `/tmp/rest-ground-demo-sleep.log`,
`/tmp/rest-ground-demo-rest.log`. No live verification remains.

At this historical checkpoint #193 remained open. Q36 was subsequently
superseded by approved Q37; safe recovery is implemented in the section below. Script-induced zero HP during an awake Short Rest still releases
held equipment when combat begins. This increment adds no new controls, no
world-wide ground-inventory framework, and no automatic collection behavior.


Final review retained generic Core inventory/rest-session ownership and SRD
condition decisions in the static library. No UI controls, save locations,
frameworks, issues or agents were added. Verification completed 04:13 UTC;
original batch start remains 01:27:34 UTC. No original issue was closed. The
rest-import substep start and token/cost delta were not separately captured.


## Safe recovery and city-watch completion (#193, Q37)

*History: superseded by [SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation).*

Q37 authorizes safe standing and collection after victory or camping completion,
cancellation and obeying the city watch. During combat, waking still leaves
Prone and dropped equipment; standing and pickup retain their normal costs.
`CombatSession::safe_recovery` queries eligible party members and reachable ground
item tokens. The SRD library uses its existing terrain/occupancy/line-of-sight
rules. Zero Speed permits nearby collection only; unconscious/dead actors do not
collect. The terminal combat snapshot remains the combat record; Core applies
the recovery to the campaign before releasing combat ownership.

Core stages standing and inventory transfers in the same transaction as the
final combat handoff. Camp recovery uses the current campsite's rest-session
records. Items return to the living original owner's inventory, or to an able
survivor if that owner died; they are stowed, not automatically equipped. Labels,
quantity and original item provenance are preserved; returned stacks receive
new, unique inventory IDs. No
healing, recharge, RNG draw or extra rest benefit is granted. Unreachable items
remain saved at their encounter.

Original ECL3.DAX/11 entry 3 explicitly rouses campers and offers GO/FIGHT. The
verified guaranteed five-minute profile now begins/resumes an actual rest,
advances that interval, explicitly wakes active members, and ends camping after
successful GO. Failed/unsupported continuation rolls back the complete event.
Unknown probabilistic profiles and the unsupported city-watch fight remain
explicit. Camp/inn-only saving and paid-inn rollback stay unchanged.

Evidence: `safe_recovery` and `rest_ground_equipment` in
`tests/natural_sleep_tests.cpp`; `watch_equipment_and_rollback` and
`resumption_services` in `tests/campaign_rest_tests.cpp`; and the actual combat
host path in `interrupted_rest_victory` in `tests/party_tests.cpp`. They cover
completion/cancellation, dead and living owners, walls, repeated handoff,
physical item conservation, save/load, spent resources, and failed watch events.
Existing activity tests cover exact hour thresholds, sleep/light/exertion,
fresh segments, stale requests, mixed eligibility and retained recovery choices.

No additional UI controls, save schemas, framework, delegation or issues were
introduced. Verification completed 2026-09-25 04:38 UTC: all 45 native/tool tests,
all 22 Godot runtime checks (34 entries including native prerequisites), rebuilt
demo sleep/rest checks, and the nine-state original-data writer/restart route in
both game and demo passed. All 871 English/Spanish messages validate. The native
suite includes the alternate-rules boundary.
Logs: `/tmp/safe-recovery-native.log`, `/tmp/safe-recovery-godot.log`,
`/tmp/safe-recovery-game-save-{write,read}.log`, and
`/tmp/safe-recovery-demo-{sleep,rest,save-write,save-read}.log`.

This completes #193's supported rest/interruption acceptance and parent #30's
resource workflow. Full class catalogs, species rest exceptions and unverified
original probabilistic encounter profiles remain outside these issues' delivered
scope. The original batch clock remains 01:27:34 UTC; it was not reset.
