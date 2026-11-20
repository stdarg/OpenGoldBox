# Buck Rogers: Matrix Cubed

The second *Buck Rogers XXVc* Gold Box game (1992), the same program as
*Countdown to Doomsday* with new data. The plan to play it to its win under
its original rules is [BUCK-ROGERS.md](../BUCK-ROGERS.md). Step B0
(groundwork) and step B1 (the reading census) are done (2026-11-19), and the
characters' and ground combat's rules of step B2 (2026-11-20); the game does not play yet, and its button in the startup menu stays disabled until it
does.

## Files and references

- Game files: `/Users/edmond/GoldBox/buck-rogers-matrix-cubed` (the folder
  `game_folder` names; the game and its tests find it by its files, whatever
  it is called: TECH.md, "Finding the games' folders").
- Rule Book, Log Book, Cluebook and Data Card:
  `/Users/edmond/GoldBox/docs/buck-rogers-matrix-cubed`.
- Reconstructed source (version 1.0, Turbo Pascal 6):
  `~/src/unsafe/buck-rogers-matrix-cubed`. It has no license; OpenGoldBox
  learns from it and writes its own code and text.

## The game's number and marker

- `GoldBoxGame::matrix_cubed` is number 11 in saves (9 is kept for *The Dark
  Queen of Krynn*, 10 is *Countdown*).
- `identify_game` knows the folder by `SHIPS.DAX` (only the Buck Rogers games
  have it) with `BUCKA.XMI`, one of its XMI songs, beside it. None of the
  earlier games' markers is in the folder.

## The census

