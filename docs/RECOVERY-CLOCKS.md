# Death saves and recovery clocks

[F04a #194](https://github.com/stdarg/OpenGold/issues/194) adds rules-owned recovery
clocks and shared vitality transitions. [Campaign scheduling #195](https://github.com/stdarg/OpenGold/issues/195)
completes [F04 #31](https://github.com/stdarg/OpenGold/issues/31) by continuing them
outside combat. Help/Medicine controls remain [#32](https://github.com/stdarg/OpenGold/issues/32).
No new controls or UI layout are introduced here. The existing all-unconscious
party defeat policy is unchanged.

Authority: [SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
pp. 17–18, Death Saving Throws and Stabilizing a Character.

## Rules and combat

`life_cycle.h/.cpp` owns value-based vitality transitions and clock validation.
Actors retain this state by value; there is no resource ownership through raw
pointers and no Godot dependency.

- An unstable living creature at zero HP rolls at turn entry, including the
  initial initiative slot. Three successes make it Stable and clear both counters;
  three failures kill it. A natural 1 adds two failures; a natural 20 restores
  one HP and permits that turn without replenishing resources.
- Becoming Stable rolls its 1d4-hour recovery delay once. Repeated stabilization
  preserves the existing delay. Combat elapsed time reduces the countdown and
  restores exactly one HP at its deadline. Ordinary turns remain governed by
  initiative; waking between slots does not grant an immediate extra turn.
- Any positive damage at zero HP ends Stable and cancels its recovery deadline.
  It adds one death-save failure, or two for a critical hit. Massive damage and
  normal enemy death-at-zero policy remain intact. The existing commands do not
  newly offer unconscious targets; their broader condition/targeting rules remain
  separate work.
- Positive healing clears mortality timers and both counters. Zero healing or
  zero damage does not change the recovery state. Script HP changes now use the
  rules adapter, preserving spent pools while clearing obsolete timers.
- A new encounter schedules death saves on its initiative slots. A restored
  encounter retains exact saved timing and adds no roll. After a death save,
  the next-turn countdown starts at six seconds and decrements with combat time.
- When a fight is won, every creature still dying rolls its remaining death
  saves at once until it is Stable or dead; a natural 20 restores 1 HP as usual.
  The combat log lists each roll ("{name} death save: {roll}") and the outcome
  ("{name} is stable.", "{name} dies." or "{name} regains 1 HP."). Everyone then
  stands: Prone does not outlast combat. A defeat rolls nothing.
  ([SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation))

## Campaign time

`recovery_timeline.h/.cpp` merges mortality and lasting-effect deadlines. Outside
combat there is no six-second death-save timeline (F04c's scheduler is removed by
SIMPLIFY-1): as soon as campaign time passes, an unstable living member rolls its
remaining death saves at once, in entity order, before any other deadline. A
member can be dying outside combat only through script damage, since a won fight
already resolves its dying. Stable recovery heals exactly one HP at its saved
deadline and does not leave the character Prone. Simultaneous later events use
entity ID, then effect application ID; death suppresses that creature's subsequent
effect saving throws. Effect expiry still precedes an effect save at the same
instant.

Successful exploration steps, waits, camp interruptions and rests advance active
PCs, recruited NPCs and reserves through this one sequence. Large intervals and
arbitrary subdivisions preserve the same vitality, effects and RNG. Zero elapsed
time, queries and load never roll or rewrite a continuation. Combat snapshots
advance only reserves through campaign time; participating actors retain the
combat-owned continuation, and repeated snapshots apply zero additional time.

Rest eligibility is captured at the start: a member recovering during someone
else's rest gains one HP but no rest recharge, Hit Dice or completion timestamp. Healing
and advancement retain their existing resource rules. Candidate state and RNG
commit together, so malformed participants, clock overflow, rejected combat
snapshots and failed original events cannot partially advance recovery.

## Persistence

In **OGCOMBAT 40**, each actor row stores the remaining death-save and
Stable-recovery milliseconds after its Hit Dice count. **SRD11** vital
continuation carries both clocks alongside existing pools, counters, Hit Dice
and FX8 effects. **PC42** includes the fixed Dwarf resistance grant;
see [typed damage](DAMAGE.md).

Clocks must agree with vitality: healthy/dead creatures have none, Stable
creatures have no death-save timer, and unstable creatures have no Stable timer.
Death-save intervals cannot exceed six seconds and a Stable countdown cannot
exceed four hours. Malformed clocks reject before replacing live state.
Loading, viewing and saving alone never roll a duration or heal a character.

## Verification

`opengold_recovery_clock_tests` checks independent fixed rolls and RNG values,
third-success stabilization, one-time duration selection, exact recovery deadlines,
healing/damage/death transitions, overflow and malformed timer rejection. Combat
and campaign checkpoints retain timers and resource expenditure; training review,
advancement and temple healing keep or cancel them as appropriate. Existing death-save turn-entry,
shared-space, grant, training, rest, party, save and effect tests remain required.

F04a delivery checks passed: twelve targeted native suites (including fuzz smoke),
the demo and game `--party-check` routes, six Godot integration checks and
localization validation for all 559 English/Spanish messages. Both extensions
were rebuilt. Scope review confirms the existing C++20/RAII and Godot boundaries;
this increment makes no independent UI layout or control choices.

`opengold_campaign_recovery_tests` adds independent fixed-roll event ordering,
32 seeded partition/reordering cases, large elapsed time,
PC/NPC/reserve saves, combat exclusion, rejected snapshot rollback, rest eligibility
and original exploration/event rollback. The Godot recovery route begins with a
Stable companion and unstable reserve; its original five-minute interruption must
wake both using exactly one death-save draw before temple/inn/next-combat checks.


F04c delivery checks passed: thirteen targeted native suites (including rebuilt
fuzz smoke), the rebuilt demo's existing `--party-check`, the rebuilt game's
expanded companion/reserve route, six Godot integration checks and localization
validation (559 messages). The game route still reports an ObjectDB cleanup
warning on exit; its acceptance assertions and the node-ownership check pass.
Scope review confirms C++20/RAII, borrowed synchronous views and unchanged UI
controls/layout. The increment changed no save schemas and introduced no
load-time recovery.

The combat checkpoint stores sourced Temporary HP after the recovery clocks; SRD11
carries both. Natural recovery and death-save healing retain the pool. See
[Temporary HP](TEMPORARY-HP.md).

SIMPLIFY-1 evidence: `death_saves_at_victory` and `campaign_death_saves` in
`opengold_posture_tests` check the combat-end rolls and log lines, and that
combat keeps turn-entry saves; `opengold_campaign_recovery_tests` checks the
immediate outside-combat resolution with fixed rolls and RNG values.
