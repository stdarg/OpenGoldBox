# Pools of Darkness: how its files differ

*Pools of Darkness* (1991) is the last *Pool of Radiance* game and the first
of the Gold Box games to draw in 256 colors. This page records what its files
change, as the census (`opengold_census`, [GAMES.md](../GAMES.md)) reads them.
The facts come from the reconstructed source of the DOS release 1.1 and from
the files themselves; the wording is our own.

## Combat compatibility checks (2026-11-17)

The [Original Rules record](../ORIGINAL-RULES.md) describes protection scrolls
and Fire Shield. The Aerie's actual dragon-breath protection scroll can now
be used before combat and its twelve-minute effect survives entry into combat.
Bane minions' innate Fire Shield returns twice an adjacent weapon hit's damage;
the cast spell grants the same retaliation, without electricity immunity.
These engine corrections have focused regressions. A current combat replay
also puts Gothmenes down through weapon damage, reaches the actual ending and
restores all six party members, without forced outcomes. The fresh whole route
also passes. Integrated validation and the route's remaining assistance are recorded in
[PLAYABILITY.md](../PLAYABILITY.md).

Behavior was checked against the DOS reconstruction at
[`f24a100d`](https://github.com/talonius/pools-of-darkness/tree/f24a100dc00a302aa1962953ba821c7d768d3c3f)
(`gdconsts.pas`, `viewchar.pas`, `magic.pas`, `effhndlr.pas`). No reconstructed
implementation is copied into the engine.

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

### In OpenGoldBox

Choosing *Pools of Darkness* at the startup menu plays INTRO as `CONTROL.EXE`
does before the game (since 2026-11-14): each frame for its time with sound
on, `P1.SMS`'s song 0 from the first, the title card deaf to keys and a key
ending the film after it (commands 13 and 14, which the decoder reports for
each frame). Then come the game's own title pictures as its Intro shows them:
`TITLE` 1 and 2 for 5 seconds each, a key on either going on to 3, the title
menu's backdrop, which stays 20 seconds; `MUSIC.DAX`'s piece 1 plays from
picture 1 (`load_title_screens`). After the win (since 2026-11-15) the view plays FINAL as `CONTROL.EXE` does once `GAME.EXE` ends: each frame for its time, `P3.SMS`'s song 0 at the music level, a key ending it where the film allows (`load_closing_film`); it stands in place of a dropped-in film (ENDING-FILMS.md).

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
portrait's next frame; any other number does nothing. ADD, SUBTRACT,
MULTIPLY, DIVIDE, AND and OR store their result in as many bytes as the
destination's operand names (its WriteGbvm): one for a byte cell, so 1 taken
from a cell at 0 leaves 255 there, and two for a word (2026-11-15, with
*Treasures*).

### Script memory

The script memory is laid out anew. Cells 1h to 400h are the game's state,
one byte each (two for a word), and 401h to 800h are the current character's
record, byte for byte (Creatures, below): 401h is its name, 401h + 508 its hit
points. A script's own image is 8000h to 9DFFh. Any other address reads 0 and
keeps nothing. The cells the engine itself reads or writes:

| Cell | What it holds |
|---|---|
| 3h | FFh holds back the step whose event set it (stairs, a barred door); OpenGoldBox honors it since 2026-11-14, before which the party stepped on as well |
| 5h–Bh | The clock: ticks, minute ones and tens, hour, day, month, year |
| 11h | The party's facing, 0 to 3 (north, east, south, west) |
| 13h | The map LOAD FILES loaded |
| 14h | 1: the temple opens at the next COMBAT |
| 15h | Monsters defeated |
| 16h | The area's script (0 at a new game) |
| 17h, 18h | The party's square before its last step |
| 19h | Searching (bit 0) and looking (bit 1) |
| 1Bh | The fight's morale: the monsters' side breaks below 100 less this of its Hit Points (CheckMorale). Not played yet (below) |
| 1Ch | Added to the party's movement in the next fight, whose end clears it (RecalcMovement) |
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

Cell 1Bh, the break point, is not played yet: OpenGoldBox's Darkness
monsters break as soon as their side is hurt, where the original's stand
until the side has lost what the script sets (70 to 100 in nearly every
script; 100 in Kalistes' egg room and the finale, so there they never
break). Played so (measured 2026-11-18 from a save before the beat, the
same dice either way), the route's party wins Kalistes in 6 plays of 480,
none with no one lost, against 67 of 240 without it, and the spy's band
after her in 0 of 480, against 148 of 240. The route must re-plan Kalistes'
egg room (84: 3,6) before 1Bh can be played; the code is kept on the local
branch `darkness18-1bh`.

OpenGoldBox's engine names the cells by *Pool of Radiance*'s addresses, and
`game_address` gives each its Darkness cell; a *Pool of Radiance* cell
Darkness has no place for is kept where no Darkness script reads it.

## Modify Character

The party menu's Modify changes a new character's scores, Hit Points and
name while its experience is still creation's 1,500,001 (750,000 or 500,000
for each class of a multi-class character); unlike the earlier games, never
one with none (`chargen.pas`, `ModifyCharacterMenu`). Its Hit Points run from
a point a level, with every level's Constitution bonus when that is above 0
(its `HpMaximum`), to each die's top with its Constitution at each level of
the dice and a fixed amount past them (`HitPointCap`). OpenGoldBox's party
screen offers it so ([ORIGINAL-RULES.md](../ORIGINAL-RULES.md#modify-character)).

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

A fight that ends without a result fails the walk. A monster still charmed
onto the party's side as a fight ends leaves as one that ran, worth neither
experience nor treasure, as the original's CombatDoneCleanup marks it.

Still to come: the evening and other skies; the combat ground tiles
(`DUNGCOM`, `WILDCOM`). The music PLAY SOUND starts plays since 2026-11-16,
until a sound effect stops it, and the closing film after the win with its
song ([later-sound.md](later-sound.md), Where the games play it). Since
2026-11-16 too the scripts' sound effects play: a PLAY SOUND of 2 to 12
plays `ZOUNDS.DAX`'s record one less (eclvm.pas, EclPlaySound; sounddrv.pas,
SoundCommand), and the footstep, sound 10 (record 9, 1772 bytes), is the
game's own for a step (explore.pas, StepForward) as for a script, where the
town view played the other games' synthesized footstep.

## The area map

Outside a dungeon (cell 22h at 0, cell 24h at 1) the party is on the area
map: big picture 240, a grid of 38 columns by 15 rows, the party's square in
cells 25h and 26h. A step goes one square in any of eight directions (north
and on clockwise, as the original's MovePartyWilderness), kept on the grid,
the square left in cells 17h and 18h; it takes half a day. Script 17 reads
the square after the step and names the site there: Phlan's crater at (9,4),
Zhentil Keep's gates at (0,4), Mulmaster at (31,11), Myth Drannor's ruins at
(0,13) and the rest (`OPENGOLD_DARKNESS_SITES` lists them all from where the
route ends).

The original draws no window of its own here: it shows the whole big
picture of the area's tile set (cell 3Ah; its areaPictureIds give 240 for
set 0, 241, 242 for sets 2 and 3, 245, and none for set 5) and draws its
marker, `CURSOR` record 1 through color 13, over the party's square, one
8-pixel cell of the picture for each square (its MapWin). OpenGoldBox's view
does the same (`render_darkness_area_map`, `darkness_area_map.cpp`, since
2026-11-14): picture 240 is the Moonsea, 304 by 120 pixels. The party's
square goes out as a wilderness map's does, so the game's view steps it
with the arrow keys and redraws the marker. Still to come: the colors the
original cycles in these pictures (its palette presets for 240 to 246).

## The win and the route

The game is won when Gothmenes falls and Bane's crystal is turned to dust
(script 75): its PROGRAM 1 saves the party and leaves the game, and the
original's `CONTROL.EXE` then plays the closing film (`FINAL`). OpenGoldBox
tells the win ("Gothmenes has fallen, and Bane's crystal is dust. You have
won.", `game_won`) and goes on.

`opengold_darkness_route` (`opengold_gold_box_games_tests --darkness-route`)
plays the story with the cluebook's party (two paladins, a ranger, two
clerics and a magic-user, made under Darkness's rules) in the cluebook's
order. It takes Rolf's tour, walks off the docks' map into West Phlan (cell
21h, set as a step leaves the map's edge), takes Sasha's escort duty at the
west gate, sees the storm take Phlan, meets Elminster in Limbo and comes back
on the area map at (26,6), where it takes a step west. It walks north-west
across the Lands of Thar, beating the ogre patrols it meets, to the stockade
of Taydome's Keep (19,1), goes in (map 22), waits while the giants lock the
gate, and beats Kimarr and his monsters in the keep's hall (6,4), rested
first.

From the hall it walks square by square, as the planning aid
(`OPENGOLD_SECRET_FIND`) finds the way, fighting what it meets and passing
by what it is offered:

- **Taydome's Keep (map 22, script 22).** South to the meeting room (5,10):
  it beats the ogres and breaks Bostell's chains (cell 128h set to 1). North
  and east past Kimarr's study (11,6) to the room where his giants hold Quil
  (9,5), event 20: the party stays, Bostell rushes in (one ally, three more
  when the front gate was left open, cell C1h 0), and Kimarr, a blue dragon,
  falls with his monsters (cell 123h set to 1). Quil has broken the great
  spell; the party goes with him to Sasha (12,2), who sends it out to the
  area map. Nothing else opens the room: the keep's other events (Ruoln at
  (7,11), the notes, the drow, the cupboard) only tell the story or change
  who fights beside the party. A note at (13,3) teleports the party to
  (11,6) and marks Kimarr's room as found (cell 126h bit 0), so he meets it
  there without his speech.
- **Vala's cave (map 37, script 37),** east on the area map at (27,2). The
  party tells the hill giants it has come to see the Silver Lady (1,8),
  meets Vala in her office (2,6), and she joins it. With her it takes the
  four items in the cluebook's order (cell 1ADh, a bit each): the Worldstone
  from the giants at (9,0), the Crucible of Flame from its fire elementals
  at (11,4), the Lindenwood Staff from the coffins at (4,12), searched
  facing west, and the Oakroot Staff at (7,14), found only in search mode.
  It rests in a quiet room (1,13), where the digging lets it (the camp
  entry allows a rest only on events 3 and 25 up), then uses the staves in
  the fortified chamber (14,8); Vala charges into the breach and the party
  beats the Vaasans who got through (cell 1ABh 128). It leaves by the
  entrance (0,8).
- **The Temple of Tyr (map 50, script 50):** at Melvaunt's crater (21,5)
  Priam joins the party and leads it in (3,7). A guard sprints upstairs as
  the party reaches the meeting room (3,5); it pursues at once, so the chase
  (the script moves the party) surprises the vampire priest and his
  marauders in the office (11,1), and the party finds journal entry 11, the
  ritual. Down the stairs (8,5, the wall of type 7 ahead) to the worship
  area, it scrubs the black hand off Tyr's altar (3,1), the priest's ward
  gone, and earns Tyr's blessing (cell 17Fh bit 1).
- **The catacombs (map 50's lower half, script 54).** Down the stairs east
  of (6,5) to (7,13); west through the cells, past the marauders fleeing
  Brimwulf (3,8), to the stairs at (0,8), which lead down to the east half
  (8,9). Brimwulf's tomb (13,13), entered facing north: the party opens the
  coffin, stands away from his wrath and agrees to be tested (cell ABh 1).
  Through the secret door beyond, the small altar (12,10): the ritual by
  journal entry 11's third way, "light incense, then sip from bowl" (cell
  17Fh bit 0), so a single elemental guards each test. The earth elemental
  (10,10) leaves the Vessel of Purity (bit 3); the fire elemental at
  Ilmater's symbol (12,8) the Spirit Flame (bit 4); in the armory (14,10)
  the flame lights the Lamp of Vigilance, Brimwulf gives his charge, and the
  party takes it (bit 5) and steps on to the shelves of arms. Back up the
  stairs at (8,9) to the west half and (7,13) to the temple, where the Red
  Plume warriors rejoin it at (6,5).
- **The temple freed (map 50).** Up the stairs west of (1,5) to the upper
  floor (8,5), and round its corner towers, lighting each iron brazier from
  the lamp (LIGHT BRAZIER; cell 17Dh gets bits 10h to 80h): (8,0), (15,0),
  (15,7) and (8,7), where the darkness is swept from the temple. In the bell
  tower (12,3) it pulls the ropes: with the lamp carried and every brazier
  lit the bells ring clear, and as neither Laurellin nor Brimwulf was slain
  their shades are reunited (cell 181h 1; journal entry 74 tells their
  story, but the script asks only that neither died). Down the stairs (8,5)
  and out through the entrance hall (3,7), the plunder in the dining hall
  left alone (taking it earns the Curse of Tyr): Priam stays with his army.
- **The Hill Giant Steading (map 21, script 21),** reached on the area map
  north-west then west along row 4 and north-west twice to (9,2). The party
  talks its way past the gate's giants as would-be mercenaries (TALK; cell
  115h bit 7), and walks north through the courtyard (the insulting giants,
  the ogres' and ettins' brawl) to the Twisted Horn (9,8), entered facing
  east, where Kardal tells it to kill the cloud giants in the private room to
  the south (journal entry 31; cell 114h bit 1). Beaten at their gaming table
  (9,10), they win it promotion (bit 2): the hill giants pull it through the
  inner steading's barred door (7,8, facing north) and send it to the
  shaman (bit 3). Kardal's dagger lies in the hall (8,3, bit 4); the party
  rescues him from his captors (6,4, RESCUE HIM; journal entry 49, bit 5).
  The rear guard at (3,6) is met from the north: a paladin challenges their
  authority (his Charisma at least 17), the giants turn on the ogres and the
  party beats the winners (bit 7). West and north round the dragons' hoard
  (3,2), it attacks the giants making a stand (5,0), then beats the white and
  black dragons in the antechamber (11,1) and the Hill Giant Shaman (13,1;
  cell 116h FFh). His vault (12,0) is locked and no one forces it (Strength
  18 or Knock), so the party leaves it; the door in the north wall at (15,0)
  leads to the Fire Giant Cave (script 32, map 32), where it arrives at
  (3,15).
- **The Fire Giant Cave (map 32).** Ungleow's bribe (5,12) is left where it
  lies. The endless corridor along the south wall is script 32's entry
  before a step (809Ah, the original's vmRun[1], which runs on the square
  the party stands on before it moves) for event 6, the squares (8,15) and
  (11,15): unless cell 132h bit 0 is set, the step east from (11,15) puts
  the party back on (9,15) and west from (8,15) on (10,15), holds the step
  (cell 3h) and counts in cell 9Eh. OpenGoldBox ran it as the original
  does already; the earlier tries counted nothing because each beat placed
  the party on (9,15) and stepped onto (11,15), which only arrives there.
  The route steps east from (11,15) until the count reaches 12, when the
  crushed skull asks "DO YOU WANT TO STOP AND COLLECT YOUR THOUGHTS?" (YES)
  and "DO YOU PICK IT UP?" (YES), and THROW IT AWAY breaks the spell. East
  and north past the fire giants (13,12), through the room that teleports
  the party (7,6) and over the rotating floor (12,7), round by the mage's
  guards (10,4) and the assassin (2,3), it attacks Ungleow in her room
  (2,5; cell 134h bit 0), beats the red, black and green dragons at (0,2)
  (cell 133h bit 1) and follows the dragons west from (0,1): the tunnel
  seals and the party is on the area map at (7,1).
- **The Dragons' Aerie (map 33, script 33),** north-west at (6,0). The four
  keys come from the nests in the order the planning aid finds them (cell
  144h, a bit each): the copper key from the green dragons (12,15), the
  iron key from the black dragons, attacked (12,1), the silver key from
  the white dragons Modthryth recruits (3,1) and the steel key from the
  blue dragons (3,14). USE KEYS at the doors (7,2, facing north; cell 144h
  FFh), then Modthryth's dracoliches (cell 141h FFh) and the gate (7,0,
  entered facing west) to Limbo.
