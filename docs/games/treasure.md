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
turn, and the delay between steps in hundredths of a second: at each
step every color of the run takes the one before it, the first the last's
(its CyclePaletteRange). The overland maps use it, and OpenGoldBox turns
their colors while the overland is shown (since 2026-11-15,
`RolfTourSession::turn_colors`); *Pools of Darkness* ignores these bytes.

**Records that only set colors.** `PIC1` records 38, 47, 48, 57, 68 and 70 and
`PIC1A` records 78, 81, 89, 90, 100 and 119 are still pictures of no rows and
no cells that carry colors 32 to 255, with no color cycle. As it loads a `PIC`
picture whose number is in cell 4Eh, the game also loads the record cell 4Fh
names, only for its colors, which it lays over the picture's (its
portrait.pas, LoadAnimation, which reads the record into a scratch picture and
frees it at once). Script 16, the new game's, sets cell 4Eh to 10, a picture every
town's script shows, and each town's script sets cell 4Fh as it starts (89 in
Secomber's, 57 in Leilon's and so on), so that picture has each town's own
colors. OpenGoldBox draws the picture so since 2026-11-16
(`RolfTourSession::pic_frames`), for a script's picture and a monster's close-up
alike.

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

## Modify Character

The party menu's Modify changes a new character's scores, Hit Points and
name while its experience is still creation's 22,600 (11,300 or 7,533 for
each class of a multi-class character), with *Pools of Darkness*'s limits,
and a magic-user chooses its first- to third-level spells again, as its
Keep runs creation's choice anew (`chargen.pas`, `ModifyCharacterMenu`,
`InitKnownSpells`). OpenGoldBox's party screen offers it so
([ORIGINAL-RULES.md](../ORIGINAL-RULES.md#modify-character)).

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

The music plays where the original plays it ([later-sound.md](later-sound.md),
Where the games play it): `MUSIC.DAX`'s one piece over the title pictures,
and nowhere else, as its reconstruction starts it only in its `intro.pas`
and for a script's PLAY SOUND from 80h, which no script has. Its end plays
no film and no music: its `CONTROL.EXE` builds the film viewer's command line
and throws it away. The scripts' only sound is PLAY SOUND 10, the
footstep, which a step plays too (its explore.pas, StepForward, and eclvm.pas,
EclPlaySound): since 2026-11-16 the town view plays it from `ZOUNDS.DAX`
(record 9, a click of 132 samples) in place of the synthesized footstep of the
other games; the fights play `ZOUNDS.DAX`'s effects since 2026-11-15 (Bugs).
The romance plays since 2026-11-16 ([ORIGINAL-RULES.md](../ORIGINAL-RULES.md),
*Treasures*' romance). It gates nothing: a lover fights better, or worse when
its love goes unanswered (the cluebook's Falling in Love).

One of the walk's fights no side can finish (map 24 at 1,5): a caster with no
spells left and no way to reach the party stands across the battlefield from
members with no way to reach it. The walk leaves it as a player would, each
member walking to the field's edge and fleeing, as it leaves *Curse*'s and
*Pools of Darkness*'s. In Freezefire's lair (map 38 at 3,6) the black-robed
mages charm dwarves who fight beside the party onto their side; such an ally
ends the fight as one out of it, as an ally the party attacked does.

## What the route needed

The route (below) plays the story as a player does, and needed these of the
engine, each as the original does it:

- **COMBAT's flags.** The fight ends with the equal flag set when the party
  won and the not-equal flag when it fled (its PostCombat clears the flags,
  then sets them), and the scripts test them at once: Geildarr's chamber goes
  on to his second fight only on a win. A fight whose treasure the party
  takes is a won one. The other games leave the flags alone.
- **A spell cast outside combat.** A script waiting for one sets the last
  spell's cell (4Bh) to FFh and ends, as Cortarra does when the party chooses
  CAST before the two Siulajias. The spell cast is recorded there by its
  number in *Treasures*' table, and the square's event runs again, as the
  original's map menu runs the step and location entries after a cast (its
  NoncombatCastMenu); Cortarra's script reads the cell against its list and
  shows her for what she is. Detect Magic is a camp spell under the original
  rules for this (spent, with nothing more to lay).
- **PROGRAM 3**, Amanitas's word: the luck reading (`LUCK.DAX` 0 to 12) of
  the first Lucky Paper mission not yet done (cells 127h to 130h at 1 when
  done), 12 when cell 1F8h is FFh, and the crown's last words (0) when the
  luck state (cell 25Ch, which Freezefire's fall sets to 1) is 1, which then
  becomes 2 (its ShowNextLuckText). Its pages are shown together and wait for
  the player to go on.
- **PROGRAM 5** makes the party whole: feeblemind, blindness, disease and
  poison cured, the dead (but elves) resurrected, the stoned made flesh,
  every drained level restored and every Hit Point back (its RepairParty),
  then waits for the player. Until 2026-11-15 it left feeblemind and
  drained levels and raised the dead by Raise Dead, at its cost (Bugs). Freezefire's lair uses it.
- **PROGRAM 4, the Fix** (its CampFixMenu, since 2026-11-15), which the
  voyages run as the ship sails: nothing when no member is missing Hit
  Points. Otherwise the party rests while its standing casters work by
  their spell slots, not their memorized spells, which it leaves alone. The
  healing is an average for each slot of a cure the caster knows (9 a
  first-level slot for Cure Light Wounds, 17, 27 and 50 a fourth- to
  sixth-level one for the Serious, Critical and Heal) and a roll for each
  slot of those levels (1d8, 2d8+1, 3d8+3 and 4d8+3, a ranger's
  second-level druid slots counting as first-level); the rest lasts the
  longest caster's casting (a quarter hour a first-level slot, half an hour
  a druid's, an hour, an hour and a quarter and an hour and a half a fourth-
  to sixth-level one), less as the healing exceeds the wounds. After it the
  casters cure as their slots allow: stone made flesh, poison neutralized,
  the dead raised (resurrected, with Resurrection), curses lifted and sight
  restored, each member in turn, then the healing is spent on the wounded in
  the party's order. A second rest, as long as the cures still needed take
  (an hour and three quarters a resurrection, an hour and a half a stone to
  flesh, an hour and a quarter a raising, an hour a poison, three quarters
  of an hour a blindness or curse), cures what the first could not. Either
  rest is rolled for interruption as a camp is (cells 2Bh and 2Ch): from a
  script the Fix then stops ("Your repose is suddenly interrupted!"), in
  camp the camp's interruption script runs. It waits for a key only when it
  has something to tell. Since 2026-11-15 it also casts what the original
  casts with the highest slots: a sixth-level slot (Heal's, known for the
  first rest) cures a member's feeblemind, and its blindness with it,
  healing nothing ("is healed"; that member's blindness then takes no Cure
  Blindness), and in the second rest each seventh-level slot gives a
  drained level back by Restoration (an hour and three quarters each, an
  hour and a half a feeblemind), each member in turn; a Resurrection raises
  the dead as the spell does, losing nothing (it raised no one before). A
  cleric of *Treasures* stops at level 10, short of those slots, so only a
  character beyond its caps would cast them (`opengold_treasure_tests` makes
  one under *Pools of Darkness*' rules, its engine's). Its bugs here are
  fixed ([ORIGINAL-RULES.md](../ORIGINAL-RULES.md), Bugs).
- **The camp's opening** (`RolfTourSession::open_camp`, since
  2026-11-16): Camp runs the map's camp entry, which sets the rest's
  interruption cells, and tells the romance's scene if one is due, then the
  camp dialog opens with Rest, Fix and Crown as those cells allow (its
  eclvm.pas, EnterCamp, and camp.pas, CampMainMenu). A rest or a Fix from
  the open dialog runs neither again; Cancel leaves the camp, and the next
  Camp runs the entry again.
- **The camp's Fix and Crown** (`RolfTourSession::camp_fix`, `camp_crown`):
  Fix camps as a rest does and runs the Fix, where the original's camp menu
  shows it (unless every rest step is interrupted). Crown, shown once the
  party has Amanitas's crown (cell 3Fh) and until his last reading is given
  (cell 25Ch at 2), tells the next luck reading as PROGRAM 3 does. Since
  2026-11-15 the game's camp dialog offers them beside Start, as the
  original's camp menu (Save View Magic Rest Alter Fix Crown Exit) does,
  each only while the session offers it (`opengold_godot_treasure_camp`
  presses both on the town screen).
- **The tenth mission** is the council: no script sets its cell (130h). The
  council's script marks it done by setting cell 1F8h to FFh, and the luck
  reading is then 12 (go to the Ice Peak), as PROGRAM 3's order has it;
  Neverwinter's Northpier Shipping and Fireshear's dock read that cell.
- **Item types.** *Treasures* numbers its item types as its item words
  (its `ITEMS` and itemNameParts): a long sword is 18, plate mail 36, a
  shield 37, a mace 8 and a dagger 4. The earlier games' generic labels
  number them otherwise, so since 2026-11-15 every item of its lists and
  definitions is named by its own words, magic or not (Bugs).
- **The crystal battles** (since 2026-11-15). A script that fights over the
  Lucky Papers sets cell 3Dh before COMBAT; each enemy agent carries a
  crystal (effects 128 red, 129 green, 130 blue). The first of them to fall
  gives the party its crystal, and every other one still fighting "smashes a
  crystal" (its EffRedCrystal and ShowCrystalSmash, the cluebook's Crystals):
  cell 3Ch gains its bit (1, 2 or 4) and cell 3Eh tells which (0 to 2),
  which the script reads after the fight. The camp names the crystals held
  above the members' details (its CampMainMenu: "The party has acquired
  the..."), as `RolfTourSession::camp_crystals` gives them.
- **Byte cells.** *Treasures* stores an arithmetic result in as many bytes
  as its destination names (its WriteGbvm): a byte cell counting down from 0
  reads 255, as Luskan's script waits for. *Pools of Darkness* shares the
  routine; OpenGoldBox keeps it to *Treasures* until that game's route
  needs it.
- **Treasures' own spell table** (since 2026-11-15). Its 126 spells are
  *Pools of Darkness*' numbers, but its table (its gdconsts.pas,
  spellTable) changes 44 rows that OpenGoldBox read from the earlier
  games': ranges (Fireball, Lightning Bolt, Slow, Fumble and Ice Storm reach
  4 squares a level, Magic Missile and Sleep too), durations (Charm Person
  lasts an hour and an hour a level), what a save does (Curse has none,
  Silence's negates), when a spell may be cast (Slow Poison in camp only)
  and how the AI weighs it (Charm Person 5, Sleep and Stinking Cloud 4,
  Dispel Magic 1, the cures 0, so the computer never casts them).
  `original::spell(n, game)` gives *Treasures*' rows; Barkskin keeps the
  rules decided for it.
- **A character stands on one square** (its chargen.pas sets
  iconDimensions to 1; the height of its icon is only drawn). OpenGoldBox
  counted a tall one as large, so a weapon struck it with its dice against
  large creatures and Hold Person never held it. Since 2026-11-15 no
  character of *Treasures* is large.
- **Spells for persons only.** Hold Person, Charm Monsters, Hold Monster and
  Mass Charm take only a creature its flags call a person (record byte
  421, bit 2, which every character has), and Hold Person never a monster
  (its ApplyEffectToTargets); its size is no matter.
- **Creatures held back** (its LOAD MONSTER and JoinDelayedCombatants). A
  script that sets cell 44h before LOAD MONSTER holds those creatures back:
  they join the fight as that round ends (the ninth at the latest), unless
  the fight is over. The council's Krakens, sorceresses and dwarves come in
  three waves, the lair's black-robed men in six. A creature held back
  joins where the fight would have placed it, or on the free square
  nearest it that is not walled off from its enemies
  ([DEVIATIONS.md](../DEVIATIONS.md)).
- **The overland's own steps.** The engine keeps the square the party stood
  on before every step, whether or not it moves (its main loop sets savedX
  and savedY, cells 17h and 18h, before ResolveMovement). On the overland
  most squares are walled; the overland's search entry, finding the party
  where it stood, moves it itself by the square's terrain (the event's low
  four bits) and the ways out its high four bits allow: along the river in a
  boat, around mountains and deep water on foot. Until 2026-11-15
  OpenGoldBox kept the square only on a step taken, so the party could not
  leave the Smuggler's Dock overland.

