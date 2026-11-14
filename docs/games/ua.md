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
past its first 398 bytes, are told under Playing a design below. The ADPCM expansion follows the
format as Creative documents it for its cards; it has not yet been compared
with a recording of the original.

## Playing a design

Phase 8 step 1 ([GAMES.md](../GAMES.md)): OpenGoldBox plays an adventure made
with the kit. Since 2026-11-14 a design loads, a party made under the kit's
rules starts at its start, the 3D view draws its walls, and its events and
fights run (`opengold_ua_session_tests`, `opengold_ua_walk`); the game
plays it from the startup menu (`opengold_godot_startup_menu_ua`).

### The seam

A design is not a DAX campaign: its areas hold the kit's event records, not
scripts, and nothing in it runs through the script machine. So it has a
session of its own, `ua::UaSession` (`ua_session.h`), beside the DAX games'
`RolfTourSession`, rather than the kit's events forced through the ECL
machine. It speaks the view's language instead:

- the same `TourSnapshot`: where the party stands, what an event says, its
  choices and ticket, the phase (waiting for Return, a choice, a fight, done
  or the party lost);
- the same exploration commands (`explore`), answers (`continue_dialogue`,
  `choose`, `input`), a fight as a `CampaignEncounter` (`pending_encounter`)
  played by the original rules' combat and handed back with
  `resolve_combat`, and the 3D view as an image (`observe_view`);
- the same party: a `CampaignParty` under the original rules, whose
  experience, money, Hit Points and clock the events change.

The kit's own words (Yes, No, "The entire party is killed!") go in the
snapshot's host dialogue and choices, which the view translates; the
designer's texts are shown as they are.

The game's exploration screen (`RolfTourView`) plays either session through
one code path. `ExplorationSession` (`exploration_session.h`) is what the
view asks of any adventure: the snapshot, the party, restart, time passing,
steps, answers, Search mode, whether the party may leave or change its
selected member, the pending fight and its result or refusal, the picture
for the view's window (`view_picture`, nothing where there is nothing to
draw yet) and the area's own name. `UaSession` implements it;
`DaxExplorationSession` wraps a `RolfTourSession` for it, so the DAX
session itself is unchanged. What only a DAX campaign has (shops and
treasure piles, its script's variables, sprites and NPC portraits, temples,
teleports and Word of Recall, close-ups, the ending film, the difficulty
and auto-equip settings) the view reaches through `dax()`, which is null for
a design, so the view leaves those out instead of drawing them wrong; an
overland area shows no picture yet. Camping (`camp`, `decline_camp`) and the
overhead map (`area_map`) the session gives itself, and a design saves its
own way (Saving and loading a game, below).

The code: `ua_design` (the files), `ua_view` (the 3D view), `ua_session`
(start, movement, fights) and `ua_events` (the event loop and its kinds), in
`src/OpenGold.Core`.

### From the startup menu

*Unlimited Adventures* is the fifth game on the startup menu's Pick game.
Its folder is the kit's (`[games] UA`, or `OPENGOLD_GOLDBOX_DIR/UA`, checked
for `CKIT.EXE` and `DISK1` to `DISK3`), asked once as the other games'
folders are. Then the design is asked each time the game is chosen, the last
one offered (`[games] UA_DESIGN`), since a player may have several: a folder
whose name ends in `.DSN` ([CONFIGURATION.md](../CONFIGURATION.md)). The kit
has no title pictures, so its menu follows at once. Play with default party
is not offered (no default party is made for a design yet); Continue and
Load offer the design's saves. The rules question offers only the
original rules, as the SRD has none of a design's monsters; Create new party
makes characters under the kit's rules
(`original::character_rules(aging, GoldBoxGame::unlimited_adventures)`), and
their fights run under the original rules with the design's Content
(`ua::load_design_content`, chosen in `game_rules.cpp` by the design's
folder). Explore plays the design from its start: its welcome, the 3D view
and the area's name over it; a fight opens the combat screen with the
design's monsters, and a fight that cannot be set up stops the adventure
with its reason (`reject_combat`). The in-game menu saves and loads a
design's game as it does any other's.

