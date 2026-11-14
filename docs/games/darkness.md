# Pools of Darkness: how its files differ

*Pools of Darkness* (1991) is the last *Pool of Radiance* game and the first
of the Gold Box games to draw in 256 colors. This page records what its files
change, as the census (`opengold_census`, [GAMES.md](../GAMES.md)) reads them.
The facts come from the reconstructed source of the DOS release 1.1 and from
the files themselves; the wording is our own.

## Pictures and graphics

Since 2026-11-13 every picture, icon, sprite, tile set and wall decodes
(`src/OpenGold.Formats/src/vga_picture.cpp`), and so do the opening and closing
films (below). The music and sound effects are laid out as in the other later
games ([later-sound.md](later-sound.md)). *Treasures of the Savage Frontier* keeps the same
layouts ([treasure.md](treasure.md)), and the same code reads both games.

### Colors

A pixel is one byte: an index into the VGA's table of 256 colors, each color
red, green and blue from 0 to 63. A picture carries only the run of the table
it needs, so what is on screen shares the table out:

| Entries | Set by |
|---|---|
| 0–255 | A title picture (all of them) |
| 0–15 | The cursor and the automap's symbols (`8X8D0` 202) |
| 16–31 | The border tiles (`BORDERS`) |
| 32–63 | The combat ground tiles (`DUNGCOM`, `WILDCOM`) |
| 32–143 | A wall set's tiles (`8X8D1`) |
| 64–95 | The party's combat icons (`COMSPR`) |
| 96–143 | Combat heads and bodies (`CHEAD`, `CBODY`) |
| 144–175 | A sky or a backdrop (`SKYGRND`, `BACK1`) |
| 144–255 | A monster's combat icon (`CPIC1`) |
| 32–255 | A picture (`PIC1`, `BIGPIC1`); a sprite carries the same run, but the game sets only 176–255 from it |

Our decoders keep each picture's indices and its run of colors as they are
(`VgaPicture`, `VgaAnimation`); turning them into screen colors comes later.

### Still pictures

`TITLE`, `BIGPIC1`, `BACK1`, `SKYGRND`, `BORDERS`, `CURSOR`, `8X8D0`,
`8X8D1`, `CHEAD`, `CBODY`, `CPIC1`, `COMSPR`, `DUNGCOM` and `WILDCOM` share one
record layout, the one the game's picture loader reads:

| Offset | Bytes | Field |
|---|---|---|
| 0 | 1 | Height in rows |
| 1 | 1 | Width in 8-pixel cells |
| 2, 4 | 2 each | A position, 0 in every record |
| 6 | 2 | Number of frames, all the same size |
| 8 | 1 | First color set |
| 9 | 1 | Number of colors set, less one |
| 10 | 3 a color | The colors |
| then | half the colors | An EGA color for each, two to a byte, for EGA cards |
| then | 4 | Not used by the VGA picture; 32 bytes when the colors are 112 from entry 32 (the wall tiles) |
| then | frames x height x width | The pixels, frame by frame, row by row |

The frames of one record are, for instance, a wall set's 256 8x8 tiles (`8X8D1`
record n for wall set n), the automap's 20 symbols (`8X8D0` 202), the 11 border
tiles, or the 26 or 40 24x24 ground tiles of a combat map. A monster's combat
icon is two records, n and n + 128, one frame each, from 24x24 to 72x48. The
game draws each kind through a see-through color it names when loading:
8 for combat icons, heads and bodies, 13 for tiles, borders and the cursor;
titles and big pictures are drawn whole.

The font is still `8X8D0` record 201: 177 glyphs of eight bytes, the EGA
games' layout.

### Animations: PIC and SPRIT

`PIC1` (49 pictures) and `SPRIT1` (29 monsters seen in the 3-D view) have their
own layout:

