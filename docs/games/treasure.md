# Treasures of the Savage Frontier: how its files differ

*Treasures of the Savage Frontier* (1992) continues *Gateway to the Savage
Frontier* on the 256-color engine of *Pools of Darkness*. This page records
what its files change, as the census (`opengold_census`,
[GAMES.md](../GAMES.md)) reads them. The facts come from the reconstructed
source of its DOS release and from the files themselves; the wording is our
own.

## Pictures and graphics

Since 2026-11-13 every picture, icon, sprite, tile set and wall decodes, and so
do the tables and texts kept among them. The pictures use the two layouts of
*Pools of Darkness*, the still pictures' and the animations', with the same
colors ([darkness.md](darkness.md)); the same code reads both games. What
differs:

| Files | What they hold | Change from *Pools of Darkness* |
|---|---|---|
| `PIC1`, `PIC1A` | 72 and 67 pictures | `PIC1A` continues `PIC1`: the game looks in `PIC1`, then `PIC1A`. Six records of each only set colors (below) |
| `OVER` | The overland maps | New file (below) |
| `BORD` | The screen frames, the font, two name tables | New file (below) |
| `LUCK` | Texts | New file (below) |
| `8X8D1`, `WALLDEF1` | 12 wall sets | Three wall sets at once, each with its own tiles (below) |
| `8X8D0` | The automap's symbols (202) and a font (201) | The game reads its font from `BORD` instead; 201 stays unused |
| `TITLE` | Six title pictures | Three more |

**The four bytes after a picture's EGA colors** hold a color cycle when the
first of them is 1: the other three are the first color that turns, how many
turn, and the delay between steps. The overland maps use it; *Pools of
Darkness* ignores these bytes.

**Records that only set colors.** `PIC1` records 38, 47, 48, 57, 68 and 70 and
`PIC1A` records 78, 81, 89, 90, 100 and 119 are still pictures of no rows and
no cells that carry colors 32 to 255. When the picture shown is the one a field
of the game's state names, the game also loads the record the next field names,
only for its colors.

**`OVER`, the overland.** Records 51 to 57 are 128x128 maps of the Savage
Frontier, colors 32 to 255, each with a color cycle. Records 1 to 4 again only
set colors: the game loads record n + 1 for overland map n to set its colors.

**`BORD`, the screen frames.** Its records:

| Record | What it holds |
|---|---|
| 0 | 64 8x8 frame tiles |
| 1 | A 320x192 backdrop |
| 3, 4 | The picture frame's top and bottom, two images of 144x24: 3 for most screens, 4 on the overland |
| 5, 6 | The picture frame's sides, two images of 24x88, likewise |
| 7 | 19 runs of frame tiles, each ended by FEh; FFh leaves a cell alone. The game draws each frame of the screen from one run, starting at a fixed place in the record |
| 8 | The font, a picture of 64 8x8 glyphs |
| 9 | 160 effect names, each a length byte and 37 letters |
| 10 | *Gateway*'s 255 item words, each a length byte and 20 letters, by which a *Gateway* character's items are renamed as they come across (its own 181 are in `GAME.EXE`) |

**`LUCK`, texts.** Each record is a text ended by the end-of-file mark 1Ah:
records 0 to 14 are the news a luck reading tells, 51 to 61 the words on lucky
papers, 101 to 117 the scenes of the romances between party members. Besides
letters a text holds line ends, 12 to wait for a key, and in the romances 2 to
8 for the names and pronouns of the people involved.

**Walls.** Three wall sets can be loaded at once, in three slots. Each slot has
a tile set of its own, `8X8D1` record n for `WALLDEF1` record n, and a symbol
of slot k means tile k x 256 + symbol in the game's space of symbols. A wall
set's symbols therefore still name its own record's frames, as in *Pools of
Darkness*.

## Scripts, creatures and items

