# Gateway to the Savage Frontier: how its files differ

*Gateway to the Savage Frontier* (1991) runs on the engine *Secret of the
Silver Blades* grew, rebuilt for VGA. This page records what its files change,
as the census (`opengold_census`, [GAMES.md](../GAMES.md)) reads them. The
facts come from the reconstructed source of the DOS release 1.2 and from the
files themselves; the wording is our own.

Since 2026-11-13 every file decodes, the music in `MUSIC.DAX` too (below).

## The files

| Files | What they hold | Change from *Secret* |
|---|---|---|
| `GEO2`–`GEO6` | 30 maps | None |
| `ECL2`–`ECL6` | 26 scripts | Two commands take more operands; new engine routines (below) |
| `MON2CHA`–`MON6CHA` | 72 creatures | Back to *Curse*'s record of 422 bytes (below) |
| `MON*ITM`, `ITEM2`–`ITEM6` | Creatures' items, item lists | None: 63 bytes an item, the name bytes scratch |
| `MON*SPC` | Creatures' effects | None: 9 bytes an effect |
| `ITEMS.1` | 128 item types | 24 bytes a type, not 16 (below) |
| `HEAD`, `BODY`, `PIC`–`PIC6`, `BIGPIC`, `BIGPIC1`–`5`, `TITLE`, `SKY` | Pictures | A color header and 32 colors (below) |
| `CHEAD`, `CBODY`, `CPIC2`–`CPIC6`, `COMSPR` | Combat icons | The pictures' format (below) |
| `DUNGCOM`, `WILDCOM`, `RANDCOM` | Battlefield tiles | None |
| `8X8D`, `8X8D1`–`8X8D6`, `WALLDEF`, `WALLDEF2`–`WALLDEF5` | Wall tiles, symbols and walls | Files without a disk number; large symbol sets (below) |
| `8X8D1` | The font (201), the symbols of set 4 (202) and the shared tiles (203) | As in *Curse* |
| `ZOUNDS` | Nine sound effects | New file: digitized sounds (below) |
| `MUSIC` | The music: a driver program (0) and a bank of instruments and a song (1) | New file (below) |

There are no `SPRIT` files. The names of items and spells are in the
program, not in a `START.EXE`: `GAME.EXE`, packed with EXEPACK as the earlier
`START.EXE` files are, holds the 255 item words in slots of 21 bytes
(`decode_item_words`). The census does not read them.

## Pictures

Every picture starts as before: its height in rows, its width in columns of
8 pixels, its origin and its number of frames, nine bytes. Then comes a color
header. When its first byte has none of the bits CCh set, it is the older
header: eight bytes of a CGA color map, with the pixels packed two to a byte,
as in the earlier games. Otherwise that byte is a set of flags, each adding a
part:

| Flag | Part |
|---|---|
| 01h | A CGA color map: 8 bytes, two bits a color |
| 02h | An EGA color map: 16 bytes, four bits a color, the even color in the low nibble |
| 04h | The pixels are five planes, not packed nibbles |
| 08h | The picture's own colors: 24 bytes |

With five planes, each row of a frame holds five runs of one bit a pixel,
each run as long as the row is wide in bytes; the first run is a color's
lowest bit. A pixel is then one of 32 colors. The EGA mode shows each through
the EGA map. The VGA mode shows colors 0 to 15 as the EGA colors, 16 and 17 as
two fixed oranges, and 18 to 31 from the picture's own colors: 4-bit levels of
red, green and blue, the low nibbles of the 24 bytes for colors 16 to 23 and
the high nibbles for 24 to 31. `gateway::picture_image` draws the VGA mode's
colors.

A picture with no rows or columns holds only its colors: `COMSPR` 254 and 255
give the colors of the exploring view's and the combat screen's frames.

`PIC` records stay animations: a frame count, then for each frame its delay
(four bytes), the nine-byte header and color header, and one frame of pixels.
Later frames hold their difference from the first, color by color. A later
frame whose header is only the flag 04h takes the first frame's colors. The
game keeps at most six frames.

The combat icons (`CPIC`, `COMSPR`, `CHEAD`, `CBODY`) are pictures of this
format; an icon's second frame is the record 128 above its first.