| Offset | Bytes | Field |
|---|---|---|
| 0, 2 | 2 each | Height in rows, width in 8-pixel cells |
| 4, 6 | 2 each | A position, 0 in every record |
| 8 | 1 | Number of the last frame (at most 8) |
| 9 | 1 | First color set |
| 10 | 1 | Number of colors set |
| 11 | 3 a color | The colors, then their EGA colors and 4 bytes, as in a still picture |
| then | 1 | The key frame |
| then | 1 | The frame the animation goes back to after the last |
| then | 3 a frame | Each frame's packed size (2 bytes) and its delay (1 byte) |
| then | | The frames, packed |

Every frame is 88x88. A frame is packed in its own way, not the archives': a
byte up to 7Fh copies that many bytes plus one; a larger byte n repeats the
next byte 257 - n times (the archives repeat 256 - n). The key frame is stored
whole; every other frame stores its difference from the key frame, which the
game undoes with an exclusive or. A delay is in tenths of a second, and 99
stops the animation on that frame.

A sprite's three frames are the monster near, at middle distance and far; the
far one is the key frame. After unpacking, the game swaps colors 0 and 13 in
them and draws through 13, so a sprite's color 0 is see-through.

### Walls

`WALLDEF1` keeps the earlier games' layout: 780 bytes a wall set, five
appearances of 156 symbols each for the ten views. What changed is where the
symbols point. The EGA games drew every wall from one space of shared tiles
and three slots of 70; here a wall set has a tile set of its own, `8X8D1` record
n for `WALLDEF1` record n, and symbol s is that set's frame s. The game never
draws symbol 0. Every symbol of the six wall sets names one of their 256 tiles.

## The opening and ending films

*Pools of Darkness* plays its opening (`INTRO`) and its ending (`FINAL`) with a
separate program, `VACE.EXE` ("Animation Control Engine", SSI 1991).
`CONTROL.EXE` runs it before `GAME.EXE` with `INTRO` and again after a win with
`FINAL`, passing the video mode from `POOL4.CFG`, `1` (keep the video mode) and
whether sound is on. VACE loads a video driver (`VGA.DRV` for 256 colors) that
draws the frames, and plays the music through `SOUND.COM`.

Each film is two files: a control file (`.ACF`) that sets colors, paces the
film and starts the music, and the frames (`.BMA`). The reconstructed source
of the game does not include VACE; what follows was worked out from the files,
`VACE.EXE` and `VGA.DRV`. Our decoder is `decode_vace_film`
(`src/OpenGold.Formats/include/opengold/vace_film.h`); the census reads both
films whole (the `FILM` row).

| Film  | Frames | Blocks | Plays for    | Song          |
|-------|-------:|-------:|--------------|---------------|
| INTRO | 133    | 14     | about 82 s   | `P1.SMS`, 0   |
| FINAL | 58     | 6      | about 33 s   | `P3.SMS`, 0   |

Every frame is the whole 320 x 200 screen, one byte a pixel. INTRO opens with
SSI's title card and tells the story in seven more scenes; FINAL has three
scenes and no words on screen.

### The control file

A list of commands. Each begins with a byte giving the command's length in
bytes (itself included, always even) and a byte giving its number; the rest
are 16-bit little-endian words. VACE reads them in order:

| Number | Words | What it does |
|-------:|------:|--------------|
| 0 | pace | Sets the pace: from now on each frame waits 261 / pace ticks of the timer (62 / pace with sound off; see below). The films use 1 (still pictures and pauses), 16 and 18. |
| 1 | — | End of the film. |
| 2 | color, red; green, blue | Sets one color: the high byte of the first word is the color's number, its low byte red; the second word's high byte is green, its low byte blue, each 0 to 63. VACE keeps all 256 colors in a table. |
| 3 | — | Waits until the frame on screen has had its time, then draws the next frame of the `.BMA` over the screen. The very first frame does not wait. |
| 5 | block, frames, size | Lists a block of the `.BMA`: its number (0, 1, ...), its frame count and the low 16 bits of its size. VACE loads the blocks by this list and skips the command while playing; the list follows the last frame. |
| 8 | song; name (3 words) | Loads the music: the low byte of the first word is the song's number, the next three words its file name (each word's high byte, then its low byte; spaces end it), to which VACE adds `.SMS`. |
| 9 | — | Starts the song (with sound on). |
| 10 | — | Stops the song. |
| 11 | — | Waits until the frame on screen has had its time, then clears the screen to color 0. The next color command first sends VACE's whole color table again. |
| 12 | — | Does nothing. |
| 13 | — | From now on a key press ends the film. |
| 14 | — | From now on keys are ignored (the default). |