`opengold_godot_startup_menu_ua` (`tests/startup_menu_tests.gd --ua`) clicks
through it all on the test design, which `opengold_ua_session_tests` writes
to `ua-fixtures/TEST.DSN` in the build folder as a CTest fixture
(`OPENGOLD_UA_DESIGN_FIXTURE`): Unlimited Adventures on the menu, the kit's
folder found, the kit's own folder refused as a design and the test design
taken, a fighter made and taken exploring, the welcome naming her, the
area's name and the 3D view, a step and a turn, and the kobolds' fight on
the combat screen.

### A design's files

A design is a folder (`NAME.DSN`). The kit reads a numbered record from the
design's loose file first (`GEO005.DAT` for area 5, `GAME001.DAT`,
`MONST012.DAT`) and from its own library otherwise (`GEO.GLB`, whose index
gives area 5 the dungeon template); a picture library likewise from the
design's folder first. `ua::DesignFiles` reads both ways; an empty design
folder makes the kit's templates the design.

- **The game record** (`GAME`, 388 bytes): the title (32 bytes), a new
  character's experience (+20h, a long; 40,001 in the kit), platinum, gems
  and jewelry (+24h to +2Fh, longs; 200 platinum), the start area (+30h) and
  its entry point (+31h, from 1), the starting equipment (+32h, 1 to 8), then
  the key and item names and the password.
- **Areas** are numbered 1 to 63. Areas 1 to 4 are the overland maps, 5 and
  up dungeons in the 3D view (the number decides, not the header).
- **The area header**: the version and size; at +4 to +6 the three wall
  sets by slot (0 none); +7 flags (bit 0, the overhead map allowed); +8 to
  +0Bh four backdrops; +0Ch and +0Dh the combat art sets; +0Eh eight entry
  points (y, x, facing, a spare byte); +2Eh rest encounters; +4Eh step
  events; +76h the name; +106h the wandering monster and +107h the area's
  level; +110h the zone texts.
- **A square**: four wall bytes (north, east, south, west), its event (1 to
  100) and its bits (0 and 1 the backdrop, 2 to 4 the zone). A wall byte's
  low four bits are its picture (0 none; 1 to 15 the set slot (v - 1) / 5
  and the variant (v - 1) % 5 + 1); its high four bits what it does to a
  step: 0 open, 1 open and secret, 2 locked, 3 locked and secret, 4 and 5 the
  same against picking (wizard locks), 6 to 13 opened by keys 1 to 8 (quest
  flags 0 to 7), 14 blocked, 15 a false door. Out of doors any wall stops the
  party. Facings are 0, 2, 4 and 6 for north, east, south and west.
- **Monsters** (`MONST`): a 450-byte record whose first 398 bytes are a
  character's, then 16 items, 16 charges and 10 effects. The fight reads its
  THAC0 (+7Fh, stored as 60 less it), Armor Class (+B3h, the same), Hit
  Points (+81h), movement (+88h), two attacks (rates at +ABh, dice at +ADh,
  sides at +AFh, bonuses at +B1h), saving throws (+83h), levels (+9Dh), Hit
  Dice (+89h), morale (+93h), size (+82h), magic resistance (+C4h) and
  experience value (+54h, a word). Their key in the original rules'
  bestiary is `UA:MONST:n`.

### The 3D view

The view is the Gold Box games' 88 by 88 window, three squares deep, drawn
far to near with the same ten pieces at the same places: far front, far
sides, middle front and sides, near front, the party's own side walls, and a
filler between two far front walls. The kit first draws the backdrop of the
party's square (`BACK.TLB`; one numbered from 20h has a night picture after
it), then each wall from its set: 8X8DB.TLB holds sets 1 to 9 and 8X8DC.TLB
10 to 16, each 48 glyphs: its colors, the filler, two walls of nine pieces,
and three overlays (doors) drawn over the first wall. The area wraps round
at its edges for the view and for steps. `ua::render_view` draws it.

### Events

An area has 100 event records of 20 bytes; a square names one. Byte 0 is the
kind (the event editor's forms, `SCRIPT.GLB`, name them), byte 1 the
condition (bits 3 and up its kind, bit 0 once only, bits 1 and 2 whether the
chain goes on after a failed or passed test), byte 2 the condition's value
and byte 3 the event that follows. Texts are slots of the area's text pool,
from 1; the first two carets name the selected member. The conditions: a
quest flag set, clear, at 255 (done), at 80h (failed) or between; day or
night; a chance in 100; searching or not; the party's facing; a class or
race in the party.

The event loop follows the kit's: an event that passes is marked done (one
bit an event an area, kept for the whole game), runs, and its kind may
choose the next event; otherwise byte 3's follows, unless the party moved. A
step back puts the party on the square it came from. A menu or question may
push its own event to come back after the branch, while the party stays.
Moving onto a square, or Look, runs its event.