- **Limbo (script 18).** Elminster's REST is PROGRAM 9, which in the
  original camps and goes on with the script (its EclProgram ends the
  script only on the area map); here it rests a night and the script goes
  on, the route choosing REST until the party is whole, its spells chosen
  after each night; the clerics and paladins memorize Resist Fire, a copy
  for every member between them, for Thorne (2026-11-15). TRAIN opens the
  hall, which in Darkness names no price ("Who will train?"). MOVE ON,
  THORNE.
- **Thorne's cave (map 34),** before the Horn of Doom's third blast: the
  shortest way the planning aid finds to Thorne (3,7), turning aside once
  into the cluebook's room 4 (15,12, entered facing south), where the party
  attacks the red dragon stuffing its treasure into a hole (ATTACK; cell
  153h bit 7) and the leading paladin takes and wears the hoard's one ring,
  the Ring of Fire Resistance (unidentified; the route's `take_items`).
  Thorne, whom the party will not leave (YES), and his seven red dragons
  are beaten (cell 152h FFh), members dead and raised after; he leaves the
  Horn of Doom. No camp in the cave rests until he falls (script 34's camp
  entry), so the Resist Fire memorized in Limbo is cast on each member
  before the fight, halving his breath's 24d10+12 (the route's
  `ward_each_with`; 2026-11-15). Back
  to the gate (0,13, westward), and from Limbo (REALMS) Elminster returns
  the party to the Aerie's gate, the script it came from (cell 9Ah). It
  walks out by the Aerie's entrance (8,15), declining the cave again.
- **Zhentil Keep (map 19, script 19),** south-west at (0,4): the party pays
  the drow's tax and walks to the training hall in the city's south half
  (map 19's north half is Mulmaster's, script 26; script 19's events 4 and
  5 there are only what the survey met). From the hall's sign (3,11) north,
  the clerk (3,10, event 4, facing north) asks 10 gems of those not on the
  approved list (YES; WHO pays); in the hall (3,9, event 5, a facing with no
  wall ahead) each member with the experience trains (PROGRAM 0). East to
  the Purple Plume Tavern (12,14, event 18, entered from its sign at 13,14
  facing west), where it stays (YES): its camps are left alone while the
  guards are not roused (cell A7h, script 19's camp entry; the cluebook's
  "mostly safe"). Then north to the courtyard gate (8,7), into the halls
  of Manshoon's tower (map 48, script 48). The seven clerics (12,12) ask
  whether it goes on, has entered Zhentil Keep before and how often (once:
  "1"), and whether it bears Thorne's gift (YES): it passes. The original
  never lets a true count pass (ORIGINAL-RULES.md, Bugs; fixed). The ten
  liches find the horn and are beaten, the tax is paid, and the path at
  (3,8) descends to the drow stronghold.
- **The Descent to the Depths (map 35, script 35)** to the path down at
  (7,14) into the drow temple's halls (map 68, script 68).