`opengold_census` reads every file of the folder, and the game is in
`tests/game_census.cmake`'s list of complete games. `opengold_census
--scripts FOLDER` adds the script listing below.
`tests/matrix_cubed_tests.cpp` checks the music files on made-up bytes and
the real files; the formats it shares with *Countdown* are in
[countdown.md](countdown.md), "The census" and "Scripts", and checked by
`tests/countdown_tests.cpp`.

The game keeps one disk: `ECL1` (33 scripts, ids 1 to 114), `GEO1` (25 maps),
`8X8D1`, `WALLDEF1`, `BACK1`, `BIGPIC1`, `PIC1` with `PIC7` and `PIC8`,
`SPRIT1` and `CPIC1`. Its `MON0CHA` holds 63 creatures in records of exactly
259 bytes (*Countdown*'s carry four unread bytes more); creature 1 is a
level 12 Sid refuge. `ITEMS`, `ITEM0.DAX`, `SHIPS.DAX`, `CHARS.DAX`,
`COMSPR.DAX`, the four combat tile sets, `BORDERS`, `CURSOR` and `8X8D0`
are *Countdown*'s files, and read the same way. `START.EXE` builds item names
from 45 words, one more than *Countdown*, and links the same sound driver.

### Files

| Files | What they hold | Read by |
|---|---|---|
| `BUCKA.XMI`, `BUCKB.XMI`, `BUCKC.XMI` | the songs, 21 sequences in all | `decode_xmi` (as for *Unlimited Adventures*) |
| `ADLIB.ADV`, `SBFM.ADV`, `MT32MPU.ADV`, `PCSPKR.ADV`, `TANDY.ADV` | the Audio Interface Library's drivers | recognized by `ail::decode_driver`, never run |
| `FATV9.AD` | the global timbre library: 152 AdLib timbres of 14 bytes | `ail::decode_global_timbre_library` |
| `FATV9.BNK`, `FATV9.CAT` | the AdLib instrument bank (175 instruments) and the catalog the library was built from | `ail::decode_instrument_bank`, `ail::decode_timbre_catalog` |
| `MUSIC.DAX` | *Countdown*'s music driver and banks | the bank reader; the game never opens it |
| `GAME.OVR` | the program's overlay | never read |
| `SEG00036.001`, `SEG00036.002`, `JOURNAL/` | two short files of scrambled text and an empty folder | not the game's: the program never opens them |

- **Music:** each sound device has its song file: `BUCKC` for the PC speaker
  and Tandy, `BUCKA` for the Sound Blaster and AdLib, `BUCKB` for the Roland
  MT-32. The driver names the suffix of the timbre library it wants, and the
  game opens `FATV9.` with that suffix; only `FATV9.AD` is here. A song's
  timbres load one by one as the driver asks, and a missing one shuts the
  music down; every timbre the three files ask for is in `FATV9.AD`, which
  the census checks.
- **The library's sources:** `FATV9.CAT` is the text the library tool read:
  162 lines, each assigning a bank's instrument by name to a bank and patch
  (it names `fatv8.bnk` as its bank). Every instrument it names is in
  `FATV9.BNK`, and every timbre in `FATV9.AD` is one of its lines; ten of its
  lines are not in the library.
- **Drivers:** each starts with the place of its function table and John
  Miles' 1991 copyright notice; the table pairs function numbers with their
  offsets and ends with FFFFh.
- **Files that are not the game's:** the two `SEG00036` files hold 30 and
  202 bytes of shuffled letters, and `JOURNAL/` is empty. Nothing in the
  program opens them; the census lists them apart and does not count them.

## Scripts

The command table, the memory window (8000h to A400h), the operand tags and
the commands' meanings are *Countdown*'s. What the scripts ask of the engine
(`opengold_census --scripts`):

- **Scripts**: 16 starts a new game; 17 is the team's ship; 18 is
  Salvation; 19 is the solar map; 24 is the demonstration. Scripts 1 and 2
  are developer tools that nothing reaches from script 16: 1 jumps the party
  to any place and runs any skill check, ship combat or shop by number, and
  loads a script 52 the game does not have; 2 stages fights. The other 30
  scripts, all but the demonstration, are reached from 16.
- **Skill checks**: 120 (117 outside the developer scripts), of 35 skills;
  66 let the party's best member roll. Fast Talk/Convince (76), Notice (83)
  and Programming (43) are the most frequent. The amateur night (script 20)
  asks Singing, Act, Mimic or Acrobatics as the performer chooses. The hail
  of a ship on the solar map repeats *Countdown*'s skills 74, 70 and 69 for a
  bluff, a threat and a call for help; OpenGoldBox checks 76, 72 and 71
  (Bugs).
- **Ship combats**: three in script 19 (ship 3, the Mohave type, and ships
  the script picks), and one in developer script 1.
- **The solar map**: script 19 writes area kind 70h and sets picture mode 2
  as it draws the map.
  Its engine draws and moves the map as *Countdown*'s, from the same
  tables and the same zone 0 cells (the bodies from 4B85h, the planet at
  4BA5h, the rocket at 4BBEh and 4BBFh), and script 19 finds the same
  thirteen bodies, so *Countdown*'s solar map serves it unchanged
  ([countdown.md](countdown.md), "The solar map"). Its arrival menus differ
  (Earth offers Salvation, Losangelorg or Duke's Hill, and Tycho or
  Copernicus as the story goes; Ceres a RAM base; Fortuna a NEO base; Juno
  the fungus asteroid), and its pictures are its own: 101 to 104 for the
  planets and 97 for every asteroid, Pallas's included, which *Countdown*'s
  script leaves without one. The game plays it once its campaign loads
  (step B4).
- **Fuel**: as in *Countdown*, only the solar-map script spends it before
  each step. Salvation (script 18) refuels to 450, a NEO cruiser or a helpful
  ship fills a low tank to 200, one event of script 19 takes 10 (or empties
  a tank of 20 or less), and script 95 sells it a unit at a time.
- **Shops**: list 1 on the ship and at Salvation (scripts 17 and 18), lists 2
  to 8 and 10 to 16 in script 20 (list 16 where *Countdown* has 9), list 2
  in script 64 and list 4 in script 112. A shop sets its price multiplier
  in cell 7F6Dh first.
- **PROGRAM**: 0 (the team menu) in scripts 18 (Salvation), 20, 64 and 112; 3,
  which the engine ignores, only in the demonstration; 2 once.
- **The winning event**: script 114, on the mining rig at Jupiter. At its
  end Wilma Deering and NEO arrive, the team reports that the Matrix device
  works, Buck and Wilma praise the team, everyone cheers, and PROGRAM 2 ends
  the game (the original quits to DOS).

## Rules: the characters (step B2)

The same program as *Countdown*'s, so the rules are *Countdown*'s
([countdown.md](countdown.md), "Rules") with its tables run on to level 20,
in the same code (`buck_rogers_rules.cpp`, keyed by game). What differs,
checked by `tests/matrix_cubed_tests.cpp`:

- **Level 7 and the level limit 20.** A new character gets the career's
  level-7 experience (40,000 for a Rocketjock, Engineer or Rogue, 48,000 for
  a Medic, 64,000 for a Warrior) and is trained free from level 0 to 7, which
  also makes its saves two better (levels 3 and 6). The Log Book says new
  characters start at eighth level; the program makes them level 7, and
  OpenGoldBox follows the program. Training goes on to level 20; a
  *Countdown* character stopped at 8 trains on.
- **The tables to level 20.** The Rocketjock's, Engineer's and Rogue's
  experience runs to 112,640,000, the Medic's to 393,216,000 and the
  Warrior's to 256,000,000; combat value to 49, 52 for a Medic or Engineer,
  59 for a Warrior. Hit Points are rolled through level 9 and fixed from
  level 10: 3 a level for a Warrior, 2 for the rest.
- **Preset skills.** A new character starts with its career's skills set
  (nine to twelve skills: a Rocketjock with 85 Pilot Rocket and 85 Maneuver
  in Zero-G, a Medic with 75 Treat Critical Wounds, a Rogue with 60 Bypass
  Security and Open Lock, and so on). It gets no points to spend, but the
  skill screen lets it take points out of a preset skill, down to that
  skill's starting level, and put them into another skill of the same kind
  (career or general), at most 105 above its start. A character at level 7
  may know twelve general skills. So may a *Countdown* character brought in
  below level 7 once it trains to 7: it can then learn several general
  skills in that one level and catch up with a new character. OpenGoldBox
  keeps this as the original has it (the user, 2026-11-20).
- **Three specializations** for a new Warrior, then one at every even level
  from 8 to 20: ten in all, the number of slots.
- **Modify** takes a character with its career's level-7 experience.
- **Treat Light Wounds:** lowering it below 30 takes back Treat Critical
  Wounds with Treat Serious Wounds. *Countdown* left Critical's points
  stranded; OpenGoldBox does as *Matrix Cubed* in both games.

## Upkeep and money (step B2)

The engine routines are *Countdown*'s, unchanged: the wounded after a fight,
poison and antidotes, AID, rest, the credit pool, shops, ammunition, HALVE
and JOIN are as [countdown.md](countdown.md), "Upkeep and money", describes,
in the same code (`buck_rogers_upkeep.cpp`). The same choices to keep the
original hold (the user, 2026-11-20): only Treat Critical Wounds is harder
on oneself, Salvation's doctor heals gone members too, pooled credits left
behind are lost after one warning, and topping up ammunition charges a
whole clip. The scripts differ:

- **Where:** Salvation is script 17, and its port, like *Countdown*'s, is
  free. Script 20 of `ECL1` is the outpost with the bank and the hospital,
  as script 99 is in *Countdown*, and script 95 has a port that charges.
  The ship's clinic is in script 19, the solar map. The game starts with
  10 medical supplies and 20,000 credits in the NEO account.
- **The bank** works as *Countdown*'s, its TRANSFER to the NEO account
  included: both games have it.
- **Prices:** the shops set the multiplier at 16 or 32 by the shop. It
  stays until the next shop sets it, so the hospital's fee is 2,560 or
  5,120 credits by the shop visited last.
- **Fines:** a port the team has damaged asks 10,000 credits before it lets
  the ship dock, from the NEO account, or else from the bank.

## Rules: ground combat (step B2)

The same program's ground combat as *Countdown*'s ([countdown.md](countdown.md),
"Ground combat"), in the same code (`buck_rogers_combat.cpp`, keyed by game):
the fights keep *Matrix Cubed*'s identity, and its difficulty levels are
*Countdown*'s six. What its levels to 20 change is a Rogue's backstab, by
(level + 7) / 4: six times the damage at level 20. `tests/matrix_cubed_tests.cpp`
checks a fight under its key. *Countdown*'s ground-combat bugs are fixed in
both games.

## Bugs

Fixed in OpenGoldBox; the ones *Matrix Cubed* shares with *Countdown* (the
check's grade, high skill values, the general-skill count, Animal Training's
refund, the DIVVY share, credits handed over, buying into a full bundle) are
in [countdown.md](countdown.md), "Bugs".

1. **Training to level 20 cut the experience** (`chargen.pas`
   `TrainingMenu`): after training, the original cuts the experience to one
   short of the next level's, which it reads from the table's entry for
   level 21. There is none, so it read the next career's first figure: a
   Rocketjock trained to 20 was left with 1,499 experience, a Warrior with
   1,249. OpenGoldBox leaves a character at the level limit its experience.
2. **Each reroll improved the saves** (`CreateCharacterMenu`): the saves are
   set before the reroll loop and lowered again by the free training at
   levels 3 and 6 of each roll, so every reroll made all eight saves two
   better, without end. The saves now start over with each roll.
3. **The hail's skills** (script 19 of `ECL1`, the checks at 932Eh, 93B7h
   and 9422h): as in *Countdown*, the bluff, the threat and the call for help
   check 74 Befriend Animal, 70 Hypnosis and 69 Paint/Draw, two of which can
   never be learned. OpenGoldBox checks 76 Fast Talk/Convince, 72 Intimidate
   and 71 Act there, only where the shipped operand is the one expected
   (`script_check_skill`; the user, 2026-11-20). The game's files are
   unchanged.
4. **The hospital's fee check** (script 20 of `ECL1`, the checks at 853Bh
   and 8555h): as in *Countdown*, the check reads one byte of the two-byte
   fee, which is 0 at the multipliers of 16 and 32, so the hospital never
   refused and charged the bank account, then the patient, into debt.
   OpenGoldBox reads the word there (`script_operand_tag`). The game's files
   are unchanged.
5. **A Rogue was worse from behind** (`cbtacts.pas` `RollAttacks`): as in
   *Countdown*, a Rogue striking from behind needed a number 2 higher, so it
   hit from behind less often than anyone. The number is 2 lower now (the
   user, 2026-11-20; [countdown.md](countdown.md), Bugs).

## Ship combat

The fight and the repairs after it are *Countdown*'s: the same tables, the
same experience by enemy ship and the same rules, which
[countdown.md](countdown.md) ("Ship combat") describes with its bugs and
the user's decisions. The three ship-combat options (COMMAND every round,
Empty helm dodges, Fuel from a boarded ship) are set for *Matrix Cubed* apart
from *Countdown*, under its own folder name
([CONFIGURATION.md](../CONFIGURATION.md)). OpenGoldBox plays both games with
one session, keyed by game (`buck_rogers_ship_combat.h`). After a boarding the solar-map script 19 loads
script 21, where *Countdown*'s loads 52. The program differs in two places:

- The ship's explosion deals each okay member its maximum Hit Points and 30,
  so it always kills them; *Countdown* deals 5d20 and the maximum, which
  can leave one alive in a coma. OpenGoldBox kills every member aboard in
  both games (countdown.md, Bugs).
- A ship's Armor Class is read as signed in the to-hit number; *Countdown*
  reads it as unsigned. Every ship in `SHIPS.DAX` has a positive Armor
  Class, so the two agree.

### Bugs

| Bug in the original | In OpenGoldBox |
|---|---|
| After a boarding, the repairs take fuel from the enemy ship, which no longer exists, so the amount is whatever the memory holds (`RepairShip`; *Countdown* checks and takes none) | The captured ship's fuel, as the Rule Book says, from the wreck the fight's end saved; with Fuel from a boarded ship off, none, as *Countdown* (countdown.md, decision 5) |
| The ship-combat bugs of *Countdown* but the explosion's coma | Fixed as there |

## Next

Step B4, *Matrix Cubed* played through its route ([BUCK-ROGERS.md](../BUCK-ROGERS.md)).
