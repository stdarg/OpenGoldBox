# Secret of the Silver Blades: how its files differ

*Secret of the Silver Blades* (1990) runs on the engine *Curse of the Azure
Bonds* grew. This page records what its files change, as the census
(`opengold_census`, [GAMES.md](../GAMES.md)) reads them. The facts come from the
reconstructed source of the DOS release 1.3 and from the files themselves; the
wording is our own.

Since 2026-11-13 every file decodes, the title music in `SOUND.DAX` too (below).

## The files

| Files | What they hold | Change from *Curse* |
|---|---|---|
| `GEO1`–`GEO3` | 17 maps | None |
| `ECL1`–`ECL3` | 22 scripts | One new command (below) |
| `MON1CHA` | 70 creatures, one file for every disk | A new record of 439 bytes (below) |
| `MON1ITM`, `ITEM1`–`ITEM3` | Creatures' items, item lists | 63 bytes an item; the name bytes are scratch (below) |
| `MON1SPC` | Creatures' effects | None: 9 bytes an effect |
| `ITEMS` | Item types | None |
| `PIC1`–`PIC5`, `BIGPIC1`–`3`, `SKY`, `TITLE` | Pictures | None |
| `CHEAD`, `CBODY`, `CPIC1`, `COMSPR`, `DUNGCOM` | Combat icons and tiles | None |
| `SPRIT1` | Monsters seen at a distance | None |
| `8X8D1`–`8X8D3`, `WALLDEF1`–`WALLDEF3` | Each disk's wall tiles and walls | Walls use the tiles of the sets loaded beside them (below) |
| `8X8D5` | The font (201), alone | The font moved from `8X8D1`; its bytes are *Curse*'s |
| `START.EXE` | Item and spell names, and the sound effects | 123 item words; the effects' songs play notes (below) |
| `SOUND.DAX` | The title music: a driver program (1) and its bank of instruments and song (2) | New file (below) |

## Scripts

Scripts load and run as in *Curse*: at 8000h, to 9DFFh, with the same zones of
cells. A new game starts with script 17 on disk 2. One command is new:

| Code | Command | Operands |
|---|---|---|
| 41h | AWARD EXPERIENCE | 2 |

AWARD EXPERIENCE gives its second operand in experience to the whole party
when its first is not 0, else to the selected character, saying so; each
member's share is divided among its classes, and only members in play gain
it. PROTECTION (3Ch) does nothing in *Secret*.