- **The slave pens (map 68, the cluebook's Silk and the Slaves of the
  Drow),** where the drow patrols fall on any camp outside the cluebook's
  quiet rooms (a camp at the stairs met twelve drow sorcerers, champions and
  a drider and lost): from (7,15) round to Silk (7,13, entered facing west
  from 8,13), whom it approaches and agrees to help (APPROACH, YES; cell
  25Eh 1): she joins it, and the room is safe to rest in (cell 9Ah 1; the
  cluebook's location 4). East through the door at (12,13) into the prison
  wing, whose events are script 71's (script 68 sends the party there
  stepping east from 12,13, and 71 back stepping west from 13,13): the
  prison guards (14,12, entered facing north) fall to the party's attack
  (YES) and leave a ring of keys (cell 262h bit 1), and in the cell at
  (10,11), entered facing west, Silk finds her Swanmays and leaves with
  them and the other prisoners (cell 25Eh 3; journal entry 89). Back to
  Silk's room to rest, and north through the barracks, where a patrol
  falls on the party (7,10), and west along the arches to the quiet room
  (1,8, entered facing west: "IT SEEMS SAFE TO REST HERE", cell 9Ah; the
  cluebook's location 4), where it rests. Then north and west over the
  spiked pit's bridge (0,1, CROSS BRIDGE) past the doors' lightning, through
  the teleporting room, past the iron golem's room (12,1, entered) and the
  fountain (14,1, waded) to the secret door at (15,1), stepped through
  eastward into Kalistes' Temple (script 69). The way is the planning aid's
  with `OPENGOLD_SECRET_PREFER=CROSS,YES`.
- **Kalistes' Temple (map 69, scripts 69 and 72; 72 holds the events the
  temple's table of cell 27Bh sends it, and sends the party back).** Its
  camps are broken (cell A2h) outside the barracks. In by the secret entry
  (0,0), east and south to the priests' quarters (8,2, entered facing
  north), where two priests mark a map and leave it (YES: it shows Hillsfar,
  the Plains of Thar and Thentia). Past the guard who locks the party in
  (9,1, WAIT) and the priest who takes it for assassins (NO), to the
  barracks (13,1, from 12,1 facing east), whose drow squad the party
  attacks (ATTACK); beaten, it rests there (cell A2h 1; the cluebook's 16).
  South along the east corridor to the oracle (9,10, entered facing north;
  journal entry 93), which the priests at the inner chamber's doors wait for
  (cell 277h; the step entry holds the party back until then). Up the middle
  aisle into the inner chamber (8,6, facing north): the party disrupts the
  ceremony and beats eight drow priests, four sorceresses, two Pets of
  Kalistes and two phase spiders (members dead), and destroys what the
  oracle named (INPUT: HANDS; cell 278h 1), closing the portals to the
  Realms. Through the last portal (8,4, YES) it steps into Limbo, where
  Elminster's camp rests it until whole, its dead raised, and it trains.
- **The Web Dimension (map 83, script 83),** from Limbo (MOVE ON, KALISTES
  LAND): the Eyes of Kalistes, Cynthia, asks who helped the party at the
  temple, and only when Silk's Swanmays are free (cell 25Eh 3) can it name
  her (YES; INPUT: SILK). Silk's sister, she asks three favors. The party
  captures the drow patrol at (4,4) (CAPTURE; cell 221h bit 5), enters the
  cave at (6,1), the Cavern of Fireflies (script 85, which runs the event
  cell 21Ah names and sends the party back), and takes its nest of pale
  spiders (YES, YES; cell 221h bit 4); back at (8,4) Cynthia's spiders
  disguise the drow as the party for Kalistes' eyes, she names the Ward of
  Safe Passage and the amulets the party needs, and asks it to kill her
  (BEHEAD). Without Silk, LEAVE sends it back to Limbo, and STAY or FIGHT
  sets Cynthia against it (cells 21Dh bit 2, 221h bit 5) and spiders fall on
  it. Script 72 stores one OR to address 0 (988Bh), which the original
  ignores (its GbvmZone): OpenGoldBox's later engine now reads address 0 as
  0 and keeps nothing. Out by (7,9) stepped south (YES, then NO to entering
  again) onto the web's paths: script 80, an area map like script 17
  (picture 241), whose sites are the minis, all on map 83 with an event
  table each by cell 1E8h (0 the ruined temple at 4,13, 1 the stockade at
  19,3, 2 the ramparts of lava at 13,11, 3 the forest village at 23,10, 4
  the ruined city at 37,6, 5 the village beside the road at 33,1, 6 the
  spiderlike structure at 5,5), with the Testing Ground's gates at 13,7
  and the Parlor's wall at 34,1. The route walks the minis by; the cluebook
  calls them a help, not a need.
- **The Drow Testing Ground (map 81, scripts 81 and 86),** six squares
  north-east and three east of the ruined temple. Past the gate's guards,
  south through the compound, ambushing two drow (12,10, YES), to the
  barracks (13,14, entered facing east), whose drow it attacks and where it
  rests ("BODIES LITTER THE FLOOR. YOU MAY REST HERE."; the compound's drow
  priests break every other camp, so the walk there is unrested). Through
  the east wall's secret door (8,14, YES, YES) into the Test (9,14): a
  member strong enough breaks the wall (10,14; WHO, Strength at least 15 to
  18 by a roll, the others trying again), the party pushes on through its
  dread (11,11, PUSH FORWARD), beats the driders (11,7) and the drow mages
  (8,4), answers the riddle with BANE (7,4), walks on through the wall of
  flame (4,6, YES, YES), and approaches Hem (4,12, APPROACH), a drider, and
  beats him and his kind. The table beyond (6,14) holds six magic daggers
  and the amulets that repel spiders (cell 1FFh 1). At the altar (8,9;
  journal entry 37) Daris flees with the Ward; the party follows round by
  (6,9), beats his guards at (7,8) and picks the Ward up (YES): Shest, the
  Pet of Kalistes, and his enormous spiders fall too (cell 1F7h 1). Back
  through the Test, resting where it is hurt (the cluebook: any section it
  has completed), and out by the compound's south way (5,15, YES).
- **Kalistes' Parlor (maps 82 and 84, script 82),** by the paths (six
  north-east, fourteen east) to the village beside the road (33,1) and round
  it to the Parlor's wall of gossamer (1,4), where the spiders shy from the
  Ward (TRY TO ENTER; script 85's event 4 sets cell 21Bh, and script 83 sends
  the party to the Parlor's foyer, map 82 at 7,15). Locaste, without her
  Ward, is devoured (journal entry 14). Since 2026-11-15 the party goes
  the cluebook's way rather than down the chute into the pets' run: west
  round the foyer to the guards' quarters beside the main doors (2,14),
  whose drow it beats and where it rests ("YOU CAN REST HERE"; the
  Parlor's camps rest nowhere else); north to the kitchen (2,1), where it
  rescues Arta from the ogre cook (RESCUE) and beats the guards, pets and
  gaze spiders he calls; back to the quarters to rest (NO to the trapdoor
  the first time, walking back without a camp), then down the kitchen's
  trapdoor (YES) to the larder (map 84 at 2,1), leaving the bound slaves
  as they ask (IGNORE), and east and round the lower level to the egg room
  (2,6), where the party destroys the eggs (DESTROY): Kalistes
  comes with seven drow champions, eight drow wizards and five pets, and
  once she falls the slave who is Tanetal's spy calls basilisks, medusae
  and earth elementals (journal entry 40). The party holds the Crystal Ring
  (cell 208h 1). Up the stairs (13,4 on map 84, YES), through the upper
  level and out by the main doors (7,15, YES); twelve squares south-west
  and eighteen west along the paths to the ruined temple, and through the
  black pool at (8,4) (YES) to Limbo, where Elminster rejoices at the ring
  (journal entry 82), rests and trains the party and returns it to the
  Realms by the way it left them, Kalistes' Temple's last portal (69: 8,5).
- **The way out (2026-11-15).** From the portal down the temple's middle
  aisle and out by its main doors (8,15, YES to leaving), which open on the
  Descent to the Depths (map 35 at 2,7) behind the guards the party never
  met. Map 35's walls name its ways by their type, read by the step entry's
  table (9B7Fh): type 1 a path (facing east it leads down, else up: north of
  row 7 to map 48, on row 7 to 69, south of it to 68) and type 2 a ladder.
  West off the map's edge, which wraps to its east side, past the spider
  idol (15,7) and north to the long hall's near ladder (12,6), where the
  drow draw it up behind the party (cell 194h keeps the row; the cluebook's
  15: "the party must use the ladder at the other end"); south down the hall
  to the far one (12,15, facing west, YES). On through the skirmishers'
  rooms (the cluebook's 14), whose archers shoot from behind and hold the
  step unless the party goes on (GO ON? YES), up the ladder at (7,12), and
  west and north to the empty wagons (2,5, facing north; the cluebook's 3):
  the party gets in and waits (YES), and the wagon carries it up into
  Zhentil Keep, where it sneaks away unseen (YES) inside the gate (map 19 at
  7,8). Manshoon's tower (48), the way down, is left aside. South through
  the city to its gate (8,15): stepping off the map asks "YOU ARE ABOUT TO
  LEAVE. CONTINUE?" (YES; cell 21h), and outside on the area map the party
  declines to enter again (NO).
- **To Myth Drannor (2026-11-15).** Nine squares south down the area map's
  west edge, fleeing the basilisks and dracolisks that roam there (one
  turned the magic-user to stone when the route fought them), to the ruins
  (0,13), where Princess Nacacia welcomes the party in her glen, asks it
  into the ruins with her knights (YES), joins it and offers six items
  against the rakshasas (TREASURE of `ITEM0` definitions 144, 151, 46 and
  more), and leads it in by the broken gate (map 66, script 66, at 15,4).
  The gifts are a light crossbow +4, quarrels +4 and four stacks of blessed
  bolts (types 27, 12 and 74), unidentified: the ranger takes the crossbow
  and readies it in place of his sword and shield, and readies the blessed
  bolts in place of the quarrels (2026-11-15).
- **Myth Drannor (map 66, 2026-11-15).** The ruins' camps rest only on
  events 16 and up (the cluebook's locations 17, the ruined houses);
  elsewhere a camp draws the roaming basilisks and dracolisks nine times in
  ten, so the party walks without camping. The cluebook's word is that
  blessed bolts kill the rakshasas, which no spell below the eighth level
  touches; a plain one takes half from weapons +1 and +2, a Rukh, Rajah or
  the Maharajah nothing from +1 and half from +2 and +3. So the route gathers
  every crossbow and blessed bolt the ruins hold, as a player would: south
  and east to the room of smashed artifacts (15,9, the cluebook's 6),
  entered by its door facing east in search mode (script 66's event 6
  reads the facing and cell 19h), where the second paladin takes the stash's
  light crossbow +3, quarrels +4 and four stacks of blessed bolts; west along
  the street north of the knights the rakshasas drain (the cluebook's 9,
  left alone, its fight would spend the bolts), to the spectres' room (3,10,
  the cluebook's 11, entered westward), where the first paladin takes the
  elf's armor and with it a light crossbow +2, quarrels +4 and two stacks of
  blessed bolts (YES). Each bearer's blessed bolts (two to a stack) are
  joined into one stack, the items menu's JOIN (below), so a fight shoots
  them all. Rested at the ruined houses (6,13), the casters taking Dispel
  Magic first, it beats the Banites and rakshasas at their altar (3,14, the
  cluebook's 10; the only way to the ruins' west side), and goes round the
  west side, past the narrow passages (YES) and the rakshasa posing as a
  knight, to the door that will not budge (4,4, facing north): the second
  paladin, of Dexterity 18, climbs over and secures a rope (CLIMB OVER,
  WHO). Round to the house west of the temple (3,0), where it rests, and
  into the temple (5,1), where it smashes the knights' globes (SMASH
  GLOBES) and beats the Maharajah, three Rukhs, two Rajahs and seven
  rakshasas (cell 1D8h FFh). Before each of the two fights the bolts'
  bearers ready their crossbows and the blessed bolts again (`ready_types`,
  where the last fight left them their swords); the route's player shoots
  the Rajahs, then the Maharajah and the Rukhs, then the rakshasas, steps a
  bearer clear of the enemy next to it (a launcher shoots nothing with an
  enemy adjacent, as cbtacts.pas has it), and frees a held member with
  Dispel Magic. Nacacia falls there; at the gate (15,4, stepped from facing
  east: "DO YOU WANT TO LEAVE MYTH DRANNOR?", YES) the knights carry her body
  away and see the party out (her glen and its chest are for a living
  Nacacia).
- **To the Red Tower (2026-11-15).** Three squares north-east and one east
  on the area map to the red tower (journal entry 99), entered (YES): the
  garden of the Red Tower (map 70, script 70, at 4,15).
- **The Tower of Marcus's lower tower (maps 70 and 71, script 70;
  2026-11-15).** Its levels are parts of one map (levels 0 to 5 on map 70,
  6 to 8 on 71, which LOAD FILES swaps in), the level in cell 17Ah; its
  paths are walls of a type the step entry reads (table 989Fh by the wall
  ahead, cell 32h), and the party's facing (cell 11h) says whether a path
  goes up (west), down or out. Through the garden to the path down (7,10,
  YES) to the dungeons of level 0, past the guards' room (the cluebook's
  10) to Shal's cell (10,9, the cluebook's 11): the party agrees to take
  her to the top (YES; journal entry 8). The events from 9 up run only
  when the step comes through a door or path (cell 9Ch, set by the step
  before from the wall ahead), as Shal's cell door. Back up through the
  dungeons (9,0) and the levels' paths, the way the planning aid finds,
  past the drow who do not strike (4,5) and the cleric lecturing his troops
  (13,5). The guards' runners (the cluebook's 10: "a few go for help")
  leave cell 176h at their row, and their reinforcements meet the party
  there on the way out; the dungeons' and the garden's camps drew the
  tower's wandering mages and monsters every time, so the party walks them
  hurt, its clerics curing the badly hurt and the knocked out. It rests in
  the levels' closets (event 30, "A CLOSET.", script 70's 92D1h; the camp
  entry's table by the square's event, 9862h, lets them rest), stepping in
  and out by their doors: level 1's (8,0, from 8,1), level 2's (7,7, from
  7,6) and level 6's (71: 2,0, from 2,1), before Moander's monsters (10,4,
  the cluebook's 12: bits of Moander, cultists, then Black Circle mages)
  and again after them. Level 7's pincer (the cluebook's 16 and 17): through
  the door at (6,9) the party hears wizards ahead and others behind; it
  taunts the group to the west (TAUNT) and ducks into the closet north
  (6,8, WAIT HERE? YES), where the two factions destroy each other (cell
  177h; "BLASTED BODIES LITTER THE FLOOR" after), as the cluebook says;
  walking on east instead met both and lost. On
  level 8 the sortie against Phlan (3,12): the party hears the troops
  mustering and does not go on (NO), the held step runs the guards' event,
  and Shal bluffs them as Petra (TRY IT? YES; cell 178h 31 with Shal): the
  party hurries past through the wizard gate into the upper tower (script
  67, map 67, which locks behind it).
