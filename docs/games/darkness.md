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

A fight that ends without a result fails the walk. A monster still charmed
onto the party's side as a fight ends leaves as one that ran, worth neither
experience nor treasure, as the original's CombatDoneCleanup marks it.

Still to come: the evening and other skies; the combat ground tiles
(`DUNGCOM`, `WILDCOM`); sound effects other than the footstep, and the music
PLAY SOUND starts; the opening and closing films in the game; the startup
menu's button.

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

One stand-in: the Vaasans who get through, seven mages and twelve
warriors, hold the party with Hold Monster before it acts and slay the held
with every dice the route tries, so the test wins that fight. A player
would gear the party with the cave's treasure and ward it first; the
route's player takes no treasure but wands. Until the route reaches the win
it passes when it plays every beat to the Temple of Tyr (map 50).

A fight's result reaches Darkness's scripts as compare flags: they test
it with IF right after COMBAT, which its PostCombat sets to "equal" when the
party won or came through and "not equal" when it fled. Until 2026-11-14
OpenGoldBox left the flags as they were, so a won fight could read as lost
and move the party back a square (the walk passed all the same).

Where it stops (2026-11-14): back in the temple (map 50, 6,5) with the
Lamp of Vigilance. Next: up the stairs west of (1,5) to (8,5), light the four braziers in the corner towers (event 4 at (8,0),
(15,0), (8,7), (15,7), answer LIGHT BRAZIER; cell 17Dh gets a bit each),
then the bell tower (event 9 at (11,3), (12,3), (11,4), (12,4), script 50
from 90A6h): ring the bells, and with journal entry 74 (the high priest's
journal, event 7 at (15,2), (15,5), and Laurellin's book in the acolyte's
cell, event 10 in the catacombs, found by searching) reunite Laurellin with
Brimwulf rather than kill her. Leave the plunder in the dining hall
(event 11 in the catacombs) alone: taking it earns the Curse of Tyr. Then
leave by the entrance (the step south from the hall, "DO YOU WISH TO
LEAVE?") and go west and counter-clockwise round the area map, the
mini-adventures on the way, to Gothmenes in Dark Phlan. `OPENGOLD_ROUTE_BEATS` and `OPENGOLD_SECRET_FIND` plan each step: the
planning aid answers NO, so it never takes stairs a question offers; find
the stairs' squares by their walls of type 7 and step them in a beat.

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