Numbers 4 to 7 and above 14 are skipped.

A film is a run of scenes, each the same shape: the scene's colors (a command
2 for each), a first frame held at pace 1 (about 3.6 seconds), the moving
frames at pace 16 or 18, then pace 1 again and an 11 that holds the last
frame as long and clears the screen. The frames run on from
one block of the `.BMA` to the next without any command naming them: the
`n`th 3 of the file draws the `n`th frame of the `.BMA`, so a film has as many
3s as frames. INTRO turns on key skipping (13) after the title card; FINAL
never does.

#### The pace

VACE counts the ticks of the PC's timer and holds a frame until
`ticks per frame` have passed since it was drawn, the value in force at the
next 3 or 11. With sound on, `SOUND.COM` sets the timer to 1,193,182 / 16,384
(about 72.8) ticks a second and VACE waits 261 / pace of them. With sound off
the timer runs at the PC's usual 18.2 a second and VACE waits 62 / pace. Both
make a frame at pace 1 last about 3.5 seconds; at pace 16 a frame lasts
16 ticks (219 ms) with sound and 3 ticks (165 ms) without. The decoder gives
each frame's time with sound on: 3583 ms at pace 1, 219 ms at pace 16 and
192 ms at pace 18 (14 ticks).

### The frames file

The `.BMA` is a string of blocks, the units VACE loads while the frame on
screen waits. Each block:

| Offset | Size | Field |
|-------:|-----:|-------|
| 0 | 4 | The block's size in bytes, this header included |
| 4 | 2 | Frame count |
| 6 | 4 x count | Where each frame starts, from the block's start |

The next block follows at the block's size. The low 16 bits of the size match
the control file's list (command 5), and so do the frame counts.

#### A frame

Every frame starts with its width and height (2 bytes each, always 320 and
200). Then come the rows it changes, top to bottom:

- a byte: the row's number, or `0xFF` for the end of the frame;
- one or more pieces of the row, each:
  - 2 bytes: the first column; the top bit (`0x8000`) marks the row's last piece;
  - a byte: how many pixels the piece draws (at most 255, so a full row takes
    two pieces, for example 241 and 79 pixels);
  - a byte: how many bytes of runs follow;
  - the runs.

A run starts with a code byte. Its count is `code / 2 + 1` pixels (1 to 128).
When the code is odd, the next byte is a color to repeat count times; when it
is even, the next count bytes are the pixels themselves.

What a frame leaves out keeps the screen's pixels: the first frame of each
scene draws all 64,000 pixels (the title card all but 2,877, which keep color
0, black), the frames after it only what moves. A frame with no rows (5 bytes: the size and
`0xFF`) changes nothing, which the films use to hold a picture.

### How it was checked

All 191 frames were drawn and looked at: the title card, Phlan and the story's
scenes in INTRO, and the ending's scenes in FINAL come out whole, in their
colors. `tests/vace_film_tests.cpp` checks the frame counts, pixels and colors
of chosen frames, that INTRO's third frame changes exactly one pixel of the
second, the frames' times, and that broken files are refused.

## Scripts, creatures and items

Since 2026-11-13 the scripts (`ECL1`, 56), creatures (`MON1CHA`, 122), their
items (`MON1ITM`, 42 lists), the item definitions (`ITEM0`) and the item types
(`ITEMS`) decode. The game has one file of each kind: no disk numbers, so a
record's number alone names it. *Treasures of the Savage Frontier* keeps all
of these layouts ([treasure.md](treasure.md)); OpenGoldBox reads both with
`late_records.h`.

### Scripts