- **The upper tower (map 67, script 67; 2026-11-15).** Level 9 is its rows
  0 to 4, level 10 rows 5 to 9 and level 11 rows 10 to 15. Its camps are
  broken (script 67's camp entry: a period of 96 steps and an interruption
  certain at its end) but in the quiet side rooms, event 27 (2,0; 5,9;
  11,15: "IT SEEMS SAFE TO REST IN"), so the party walks on hurt between
  them, its clerics curing the badly hurt. Each faction broken up sets a
  bit of cell 251h; with all eight Marcus stands in his chambers. The
  cult's altar (15,2; cut them down, YES; bit 0); down the path at 11,2 to
  level 10; the Red Wizards' antechamber (13,5, WAIT HERE? YES: the code
  word RED), the two Red Wizards surprised beyond its south door (13,6;
  cell B0h 1), and the loft's door (14,7: RAP, YES; RED): the covens fight
  each other and the party beats what is left (bit 1). The Black Circle's
  lofts (row 5, each entered through its door from row 6, and 0,6; bit 2),
  a rest at 5,9, the Fountains of Moander (1,8, through its door from 1,7;
  bit 3) and the Black Circle formed outside them (1,7, stepping out
  through the door; another bit 2). Down the path at 6,9 to level 11: the
  manufactories of the Red Wizards (7,11 from 8,11; bit 4), the Black
  Circle (7,14 from 6,14, ATTACK; bit 5) and the cult (9,13 from 9,14;
  bit 6), a rest at 11,15, the Dispersion Diagram (14,15 from 13,15; bit
  7, MOVE rather than break the beasts' circle). Through two squares of
  the Temple of Transformation's rapt devotees (MOVE; a third and fourth
  step wake them, cell 9Dh) to the secret underpass (11,10 west to 3,10)
  and the Chambers of Marcus (0,12 south): his spectres, his minions and
  Marcus himself (script 77; a stand-in, below). Shal and Petra are whole
  again; back by the underpass to the Vessel of Moander (13,9 from 13,10):
  it reels as the tower falls, the party leaps aboard, and the ichor ebbs
  into a Pool of Darkness and Limbo, where Shal leaves.
- **Limbo and Moander (2026-11-15).** Elminster's camp rests the party and
  it trains (500,000 test experience a member, a stand-in for the tower's
  wandering mages, cultists and bits of Moander); MOVE ON, MOANDER, and
  the party arrives in the depression on Moander's body (map 52, script
  52, 7,10). It climbs out by the footholds north of 7,9 (YES; the event
  asks only with a wall ahead), onto the body (script 51), and declines to
  climb down again (NO).
- **Moander's body and the Large Wound (scripts 51 and 52; 2026-11-15,
  late).** Script 51 is an area map of 38 by 15 places (cells 25h and 26h,
  as script 17's; script 51's table by row at 8A3Fh gives each square's
  ground, 20 and up a place, its event table 8E11h by place). From the
  depression (12,4) the party walks six steps south-east and three
  south-west to the great wound in Moander's arm (15,13; the cluebook's
  Large Wound), fleeing what roams the body, and enters (DO YOU ENTER?
  YES): script 52 places it in the wound's part of map 52 at (12,15). Its
  camps rest only with Aeghwaet once he is helped (the cluebook), so it
  walks hurt, its clerics curing. North to Aeghwaet (12,12, event 11),
  who asks it to rid his home of Tanetal's minions (YES; cell 232h bit 7);
  the minions' four fights (events 14 to 17 at 6,11, 11,10, 10,15 and
  14,13) come in the order cell 233h keeps wherever they are met, each a
  bit of cell 239h (30 with all four): Bane minions mining the flesh, a
  swarm of bits of Moander, mages with elementals and
  elementals excavating. Back at (12,8, event 13, from 13,8 westward)
  Aeghwaet gives a small reward, then the two potions the heart needs
  (cell 23Bh 1), and his healing (YES; his temple's menu).
- **The Entrance to Heart (map 52 from 8,0; script 52).** With Aeghwaet
  helped the wound's camps rest: rested, the party leaves by the opening
  (12,15: DO YOU WANT TO LEAVE? YES, then NO to entering again), walks the
  body north and north-east to the wound in Moander's side (21,3: DO YOU
  ENTER? YES; the cluebook's 24), rests there, and at the lung's muscle
  wall (8,5, event 29; the cluebook's 25) spreads the potion on it (USE
  POTIONS) and cuts through (YES) into the heart (script 53).
- **Moander's heart (map 53, script 53; its words are script 55's, which
  it runs with NEW ECL and returns from).** No camp: Tanetal comes to slit
  sleeping throats (the cluebook), so the party walks hurt, its clerics
  curing. The heart beats: cell 9Bh counts the party's steps, DUB comes at
  half the beat (cell 243h; 60 steps at first) and drains a party in an
  atrium or its vessels (events 20 to 23) into a ventricle, LUB at the
  whole beat flushes one in a ventricle or the vessels it drains (events
  10 to 18) out of the heart; cell 9Ah is 1 after a LUB and 2 after a DUB.
  Its muscle tissue (event bit 40h) is safe, and a player times the
  vessels by the beat, so the route paces tissue until the beat it waits
  for has passed (`pace_heart`). The heart's story comes to a party
  wandering its tissue (event 0: 1 step in 20, then 1 in 10), each part a
  bit of cell 24Ah: the army's remains (32), the Watcher's scolding (4),
  Tanetal slaying two soldiers (1); then the Watcher's five spots (events
  26 to 30; cell 242h her business, cell 24Dh the spots used) serve her
  story in turn wherever met: at 1,13 the potions are mixed (the slime
  falls away, cell 241h 1); the tools (11,1), the arrow's four weapons
  (4,5: YES four times, NO before it stings; Moander's pulse quickens to
  a beat every 14 steps, cell 241h 2); a fake Tanetal attacked (ATTACK);
  at 7,6 the unguent stolen; the cornucopia (8,0); at 9,4 Moander lulled
  to sleep (a beat every 80 steps, the tuning fork, cell 241h 3); the
  magnetic iron golems up the pulmonary artery (9,3); the unguent (15,3,
  carried there by the artery's current); at 10,10 the magnetic weapons
  noticed; the shock in the left ventricle (from 11,8: YES), which flushes
  Tanetal and drops the Talisman; at 15,7 the trap (YES); the chase
  (events 32 to 38, cell A2h counting them: 15,4, then south to 15,15,
  west to 12,15, north to 12,12, west to 9,12, south to 9,15, west to 2,15,
  where shocking the wall, YES, drains the party into the right
  ventricle); and Tanetal's last stand (5,12, event 8), which leaves the
  Talisman of Bane (cell 245h 1). Out by the vena cava's end (2,15,
  south: ARE YOU SURE YOU WANT TO LEAVE THE HEART? YES) onto the body by
  the side wound (20,3), and back to the Large Wound, where Aeghwaet heals
  the party. A step where agents of Tanetal no play of the party beats came
  on it is played again with other dice (`RouteBeat::reloads`, a player's
  reload): three in the heart.
