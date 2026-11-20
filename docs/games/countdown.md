# Buck Rogers: Countdown to Doomsday

The first *Buck Rogers XXVc* Gold Box game (1990). The plan to play it to its
win under its original rules is [BUCK-ROGERS.md](../BUCK-ROGERS.md). Step B0
(groundwork) and step B1 (the reading census) are done (2026-11-19), and the
characters' and ground combat's rules of step B2 (2026-11-20). The first part
of step B3.1 (2026-11-20) has the engine play its scripts, views, ground fights
and outposts ("The game in the engine"); its button in the startup menu stays
disabled until the new screens and the route are done.

## Files and references

- Game files: `/Users/edmond/GoldBox/buck-rogers-countdown-to-doomsday` (the
  folder `game_folder` names; the game and its tests find it by its files,
  whatever it is called: TECH.md, "Finding the games' folders").
- Rule Book, Log Book, Cluebook and the Amiga Data Card:
  `/Users/edmond/GoldBox/docs/buck-rogers-countdown-to-doomsday`.
- Reconstructed source (version 1.3, Turbo Pascal 5.5):
  `~/src/unsafe/buck-rogers-countdown-to-doomsday`. It has no license;
  OpenGoldBox learns from it and writes its own code and text.

## The game's number and marker

- `GoldBoxGame::countdown_to_doomsday` is number 10 in saves. Number 9 is
  kept for *The Dark Queen of Krynn*, which another session adds after
  *Death Knights*; `game_numbered` finds a game by its number, so the gap is
  safe.
- `identify_game` knows the folder by `SHIPS.DAX` (only the Buck Rogers games
  have it) with `ECL6.DAX` beside it. `ECL6.DAX` alone marks nothing, since
  *Pool*, *Curse* and *Gateway* have one too. None of the earlier games'
  markers is in the folder.

## The census

