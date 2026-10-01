# First adventure audit — issue #13

Audit date: 2026-09-19. Baseline: `0ab857d358680abec94d147cafe24ffe02d31a94`.

A normally created six-person level-one party equipped itself, defeated the
original four-orc paper encounter, earned level two, and returned to New Phlan.
Recovery on this route was blocked: the party has gold and silver, but the inn
requires platinum. (Resolved for #17: scripts now
[make change](../PHLAN.md#coin-payments), so the same payer pays 10 gp.) Two saves
survived a fresh process with byte-identical
immediate re-saves. No gameplay changes were made for this audit.

This completes the investigation in [#13](https://github.com/stdarg/OpenGold/issues/13),
not the first-adventure milestone [#11](https://github.com/stdarg/OpenGold/issues/11).

## Baseline and method

| Component | Observed baseline |
| --- | --- |
| Platform | macOS 26.6.2, arm64, Apple M1 Max |
| Build | CMake `macos-universal`, C++20 native game GDExtension |
| Godot | `4.7.2.stable.mono.official.ed1daf0bf`, OpenGL 4.1 / Metal |
| Game | `src/OpenGoldBox/godot`, actual startup, creation, town and combat scenes |
| Original files | `/Users/edmond/POOLRAD`; all 115 files match the bundled [Steam PC 1.3 MD5 manifest](../../src/OpenGoldBox/godot/config/por-pc13-md5.json) |
| ECL2.DAX MD5 | `d3b062063903482ddc60d0ac22f6d6ce` |
| GEO2.DAX MD5 | `30624c5ba51c0118b370325dc2baf9cc` |
| MON2CHA.DAX MD5 | `4783ac90288e8f7b5ecafaa685583f8a` |
| Randomness | Creation: time-derived seed, not exposed/recorded; first roll batch retained for each PC. ECL: native initial seed 5489. Combat and campaign services: 42. These are OpenGoldBox sequences, not DOS RNG equivalence. |

A temporary Godot driver invoked visible, enabled buttons and ordinary selection
signals, and sent viewport mouse input for ability dragging and battlefield
clicks. Decisions were made during the run. It did not call campaign mutation
APIs, edit saves, grant resources, or use the level-four expedition fixture.
This was an assisted playthrough through real game controls, not a manual
keyboard/mouse accessibility or discoverability study. Navigation used decoded
original maps and the current collision policy.

The ignored `build/issue13/game` project linked the existing scenes, assets and
native library. Its only project-setting change selected an isolated user-data
directory, `Godot/app_userdata/OpenGold-issue13-20260919`. Original files and the
ordinary campaign save directory were not changed. Temporary automation,
screenshots, decoded original content and saves are not committed.

Build and checks completed successfully, in macOS Bash:

```bash
cmake --preset macos-universal
cmake --build --preset macos-universal -j 6
OPENGOLD_GAME_DIR=/Users/edmond/POOLRAD ctest --preset macos-universal --output-on-failure
```

All **27 tests passed** in 14.11 seconds. The audit used the current game, rather
than a demo project. For a manual repeat, use the [existing game launch
instructions](../../src/OpenGoldBox/README.md), a separate campaign slot and your
own original files. Do not replace normal character creation with fixture flags.
The rolls below record this run; a new normal creation run will roll differently.

## Party and equipment

All six are Human, Lawful Good, level one, with 250 gp and no equipment at
creation. Cora and Elin are female; the other four are male. No rerolls were used.
Raw dice appear in their displayed order; final scores are STR/DEX/CON/INT/WIS/CHA.

| Character | Class / background / bonus | Raw rolls | Final scores | HP at level 1 → 2 |
| --- | --- | --- | --- | --- |
| Arden Vale | Fighter / Soldier / STR +2, CON +1 | 8,16,17,14,15,8 | 19/15/17/8/14/8 | 13 → 22 |
| Bryn Stone | Fighter / Soldier / STR +2, CON +1 | 13,16,12,9,9,9 | 18/12/14/9/9/9 | 12 → 20 |
| Cora Dawn | Cleric / Acolyte / INT +1, WIS +2 | 17,10,11,13,12,15 | 12/13/15/12/19/10 | 10 → 17 |
| Darin Ash | Cleric / Acolyte / INT +1, WIS +2 | 10,11,12,14,13,18 | 12/13/14/12/20/10 | 10 → 17 |
| Elin Reed | Wizard / Sage / CON +1, INT +2 | 10,14,17,7,13,12 | 10/13/15/19/12/7 | 8 → 14 |
| Fenn Holt | Wizard / Sage / CON +1, INT +2 | 13,15,12,14,11,9 | 11/13/15/17/12/9 | 8 → 14 |

Assign highest rolls to STR/CON/DEX for Fighters, WIS/CON/DEX for Clerics and
INT/CON/DEX for Wizards, then complete the normal creation controls and add each
character to the party. Follow Rolf's eight dialogue pauses to town `(0,4)` West.
Walk to the arms shop at `(13,8)` North. On the audited approach, the city-hall
message was acknowledged and the bishop's restricted entry at `(10,5)` was left
through its LEAVE choice. No commission reward or training XP was claimed.

Select each buyer in the town party list. Close the character sheet before
shopping. To reopen a finished shop event, leave south to `(13,9)` and reenter
north at `(13,8)`; Look alone did not reopen it.

| Buyer | Purchases and gold cost | Equipped AC | Gold left |
| --- | --- | --- | --- |
| Arden | Chain Mail 75, Long Sword 15, Shield 15 | 18 | 145 |
| Bryn | Chain Mail 75, Long Sword 15, Shield 15 | 18 | 145 |
| Cora | Scale Mail 45, Mace 8, Shield 15, then Leather Armor 5 | 14 | 177 |
| Darin | Leather Armor 5, Mace 8, Shield 15 | 14 | 222 |
| Elin | Dagger 2 | 11 | 248 |
| Fenn | Dagger 2 | 11 | 248 |

Cora's Scale Mail purchase succeeded, but Equip reported
`Unsupported equipment conversion: por:unsupported:54`. It remains in inventory,
unequipped. Leather Armor was the supported replacement. Total spending was
315 gp, including the unusable 45 gp purchase; 1,185 gp remained. This proves
useful equipment is affordable, with a specific purchase-disclosure defect.

## Stage results

| Stage | Status | Expected / actual |
| --- | --- | --- |
| Create normal party and enter town | Passed | Six level-one PCs, 1,500 gp total, no fixture grants; tour completed. |
| Buy and equip | Passed with defect | Supported loadouts equipped; Scale Mail took payment before revealing unsupported conversion. |
| Enter Slums | Passed with defect | Area/script changed to 20 at `(15,4)` West; exploration labels still said New Phlan. |
| First useful encounter | Passed | Original four-orc group defeated in two rounds; all six survived. |
| Receive rewards | Passed within supported scope | 300 XP each, 96 silver and one original item; unknown item effect stayed unsupported. |
| Earn advancement | Passed | All six advanced manually to level two using the legitimately earned 300 XP. |
| Pre-combat avoidance and return | Passed | Flee escaped a roaming orc encounter; party returned through the western gate. |
| Affordable recovery | Blocked on this route | Inn required 1 pp; party had 0 pp. Camp triggered the watch, consumed five minutes and granted no recovery. |
| Save, exit, reload | Passed at two checkpoints | Immediate re-saves were byte-identical; completed-event revisit did not duplicate rewards. |
| Start proposed Ohlo quest | Blocked (since resolved for #14) | Step west at Slums `(14,10)` could not enter `(13,10)`; locked-door code is unsupported by movement. Locked doors now offer Bash; see [QUESTS.md](../QUESTS.md#locked-doors). |
| Ohlo acceptance, delivery, hand-in | Not reached (since implemented for #14) | Script-inspected recommendation only; host reward handling is also known to be incomplete. The quest and its reward are now supported; see [QUESTS.md](../QUESTS.md#ohlos-potion-delivery). |
| Defeat, mid-combat retreat, larger-group balance | Not reached | No defeat occurred; retreat after combat starts remains outside current scope. |

## First fight, rewards and return

All coordinates are zero-based, with east increasing X and south increasing Y.
From New Phlan `(0,4)` West, Step enters Slums `(15,4)` West. Follow these cells,
turning before each change of direction:

```text
(15,4) → (14,4) → (14,5) → (14,6) → (14,7) → (13,7)
       → (13,6) → (13,5) → (13,4) → (13,3) → (13,2)
       → (14,2) → (14,1) → (13,1)
```

Acknowledge the paper encounter text to start combat automatically. It is
`ECL2.DAX:20`, search event 1: LOAD MONSTER at `0x9e5e` requests record 13/count 1,
then `0x9e65` requests record 4/count 3; COMBAT is at `0x9e6c`. The converted
opponents each had 12 HP and AC 13. Original identities/counts were preserved;
combat statistics are the project's authored SRD conversions. The arena was the
original 50×25 dungeon geometry.

Initiative was Cora, Arden, Orc 4, Bryn, Elin, Orc 2, Orc 1, Orc 3, Darin, Fenn.
Accept offered opportunity attacks, and use End Turn after each living PC's action.
The recorded combat sequence, using battlefield coordinates, was:

| Round | Actor | Action and observed result |
| --- | --- | --- |
| 1 | Cora | Dodge. |
| 1 | Arden | Move `(21,13)`; melee Orc 2 at `(20,13)` for 11. |
| 1 | Orc 4 | Hit Elin for 4; movement provoked Arden, who hit it for 10. |
| 1 | Bryn | Move `(21,12)`; incoming opportunity attack missed; kill Orc 2 for 6. |
| 1 | Elin | Magic Missile at Orc 1 `(19,13)` for 8. |
| 1 | Orcs 1 and 3 | Orc 1 missed Elin; Orc 3 hit her for 3, leaving 1/8 HP. |
| 1 | Darin | Cure Wounds on Elin `(25,14)`, restoring 7 HP. |
| 1 | Fenn | Magic Missile at Orc 3 `(21,11)` for 10. |
| 2 | Cora | Move `(23,12)`; kill Orc 4 `(23,11)` for 3. |
| 2 | Arden | Move `(20,12)`; melee Orc 1 `(20,11)` missed. |
| 2 | Bryn | Kill Orc 3 `(21,11)` for 6. |
| 2 | Elin | Magic Missile at Orc 1 `(20,11)` killed it; town exploration resumed automatically. |

The final spell's numeric damage was not retained in the visible log before the
scene changed; victory and resulting state were verified. Total incoming damage
was 7, all to Elin and all healed. Three Magic Missiles and one Cure Wounds spent
four slots; neither Fighter spent Second Wind. All six ended at full HP.

Each member received 300 XP. Arden received 96 silver; Elin received one
`por:unsupported:62` / `Item type 62`, unequipped. Its original provenance was
retained; no magical effect was assumed. The claim IDs were exactly:

```text
por:ECL2:20:search1:orcs:v1
por:ECL2:20:search1:orcs:v1:loot
```

At the idle post-fight boundary, `0x4ACA=255`, `0x4ABB=1`, `0x6DC8=4`, and the
pose was area/script 20, `(13,1)` West. Save before advancing, then open each
character sheet and confirm its available level-two advancement. No XP was
injected and no repeatable reward was farmed.

| Resource | After fight at level 1 | After advancement at level 2 | After inn refusal and interrupted camp |
| --- | --- | --- | --- |
| Arden / Bryn Second Wind | 2/2 each | 2/2 each | Unchanged |
| Cora level-one spell slots | 2/2 | 3/3 | Unchanged |
| Darin level-one spell slots | 1/2 | 2/3 | Unchanged |
| Elin level-one spell slots | 0/2 | 1/3 | Unchanged |
| Fenn level-one spell slots | 1/2 | 2/3 | Unchanged |

On the return route, a roaming orc menu appeared at `(13,6)` South. Flee succeeded,
placing the party at `(14,6)` West. Continue to `(15,4)` East, then Step into
New Phlan `(0,4)` East. This exercised pre-combat avoidance, not retreat from an
active battle; the roaming group was not defeated and granted no reward.

Enter the inn at town `(4,12)` South, accept its one-platinum quote, and select
Arden. Payment fails despite his 145 gp and 96 sp. Camp at the same location
triggers the watch; choose GO. Time advances from 8 minutes 33 seconds to
13 minutes 33 seconds, with no completed rest. After restart, reentering the inn
and choosing Fenn (248 gp, 0 pp) reproduces the payment rejection.

## Fresh-process save verification

The writer exited normally. A new Godot process loaded each named slot using the
actual Load Game controls and immediately saved under a new name. A read-only
helper using the existing native `decode_campaign` API independently inspected
party resources, reward claims, script flags, poses and exploration masks.

| Checkpoint | Relevant state | Immediate re-save |
| --- | --- | --- |
| `Issue13 After Orcs` | Level 1, 300 XP each, full HP, spent slots, original loot; Slums `(13,1)` West; 14 visited / 37 seen cells; time 6:21 | Identical, 52,372 bytes |
| `Issue13 After Camp` | Level 2, 300 XP each, full HP, spent slots retained; town `(4,12)` South; 53 visited / 129 seen town cells; time 13:33; no completed rest | Identical, 52,857 bytes |

SHA-256 for the original and matching re-save respectively:

```text
After Orcs: 6d86181c189d82efe18cc96d3a4dc90e4549f6a84c9542a2fe60b35dcc433e1e
After Camp: 4a7f0790179389cf399fd4182bd665904c23030221053b548a2f4fb03e546196
```

After loading the post-orc checkpoint, Explore and Look at the completed event
did not restart combat. The subsequent save still had 300 XP per member, 96
silver, one item 62, the same two claims, and unchanged completion flags and map
counts. No loss or duplication was observed. These two checkpoints provide
evidence for [#10](https://github.com/stdarg/OpenGold/issues/10); they do not finish
the broader quest restart matrix in [#15](https://github.com/stdarg/OpenGold/issues/15).

## Prioritized findings and ownership

| Priority / category | Reproduction and bounded expected outcome | Follow-up |
| --- | --- | --- |
| 1 — recovery/progression blocker | Return with the audited gold/silver, accept the inn quote, select a payer with 0 pp; rejection repeats after restart. Establish a documented, attainable payment/recovery path without silent price changes or fixture funds. | [#17](https://github.com/stdarg/OpenGold/issues/17) |
| 2 — proposed quest route blocker | Slums `(14,10)` West: Step twice and Look leave the party outside Ohlo's room. Original west edge has door code 2; `RolfTourSession::move_party` blocks codes greater than 1. Implement the required original door interaction and validate the delivery path. **Resolved for #14:** the original "Locked." menu with Bash (an SRD Strength (Athletics) check) and Exit; the booth needs two more forced doors. | [#14](https://github.com/stdarg/OpenGold/issues/14), broader ECL context [#4](https://github.com/stdarg/OpenGold/issues/4) |
| 2 — proposed quest reward blocker, source-inspected | Ohlo's reward uses nonzero non-shop TREASURE operands, which the current host rejects. Implement transactional original reward handling and the subsequent service-only COMBAT continuation. This branch was not reached in game. **Resolved for #14:** 150 pp and 1 jewelry through the loot service, no XP; the random item is reported as unsupported. See [QUESTS.md](../QUESTS.md#reward). | [#14](https://github.com/stdarg/OpenGold/issues/14), reusable mapping [#9](https://github.com/stdarg/OpenGold/issues/9) |
| 2 — quest route survival | Walking from the gate to Ohlo and the booth meets roaming groups sized by PARTY STRENGTH; surprised groups cannot be fled, and the automated six-fighter party loses (e.g. four goblin leaders and sixteen goblins at `(14,7)`). Needs an encounter-balance decision. | [#14](https://github.com/stdarg/OpenGold/issues/14), [#5](https://github.com/stdarg/OpenGold/issues/5) |
| 3 — purchase usability/conversion | Buy Scale Mail for 45 gp, then Equip fails with `por:unsupported:54`; there was no pre-purchase support warning. Make conversion support clear before spending and preserve original item provenance. | [#18](https://github.com/stdarg/OpenGold/issues/18) |
| 4 — misleading location UI | Enter or reload Slums area 20; map title, view header and idle speaker still say New Phlan. Derive district labels from the current area. | [#19](https://github.com/stdarg/OpenGold/issues/19) |
| Existing bounded conversion limit | Loot item 62 persists as an unsupported original item. Identify an explicit conversion if a later route requires its use; this audit does not invent a magic effect. | [#9](https://github.com/stdarg/OpenGold/issues/9), [expedition scope](../EXPEDITION.md) |

No crash, save-state loss or duplicate reward was observed. The normal encounter
was winnable with this party, but Elin reached 1 HP and used all her first-level
slots. That is one balance observation, not grounds to reduce original counts or
claim every ordinary party can win. Recovery evidence is limited to the tested
inn/camp route; all possible platinum sources and rest locations were not exhausted.

## Recommended next quest: Ohlo's potion delivery

Recommend Ohlo → Old Rope Guild booth → Ohlo for #14. Its acceptance, named
delivery and return reward form a smaller original quest than clearing all
Slums encounters. The four-orc paper encounter alone does not prove a complete
commission/hand-in quest. Ohlo's route was inspected in the original script;
only travel to its blocked entrance was exercised in game.

| Milestone in `ECL2.DAX:20` | Original evidence / required continuation |
| --- | --- |
| Meet Ohlo | Search event 3, entry `0x9f13`, cells `(11..13,9..10)`; SETUP MONSTER record 24 at `0x9f29`. The tested entrance is west from `(14,10)` into `(13,10)`. |
| Accept | TALK, then NICE or MEEK parley leads to the commission offer at `0xa1bc`; acceptance writes commission flag `0x4A04=250` at `0xa251`, then exits to `(14,10)` East. |
| Obtain delivery | Search event 19, `(15,12)`, entry `0xae1e`; SPEAK reaches INPUT STRING at `0xaf9f`; comparison string OHLO at `0xafa5`; matching branch writes potion flag `0x4A81=250` at `0xb048`. |
| Hand in | Return to Ohlo with the potion flag; GIVE menu at `0xa29d`. CLEAR MONSTERS at `0xa38f`, TREASURE at `0xa390`, COMBAT at `0xa3a1`, then completion writes `0x4A04=255` and `0x4A81=255` at `0xa3a2` / `0xa3a8`. |

The reward's decoded TREASURE operands are `[0,0,0,0,150,0,1,129]`. #9/#14 must
document their currency/item interpretation, recipients and XP policy before
implementation. The commission's potion is represented by its script flag; do
not mistake the four-orc loot item 62 for the quest delivery.

Smallest known dependencies are:

1. Support the required original door interaction at Ohlo's entrance and verify
   access to the Old Rope Guild booth. Static navigation also found no route to
   the booth using only currently traversable edges; its exact interaction still
   needs in-game validation. Do not bypass doors by changing the original map.
2. Validate the original dialogue/presentation requests, string answer path and
   required record/item conversions. Those branches remain unplayed; successful
   disassembly is not runtime acceptance.
3. Reuse campaign reward services for the non-shop TREASURE/COMBAT sequence,
   with party changes, claims and ECL completion flags committed together.
4. Exercise acceptance, collection, hand-in, repeat visits and fresh-process
   reload with normally earned resources. Resolve #17 for a repeatable outing
   loop; the script inspection alone does not establish that this quest requires
   a paid rest or training.

Keep full Slums clearance, unrelated monsters and unneeded class features out of
this increment. Confirm any new UI layout/control behavior before coding, as
required by [AGENTS.md](../../AGENTS.md).

## Local evidence and limits

Evidence retained in ignored `build/issue13/` includes `baseline.json`,
`party.json`, creation/equipment logs, per-action `response-*.json` UI snapshots,
`save-summary-writer.txt`, `save-summary-reader.txt`, `reload-results.log`,
`03-unsupported-scale-mail.png`, `05-first-combat.png`,
`09-inn-payment-blocked.png` and `15-ohlo-door-blocked.png`. Build/test logs are
`build/issue13-configure.log`, `build/issue13-build.log` and
`build/issue13-tests.log`. Named saves remain only in the isolated Godot profile.
The report contains the reproducible actions and measured outcomes; the local
artifacts are supplemental and are not public repository dependencies.

This is one assisted playthrough on one platform and original release. Random
creation rolls and roaming encounters can differ on a repeat. It does not test
every party composition, OS, defeat/reload path, full quest or unsupported item.
Repository changes are documentation only; no UI, rules, original content,
runtime, ownership model or save format was changed.