Since 2026-11-13 the scripts (`ECL1`, 30), creatures (`MON1CHA`, 102), their
items (`MON1ITM`, 46 lists), the item definitions (`ITEM0`) and the item types
(`ITEMS`) decode. Their layouts are *Pools of Darkness*' ([darkness.md](darkness.md)),
with the differences below; OpenGoldBox reads both games with
`late_records.h`.

### Scripts

The commands are *Pools of Darkness*' with three more, and a script may be
longer: it loads at 8000h and may run to B6B0h (the original cuts a longer
one there). A new game starts with script 16.

| Code | Command | Operands |
|---|---|---|
| 42h | LOAD WALL SETS | 4: a map (below 7Fh, indoors only) and the wall sets for the three slots; 0 or FFh empties a slot |
| 43h | FIND NAMED ITEM | 4: an item type (0 for any) and its three name words, the third first (a third word of 0 matches any name); the equal flag says whether a party member carries one |
| 44h | TAKE NAMED ITEM | 4: as FIND NAMED ITEM, and takes the first such item from the party |

As in *Pools of Darkness*, CLEAR AND EXIT (23h) ends a script, and the census
decodes the scripts without running them.

### Creatures

The record is *Pools of Darkness*' 510 bytes, except that a character knows
126 spells, at 179–304, and so has no attack level. Every other field is at
the same place. The orc, creature 12, has 1 Hit Die, THAC0 18, AC 6 and 4 hit
points.

### Items and item types

As in *Pools of Darkness*: 17-byte items, 254 definitions in `ITEM0`, and 128
item types in `ITEMS` with no header.

### Script memory

A script names the game's cells anew. The game's state is cells 1 to 400h,
cell n holding the state's byte n − 1, and the current character's record is
cells 401h to 800h, its byte n at 401h + n; the script runs at 8000h to
B6B0h. A cell outside these reads 0. Some cells the engine serves itself:

| Cell | What it holds |
|---|---|
| 6 to 0Bh | The clock: minutes, tens of minutes, hour, day, month, year |
| 11h | The party's facing, 0 to 3 (written, 4 and up count less 4) |
| 13h | The map loaded last |
| 15h | The monsters defeated |
| 16h | The script the area returns to |
| 20h | The party's size |
| 22h | 1 inside, 0 on an area map with a cursor, which no script of this game uses |
| 2Bh, 2Ch | Rest steps between interruption rolls, and an interruption's chance |
| 2Dh | The event of the party's square |
| 30h | The classes a hall trains (127 every class) |
| 32h | The wall ahead |
| 34h, 35h | The party's square, x and y |
| 40h | 1 while the overland is shown |
| 42h | The overland's colors: the OVER record after this number |
| 54Ch, 54Eh, 550h | The character's platinum, gems and jewelry |
| 5EEh | The character's status |
| 5FDh | The character's hit points |

`game_address` gives these cells for the engine's own (Pool of Radiance's).

### What the commands do