## Walls

Wall tiles and symbols are pictures of 8 by 8 frames in the new format.
`8X8D.DAX` and `WALLDEF.DAX` have no disk number: the original first looks for
a record there, then in the file named with the current disk's number. Each
`WALLDEF` record is paired with the tiles of the same number, as before.

A wall set's tiles number 70, or 210 for a large set. The walls of a set of 70
name symbols in the shared space, shifted 70 for each slot past the first, as
in *Secret*. A large set keeps its own numbers: its walls name the 45 shared
symbols as 1 to 45 and its own 210 tiles as 46 to 255, in whatever slot it is
loaded. `8X8D` 15 and 30, `8X8D2` 31 and 36, `8X8D3` 36 and `8X8D4` 31 are
large. `8X8D6` holds 13 sets of 70 with no walls in any `WALLDEF`: on the
overland the game loads a symbol set alone.

`decode_wall_tiles` reads these tiles in the colors the EGA mode shows. The
census pads each set to the three slots' symbol space, as for *Secret*.

## Creatures

The creature record is *Curse*'s again: 422 bytes, with the same fields at the
same places ([curse.md](curse.md)); byte 331 holds the creature's type flags. `decode_character(bytes, game)` reads it with
*Curse*'s decoder.

## Items

`ITEMS.1` holds two bytes, then 128 item types of 24 bytes: the 16 of the
earlier games' `ITEMS`, then 8 the game never reads. The first 16 bytes match
*Curse*'s for all but six types (9, 28, 67, 73, 88 and 89).

## Scripts

Scripts load and run as in *Secret*: at 8000h, to 9DFFh. A new game starts
with script 1 on disk 2 (`ECL2.DAX`); the demonstration runs script 82. The
commands are *Secret*'s, but two take more operands:

| Code | Command | Operands |
|---|---|---|
| 32h | FIND ITEM | 4: the item type (0 for any), then its three name words from the last |
| 40h | DESTROY ITEMS | 4: the same; it removes the first matching item in the party |

CALL keeps *Curse*'s routines (less 7FFFh: AE11h redraws the view, 1 and 2
the duels, 3201h the sound or music, 401Fh the step forward, E804h the next
portrait frame). 401Bh rereads the wall and event of the party's cell, where
*Curse* had 4019h. Three are new: 880Ah waits for Enter, 880Dh asks Yes or
No and sets the comparison, and 8810h changes the current character's coins
up into larger ones.

## The opening

A new game runs script 1 of disk 2 (`ECL2.DAX`), its load entry only, as the
original's game loop does. It loads map 1 of `GEO2.DAX`, Yartar, with the wall
sets 36, 31 and 15, all three large: 36 and 31 from `WALLDEF2` and `8X8D2`, 15
from the files without a disk number. Over picture 5 (`PIC2`), the tavern, it
tells the night of celebration after the caravan from Citadel Adbar, naming
two members of the party, then the morning's loss of everything to Yartar's
thieves, and leaves the party in the inn's room at (1,10) facing north.
Between its screens it waits for Enter (CALL 880Ah).

`load_campaign(folder, GoldBoxGame::gateway_to_the_savage_frontier)` plays it
with the loader of *Curse* and *Secret* (`campaign_session.cpp`). What Gateway
changes there:

- Script numbers are unique across the disks, as in *Curse*; a script's disk
  is the `ECLn.DAX` that holds it. Scripts played out of any map use the
  files of disk 6, the overland's.
- A record of `8X8D`, `WALLDEF`, `PIC`, `BIGPIC`, `HEAD` and `BODY` is looked
  for first in the file without a disk number, then in the disk's, as the
  original's LoadDax does: `PIC` 73 is `PIC.DAX`'s on every disk, though
  `PIC5.DAX` has one too.
- Pictures and animations are drawn in the VGA mode's colors
  (`gateway::picture_image`). Wall tiles keep the 16 colors of the EGA mode
  (`decode_wall_tiles`); the large sets' own colors match them.