## The route

`opengold_treasure_route` (`opengold_gold_box_games_tests --treasure-route`)
plays the story in the cluebook's order with its party (two paladins, a
ranger, two clerics and a magic-user, made under *Treasures*' rules, carrying
+2 weapons and +1 armor as every game's route does). A party brought from
*Gateway* carries that game's experience and money: the test gives it (a
stand-in, 100,000 experience and 10,000 gold at each of four visits to
Llorkh's hall, training the party to levels 8 and 9).

1. **Llorkh** (map 16): before each fight the party has its dead raised at the
   Temple of Dumathain (15,3) and rests at the inn (12,1), for the streets'
   camps draw the guards. It trains at the hall facing its door west (1,3),
   fights the Lord's men, Zhentil fighters, ettins, griffons and Banites
   about the city, then the Keep's courtyard (8,10), and goes down the
   stairs east of it (9,10) into Geildarr's Keep.
2. **Geildarr's Keep** (map 17): the black-caped man (11,5); Cortarra's twins
   (11,1), answered with CAST and Detect Magic, her fight, and Siulajia
   joining; Lord Geildarr's chamber (15,4), the skeleton and then Geildarr
   and his guards. Up the stairs (11,10 west), and out of Llorkh by the boat
   rentals (3,15), open now.
3. **Secomber** (map 19), down the river and overland (maps 57 and 56): the
   first crystal battle (13,15) and Amanitas (7,10), who gives the crown and
   tells of the ambassadors (PROGRAM 3), rested for at the Inn of the
   Eccentric Owl (0,11, facing south) before each, the magic-user's Fireballs
   memorized for the battle's sorceresses (the town's guards break a camp in
   the streets, and since the round-4 rules an interrupted rest leaves the
   spells unlearned).