- **The depression's gate (2026-11-16).** Out of the wound (YES, then NO)
  and over the body three squares north-east and six north-west to the
  depression (12,4; DO YOU ENTER? YES, map 52 at 7,9). Its gate is event 10
  (7,10), met with the wall south of it ahead (script 52, 8833h): DO YOU
  STEP THROUGH THE GATE? YES, and in Limbo Elminster greets the Talisman
  (journal entry 41), his camp rests the party and it trains. Script 18's
  gates are four, cell 99h the one the party came by (the Aerie, the drow
  temple, Moander, Arcam's cave), each naming the way back and the way on
  (cells 2EFh and 30Dh): from Moander's, REALMS goes down Elminster's
  rope from the Red Tower's pool to the area map (5,12), the party's
  stored goods left (NO).
- **Mulmaster (map 19's north half, script 26; 2026-11-16).** Twenty-five
  squares east along row 12, passing the wayhouse and the village (NO),
  and north-east into the black city (31,11; YES). MOVE ON past the gate's
  guards (0,3); in the alleys Banites chant the labyrinth's way (journal
  entry 24). The party has offended no one, so the arena (6,6, event 21,
  entered from its grating at 5,6) holds a round or two (cell A6h: a random
  1 to 3, one more with cell A1h's bit), the pens' monsters each time; the
  beholder's interview (WILL YOU COME? YES) takes it down into the Cave of
  the Beholder (script 36, map 36 at 0,0; cell F8h bit 1).
- **The Cave of the Beholder (map 36, scripts 36, 38 and 42; 2026-11-16).**
  Script 36's step entry reads the wall ahead (cell 32h into 9Dh): type 2 a
  ramp (GO ON?), type 4 facing south the labyrinth's door, type 1 events 50
  to 54 that move the party, and its event table by cell A5h. South from
  the ladder to the mustering troops (5,4: the Chancellor's guard of
  margoyles and two mercenary teams, cell 1A3h) and the spectacle (5,5,
  journal entry 39): the Chancellor offers a task against Arcam's rivals
  (script 42: Manshoon in Zhentil Keep), which the party refuses with each
  higher price (NO four times; YOUR TASK IS TO DIE), and it beats his
  mercenaries; rested at 5,7, it beats the Chancellor covering Arcam's
  retreat (5,8; cell 19Bh bit 0). South, an escaped prisoner (5,9) leads it
  out by the tunnel to the surface (0,3: GO UP? YES), and the tunnel on the
  area map brings it back in at 14,5, the way the planning aid finds round
  the labyrinth: west and south through Arcam's chamber, the ramps at 12,8
  and 1,7 and the passage at 15,8, to the Sentinel Halls (8,11, script 38;
  the cluebook's 26). There its camps rest once Bane's minions have
  chanted past three times (cell A4h; the route camps again where they
  only pass by), and east along row 11 the dragon sentinel hears the Horn
  of Doom (dragons, beaten), the spider sentinel sees the Crystal Ring
  (spiders, beaten), and at 2,11 the fist senses the Talisman (cell 245h)
  and opens (cell 1A2h 1; its minions beaten); stepped east again it lets
  the party through, and south and east at 4,12 the Pool of Darkness (event
  7, the wall of type 1 ahead; DO YOU ENTER? YES) leads to Limbo. Arcam
  himself is not met: the scripts send the party on without him, and
  Elminster finds it ready for the Dark Dimension.
- **Dark Phlan (map 64, script 64; 2026-11-16).** From Limbo, MOVE ON,
  BANE'S LAND, YES: the drow carrying Sasha off are beaten where the party
  arrives (13,0), the crowd carries it to Cadorna's announcement (11,1,
  LISTEN; journal entry 76), and it rests at the inn (12,11, YES; the
  camps rest only on its events 2 and 3). West through the city, Gothmenes'
  welcoming minions met at its west gate (1,5), and on past the map's
  western edge to the Iron Wall (column 14, met facing west; event 1):
  USE HORN OF DOOM, Gothmenes' minions beaten (a stand-in, below), the
  horn blown once for each member (the INPUT: 6, cell 20h), and through the
  ruins (YES) with the Crystal Ring held (cell 208h) to the palace (script
  65).
- **The Palace of Gothmenes (map 65, scripts 65, 73, 74 and 75;
  2026-11-16).** Its secret doors (walls of type 5) let a party through
  only while it searches (cell 19h at 1; cell B1h), and a step taken
  searching runs the objects of power (who bears each, the first time)
  instead of the square's event, so the route walks the palace's winding
  way in search mode, EXIT leaving the objects as they are. At 4,4 it
  stops searching and meets "Sasha" and the Phlanerians (event 0; the
  cluebook's 4): it will not tell where Elminster is (NO, NO), which sets
  the Tantalus Clock's strikes to 77 (cell 1CDh; telling sets 66, and a
  party that never meets her has none, so Gothmenes returns at once), and
  the rakshasas are revealed (a stand-in). At the inner palace's entrance
  (8,8, event 11) Gothmenes winds the clock and leaves (script 74, then 73).
  The clock strikes every second step (cell 1CEh), no wandering monster
  comes while it ticks, and Bane's Crystal shows in the pool only past the
  50th strike, so the party paces the entrance hall, then from the pool's
  edge (9,8, stepping south) the ring's bearer tries (YES), puts the ring
  on rather than reach in with it in a pocket (NO, YES) and takes the
  crystal (cell 1CFh FFh): the clock is set to run six more strikes. When
  it stops Gothmenes returns with Elminster in his whip and summons the
  party to the pool (script 65): it will not trade (NO, NO), the bearer
  countermands the summons (YES; at the cost of nearly all its Hit Points
  many servants are driven back), and in script 75 the dracoliches, bits of
  Moander, minions and Pets of Kalistes, then Xoham and his beholders,
  then Gothmenes (the party will not bargain for the crystal's word,
  AGREE? NO, so only a few minions answer him) fall (a stand-in), and the
  crystal crumbles: "Gothmenes has fallen, and Bane's crystal is dust. You
  have won." Elminster speaks from far away and the party stands again in
  peaceful Phlan (script 24), where the route ends.

Since 2026-11-15 the route's casters memorize spells of every level they
have slots of (Pools of Darkness's levels 6 to 9 among them) and raise the
dead with Resurrection too; a member knocked out (0 Hit Points), whom
resting does not heal, is cured in camp as a player would. In the Temple
of Tyr the party rests in the scribe's office (9,4, entered facing west:
"IT MIGHT BE A GOOD PLACE TO REST"), the one room whose camps the patrols
leave alone until the bells ring (the cluebook's location 5), after the
vampire priest and again before the braziers.

A step's event that sets cell 3h to FFh (stairs, the steading's barred door
and its gate) holds back the step that ran it, as the original's movement
state does (Script memory, above). Until 2026-11-14 OpenGoldBox stepped on
anyway, so stairs landed a square beyond their landing and a held step could
wrap round the map's edge.

Stand-ins (2026-11-16, darkness17): two, the fights the test wins, down
from nine (The stand-ins measured, below; The sides and the round limit,
below; The last stand-ins, below). Kalistes' fight in her egg room (84:
2,6), a stand-in until then, the route's player now wins, wearing the
Rings and Cloaks of Protection the party has found (33 plays of 240). The
finale's first fight (script 75: the pool's dracoliches, bits of Moander,
minions and Pets of Kalistes, whose breaths and Charm Person beat the
party, its bearer of the crystal near death from countermanding the
summons; none of 600 plays wins). And Gothmenes' last stand at its end,
under Bane's power that bars all magic (none of 600 plays wins; The sides
and the round limit, below). Xoham's beholders between them, a stand-in
on the beholders' rays, the route's player now beats with no one lost (14
plays of 600; 425 win, most with members lost, whom no camp can raise
before the finale's end, so the player reloads a costly win there). The
fights the route's player now wins: Thorne and
his seven red dragons (34: 3,6), the Aerie's blue dragons (33: 3,14), the
Chambers of Marcus (67: 0,12 south; script 77), the heart's magnetic iron
golems (53: 9,3) and Tanetal's last stand (53: 5,12), the party hasted for
both, the Iron Wall's fourteen Minions of Bane (64: 15,5) and the palace's
rakshasas (65: 4,4); where few plays win (Thorne, the blue dragons, the
rakshasas) the player reloads until one does (`RouteBeat::reloads_until_won`, 240 more plays at
most), and members killed in a won fight are raised after it, as a player
raises them. Before them (2026-11-15, late) the Vaasans (37: 13,8), Myth
Drannor's altar and its temple (the blessed bolts, above) and the Large
Wound's swarm of bits of Moander. The route's party carries the
game's own starting equipment (ORIGINAL-RULES.md, *Pools of Darkness*).
The route plays to the win (2026-11-16, `darkness_route_ends_the_game`): it
passes when every beat plays, the win is shown and no member is dead; it
ends in Phlan, where script 75 leaves the party after Elminster's last
words.

The route's rest takes a Darkness camp entry that sets cell 2Bh (the rest
period) for one that refuses the rest, rests a night and cures the badly
hurt; the tower's camp entry sets it in its quiet rooms too, where the rest
finishes, so there the route rests on until the party is whole
(`RouteBeat::rest_through_period`, 2026-11-15; the earlier beats keep their
night). With it, a camp whose wanderers only pass by (the Sentinel Halls'
chanting minions, who stop breaking camps once they have passed three
times) is made again, up to twenty in a row (2026-11-16).

