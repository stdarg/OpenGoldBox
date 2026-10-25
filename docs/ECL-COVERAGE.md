# ECL command coverage

Recorded 2026-10-24 for issue #4. [SCRIPTS.md](SCRIPTS.md#5-track-closure-and-complete-session-persistence)
asks for three milestones per engine command, tracked separately:

- **Implemented:** the game's handler, and the variants it still refuses.
- **Synthetic:** a hand-written script runs the command through the game's
  handler and checks its effect. "VM only" means `ecl_tests.cpp` checks just the
  request and reply contract with a hand-made reply.
- **Original event:** an installed-data test (`OPENGOLD_GAME_DIR`) runs the
  original script through the command and checks its effect against the area's
  [audit](audits/). "Runs, unasserted" means an original route passes through it
  without a fault (every route's `settle()` rejects unsupported events) but no
  check isolates its effect.

No command in this table has been compared with the running DOS game; the
[DOS probe](../dev_tools/docs/dos-probe.md#findings) has so far compared the
script machine's own semantics (GETTABLE's flags, RANDOM's bounds, IF's skips)
and found OpenGoldBox matches on every original script. The original-event
column is checked against the original bytecode, the cluebook and the audits,
not against recorded play of the original. The user accepted this evidence on
2026-10-24 for closing issues #3 and #4: an original-event check counts as
verified without recorded play. Records from the running original are added as
the [DOS probe](../dev_tools/docs/dos-probe.md) makes them.

Handlers are in [phlan_session.cpp](../src/OpenGold.Core/src/phlan_session.cpp)
unless named otherwise. Test files: `ecl` [ecl_tests.cpp](../tests/ecl_tests.cpp),
`party` [party_tests.cpp](../tests/party_tests.cpp), `tour`
[tour_tests.cpp](../tests/tour_tests.cpp), `exp`
[expedition_tests.cpp](../tests/expedition_tests.cpp), `rules`
[rules_tests.cpp](../tests/rules_tests.cpp), `save` [save_tests.cpp](../tests/save_tests.cpp).

| Op | Command | Implemented | Synthetic | Original event | Still refused |
| --- | --- | --- | --- | --- | --- |
| 0A | LOAD CHARACTER | Selects a member; stores land on it | `party` `script_handoff`; `exp` `script_payment_tests` | Inn payment (`exp` `rest_at_inn`); Dirten dismissed (`exp` `temple_of_bane`) | A store to another member; unsupported HP changes |
| 0B | LOAD MONSTER | Converted creature groups | `party` `monster_picture_before_combat`, `rejected_combat_handoff` | Four orcs (`exp` `defeat_four_orcs`, `rules`); set encounters (`exp` `slums_set_encounters`) | Creatures without a conversion; invalid icons |
| 0C | SETUP MONSTER | Distance and sprite ([rolf_tour.cpp](../src/OpenGold.Core/src/rolf_tour.cpp) for the tour) | `tour` `synthetic`; `party` `monster_picture_before_combat` | Rolf's tour (`tour` `installed`); Norris approaching (`exp` `kutos_well_catacombs`) | Other tour setups, distances and sprites |
| 0D | APPROACH | rolf_tour.cpp | `tour` `synthetic` | Norris (`exp` `kutos_well_catacombs`); Rolf's tour | — |
| 0E | PICTURE | Tour (255) and town pictures | VM only | Picture 16 rolls the plaza group (`exp` `podol_mission`) | Other tour and town pictures |
| 1C | CLEAR MONSTERS | Ends the encounter's creatures | — | Runs, unasserted (Slums orcs, Ohlo) | — |
| 1D | PARTY STRENGTH | `ecl_party_host.cpp` | `party` `script_handoff`; VM `ecl` | Podol's group sizes (`exp` `podol_group_rolls`) | — |
| 1E | CHECK PARTY | `ecl_party_host.cpp` | `party` `script_handoff` (with rollback) | Slums parley Run (`exp` `slums_parley_outcomes`) | Other thief skills, effects and attributes |
| 1F | (undefined) | Faults by design | `ecl` `por_operations` | — | Always |
| 20 | NEW ECL | Script, previous script, bank `0x6E12` | `save` fog saves; VM `ecl` | West gate round trip (`exp` `defeat_four_orcs`); refused exits (`exp` `refused_exit`) | Areas not in the game yet |
| 21 | LOAD FILES | Map, wall art and map memory per area | `save`; `party` `rejected_combat_handoff` | West gate round trip; Kuto's Well (`exp` `enter_kutos_well`) | Resources outside the supported areas; invalid facing |
| 22 | PARTY SURPRISE | Writes 0, 0 (converted parties have no surprise modifiers) | — | — | — |
| 23 | SURPRISE | Stores the bits COMBAT reads | VM only | — | Unsupported surprise values, at COMBAT |
| 24 | COMBAT | Monsters, temples, shops, treasure | `tour` `shopping_tests`; `party` `recovery_hosts`; `exp` `script_treasure_tests` | Orcs; shops; Ilmater's temple (`exp` `podol_temple`) | Other town combat routes |
| 27 | TREASURE | Coins and item lists | `tour`; `exp` `script_treasure_tests`, `deferred_treasure_tests` | Ohlo's reward; the original shop list; holy symbols | Unsupported town lists |
| 28 | ROB | Form 1: a share of the coins (items kept by the user's choice) | `exp` (the refused form) | Norris's surrender; the Pitt's bribe and drinking (`exp` `podol_pitt`) | Every other form |
| 29 | ENCOUNTER MENU | Slums encounter menu | — | `exp` `slums_encounter_menu` | Other contexts |
| 2D | CALL 0x2C90, 0xC018 | Redraw (rolf_tour.cpp) | `tour` `synthetic` (0x2C90) | Rolf's tour redraws | Invalid poses |
| 2D | CALL 0xC01E | Scripted walk (rolf_tour.cpp) | — | Runs, unasserted | Invalid poses |
| 2D | CALL 0x8001 | Duel flag | — | The Pitt's duel (`exp` `podol_pitt`) | A duel without a chosen member |
| 2D | CALL 0xBA03 | Footstep sound (rolf_tour.cpp) | — | Rolf's tour footsteps | Other sound selectors |
| 2D | CALL 0x8000 | **Not implemented** | `tour` `synthetic` expects the fault | — | Always |
| 2E | DAMAGE | Volley, chosen, random or whole party | — (rules level in `kutos_well_tests.cpp`) | Arrow volley; Cadorna's well fall (`exp` `cadorna_textile_house`) | Other forms |
| 31 | SPRITE OFF | rolf_tour.cpp | `tour` `synthetic` | Runs, unasserted (Rolf's tour) | — |
| 32 | FIND ITEM | `ecl_party_host.cpp` | `party` `script_handoff`; VM `ecl` | — | — |
| 34 | ECL CLOCK | Time passes for its steps (campaign only, [nomad camp](audits/nomad-camp.md)) | VM faults without the host | Runs, unasserted (nomad escort) | Without a campaign party |
| 36 | ADD NPC | Converted companions | `party` `script_handoff` | Dirten, Skullcrusher, the mad fighter | NPCs without a conversion |
| 37 | LOAD PIECES | Checks the district's wall banks | — | Runs at every area entry | Unverified wall banks |
| 38 | PROGRAM 0 | Training hall | `party` `training_hall_service`; `exp` `hall_event_tests` | — | Halls without a known class word |
| 38 | PROGRAM 9 | Inn and camp | `party` `recovery_hosts`; `campaign_rest_tests.cpp` | Inn (`exp` `rest_at_inn`); nomad camp | A party not eligible for a Long Rest |
| 38 | PROGRAM 8 | **Not implemented** | `exp` `unsupported_program_tests` | — | Always |
| 39 | WHO | Chooses the acting member | `party` `script_handoff`, `shop_buyer_switch` | Inn payer; the Pitt's champion | No member |
| 3A | DELAY | rolf_tour.cpp | `tour` `synthetic` | Runs, unasserted | — |
| 3B | SPELL | Knock (31) | VM only | Kovel's cabinet (`exp` `kovel_mansion`) | Every other spell |
| 3C | PROTECTION | Rune lines shown translated (SOKAL-1) | — | Sokal Keep's scroll; the Pyramid's runes | — |

## Gaps

- **Not implemented:** CALL 0x8000, PROGRAM 8 and opcode 0x1F. Each stops the
  event with an explicit error, and the event's checkpoint undoes it.
- **No synthetic test:** PICTURE, CLEAR MONSTERS, PARTY SURPRISE, ENCOUNTER
  MENU, CALL 0xC01E, 0x8001 and 0xBA03, DAMAGE, LOAD PIECES, PROTECTION.
- **Table-driven random encounter:** Podol Plaza's roaming group table is
  checked roll by roll ([audit](audits/podol-plaza.md#status)).
- **No original event checks the effect:** CLEAR MONSTERS, PARTY SURPRISE,
  SURPRISE, CALL 0xC01E, SPRITE OFF, FIND ITEM, ECL CLOCK, LOAD
  PIECES, PROGRAM 0, DELAY.
- **Not compared with the running original:** every command; see
  [the source audit](script-source-audit.md#compatibility-questions-and-deliberate-limits)
  for the questions only a DOS trace can settle. This is the accepted limit of
  the evidence (above), not an open requirement.
