# Unlimited Adventures: its files

*Forgotten Realms: Unlimited Adventures* (1993) is not a campaign but a
construction kit, `CKIT.EXE`, that both builds adventures and plays them. SSI
rewrote it in C, and its files share nothing with the DAX games' archives. This
page records its file kinds as the census (`opengold_census`,
[GAMES.md](../GAMES.md)) reads them. The facts come from the reconstructed
source of release 1.2 and from the files themselves; the wording is our own.

Since 2026-11-13 every data file decodes (`opengold_ua_census_tests`).

## Where the files are

The game's folder holds the programs; the data sits in four folders, which the
census reads with their names (`DISK2/PICA.TLB`):

| Folder | What it holds |
|---|---|
| `DISK1` | The always-loaded art, the title, the font, item types and items, the speaker sounds, the sound effects and the music |
| `DISK2` | Pictures, sprites, combat icons, battlefield tiles, wall tiles and the monsters |
| `DISK3` | Screen frames and menus, and the kit's design data: areas, labels, game settings and the event editor's forms |
| `ART` | Templates for drawing new art in a paint program, as LBM and PCX |

These are the templates a new design starts from. A design made with the kit
saves its own copies of them (and loose `.DAT` and `.TLB` files) in its
`NAME.DSN` folder, which phase 8 will read.