Each script a NEW ECL loads starts with its map's own cells cleared, 4C00h to
4C1Fh and 7F79h to 7F82h, as the original's EclInit clears them in *Curse*
and *Gateway* too (*Pool of Radiance*'s are 4A00h to 4A1Fh). The Castle
Entrance's levers (4C04h and 4C05h) are gone in the Sanctum, where 4C05h at 1
marks its Brass door open. Until 2026-11-14 OpenGoldBox cleared *Pool of
Radiance*'s 4A00h to 4A1Fh for every game but *Gateway*; each game now clears
its own (*Pools of Darkness* and *Treasures* 97h to C8h, their area's state),
and a loaded game keeps them, as the original skips the clearing on its first
start after a load.

CALL's engine routines moved: the view's redraw is 2DCBh and the sound B203h
(*Curse*'s addresses less 7FFFh are compared, as there), the wall ahead and the
step forward keep *Curse*'s, and the duels are gone.

## Creatures

The record grew from 422 to 439 bytes. A character now knows up to 117 spells,
holds 75 memorized ones and has spell slots for seven spell levels; there are
seven classes (no monks), and type flags and a magic resistance take the
place of the type. Byte offsets:

| Offset | Field |
|---|---|
| 0 | Name: a length byte and up to 15 letters |
| 16–27 | Six abilities, each a permanent and a current score |
| 28, 29 | Exceptional Strength: current, permanent |
| 30–104 | Memorized spells, one a slot; bit 7 while still being memorized |
| 105 | Hours of rest before memorizing finishes |
| 106 | THAC0, as 60 minus the value |
| 107, 108 | Race, class |
| 109 | A paladin's Cure Disease uses left |
| 110 | Age (two bytes) |
| 112 | Maximum hit points |
| 113–229 | Known spells |
| 230, 231 | Attack level, icon size |
| 232–236 | Five saving throws |
| 237 | Movement |
| 238–241 | Highest level, level at the change of class, drained levels, drained hit points |
| 242 | Undead level, as in *Curse* (0 for every creature in `MON1CHA`) |
| 243–250 | Thief skills |
| 255 | NPC flag |
| 257 | The class a dual-class character left |
| 259–272 | Coins: copper, silver, electrum, gold, platinum, gems, jewelry (two bytes each) |
| 273–279 | Levels: cleric, druid, fighter, paladin, ranger, magic-user, thief |
| 280–286 | The same levels before the change of class |
| 287, 288 | Gender, alignment |
| 289, 290 | Attacks per two rounds, first and second attack |
| 291–296 | Their dice: two counts, two sides, two modifiers |
| 297 | Armor Class, as 60 minus the value |
| 298 | Whether Strength adds to hits and damage |
| 299 | Combat icon |
| 300 | Experience (four bytes) |
| 304, 305 | Class mask, hit points rolled |
| 306–333 | Spell slots: cleric, druid, unused and magic-user lists, levels 1 to 7 |
| 334 | Experience for defeating it (two bytes), then 336 experience per hit point |
| 339–342 | Portrait head and body, order, size |
| 343–348 | Combat icon colors |
| 349, 350 | Creature type flags, target flags |
| 352 | Number of items |
| 409–412 | Hands in use, saving throw bonus, encumbrance (two bytes) |
| 421 | Magic resistance |
| 422–425 | Status, in play, hostile, quick fight |
| 426–438 | Current THAC0, front and rear Armor Class, attacks, their dice, hit points and movement |

The other bytes hold pointers and the running game's bookkeeping.
`secret::decode_creature` reads the record into the fields every game's
records share (`por::CharacterRecord`), the cleric, druid and magic-user slots
of spell levels 1 to 5 among them.

## Items

An item's name is built from its three name words each time it is shown, so
the 41 name bytes at the start of an item hold whatever was last built there,
often not a name. OpenGoldBox reads the games after *Pool of Radiance* without
them (`decode_items(bytes, game)`). `START.EXE` holds 123 name words, not 255.

## Walls

The original draws every wall from one space of symbols: the 45 shared ones,
then 70 for each of the three wall slots. A slot's definitions name symbols in
that space, shifted by 70 for each slot past the first. *Secret*'s walls also
name the tiles of the slots loaded beside their own: map 32's LOAD PIECES 7
and nothing else, for one, leaves slot 3 empty, yet set 7's second block draws
some of its pieces from slot 3. OpenGoldBox draws every game's walls this way
(`load_wall_art`): the three slots' tiles go into one space, empty slots
blank, and each slot's walls are read with its shift. The census decodes each
set alone, with the slots it does not fill left blank. LOAD PIECES leaves a
second or third slot of 127 empty, as of 255.

## Modify Character

The party menu's Modify changes a new character's scores, Hit Points and
name while its experience is still creation's 200,000 (100,000 or 66,667 for
each class of a multi-class character), or none (`chargen.pas`,
`ModifyCharacterMenu`). Its Hit Points are *Curse*'s limits with each class's
own Constitution bonus, a fighter or paladin past its Hit Dice gaining 2 a
level. OpenGoldBox's party screen offers it so
([ORIGINAL-RULES.md](../ORIGINAL-RULES.md#modify-character)).

## The opening

A new game runs script 17 of disk 2, which has no map: over picture 119 (the
valley, `BIGPIC2`) it tells the party's dream of the valley, the town, the
mine, the glacier with its castle and the well, then sets the disk cell
(7F12h) to 1 and runs NEW ECL 16. Script 16, on disk 1, loads map 16 with wall
set 21: New Verdigris, where the party lies at (3,3) facing south. Script
numbers are unique across the disks, as in *Curse*, so a script's disk is the
`ECLn.DAX` that holds it.

There Priam, the mayor, welcomes the party, gives it what the town could
gather (TREASURE: item list 18 of `ITEM1.DAX`, twelve items such as plate
mail +1 and a Wand of Ice Storm, and 20 gems), and tells it of his house, the
Black Circle and the teleporter to the Well of Knowledge.

`load_campaign(folder, GoldBoxGame::secret_of_the_silver_blades)` plays it
with *Curse*'s loader (`campaign_session.cpp`), which differs only in the
opening, in having no wilderness, and in reading the one sprite, combat icon
and creature file for every disk. *Secret* has no portrait files for its
scripts.

## The script host

What *Secret*'s scripts need beyond *Curse*'s (found by walking every map):

- **AWARD EXPERIENCE** (41h): its second operand to every member in play, or
  with a first operand of 0 to the selected character alone; each member's
  share is divided among its classes, with no bonus for its prime scores.
- **ADD NPC** names a record of the one creature file, whatever the disk; a
  party already eight strong takes no one, as the original's
  AddCreatureToParty adds no one.
- **DAMAGE**: bit 10h of the first operand hurts a member who saves as much
  as one who fails (the whole party's D0h form).
- **CHECK PARTY** asks whether a race is in the party (by *Secret*'s race
  numbers: elf 1, half-elf 2, dwarf 3, gnome 4, halfling 5, human 6), and for
  Charisma and class levels (the lowest and highest of those at least the
  second operand, and the average of all).
- **SPELL** asks who has a cleric's (41) or a magic-user's (46) Dispel Magic
  memorized; that member casts it.
- The colour-selector cells C059h and C05Fh (and C05Ah, which the original
  ignores) are stored with no effect yet.

## Area names

As in *Curse*, the game shows no area's name, and the exploration screen
names each map as the cluebook's maps and the scripts' words do. Area 0, the
vision, which has no map, shows the game's title. The mine's wheel lift
(script 48) plays levels 1 to 4 with script 49 and 5 to 8 with script 50, both
on map 48, and levels 9 and 10 and the way to the temple with script 51 on
map 49, so both maps are the Mines of Verdigris.

| Map | Name | Map | Name |
|---|---|---|---|
| 16 | New Verdigris | 66 | Dungeon, levels 2 and 1 |
| 32 | Ruins of Old Verdigris | 68 | Drider Base |
| 33 | Well of Knowledge | 80 | Western Crevasses |
| 34 | Black Circle Headquarters | 81 | Frost Giant Village |
| 48, 49 | Mines of Verdigris | 82 | Eastern Crevasses |
| 50 | Temple of Tyr | 96 | Castle Entrance |
| 64 | Dungeon, levels 10 to 7 | 97 | Middle Level of the Castle |
| 65 | Dungeon, levels 6 to 3 | 98 | Dreadlord's Sanctum |

## The temple

New Verdigris's Temple of Gond heals the town's champions for nothing: its
HealMenu takes no money, so every service costs 0.

## Rooms joined by passages

The ruins (map 32) and the mine's levels (map 48 for levels 1 to 8, map 49
for the temple level and levels 9 and 10) are each one 16x16 map that serves
many places. A wall of a given kind (7 or 8 in the ruins, 1 in the mines)
stepped into from a square with an event is a passage: the step script looks
the event up in the place's tables and moves the party to another square,
and perhaps another place.

- **The ruins**: cell 4C4B names the sector (1 to 10, each with its own
  table of 64 events); bit 64 of a table's entry moves to another sector, and
  4CFF turns the party's facing. 4CFD and 4CFE count the rooms on the
  cluebook's large map. Sector 5 holds the town's gate, sector 8 the Black
  Circle Headquarters' main entrance and the mine's entrance, sector 9 the Old
  Red Dragon's lair. The Well's corners lead into sectors 6 and 10.
- **The mines**: cell 4C69 is the level and 4C6B the level's half (each
  level has two tables, one a half); an entry of 128 or more is rubble. A
  hidden trapdoor on level 2 drops an unwary party to level 3 (search mode, a
  dwarf or a thief notices it).
- **The Dungeon** (map 64 for levels 10 to 7, 65 for 6 to 3, 66 for 2 and
  1): cell 4C87 is the level; its stairs are guarded by the spirit's
  riddles, and each key lies on its own map.
- **The crevasses** (maps 80 and 82): passages as the mines', the places told
  apart by cells 4C05 and 4C6B; 4CFD and 4CFE count the cluebook's
  coordinates, the x one wrapping at 99.

## The ending

The game has no ending of its own: script 99 tells of the soul gem's
shattering (it sets cell 4CD9 to 1) and the game goes on; back in New
Verdigris every building celebrates and the mayor thanks the party (ECL1:16
at 9132h), setting the cell to 255. OpenGoldBox wins the game as the event
that frees the soul ends ([DEVIATIONS.md](../DEVIATIONS.md)): its closing
words end "You have won." and the session's `game_won` holds until the player
goes on.

## The route

`opengold_secret_route` (`opengold_gold_box_games_tests --secret-route`) plays
the cluebook's order of play with two paladins, a ranger, two clerics and a
magic-user made under Secret's rules, from New Verdigris to the soul gem's
shattering and the win; its beats and stand-ins are in [GAMES.md](../GAMES.md),
phase 4 step 4. Planning aids help change it: `OPENGOLD_SECRET_LINKS` prints the ruins' passages, and
`OPENGOLD_SECRET_FIND=X,Y[,MAP]` (or `*`, or `map:N`, with
`OPENGOLD_SECRET_KEYS` and `OPENGOLD_SECRET_GOAL` naming cells, and
`OPENGOLD_SECRET_SEARCHING`, and `OPENGOLD_SECRET_PREFER=WORD,...` for the
choices to take on the way) searches from where the route ends for the steps
that reach a square or set a cell, playing each step as the game does
(questions answered NO, every fight won at once), and prints them for the
route's `walk_passages`. `OPENGOLD_ROUTE_SAVE=FILE` saves the game where the
route ends and `OPENGOLD_ROUTE_RESUME=FILE` plays on from it (with
`OPENGOLD_ROUTE_BEATS=N` to stop after N beats), so a search starts in seconds
instead of after the route's six minutes.

### Stand-ins and win rates (2026-11-16)

On main before this work (4b5b95686, the party made with Modify) the route's
test won 17 fights: a basilisk and medusa patrol on the mine's tenth level,
ten in the Eastern Crevasses (the Black Circle's patrols, the three guard
posts of ancient red dragons, the cloud giants and white dragons, Oswulf's
last stand), Sargatha's horde and her lone medusa at the Castle Entrance,
and in the Sanctum the warrior in golden armor, a camp's fights by the
hydras, the Dreadlord and the soul gem's guards. The current route forces
none of these wins. What changed, each as a player plays it:

- **Dice a beat**, as Pools of Darkness's and Gateway's routes have, so a
  save resumed with `OPENGOLD_ROUTE_RESUME` plays on as the whole run does.
- **Arms:** the party takes from the mayor's pile its Shield +2 (paladin 1),
  Gauntlets of Ogre Power (paladin 2), Wand of Ice Storm and Bracers AC 6
  (the magic-user), and on the way the cluebook's items where its beats win
  them: the Old Red Dragon's cloak and ring (the magic-user), the dead
  adventurer's plate, sword, shield and ring on the mine's first level
  (paladin 1), the Ring of Wizardry, the mines' 81 (the sword against
  giants for the ranger, the banded mail +5 for paladin 2, the cloak for
  cleric 4), the frost giant king's plate, sword and shield, and Oswulf's
  Silver Long Sword and Silver Shield (paladin 1). Below the mine's tenth
  level each member readies its mirror in place of its shield, in the
  crevasses its shield again, and in the castle its mirror but the Silver
  Shield's bearer, whose shield turns gazes back.
- **Spells:** the casters memorize to the seventh level (Heal, Disintegrate),
  and the magic-user learns the Globe of Invulnerability and Disintegrate
  as it trains.
- **Healing:** the camp's Fix (ORIGINAL-RULES.md, *Secret*) whenever a member
  is hurt, and before each rest; a rest when a member is below half its Hit
  Points or the magic-user has cast half its spells. Fights won at the cost
  of members count as won, their dead raised by the Fix's Raise Dead.
- **Tactics:** the members strike first the enemy's casters (the Black
  Circle's and the Legion's wizards, the necromancers, the Banite priests)
  and those beginning a spell, and the magic-user casts its Fireball where it
  takes the most of them and spares the party, as the cluebook bids. This
  turned the Black Circle's patrols from 0 and 8 wins in 120 plays to 31 and
  40.
- **Reloads:** from the crevasses on, a fight lost in each of 120 plays is
  played up to 240 more times with new dice. It also retries a victory
  costing one member, including a petrified magic-user who cannot cure itself,
  within the same budget; a surviving victory is retained when that budget ends.

The final three forced victories were removed on 2026-11-16. A fresh
whole-route run with `OPENGOLD_ROUTE_PLAY_STAND_INS=1` reaches the actual
ending and wins Oswulf's chained battles, the Dreadlord and the soul-gem
fight through combat commands. The town hall spends already earned
experience on the mid-game return; a later temple visit raises the dead and
cures the petrified magic-user before further training. The last Well rest
retains the globe, Mass Invisibility, Mirror Image and Haste plan. Globes
and Mass Invisibility are now available in camp, and the globes persist
into combat ([ORIGINAL-RULES.md](../ORIGINAL-RULES.md)). Holds prioritize
hostile casters, clerics retain Dispel Magic, and fireball planning respects
range and line of sight. An event can be reloaded as a whole when its chained
battles defeat the party; victories with survivors are kept even with casualties.

The former 0-of-360 measurements describe the old party preparation and
player tactics, not an engine impossibility. The successful complete replay
is recorded in [PLAYABILITY.md](../PLAYABILITY.md). It still has the
progression and relocation assistance below; it is not proof of a complete
unassisted playthrough.

The experience a party brings from *Curse* stays a stand-in: the four
awards give each paladin 1,550,000 above Secret's new character's, and the
whole route earns about 664,000 a member, too little to replace it.

The castle as the route plays it: the Castle Entrance's rotating alcoves turn
the party into the rooms beside the great hall and back only once the room's
lever is pulled (cells 4C04 and 4C05, a bit a room); a flagstone at 9,15
sends a party stepping east to 13,0, by the stairway up, and one at 13,0
stepping west back; its gateway at 6,9 and the Sanctum's at 12,11 lead to the
Well's gates 13 and 6. The Sanctum's three key doors open with the Silver Key
(12,0), the Gold Key (2,9) and the Brass Key (6,1; cells 4C06, 4C07 and 4C05
set to 1), the last onto script 99, which plays on the Sanctum's own map.

## Sound effects

The PC speaker's driver is *Curse*'s, found the same way, with 13 songs. Its
streams use the whole of the driver's language, which *Pool of Radiance*'s and
*Curse*'s effects use only part of: three-byte note events (the voice times
20h, the pitch in semitones, 7Fh for a rest, bit 7 on the step's last event,
and the step's length), the twelve note dividers of the lowest octave that
follow the eight voice blocks, halved an octave up, the transposition and
tempo fields, 0FAh to clear the selected voice, and the level envelopes a
note's sustain runs out into. Song 12, a long tune of notes, is never played
by the game.

## Sound and music

`SOUND.DAX` holds the music of the introduction: the music driver (record 1)
and the bank it plays (record 2), a library of instruments and one song of four
tracks. The later games keep this driver and its banks; the layout is in
[later-sound.md](later-sound.md). *Secret*'s build differs in three ways:

- its library holds 31 instruments, the Covox's last one missing;
- it finds the songs 1B2h bytes into the bank, where the library ends;
- it ignores the song's tempo and counts 118 units of a note's length a tick,
  so the title music's four tracks each last 6150 ticks, 84 seconds.

`decode_music_bank` reads the bank. The introduction does not play it yet.