Scripts load as in *Secret*: at 8000h, up to 9DFFh, behind two header bytes
and five entry jumps, with the same operand tags and packed text. A new game
starts with script 16. The command numbers stay, but nine commands are
replaced and four take other operands:

| Code | Command | Operands | In *Secret* |
|---|---|---|---|
| 0Ch | SETUP MONSTER | 4: sprite, greatest distance, picture, directions the fight may come from | 3 |
| 1Dh | WAIT KEY | none: waits for the player to go on | PARTY STRENGTH |
| 1Fh | JOURNAL ENTRY | 1: tells the party which journal entry to read | unused |
| 21h | LOAD FILES | 2: a map and one wall set (FFh keeps the current set) | 3 |
| 22h | YES NO | none: a yes or no question; yes sets the equal flag | PARTY SURPRISE |
| 23h | CLEAR AND EXIT | none: clears the text box and ends the script | SURPRISE |
| 27h | TREASURE | 4 and a list: platinum, gems, jewelry, then the number of items and that many item numbers | 8 |
| 29h | (unused) | none: the command is skipped | ENCOUNTER MENU |
| 2Ch | PLAY SOUND | 1: from 80h up a piece of music (less 80h), 2 to 14 a sound effect | PARLAY |
| 31h | ACTION MENU | 2 and a list, as HORIZONTAL MENU, asking "what do you do?" first | SPRITE OFF |
| 37h | FIND NPC | 2: a name, and the cell that receives the party member's place | LOAD PIECES |
| 3Ch | STORE | 1: opens one of eleven shops whose stock the program lists | PROTECTION |

Data may follow CLEAR AND EXIT, as after EXIT, so the census's walk stops
there. TREASURE's items are numbers in `ITEM0`'s definitions; FFh is a scroll
of three random magic-user spells. One script (49) holds a 29h, which the
original skips.

The machine runs WAIT KEY, JOURNAL ENTRY (it tells the entry's number and
waits), YES NO (yes sets the equal flag, no the not-equal flag; no cell keeps
the answer), CLEAR AND EXIT (it clears the text box and ends the script),
ACTION MENU (a horizontal menu after "What do you do?") and 29h (nothing). The
script host answers the others. CALL names three engine routines by number,
not by address: 1 redraws the view, 2 steps the party forward and 3 shows a
portrait's next frame; any other number does nothing.

### Script memory

The script memory is laid out anew. Cells 1h to 400h are the game's state,
one byte each (two for a word), and 401h to 800h are the current character's
record, byte for byte (Creatures, below): 401h is its name, 401h + 508 its hit
points. A script's own image is 8000h to 9DFFh. Any other address reads 0 and
keeps nothing. The cells the engine itself reads or writes:

| Cell | What it holds |
|---|---|
| 5h–Bh | The clock: ticks, minute ones and tens, hour, day, month, year |
| 11h | The party's facing, 0 to 3 (north, east, south, west) |
| 13h | The map LOAD FILES loaded |
| 14h | 1: the temple opens at the next COMBAT |
| 15h | Monsters defeated |
| 16h | The area's script (0 at a new game) |
| 17h, 18h | The party's square before its last step |
| 19h | Searching (bit 0) and looking (bit 1) |
| 20h | The party's size |
| 22h | 1 in a dungeon (the 3D view), 0 on the area map |
| 23h | The facing as the compass's 0 to 7 |
| 2Bh, 2Ch | Camp: the rest period and the chance of an interruption |
| 2Dh | The event of the party's square |
| 2Eh | Surprise: 1 the party, 2 the monsters |
| 32h | The wall ahead |
| 34h, 35h | The party's square |
| 36h | 1 when a member attacked its own side |
| 39h | The color of PRINT's text |
| 3Ah | The tile set and picture of the area map |

OpenGoldBox's engine names the cells by *Pool of Radiance*'s addresses, and
`game_address` gives each its Darkness cell; a *Pool of Radiance* cell
Darkness has no place for is kept where no Darkness script reads it.

## The opening