Never read: `CKIT.EXE`, `INSTALL.EXE`, `SOUND.EXE`, the batch files, the
sound drivers (`*.ADV`), `INSTR.AD` (the AdLib driver's timbres), `CKIT.CFG`
(the player's settings) and `UAHELP.TXT`, the designer's printed supplement,
which `README.BAT` only shows.

## Resource libraries

Every `.TLB` and `.GLB` file is a resource library. Its 16-byte header holds
the tag `HLIB`, the library's size, the count of items, a packing method, an
index flag and a four-letter signature; a table of item offsets follows, one
more than the items, so item *n* runs from offset *n* to offset *n* + 1.

- The signature says what the items are: `TILE` glyphs, `DATA` records,
  `DIG4` speaker sounds, or `HLIB` for a library whose items are libraries.
- With the index flag set, item 0 is an index: a count, then pairs of the
  number the game asks for and the item that holds it. `PICA.TLB`'s index,
  for example, maps picture numbers to its nested libraries.
- The program can unpack five packing methods (PackBits, two Huffman tables,
  LZW and an LZ code). No shipped library is packed, so OpenGoldBox reads
  only unpacked ones and refuses the rest.
- A library's size may be odd; the file or item holding it then has one more
  byte.

## Glyphs

A `TILE` item is a glyph: an 8-byte header and its bytes. The header gives
the rows, how far up and left of the place it is drawn the glyph starts, the
bytes of one plane's row, and a layout in its low four bits.

The kit draws in the VGA's unchained 256-color mode, 320 by 200, where four
planes each hold every fourth pixel: pixel *x* of a row is in plane *x* mod
4. A glyph with *b* bytes a plane row is 4*b* pixels wide. The layouts the
shipped files use:

| Layout | Count | What follows the header |
|---|---|---|
| 0 | 349 | Each plane's rows in turn |
| 1 | 13 | Four mask planes, then four image planes; mask 0FFh keeps the screen's pixel, 0 takes the image's |
| 2 | 184 | Each row PackBits-packed on its own, unpacking to plane 0's bytes, then planes 1, 2 and 3 |
| 5 | 1,170 | As 0, with 0FFh see-through |
| 7 | 174 | Each plane's rows as signed counts ended by 0: a positive count copies that many bytes, a negative one skips see-through ones |
| 8 | 264 | A palette (below) |
| 9 | 9 | A composite: parts of six bytes (the glyph, the count of parts, a row and a column) |

The program also knows a single-plane bit (80h), packed rows for other video
modes (40h), and layouts 3 and 10; no shipped glyph uses them, and the
decoder refuses them. A composite's parts must be images of the same library.

A palette glyph sets a run of the 256 colors. Bit 0 of its rows says the
header names the first entry (in the place of the row) and the count (the
column); without it, all 256 are set. Bit 1 says color cycles follow the
colors, as many as the row bytes: four bytes each, a direction flag, a rate,
a first entry and a count. Levels are stored 0 to 255 and the program hands
the VGA their top six bits. The decoded glyphs use the same image and color
types as the 256-color DAX games' pictures (`vga_picture.h`).

The census groups the `.TLB` files by what they show:

| Row | Files |
|---|---|
| Pictures | `TITLE`, `BIGPIC`, `BIGPIX`, `PICA`–`PICF`, `BACK` |
| Sprites | `SPRIT`: near, middle and far views |
| Combat icons | `CPIC`, `COMSPR`, `CBODY`, `DUNGCOM`, `WILDCOM` |
| Wall tiles | `8X8DB`, `8X8DC`: 48 wall pieces a set |
| Screen parts | `ALWAYS`, `FRAME`, `GEN`, `MENU`, `TOPVIEW` |

## Records

| File | What it holds | Layout |
|---|---|---|
| `GAME.FON` | The font | 96 characters (space to DEL) of 12 rows of 8 pixels; the program draws 8 of the rows. A longer file adds 32 control characters |
| `ITEMS.DAT` | 128 item types | The 16-byte type of the DAX games, with no header (as in *Pools of Darkness*) |
| `ITEM.DAT` | 254 items, numbered from 1 | 18 bytes: type, three name words, weight, value, bonuses, readied, hidden name words, cursed, count, three effects |
| `MONST.GLB` | 127 monsters, indexed | A 450-byte record PackBits-packed behind its big-endian size; its first 398 bytes are a party member's |
| `GEO.GLB` | 3 area templates, indexed by area number | IFF chunks (below) |
| `STRG.GLB` | 5 label tables | 14-byte records: the first holds the count; each other a number, the kinds it names and 12 bytes of six-bit text |
| `GAME.GLB` | The game's settings | 388 bytes: the title, the names of 8 keys and 12 special items (16 bytes each from 34h), the password at 174h |
| `SCRIPT.GLB` | The event editor's 58 forms | Below |
| `SOUNDS.GLB` | 13 speaker sounds (`DIG4`) | A big-endian rate and byte count, then 4-bit samples two to a byte, the low nibble first |

**Six-bit text** packs four characters into three bytes, the first in the
top bits. Codes 1 to 31 are the letters (40h added), 32 to 63 space, digits
and punctuation as in ASCII, and 0 ends the text.

**An area** is an IFF `FORM` of type `AMOD` (the kit writes a size after
`AMOD` too) with four chunks of fixed sizes, their sizes big-endian:
`HDR ` (290 bytes: a big-endian version, the rows and columns, the wall sets
and the rest of the area's settings), `MAP ` (576 squares of six bytes: a
wall byte for each direction, an event and the square's bits, row by row),
`ENCR` (2,000 bytes of event records) and `STRG` (the area's text pool: a
6-byte header of capacity and bytes used, 400 length bytes, and the six-bit
texts one after another).

**An event editor's form** starts with a length byte and the form's title.
Pages follow until a zero byte: each a kind byte, its size and number, then
fields until the byte 2 that ends the page. A field is a kind, a size, and
either its place in text cells and its text (kinds below 20h, the text
length counting its own byte) or a setting byte (20h, 21h; 22h has none);
the rest are operations that tie the field to bytes of the event record.
The forms describe every event's record layout, which phase 8's player
needs; the census checks their structure, not their operations. Numbers no
event kind uses are empty items.

## Pictures, sounds and music in common formats

- **`ART/*.LBM`**: IFF ILBM pictures, 320 by 200 in 8 interleaved bit
  planes, ByteRun1-packed, with a 256-color `CMAP`.
- **`ART/*.PCX`**: PCX version 5, one 8-bit plane, run-length coded (a byte
  of C0h or more repeats the next byte its low six bits times), with the 256
  colors after a 0Ch at the end.
- **`SFXDQ.VOC`**: a Creative Voice file of 13 effects. The game plays
  effect *n* from marker *n*; each marker is followed by one block of
  Creative's 4-bit ADPCM at 5,555 to 11,111 samples a second. A block's first
  byte is the starting level; each nibble then moves the level by its low
  three bits, shifted by a scale that grows after large steps and shrinks
  after zero ones, in the direction of its top bit. On a card without
  digitized sound the game plays `SOUNDS.GLB` on the speaker instead.
- **`*.XMI`**: Miles' extended MIDI, one set of three files for each music
  driver (`ADDQ` AdLib, `RODQ` Roland, `TYDQ` Tandy, `PCDQ` speaker). Files 1
  and 3 hold one song; file 2 holds six. A file is a `FORM XDIR` whose `INFO`
  counts the sequences, then a `CAT XMID` of `FORM XMID`s, each with `TIMB`
  (the timbres to load) and `EVNT`. Events have no running status; a byte
  below 80h is a pause in 120ths of a second, and a note-on carries its
  length instead of a later note-off.

## What the census does not do

Decoding is not playing. What the area bits, the event records and the form
operations mean, and how the monster record differs from a party member's
past its first 398 bytes, belong to phase 8. The ADPCM expansion follows the
format as Creative documents it for its cards; it has not yet been compared
with a recording of the original.
