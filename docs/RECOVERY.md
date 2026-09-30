# Campaign recovery and advancement subset

Issue [#6](https://github.com/stdarg/OpenGold/issues/6) now has a bounded native
implementation and a Godot acceptance route. It is not full SRD advancement or
complete original campaign service coverage.

## Supported behavior

- In combat, an unstable member at 0 HP makes one death save on turn entry,
  including the initial initiative slot. A natural 20 restores 1 HP and permits
  that turn; becoming Stable clears both death-save counters. Combat checkpoint
  restoration adds no roll, and campaign handoff preserves stabilization and
  spent resources. Stable status now records its one rolled recovery delay;
  combat time counts it down and restores 1 HP. [Recovery clocks](RECOVERY-CLOCKS.md)
  also survive healing, damage and save/load. Campaign time advances the same
  mortality rules for active members and reserves, interleaved with lasting
  effects; repeated combat snapshots cannot apply elapsed time twice.
- The authored party Bandit preview grants **300 XP per living active member**
  on its first victory. Its reward key is `preview:bandit:v1`; reopening the scene,
  restarting it or using a different seed cannot award it again. The original
  four-orc Slums adapter has a separate key, `por:ECL2:20:search1:orcs:v1`, and the
  same explicitly authored 300 XP conversion when a campaign party is attached.
  These amounts are conversion policies, not decoded original XP or general
  monster-XP calculations. Dead and reserve members receive no award.
- Fighter, Cleric and Wizard now use [manual advancement](ADVANCEMENT.md) through
  level 4. XP enables a small arrow beside the name; Confirm applies one level,
  while Cancel changes nothing. Fixed-average HP, supported feats and spells,
  Constitution changes and new resource capacity apply together. Existing
  expenditure is preserved; advancement does not wake or revive a character.
  Full class/subclass features and other classes' advancement remain unavailable.
- A Long Rest checks each active member at its start: at least 1 HP, alive and
  at least 16 hours since the previous completion. Eligible members recover HP,
  spent Hit Dice and supported resources after eight hours; ineligible/reserve
  members gain no rest benefits. Time, effects and mortality advance once for
  the whole roster.
  If nobody is eligible, the request changes nothing. Removal/rejoin preserves
  individual timers. Short Rest transactions now recharge one Second Wind and
  permit one Hit Die decision at a time after the hour, with persistent tickets
  and immediate resource/RNG commits. [Rest controls #192](https://github.com/stdarg/OpenGold/issues/192)
  remain pending. See [rest support](REST-RESOURCES.md) for the APIs, persistence,
  effect handling and unsupported interruption/resumption boundaries.

## Original campaign mappings

**Camp [C]** runs ECL entry 2 before any recovery. `6DD3=255` denies rest.
An interruption-free profile permits the requested rest for eligible members.
New Phlan's guaranteed city-watch interruption (`6DD2=1`, `6DD3=100/101`) advances five minutes, runs
entry 3, and grants no rest benefits. Mortality and effect timers still advance.
Choose **GO** to leave peacefully; combat with
the watch remains unsupported and rolls the event back. Other nonzero
probabilistic interruption profiles fail explicitly.

The original inn's **PROGRAM 9** request follows its own pre-camp subroutine and
payment dialogue. A safe, eligible request completes a long rest. The script
collects one platinum piece from the chosen payer; the host does not invent a
currency exchange. Failed services restore the event checkpoint, including any
payment. Original training requests (PROGRAM 0) and victory services (PROGRAM 8)
remain unsupported. Manual SRD advancement does not imply payment of an
original training fee or completion of a training-hall script.

The original temple **COMBAT** request with `6DE2=1` offers the supported
**Cure Wounds** service using the existing dialogue list. Select a wounded living
active member, or **Cancel**. The original Cure Light Wounds price is retained at
**100 gp**, collected in active-slot order from gold purses only. Its healing is
explicitly converted to SRD Cure Wounds, **2d8 + 3**, with an authored Wisdom +3
temple caster. Dice are deterministic and campaign-owned; the rules module
calculates healing, caps HP and clears death-save state without refilling slots
or Second Wind. Resurrection and other temple spells remain unsupported.

Unaffordable, dead, full-health, reserve and stale-ticket requests do not charge
or change HP, resources or RNG. Success and cancellation clear `6DE2` and resume
the waiting original script. A later unsupported instruction rolls back the
entire event, including healing, payment and RNG.

Campaign minutes are authoritative for recovery. ECL minute/hour/day fields are
synchronized from a noon, day-one starting point, using a bounded 30-day-month,
12-month calendar. Successful forward steps advance six seconds; a full combat
round advances six seconds. Milliseconds within a minute and precise rest
completion offsets survive saves, so timing is retained across encounters.
Turning, looking, blocked steps and menus consume no time. Lasting effects
advance during movement, combat, waits and rests; see [status effects](STATUS-EFFECTS.md). General
campaign scheduling, quest rewards and non-shop treasure conversion remain open.

## Persistence and verification

`PartyState` native checkpoints retain XP, claimed reward IDs, HP/resources,
purses, recovery timers, clock and RNG for rollback. [Campaign file save/load](SAVES.md) now persists this supported state at the party/idle-town boundaries, with fresh-process restart verification.
The combat checkpoint format is `OGCOMBAT 27` and the rules module identity is
`opengold.srd5` **0.6.62**. Older formats and other identities reject; see the
[pre-1.0 format policy](SAVES.md#pre-10-format-policy).

From PowerShell:

```powershell
.\demos\build-rolf.cmd
godot --headless --path demos/godot res://scenes/character_creation.tscn -- --party-check
```

The deterministic route uses the actual creation/shop/combat callbacks, verifies
the level-two sheet, then exercises the original city-watch interruption, temple
payment and inn rest, rejects a repeated rest and fights again without duplicate
XP. The service fixture supplies one wounded survivor, 200 gp and one platinum;
a Stable companion and an unstable reserve also verify natural recovery and
a death save during the interruption in the game route. These are test-only setup
changes. Add `--capture` and omit `--headless` for local
captures. Native party tests separately cover threshold boundaries, Dwarven HP,
caster resources, overflow, reward reentry, checkpoint continuation, rest denial,
temple eligibility, payment failure and event rollback.

Rules references: [SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
character advancement, class tables, Dwarven Toughness, Cure Wounds and Long Rest.
Original interface/price evidence: [Lee's PC 1.3 research](https://gamefaqs.gamespot.com/c64/578753-pool-of-radiance/faqs/73869),
sections 8.5, 12.3 and 12.4.2. Original dialogue, scripts and assets are loaded
from the user's installation and are not distributed.

Rules 0.6.15 also persists Orc Adrenaline Rush uses and pending Temporary HP
replacement in the combat checkpoint and SRD9; see [Temporary HP](TEMPORARY-HP.md).