- `load_wall_art` draws a large set from the shared symbols 1 to 45 and its
  own 210 tiles as 46 to 255, unshifted, in whatever slot it is loaded; a set
  of 70 draws from the view's one space of symbols, which holds the first 70
  tiles of each slot, as before.
- The item types are read from `ITEMS.1`, the item words from `GAME.EXE`, the
  creatures from `MON2CHA` to `MON6CHA` as *Curse*'s records. There are no
  sprite files.
- CALL 880Ah (wait for Enter) and 880Dh (Yes or No) are menus the script
  machine asks the player: Enter clears the text, and 880Dh stores Yes as 0
  and No as 1 in 7F79h and compares the answer with 1.

- CALL 8810h (the address 080Fh), which the inns and halls call before a
  payment (ECL2:1 0x9AC0), changes the current character's coins up into
  larger ones, as its Money.ConvertMoney(3) does: ten copper a silver, ten
  silver an electrum, two electrum a gold and five gold a platinum. The party
  keeps one purse, so the purse is changed up (`change_coins_up`), worth what
  it was.

## What the route needed

The route ([GAMES.md](../GAMES.md), phase 5 step 4) needed more of the script
host, each as Gateway's engine has it:

- PROGRAM 12h brings every member to full Hit Points and compares equal when
  it healed anyone (its HealPartyForProgram), as Krevish's last wish does in
  Ascore (ECL6:28 0x8904).
- LOAD CHARACTER with bit 7 makes the member at its position current and,
  when the script has cleared the name and status cells, removes that member
  (its EclLoadCharacter): Erek leaves with Amanitas (ECL3:5 0x90F9), though
  the record the script cleared is another member's.
- The record a script reads shows the Dexterity (byte 17h), which the Kraken
  cell's grating rolls a climb against (ECL2:2 0x8798), as well as *Curse*'s
  Intelligence and gender.
- SPELL asks for any memorized spell: the squid tank's water ruins each fire
  spell, one a question, until none is left (ECL2:2 0x9263).
- A companion whose record marks two items readied in one place (Brinshaar,
  ECL3:9) wears the first; the other stays packed.
- Many events look at the party's facing, which the scripts read from 03B2h:
  Gundbarg's ship sails only for a party facing down its pier (south), and
  Silverymoon's forge opens to one facing east.

## The ending

Script 28 of disk 6, which Ascore's last fight leads to, tells of the
statuettes set in the altar and Vaalgamon's fall, then calls PROGRAM 8, the
original's WinGame: six screens of four lines over BIGPIC 126 to 131. Their
words are read from the player's `GAME.OVR` (`read_ending`, at 2CFA8h of the
release of 347,622 bytes; another release yields none), each screen's
"(More)" left out and a word broken at a line's end joined whole, and shown
with "You have won." as a win (`game_won`) until the player goes on, as
*Curse*'s is. The view then plays `endings/GATEWAY.ogv` when it is there
([ENDING-FILMS.md](../ENDING-FILMS.md)). The script goes on to Yartar (NEW ECL
1), as the original's does. `opengold_gateway_tests` plays script 28 to the
win.

## Area names

The exploration screen names each map as the cluebook's sections do. The
overland's six maps are named by their region: 21, 22 and 23 by the towns the
route enters from them, 24 to 26 by where the overland script's edges lead
(24 east of 23, 25 and 26 south of the first row), which the route has not
crossed yet. Ascore's final battle (maps 27 to 30) is Ascore.

| Map | Name | Map | Name |
|---|---|---|---|
| 1 | Yartar | 16 | Llorkh |
| 2 | Kraken Complex in Sub-Yartar | 17 | Arena Beneath Llorkh |
| 3 | Nesme | 18 | Star Mounts |
| 4 | Silverymoon | 19 | Sundabar |
| 5 | Everlund | 20, 27 to 30 | Ascore |
| 6 | Neverwinter | 21 | The Sword Coast |
| 7 | Port Llast and the Gallant Prince | 22 | The Dessarin Valley |
| 8 | Luskan | 23 | The Silver Marches |
| 9 | Hosttower of the Arcane | 24 | The Eastern Wilds |
| 10 | Uttersea, Tuern | 25 | The High Forest |
| 11 | Gundarlun | 26 | The Delimbiyr Vale |
| 12 | Purple Rocks | | |
| 13 | Kraken Headquarters | | |
| 14 | Secomber | | |
| 15 | Loudwater | | |