CALL takes a routine's number: 1 redraws the view, 2 steps forward and 3 shows
the portrait animation's next frame. CHECK PARTY reads its operands and does
nothing. PICTURE 254 blanks the picture's box, 255 shows the view again; a
picture from 240 up is a big picture (`BIGPIC1`), below a `PIC1` or `PIC1A`
animation. YES NO sets the equal flag on Yes. JOURNAL ENTRY tells the party
"This is recorded as journal entry n." and waits for a key. LOAD FILES loads
its wall set into the first slot and empties the others, unless the set is
there already; LOAD WALL SETS fills the three slots, each set copying its
appearances into its slot and the ones after it. TREASURE adds its platinum,
gems and jewelry and its items (`ITEM0`'s definitions) to the pile the next
COMBAT with no monsters gives.

## The opening

A new game runs script 16, its load entry only. Over pictures 2, 3 and 4 it
tells of a lazy afternoon on a hilltop above Yartar, the party's waking in a
stone fortress, its old friend Amanitas's word and Milzorr the dwarven leader's
welcome, each screen waiting for a key (WAIT KEY), and ends with CLEAR AND
EXIT. Its entry then loads map 16 with the wall sets 9, 10 and 11 (LOAD WALL
SETS) and leaves the party at (10,7) facing north.

`load_campaign(folder, GoldBoxGame::treasures_of_the_savage_frontier)` plays it
with the code that plays *Pools of Darkness*, its engine's (`vga_engine`):
the same loader (`campaign_session.cpp`), script machine, memory map
(`game_address`), character record (`add_vga_record`), pictures, sprites,
combat icons, walls and backdrops in the colors each sets (`vga_picture.h`,
`load_vga_wall_art`), and script host (`vga_host.cpp`). What *Treasures* adds
there: LOAD WALL SETS fills three slots, each set copying its appearances into
its slot and the ones after it, as its LoadWallSet does (Pools of Darkness
loads one set, into the first slot); `PIC1A` continues `PIC1`; its 181 item words
are in `GAME.EXE`; its stores are listed differently (The walk); the
overland, FIND and TAKE NAMED ITEM, and CHECK PARTY doing nothing.

## The overland

The overland is script 51. It sets the overland cell (40h) and loads the map
its cell 2EEh names, one of `GEO1`'s maps 51 to 57, and the "wall set" of the
same number, which on the overland is `OVER`'s picture of that map: 128 by 128
pixels, a square of 8 for each of the map's. It puts the party where cells
2EBh to 2EDh say (facing, x and y). The view shows the 11 by 11 squares
around the party, kept on the map, with the party's arrow (the automap's
symbols 16 to 19), in the colors of the `OVER` record after the cell 42h's
number (its DrawOverMap and LoadOverPalette). A step there passes as inside,
by the map's walls and doors.

## The walk

`opengold_treasure_walk` (`opengold_gold_box_games_tests --treasure-walk`)
walks every square of every map, the overland's too, with six fighters made
under *Treasures*' rules, and runs each event and fight. Entering an overland
map directly, it sets the cells 2EBh to 2EEh as the way there would. What the
walk needed besides the opening's:

- **Shops.** STORE opens one of 16 stores whose stock `GAME.EXE` lists: runs
  of item definitions (`ITEM0`, from 1), each ended by FFh, at a fixed place
  in the unpacked program of this release. A number outside 1 to 16 opens the
  first; item 0 sells nothing, so store 12, the gem trader's, sells nothing.
- **Off the map.** An overland script may put the party off the map for a
  moment (stepping off an edge it did not step over), where the original finds
  no wall and no event; the view keeps the square it showed.
- **FIND NAMED ITEM** answers with the comparison flags; **TAKE NAMED ITEM**
  takes the first matching item from the party.
- The demonstration (script 4, which the original runs when the title is left
  alone) loads the opening's map and the overland's too; the game's own
  scripts hold those maps.

Still to come: the records that only set colors for the picture shown, the overland's color
cycling, the music and sounds other than the footstep, PROGRAM 3 to 5
(the luck readings, the Fix menu, the party's repair), the romances, and the
route.

One of the walk's fights no side can finish (map 24 at 1,5): a caster with no
spells left and no way to reach the party stands across the battlefield from
members with no way to reach it. The walk leaves it as a player would, each
member walking to the field's edge and fleeing, as it leaves *Curse*'s and
*Pools of Darkness*'s. In Freezefire's lair (map 38 at 3,6) the black-robed
mages charm dwarves who fight beside the party onto their side; such an ally
ends the fight as one out of it, as an ally the party attacked does.

## The ending

*Treasures* has no ending of its own. In Freezefire's lair (script 38, Ice
Peak's fourth stage) the party fights the black-robed men and the dragon
Freezefire; as the dragon dies the script sets cell 25Ch to 1, tells of the
dwarves carrying the party to Icewolf, the feast, Luskan's withdrawal and
Waterdeep's honors, and leaves the party on the overland outside Waterdeep
(map 54), where the game goes on. OpenGoldBox wins the game as that event
ends: "You have won." follows the script's closing words (`game_won`) until
the player goes on ([DEVIATIONS.md](../DEVIATIONS.md)).
