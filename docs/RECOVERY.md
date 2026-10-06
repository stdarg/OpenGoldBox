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
  also survive healing, damage and save/load. A won fight rolls every remaining
  death save at once and logs each roll and outcome; outside combat a dying
  member resolves the same way as soon as campaign time passes
  ([SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation)).
  Campaign time advances Stable recovery for active members and reserves,
  interleaved with lasting effects; repeated combat snapshots cannot apply
  elapsed time twice.
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
  individual timers. Short Rest transactions recharge one Second Wind and
  offer Heal with Hit Dice after the hour, with persistent tickets and
  immediate resource/RNG commits. A rest completes in one step or is interrupted
  and grants nothing; it never unequips gear. See [rest support](REST-RESOURCES.md)
  for the APIs, persistence and effect handling.

## Original campaign mappings

**Camp [C]** runs ECL entry 2 before any recovery. It sets the area's camp
profile: `6DD2` is the check interval and `6DD3` the chance. `6DD3=255` denies rest.
Otherwise the rest is checked in five-minute steps, as the original engine does:
each step counts toward the interval, and on reaching it the count restarts and
d100 at or below the chance interrupts the rest. The count carries over between
rests and is saved with the campaign. The d100 uses the campaign's saved script
random state, so a reload repeats the same outcomes. A rest with no interruption
completes as usual: a Short Rest takes one hour, a Long Rest eight hours, for
eligible members.

An interrupted rest advances time to the interrupting step, grants no rest
benefits and runs entry 3. There is nothing to resume, and mortality and effect
timers still advance. When the exploration event finishes, the text window says
"The rest was interrupted. Rest again to recover."
Supported profiles:

- `0/0`, or a zero interval or chance: the rest is never interrupted.
- New Phlan's city watch, `1/100` or `1/101`: the first step always interrupts,
  five minutes in. Choose **GO** to leave peacefully; combat with the watch
  remains unsupported and rolls the event back.
- A Slums street, `24/24`: a 24% check every two hours. A Long Rest has four
  checks and is interrupted about two times in three (1 − 0.76⁴); a Short Rest
  (12 steps) reaches a check only every second rest. Entry 3 starts an original
  roaming encounter. Its menu (Fight, Wait, Flee, Advance, Parley) and surprise
  behave as for roaming encounters, and the text window adds "Your camp is
  attacked!" Combat starts with every member resting, so each conscious member
  wakes up Prone ([SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation)).
  After a victory, the party returns to exploration and can rest again.

Other nonzero profiles fail explicitly. A failure in the camp event rolls back
the party, time, the step count and the script random state.

### Camp interruption evidence

The Slums pre-camp entry (`ECL2.DAX` record 20, entry 2 at `0x9a0e`, from
`opengold_scripts --inspect`) chooses the profile:

| Condition | Profile |
| --- | --- |
| `4A0B == 255` (`0x9a0e`) | `24/24` (`0x9a3c`) |
| `4ABB >= 254` (`0x9a19`) | `0/0` (`0x9a2f`) |
| `6E82 == 0` (`0x9a24`) | `24/24` |
| otherwise | `0/0` |

- `6E82` is the party cell's event number, `C04F & 127` (`0x996b`, `0x9987`).
  The search entry dispatches on it (`ON GOTO` at `0x99c9`). Plain streets are
  0, so camping on them is checked; special-event cells, such as event 21 around
  `(8,5)` and the Old Rope Guild's 17 and 18, are safe.
- `4A0B` is area-local, cleared on entering the Slums (`4A00..4A1F`). The
  fortune teller sets it to 251 when visited (`0xa642`) and to 255 when murdered
  (`0xa74f`, "THE GODS HAVE NOTED YOUR ACTIONS"). While it is 255, every Slums
  camp is checked, even after the Slums are cleared.
- `4ABB` counts completed Slums events. The subroutine at `0xb69c` adds one
  per call and saturates at 254 from 25 (`0xb6b5`), the "Slums cleared" state.
  It is called after fixed events finish, for example the four orcs' fight
  (`0x9e73`) and Ohlo's reward (`0xa3b2`).

The interruption entry (entry 3, `0x9a49`) saves 200 to `4A1F`, 0 to `6DCB` and
the party strength to `9808`, then jumps to `0x9b68`. That is the roaming-encounter
selection the search entry uses (`0x9b2a`–`0x9b67` roll for it first): it
picks kobolds, goblins or orcs scaled by party strength (`0x9b8a`), rolls
surprise (`0x9c01`) and offers the encounter menu or a surprised combat.

The step loop follows the Gold Box engine's resting routine as reconstructed in
Simeon Pilgrim's [coab](https://github.com/simeonpilgrim/coab) (`9dc46f1`,
`engine/ovr021.cs` `resting()`, from line 516). Each five-minute step increments
`gbl.rest_incounter_count`; on reaching `rest_incounter_period` it resets the count
and interrupts when `roll_dice(100, 1) <= rest_incounter_percentage` ("Your repose
is suddenly interrupted!"). The count is a global, not area memory, so it
carries over between rests; coab resets it only when a game starts or loads
(`engine/seg001.cs` lines 268 and 362). It also resets the profile when an area's
script loads (`engine/ovr008.cs`). coab targets *Curse of the Azure Bonds*, so its
field offsets (`Area2.cs`, `0x5a4`/`0x5a6`) are not PoR addresses. `6DD2` and
`6DD3` are matched to the two fields by role: the pre-camp entry sets them, and
New Phlan's `1/100` profile interrupts after exactly one step.

OpenGoldBox differs from that routine in two deliberate ways. The count is
saved rather than reset on load, so saving and reloading cannot reroll a camp.
The d100 uses the saved script random state rather than the DOS generator.

The original inn's **PROGRAM 9** request follows its own pre-camp subroutine and
payment dialogue. A safe, eligible request completes a long rest. The script
collects one platinum piece from the chosen payer, who
[makes change](PHLAN.md#coin-payments) from gold and silver when needed.
Failed services restore the event checkpoint, including any payment. Original
training requests (PROGRAM 0) and victory services (PROGRAM 8)
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
campaign scheduling remains open; script treasure money follows [QUESTS.md](QUESTS.md#reward).

## Persistence and verification

`PartyState` native checkpoints retain XP, claimed reward IDs, HP/resources,
purses, recovery timers, clock and RNG for rollback. [Campaign file save/load](SAVES.md) now persists this supported state at the party/idle-town boundaries, with fresh-process restart verification.
The combat checkpoint format is `OGCOMBAT 40` and the rules module identity is
`opengold.srd5` **0.6.69**. Older formats and other identities reject; see the
[pre-1.0 format policy](SAVES.md#pre-10-format-policy).

From PowerShell:

```powershell
.\demos\build-rolf.cmd
godot --headless --path demos/godot res://scenes/character_creation.tscn -- --party-check
```

The deterministic route uses the actual creation/shop/combat callbacks, verifies
the level-two sheet, then exercises the original city-watch interruption, temple
payment and inn rest, rejects a repeated rest and fights again without duplicate
XP. The service fixture supplies one wounded survivor and 200 gp; the inn's
platinum is paid as 10 gp and the logged exchange is checked;
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
replacement in the combat checkpoint and SRD11; see [Temporary HP](TEMPORARY-HP.md).