The kinds played (2026-11-14): fights (1, and 33 with one group picked at
random), text (2, up to five texts, each with Return or not), treasure (3 at
once, 25 kept for the next fight), damage (4), the three transfers (5 within
the area, 11 to another area or entry point, 34 the teleporter, asked or
not), button and list menus (10 and 21), utilities (16: quest flags set,
added, combined, or the adventure ended), sounds (17, silent for now), chain
(23), experience (26), time passing (27), a quest's stage (35), Yes or No
(36), tavern tales (37), special items (38, a quest flag) and an NPC's words
(14, heard only with that NPC in the party). Since 2026-11-14 the rest
play too (`ua_services.cpp`), their menus in the kit's words (host
messages the view translates) and their prices in platinum at one of the
kit's twenty rates (`ua::rated_price`, temple.c's `rate_price`):

- **Training hall** (6): asks whether the party trains, then who; a member
  of a class the hall's bits name (byte 8) with the experience rises a
  level in each class it can, for 1,000 platinum at byte 9's rate; a
  magic-user's level picks a spell.
- **Tavern** (7): Drink (byte 7, bit 20h: the designer's drinks menu, each
  stronger, and at 60 points of drink event 13h), Talk (its four tales and
  the tavern tales of kind 37, at random or in turn), Fight (10h: event
  12h, or no one to fight) and Leave.
- **Shop** (8): its stock from four masks of ITEM.DAT rows (shop.c's
  table), at byte 5's rate; Buy for the selected member, Sell at half the
  price (nothing for a cursed or worthless item), Identify for 20.
- **Temple** (9): the cures of its level (byte 4), from Cure Light Wounds
  (level 1, 20) to Resurrection (7, 2,000), at byte 5's rate; Donate (byte
  7, bit 8) counts toward a tithe (bytes 8 to 11) that runs event 0Ch.
- **Vault** (24): the party's money left and taken back, kept for the game.
- **Inn** (29): the party rests.
- **Small town** (22): Temple, Hall, Shop, Inn, Pub and Vault by byte 7's
  bits, each made from the town's bytes as the kit does, back to the town
  after each.
- **Guided tour** (12): from its start square or the party's, up to its
  steps of turns and steps forward (through walls, no square's event on
  the way), its two texts at their steps.
- **NPCs** (13 and 32): a MONST record joins as a member under the kit's
  rules (`imported_draft`) with its percent of Hit Points, unless with the
  party already or the party has eight; it leaves again.
- **Encounter** (15): Approach, Retreat, Fight, Wait and Talk as the
  designer set each button, the range closing to the fight, the talk or a
  flight, the monsters' speed against the party's slowest deciding surprise
  and whether the party gets away.
- **Who tries, who pays, passwords** (18 to 20): a member picked tests an
  ability or a thief's skill, pays gems, jewelry or platinum, or the party
  types the password (any case); success or failure shows its words and
  chains, teleports or steps back.

Any other kind is reported as not played yet (`diagnostics`).

A fight's event names up to six groups of a count and a monster. The fight
is played by the original rules' combat with the kit's monsters (Fights,
below). A treasure's items are ITEM.DAT's (numbered
from 1, of ITEMS.DAT's types; FFh a scroll of three magic-user spells),
identified when byte 7's bit 80h says so, each to the first member with
room. A trap's defence 1 halves its damage, rounded down, on a save.

### Fights

Since 2026-11-14 a design's fight is laid out, fielded and ended as the kit
does it (`ua_combat`, `ua_session`; the reconstruction's `cmbsetup.c`,
`evcmds.c` and `postcmbt.c`).

- **The field.** In a dungeon, the 50 by 25 squares of DUNGCOM tiles around
  the party: 13 by 5 exploration squares, each stamped with its walls, as
  the DAX games lay theirs (one code path, `kit_dungeon_battlefield`; the
  kit draws one corner piece of its own, where a wall from the north ends).
  A wall byte with a picture is a wall when it stops a step (14 and 15) and
  a doorway otherwise; past the area's edge is wall, but for the party's
  own row east and west. The set is the area header's +0Ch (0 the first).
  Out of doors (byte 8's bit 20h in a dungeon, or any overland area, by the
  set of the zone the party stands in, +108h) WILDCOM's set (+0Dh): open
  ground with trees (a crown over a trunk, or a lone tree) one time in 20
  where two squares of ground stand one above the other, then brush on 3
  in 100, grass on 14 and stones on 5. The kit's terrain byte and river
  flag are never set, so no river runs. A tile's picture is its set's
  glyph after the palette; trees, walls and stones stop a step, brush costs
  two. A design's own loose set (`DUNG1003.TLB`) takes the kit's place.
- **Colors.** The combat screen's (COMSPR's), then the field set's over
  them; the icons are drawn in these, as the kit loads none of its own
  (CPIC's colors 144 to 255 are the field sets').