4. **Leilon** (map 20), overland by maps 56, 54 and 53: the drugged dinner at
   the Rutting Buck (9,7), rested for, the clerics memorizing Resist Fire
   for every member, and the escape from the barn by the sick trick (12,5).
5. Out of Leilon by the farmhouse and north (maps 53 and 52), where the
   Waterdeep 'guards' take the party to Fell Hatchet, who has it thrown into
   the harbor; the sea elves take it into **the caverns** (map 22). Past
   Mandoul's gateway and its guardians, the confusing room and a locked door,
   to the Zhentil captain's office (12,0), its members warded with Resist
   Fire against his hell hounds' breath and the badly hurt cured, without a
   camp (the hounds and guards break every one in the caverns), whose fall
   ends the second Lucky
   Paper mission (cell 128h), and up the rope ladder (15,1) to **the
   Smuggler's Dock** (map 21).

6. **Daggerford** (map 39), out of the dock's east gate and overland (maps
   55 and 56): resting at the Inn of the Safe Crossing (14,7) before each, the
   alliance's men at the door (12,13), the mages at their pot (11,14), the
   troops resting (6,7), the fire giants (0,0) and the headquarters (6,9),
   whose fall frees the town's men; at once to the south edge, where the
   Zhentarim slipping away to the Way Inn are caught (8,14), then back to
   the inn to rest before leaving by the edge (8,15), where the streets'
   patrols beat a worn party (since 2026-11-15).
