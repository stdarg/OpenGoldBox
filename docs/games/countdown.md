# Buck Rogers: Countdown to Doomsday

The first *Buck Rogers XXVc* Gold Box game (1990). The plan to play it to its
win under its original rules is [BUCK-ROGERS.md](../BUCK-ROGERS.md). Step B0
(groundwork) is done (2026-11-19); the game does not play yet, and its button
in the startup menu stays disabled until it does.

## Files and references

- Game files: `/Users/edmond/GoldBox/buck-rogers-countdown-to-doomsday` (the
  folder `game_folder` names; tests read `OPENGOLD_GOLDBOX_DIR/<folder>`).
- Rule Book, Log Book, Cluebook and the Amiga Data Card:
  `/Users/edmond/GoldBox/docs/buck-rogers-countdown-to-doomsday`.
- Reconstructed source (version 1.3, Turbo Pascal 5.5):
  `~/src/unsafe/buck-rogers-countdown-to-doomsday`. It has no license;
  OpenGoldBox learns from it and writes its own code and text.

## What OpenGoldBox knows so far

- `GoldBoxGame::countdown_to_doomsday` is number 10 in saves. Number 9 is
  kept for *The Dark Queen of Krynn*, which another session adds after
  *Death Knights*; `game_numbered` finds a game by its number, so the gap is
  safe.
- `identify_game` knows the folder by `SHIPS.DAX` (only the Buck Rogers games
  have it) with `ECL6.DAX` beside it. `ECL6.DAX` alone marks nothing, since
  *Pool*, *Curse* and *Gateway* have one too. None of the earlier games'
  markers is in the folder.
- Its files carry disk number 0 for the creatures (`MON0CHA`, `MON0ITM`,
  `MON0SPC`). `ITEM0.DAX` holds item templates, not item lists, and there are
  no `HEAD` or `BODY` files.
- The census (`opengold_census`) reads its maps (`GEO`, 17), creature effects
  (`MON0SPC`, 25) and music (`MUSIC`, 9) with the existing readers. Every
  other kind of file waits for step B1, as does every script.

## Next

Step B1: a reading census of every file ([BUCK-ROGERS.md](../BUCK-ROGERS.md),
B1).
