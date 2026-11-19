# Buck Rogers: Matrix Cubed

The second *Buck Rogers XXVc* Gold Box game (1992), the same program as
*Countdown to Doomsday* with new data. The plan to play it to its win under
its original rules is [BUCK-ROGERS.md](../BUCK-ROGERS.md). Step B0
(groundwork) is done (2026-11-19); the game does not play yet, and its button
in the startup menu stays disabled until it does.

## Files and references

- Game files: `/Users/edmond/GoldBox/buck-rogers-matrix-cubed` (the folder
  `game_folder` names; tests read `OPENGOLD_GOLDBOX_DIR/<folder>`).
- Rule Book, Log Book, Cluebook and Data Card:
  `/Users/edmond/GoldBox/docs/buck-rogers-matrix-cubed`.
- Reconstructed source (version 1.0, Turbo Pascal 6):
  `~/src/unsafe/buck-rogers-matrix-cubed`. It has no license; OpenGoldBox
  learns from it and writes its own code and text.

## What OpenGoldBox knows so far

- `GoldBoxGame::matrix_cubed` is number 11 in saves (9 is kept for *The Dark
  Queen of Krynn*, 10 is *Countdown*).
- `identify_game` knows the folder by `SHIPS.DAX` (only the Buck Rogers games
  have it) with `BUCKA.XMI`, one of its XMI songs, beside it. None of the
  earlier games' markers is in the folder.
- Its files carry disk number 0 for the creatures (`MON0CHA`, `MON0ITM`,
  `MON0SPC`). `ITEM0.DAX` holds item templates, not item lists, and there are
  no `HEAD` or `BODY` files.
- The census (`opengold_census`) reads its maps (`GEO`, 25), creature effects
  (`MON0SPC`, 35) and `MUSIC.DAX` (9) with the existing readers. Every other
  kind of file waits for step B1, as do every script, the XMI songs, the
  `FATV9` timbres and the AIL drivers.

## Next

Step B1: a reading census of every file ([BUCK-ROGERS.md](../BUCK-ROGERS.md),
B1).