A new game runs script 16, its load entry only, as the original's game loop
does when the area's script cell (16h) is 0. It loads map 16, New Phlan's
docks, with wall set 1, and puts the party at (15,0) facing west over picture
148, where Rolf, New Phlan's portmaster, offers the party a tour of the city.
Declined, he walks it two squares west and on through the gate's street to
(11,2), facing south, a footstep a square (PLAY SOUND 10). Accepted, he shows
the council chambers, the Cloister of Tyr, the training hall and the rest
before he leaves the party.

`load_campaign(folder, GoldBoxGame::pools_of_darkness)` plays it with the
loader of the later games (`campaign_session.cpp`). What Darkness changes
there:

- One file of each kind, numbered 1; LOAD FILES names a map and one wall set,
  and FIND NPC, not LOAD PIECES, is 37h.
- A wall set is `WALLDEF1` record n drawn from `8X8D1` record n's tiles in the
  colors that record sets, over the 16 EGA colors, with color 13 see-through
  (`load_vga_wall_art`). The maps name its appearances from 1.
- `PIC1` holds animations and `BIGPIC1` still pictures, each drawn in the
  colors it sets (`vga_color_table`, `vga_rgba`).
- TREASURE names its items in `ITEM0`'s definitions: there are no item lists.
- The item words are in `GAME.EXE`: 125 slots of 21 bytes, "Battle Axe" first.
- Behind the walls the view draws a picture, as the original's DrawBackdrop
  does: on a square whose event has 80h, the wall set's own (`BACK1` record
  n), elsewhere the sky and ground (`SKYGRND` 1, the daytime sky, through
  color 13). The original picks other skies by the area's tile set, the hour
  and cell 3Bh; OpenGoldBox shows the daytime one.

`opengold_darkness_tests` plays the opening on the real files, declining the
tour.

## The walk

`opengold_darkness_walk` (`opengold_gold_box_games_tests --darkness-walk`)
walks every square of every map with six fighters made under Darkness's rules
(ORIGINAL-RULES.md, *Pools of Darkness*) and runs each event and fight; it
passes when no event needs what the session cannot do (2026-11-14). Map 49 is
in `GEO1` but no script loads it, so no one walks it. What the walk needed:

- **The current character's record** at 401h: its name, both scores of each
  ability, Exceptional Strength, race (Darkness's numbers: elf 0, half-elf 1,
  dwarf 2, gnome 3, halfling 4, human 5), class (numbered as the original
  rules' professions), age, maximum and current Hit Points, levels, thief
  skills, gender, alignment, companion flag, the party's platinum, gems and
  jewelry, status (0 okay, 6 dead), whether it is in play and its side. A
  script's change to the Hit Points and coins is read back.
- **TREASURE** hands out platinum, gems, jewelry and `ITEM0`'s definitions,
  as the other games' treasure is handed out; FFh is a scroll of three magic-
  user spells drawn at random.
- **STORE** opens one of eleven stores, its goods identified. The program
  lists each store's first and last slot and the slots' item definitions; the
  session finds the lists in the player's `GAME.EXE` by their shape (the
  first store starts at slot 1, each one after the last).
- **FIND NPC** stores a named companion's place in the party (80h more when
  it is out of play), or FFh.
- **SPELL** asks for any spell: the first member with it memorized casts it.
- **DAMAGE:** with 80h its low four bits are the save's modifier and the
  value's three bits the save; 40h hurts the whole party (20h with no save),
  the value's 80h the current character (its save one less, none at 0), else
  a random member; with 10h a made save halves the damage, without it avoids
  it. Without 80h it is that many attacks on random members, each to hit with
  the value.
- **Encounter sprites** (`SPRIT1`) are animations of three frames, near,
  middle and far, drawn in the colors they set with color 0 see-through.
- **Combat icons** (`CPIC1`) are pictures in the colors they set, color 8
  see-through; a monster's second pose is the record 128 above.
- A training hall's classes are cell 30h's mask (127 every class).
- An NPC's record (510 bytes) joins the party as its record holds it.