Myth Drannor's way out (2026-11-15, late) needed the 256-color engine's
LOAD CHARACTER as the original has it: it reads its argument's low seven
bits only (eclvm.pas, EclLoadCharacter), where OpenGoldBox read the high
bit as *Pool of Radiance*'s store of the record and refused the script
(ECL1:66 0x8658, after FIND NPC); *Treasures* shares the engine.

What the route needed besides (2026-11-14): a Darkness party saves and
loads, its levels to 40, on the area map (whose script, 17, does not load
the map the party left) and after a level drain (a character's spell
choices, training and hall entries replay in the order they were made, as
a drain may bring it back to a level it has left); a restored machine
leaves ACTION MENU to the machine, as a new one does. And (2026-11-15): the
same spells chosen again at the same level keep one level-history entry,
for the nightly choices of spells of nine levels grew the save past the
campaign save's 16 MB by Zhentil Keep; and the training hall's cap of
level 15 is the SRD's only, so a character under the original rules trains
as far as its game's tables go.

A fight's result reaches Darkness's scripts as compare flags: they test
it with IF right after COMBAT, which its PostCombat sets to "equal" when the
party won or came through and "not equal" when it fled. Until 2026-11-14
OpenGoldBox left the flags as they were, so a won fight could read as lost
and move the party back a square (the walk passed all the same).

On the original-rules round (its effect numbers, stronger monsters,
2026-11-15, late): the vampires that roam the Moonsea's north shore drain
two levels a bite, so the route flees them on the way to the Hill Giant
Steading, and its camps cast Restoration on a drained member when a cleric
has it. Its bows and crossbows shoot (arrows and quarrels are *Secret*'s
item types 30 and 12 in Pools of Darkness; OpenGoldBox had looked for the
earlier games' 73 and 28, so no bow found its arrows), and its dragons
breathe (ORIGINAL-RULES.md, *Pools of Darkness*). With them the route rests where the scripts let it, as a
player does: the Fire Giant Cave's camps rest only in its quiet rooms
(script 32's camp entry: events 34 and up, the cluebook's location 13),
so the party rests in the room north of the passage (13,2); the Aerie's
only in its disused rooms (events 20 and 21, the cluebook's 9), so between
the nests it steps into the north-east one (13,5) twice and rests in the
north-west (3,2) and south-west (2,12) ones, the last before the blue
dragons and again before the dracoliches. Where a camp is refused (the
camp entry sets cell 2Bh) its clerics raise the dead and cast their cures
instead, and they keep Raise Dead ready. Kalistes' Parlor is entered the
cluebook's way, by the kitchen's trapdoor (The win and the route, above).

The items menu's JOIN (viewchar.pas, JoinItem) is in the party since
2026-11-15 (`CampaignParty::join_like_items`): every other stack of the
member's like the item (the same record and name; a stack in the original,
with fewer than two charges) joins it, up to 255 a stack. The game's Items
screen has no Join button yet; the route joins its blessed bolts.

The gaze spider's paralyzing gaze and the vampires' charming gaze (effects
114 and 124) are an option since 2026-11-15, the user's decision: the
original registers their handlers but never runs them, so off (the
default, which the route plays) they do nothing, and on they work as the
handlers describe (CONFIGURATION.md, `[darkness_gazes]`; DEVIATIONS.md).

The route walks straight to its goals, where a player meets the wandering
monsters of every area (the cluebook names them for each) and, in the Web
Dimension, gains "as much experience as you can" before Kalistes. So the
test gives each member experience at the training points a player would
come to stronger, a stand-in for those fights (2026-11-15): 300,000 in
Limbo after the Aerie, 300,000 at Zhentil Keep's hall, 400,000 in
Limbo before the Web Dimension and 500,000 at Zhentil Keep's hall again on
the way out to Myth Drannor (the Web Dimension's paths and the Descent),
and the party trains up to two levels a
visit (each class's award stops short of the level after next, as
training keeps it). It reaches Kalistes at levels 15 to 18 (the
magic-user 17, with eighth-level spells).

The route's dice (2026-11-15, late): each beat plays with dice of its own,
seeded from where it plays, what it answers and how many like it came
before (`beat_dice_seed` in the route harness, Pools of Darkness's route
only). One stream had run the whole route, so any engine change that
rolled a die more or less (the rebase onto Treasures' Charm Person and Fix
did) reshuffled every beat after it, and the route fell where it had won.
Now such a change reshuffles only the beats it touches, and a save resumed
with `OPENGOLD_ROUTE_RESUME` plays on exactly as the whole run does. The
dice are as fair as before: it is a player's reload before each step. The
script machine takes a seed only between scripts, so a beat that answers a
waiting menu (Elminster's) keeps the stream it has.

What that and the rest of any length (round 4) took (2026-11-15, late):
Zhentil Keep's journey is two beats, the last step into the city after a
rest on the area map; a member a basilisk turns to stone is brought back
by a magic-user's Stone to Flesh, memorized first and cast in camp
(OpenGoldBox cast it only in a fight; the later games' table lets it be
cast out of one, and their FIX uses it too, camp.pas); the Web Dimension's
ruins are walked unrested after Cynthia (a camp there drew Kalistes' pets,
iron golems and giant spiders); and the lower tower is played as above.

Pools of Darkness's computer casts as *Treasures*' does since 2026-11-16:
the two games' `combatai.pas` and `cbtacts.pas` are the same (SelectSpellToCast,
PickCastableSpell, RateSpellTarget, AimAreaSpell, and a SelectItemToUse that
uses nothing), so the memorized spell of highest AI weight that has a
target is cast first (ORIGINAL-RULES.md, *Pools of Darkness*).

Where it stops (2026-11-16, night): at the win, in Phlan (map 17, script 24)
after Gothmenes falls. What is left is to win the stand-ins by play: the
party the route brings is weaker than an original player's, who meets the
wandering monsters of every area (the end's palace, cave and city have
theirs: beholders, death tyrants, vampires, Bane minions) and their
treasure, and the Cave of the Beholder's missions for the Chancellor and
its captives, which the route skips. Scripts and the scripts
they go on to (`opengold_scripts --inspect DARKNESS ECL1.DAX N`, NEW ECL):
19 to 48; 48 to 35 and 49; 35 to 41, 68, 69; 68 to 69 and 71; 69 to 72 and
18; 66 and 70 from the area map; 70 to 67 and 76; 67 to 75 and 77; 64 to
65 and 74; 65 to 73, 74, 75. `OPENGOLD_ROUTE_JUMPS` names a beat that puts
the party on a square it does not stand on (a step the beats before it did
not walk). `OPENGOLD_ROUTE_SAVES=PREFIX` with `OPENGOLD_ROUTE_SAVE_AT=N,N`
saves after those beats (PREFIXN.sav, for `OPENGOLD_ROUTE_RESUME`) in one
run; `OPENGOLD_FIGHT_TACTIC=N` plays every try with one tactic,
`OPENGOLD_FIGHT_CELLS` adds where each fighter stands to the fight lines
and `OPENGOLD_FIGHT_GEAR` prints each member's memorized spells and items as
a fight begins.
`OPENGOLD_ROUTE_BEATS` and `OPENGOLD_SECRET_FIND` plan each step: the
planning aid answers NO, so it never takes stairs a question offers; find
the stairs' squares by their walls of type 7 and step them in a beat. A
script's events are found by its step entry's ON GOTO on cell 2Dh's low six
bits: event e runs the target at index e, counted from 0
(`opengold_scripts --inspect DARKNESS ECL1.DAX N`).

### The stand-ins measured (2026-11-16)

Each stand-in's fight played from a save before it
(`OPENGOLD_ROUTE_SAVES`, then `OPENGOLD_ROUTE_RESUME` with
`OPENGOLD_ROUTE_PLAY_STAND_INS` and `OPENGOLD_FIGHT_SCORES`), 120 plays each
(40 dice, three tactics); "wins" are plays the party wins, "clean" those it
wins with no member dead:

| Fight | Before | Now |
|---|---|---|
| The Aerie's blue dragons (33: 3,14) | 3 wins, 0 clean | 4, 0 |
| Thorne (34: 3,6) | 0 | 6, 2 |
| Kalistes (84: 2,6) | 0 | 0 |
| The Chambers of Marcus, Marcus's wave | 1, 0 | 88, 42 |
| The heart's iron golems (53: 9,3) | 0 | 3, 0 |
| Tanetal's last stand (53: 5,12) | 2, 0 | 5, 0 |
| The Iron Wall's minions (64: 15,5) | 2, 1 | 63, 18 |
| The palace's rakshasas (65: 4,4) | 0 | 9, 4 |
| The finale's first fight (75) | 0 | 0 |

"Now" is measured after the rebase onto the rest's rules of 2026-11-16
(campaign_rest.cpp). "Before" is the party of 2026-11-16 as it was. Now the route's
player rolls each character again at creation until its class's ability is
18 and its Constitution 17 (the creation screen's Roll, as a player rolls
for a party meant to finish the game; the original's Modify, which sets the
scores outright, OpenGoldBox does not have yet), so the magic-user, of
Intelligence 18, has its eighth- and ninth-level slots (TrimHighSpellSlots)
and the members start with 666 Hit Points between them where they had 553;
the magic-user, of Dexterity 18, climbs Myth Drannor's wall. The party rests
in Thorne's cave in the room the Ki-rin made safe (10,9, event 39; the
cluebook's 14), the one camp script 34's camp entry lets rest while Thorne
lives, memorizing Resist Fire there.

The iron golems (six and four large ones) heal from fire and take no other
spell (EffIronGolem; the clerics' maces, crushing weapons, do them no harm
either), so the route's player casts no fire where one stands and, where only
golems stand, no spell against them and no Quick magic but its magic-user's
Lightning Bolts, which slow them, its clerics taking up their maces and
shields (with spells cast as Quick casts them, 1 play of 120 won). Since
2026-11-16 (night) the magic-user memorizes Hastes instead in the camp before
the heart and hastes the party as it steps up to them (below).
The finale's clerics cure the party before it (the pool's waters hurt the
crystal's bearer), which no play still turns. The web's paths to Kalistes'
Parlor are fled (`RouteBeat::fleeing`): with the rolled party a band of Pets
of Kalistes came on the party there, and the fight it won put it off its way.

No engine fault was found in these fights: the iron golems, the dragons'
breaths and the computer's spells play as the reconstruction has them.
Experience is not what the three stand-ins lack: a late fight gives each
member some 9,000 (the Iron Wall's minions) to 74,000 (the finale), where a
paladin's level costs 350,000, so wandering fights would not raise the party
a level before the end.

### Tanetal re-planned, and the beholders' rays (2026-11-16, night)

Pools of Darkness's AI never steps into Blade Barrier's blades
(ORIGINAL-RULES.md, its cloud step), and the party's members on Quick step
so too. Under the earlier games' step the bits of Moander of Tanetal's last
stand walked into their own cultists' blades and were cut down (in the plays
it won, every bit of Moander was cut); now they keep out of them, and their
poisonous blows, which slay a member that fails its save, beat the party:
none of 1,080 plays won, whatever its tactic. Protection from Evil on each
member won none of 360 either; the clerics memorizing Blade Barrier and
casting it on the bits won 1 of 193.

What wins is Haste. Cast out of a fight it hastes the whole party for 3
rounds and 1 a level (its spell row; one of level 16 to 31 ages no one,
Bugs), so the route's magic-user memorizes Hastes in the camp at the
Entrance to Heart, the heart's last, and casts one as the party steps up to
the iron golems (9,3) and one as it steps up to Tanetal (6,12)
(`RouteBeat::ward_each_with`, cast on the first member). Hasted, the party
wins 87 of 120 plays of the golems (3 unhasted) and 23 of 120 of Tanetal's
last stand, members dead in each, raised after it; neither needs a reload
now. The Lightning Bolts the magic-user took for the golems gave way to the
Hastes.

The beholders' rays play since 2026-11-16 (ORIGINAL-RULES.md). The route
meets beholders only in the finale (script 75): Xoham, two death tyrants, an
elder orb and four beholders, after the pool's servants. The party wins 114
of 120 plays, but in each the rays take one to five members (disintegrated,
stoned or slain), so the test, which wins a fight whose best play costs a
member (the stand-ins' rule), wins it: a stand-in. Hastes memorized in the
city's inn do not last to it, the Quick magic-user casting them at the Iron
Wall's minions; a Haste on the party at the Iron Wall wins its second fight
in 91 of 120 plays where 61 won.

Fear (the spell, and a beholder's) now forces its flight while the fright
lasts, as SpellFear and EffFear have it (ORIGINAL-RULES.md); until then the
next turn's morale check ended it. The route still wins as above with it.

### Kalistes and the finale tried again (2026-11-16, late)

**Two differences from the original found** (made since: The sides and the round limit, below). Pools of
Darkness's SETUP MONSTER takes a fourth operand OpenGoldBox reads and
ignores: the sides (bit 0 north, 1 east, 2 south, 3 west) its monsters come
from. The original deals the monsters out to those sides in turn from the
east, each group a square off the party's (at least one even where the
script asks for 0) and facing it (eclvm.pas, EclSetupMonster; combatst.pas,
PlaceAllCreatures; Unlimited Adventures' placement, which OpenGoldBox has,
deals them the same way). OpenGoldBox stands every monster ahead of the
party, and where the script asks for distance 0 a band too big for its
half-square spills round the party, so that Kalistes' drow stand among the
members. Kalistes' egg room names north, east and south (7, distance 0);
the finale's servants come from the east and south (6); script 75's later
fights from all four sides (15). *Treasures* reads the same operand (its
`hostileFacingMask`).

Dealt out so (built and measured on 2026-11-16, then taken back out), many
of the route's other fights stood monsters in squares the party cannot
reach (behind a wall of the map, or on the web's paths and Moander's body,
where gaze spiders and bits of Moander stay put), and 228 fights of one run
ended in stalemates; the route then lost its way at Moander's body (52: 0,0,
"A fight ended without a result"). The original ends such a fight by its
second rule OpenGoldBox lacks: after 15 rounds with no attack made
(combatst.pas, CombatStart, `fightEndRound` 15; cbtacts.pas, MakeAttack,
which sets it 15 rounds on; combat.pas, CombatMain and EndOfRound). Secret
and Gateway have the same limit (their `roundLimit`). The two belong
together: the sides first would leave the route's fights to the test's
stalemate handling. Both are a change to every newer game's combat (and its
rules interface, which has no outcome for a fight that just stops), for the
rules' agent rather than the route's.

**What was measured** (120 plays of each fight from a save before it, three
tactics; 0 means no play won; the monsters dealt to their sides as above
for every row, which changes nothing here):

| Fight | Plays won |
|---|---|
| Kalistes (84: 2,6), as the route plays it | 0 of 120 |
| Kalistes, the party scattering (below) | 0 of 900 |
| The finale's first fight (75), as the route plays it | 0 of 120 |

The party scattering was a player's answer to the eight Ice Storms (an
Ice Storm takes everyone within 2 squares of its aim, no save, 3d10; the
computer aims it where it takes the most enemies and none of its own side,
cbtacts.pas, AimAreaSpell): while an enemy is casting, each member shot or
struck a caster it could reach (a blow loses the spell), else stepped away
from the others until none stood within 4 squares. It works as meant: most
wizards find no square that takes two members and abort their storms. But
the pets of Kalistes reach the party in the first round and their poison
kills where a save fails, Kalistes' six weapons do some 90 a round, and her
champions strike every member who steps away; no play of 900 won. (Built and measured, then taken back out with the
sides above, the egg room being its only use.)