## The overland

The overland is script 21 of disk 6. It clears the in-dungeon cell (4BE6h)
and loads the map its cell 4C24h names, one of maps 21 to 26 of `GEO6`; the
script that leads out sets it (Yartar's sets 22), and the script itself
changes it as the party walks off an edge. No script names these maps in a
LOAD FILES, so the loader gives every map no script names to the scripts of
its disk that load maps by cells. Its LOAD PIECES names the symbol sets 18,
twice the map and 8, and twice the map and 9 (`8X8D6`'s sets of 70); outside
a dungeon the original loads them alone, with no walls (view3d's
LoadWallSet). Each square's north-and-east wall byte is the number of a
symbol of the three sets, and the view shows the 11 by 11 squares around the
party from above with its arrow (`8X8D1`'s set 202, by facing), as its
DrawOverlandView does (`render_symbol_map`).

What else the overland changes, as the original has it:

- Every new script starts in a dungeon (its EclInit sets the cell to 1, as
  NEW ECL and entering a map do here) and clears 4C00h to 4C1Fh and 7F79h to
  7F82h.
- The scripts read the barrier ahead in 4CF6h: the side's barrier where it
  has a wall or on the overland, else open (1).
- On the overland a step passes only an open way (barrier 1), with a wall or
  without; monsters stand 2 squares off, and Parley replaces Advance.
- Fights there are on grass (its BuildWildernessArena): a river with fords
  by chance on rough ground, trees one or two squares tall, and scattered
  features, by the class bits of the ground (cell 4CA1h, which nothing sets,
  so class 0). The terrains (each one's movement cost and `WILDCOM` tile) and
  the 32 grounds' classes are read from the unpacked `GAME.EXE`
  (`gateway_wilderness`); movement costs above 1 are open ground, as in
  *Curse*'s fields.

## The walk

`opengold_gateway_walk` (`opengold_gold_box_games_tests --gateway-walk`)
walks every square of every map, the overland's too, with six fighters made
under Gateway's rules and runs each event and fight. Entering a map directly,
it sets cell 4C24h as the way there would: the overland map entered, else
Yartar's (22). What the walk needed:

- Combat icons are pictures of Gateway's format, drawn as its LoadIconAnim
  draws them: the VGA mode's colors, black transparent and dark gray shown
  black (`gateway::decode_combat_icon`).
- The overland and its fights, above.

## Sound effects

`ZOUNDS.DAX` holds digitized sounds: unsigned 8-bit samples that a resident
driver (`SBLASTER.EXE`, `IBMSND.EXE`, `TANSLTL.EXE` or `ADLIB.EXE`) plays. The
game loads effects 2, 4, 5 and 7 to 12, each into a buffer of 9000 bytes, and
plays 3 and 6 as 9. The rates are the program's, in samples a second:

| Effect | 2 | 4 | 5 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|
| Rate | 4000 | 7075 | 9000 | 8200 | 8000 | 8101 | 8107 | 7546 | 7075 |

`gateway::decode_sound_sample` reads them.

## The title

The full introduction shows `TITLE.DAX`'s pictures 1 to 5, each alone on a
cleared screen, for 3, 3, 10, 20 and 20 seconds; a key passes the picture
shown, except that a key on the first ends the whole title. The title music
(`MUSIC.DAX`'s song) starts with the second picture, and the last stays until
the song ends when the music is on (OpenGoldBox keeps it its 20 seconds).
`load_title_screens` draws them with `gateway::picture_image`.

## The music

`MUSIC.DAX` holds the music driver (record 0) and the bank it plays (record 1),
a library of instruments and one song. It is the driver of *Secret*'s title
music, in a later build that divides the song's tempo by 84 to time its notes
(the song's tempo 2D00h gives 137 units a tick; its tracks last 3973 ticks).
The layout is in [later-sound.md](later-sound.md); `decode_music_bank`
reads the bank.