7. **The Way Inn** (map 40), overland by map 56: resting a night in a room
   (13,1, upstairs 14,14) before each, so the casters have their spells
   again, the storeroom (12,2), the barracks (4,4), the inn's door (7,6), its
   hall (2,7), and upstairs the alliance's meeting (14,13), the alarm room
   (4,15) and Dauravyn Redbeard's captors (7,15), whose fall ends the third
   Lucky Paper mission (cell 129h). Six Hosttower sorceresses each hold a
   Lightning Bolt and a Fireball (their record, `MON1CHA` 114), which their
   AI casts first, and most often all six bolt the party before its
   magic-user acts, the bolts bouncing off the wall behind it; the route's
   player wins about one play in eleven, where its Fireball goes off at
   their row first, so it reloads that fight until it wins
   (`RouteBeat::reloads_until_won`, since 2026-11-15; until then the test won it).

8. **Yartar** (map 23), overland north by maps 56 and 54: resting in a
   house off the streets (11,9), the robed Krakens at the secret door
   (11,14) and the Kraken base (14,15), whose papers blame Triboar.
9. **Triboar** (map 24), by map 54, telling its guards the party has not come
   from Yartar: resting in a house (13,7), into the Kraken complex by its
   secret door (4,0), its offal heap (0,1), the ogres (3,4) and the
   Waterbaron's captors (1,7), whose fall ends the fourth mission (cell
   12Ah); the Lord Protector hears the Waterbaron (12,2).
10. **Longsaddle** (map 25), by maps 54 and 52: resting indoors (8,7) and
    going on in the morning, the rancher in the market (7,12) in its hours;
    the Krakens' ranches, Legson's (12,0) and its cellar (10,1), the Big Star
    (2,2) and its barn (3,1), the ettins' (2,9) and their pit (3,8), the ranch
    asking the party's business (14,10; 13,11) and its trap-door (15,12),
    the Harpells' mansion (10,4); then the ranchers who point to the barn
    (3,13) and the barn of statues (1,15), where Malchor Harpell ends the
    fifth mission (cell 12Bh). The Tower of Twilight is left alone, as the
    cluebook allows.
11. Back to **the Smuggler's Dock** by maps 52, 53 and 55, paying the
    gatekeeper's fee, and at sunrise the ship from the Mintarn pier (2,10).
12. **Mintarn** (map 26): trained at the Magical Arts hall (1,5, facing
    west), a level for every member, and before each fight its dead raised
    at the Temple of Waukeen (9,5) and rested in a house by the docks
    (6,6): the Tyrant
    told the party came to help (12,5), Southclaw Fort (Jagaerda joining,
    8,11; 11,11; 11,12; 13,13), the four warehouses (8,8; 7,7; 4,8; 2,11)
    and the invaders' headquarters (0,8), whose map ends the sixth mission
    (cell 12Ch). At 0600 the passenger dock (7,0, facing the sea) sails the
    party to Orlumbor as pirates carry Siulajia off to Luskan.
13. **Orlumbor** (map 27): the Temple of Talos (10,15) and Wrighttson told
    at his castle (11,3), the seventh mission (cell 12Dh); at 0600 the dock
    the party came in by (13,11, facing the sea) sails it to Neverwinter.
    Farr Windward is left alone.