Still to come: the area map outside dungeons (in-dungeon cell 22h at 0, with
`BACK1` and the tile set of cell 3Ah) is not drawn; the evening and other
skies; the combat ground tiles (`DUNGCOM`, `WILDCOM`); sound effects other
than the footstep, and the music PLAY SOUND starts; the opening and closing
films in the game; the startup menu's button; the route.

### Creatures

A creature is 510 bytes. A character can hold 141 memorized spells and know
125, has spell slots for nine spell levels in three lists, keeps only
platinum, gems and jewelry, and remembers the experience and hit points a
level drain took. Byte offsets:

| Offset | Field |
|---|---|
| 0 | Name: a length byte and up to 15 letters |
| 16–27 | Six abilities, each a permanent and a current score |
| 28, 29 | Exceptional Strength: current, permanent |
| 30–170 | Memorized spells, one a slot; bit 7 while still being memorized |
| 171 | Hours of rest before memorizing finishes |
| 172 | THAC0, as 60 minus the value |
| 173, 174 | Race, class |
| 175 | A paladin's Cure Disease uses left |
| 176 | Age (two bytes) |
| 178 | Maximum hit points |
| 179–303 | Known spells |
| 304 | Attack level |
| 305 | Combat icon size |
| 306–310 | Five saving throws |
| 311 | Movement |
| 312 | Hit Dice: the character's level for the spells and effects that ask for it |
| 313, 314 | Level at the change of class, undead level |
| 315–322 | Thief skills |
| 327 | NPC flag; for a monster, from 80h up, its morale |
| 331–336 | Coins: platinum, gems, jewelry (two bytes each) |
| 337–343 | Levels: cleric, druid, fighter, paladin, ranger, magic-user, thief |
| 344–350 | The same levels before the change of class |
| 351–357 | The highest level reached in each class |
| 358, 359 | Gender, alignment |
| 360, 361 | Attacks per two rounds, first and second attack |
| 362–367 | Their dice: two counts, two sides, two modifiers |
| 368 | Armor Class, as 60 minus the value |
| 369 | The creature's number |
| 370 | Experience (four bytes) |
| 374, 378 | Experience and maximum hit points before a level drain |
| 379, 380 | Class mask, hit points rolled |
| 381–407 | Spell slots: cleric, druid and magic-user lists, levels 1 to 9 |
| 408 | Experience for defeating it (two bytes); there is no award for each hit point |
| 410–413 | Combat icon head and body, order, icon size |
| 414–419 | Combat icon colors |
| 420, 421 | Creature type flags, immunities |
| 422 | Number of items |
| 479–482 | Hands in use, saving throw bonus, encumbrance (two bytes) |
| 491 | Magic resistance |
| 493–496 | Status, in play, side, quick fight |
| 497–509 | Current THAC0, front and rear Armor Class, attacks, their dice, hit points and movement |

The other bytes hold pointers and the running game's bookkeeping.
`late::decode_creature` reads the record into the fields every game's records
share (`por::CharacterRecord`): the coins go to platinum, gems and jewelry,
the Hit Dice to the highest level, and the slots of spell levels 1 to 5. The
bulette, creature 1, has 9 Hit Dice, THAC0 12, AC −2 and 41 hit points.

### Items

An item is 17 bytes: the last 17 of the earlier games' 63, without the name
area. In order: the type, three name words, the magic bonus, the saving throw
bonus, readied, the name words still hidden, cursed, the weight (two bytes),
the count, the value (two bytes), then three bytes of charges, effect and
special power, or a scroll's three spells. `MON1ITM` holds each creature's
list. `ITEM0` holds one record of 254 definitions, numbered from 1, which
TREASURE and the shops copy into a new item. OpenGoldBox reads such a list as
the earlier games' items with no names (`decode_items(bytes, game)`).

### Item types

`ITEMS` holds 128 types of 16 bytes, the earlier games' layout, but with no
header: type 0, empty, starts the file (`decode_item_templates(bytes, game)`).
