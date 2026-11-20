# Buck Rogers: Matrix Cubed

The second *Buck Rogers XXVc* Gold Box game (1992), the same program as
*Countdown to Doomsday* with new data. The plan to play it to its win under
its original rules is [BUCK-ROGERS.md](../BUCK-ROGERS.md). Step B0
(groundwork) and step B1 (the reading census) are done (2026-11-19); the game
does not play yet, and its button in the startup menu stays disabled until it
does.

## Files and references

- Game files: `/Users/edmond/GoldBox/buck-rogers-matrix-cubed` (the folder
  `game_folder` names; tests read `OPENGOLD_GOLDBOX_DIR/<folder>`).
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
  bluff, a threat and a call for help (BUCK-ROGERS.md, open question 2).
- **Ship combats**: three in script 19 (ship 3, the Mohave type, and ships
  the script picks), and one in developer script 1.
- **The solar map**: script 19 writes area kind 70h and sets picture mode 2
  as it draws the map.
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

## Next

Step B2: the Buck Rogers rules ([BUCK-ROGERS.md](../BUCK-ROGERS.md), B2).