14. **Neverwinter** (map 28): trained at the Hall of Training (6,11,
    facing west) where the experience allows; before each fight its dead raised at the temple on Neverwinter
    Square (9,10), where it rests (the gardens' Krakens break camps until
    the ambassadors are free, the cluebook's Resting): the gardener (6,13),
    the Krakens' bases (10,1; 15,11, its fire giant mages met with every
    member warded with Resist Fire) and the ambassadors Froal (10,0) and
    Wimeg (15,15), the eighth mission (cell 12Eh), then Lord Nasher (5,7).
    The route keeps clear of the guards who would find the planted orders.
15. **Luskan** (map 31), by map 51: resting in a house (11,6), Siulajia
    freed (11,1), each member warded with Protection from Evil and Dispel
    Magic memorized against her Banites' Hold Person, whose held are slain
    at the next blow (since 2026-11-15, when Hold Person took the tall
    members too) and Jagaerda's boat (6,6) to **Ruathym** (map 32), where,
    resting at the inn (9,8), the party clears the men around Captain
    Redleg (8,2; 10,5; 3,7; 4,9), frees him (3,8) without taking him into
    the party, so that he finds his boat alone and the party leaves
    unscathed, as the cluebook's Flint bids (with him, the pier's guards,
    captains and mages fall on the party), and leaves in his boat (15,4),
    the ninth mission (cell 12Fh). Back in Luskan: Redleg's farewell (8,3)
    and Jagaerda freed (7,4), the room where a captain argues with a
    sorceress (11,3) left alone (no mission asks for it, and its guards and
    mages leave the party all but dead), and, rested in the house (11,6),
    out by the gate (15,15) on the east road.
