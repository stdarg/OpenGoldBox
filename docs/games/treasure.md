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
| 10 | 255 item words, each a length byte and 20 letters |

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