**How far the party is from winning them.** To see whether any tactic or
spell could turn these fights, the two were played with the party made
stronger outside the rules (a throwaway build, not kept): every member
Mind Blanked (no charm, hold, confusion, feeblemind or fear), 8 better on
every saving throw and 6 better in Armor Class (more than the Rings and
Cloaks of Protection +4 a player finds give), all harm to the party halved
(as if its Hit Points were doubled), and its own blows and spells doing
twice the harm. Each alone and all together, no play of 120 won either
fight. So Mind Blank against the dracoliches' Charm Person, Globes of
Invulnerability (the magic-user's own only: a spell on the caster, magic.pas
and spell 112's table row), the order of march (it sets only the ranks of
the party's own square) and the casters' spells cannot turn them: the
finale's 29 servants (5 dracoliches, whose breath does their 80 maximum Hit
Points, 4 bits of Moander, 11 Minions of Bane breathing for 45 and 9 Pets of
Kalistes casting Disintegrate, Death Spell and Feeblemind; script 75 brings
these after a countermand that succeeds, 34 or 40 otherwise) and Kalistes'
21 put more death into the first two rounds than six members survive. The
stand-ins stay; what would turn them is a party of the original player's
make (two or three magic-users or more casters against one here, and the
items of the whole game; the cluebook lists them by area), not a tactic.

Seen on the way, for a later round: the finale's crystal-bearer goes in at
2 Hit Points whatever it had (the countermand's DAMAGE takes all but 2), so
the clerics' cures before it (`cure_to_full`, eleven Heals on him in one
play) spend Heals for nothing; and after the palace's rakshasas the bolts'
bearers keep their crossbows readied (`ready_types`), so in the fights after
paladin 2 and the ranger strike with them.

### The sides and the round limit (rules round 6, 2026-11-16)

SETUP MONSTER's sides and the 15-round limit are played now, together
([ORIGINAL-RULES.md](../ORIGINAL-RULES.md), Combat, item 10): the monsters
are dealt to the sides the script names, facing the party, and a fight in
which no one attacks for 15 rounds ends won, the monsters left standing
leaving unpaid. The run that had 228 stalemates with the sides alone has
none: such fights now end by the limit.

The route wins on them. What changed in its play:

- Thorne's dragons (34: 3,6) come from the east, south and west (his SETUP
  MONSTER's 14) and surround the party: 9 of 1,200 plays win, most at the
  cost of both clerics, where 6 of 120 won before. The kept win had left both
  clerics dead, no one to raise them, and the party fell at Moander's body
  (52: 0,0) with four members.
- The drow temple's statue (69: 8,7): 26 of 300 plays win, but the best of
  the first 120 again cost both clerics.