16. **Mirabar** (map 30), by maps 51 and 52: rested at the Dig Inn (4,11)
    after the road, the council (7,9) and the
    Krakens and sorceresses who burst in, in three waves as the dwarves
    come to help (won by the route's player since 2026-11-15), King
    Steelfist (15,4) and the Marchion (1,7),
    who sends his hammer Kriiador along. The council done (cell 1F8h at
    FFh, the tenth mission), the dead are raised at the Temple of Vergadain
    (2,13) and the party rests at the Dig Inn (4,11).
17. **Neverwinter** again, by the south gate (7,15) and maps 52, 51 and 53:
    Northpier Shipping (4,12) books the passage, 250 platinum, and at 0700
    the ship sails from the new north pier (2,11, facing west).
18. **Fireshear** (map 33), its leaders taken by the yeti: from the landing
    (6,15) through the streets and down the shaft at (0,14) to the
    Firemines' refuge (map 34, 5,15), where it rests; back up, and down the
    shaft at (5,12) to the mines' great chamber (7,7), where two waves of
    yeti and ice hounds and the grand yeti fall (cell 22Dh, the
    Shearmaster freed). Up the shaft at (1,11), rested, into the cavern on
    the sea (1,4, stepped into facing west): the yeti and the beholder (cell
    231h). From the dock (0,4) the dwarves sail the party free to the Ice
    Peak.
19. **Aurilssbaarg** (map 35): north and east through the streets to the
    path east (15,10), refusing the guards' escort to Tranjer Rolsk's trap
    and telling the Northmen the party fights Luskan (cell 238h at 1),
    without camping in the streets, where the black-robed men and Luskan's
    guards hunting the party fall on a camp (the cluebook bids it hurry).
20. **Bjorn's Hold** (map 36), the Northmen friends: rested by the
    feasters' campfire (11,5), to the pier (15,3) and Icewolf. The two
    parties of black-robed men (7,10; beyond the archway at 10,8) are left
    alone: no later script asks whether they fell, and the first beats the
    route's party with every dice.
21. **Icewolf** (map 37): the Northhelm, the Northmen's gift (10,0), worn by
    the first paladin; two visits to the Northman's training hall (15,6);
    Bleak Sky at Morning (10,8), told it is time, lets the party through the
    one-way door (8,9), where it rests and climbs west to the Ice Peak.
22. **The Ice Peak** (map 38): the wand of fireballs from the dead
    adventurers (12,7) for the magic-user; the avalanche's tunnel (9,10)
    into the great cavern (7,8), where the party rests, and west and north
    into Freezefire's lair (3,6): the black-robed men combing the hoard,
    then Freezefire risen as the Gem is pried from his claw, with the
    dwarves fighting beside the party. As the dragon dies the game is won
    (The ending). The black-robed spies and mages hold and slay the party's
    members from the first round (Hold Monster, Hold Person, Slay Living)
    and the masters slay the held: the route's player wins about one play in
    twenty, most members dead, so it reloads until it wins, then beats
    Freezefire with the dwarves; the ending's PROGRAM 5 makes the party whole
    ("One member surviving is all the party needs to win the battle", the
    cluebook says). Until 2026-11-15 the test won the black-robed men's
    fight.

*Stand-ins:* the experience and money at Llorkh's hall. The Way Inn's
last fight and the lair's black-robed men, which the test won until
2026-11-15, the route's player wins by reloading them until it does
(`RouteBeat::reloads_until_won`: past the 120 plays every fight gets, up to 600 more
while none has won), as a player reloads a fight the enemy most often wins
in its first round. Tried there on 2026-11-15, each over 300 plays, and
none changing the odds: Magic Missile at a sorceress casting, Charm Person
at the sorceresses first (the cluebook's advice for the lair), Fireball
then Charm Person, every member warded with Protection from Evil or Resist
Fire, the clerics' Hold Person at the casters, blows at the held first, and
long bows for the paladins and the ranger (which must put sword and shield
away to ready them; the Way Inn's Limantour Saddlery, store 16, sells
belts, robes, cloaks, boots, a mirror and boulders, not bows). The
sorceresses' records begin their memorized spells at the second slot, so
the original's sort past the first slot (Bugs) changes nothing there. The
route's player frees a member charmed onto the other side with
Charm Person, which in *Treasures* ends a charm with no save (its
SpellCharmPerson), or with Dispel Magic, which also ends a hold, its
casters memorizing them first (`RouteBeat::memorize_first_of`). The
council's Krakens were a stand-in until 2026-11-15, when OpenGoldBox took
up *Treasures*' own spell table, its characters' one square and its
creatures held back (What the route needed): the route's player then won
it with every member alive. The same rules made the lair harder: with every
character on one square the black-robed men's Hold Person takes the tall
members too, as the original's does. Tried and dropped at the Way Inn:
Silence, 15' Radius at the sorceresses and Charm Person at them, as the
cluebook bids in the lair; neither won it. Also tried in the lair: healing
before the black-robed men's last falls, as the cluebook bids; the fight
was lost before it.

Every walk is played step by step as the planning aid
(`OPENGOLD_SECRET_FIND`, with `OPENGOLD_FIND_YES` for a town's "ENTER ...?"
and `OPENGOLD_SECRET_PREFER` for a ship's destination) finds it, and a beat
that rests first rests a night even when the party is whole
(`RouteBeat::rest_for_spells`), so its casters have their spells again;
`OPENGOLD_WALLS` prints a map's walls, doors and events, and
`OPENGOLD_CURSE_SURVEY=N` surveys map N, and `OPENGOLD_ROUTE_PLAY_STAND_INS`
plays a stand-in's fights as any other, the test winning none.
`OPENGOLD_FIGHT_TRIES=N` plays a fight N times instead of 120 and, with
`OPENGOLD_FIGHT_TACTIC`, each try with dice of its own, to measure a
tactic's odds (with `OPENGOLD_FIGHT_SCORES`, `OPENGOLD_ROUTE_RESUME` and
`OPENGOLD_ROUTE_BEATS` for one fight). Its check passes when "You have won."
is shown with every member alive (`treasure_route_ends_the_game`).

*Played whole (2026-11-15),* on the original-rules round's stronger
monsters, the route wins the game in one run (about ten minutes). What it
took, as a player would play it:

- **Ruathym.** Redleg, freed, is not taken into the party (the cluebook's
  Flint): with him the pier's guards, captains and mages fell on the party
  and beat it; without him he finds the boat alone. Luskan's room of the
  arguing captain and sorceress (11,3), which no mission asks for, is left
  alone.
- **Temples and halls.** The party trains at Mintarn's and Neverwinter's
  halls on the experience earned since Llorkh, and has its dead raised at
  their temples before each fight; the ranger and a cleric had fallen in
  Neverwinter and gone unraised to Ruathym.
- **Fireshear's camps.** Its map's camp entry speaks before the rest, so the
  rest ends only once those words are answered; the route chose the casters'
  spells before then, and the next camp was refused for the spells still to
  be chosen (and no step ran an event). Not an engine bug: the game's rest
  dialog offers the spells whenever a rest has ended. The route now chooses
  them after the camp's words too.
- **The overland.** The route walks on hurt between towns, as before: a
  Fix in camp on the overland, tried, rolls the wandering monsters more
  often than it heals. Since the round-4 rules (2026-11-15) it answers the
  overland's ENEMIES ATTACK with FLEE (the script lets it go 2 times in 5),
  and where a band it cannot flee kills a member or beats the party (the
  fire giants and their mages after Mirabar), it meets the step again with
  the new dice a reload gives (`reseed_dice`), as a player reloads; a run
  off the field is its first tactic there, and a member left on a fled
  field counts as lost.
- **The round-4 rules** (2026-11-15). The Zhentil captain's hell hounds
  breathe 7 of fire every turn and the caverns' camps are broken by them
  and his guards, so the clerics memorize Resist Fire in Leilon and ward
  every member before him, curing the hurt instead of camping; Neverwinter's
  eastern base's fire giant mages are met warded the same way. Mirabar's
  council held a sorceress who joined inside its table, out of reach: a
  creature held back now joins where its enemies can walk to (Bugs), and a
  fight where every turn is a guard is played on by walking the members
  round to the nearest enemy before it is left.

*Next (where the work stopped, 2026-11-15, treasure8):* the route wins the
game with one stand-in left, the experience and money at Llorkh's hall
(probably unearnable: a party brought from *Gateway* carries them). Found and
left:

- **Lightning Bolt's path** is *Treasures*' own since 2026-11-16 (treasure9;
  [ORIGINAL-RULES.md](../ORIGINAL-RULES.md), Lightning Bolt by game): each
  step counted once, turned only indoors. *Pools of Darkness*' bolt, the
  same in its reconstruction, still runs *Pool of Radiance*'s path. The
  route still wins with it, the Way Inn's sorceresses' fight reloading until
  won as before.
- **The romance** plays since 2026-11-16 (treasure9): the route's leader,
  the first paladin, and Siulajia, who joins in Geildarr's Keep, are its
  pair. Their loves rise as each fells evil foes; the route's camps tell its
  scenes on the way, up to the pair's proposal before Fireshear's cavern on
  the sea, which the route accepts, and a lover goes into a frenzy where the
  other falls. Treasures' own turning plays too: the route's clerics and
  paladins turn the undead they meet. The cavern's fight (yeti and ice
  hounds, then the beholder) left the whole party dead in every play while
  the yetis' fright kept its members fleeing to the fight's end (Bugs);
  ended with the fright, the route wins it with no member lost, and the
  game with no re-tuning and no fight won by the test.
- The records that only set colors (Pictures and graphics; its
  portrait.pas, LoadAnimation) and the scripts' footstep (PLAY SOUND 10,
  record 9) play since 2026-11-16; the stone golem (147) and the yetis' fear
  on surprise (152) since 2026-11-15. The music plays where the original
  plays it, over the title alone (since 2026-11-16, treasure10: checked,
  nothing was missing).