`opengold_census` reads every file of the folder, and the game is in
`tests/game_census.cmake`'s list of complete games. `opengold_census
--scripts FOLDER` adds the script listing below. `tests/countdown_tests.cpp`
checks the layouts on made-up bytes and the real records.

| Files | What they hold | Read by |
|---|---|---|
| `ECL1`-`ECL6` | 25 scripts | the Buck Rogers command table (below) |
| `GEO1`-`GEO6` | 17 maps | the shared map reader |
| `TITLE`, `BIGPIC`, `PIC`, `BACK` | title screens, big pictures, portraits and animations, backdrops | the 256-color readers of *Pools of Darkness* |
| `SPRIT`, `CPIC`, `COMSPR`, `CHARS` | encounter sprites, combat icons, effect icons, the 36 body icons (two frames each, ids n and n + 128) | the same |
| `8X8D0`-`8X8D6`, `WALLDEF1`-`WALLDEF6` | wall tiles and walls | each wall set with the tile record of its own id in its own disk's `8X8D` file |
| `8X8D1` record 201 | the font, 2,048 bytes | the shared font reader |
| `BORDERS`, `CURSOR` | the frame's tiles, the wilderness cursor | 256-color pictures |
| `HITEK`, `LOTEK`, `MARSCOM`, `VENUSCOM` | ground-combat tile sets of 24 by 24 tiles | 256-color pictures |
| `SHIPS` | the rocketships (records 40 to 44 and 50) and their pictures (0 to 4, 10 to 38, 128) | `buck_rogers::decode_ship`, 256-color pictures |
| `MON0CHA`, `MON0ITM`, `MON0SPC` | 54 creatures, their items, their effects | `buck_rogers::decode_creature`, `decode_items`, the shared effect reader |
| `ITEM0` | 91 item templates | `buck_rogers::decode_item_templates` |
| `ITEMS` | 39 item types | `buck_rogers::decode_item_types` |
| `MUSIC` | the music driver and its banks | the bank reader of *Gateway* and the later games |
| `START.EXE` | the 44 words item names are built from, and the sound driver's 21 songs | `decode_item_words`, `load_sound_effects` |
| `GAME.OVR`, `SAVGAMA.DAT`, `SAVE/` | the program's overlay, a saved game in slot A, the save folder | never read |

What the census found:

- **Pictures** are the 256-color layout of *Pools of Darkness*: still
  pictures, and animations whose frames are packed differences from a key
  frame. Every record decodes exactly to its size.
- **Walls**: each disk's `WALLDEF` file draws with the tiles of the same id in
  that disk's `8X8D` file (`WALLDEF2` record 1 with `8X8D2` record 1). The
  wall-art reader now looks in the wall file's own disk for every 256-color
  game; *Pools of Darkness* and *Treasures* have only disk 1, so nothing
  changes for them. `8X8D1` record 202 holds the tiles the game loads at start
  into slot 4, and `8X8D0` record 1 the solar map's tiles.
- **Combat tile sets**: the game copies 53 tiles from `HITEK` and `LOTEK` and
  52 from `MARSCOM` and `VENUSCOM` (`combatst.pas`), but `LOTEK` holds 52,
  `MARSCOM` 42 and `VENUSCOM` 44. The copy runs past the picture's pixels; the
  arena should never draw those tiles. Step B3 checks that the arenas use
  only tiles the files hold.
- **Ships**: a ship is 108 bytes: its name, tonnage, length and width, six
  systems (hull, sensors, controls, life support, fuel and engines) as current
  and whole strength, cargo, crew, Armor Class, the pilot's and gunners'
  values, speed, reaction, maneuver, type, race and eight weapon mounts of six
  bytes. Records 40 to 44 are the enemy types SHIP COMBAT loads (its first
  operand less one, plus 40): the Hubble, Adder, Mohave, Flare and
  Liberator. Record 50 is the party's Maelstrom Rider: 150 tons, a hull of
  600 and 450 fuel. The game renames enemy ships from its own name list as
  they appear.
- **Creatures**: `MON0CHA` entries are 263 bytes; the game copies the first
  259, a character record, and never reads the last four. The record holds
  the name, seven abilities and their current scores, eight saving throws,
  gender, race (0 for monsters), career, level, credits, experience, combat
  value, movement, Armor Class (50 is unarmored), Hit Points, 84 skill
  levels, the experience the creature is worth and its status. Creature 4 is
  the desert ape: level 8, 48 Hit Points, Armor Class 55, worth 625.
- **Items**: an item record is 62 bytes. Its first 46 hold a name and a list
  link that the game builds afresh when it loads the item; in `MON0ITM` they
  are leftover bytes and are not read. The last 16 are the item's template:
  its type (a row of `ITEMS`, or 255 for none), three name words, its bonus,
  its saving-throw bonus, flags, weight, quantity, value and three special
  bytes. The game builds a name from the words in reverse order: template 7,
  the free weapon of a new character, has words 7 and 8, "Bolt Gun".
- **Item types**: `ITEMS` holds 39 rows of 9 bytes (slot, attacks per round
  doubled, weapon class or armor value, a heavy flag, damage dice and bonus,
  range, a consumable flag). The game asks for 129 rows and takes what the
  file has; BUCK-ROGERS.md had read the 129 as the file's size.
- **Sounds**: `START.EXE` links the same speaker driver as *Curse* and the
  Krynn games, with 21 songs. Sound command n plays song n - 1: 2 damage, 3 a
  launch, 4 a grenade's cloud, 5 a creature down, 6 the sonic stunner, 7 a
  hit, 8 the dazzle grenade, 9 a miss, 10 a footstep, 11 an explosion, 12 to
  14 and 16 to 18 the guns, 19 and 20 the ship's weapons. No command plays
  song 14 or starts song 20.
- **Music**: `MUSIC.DAX` is the driver and its instrument and song banks, in
  the format *Gateway*, *Pools of Darkness* and *Treasures* use. Its tempo is
  read as theirs; step B3 checks it by ear against the original.

## Scripts

The engine is *Curse*'s, with new commands. OpenGoldBox reads the scripts with
their own table (`game_opcodes`):

- Scripts load at 8000h and may run to A400h, that address included.
- Operands keep the Realms tags and add two: tag 4 is a four-byte number and
  tag 5 the address of a four-byte cell (the team's credits, for one).
- Changed commands: SETUP MONSTER takes 4 operands; 1Dh sets the speaker
  (2); 1Fh is SHIP COMBAT (4: enemy ship 1 to 5, an unused value, the enemy
  pilot's skill, its to-hit number); 22h and 23h are SKILL CHECK and SKILL
  CHECK WITH TEXT (3); 27h TREASURE gives credits and then a list of item
  templates; 29h waits for a key; 2Ch asks yes or no; 31h is the menu with
  "what do you do?"; 37h loads files as 21h does; 3Fh names a Log Book entry
  (2); 40h DESTROY ITEMS takes 2.
- New commands 42h to 4Ch: CLEAR AND STOP (ends the script), PLAY SOUND, SKIP,
  DISTANCE AHEAD, LOOP START and LOOP NEXT, SWAP ITEM LISTS, SKILL CHECK WITH
  DAMAGE (6: skill, who, difficulty, dice, sides, bonus), SOLO COMBAT, SHOP
  and PICTURE MODE (0 the 3D view, 2 the solar map, 3 a picture, 4 the
  wilderness).
- 3Bh and 3Ch do nothing and leave the script where it is, so a script that
  reached one would never end. No script reaches either; the census fails if
  one does.
- CALL names the same routines as *Secret of the Silver Blades*.
- The original's IF skips the next command by a table of its own
  (`SkipEclCmd`), which is wrong for some commands (SETUP MONSTER, SHIP
  COMBAT, ECL CLOCK, the menus, ON GOTO, TREASURE). No IF in either game is
  followed by one of them, so the reader's skip and the original's agree.

### What the scripts ask of the engine

The listing traces a value a command reads from memory back to the literals
the script saved there on every path, through subroutines.

- **Scripts**: 16 starts a new game; 17 is Salvation; 18 is the
  demonstration, which only the demo runs. Every other script is reached
  from 16 through NEW ECL. Script ids carry their disk: 16 to 18 on disk 1,
  32 to 35 on 2, 48 to 52 on 3, 64 to 67 on 4, 80 to 83 on 5 and 94 to 99 on
  6. A script changes disk by saving the disk's number in cell 7F12h before
  NEW ECL.
- **Skill checks**: 49, of 26 skills. Difficulty 0 quadruples the skill, 1
  doubles it, 2 keeps it, 3 halves it and 4 quarters it; most checks are at
  1, 2 or 3. 27 let the party's best member roll, most of the rest the
  current character. Among them: Notice (83), Programming (43), Library
  Search (45), Bypass Security (7), Open Lock (8), Demolitions (11),
  Astrogation (46) on the solar map, Commo Operation (9) to hail a ship, Fast
  Talk/Convince (76), Climb (65), Acrobatics (64), Hide in Shadows (61), Pick
  Pockets (63), Disguise (50), Etiquette (79) and Planetary Survival (84).
  Two checks deal damage to those who fail: Maneuver in Zero-G (67) at
  difficulty 1 for 1d4 when a ship lurches (script 32), and Acrobatics (64)
  at 2 for 1d6 (script 83). Three checks read a difficulty the trace cannot
  follow (scripts 64, 66 and 96).
- **Hailing a ship** (script 34): after Commo Operation, the team may bluff,
  intimidate or ask for help. The script checks skills 74, 70 and 69 for
  these: Befriend Animal, Hypnosis and Paint/Draw. Everywhere else the
  numbers name the skill the text asks for (Matrix Cubed's amateur night
  asks 77 Singing, 71 Act, 48 Mimic and 64 Acrobatics); Fast Talk/Convince
  (76) and Intimidate (72) are two numbers higher. OpenGoldBox checks 76, 72
  and 71 (Bugs).
- **Ship combats**: three. Script 34 fights ship 1 (the Hubble type) with
  pilot skill 50 and to-hit 18, and random encounters with ships 1 to 5,
  whose pilot and to-hit values the script sets before the fight. Script 95
  fights ship 2 (the Adder type), a RAM cruiser with pilot skill 80 and
  to-hit 16, as the team escapes into space.
- **The solar map**: script 34 is the solar map. It writes area kind 70h at
  its start and sets picture mode 2 after its clinic and as it moves the
  rocket; script 95 sets mode 2 for the escape.
- **Fuel**: the engine never spends fuel; script 34 does. Before each step it
  costs 10, 10 more without sensors, and 10 more when the party fails
  Astrogation at difficulty 1. The step needs working controls, fuel and
  engines and enough fuel, or the rocket "can't move". A NEO cruiser answering
  the radio fills a tank below 40 to 200, and a hailed ship that helps one
  below 100; Salvation refuels the ship to 450 (script 17); the port of
  script 99 sells fuel up to 450, a unit for 20 from the four-byte cell
  4CE6h, which Salvation's welcome sets to 20,000: the NEO salvage account
  ("Upkeep and money").
- **Shops**: SHOP 1 at Salvation (script 17) and lists 2 to 15 in the port of
  script 99.
- **PROGRAM**: 0 (the team menu, with training) at Salvation and at the port;
  3, which the engine ignores, only in the demonstration.
- **The winning event**: the escape from the Mariposa, in script 98. The team
  enters the escape pod, through the pod control center's authorization or by
  demolishing its door (Demolitions at difficulty 3), and launches; the
  Doomsday device and the Mariposa's top explode, Wilma picks the pod up, and
  she tells the team its mission is over. The script then saves disk 1 and
  loads script 17: the team is back at Salvation. The engine has no ending of
  its own.

## Rules: the characters (step B2)

The character half of step B2 is done (2026-11-20): careers, races,
abilities, Hit Points, experience, saving throws, skills and their checks,
creation, training, Modify and the character record. Ship combat is another
part of B2; ground combat, injuries and healing, and money come after. The
rules live in `src/OpenGold.Rules.Original/src/buck_rogers_rules.cpp`
(`opengold/buck_rogers_rules.h`), keyed by game; *Matrix Cubed*'s
differences are in [matrix-cubed.md](matrix-cubed.md). A character is the
game's own record (`buck_rogers::Creature`), so what the rules make is what
the original saves. `tests/countdown_tests.cpp` checks each rule and names
the routine of the original it follows.

### Careers and races

- **Careers**, numbered 1 to 6 in the record: Rocketjock, Medic, Warrior,
  Engineer, Rogue and Scout. They are `Profession::rocket_jock` to
  `Profession::scout`, after the AD&D classes, whose numbers do not change.
- **Races**, numbered 0 to 8: the monsters' race 0, then Terran, Martian,
  Venusian, Mercurian, Tinker, Desert Runner, Lunarian and Lowlander
  (`Race::monster` to `Race::lowlander`, after the Krynn races).
- **The menus** offer the first six races. The four human races may take
  the first five careers; a Tinker may be a Medic or an Engineer; a Desert
  Runner a Rocketjock, a Warrior or an Engineer. No race is offered the
  Scout: the source lists it sixth for the human races but counts only five.
  `professions_for(race, game)` gives these lists in the two games.
- **Movement** is 12; a Tinker's is 9 and a Desert Runner's 15.
- **Unarmed:** one attack a round for 1d2; a Warrior punches twice for 1d3;
  a Desert Runner's claws make it 1d3 + 1.

### Abilities

Seven: Strength, Dexterity, Constitution, Intelligence, Wisdom, Charisma and
Tech. Each is the best of six 3d6 rolls, plus the race's adjustment; a
result below 1 is 1, and a result below the career's minimum is raised to
it. The player may reroll the whole set as often as wished.

| Race | Str | Dex | Con | Int | Wis | Cha | Tech |
|---|---|---|---|---|---|---|---|
| Terran | | | +1 | | +1 | | |
| Martian | -1 | +1 | -1 | | -1 | +1 | |
| Venusian | | -1 | +1 | | +1 | -1 | |
| Mercurian | -1 | +1 | +1 | | | | |
| Tinker | -2 | +3 | -2 | | | | +3 |
| Desert Runner | +2 | +2 | +1 | | | -1 | |

| Career | Minimums |
|---|---|
| Rocketjock | Dex 13, Int 11, Cha 12 |
| Medic | Dex 12, Int 12, Wis 12 |
| Warrior | Str 10, Dex 8, Con 10, Int 8 |
| Engineer | Str 10, Con 12, Int 8, Tech 13 |
| Rogue | Dex 13, Int 8, Wis 9, Cha 13 |

### Hit Points, experience and combat value

- **Hit Points** each level: the better of two rolls of the career's die (a
  Warrior's d10, an Engineer's d8, the rest d6), plus the Constitution bonus
  (-3 at 1 up to +6 at 21; none from 7 to 14), which only a Warrior gets
  above +2; at least 1. The source fixes the gain from level 9 (2, a
  Warrior 3), which *Countdown* never reaches.
- **Experience** has one table a career, to level 9: the Rocketjock's,
  Engineer's and Rogue's from 1,250 at level 2 to 110,000 at 9; the Medic's
  from 1,500 to 192,000; the Warrior's from 2,000 to 250,000. The level
  limit is 8.
- **Combat value** is 40 at levels 0 to 2 and rises to 44 at level 9 (48
  for a Warrior).

### Saving throws

Eight categories, numbered 0 to 7. Each is a d20 to meet or beat; 1 always
fails and 20 always saves. A new character's are 15, 14, 14, 13, 13, 16, 13
and 13, less its race's adjustments: a Terran is one better in category 0, a
Mercurian three better in 4 and four in 6 but three worse in 7, and so on.
Each third level makes every save one better. The engine uses category 0
against a stun and 2 for a crew member as its ship is hit; the weapons and
effects that call the others name only their numbers, and the manual gives
the categories no names.

### Skills

- **84 skills** in five groups: technical (1 to 23), Intelligence (24 to
  50), Dexterity (51 to 69), Charisma (70 to 79) and Wisdom (80 to 84); the
  Constitution group is empty. 54 are offered; the other 30 (Cook, History,
  Hypnosis, Paint/Draw and the like) are on the list but can never be
  learned.
- **Career skills:** each career's eight, which only career points buy. The
  rest offered are general skills, which general points buy; the medical
  skills (Life Suspension Tech to Diagnose) are a Medic's career skills and
  no one else's.
- **Prerequisites:** Repair Nuclear Engine and Repair Rocket Hull need 10
  in Repair Mechanical; Repair Life Support 30 Electrical and 25
  Mechanical; Jury Rig 10 of each; Treat Serious Wounds 30 Treat Light;
  Treat Critical 40 Treat Serious; Treat Poisoning 15 Diagnose and Treat
  Disease 25; Programming 10 Mathematics; Astrogation 20 Astronomy and 25
  Mathematics; Navigation 15 Astronomy and 25 Mathematics; Planetary
  Survival 10 Planetology.
- **Starting levels:** a Rocketjock's piloting and driving skills and a
  Rogue's eight career skills start at 10; everything else at 0.
- **Spending** (the skill screen): at creation 80 career and 40 general
  points, at most 30 in one skill; at each training 40 and 20, at most 15. A
  general skill stops at 80. A new character may learn seven general skills;
  later, one new one a level. Points may be taken back until the screen is
  left. When a skill drops below another's prerequisite, the other goes to 0
  and its points come back.
- **Value:** a skill's level plus the score of its group's ability, while
  its prerequisites are met (otherwise the level alone), for a character
  who can act; 0 without the skill.

### Skill checks

The check scales the value by its difficulty (0 four times, 1 twice, 2 as it
is, 3 half, 4 a quarter, rounded down) and rolls a d100. A roll up to the
value succeeds, and one at least half the value under it succeeds greatly; a
miss by more than the value is a bad failure. When the whole party may try,
every member rolls and the best margin acts. The hail's checks are corrected
(Bugs).

### Creation

In order: race, gender, career; the age (1d6 + 18); the abilities; the free
training from level 0 to level 2 (two levels of Hit Points, and no skill
points or specialization in it); the reroll offer; the name; a Warrior's one
weapon specialization; the skill screen; the body icon; and the save, with
1,000 credits, Armor Class 50 and two readied items: template 7, a Bolt Gun
with 100 rounds, and template 20, a Spacesuit.

### Training and specializations

Training is free and raises one level: the career's combat value, the
level's Hit Points, the experience cut to one point short of the next level,
the saves every third level, a Warrior's specialization at even levels, and
the skill points. It needs a conscious character with the next level's
experience, below level 8. A specialization adds one to hit and to damage
with weapons of its type (the Sonic Stunner's only to hit), at most three in
one type and ten in all; the weapons are item templates 1 to 15.

### Modify Character

Only a character with its career's starting experience. Each score stays
between the race's adjustment + 3 and + 18, and at least the career's
minimum. The Hit Points stay between (the Constitution bonus + 1, at least 1,
and for a non-Warrior at most 3) times the level and (the bonus, at most 2
for a non-Warrior, plus 10 for a Warrior, 8 for an Engineer and 6 for the
rest) times the level; a Constitution change moves them inside the new
limits.

### The record

A character is 259 bytes (`.who`): the name, the seven abilities and their
current scores, the eight saves, gender, race, career, level, credits,
experience, combat value, movement, Armor Class, age, the unarmed attacks,
Hit Points, 84 skill levels, the ten specializations, the icon, status and
the combat leftovers. Its items are 62-byte records (`.stf`), of which the
last 16 are the template. `encode_creature` and `encode_items` write them
back; every creature in `MON0CHA` and every list in `MON0ITM` of both games
comes back byte for byte.

## Upkeep and money (step B2)

How the team is looked after and how it pays, as the original's engine and
scripts do it. The engine's part is in
`src/OpenGold.Rules.Original/src/buck_rogers_upkeep.cpp`
(`opengold/buck_rogers_upkeep.h`); `tests/buck_rogers_upkeep_tests.cpp`
checks each rule and names the routine it follows. The bank, the hospital,
the port and the ship's clinic are the game's scripts, which the script host
runs as they are (step B3). *Matrix Cubed* runs the same engine routines,
and its scripts differ a little ([matrix-cubed.md](matrix-cubed.md)).

### The injury states

The record's status byte: 1 okay, 2 gone, 3 dead, 4 dying (bleeding), 5
unconscious, 6 fled, 7 poisoned, 8 comatose, then the fight's own states
(dodging, sprinting, failed in zero gravity, the jet pack). Only an okay
character can act. The fight sets them; upkeep reads them:

- **Bleeding:** each round of a fight takes a bleeding character closer to
  a coma, which comes after ten. A medic's bandage stops it.
- **Unconscious:** 0 Hit Points; time brings the character round.
- **Poisoned:** after the fight a Medic or an antidote cures it, or the
  character slips into a coma.
- **Comatose:** only a hospital, Salvation's doctor or the ship's clinic
  brings the character back. Time does nothing.

### After a fight

The order (`postcbt.pas` `PostCombat`): the fight's result, then, unless the
team was destroyed, the medics' treatment, the ship's repairs after a ship
fight, the experience summary and the booty. There is no treatment after
the team boards a ship, nor once after a script sets cell 4B80h.

- **The wounded** (`DetermineCombatResults`), after a fight the team did not
  flee: a bleeding member falls unconscious, and an unconscious one with Hit
  Points comes round. A script can set cell 7EE6h to put the bleeding and
  the unconscious back on their feet instead, with at least 1 Hit Point.
- **Poison** (`TreatInjuries`): each okay Medic in turn tries Treat
  Poisoning on each poisoned member until one succeeds. Failing that, the
  game offers the antidote of the first member carrying one, and one of the
  bundle is used. Failing that, the patient slips into a coma. A cured
  patient is left unconscious with 0 Hit Points.
- **Wounds:** every team member who is okay, bleeding or unconscious and
  short of Hit Points is seen to. First each okay member in turn may give
  First Aid: 1d6, never above 6 Hit Points. Then the first success of Treat
  Critical Wounds (twice 3d8 + 3), Treat Serious Wounds (twice 2d8 + 1) or
  Treat Light Wounds (twice 1d8), tried by each okay member in turn. Every
  check is at difficulty 2, except a medic's Treat Critical Wounds on
  itself, at 4. Anyone with the skill treats, and only Medics learn the
  Treat skills. A treated patient is okay. Only Treat Critical Wounds is
  harder on oneself, as in the original (the user, 2026-11-20).
- **Unkillable:** a member with the Unkillable effect and fewer than 45 Hit
  Points stands up with 45.
- **The report** lists each cure, antidote, coma and healing, and ends with
  one of four lines: no injuries, no medics, nothing treated, or healing
  completed.

### AID: medics in a fight and aboard

AID (`viewchar.pas` `FindPatients`, `TreatPatients`) is a command of the
ground fight and of the ship fight's crew. A medic with First Aid or Treat
Light Wounds bandages every bleeding member, without a roll: the patient is
unconscious and bleeds no more. One with Treat Stun tries it on every dizzy
or stunned member who can act, and a success ends both. In a ground fight
only the members next to the medic are patients; aboard, anyone is. While
exploring there is no medic command: treatment comes after a fight, with
rest, or at a hospital or the clinic.

### Time and rest

- **Rest** (`gametime.pas` `RestAndHeal`): each day every unconscious
  member comes round and every okay member gains a Hit Point, up to the
  maximum. The bleeding, poisoned and comatose gain nothing.
- **The rest menu** (PROGRAM 9; `explore.pas` `RestMenu`): Rest, Save,
  Change and Exit. Each Rest is one day.
- **The solar map:** each step of the rocket is three days of rest
  (`MoveRocket`).

### Credits

- Each character carries its own credits; a new one brings 1,000.
- **The pool** (`money.pas`): POOL puts the credits of every player
  character and party-member NPC into the pool; DIVVY shares it evenly among
  them, the remainder a credit each to the first; TAKE gives the current
  character what it asks for, at most the pool. The booty menu and the shops
  have these commands, and fallen enemies' credits go into the pool. A shop
  empties the pool as it opens. Leaving the booty menu or a shop with
  credits still pooled loses them, after one warning, as in the original
  (the user, 2026-11-20).
- **Trading:** on the character sheet a character hands credits to another,
  at most what it has.

### Shops

- **The stock** (`eclvm.pas` `EclShop`): SHOP 4Bh names one of 16 lists of
  item templates (`gdconsts.pas` `itemListStarts`). SHOP 1 is Salvation's,
  2 to 15 the port's. The shop shows the list's last item first. List 9
  names templates 92 and 93, which `ITEM0.DAX` does not hold, so it offers
  eight items.
- **Prices** (`shop.pas` `ItemPrice`): the item's value (1 when it has none)
  times the multiplier in cell 7F6Dh, over 16. The scripts set 16 before
  each shop; a multiplier of 0 means the value.
- **BUY:** the current character's credits pay; when they fall short, all
  of them and the rest from the pool. A bought grenade or antidote joins a
  like bundle. A character carries at most 16 items.
- **SELL:** half the item's value, for each item of a bundle. An item worth
  nothing is taken away for nothing.
- **AMMO** (`ShopAmmoMenu`): every missile weapon the character carries,
  readied or not, with the price of a clip and of a fill. A clip is 10
  rounds and a weapon holds 250; a rocket launcher or a plasma thrower holds
  10 and is sold a round at a time. Each 10 rounds, or part of them, costs
  the weapon type's price, 5 to 100 credits (`gdconsts.pas` `ammoPrices`).
  The shop's multiplier does not apply. Topping up a weapon charges a whole
  clip for a part of one, as in the original (the user, 2026-11-20).
- **HALVE and JOIN** (the gear menu): HALVE splits a bundle of grenades or
  antidotes in two, the larger half staying; the menu offers it below 16
  items. JOIN gathers every like item into the chosen one, up to 100 a
  bundle, and the rest makes the next bundle. Weapons and armor never join.

### The outposts (the scripts)

The outpost's menu is the script's: script 99 of `ECL6` offers some of
BANK, DOWNTOWN, HOSPITAL, PORT and TRAINING, by the outpost (cell 4C31h).

- **The bank** keeps one account for the team, the four-byte cell 4CE2h,
  which every branch shows. DEPOSIT and WITHDRAW move credits between a
  member and the account; TRANSFER moves them from the account into the NEO
  salvage account, from which they never come back.
- **The NEO salvage account** (cell 4CE6h, 20,000 credits at the start)
  receives the salvage of every ship the team takes, and pays for
  everything at the port.
- **The hospital** heals a member who is not dead or gone, in any state,
  comatose included, to full Hit Points and okay. The fee is 160 times the
  multiplier (2,560 credits at 16), from the bank account when it holds
  enough, else from the member's own credits; else the hospital refuses.
  The shipped script checks only one byte of the fee (Bugs).
- **The port:** LAUNCH; REPAIR, system by system, at a price for each
  point of damage of 4 for the hull, 16 for the sensors, 12 the controls,
  14 life support and 28 the engines, times the multiplier over 16, and
  10,000 a K-cannon, 3,000 a missile mount and 1,000 a laser; FUEL, 20
  credits a unit up to 450; AMMO, a K-cannon reload for 1,500 (5 at most)
  and a missile reload for 300 (7 at most); MED SUP, a unit of medical
  supplies for 2,000 (10 at most, cell 4C63h).
- **Salvation** (script 17) charges nothing: its port refuels, repairs,
  rearms and restocks 10 medical supplies, and its doctor heals every member
  who is not dead, gone ones too, as in the original (the user, 2026-11-20).
- **The ship's clinic** (script 34, when the team takes to space hurt):
  one unit of medical supplies heals a member who is not dead or gone to
  full Hit Points and okay.
- **Training** (PROGRAM 0, at Salvation and the outposts) is free.

## Rules: ground combat (step B2)

Ground combat is done (2026-11-20). The rules are game-keyed functions in
`src/OpenGold.Rules.Original/src/buck_rogers_combat.cpp`
(`opengold/buck_rogers_combat.h`); the original-rules combat session
(`combat_session.cpp`) keeps its grid, turns and computer play, and runs a
fight of these games through them. A creature enters a fight as its own
record, its items and the `ITEMS` rows they name (`encode_ground_record`, its
character profile), and the fight leaves the record as it ends: Hit Points,
status, rounds and readied gear (`rules::VitalState::resources`), which the
after-fight treatment reads. No spell, class or AD&D effect runs in these
fights. `tests/countdown_tests.cpp` checks each rule and names the routine it
follows.

### The numbers a creature fights with

Worked out at every turn and attack (`charhlp.pas` `RecalcStats`,
`WeaponAdjust`):

- **Combat value:** the career's value, with Strength's adjustment unarmed or
  with a melee weapon (-5 at 1 to +4 at 21), Dexterity's missile adjustment
  with any other weapon (-6 to +4), and the weapon's plus and the Warrior's
  specializations in its type. A failed Zero-G roll takes 2; the side's
  tactics roll adds 1.
- **Damage:** unarmed, the record's own dice (a Warrior's two 1d3 punches a
  round, a Desert Runner's claws 1d3 + 1) with Strength's damage bonus (-4 to
  +10); with a weapon its `ITEMS` dice, its plus and the specializations, and
  Strength's bonus for a melee one. A Sonic Stunner's plus and
  specializations add to hit only.
- **Armor Class** climbs from 50: armor adds its row's value less 50 and its
  plus (Battle Armor with fields 12); dodging adds 2, or half the level from
  level 4; Dexterity adds -5 to +5 in front. The rear Armor Class is 2 below
  the front one before Dexterity.
- **Armor bits:** any body armor protects the body and the breathing; Battle
  Armor with fields adds its field; goggles, a breathing mask and an ECM
  package their own.
- **Movement:** 12, a Tinker's 9 and a Desert Runner's 15, halved past half
  the weight Strength allows; doubled in half-moves, as the frame counts it.

### The attack roll

- **To hit** (`fxengine.pas` `AttackRoll`): a d20, the combat value and the
  side's modifier must reach the target number. A natural 1 misses; a natural
  20 counts as 100. A helpless defender is always hit.
- **The target number** (`cbtacts.pas` `RollAttacks`, `ApplyRangePenalty`):
  the defender's Armor Class, or its rear one from behind, 2 lower for a
  Rogue from behind (the original's 2 higher is a bug, fixed; Bugs).
  A guard's shot with a ranged weapon is 5 harder; otherwise cover on the
  line past the shooter's first two squares adds 2, and range adds 2 past half
  the weapon's range and 5 past three quarters. Cover is difficult ground on
  the battlefield until step B3 maps the arenas' tiles.
- **From behind** (`IsRearAttack`): a one-square defender that has drawn two
  attacks this turn, struck along its facing or one direction to either side.
  A defender turns to face its first attacker of a turn.
- **Backstab** (`IsBackstab`, `CalcWeaponDamage`): from behind, with a
  weapon of range below 2, straight along the defender's facing, and on a
  Move Silently or Distract check (a Rogue's skill doubled). A Rogue's damage
  is multiplied by (level + 7) / 4, anyone else's doubled.

### What stops a hit

In the original's order (`CalcWeaponDamage`; `effhndlr.pas` at
`WEAPON_DAMAGE_DEFENDER`): the defender's immunities to rockets, microwaves,
heat and plasma, or lasers; half the time a slug from beyond the next square
dodged, a ranged shot from there or a melee blow resisted; an unkillable
creature never taken below 0; then a rocket stopped by chaff on the line, by
an ECM package half the time and by fielded battle armor three times in four;
a laser by aerosol mist on the line; a microwave beam by any body armor. A
Sonic Stunner stuns for 1d4 + 4 rounds unless the save of category 0 is made,
and does no damage. A creature poisonous in its second attack slot poisons
with each damaging hit from it: dizziness, or death as poisoned, unless the
save of category 1 is made.

### Area weapons and clouds

- **Projectiles** (`gdconsts.pas` `projectileKinds`): the dazzle grenade
  (blinds 1d6 + 1 rounds, does 1d3; save 3 cancels; goggles spare), the mini
  and explosive grenades (2d10, 4d10; radius 1; save 2 halves), gas (helpless
  2 to 7 rounds; save 1 cancels; a mask or body armor spares), chaff, aerosol
  mist, the plasma thrower (4d10, radius 3) and the rocket launcher (5d10,
  radius 2), both saved for half, and the stun grenade (stunned for its 1d4 +
  2 rounds; save 0 cancels). Item templates 17, 18 and 32 to 38 fire them.
- **A launch** (`CombatLaunchProjectile`) aims at a square. The damage is
  rolled once. A shot whose combat value and d20, less the range's penalty,
  come to 50 or less goes astray: up to four tries at a square farther out
  (`ScatterOffset`), the first within about four squares of the shooter,
  each later one up to twice as far as the last walked, as in the original
  (the user, 2026-11-20).
  Chaff on its way sets it off there. Everyone a clear line from where it
  lands reaches within its radius (2 a straight step, 3 a diagonal, at most
  twice the radius and 1) saves or takes it. The launch ends the shooter's
  turn and spends a round; a grenade bundle's last grenade goes.
- **Reloading:** the rocket launcher and plasma thrower, and a grenade
  launcher that fired, must be readied again, and readying a heavy item ends
  the turn (`viewchar.pas` `ToggleReadyItem`). A grenade goes 3 squares
  thrown, 12 from a readied grenade launcher (`ReadiedWeaponRange`).
- **Clouds** (`types.pas` `TAreaEffect`): gas, chaff and mist cover the
  square a grenade lands on and the eight around it, where the ground can be
  walked, for four rounds, or one in zero gravity
  (`fxengine.pas` `CountDownAreaEffects`). Gas leaves one who steps into it
  helpless for 1d6 + 1 rounds unless it saves (`ConsiderTerrainEffects`);
  chaff and mist stop rockets and lasers whose line crosses them.

### Ammunition

A ranged weapon's rounds are its quantity. It attacks no more often than it
has rounds, and spends one for each attack of its first slot (`cbtacts.pas`
`RecalcAttacksLeft`, `MakeAttack`). Attacks are a weapon's or the record's
rate a round doubled, a half rounded up in odd rounds, counted from round 0.
The shop's clips and fills are the after-fight rules' (`buck_rogers_upkeep`).

### The new commands

The combat menu (`combat.pas` `CombatMenu`, `ManualControl`) offers Move,
Target (a blow or a shot at an enemy, or a launch at a square), the readying
of items, Wait (`delay`), Quick, End and fleeing off the field, and:

- **INTIMIDATE** with the skill and an enemy next to it, unless dodging or
  after a failed Zero-G roll: on an Intimidate check each enemy next to it not
  helpless is scared unless it makes a save 0, and loses its next turn
  (`IntimidateTarget`, `effhndlr.pas` `EffScared`). It ends the turn.
- **AID** with a teammate next to it to treat (`viewchar.pas`
  `FindPatients`, `TreatPatients`, which the fight plays through the
  after-fight rules' `find_patients` and `treat_patients`, among the
  creatures on the medic's square and next to it): First Aid or Treat Light Wounds bandages
  the bleeding without a roll (unconscious, no longer bleeding); Treat
  Stun/Paralysis cures the stunned and dizzy on a check. It ends the turn.
  Treat Wounds and Treat Poisoning work only after the fight.
- **SPRINT** doubles the movement left; a sprinter may not strike or guard.
- **DODGE** ends the turn with the dodge's Armor Class until its next turn.
- **JETPACK** with a readied jetpack, out of doors, not again in a round: on
  a Use Jetpack check it lands on any free square it chooses and may still
  attack; on a failure it lurches through eleven random squares near it and
  lands on the last it may stand on, which ends its turn. Guards strike as it
  leaves and as it lands (`CombatUseJetpack`). The Rule Book says that ranged
  attacks on a flier count as long range; the program does not, and the
  source decides.
- **GUARD**, unless sprinting, dodging or holding an area weapon: a guard
  strikes once at an enemy that steps where its weapon reaches
  (`GuardersAttack`). No one strikes a creature for leaving.

### Skills in a fight

- **Initiative** (`cbtacts.pas` `BeginRound`): d10, Dexterity's missile
  adjustment and 1, at least 2, 6 less when surprised in the first round, 0
  outside 0 to 20, with a d127 to break ties; the round's order is set as it
  begins (`combat.pas` `OrderCreatures`).
- **Battle Tactics** (`RollTactics`): as each round begins, each side's able
  creatures, in order, roll until one succeeds, which gives its side 1 to hit
  for the round.
- **Leadership** (`UseLeadership`): as the fight begins, a member who is no
  companion, with the skill, may take a companion of Intelligence above 3 off
  the computer's control on a check. The others the computer plays.
- **Maneuver in Zero-G** (`RollZeroG`): in zero gravity each creature rolls
  it as its turn starts (not after waiting); a failure leaves it 2 worse to
  hit with a quarter of its movement until its next turn.

### Saving throws

A d20 against the record's save of the category; 1 fails, 20 saves;
blindness takes 4 (`fxengine.pas` `RollSavingThrow`). The fight uses
category 0 against stuns and intimidation, 1 against gas and poison, 2
against blasts, 3 against the dazzle and 6 against an acid strike.

### Injuries

Damage (`charhlp.pas` `InflictDamage`) leaves a creature at exactly 0
unconscious; below 0 it bleeds (dying), past 9 below it is comatose and past
29 dead. A bleeding creature's count starts at how far below 0 it went and
rises each round; past 9 it slips into a coma (`ProcessDyingCharacters`),
never death. A lethal poison leaves its victim poisoned. As the fight ends
the new commands' statuses go back to okay and blindness, helplessness and
stuns end (`combat.pas` `CombatDoneCleanup`); everything else, treatment
included, is the after-fight rules'.

### Difficulty levels

As [BUCK-ROGERS.md](../BUCK-ROGERS.md), Difficulty, sets them
(`apply_original_difficulty`): Novice, Cadet, Trooper, Veteran, Ace and
Admiral give the party's side +2 to -3 to hit (`AttackRoll`), the hostile
monsters (level + 2) quarters of their Hit Points as LOAD MONSTER loads them,
which become their maximum (`eclvm.pas` `EclLoadMonster`; Admiral twice), and
the table's to-hit and Armor Class (`original_monster_strength`, Novice
Easy's). `novice` is a value of the difficulty setting (`DifficultySetting`,
`difficulty.h`); only these games offer it (`offers_novice`), and the
others play it as Easy. The six names for the Options screen are
`buck_rogers_difficulty_names`.

### Not yet played

For step B3, when these games' fights load: the monsters' own powers at
their turn's start (acid spitting and an adjacent strike), an unkillable
companion's leap for cover from a blast, explosions breaking the arena's
tiles, and the computer's own Buck Rogers choices (grenades and readying
weapons again); the frame's computer plays these fights for now.

## Bugs

Fixed in OpenGoldBox, in both games unless named:

1. **The skill check graded by the wrong member** (`charhlp.pas`
   `SelectSkillCharacter`): the best roller acted, but its grade came from
   the value of the last member examined. The grade is now the roller's.
2. **A high skill value turned negative** (`SkillValue`): *Countdown* kept
   the value in a signed byte, so a level and ability past 127 (a career
   skill of 110 with a Dexterity of 18) went negative and failed every
   check; *Matrix Cubed*'s byte wrapped past 255. The value is no longer
   cut.
3. **Lowering a career skill forgot a general skill** (`skills.pas`
   `EditSkills`): taking a point back from a career skill at its start
   counted one general skill fewer, so raising and lowering a career skill
   let a character learn any number of general skills. Only general skills
   count now.
4. **Free points from Animal Training** (`EditSkills`,
   `AdjustPrerequisites`): a Rogue's Animal Training starts at 10 for free
   once Befriend Animal reaches 20, and lowering Befriend Animal to 19
   refunded those 10 as if they were spent, every time. The refund is now
   what was spent.
5. **Treat Critical Wounds' points stranded** (*Countdown* only,
   `AdjustPrerequisites`): lowering Treat Light Wounds below 30 took back
   Treat Serious Wounds but left Treat Critical Wounds with its points and
   an unmet prerequisite. It is taken back too, as *Matrix Cubed* does.
6. **The hail's skills** (script 34 of `ECL2`, the checks at 8DE8h, 8E67h
   and 8ED1h): the bluff, the threat and the call for help check 74 Befriend
   Animal, 70 Hypnosis and 69 Paint/Draw. Hypnosis and Paint/Draw cannot be
   learned, so two of the three choices could never succeed; the script was
   written against a skill table two shorter below 69. OpenGoldBox checks 76
   Fast Talk/Convince, 72 Intimidate and 71 Act there, and only where the
   shipped operand is the one expected (`script_check_skill`, which the
   script host reads each SKILL CHECK through once it runs these games in
   step B3; the user, 2026-11-20). The
   game's files are unchanged.
7. **DIVVY lost a share** (`money.pas` `ShareMoney`): the pool was divided
   among the player characters and party-member NPCs but paid only to the
   player characters (and to any member whose npc byte is 1 to 7Fh, who
   was not counted), so a party-member NPC's share vanished. OpenGoldBox
   shares the pool among the members POOL takes from.
8. **Handing over credits kept 16 bits** (`viewchar.pas`
   `TradeTreasureMenu`, `money.pas` `TradeMoney`): an amount above 65,535
   was cut to its last 16 bits, so giving 70,000 credits gave 4,464. The
   whole amount moves now.
9. **Buying or taking into a full bundle** (`shop.pas`
   `GiveItemCopyToCreature`): a character with 16 items, one of them a
   bundle of 100, who bought or took one more of it lost 99: the new item
   took the bundle's 100 as it joined, and was then taken back for the
   overload. OpenGoldBox leaves the gear as it was.
10. **The hospital's fee check** (script 99 of `ECL6`, the checks at 84F1h
    and 850Bh): the script writes the fee, 160 times the price multiplier,
    to cell 7F79h as a word and charges it as a word, but compares one byte
    of it with the bank account and then with the patient's credits. At the
    multipliers the game sets that byte is 0, so the hospital never
    refused: it charged the bank account into debt, and after it the
    patient's credits. OpenGoldBox reads the word there
    (`script_operand_tag`, which the script host reads each operand's width
    through once it runs these games in step B3). The game's files are
    unchanged.
11. **One rear attack missed** (`cbtacts.pas` `IsRearAttack`): the test of
    one direction either side of the defender's facing did not wrap, so a
    defender facing north was never struck from behind by a blow heading
    north-west. Every such pair counts now.
12. **Gas left its victims helpless for good** (`CombatLaunchProjectile`):
    the gas grenade's helplessness lasts as many rounds as its damage dice
    roll, and its dice are 0d0, so the effect never ran out. The Log Book
    gives two to seven rounds, as the cloud's own gas gives; a blast's gas now
    lasts 1d6 + 1 rounds.
13. **A failed jetpack flight drifted south** (`CombatUseJetpack`): each lurch
    picks one of the 24 squares round the last, but the rows ran from one
    above to three below. They run two either way now.
14. **Launches rolled as if unarmed** (`CombatLaunchProjectile`): the rocket
    launcher, the plasma thrower and a grenade launcher were put away before
    the launch's roll, so the shot used the unarmed combat value (Strength in
    place of Dexterity, no plus or specialization), no range penalty for the
    rocket launcher and plasma thrower and a thrown grenade's range of 3 for a
    launched one; and an immunity to plasma never met the plasma thrower. The
    roll now uses the weapon fired.
15. **A stray shot's fourth landing was lost** (`CombatLaunchProjectile`):
    of four tries at a stray landing, a fourth that succeeded was taken as no
    landing at all, and the shot came down on its target. It lands there now.
16. **LOTEK's missing wall tile** (`combatst.pas` `LoadCombatTileSet`,
    `BuildDungeonArena`): the arenas of area kinds 82h and 88h copy 53 tiles
    from `LOTEK`, which holds 52, and a doorway in a north wall lays combat
    terrain 44, whose tile is the 53rd: the original drew it from past the
    picture's pixels. It shows `LOTEK`'s last tile, a piece of the same walls.
    No other arena asks for a tile its file lacks (`MARSCOM` and `VENUSCOM`
    included, checked on every ground with many dice).
17. **A Rogue was worse from behind** (`cbtacts.pas` `RollAttacks`): a Rogue
    striking from behind needed a number 2 higher, so it hit from behind less
    often than anyone. These games count Armor Class upward, and the earlier
    games' thief bonus kept its sign unturned. The number is 2 lower now (the
    user, 2026-11-20).

## Ship combat

Step B2 builds the fight that SHIP COMBAT (1Fh) starts and the repairs after
it, from `shipcbt.pas` (`ShipCombat`) and `postcbt.pas` (`RepairShip`), as
`ShipCombat`, `repair_ship` and `ship_combat_memory`
(`src/OpenGold.Rules.Original/include/opengold/buck_rogers_ship_combat.h`),
checked by `tests/buck_rogers_ship_combat_tests.cpp`. The session takes the
crew as the team's members, their records and stuns as the upkeep rules keep
them (`TeamMember`, `buck_rogers_upkeep.h`), its dice from outside, and the
three ship-combat options below (`ShipCombatOptions`). Step B3 joins it to
the party and gives it a screen. *Matrix Cubed* plays it the same way
([matrix-cubed.md](matrix-cubed.md), "Ship combat").

- **The start:** SHIP COMBAT's operands are the enemy ship (1 to 5, `SHIPS`
  record 39 plus the number), an unused value, the enemy pilot's skill and
  the enemy's to-hit number. The pilot's skill is what the enemy dodges a ram
  with; the enemy's gunnery becomes 60 less the to-hit number. Beating the
  ship is worth 300, 3,000, 12,600, 2,500 or 4,600 experience by its number.
- **The ships:** a ship's maneuver is signed: the 50-ton Hubble is -2, the
  500-ton Mohave +2. It adds to the total of anyone shooting at the ship, so
  small ships are harder to hit. Its reaction weighs in the pilot roll. A
  mount's kind has a top bit for the crew's weapons; without it only the
  pilot fires the mount. The weapons are the beam and pumped lasers (no
  rounds), the gyrocannon, missile and heavy missile mounts, the K-cannon and
  the light and heavy acceleration guns.
- **The opening:** the range is 8. Each side rolls a d10, and the team adds
  its ship's reaction less the enemy's. When the team comes out ahead, the
  enemy moves first in every round; otherwise the team does (decision 1
  below).
- **A round:** the team's members act one by one in party order, those who
  are okay; the enemy plays its whole turn before or after them. Moves change
  the range at the round's end, so every shot and order of a round uses the
  range it began with. A range below 0 becomes 0, and once it falls below 4
  a losing enemy starts to flee. Past 8 the enemy is lost from the scanners:
  the fight ends with outcome 1 and no experience. When no member can act
  and a round passes in which the enemy did nothing that could change the
  fight (no shot, no ram that might land, no reload, no boost or jury-rig
  that might work, no move), the enemy breaks off, and the fight ends the
  same way (Bugs).
- **The pilot** fires every pilot's weapon it can, each mount as many times
  as it has working weapons (FIRE), or the same at one system (TARGET). It
  may CLOSE or WITHDRAW by its ship's speed once a round, and still act after.
  At range 1 or less it may RAM, and BOARD a ship that can no longer move.
- **The rest of the crew** fire one shot of the crew's best weapon, LOAD an
  empty mount that has reloads left, or JURY RIG with the skill: a mount gets
  one weapon back, a system 2d10 points; a system is rigged once a fight, and
  fuel never. COMMAND, offered while the pilot is dead or incapacitated,
  takes the helm, once a round, and the member acts on as the pilot
  (decision 2). AID is the ground fight's ("AID: medics in a fight and
  aboard" above), with every member of the crew a patient. BOOST ENGINES checks
  Repair Nuclear Engine at half: a success adds a point of speed, at most two
  a fight; a failure costs the engines 1d100. SENSORS checks Sensor Operation
  at double and shows the enemy ship.
- **To hit:** a d20 at or above 60 less the target's Armor Class and the
  shooter's total, never more than 20 (no natural 1 or 20). The total is the
  gunner's combat value, its Dexterity's missile adjustment and, for a
  Rocketjock, its level; the team adds 3; the enemy uses its gunnery. The
  weapon's bonus (+1 to -3) is added, 5 taken off beyond range 4 and 2
  beyond range 2, 5 off when the shooter's controls are out, 3 added when the
  target's are, and the target's maneuver added. An aimed shot first needs an
  even chance.
- **Hits:** a shot not aimed lands on the hull 28 times in 100, the sensors
  9, the controls 9, life support 13, fuel 10, the engines 18 and the
  weapons 13. A hit on a destroyed system falls on the hull. A hit on the
  weapons takes one working weapon, picked by a die of their count. Half a
  hit's damage is the chance its ship loses a crew member; the enemy's crew
  sets how many actions it has. A hull at 0 destroys its ship.
- **The team's crew:** after each run of shots from one enemy mount, each
  okay member is hurt a fifth of their damage percent of the time, for 2 to
  10, unless a d20 makes its saving throw (save 2). When the team's life
  support is destroyed, every okay member takes 2 to 10 once, with no saving
  throw. Hit Points below 0 bring dying, a coma at 10 past and death at 30
  past (`InflictDamage`); a pilot who falls leaves the helm, and the dying
  fall into a coma after ten rounds. When the team's ship is destroyed, the
  team dies with it.
- **Speed:** the ship's speed and its boosts, halved with the hull below
  half, 1 less with the engines below half or 3 less below a quarter, at
  least 1; 0 without controls, fuel or engines.
- **Ramming:** each hull takes the other ship's tonnage, unless the rammed
  ship, moving, dodges: the enemy tries when its hull is the weaker, the team
  when the player says so. The dodge is the pilot's Pilot Rocket, or 0 with
  no one at the helm (decision 4), plus ten for each point of speed the
  rammed ship has over the rammer.
- **The enemy** weighs both ships (hull, a third of controls, fuel and
  engines when it can move, and its weapons' damage): it flees when it is
  worth less than half the team's ship, attacks when it is worth more, and
  holds otherwise. It rams at range 1 or less when its hull beats both the
  team's hull and the team ship's tonnage and its weapons do not outweigh the
  team pilot's dodge; else a fleeing enemy withdraws by its speed, a holding
  one withdraws up to 3 at range 2 or less, and an attacking one, or any at
  range 5 or more, closes. Its pilot fires every pilot's weapon. Then all
  but a tenth of its crew (rounded up) reload or fire one shot of the crew's
  weapons each, and that tenth boost or jury-rig with the ship's own skill. A
  fleeing, slower enemy may boost once a round, with no limit (decision 3).
- **The end:** the outcome goes to 4BBAh: 0 when the enemy was destroyed or
  boarded, 1 when the team's ship was destroyed or the enemy lost or broke
  off. 4D7Ch gets
  post-combat mode 1 after boarding, else 0, and 4B80h 0. The team's ship
  goes back to 4D00h with its jury-rigging taken off. The enemy's systems,
  type, crew, gunnery and working weapons go to 4D6Ch to 4D82h, cleared when
  the enemy got away. Script 34 reads them: after a boarding it loads script
  52, the fight through the enemy's airlock, bridge and engineering, which
  sets post-combat mode 2 so the repairs follow it; otherwise it pays the
  wreck's salvage, by the saved systems and weapons, into the NEO account at
  4CE6h.
- **After the fight**, unless the team boarded: the medics treat the wounded
  (B2's character rules), then `repair_ship`. The ship-combat experience is
  shared among the enabled members. Each damaged system but fuel is mended
  by the first okay member whose repair skill succeeds (Repair Rocket Hull,
  Repair Electrical, Repair Mechanical, Repair Life Support, Repair Nuclear
  Engine): 20, 4, 4, 8 and 12 d10. Each broken weapon is fixed by the first
  whose Repair Weapon succeeds. When experience was shared, 9 to 15
  sixteenths of the enemy's fuel and 1d16 more fill the tanks as far as they
  go, and the enemy's tanks are emptied. After a boarding the repairs follow
  the fight on the enemy's decks, and the fuel comes the same way from the
  captured ship as the fight's end saved it (decision 5).

### Bugs

| Bug in the original | In OpenGoldBox |
|---|---|
| The ship's explosion hurts only members who are okay, so the unconscious and dying live on, and its 5d20 plus maximum Hit Points can leave an okay member in a coma (`DamageCrew`) | Every member aboard dies |
| A skill check's chance is kept in a byte: a slow pilot's dodge below 0 wraps round to a certain dodge, and one above 255 to a small chance (`SkillCheck`) | Below 1 never succeeds, 100 or more always does |
| A hit's chance to kill a crew member, half its damage, is passed in a byte, so a ram of more than 511 wraps round to a small chance (`DamageShip`) | It stops at certain |
| The enemy adds up its pilot's weapons in a byte read as signed, so weapons worth more than 127 (the Adder's lasers and K-cannon, 150) count as less than nothing, and it rams whenever it may (`EnemyFireWorthwhile`) | The weapons count in full |
| With no one at the helm, the enemy weighs the dodge of a pilot who is not there (`EnemyFireWorthwhile`) | The dodge the ship would try: 0, or its own pilot value with Empty helm dodges on |
| The end of the fight takes a system's jury-rigged points off only when they are fewer than its points, so a system rigged back from 0 stays mended for good (`RemoveJuryRigs`) | Every system loses them, down to 0, as the mounts do |
| A ship destroyed by its own ram still fires a shot (`EnemyTurnBody`) | It fires no more |
| COMMAND leaves the pilot it replaces marked as pilot, and the first marked member in party order takes the helm in the next fight | The replaced pilot stands down |
| AID looks its patients up in the ground fight's table of combatants, which a ship combat does not fill (`TreatPatients`) | It treats the crew, by the ground fight's AID |
| A fight in which no member can act and the enemy can neither destroy the team's ship nor get away never ends (the user, 2026-11-20) | After a helpless round in which the enemy did nothing that could change the fight, it breaks off: outcome 1, no experience |
| LOAD and JURY RIG pick a mount by the first letter of its weapon's short name: the acceleration guns ("acl-Cann", "hv-Accel") can never be picked, and with two mounts of a kind the last is taken, needed or not | The player picks the mount |

### Decisions

Decided by the user on 2026-11-20, for both games. Three are options on the
Options screen while a Buck Rogers game plays, one key per game
([CONFIGURATION.md](../CONFIGURATION.md); [DEVIATIONS.md](../DEVIATIONS.md)).

1. The team that wins the pilot roll lets the enemy move first in every
   round. Kept, as in the original.
2. **COMMAND every round** (`[command_every_round]`). The original offers
   COMMAND to any crew member once a round, even with the pilot well, so a
   second member may ram again in the same round; the Rule Book offers it
   only when the pilot is dead or incapacitated. Off, the default, plays the
   Rule Book's way; on, the original's.
3. The enemy may boost its engines without limit; the team twice. Kept, as
   in the original.
4. **Empty helm dodges** (`[empty_helm_dodges]`). With no one at the helm the
   original's ship dodges a ram by its own pilot value, 100 for the Maelstrom
   Rider, better than most pilots. Off, the default, dodges with 0, and the
   enemy weighs that dodge; on, by the ship's own value.
5. **Fuel from a boarded ship** (`[boarding_fuel]`). After a boarding the
   original's repairs take no fuel (*Matrix Cubed* reads it from a ship that
   is gone), though the Rule Book says the captured ship's fuel is
   transferred. On, the default, transfers it, as after a kill; off takes
   none.
6. A fight in which no member can act and the enemy can neither destroy the
   team's ship nor get away never ends. Fixed as a bug: the enemy breaks off
   (Bugs).

## The game in the engine (step B3.1)

The first part of B3.1 is done (2026-11-20): the engine runs *Countdown*'s
own scripts, maps, pictures and fights with a team its creation rules made.
The three new screens (creation and skills, the solar map and ship combat)
are designed from mockups; ship combat plays on its screen ("The ship
combat screen", below), and the session stops at a hand-off for the others. `tests/countdown_campaign_tests.cpp` checks it on the game's files,
and with `--walk` it is `opengold_countdown_walk`.

### The team in the campaign

A member is its game's record, whole (`buck_rogers_characters.h`): its
creation draft holds the `GroundRecord` (the 259-byte record, its items, its
second item list and the `ITEMS` rows they name) as made, and the member's
vitals keep it as it is now (`CampaignParty::game_record`,
`keep_game_record`), as the combat session gives it back after a fight. The
sheet the party's screens read is drawn from it (`buck_rogers_sheet`). The
campaign save keeps both without a new format: format 53 already saves a
draft's rules data and the vitals' resources, and a saved team loads and
saves again byte for byte. The walk's team is made in code by the creation
rules (the creation screen comes later).

### Views, pictures and the text

The maps are the shared `GEO` reader's; the walls are each disk's
`WALLDEF` with the same disk's `8X8D` tiles, and the backdrops its `BACK`
pictures: the set's own indoors (an event byte with bit 80h) and the set's
number plus 80h out of doors (`view3d.pas` `LoadBackdrop`). Pictures from
112 are `BIGPIC`s, below `PIC` animations, all in the 256-color layout. A new
game starts as `GameInit` sets it: Trooper (or the setting's level), area
kind 1, message speed 4 and the Maelstrom Rider in zone 0 from 4D00h. The
current character is served from 7C00h as `SpecialReadZ1` does: the name,
the record's bytes 10h to B9h (its credits and experience four bytes, its Hit
Point maximum a word), the specializations at C4h, then the status, side,
attack bonus, Armor Class, Hit Points and movement at their own places; what
a script writes there goes back into the record.

### The script machine and its host

Numbers are the original's LongInts: each operand is read and written in
the width its tag names (a byte, a word or four bytes; a four-byte cell is
kept as its low word and the word two cells on), signed where the original
reads four bytes. The machine runs COMPARE, the arithmetic, SAVE and PRINT
that way, and CONTINUE PROMPT, YES NO, the horizontal ACTION MENU, LOGBOOK
ENTRY (the entry's number and what it holds; never the Log Book's text),
CLEAR AND STOP (which ends the script), SKIP, LOOP START and LOOP NEXT, and a
RETURN with nothing to return to, which ends the script. Every operand's
width goes through `script_operand_tag` (the hospital's fee, Bugs). Past a
script's end its zone reads 0, as the original clears it before loading.

The host (`buck_rogers_host.cpp`) answers SET SPEAKER, the SKILL CHECKs
(22h, 23h, the skill through `script_check_skill`, the roller's place and
grade written where the operands name cells), SKILL CHECK WITH DAMAGE, DAMAGE
with its saves and attack rolls, AWARD EXPERIENCE, FIND ITEM, DESTROY ITEMS,
SWAP ITEM LISTS, PLAY SOUND (the driver's sound commands; nothing above 127),
DISTANCE AHEAD, PICTURE MODE, LOAD MONSTER, COMBAT, SOLO COMBAT, TREASURE,
SHOP, ADD NPC and PROGRAM 0 and 9. LOAD FILES' map 127 is no map (an outpost
or the solar map, shown as pictures), and none loads in a wilderness. CALL's
sound routine plays nothing, as its selector is never set. The other
commands are the Curse engine's, as for the earlier games.

### Ground fights

A fight a script loads is played by the combat screen on the arena the
area's kind names (`buck_rogers_battlefield.h`): `HITEK` indoors, `LOTEK` for
kinds 82h and 88h, laid from the walls round the team's square
(`BuildDungeonArena`, its decoration seeded by the script's number as the
original seeds it), and `MARSCOM` or `VENUSCOM` scattered by the wilderness
ground cell 4BB7h (`BuildWildernessArena`). Each cell is one of the 154 kinds
of combat terrain: an impassable one or one that blocks a line of fire is an
obstacle, one that gives cover difficult ground (the line's cover past its
first two squares). Bit 10h of the area's kind is zero gravity. Creatures are
`MON0CHA` records with `MON0ITM`'s items, drawn with the disk's `CPIC` icon;
the team fights with its `CHARS` icons. The fight's end (`PostCombat`): the
fallen enemies' experience, shared by the members who can act; their
credits to the pool and their gear to the booty unless cell 7EE3h says not;
the wounded (`recover_after_fight`) and the medics (`treat_injuries`) unless
cell 4B80h says not; a team that fled leaves behind those still on the
field; the result in 7EC7h and the creatures defeated in 7EC8h, and the
script's fight cells cleared. The booty and a shop are menus the session
offers in the original's place (Take, Divvy, Pool, Exit; Buy, Sell, Ammo,
Pool, Exit), with their words in Spanish too.

### Outposts, the wildernesses and music

The outposts are script 99's menus as they are: the bank, the hospital, the
bars, libraries and shops downtown, the port's launch, repairs, fuel, ammunition
and medical supplies, and training, which is PROGRAM 0's team menu (one level
a member, free; the skill points it gives are the skill screen's). PROGRAM 9
is the rest menu (a day a Rest). On Venus and Mars (area kinds A2h and A8h)
the team steps about the big picture as on Pools of Darkness's area map, 38
by 15 squares, a step half a day. *Countdown*'s music is the title's alone:
`MUSIC.DAX` record 2 from its first song, at the later games' tempo; no
script starts music.

### Hand-offs to the new screens

- **The solar map**: `TourSnapshot::solar_map` is set while PICTURE MODE 2 or
  the area kind 70h is on (script 34). The session runs the script's entries;
  the screen will move the rocket (`explore.pas` `MoveRocket`), set the
  bodies' squares from 4B85h and the rocket's from the orbit's cells, and
  pass the days. A new game's rocket and orbit (`InitShipAndOrbit`) are the
  screen's to set.
- **Ship combat**: played on its own screen (below, "The ship combat
  screen").
- **Creation and skills**: a team is made in code; training's skill points and
  a Warrior's specialization wait for the skill screen.

### The ship combat screen

SHIP COMBAT leaves the phase `ship_combat` with `TourSnapshot::ship_combat`
(the enemy's type, the unused operand, the pilot's skill and the to-hit
number), and the game opens the ship combat screen
(`src/OpenGoldBox/ship_combat_view.cpp`, `scenes/ship_combat.tscn`). The
session opens the fight with the player's options (`ship_fight`): the team's
ship as zone 0 keeps it at 4D00h, the enemy's `SHIPS` record named as the
next ship of its type (`LoadShipData`: five names a type, Van Allen first of
the Hubbles, counted in 4D91h on), the team in party order, and the
script's dice (`buck_rogers_ship_fight.h`). The screen draws the fight and
forwards the player's orders to the rules; nothing in it decides a rule.

- **The scene** is the enemy's picture, ship n drawn with picture n - 1 of
  `SHIPS.DAX` (`LoadShipData` loads picture `id mod 5`, its id the operand
  less 1), with its maneuver (Closing, or Retreating after its last move
  took it away) and the range at its top left, and its name and type. The
  picture grows smaller with the range, an option (`[ship_picture_shrinks]`,
  [DEVIATIONS.md](../DEVIATIONS.md)).
- **A range track** marks the range and, over each step, the team's weapons
  that reach no farther (an OpenGoldBox addition).
- **Both ships' systems**, hull to engines, with the original's icons
  (pictures 27 to 32 for the team's ship, 33 to 38 for the enemy's), a bar
  and their values: the team's in light cyan, the enemy's in light green, a
  damaged system in yellow and a destroyed one in red, as the original's
  meters and weapon list colour them. With the team's sensors destroyed the
  enemy's systems are not shown (`DrawSystemMeter`).
- **The weapons**: who fires each mount, its working weapons, rounds,
  reloads and reach. The enemy's rounds and reloads read "?" until a
  Sensors check succeeds, while the team's sensors work (`VALUE_SENSED`).
- **The orders** in the original's order (`BuildPlayerMenu`): the pilot's
  Fire Target Quit View / Close Withdraw Ram Board, the crew's Fire Target
  Load Jury rig Quit View / Command Aid Boost engines Sensors. An order the
  rules do not offer stands greyed out (the user, 2026-11-20). Target asks
  for the system, Load for the weapon and Jury rig for the system or weapon
  (the player picks the mount: Bugs above); Ram asks to confirm, as the
  original does, and an enemy's ram asks whether to try to dodge. The keys
  are the original's letters. View writes the ship's figures, or the acting
  member's ship skills, into the log.
- **The log** tells the fight's events in OpenGoldBox's words, and after the
  fight what followed it: the medics' work, the experience, the repairs and
  the fuel taken.
- **The crew column**: each member's CHARS icon, career, race and gender,
  Hit Points and state; the pilot starred, the member whose order is awaited
  lit.

At the fight's end the screen gives it to the session (`finish_ship_combat`),
which plays what follows (`follow_ship_fight`, `postcbt.pas` `PostCombat`):
a team with no one left alive is destroyed; else the wounded recover, and
unless the team boarded the medics treat them and the crew repairs the ship,
sharing the experience. The end goes into the script's memory
(`ship_combat_memory`: the outcome at 4BBAh, the team's ship at 4D00h, the
saved enemy and the post-combat mode at 4D7Ch) and the team's records, and
the script goes on. After a boarding script 34 or 95 loads script 52, the
fight through the airlock on the decks of the ship type 4C61h names; when
that fight's script sets post-combat mode 2, the repairs follow it, with the
ship fight's experience and the captured ship's fuel (decision 5), and the
mode returns to 0. *Matrix Cubed* plays its fights on the same screen.

`tests/ship_combat_view_tests.gd` (`opengold_godot_ship_combat`) clicks the
screen through seeded fights of the walk's team made a ship's crew (its
Rocketjock trained in Sensor Operation, its Engineer in Repair Nuclear
Engine), from the campaign save `opengold_countdown_campaign_tests` writes:
the opening, Sensors, Target's choice, the Engineer's orders, a system hit's
colours, boarding, withdrawing and the picture's size, at 1920 by 1080 and
1120 by 800. `ship_fights_play_through_the_session` checks the session's
part: the call, the opening, the end in the script's memory, the next name,
the pilot kept, and a boarding leading to script 52's fight.

### The walk

`opengold_countdown_walk` starts a new game for each map, enters it, looks
at every square and plays its event: the first choice of a menu, then each
in turn until each has come twice; every fight played by the game's own
computer on both sides; treasure left; every ship fight played. Then it
enters every script without a map of its own (script 99 at each of its
thirteen outposts) and steps about the wildernesses. On 2026-11-20, with
the ship fights played, it walked 15 maps and 20 scripts without a map,
played 437 ground fights (zero gravity in 286 of them, and on `HITEK`,
`LOTEK` and `MARSCOM`) and 23 ship fights (21 boarded, one won outright, one
lost or broken off), and met nothing the engine lacks; it reached the solar
map from 17 places. Map 99 is the Mariposa's top, which script 95 enters on
the escape; the walk notes it.
Places the solar map's script names first (the outpost, 4C31h and 4C2Fh) are
set by the walk, as aids. Ship fights are played through the session as the
screen plays them, by a simple player's orders (board when it may, close,
aim at the engines, fire, load); no walk of the maps reaches one without
flying the solar map, so the walk runs the two fixed ones from the
instruction where their setup begins (ECL2:34 8A54h, the hail a RAM
cruiser does not believe, a Hubble; ECL6:95 8B63h, the escape from the
Mariposa, an Adder), each until the team boards. Map 52, the boarded ship's
decks, is entered by boarding the Hubble, as a player enters it; on
2026-11-20 the walk's team, fresh from creation, lost the fight through the
airlock on every one of twenty sets of dice, so the walk notes the map as
one it could not stand on and walks the other maps.

## Next

Step B3.1's second part: the three new screens on these hand-offs, then the
route ([BUCK-ROGERS.md](../BUCK-ROGERS.md)).