- **Icons.** A monster's CPIC icon by its MONST number, its action pose 128
  above (a design's `CPIC1nnn.TLB` first); its squares from the icon's
  shape (1 one square, 2 tall, 3 wide, 4 two by two), which also make it
  large for weapons' damage, with the record's top size bit. The party's
  icons are those of its creation (DEVIATIONS.md).
- **Monsters.** Up to 50. A MONST record fights as the original rules
  recalculate a creature's record (`combatant_from_record`): its base
  THAC0, Armor Class, attacks and movement, Strength's bonuses on its blows,
  and its items (the tail at +18Eh: ITEM.DAT numbers, unidentified, with
  their charges at +19Eh from the kit's table of 0 to 100), each readied in
  turn where its hands, its places and its classes allow (a monster of no
  class readies nothing). Groups 4 to 6 fight for the party when byte 0Ah's
  bits 20h, 40h and 80h say so; a group picked at random (kind 33) is never
  an ally.
- **Where they stand.** The event's distance (byte 0Eh's bits 60h), cut
  short by a wall ahead in a dungeon. With a quarter set (byte 10h's top
  bits north, east and south, byte 12h's bit 20h west) the enemies are dealt
  to the quarters in turn from the east, each group its distance that way
  and at least a square off beside or behind the party.
- **The end.** A won fight: the monsters brought down (not those that fled)
  give their experience (+54h) and, unless byte 0Ch's bit 80h says they keep
  it, their platinum, gems and jewelry (+4Ch) and items to the treasure (a
  monster whose first item is of type 69h keeps its items); then the
  treasure's platinum (2 each), gems (250), jewelry (2,200) and magic items
  (400 a plus) count too, and the whole is shared among the members
  standing, each share as the rules give it (single classes 10% more for
  their prime scores, a cleric's from Wisdom 15; a two-class member half, a
  three-class a third). Where byte 0Ch's bit 40h says the party cannot die,
  the fallen get up with 1 Hit Point, and only a party with no one alive is
  lost.
- **Difficulty.** The kit scales a monster's Hit Points by its Level (1 to
  5) times (Level + 1) / 4; at its Normal, Level 3, they stand as recorded.
  The original rules play at Normal ([ORIGINAL-RULES.md](../ORIGINAL-RULES.md)),
  so they are never scaled.

`opengold_ua_session_tests` checks each on the test design's fights and
every monster of the kit; `opengold_godot_startup_menu_ua` sees the kobolds
on the kit's field in their icons.

### A party under the kit's rules

The kit's creation (create.c and its tables) is *Curse*'s in its classes by
race and its rolls (3d6 + 1, best of six); OpenGoldBox makes a party for a
design with `original::character_rules(aging, GoldBoxGame::unlimited_adventures)`.
Its differences from the Pool family ([ORIGINAL-RULES.md](../ORIGINAL-RULES.md)):

- No half-orcs: the races are elf, half-elf, dwarf, gnome, halfling and
  human. *Done.*
- A new character's experience and money come from the design's game record:
  40,001 experience (shared among its classes, trained at once) and 200
  platinum by default, and the gems, jewelry and set of equipment (1 to 8,
  by class group: fighters, clerics, thieves, magic-users; create.c) it
  names. *Done* (`original::design_character_rules`, chosen in
  `game_rules.cpp` by the design; `ua::outfit_new_character` when the
  character joins).
- Experience for a level: *Curse*'s numbers, carried on to level 11 for
  every class (the paladin's and ranger's eleventh 1,050,000 and 650,000),
  then a fixed amount a level to level 40 (chartab.c). *Done* (2026-11-14,
  `experience_for_level`): before it the kit's characters followed *Pool of
  Radiance*'s table, which has no paladins or rangers, so they never rose.
- A ranger learns a magic-user spell each time its ranger level rises past
  8, of a level its new ranger level casts, and knows the spells it
  learned. *Done* (2026-11-14), with the kit's bug fixed (ORIGINAL-RULES.md).
- Level caps by race: elves fighters 7 and magic-users 11; half-elves
  clerics 5 and fighters, rangers and magic-users 8; dwarves fighters 9;
  gnomes and halflings fighters 6; humans and thieves without limit. *Still
  to come.*
- THAC0: a magic-user's is 20 at levels 1 to 5 (*Curse*'s 21). Initiative is
  1d6 and the Dexterity bonus, ties broken by a d2. A roll of 17 or more that
  hits is a critical hit. Saving throws take the best of a character's
  classes. *Still to come.*