- **The romance's scenes** are told as the camp menu opens (since
  2026-11-16, treasure10), as the original's EnterCamp runs the map's camp
  entry and then its CampMainMenu, whose first act is the romance: before,
  OpenGoldBox told them only as the party rested or used Fix, so a camp
  opened for the Crown alone heard none.

### The sides and the round limit (rules round 6, 2026-11-16)

With SETUP MONSTER's sides played ([ORIGINAL-RULES.md](../ORIGINAL-RULES.md),
Combat, item 10) and the Hit Points of AdjustLevelHitPoints, the route no
longer wins; where it stops:

- Beat 28 (map 28), lost on the Hit Points fix alone, is won again.
- The Way Inn's last fight (40: 7,15; Redbeard's captors): its script deals
  the monsters ahead and to the right (sides 3, or 9 when the party faces
  west, cell 11h), so half the sorceresses bolt from beside the party. 3
  plays of 3,000 win (measured before the Hit Points fix), 15 of 120 with
  the sides taken away, none of 720 facing west. Made a stand-in (the test
  wins it); its reloads until won are dropped.
- The council of Mirabar's ambush (30: 7,9): the Krakens and sorceresses
  come from the north, east and west (11): 1 play of 300 wins, two members
  dead. The route loses here.

Next: re-plan these fights (and any after them) for the sides, or put
them to the user as stand-ins.

## The ending

*Treasures* has no ending of its own. In Freezefire's lair (script 38, Ice
Peak's fourth stage) the party fights the black-robed men and the dragon
Freezefire; as the dragon dies the script sets cell 25Ch to 1, tells of the
dwarves carrying the party to Icewolf, the feast, Luskan's withdrawal and
Waterdeep's honors, and leaves the party on the overland outside Waterdeep
(map 54), where the game goes on. OpenGoldBox wins the game as that event
ends: "You have won." follows the script's closing words (`game_won`) until
the player goes on ([DEVIATIONS.md](../DEVIATIONS.md)).

## Bugs

OpenGoldBox's own bugs found in *Treasures* and fixed (the original's are
in [ORIGINAL-RULES.md](../ORIGINAL-RULES.md), Bugs):