- So the route's player reloads a won fight that cost two members or more,
  up to 240 more plays (Pools of Darkness's route only), as a player keeps a
  cleric to raise the dead. Thorne is won with the magic-user lost and
  raised.
- The finale beat (script 75) was a stand-in already; now its last fight,
  Gothmenes and the minions who still answer him, is won by the test too:
  they come from all four sides (15), and the Minions of Bane's breaths from
  every side beat the party in all 120 plays, where before they came at it
  from ahead and it won. Stand-ins: Kalistes' egg room (both fights) and the
  finale beat (its first fight and Gothmenes').

Then the Hit Points of AdjustLevelHitPoints came with it (a level past a
class's Hit Dice adds its fixed amount, no Constitution bonus): the default
party starts with about a fifth fewer Hit Points (its paladin 102 where it
had 130 before Thorne). The blue dragons of the Aerie (33), lost on that fix
alone, are won again, but Thorne's dragons, from three sides, now beat the
party in all of the beat's 360 plays (2026-11-16, late): the route stops in
Thorne's chamber (34: 3,6). Next: measure Thorne on more dice, and re-plan
(a stronger party of the original player's make, or Thorne a stand-in
again, for the user to decide).

**The party made on Modify Character** (rules round 6, the coordinator's
decision of 2026-11-16). In every game a player may set a new character's
scores and Hit Points with Modify before its first experience
([ORIGINAL-RULES.md](../ORIGINAL-RULES.md), Modify Character), and players
did, as the cluebooks assume. The route's player now does: each member's
scores are raised to its race's and class's highest and its Hit Points to
the game's highest, a step at a time through the rules' own Modify
(`opengold::test::modify_to_the_highest`, after creation, before anything
else), in place of rolling again until the class's ability was 18 and the
Constitution 17. The party starts with 135 Hit Points a paladin, 138 the
ranger, 100 a cleric and 69 the magic-user (2026-11-16), every score at its
highest (the magic-user's Intelligence 18 keeps its eighth- and ninth-level
slots). With it:

- Thorne's dragons (34: 3,6), from three sides: 130 plays of 360 win, 8
  with no one lost (none of 360 before); the route wins it with no one lost.
- Kalistes' egg room: the spy's fight after Kalistes is won by play (149 of
  600); Kalistes' own stays a stand-in (4 of 840, each costing three
  members or more).
- Xoham's beholders (the finale): won by play with no one lost (above).
- Gothmenes' last stand stays a stand-in. Its script sets cell 2
  (antiMagic) before it, which OpenGoldBox now plays
  ([ORIGINAL-RULES.md](../ORIGINAL-RULES.md), *Pools of Darkness*, Its
  scripts' antiMagic): no spell and no Use on either side. Gothmenes, two
  green dragons, eight Minions of Bane and two bits of Moander come from all
  four sides (SETUP MONSTER's 15), the minions' lightning three times each
  (their 45 maximum Hit Points, half on a save) and the dragons' poison
  three times each (72), against a party whose bearer of the crystal stands
  at 2 Hit Points: none of 600 plays wins, the party with magic barred
  (none of 120 before the bar either). Tried and dropped: the members
  stepping apart before they close in, so that a breath takes one member
  where it took several (none of 600 won); the monsters reach the party in
  its first turns. No reasonable play of this party wins it.

### Historical stand-in investigation (darkness17, 2026-11-16)

The unsuccessful tactics and counts below predate the November 17 fixes and
verified finale plan described above. They are historical diagnostics, not
a claim that Gothmenes remains undefeated by the current replay.

**Rules found and played** ([ORIGINAL-RULES.md](../ORIGINAL-RULES.md),
*Pools of Darkness*):

- The antiMagic cell (2) bars casting out of a fight too: the camp and Cast
  menus refuse with "<name> cannot cast spells in this area" and the items
  menu offers no Use. Only the finale's last stand and Dave's Challenge
  (scripts 39 and 40, after the win) set it, so the route never camps under
  it.
- The finale's last words ("A FOG OF PAIN LIFTS", ECL1:75 0x95BF) set each
  member's status to okay and its Hit Points to their maximum; a member the
  last stand left dead stands up again (it threw "Unsupported script HP
  change"). The script counts from cell C4h, which Xoham's fight left at the
  beholders' number, so the original heals the members from the fourth or
  fifth on only. Since 2026-11-17 the final combat reply resets the counter
  for the recognized DOS 1.1 recovery loop. `darkness_tests` now records
  actual dead campaign vitals for the first and last members and checks
  that all six recover. Its earlier display-only casualties were insufficient.
- The scripts' to-hit cells were not read: cell 1 (monsterHitMod) adds to
  the monsters' attack rolls, cell 2Fh (partyHitMod) to the party's, and
  the fight's end clears them (`fxengine.pas`, AttackRoll). Gothmenes' last
  stand sets them to 6 and F9h when the party will not bargain (8 and F5h
  when it will): the monsters 6 better, the party 7 worse, on top of the
  bar on magic. The route's other fights that set them (the Descent's
  slaves, the Temple of Tyr's catacombs, Moander's heart) it still wins.

**What the cluebook and scripts offer.** Read with `opengold_scripts
--inspect DARKNESS ECL1.DAX 75` and 82:

- Script 75 has no way round its three fights. Not countermanding the
  summons brings all 40 servants (11 dracoliches, 9 bits of Moander, 13
  minions, 7 pets); a countermand brings 34, or 29 when a d25 roll is below
  the bearer's Wisdom (cell 416h). Fleeing the servants brings Gothmenes'
  "RETURN!" and all 40 again; fleeing Xoham, minions who block the retreat
  and then the beholders again; fleeing the last stand, the larger band
  ("REFRESHED BY A DIP IN THE FELL POOL"). Bargaining for the crystal's word
  (AGREE? YES) and saying it is the cluebook's lethal trap (every member
  slain); saying nothing or another word brings the larger last stand
  (Gothmenes, three green dragons, eight minions and three of creature
  record 4 where the other has two bits of Moander, the monsters 8 better,
  the party 11 worse). The route's NO is
  the easiest way.
- Kalistes' egg room (script 82, 0x993E): once the commotion has been
  raised (cell 20Bh bit 7), coming back into the room brings "KALISTES SPOTS
  YOU AND ATTACKS": the same band, from ahead at distance 1 instead of from
  three sides. Played from a save (the bit set by the test), it won none of
  126 plays, so a player gains nothing by fleeing her first fight.
- The cluebook's items by area (its Major Magic Items) are what an original
  party wears at the end: Rings of Protection +4 (the Temple of Tyr, Dark
  Phlan, the palace), Cloaks of Protection +4 (Dark Phlan, Silk), Bracers,
  Boots of Speed, +4 armor. The route had taken only the items its beats
  named.

**The party wears its protection.** Since darkness17 the route's player
puts on every Ring (type 59) or Cloak of Protection (75) a treasure offers
that raises a member's protection, in place of a worse one, two rings a
member (`take_protection`; companions, who leave, take none): the treasures
on its way hold Rings of Protection +1 and Cloaks +1 and +3, so by Kalistes
most members wear two rings +1 and the paladins and ranger a cloak. With
them:

| Fight | Before | Now |
|---|---|---|
| Kalistes (84: 2,6), 240 plays | 0 (4 of 840 before the rings) | 33 won, none with no one lost |
| The spy's band after her | 149 of 600 | 156 of 240, 27 with no one lost |
| The finale's servants (75) | 0 of 600 | 0 of 240 |
| Xoham's beholders | 425 of 600 | 174 of 240 |
| Gothmenes' last stand, with the to-hit cells | 0 of 120 (without them) | 0 of 600 |

Kalistes is no longer a stand-in: the player reloads her fight until a play
wins (`reloads_until_won`), the member lost raised after it. Upper bounds,
measured from saves and not kept: every member in a Ring and a Cloak of
Protection +4, more than the whole game holds, wins Kalistes in 156 plays
of 240, but neither of the finale's stand-ins (none of 240 each). Those two
stay: no reasonable play of this party wins them.

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

### Characters from Secret

Its LoadChar reads a *Secret* character's 439-byte record and moves it into
its own 510 bytes in thirteen pieces (`saveload.pas`, the legacy storage): the
name, scores and Exceptional Strength; THAC0 to the known spells, whose 117
flags keep their numbers; the saves, movement and levels; the thief skills;
the platinum, gems and jewelry, the levels and those before a change of
class; gender to experience; the class mask, rolled Hit Points and spell
slots; the portrait and icon; the items' count to the magic resistance; and
the fight's bookkeeping. The memorized spells, the copper to gold and the
levels a drain took are left out, and the race is numbered one lower. Its
items are *Secret*'s records as they were, but a bundle (type 73) and what it
holds, boots of type 45 made +2 and a cloak of type 58 given displacement
(effect 89). The record's byte 230, *Secret*'s attack level, lands on the
known flag of Fire Touch (118), which OpenGoldBox leaves unlearned
([ORIGINAL-RULES.md](../ORIGINAL-RULES.md), Bugs). OpenGoldBox brings one
across from a *Secret* save on the party screen (*Add from Secret*,
`secret_items_for_darkness`, 2026-11-16).

### Items

An item is 17 bytes: the last 17 of the earlier games' 63, without the name
area. In order: the type, three name words, the magic bonus, the saving throw
bonus, readied, the name words still hidden, cursed, the weight (two bytes),
the count, the value (two bytes), then three bytes of charges, effect and
special power, or a scroll's three spells. `MON1ITM` holds each creature's
list. `ITEM0` holds one record of 254 definitions, numbered from 1, which
TREASURE and the shops copy into a new item. OpenGoldBox reads such a list as
the earlier games' items with no names (`decode_items(bytes, game)`).
Its item types are numbered as *Treasures*' are (a long sword 18, plate
mail 36), not as the earlier games' labels (where 18 is a halberd), so
every item, plain or magic, is named by `GAME.EXE`'s words: the lists,
`ITEM0`'s definitions and the monsters' items (`names_every_item`, the
creature catalog; 2026-11-16). Until then a plain long sword showed as a
halberd.

### Item types

`ITEMS` holds 128 types of 16 bytes, the earlier games' layout, but with no
header: type 0, empty, starts the file (`decode_item_templates(bytes, game)`).