- Difficulty (five levels, Normal the third) changes the party's to-hit and
  the monsters' Hit Points; the original rules play at Normal, where
  neither changes (Fights). Experience from a fight gives single classes
  10% more. *Done* (Fights).

### The test design

No design ships with the developer's copy of the kit (no `.DSN` folder;
*Heirs to Skull Crag*, the kit's sample, is not in it), and nothing is
downloaded. So `tests/ua_session_tests.cpp` builds one from the kit's own
templates, as its design menu does: two areas from GEO.GLB's dungeon
template and the game record from GAME.GLB, written into a temporary
folder of each run's own (`opengold_ua_test.dsn`, `opengold_ua_walk.dsn`,
`opengold_ua_seam.dsn`, as the runs go at once), and for the startup check
into `ua-fixtures/TEST.DSN`.

- **Area 5, the hall** ("THE TEST HALL", the start, entry point 1 at (2,2) facing east): a
  wall down the middle with a locked door and a door for the brass key, a
  pillar of the second wall set and an open doorway; a welcome naming the
  selected member chained to the brass key (a special item), a chest asked
  about (Yes or No) holding platinum and gems, three kobolds once only, a
  fountain's menu (experience), a dart trap, the mayor's quest and a flag it
  sets, his thanks on that flag, a teleporter, a chain to an hour passing,
  stairs down asked about, and goblins or orcs picked at random; and for
  the fights, two kobolds out of doors, two kobolds with a goblin on the
  party's side two squares off (a wall a square west of them), a thug
  with his items, one who keeps them where the party cannot die, and two
  kobolds starting north and south of the party.
- **Area 6, the cellar**: words on arrival, a statue's list (damage to all,
  experience to the selected member, or nothing), stairs back to the
  hall's second entry point, rats that break a rest at its first hour, a
  draft every third step, no overhead map, and the adventure's end at
  (8,8). The hall's map is shown, and a no-magic fight waits at (1,13).
- **Area 7, the town**: a training hall, a tavern with a tale, drinks and a
  brawl, a shop, a temple taking a tithe, a vault, an inn, a small town with
  every service, a guided tour, an NPC (the kit's 70h) joining, speaking and
  leaving, strangers to approach and talk to, a stuck door for the strong,
  a toll and a password.

The game record starts a new character with 20,000 experience, 75
platinum, 2 gems, a jewel and the first set of equipment, and the chest's
treasure holds an item and a random scroll.

`opengold_ua_session_tests` plays it beat by beat. `opengold_ua_walk`
(`opengold_ua_session_tests --ua-walk`) walks every square of every area of
it with six fighters made under the kit's rules: on each it draws the view
facing each way, plays the square's event and takes every open step,
playing each event and fight; it fails on any event the session cannot play.

### What remains

- The services' smaller parts (DEVIATIONS.md, Unlimited Adventures'
  services): Pool, Share and Take, Appraise, vault items, the camp menu at
  an inn, a temple's Restoration and Fix; drinks as more than a count; the encounter's monster
  picture at its range.
- Overland areas: their big picture (steps already move the party there,
  and their rest encounters break a camp); zone texts and step events.
- Locked doors: bashing, picking and Knock.
- Fights:
  - The 14 monster effects no game's effect plays yet (Monsters' effects,
    below), the kit's monster types (+BFh) as the fight's kin for the
    effects that name them (26, 47, 105), and the differences of the near
    matches.
  - Monsters' spells (+C6h) and an item's special effect when readied.
  - The area's wandering monster (+106h): the kit fields ten of it out of
    sight, for a Monster Summoning to gate in beside its caster; the
    original rules have no summoning, so it waits for them. (The kit's other
    wandering monsters, its step events and rest encounters, play.)
  - Byte 12h's top bits (the turning level +0, 2, 10 or 41 over the
    area's) and an icon over two squares a side, which stands as two.