| Bug | Fixed |
|---|---|
| *Treasures* read the spell rows it shares with the earlier games from their tables: Charm Person lasted forever, Fireball and Lightning Bolt reached a square a level, and the computer weighed spells by *Pool of Radiance*'s numbers (2026-11-15) | Its own table's rows (its gdconsts.pas) |
| Every tall character counted as large: weapons struck it with their dice against large creatures, and Hold Person never held it (2026-11-15) | A character stands on one square, as its chargen.pas has it |
| Hold Monster, Charm Monsters and Mass Charm took any monster, and Hold Person judged by type and size (2026-11-15) | Only those its flags call persons; Hold Person no monster |
| LOAD MONSTER brought every creature at once where a script held some back (cell 44h) (2026-11-15) | They join the fight as their round ends |
| Charm Person judged a target by *Pool of Radiance*'s rule (a creature type above 1, or a tall icon): every character of normal height counted as large, so neither side's Charm Person took or freed one, and monsters went by their type, not their flags (2026-11-15) | *Treasures*' own rule: a character always, a monster its flags call a person (bit 2 of its "immunities", record byte 421) on one square; one charmed already is freed |
| The Fix's Resurrection raised no one, the original rules having no such service, and while a cleric had it Raise Dead was not cast either (2026-11-15) | The dead but elves are resurrected, losing nothing |
| A temple's Heal cured nothing besides wounds in *Treasures*, whose Heal lifts blindness and feeblemind (its `temple.pas`, CureWounds) (2026-11-15) | It lifts them |
| PROGRAM 5 left feeblemind and drained levels and raised the dead by Raise Dead, at its cost in Constitution (its RepairParty resurrects and restores) (2026-11-15) | As RepairParty |
| *Treasures*' own effects (128 to 160) did nothing: its kampfults, yetis, remorhazes, harpies, manticores and stone guardians only struck (2026-11-15) | They play ([ORIGINAL-RULES.md](../ORIGINAL-RULES.md), *Treasures*' own effects): the stone golem and the yetis' fear on surprise since 2026-11-15, the romance since 2026-11-16 |
| No fight played the crystals (128 to 130): a crystal battle's script never heard of a crystal won, and the camp never named those held (2026-11-15) | The first bearer to fall gives its crystal (cells 3Ch and 3Eh) and the others smash theirs; the camp names those held (What the route needed) |
| A plain item of *Treasures*, a shop's or a treasure's, was named by the earlier games' item types: the Way Inn's saddlery sold "bows" that were belts, boots, cloaks and robes, and its firepots and flasks of oil were "Boulders" (2026-11-15) | Every item of its lists and definitions is named by its own words |
| Its fights played *Pool of Radiance*'s sounds, none as a spell went off, and *Pools of Darkness*'s and *Treasures*' sound effects were never loaded (2026-11-15) | `ZOUNDS.DAX`'s effects, sound n record n - 1, as their program plays them: a hit (7), a miss (9), a fall (5), a step (10), Fireball (11), Lightning Bolt (8) and any other spell (2) as it goes off (magic.pas, CastSpell) |
| Lightning Bolt ran *Pool of Radiance*'s bolt of 8 squares, reaching 4 squares and 1 a level, in every game (2026-11-15) | *Treasures*' own: 9 squares, reaching 4 and 4 a level ([ORIGINAL-RULES.md](../ORIGINAL-RULES.md), Lightning Bolt by game) |
| The computer chose its spells as *Pool of Radiance*'s does, three random picks a pass, and used its items (2026-11-15) | *Treasures*' own: the spell of highest AI weight with a target, bursts aimed where they take the most enemies, no items (its combatai.pas) |
| A creature held back could join on a square walled off from its enemies, where no one reached it and the fight never ended (Mirabar's council, 2026-11-15) | It joins on the nearest free square its enemies can walk to |
| Its romance (131) did nothing: no love in its fights, no scene at its camps (2026-11-16) | As its romance.pas and EffInLove ([ORIGINAL-RULES.md](../ORIGINAL-RULES.md), *Treasures*' romance) |
| Its Lightning Bolt ran *Pool of Radiance*'s path: its length taken again at every creature, a bounce out of doors (2026-11-16) | Its own LightningPath: each step counted once, turned only indoors |
| Its pictures ignored the records that only set colors: every town showed picture 10 in its own colors (2026-11-16) | The record cell 4Fh names colors the picture cell 4Eh names (LoadAnimation) |
| Its steps and PLAY SOUND 10 played the other games' synthesized footstep (2026-11-16) | Its own sound 10, `ZOUNDS.DAX` record 9 |
| Dispel Magic took the love, a kampfult's crushing, a stone golem's stone and a yeti's grab with the rest (2026-11-16) | It leaves them, as its SpellDispelMagic does |
| Its turning was the earlier games': clerics only, once a fight, the weakest undead first, a failed roll ending it (2026-11-16) | Its own CombatTurnUndead ([ORIGINAL-RULES.md](../ORIGINAL-RULES.md), Turning undead) |
| A member its yetis frightened as they surprised the party fled to the fight's end, the fright's 2 rounds over: in Fireshear's cavern on the sea every play of the route left the party dead (2026-11-16) | The flight ends with the fright (its EffFear, as the effect is removed) |
| A *Gateway* character's items came across renamed and renumbered, but their record's bytes, which the inventory keeps an item by, kept *Gateway*'s type and words (2026-11-16) | The bytes are rewritten with them |