- The treasure menu, and items' names (the kit's words are in CKIT.EXE,
  which is not read; an item shows its type's name).
- The kit's rules' remaining differences (above).
- A default party; the kit's camp menu's own parts (Alter, Fix, the rest
  menu's chosen hours); an overland area's picture (the kit draws the
  area's big picture, `BIGPIC`, the header's first backdrop, with the
  party's marker over its square: `view3d.c`, `wild_pic_open`; `cursor.c`)
  and its zone texts, step events and maps larger than 16 by 16.

### Monsters' effects, "no magic" fights and step events

Since 2026-11-14 (`ua_session.cpp`, `combatant.cpp`):

- **Monsters' effects.** A MONST record's ten effects (+1AEh) fight as the
  kit loads them, for good at strength FFh, each as the original rules'
  effect that does what the kit's does (`original::canonical_effect`, as
  for *Curse* and *Secret*; judged from the reconstruction's `effects.c`,
  `fxcore.c` and `fxcombat.c`). The kit's monsters carry 68 kinds; 54 match
  an effect the fight plays:

  | Kit | Fight | Kit | Fight | Kit | Fight | Kit | Fight |
  |---|---|---|---|---|---|---|---|
  | 6 | 335 | 64 | 64 | 92 | 492 | 141 | 108 |
  | 8 | 8 | 65 | 465 | 93 | 112 | 143 | 111 |
  | 18 | 124 | 66 | 115 | 95 | 107 | 158 | 109 |
  | 24 | 24 | 70 | 470 | 96 | 290~ | 159 | 117~ |
  | 26 | 26 | 73 | 273 | 98 | 310 | 169 | 116 |
  | 37 | 237 | 79 | 479 | 100 | 482~ | 170 | 87~ |
  | 45 | 45 | 82 | 330~ | 101 | 85 | 171 | 82 |
  | 47 | 47 | 84 | 504 | 102 | 86 | 173 | 284 |
  | 58 | 458~ | 85 | 483 | 103 | 119~ | 174 | 282 |
  | 59 | 127 | 86 | 70 | 105 | 334 | 175 | 281 |
  | 60 | 460~ | 88 | 488~ | 108 | 303~ | 176 | 257 |
  | | | 89 | 89 | 117 | 287~ | 216 | 100 |
  | | | 91 | 88~ | 124 | 84~ | 217 | 101 |
  | | | | | 132 | 488 | 220 | 296 |
  | | | | | | | 223 | 480 |
  | | | | | | | 224 | 68~ |

  A ~ marks a near match: the gaze of 58 reaches six squares and spares no
  one the kit's 126 or 79 spare; 60 and 103 let any monster of their level
  through, armed or not; 82 slays with the banishing quarrel; 88 and 224
  paralyse on either attack and 224 for longer; 91 breathes as Pool of
  Radiance's dragons do; 96 is acid as well as breath; 100's terror takes
  no save; 108 adds 1d6 to the blow; 117's rays need their exact
  distances; 124's charm is saved at -2; 159's holy water is the fight's;
  170's rot takes no Charisma. The other 14 (40, 76, 80, 81, 90, 99, 125,
  126, 137, 168, 186, 193, 195, 219: spell immunity, disease immunity,
  half damage from weapons below +3 and +4, poison breath, the bulette's
  attacks, +2 weapons needed, sure saves against gazes, stench, damage from
  magical fire, the pudding's corrosion, no damage from weapons, the
  gorgon's breath, less fire damage) are kept 600 higher
  (`kit_effect_offset`), which no handler plays yet.
- **"No magic" fights.** Byte 0Eh's bit 80h of a fight's event bars every
  spell (evcmds.c): `CampaignEncounter::no_magic` and
  `rules::Encounter::no_magic`; the original rules' fight gives every
  fighter an effect that bars casting (600, `effect::no_magic`), so the bar
  lasts as a saved fight's effects do, and neither the player nor the AI
  is offered a spell.
- **Step events**, the kit's wandering monsters: the area header's eight
  from +4Eh (move.c, `step_events_tick`), each the steps between its runs
  (a word), the event, and the zones where steps do not count (bit n, zone
  n). Each counts the steps the party takes in its zones from its arrival
  in the area and runs when the count comes round, before the square's own
  event; the counts are saved.

`opengold_ua_session_tests` checks the skeleton's, troll's and rakshasa's
effects, the hall's no-magic kobolds and the cellar's draft every third
step; `opengold_original_combat_tests` that no one casts in a no-magic fight,
saved or not.

### The end

A design ends where its designer says: a utilities event's bit 10h
(mapevent.c, `event_utilities`), on which the kit leaves the adventure.
Since 2026-11-14 OpenGoldBox shows it as the game won, as the DAX games'
wins: the event's chain stops, "You have won." follows the designer's last
words (`TourSnapshot::game_won`), and the party explores no more. A party
put somewhere else (`place`, the kit's test mode) plays on.

`opengold_ua_route` (`opengold_ua_session_tests --ua-route`) plays the test
design from its start to that end by steps alone, as a player walks it: it
finds the way to each beat through the open steps (`route_to`), answers
every event met (Yes where asked) and fights every fight, through the hall
and its brass key's door, down the stairs at (12,12), and across the cellar
to its end at (8,8), and checks the win.

### Camping and the overhead map

Since 2026-11-14 a design's party camps and its dungeons have an overhead
map (`ua_session.cpp`; the reconstruction's `rest.c` and `camp.c`).

- **Camp** opens the Rest dialog the DAX games' camps use (DEVIATIONS.md,
  Unlimited Adventures' camp): its Start rests the party as the rules rest
  it (the original rules' day), its spells memorized in the dialog that
  follows, and Save game. As the kit's `camp_rest` does, the rest passes in
  five-minute steps, each a chance for the rest encounter of the zone the
  party stands in: the area header's four bytes a zone from +2Eh, the
  minutes between rolls (a word), the event, and the chance in 100 every
  that many minutes (80h and up, the event's own test at every step). A
  broken rest gives nothing: the time rested passes, "The party's repose is
  interrupted!", and the event runs. A chance with no minutes between rolls
  divided by zero in the kit; it never rolls (ORIGINAL-RULES.md, Bugs).
- **The overhead map** shows a dungeon whose header allows it (bit 0 of
  +7), as the DAX games' map does: a wall where a picture stops a step or
  hides a secret door, a door where one opens, and the squares the party
  has stood on; its first 16 by 16 squares.

`opengold_ua_session_tests` rests in the hall (healed, the rules' time
passed) and in the cellar, whose rats break the rest at its first hour, and
reads the hall's map; `opengold_godot_startup_menu_ua` sees the hall's map
and rests there from the Camp button.

### Saving and loading a game

Since 2026-11-14 a design's game saves and loads (phase 8 step 1). A save is
OpenGoldBox's own campaign save, in the game's own folder (`saves/UA`), not
the kit's `SavGamA..J.csv`:

- **What it keeps:** the party, as every save keeps it; the design's folder,
  in the save's header beside its rules (`CampaignRules::design_folder`), so
  loading picks the design's monsters before the party; and the session's
  own state (`ua::UaSession::saved_state`, `ua_save.cpp`): the area, the
  party's square and facing, the square a step back returns to, the quest
  flags, every area's events done, the squares seen, the step
  events' counts, Search mode, the clock
  while no party plays, the session's dice, the tithe given, the vault, the
  tavern's next tale and drink, and the tavern tales heard on this square.
  Campaign format 51 (`DesignSave`); older saves load as before.
- **When:** only while nothing waits for the player (`can_leave`), as in the
  DAX games, so no event and no fight is ever saved half-run; a party made
  and not yet exploring is saved at the design's start.
- **Loading** (`UaSession::resume`) puts the party back where it was, its
  events done staying done, and runs no event; a save whose design folder
  is gone fails with "This save's design is no longer at …", and a save of
  another design plays that design (`[games] UA_DESIGN` follows it).

`opengold_ua_session_tests` saves mid-design (the kobolds beaten, the
mayor's quest taken), refuses to save while the chest asks, loads into a new
session that plays on identically (its flags, done events, pose, clock and
the same steps meeting the same events), and refuses an older format and an
unreadable state. `opengold_godot_startup_menu_ua` saves from the in-game
menu, goes back to the startup menu, continues and plays on to the kobolds.
