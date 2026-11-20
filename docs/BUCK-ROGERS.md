# The Buck Rogers games

The plan for OpenGoldBox to play the two *Buck Rogers XXVc* Gold Box games
from start to finish under their original rules: *Countdown to Doomsday*
(1990) and *Matrix Cubed* (1992). It follows the method of the *Forgotten
Realms* games ([GAMES.md](GAMES.md)) and of the *Krynn* games
([KRYNN.md](KRYNN.md)). Written 2026-11-19. B0 (groundwork) done
2026-11-19; B1 (the reading census) done 2026-11-19; B2 (the Buck Rogers
rules) done 2026-11-20.

Each game must meet the standard in [PLAYABILITY.md](PLAYABILITY.md)
("Acceptance and scope"). The route reaches the game's actual winning event
through player commands. The party earns its experience and equipment, or
comes from a verified import. Movement is physically valid and saves work.
The test forces no results, grants nothing and moves no one. SRD rules are out
of scope: no SRD work for these games, as for every game but *Pool of
Radiance*, until all twelve games win under their original rules.

## Where the files are

| Game | Folder | Game files | Manual, log book, cluebook | Reconstructed source |
|---|---|---|---|---|
| *Countdown to Doomsday* | `buck-rogers-countdown-to-doomsday` | `~/GoldBox/buck-rogers-countdown-to-doomsday` | `~/GoldBox/docs/buck-rogers-countdown-to-doomsday` | `~/src/unsafe/buck-rogers-countdown-to-doomsday` |
| *Matrix Cubed* | `buck-rogers-matrix-cubed` | `~/GoldBox/buck-rogers-matrix-cubed` | `~/GoldBox/docs/buck-rogers-matrix-cubed` | `~/src/unsafe/buck-rogers-matrix-cubed` |

Each docs folder holds the Rule Book (manual), the Log Book, the Cluebook and
the Data Card (refcard). *Countdown*'s Data Card is the Amiga one. The rules
tables (careers, skills, advancement, weapons) are in the Log Books, not the
Rule Books.

The sources rebuild the shipped DOS programs byte for byte: *Countdown* 1.3 in
Turbo Pascal 5.5, *Matrix Cubed* 1.0 in Turbo Pascal 6. Where the source and a
manual disagree, the source shows what the original does. The sources carry no
license. OpenGoldBox learns from them and writes its own code and text, and
copies neither. It also copies no text from the manuals.

## What the games are made of

Both games are the *Curse* engine family. About 41 of *Countdown*'s units
have *Curse*'s names (`eclvm`, `eclmenu`, `combat`, `cbtacts`, `view3d`,
`saveload`, `daxio` and the rest). The magic, camp, temple and ending units are
gone (about 5,500 lines). New units hold the skills (`skills.pas`, 558 lines),
the solar map (`orbitmap.pas`, 166) and ship combat (`shipcbt.pas`, 1,311).
`chargen`, `charhlp`, `postcbt` and `gdconsts` are largely rewritten.
*Countdown* is about 30,000 lines.

*Matrix Cubed* is the same program with new data. Its opcode table, record
layouts and save format are unchanged. Ship combat, the solar map, combat,
shops and money behave the same. The changes are small and few:

- creation starts at level 7 with preset skills;
- the level limit is 20, not 8;
- it can bring in *Countdown* characters;
- music is XMI through the Miles (AIL) drivers;
- PROGRAM 2 ends the game;
- the copy-protection table is different.

`ITEMS`, `ITEM0.DAX`, `SHIPS.DAX`, `MUSIC.DAX`, `CHARS.DAX`, `COMSPR.DAX`,
`LOTEK.DAX`, `MARSCOM.DAX`, `VENUSCOM.DAX` and `8X8D0.DAX` are byte-identical
in the two games.

| | *Countdown to Doomsday* | *Matrix Cubed* |
|---|---|---|
| Scripts | 25 in `ECL1`-`ECL6` | 33 in one `ECL1` (ids 1-114) |
| Maps | 17 in `GEO1`-`GEO6` | 25 in one `GEO1` |
| Monsters | 54 in `MON0CHA` (263-byte records, 259 read) | 63 in `MON0CHA` (259 bytes) |
| Music | `MUSIC.DAX` | `BUCKA`/`BUCKB`/`BUCKC.XMI`, timbres `FATV9.*`, drivers `*.ADV` |
| New game starts in | script 16 | script 16 |
| Winning event | scripted, no engine ending | scripted, ends with PROGRAM 2 |

The record counts come from the DAX headers. Champions had 22 scripts and 15
maps, so *Countdown*'s route is about as long, and *Matrix Cubed*'s is about
half as long again.

## What carries over

These engine parts serve the games as they are, or with a new per-game entry.

- **DAX archives:** the same 9-byte index and run-length packing (`daxio.pas`
  `LoadDax`, `DecompressDax`). OpenGoldBox reads them with
  `decode_dax_archive` and `decode_dax_record`
  (`src/OpenGold.Formats/src/formats.cpp`). The first matching id wins, as for
  Champions' duplicate ids.
- **Maps:** `GEO` maps are the shared format (`geo_map.cpp`,
  `src/OpenGold.Core/src/map_catalog.cpp`).
- **The 3D view and walls:** `WALLDEF`, `8X8D` and `BACK` backdrops, as the
  VGA Realms games load them (`view3d.pas`; OpenGoldBox's
  `load_vga_wall_art` and `vga_wall_set` in
  `src/OpenGold.Core/src/area_loading.cpp`, drawn by `exploration_view.cpp`).
  Both games offer EGA and VGA modes (`gameboot.pas`). Their portraits are VGA
  key frames with run-length delta frames (`portrait.pas`, `vgaframe.pas`),
  which `vga_picture.cpp` and the census will be checked against.
- **Pictures, sprites and fonts:** `PIC`, `BIGPIC`, `CPIC`, `SPRIT`, `CHARS`,
  `COMSPR`, `CURSOR` and `BORDERS` use the readers the Realms games use
  (`formats.cpp`, `vga_picture.cpp`).
- **The script machine:** opcodes 00h to 41h keep the Realms numbering, and
  most keep their meaning. The memory zones start at 4B00h, 7C00h and 7A00h,
  as in *Curse*, *Secret*, *Gateway* and *Champions* (`zone_starts` in
  `src/OpenGold.Core/src/ecl_address.cpp`). The script window is 8000h to
  A400h (Champions' ends at 9E00h). AWARD EXPERIENCE (41h) reads a whole
  word, as in Champions. The game gets its own opcode table in
  `game_opcodes`, its memory in `ecl_memory` and its engine routines in
  `ecl_engine_routines` (`src/OpenGold.Formats/src/ecl.cpp`). The interpreter
  is `EclMachine` (`src/OpenGold.Core/src/ecl_machine.cpp`).
- **Combat's frame:** the tactical grid, the ten segments of a round,
  initiative from Dexterity, MOVE, TARGET with NEXT/PREV/MANUAL, WAIT, GUARD,
  QUICK and END, facing and rear attacks, morale and the computer's control
  of opponents and NPCs. These are the shape of the original-rules combat
  session (`src/OpenGold.Rules.Original/src/combat_session.cpp`). The
  battlefield tile sets (`HITEK`, `LOTEK`, `MARSCOM`, `VENUSCOM`) work like
  Champions' `DUNGCOM` and `RANDCOM` (`dungeon_battlefield.cpp`).
- **Saving throws:** a d20 against a save in each category, as the Realms
  games roll them (`fxengine.pas` `RollSavingThrow`).
- **NPCs:** joining, leaving, morale and computer control (ADD NPC, LOAD
  CHARACTER, the NPC flag).
- **Shops, pooling and dividing money, the booty menu and trading** have the
  Realms games' shape (`shop.pas`, `money.pas`; OpenGoldBox's
  `shop_sale.cpp`, `coin_purse.cpp`).
- **Training halls** (`training_hall.cpp`): one level at a time, and excess
  experience dropped to one point short of the next level, as in the Realms
  games. Buck Rogers training is free, as Champions' is.
- **Modify Character** (`modify.cpp`): the original lets a new character's
  abilities and Hit Points be changed before it adventures (`chargen.pas`
  `ModifyCharacterMenu`, `HpMinimum`, `HpMaximum`).
- **Difficulty levels:** the original's LEVEL menu (Novice, Cadet, Trooper,
  Veteran, Ace; Trooper the default) gives each hostile monster
  (level + 2) / 4 of its Hit Points as it is added (`eclvm.pas`), as
  *Secret*'s levels do. OpenGoldBox already maps such levels
  (ORIGINAL-RULES.md, Difficulty): Cadet, Trooper, Veteran and Ace are Easy,
  Normal, Hard and Nightmare, Admiral (Infernal) goes past Ace, and Novice
  is a new level below Easy (see Difficulty, below). The table then adds only
  its to-hit and Armor Class.
- **Saves:** `savgam?.dat` holds the data set, the three memory zones, the
  party's place and view state. OpenGoldBox's campaign save
  (`src/OpenGold.Core/src/campaign_save.cpp`, format 53) records the game and
  its script memory. A raise of the campaign format is claimed on the session
  board first.
- **Music:** `MUSIC.DAX` is the bank family that `music_bank.cpp` and
  `music_render.cpp` read for *Gateway*, *Pools of Darkness* and *Treasures*.
  *Matrix Cubed*'s XMI files are what `decode_xmi`
  (`src/OpenGold.Formats/src/ua_media.cpp`) reads for *Unlimited Adventures*.
- **Journal references:** the LOGBOOK ENTRY command (3Fh) tells the party which
  Log Book entry to read. OpenGoldBox shows the entry's number, as *Pools of
  Darkness*' JOURNAL ENTRY does, and never the text.
- **Copy protection** (`copyprot.pas`, `fixmain.pas`) is not played, as with
  the other games.
- **The test harness:** the walk and route machinery in
  `tests/gold_box_games_tests.cpp` (`walk_census`, `walk_earned_campaign`), the
  skip when a game's files are missing, and the winners'-save fixture that
  carries a party from one game's route to the next (as Gateway's winners feed
  *Treasures*).

## What is new

The two games are science fiction on the *Curse* engine. They have no
spells, no clerics, no camp and no memorizing. Skills, medics, weapons with
ammunition and a rocketship take their place. Each system below is checked
against the source in step B2, and written up in `docs/games/<game>.md` in
our own words.

### Careers and races

- **Careers:** Rocketjock, Warrior, Engineer, Rogue and Medic (`types.pas`
  `TCareer`, `gdconsts.pas` `careerNames`). The source also has a Scout
  career, with its own experience table, that no race is offered: the four
  human races list six careers but count five (`gdconsts.pas`
  `raceCareers`). OpenGoldBox offers what the original offers.
- **Races offered:** Terran, Martian, Venusian, Mercurian, Tinker and Desert
  Runner (`chargen.pas` `CreateCharacterMenu`). Lunarian and Lowlander exist
  for NPCs and monsters only. Tinkers may be Engineers or Medics; Desert
  Runners Rocketjocks, Warriors or Engineers; the human races anything.
- **Career powers:** Warriors take more Hit Points (d10), punch twice a round
  for 1d3, and choose a weapon specialization at every even level, at most +3
  a weapon (`viewchar.pas` `SpecializationMenu`). Engineers roll d8, the rest
  d6, the better of two dice a level (`chargen.pas` `AddLevelHitPoints`).
  Desert Runners' claws add a point unarmed. Movement is 12, Tinkers 9 and
  Desert Runners 15.
- **Experience:** one table a career (`gdconsts.pas` `experienceNeeded`).
  *Countdown* stops at level 8 (`includes/c_career.pas` `MAX_LEVEL`).
  *Matrix Cubed*'s tables run to level 20 (`chargen.pas` `MAX_LEVEL`).
- **Combat value by level** (`gdconsts.pas` `careerBaseCombat`): about 40 at
  the start, rising to 44, or 48 for Warriors.
- **Saving throws** improve by one at every third level (`chargen.pas`
  `TrainingMenu`). Each race adjusts them (`gdconsts.pas` `raceSaveAdjust`).

Rules home: a new `buck_rogers_rules.cpp` in `src/OpenGold.Rules.Original`,
beside `champions_rules.cpp`. The `Race` and `Profession` enums gain the new
values. Existing values keep their meanings in the other games.

### Skills

- **84 skills** in seven groups: career, Dexterity, Constitution (empty),
  Intelligence, Wisdom, Charisma and technical (`gdconsts.pas`
  `skillDefinitions`, `includes/c_skill.pas`). Each career has eight career
  skills (`gdconsts.pas` `careerSkills`). Medical skills are for Medics only
  (`charhlp.pas` `MeetsSkillPrerequisite`). Some skills need others first,
  and dropping one refunds what depends on it (`skills.pas` `EditSkills`,
  `AdjustPrerequisites`).
- **A skill's value** is its level plus the score of its group's ability
  (`charhlp.pas` `SkillValue`).
- **The check** (`charhlp.pas` `SelectSkillCharacter`): the value is scaled
  by a difficulty (four times down to a quarter), and a d100 is taken from
  it. The party's best member may roll. It grades four results: bad failure,
  failure, success and great success.
- **Skills the engine uses itself:**
  - in ground combat (`cbtacts.pas`): Battle Tactics (+1 to hit for the side),
    Leadership (control of NPCs), Intimidate, Move Silently for backstabs,
    Use Jet Pack and Maneuver in Zero-G;
  - in healing (`viewchar.pas` `TreatPatients`, `postcbt.pas`
    `TreatInjuries`): First Aid, the Treat Wounds skills, Treat Poisoning and
    Treat Stun;
  - in ship repair after a battle (`postcbt.pas` `RepairShip`, with
    `gdconsts.pas` `repairSkills` and `repairDice`);
  - in ship combat (`shipcbt.pas`): Pilot Rocket, Jury Rig, Repair Nuclear
    Engine and Sensor Operation.
- **The scripts** call skill checks with SKILL CHECK (22h, 23h) and SKILL
  CHECK WITH DAMAGE (49h). Which skills they ask for is in the script data.
  The census lists them.

### Abilities

Seven abilities: Strength, Dexterity, Constitution, Intelligence, Wisdom,
Charisma and Tech (`types.pas` `TCharacter`). Each is the best of six 3d6
rolls, plus the race's modifier, and then raised to the career's minimum
(`chargen.pas` `CreateCharacterMenu`, `gdconsts.pas` `raceStatAdjust`,
`careerStatMinimum`). A low roll is raised, never refused. Strength sets melee
to-hit, damage and carrying capacity. Dexterity sets Armor Class, ranged
to-hit, initiative and ship gunnery. Constitution sets Hit Points. Every
ability except Constitution also adds to its group's skills.

The character record grows a seventh ability, which no other game has. It is
259 bytes (*Curse*'s is 422), and items are 62 bytes (`types.pas`).
`decode_character(bytes, game)` (`src/OpenGold.Formats/src/pool_radiance.cpp`)
gets a Buck Rogers layout in a new `buck_rogers_records.cpp`, as
`champions_records.cpp` is for Champions.

### Weapons, armor and damage

- **To hit** (`fxengine.pas` `AttackRoll`): a d20 plus the attacker's bonus
  must reach the defender's Armor Class. Armor Class starts at 50 and rises
  with armor and Dexterity. This is the ascending form of the manual's
  THAC0 and AC. A natural 1 misses and a natural 20 always hits. The rear
  Armor Class is 2 lower.
- **Modifiers:** long range adds 2 or 5 to the target number, a blocked line
  of fire 2 (`cbtacts.pas` `ApplyRangePenalty`). Zero gravity gives -2 to
  anyone who fails Maneuver in Zero-G, and quarters their movement.
- **Weapon classes** (`includes/c_wclass.pas`): melee, slug, rocket, laser,
  microwave, rocket launcher, grenade and plasma. The last three hit an area
  (`gdconsts.pas` `projectileKinds`).
- **What stops a hit** (`cbtacts.pas` `CalcWeaponDamage`): chaff stops
  rockets, ECM half the time and fielded battle armor three times in four.
  Aerosol mist stops lasers. Body armor stops microwaves. Sonic stunners stun
  instead of damaging. Goggles stop dazzle grenades; breathing masks and body
  armor stop gas (`includes/c_armor.pas`).
- **Clouds** of chaff, mist and gas linger four rounds, one in zero gravity
  (`types.pas` `TAreaEffect`).
- **Ammunition:** a ranged weapon carries its rounds; clips hold 10; rocket
  launchers and plasma throwers load one round at a time (`shop.pas`
  `ShopAmmoMenu`, `gdconsts.pas` `ammoPrices`). Grenades and antidotes come in
  bundles that HALVE and JOIN split and merge.
- **Backstab:** a Rogue directly behind its target, with a successful Move
  Silently check, multiplies damage by (level + 7) / 4; others double it.
- **New combat commands** (Rule Book, `cbtacts.pas`): INTIMIDATE, AID,
  SPRINT, DODGE and JETPACK (outdoors, with a jet pack and the skill).
- **Item tables:** `ITEMS` holds 39 rows of 9 bytes, of the 129 the game
  asks for; `ITEM0.DAX` holds 91 16-byte item templates (`gameinit.pas`,
  `eclmenu.pas` `LoadItemTemplates`).
- **Injury states:** bleeding, unconscious at 0, comatose and gone. A
  poisoned character goes comatose unless a Medic or an antidote treats it
  after the fight (`postcbt.pas` `TreatInjuries`). Comatose characters need a
  hospital or the ship's Medical Center.

Rules home: the combat session keeps its grid and turn order. The to-hit,
damage, save, area and cloud rules come from game-keyed functions, as
`canonical_effect(code, game)` and `ammunition_types` already are
(`src/OpenGold.Rules.Original/include/opengold/original_combat.h`). No spell
path runs for these games.

### Space travel and the solar map

- **Entering it:** a script writes 70h to the area kind at 4BE6h, or sets
  picture mode 2 with 4Ch (`eclmenu.pas` `WriteGbvm`). The interface becomes
  the solar map.
- **The map** (`orbitmap.pas` `PaintOrbitMap`; `gdconsts.pas` `orbitPositions`,
  `orbitLengths`, `orbitStartPhases`): a 22 by 22 grid around the Sun, five
  orbits of 2, 4, 8, 16 and 28 positions, and eight asteroid bodies. The
  rocket moves one of eight directions a step and cannot enter the Sun's
  square (`explore.pas` `MoveRocket`).
- **Time:** a step is three days, and heals three Hit Points (`gametime.pas`
  `RestAndHeal`). Every 32 ticks the planets move one phase of 224.
- **Arrival** is the script's work. It runs after each step and compares
  the rocket's square with the bodies' squares, which the map writes into
  memory from 4B85h (`types.pas` `TOrbitView`).
- **Fuel** is shown but not spent by the engine; the census shows whether
  the scripts spend it.
- **The menu** (`explore.pas` `MapMainMenu`): Move, Rocket (view the ship),
  Change and Save.

Home: a new solar-map session view in Core beside `overland.cpp`, and its
Godot screen beside `rolf_tour_view.cpp`'s overland map.

### Ship combat

- **Started** by SHIP COMBAT (1Fh) with the enemy ship, its pilot's skill and
  its to-hit number (`shipcbt.pas` `ShipCombat`).
- **Ships** are 108-byte records in `SHIPS.DAX` (`types.pas` `TShip`): six
  systems (hull, sensors, controls, life support, fuel and engines), tonnage,
  speed, maneuver and eight weapon mounts of eight kinds (`gdconsts.pas`
  `shipWeapons`). The party's ship lives in memory at 4D00h.
- **The fight:** it opens at range 8, and an opposed roll decides who goes
  first. The pilot can FIRE, TARGET a system, CLOSE, WITHDRAW, RAM and
  BOARD. The crew can FIRE lasers, LOAD, JURYRIG, COMMAND (take the helm),
  AID, BOOST ENGINES (twice a battle at most) and use SENSORS. Gunners use
  their own combat value and Dexterity. Hits fall on random systems by a
  table (`gdconsts.pas` `systemHitChances`). Damage slows a ship. A
  destroyed hull destroys it, and the party's crew dies with it. Ramming does
  each ship the other's tonnage in hull damage.
- **The end:** the outcome goes to 4BBAh. Boarding a disabled ship switches to
  a ground fight on the enemy's decks (post-combat mode 4D7Ch). Taking the
  bridge and engineering earns the ship's salvage. Then medics treat the
  wounded, engineers repair the ship, and the enemy's fuel is taken
  (`postcbt.pas` `PostCombat`, `RepairShip`).
- **Experience** comes from the enemy's type.

Home: a new ship-combat session in `src/OpenGold.Rules.Original`, keyed by
game. Its screen is new in Godot, and is designed with mockups of the real
screen first.

### The outpost, the port and money

- **One currency:** credits, with the shared pool (`money.pas`).
- **The bank** keeps the team's credits; in both games it can move them to
  the NEO account, never back (the outposts' scripts).
- **The NEO salvage account** pays for repairs, fuel, ship ammunition and
  medical supplies at the port. It is free at Salvation Station.
- **The hospital** heals for a fee; the ship's Medical Center uses medical
  supplies (byte 4C63h).
- **Shops** open with SHOP (4Bh) and a list number (`gdconsts.pas`
  `itemListStarts`). A price multiplier sits at 7F6Dh.
- **Libraries, bars and restaurants** are script menus. Their skill uses (Library
  Search) come from the scripts.

### Other game-specific systems

- **Digital personalities** such as Scot.DOS are NPCs in the scripts. The
  engine has no separate computer-ally system.
- **Leadership** takes a companion off computer control for a fight when its
  check succeeds, and only for companions with Intelligence above 3.
- **The difficulty levels** (`includes/c_diff.pas`): Novice, Cadet,
  Trooper (where a new game starts), Veteran and Ace. Each gives the party a
  bonus to hit, from +2 at Novice down to -2 at Ace (`fxengine.pas`
  `AttackRoll`), and scales a hostile creature's Hit Points as the script
  loads it, by (level + 2) / 4 (`eclvm.pas`). The Realms games' levels have no
  part for the party. The Rule Book also says harder levels give more
  experience. The engine has no such scaling, so B2 checks whether it comes
  only from the monsters' larger Hit Points. OpenGoldBox plays these levels
  under their own names, with a sixth, Admiral (see Difficulty, below).
- **New script commands** (`eclvm.pas` `DispatchEclCommand`, 00h to 4Ch,
  77 commands):
  - changed: 1Dh sets the speaker; 1Fh ship combat; 22h and 23h skill
    checks; 29h a continue prompt; 2Ch a yes/no compare; 31h a horizontal
    menu with a prompt; 3Fh a Log Book entry;
  - new: 42h to 4Ch (clear and stop, play sound, skip, distance ahead, a
    counted loop, swap item lists, skill check with damage, solo combat, shop
    and picture mode);
  - empty: 3Bh and 3Ch do nothing and do not move past themselves, so a
    script that reached them would never end. The census confirms that no
    script reaches them;
  - PROGRAM (38h): 0 opens the team menu with training, 9 the rest menu. In
    *Matrix Cubed*, 2 ends the game. *Matrix Cubed*'s PLAY SOUND starts an
    XMI piece when its value is above 127.

## Character creation

### *Countdown to Doomsday*

From `chargen.pas` `CreateCharacterMenu`:

- **Level 2,** with the career's level-2 experience. The character is
  trained free from level 0, so it gets two levels of Hit Points, skill
  points and, for a Warrior, a specialization.
- **1,000 credits.**
- **Gear:** two free items, item template 7 with 100 rounds and template 20,
  both readied (`shop.pas` `BuildAmmoItems`, `gdconsts.pas`
  `freeAmmoTemplates`). B2 names them from `ITEM0.DAX`.
- **Skill points:** 80 career and 40 general at creation, at most 30 in a
  skill. Each new level gives 40 and 20, at most 15 a skill. General skills
  stop at 80. Some career skills start at 10 (`skills.pas`
  `StartingSkillLevel`).
- **Age** 1d6 + 18. Armor Class 50.
- **Team:** up to six player characters and two NPCs (`includes/c_combat.pas`).

### *Matrix Cubed*

From its `chargen.pas` `CreateCharacterMenu`:

- **Level 7,** with the career's level-7 experience, trained free from level
  0. The Log Book says new characters start at eighth level. The program
  makes them level 7; B2 checks this in the running original.
- **1,000 credits** and the same two free items.
- **Skills are preset** by career (`gdconsts.pas` `careerInitialSkills`,
  `skills.pas` `ApplyInitialSkills`). The player spends no points at
  creation. Warriors choose three specializations.
- **Level limit 20.** Hit Points are rolled below level 10 and fixed above.

### *Matrix Cubed* brings in *Countdown* characters

Yes. The Add Character and Load Game menus ask "Matrix or Countdown"
(`chargen.pas` `AddCharacterMenu`, `saveload.pas` `LoadGame`). With Countdown,
the game reads *Countdown*'s save folder, named in `matrix.cfg`
(`gameboot.pas`). It loads each character's record (`.who`, 259 bytes) with
its items, second item list and effects (`.stf`, `.hid`, `.sfx`)
(`saveload.pas` `LoadCharlist`, `LoadChar`). A *Countdown* saved game brings
only its party; its world state is dropped, and the game starts at script 16.

Nothing is converted. Level, experience, credits, items, effects and skills
come over as they were, because the layouts are the same. A *Countdown*
character stops at level 8 and can train on to 20.

OpenGoldBox brings the party in from an OpenGoldBox save of *Countdown*'s end,
as *Treasures* brings in *Gateway*'s winners. The original also deletes
same-named files in its own save folder after an import; OpenGoldBox does
not.

## How the work stays safe

The Realms and Krynn rules hold:

- **Read before playing.** A game is first decoded, read-only, with a
  measurable result.
- **Every test of the other games keeps passing.** *Pool of Radiance* stays the
  default game.
- **Code becomes shared only when a second game needs it.** The Buck Rogers
  rules live in the original-rules module, keyed by game. *Matrix Cubed*
  shares *Countdown*'s tables except where its source differs.
- **One code path.** The tests, the walks and the routes run the real game's
  code.
- **Every step ends in a check that reads the real files.** It skips when a
  game's folder is missing.
- **Stable numbers.** New games are appended to `GoldBoxGame`, after whatever
  is last when B0 lands (*Death Knights* and *The Dark Queen* may come
  first). The number and any campaign format raise are claimed on the session
  board.

## Plan

The steps follow release order. *Matrix Cubed* runs the same program, so
most of its work is its route. B1 can run for both games at once. B3 needs
B2; B4 needs B3's winning party.

### Who does which step

Two kinds of session share the work, as the user decided on 2026-11-20.
Each builds on the other's work on main and never builds a second copy of
it (one code path).

- **The macOS Claude session** builds what the player sees and does, for
  both games:
  - the creation and skill screens;
  - the solar map, its rules (`buck_rogers_solar_map.h`) and its screen;
  - the ship combat screen and the code that drives a ship fight
    (`buck_rogers_ship_fight.h`);
  - quick save and quick load, for every game;
  - Novice reaching the game, the antidote asked about after a fight, and
    ground that blocks movement but not shots;
  - B5: the Options rows, the difficulty names and the rest of the menus.
- **A Codex session** plays *Countdown* to its end: B3's route test. It
  flies the solar map and fights the ship fights through the code above,
  and fixes what its route finds missing in the script host.
- **A Codex session** puts *Matrix Cubed* in the engine: B4's campaign,
  its scripts, its save (campaign format 55), its ending, its walk and its
  route. It uses the shared script host and the screens above.

Every session changes `buck_rogers_host.cpp`, so each rebases on main before
it pushes and keeps its changes there small. The session board shows who
holds what: `tools/session_board.py list`.

### B0. Groundwork

Scope:

- `GoldBoxGame` gets `countdown_to_doomsday` and `matrix_cubed`, appended
  (`src/OpenGold.Formats/include/opengold/gold_box_game.h`), with
  `game_title`, `game_folder` and `game_disks`.
- `game_folder` returns the names `buck-rogers-countdown-to-doomsday` and
  `buck-rogers-matrix-cubed`, which the settings' keys and the save folders
  use. The games' folders themselves are found by their files wherever they
  are and whatever they are called (TECH.md, "Finding the games' folders"),
  so the copies on each machine need no particular names.
- `identify_game` (`src/OpenGold.Core/src/game_census.cpp`) knows each game
  by its files: `SHIPS.DAX` marks Buck Rogers; `BUCKA.XMI` with it marks
  *Matrix Cubed*, and `ECL6.DAX` *Countdown*. Neither folder has a file that
  an earlier marker matches; the test checks that.
- `application_settings.cpp` holds a folder for each. The startup menu
  (`startup_view.cpp` `playable_games`) lists both, disabled until each
  plays. The menu's layout for twelve games follows KRYNN.md's decision
  (mockups of the real screen).
- A save records the new games, and every old save loads as its own game.
- GAMES.md's "Later" section points here, and each game gets its
  `docs/games/<game>.md`.

Verified by: `each_game_is_told_apart_by_its_files` and `a_save_names_its_game`
in `tests/gold_box_games_tests.cpp`, `tests/startup_tests.gd`, and the full
suite.

Risks: the game number collides with a Krynn game landing at the same time
(claim it first); a marker that also matches another game's folder.

**Status:** done 2026-11-19. *The Dark Queen* had claimed number 9 on the
session board, so *Countdown* is 10 and *Matrix Cubed* 11, and a save's
number finds its game by value (`game_numbered`), not by place. The menu
lists both, disabled, in the existing picker. `ECL6.DAX` counts only beside
`SHIPS.DAX`: *Pool*, *Curse* and *Gateway* have one too. The Linux machine's
copies were not checked from macOS: the docs record only that its games are
under `/home/edmond/GoldBox/<folder>`, with no listing of the Buck Rogers
folders.

### B1. A reading census for each game

Scope: `opengold_census` decodes every file of each game:

- the DAX archives, pictures, walls, backdrops, sprites, fonts, the cursor and
  borders;
- `MON0CHA`, `MON0ITM` and `MON0SPC` (*Countdown*'s 263-byte stride, of
  which 259 bytes are read);
- `ITEMS` (9-byte rows) and `ITEM0.DAX` (16-byte templates);
- `SHIPS.DAX` (108-byte ship records, and its pictures);
- the combat tile sets `HITEK`, `LOTEK`, `MARSCOM` and `VENUSCOM`;
- `TITLE.DAX`, `MUSIC.DAX`, and *Matrix Cubed*'s XMI files, `FATV9` timbres
  and AIL drivers (recognized, never run);
- every script, with the new opcode table (`game_opcodes`) and the 8000h to
  A400h window.

The census also lists, for the game docs:

- the skills each script checks, and at which difficulty;
- where scripts start ship combat, the solar map and shops;
- which scripts spend fuel;
- *Countdown*'s winning event: the script and event that follow the escape
  from the Mariposa, which the cluebook's finale describes;
- that no reachable command is 3Bh or 3Ch.

Verified by: `opengold_game_census` (each game joins
`tests/game_census.cmake`'s list when complete), and record checks in new
`tests/countdown_tests.cpp` and `tests/matrix_cubed_tests.cpp` that read the
real files, as `champions_tests.cpp` does.

Risks: picture formats that differ from the Realms VGA ones; the script
decoder walking into data after a jump table; the AIL timbre library being
a new format (UA's `INSTR.AD` is the nearest reader).

**Status:** done 2026-11-19. Both games' censuses are complete and both
games are in `tests/game_census.cmake`'s list. Every picture, sprite, icon,
tile set, wall and backdrop is the 256-color layout of *Pools of Darkness*;
walls draw from their own disk's `8X8D` file. The records have their own
readers (`buck_rogers_records.h`: creatures, items and templates, item types,
ships), the scripts their own table and the four-byte operand tags 4 and 5
(`game_opcodes`, `ecl_memory` 8000h to A400h), *Matrix Cubed*'s music files
theirs (`ail_files.h`: drivers recognized, the timbre library, the bank and
catalog it was made from). `opengold_census --scripts` lists what the
scripts ask of the engine (`buck_rogers_scripts.h`), and
[countdown.md](games/countdown.md) and
[matrix-cubed.md](games/matrix-cubed.md) record it: the skill checks with
their difficulties, the ship combats, the solar map, the shops, the fuel
(only the solar-map script spends it), the PROGRAM commands, *Countdown*'s
winning event (script 98's escape pod, which sends the team home to script
17) and *Matrix Cubed*'s (script 114's PROGRAM 2). No reachable command is
3Bh or 3Ch, and no IF precedes a command the original skips unevenly.
Findings for later steps: `LOTEK`, `MARSCOM` and `VENUSCOM` hold fewer tiles
than the game copies (B3); the hail of a ship checks unlikely skills (open
question 2); *Matrix Cubed* keeps two developer scripts and two files that
are not the game's.

### B2. The Buck Rogers rules (original rules)

Scope, in `src/OpenGold.Rules.Original`, keyed by game:

- races, careers, ability rolls and minimums, Hit Points, experience tables
  and level limits (8 and 20);
- the 84 skills, their prerequisites, points at creation and at each level,
  *Matrix Cubed*'s preset skills, and the skill check;
- creation as each game has it, Modify Character, free training and
  Warriors' specializations;
- the seven-ability character record and its 62-byte items;
- ground combat: the ascending to-hit roll, range and cover, zero gravity,
  weapon classes, ammunition and reloading, area weapons and clouds, what
  stops a hit, stun, backstab, and the new commands (INTIMIDATE, AID, SPRINT,
  DODGE, JETPACK); Battle Tactics and Leadership;
- saving throws by category, race and level;
- injury: bleeding, unconscious, comatose and gone; medics in and after a
  fight; poison and antidotes; the hospital and the Medical Center;
- natural healing over time (`gametime.pas` `RestAndHeal`) and the rest menu;
- ship combat as a session of its own, with salvage, repair, fuel and the
  boarding fight;
- credits, the bank, the NEO salvage account, shops, ammunition and the port;
- the difficulty levels as Difficulty (below) sets them: the party's to-hit,
  the monsters' Hit Points, and the table's monster to-hit and Armor Class.

Each rule gets a test that names the source routine it follows, as in
[ORIGINAL-RULES.md](ORIGINAL-RULES.md). Obvious bugs in the original are
fixed, listed under Bugs, and reported to the user one by one. Known
candidates:

- the skill check grades its result by the last member's value, not the
  roller's (`charhlp.pas` `SelectSkillCharacter`);
- *Matrix Cubed* reads one past its experience table at level 20;
- *Matrix Cubed*'s import deletes files in its own save folder.

Verified by: synthetic checks in `countdown_tests.cpp` for creation (every
race and career pair the menus offer), skills and checks, the to-hit and
damage rules, area weapons, ammunition and injuries. Ship-combat checks
cover range, ramming, boosting, jury-rigging, system hits, boarding and
salvage. The original-rules combat suites keep passing for every other game.
A party created under these rules is one the original would create.

Risks: this is the largest step. The combat session is 11,830 lines built
around the AD&D games, and its spell and class assumptions must stay out of
the Buck Rogers path without a second combat engine (one code path). The
seventh ability touches the character model shared by every game. Ship
combat is a whole new mode with no counterpart in the engine.

**Status:** the characters are done (2026-11-20): races and careers (the
`Race` and `Profession` enums gain them after the existing values), the
seven abilities, Hit Points, experience to levels 8 and 20, combat value,
saving throws by race and level, the 84 skills with their prerequisites,
points, refunds and *Matrix Cubed*'s presets, the skill check, creation,
training, specializations, Modify, and the 259-byte record with its 62-byte
items, decoded and encoded byte for byte (`buck_rogers_rules.h`,
`buck_rogers_records.h`; [countdown.md](games/countdown.md) and
[matrix-cubed.md](games/matrix-cubed.md), "Rules" and "Bugs"). The seventh
ability lives in the game's own record, so the character model of the
other games is unchanged. The hail's corrected skills are a rules function
the script host will read SKILL CHECK through in B3. Ground combat and the
difficulty levels are done too (2026-11-20): the ascending attack roll,
range, cover and zero gravity, the weapon classes, ammunition and
reloading, area weapons and clouds, what stops a hit, stuns, backstabs,
INTIMIDATE, AID, SPRINT, DODGE and JETPACK, Battle Tactics, Leadership,
saving throws in a fight, the injury states, and the six levels with the
setting's `novice` (`buck_rogers_combat.h`; [countdown.md](games/countdown.md),
"Ground combat"). The original-rules combat session runs these fights by
them, one code path: a creature fights as its own record, and no spell,
class or AD&D effect runs. Five more original bugs are fixed and listed.
So are ship combat, as a session of its own, and upkeep and money:
the wounded after a fight, healing, rest, credits, shops and the outposts
([countdown.md](games/countdown.md), "Ship combat" and "Upkeep and money").
*Matrix Cubed*'s import (and its deleting of files) comes with B4.

### B3. *Countdown to Doomsday*

1. **The engine plays the game:**
   - its 3D views, walls and backdrops;
   - the solar map and travel between outposts;
   - its script commands, including skill checks, Log Book references and
     picture modes;
   - its ground fights on their tile sets, in normal and zero gravity;
   - ship combat and boarding;
   - outposts: the bank, shops, ammunition, the hospital, the port and
     training;
   - its music.
2. **A walk test** (`opengold_countdown_walk`) visits every map, flies the
   solar map, and starts every ground and ship fight its scripts can load.
3. **A route test** (`opengold_countdown_earned_ending`) plays from a fresh
   party made by *Countdown*'s creation rules, from Salvation Station to the
   escape from the Mariposa. It uses the same planning aids, per-beat dice
   and reloads where a player would reload as the earlier routes. Skill
   checks are rolled as the original rolls them; a failed check is played on,
   or reloaded where a player would. The party's skill points are spent for
   the checks the cluebook's areas need. It writes
   `countdown-earned-winners.sav` as a fixture.
4. The ending is shown, with room for a drop-in ending film
   (`endings/<folder>.ogv`, made by the user).

Verified by: the walk, the route setting `game_won` at the winning event B1
found, and a byte-identical round trip of the winning save. Certification
requires actual wins, earned rewards and physically valid movement. Forced
outcomes, grants, healing or placements remain diagnostic and cannot certify
a win.

Risks: random skill checks gate the story and can fail; ship fights can be
lost outright with the crew; fuel, salvage money and medical supplies can run
short; a comatose character needs a hospital that may be far away.

**Status:** the first part of 1 and the walk are done (2026-11-20): a team
made by the creation rules starts the campaign and saves; the views, walls,
backdrops, pictures and text; the script machine with the Buck Rogers
commands and the host; ground fights on the four tile sets, in normal and
zero gravity; the outposts as their scripts run them; the title's music; and
`opengold_countdown_walk` over every map and every script without one
([countdown.md](games/countdown.md), "The game in the engine"). Ship combat
plays on its own screen through the rules (2026-11-20;
[countdown.md](games/countdown.md), "The ship combat screen"), and the walk
plays real ship fights. The solar map and the creation and skill screens
are designed from mockups; the session stops at a hand-off for each.

### B4. *Matrix Cubed*

The same four parts, and:

- the single-disk layout (every script and map from `ECL1` and `GEO1`);
- the level 20 tables and the preset creation skills;
- bringing in a *Countdown* party from `countdown-earned-winners.sav`
  (`OPENGOLD_COUNTDOWN_WINNERS`), without conversion;
- the bank's transfer to the NEO account;
- XMI music through the existing XMI reader and the AIL timbres;
- PROGRAM 2 as the end of the game.

Verified by: `opengold_matrix_cubed_walk`, and
`opengold_matrix_cubed_earned_ending`, which starts from the imported
*Countdown* party and sets `game_won` when the Matrix crucible is ignited
(the cluebook's Jupiter finale). A shorter check starts a fresh level-7
party and plays its opening.

Risks: the route is half again as long as *Countdown*'s; the story branches
(the Living Ship's pirates and Stormriders) must be played as a player would
choose; the route waits on B3's winners.

### B5. Each game's settings and menu

These carry over from the other games, per game:

- the startup menu buttons, enabled once each game wins;
- the difficulty levels under the original's names, with Admiral (see
  Difficulty, below), on the Options screen while a Buck Rogers game plays;
- a default party for the original rules only (no SRD rules for these
  games), made by the game's own creation rules
  (`data/default_parties/<folder>.json`,
  [DEFAULT-PARTIES.md](DEFAULT-PARTIES.md)): level 2 in *Countdown*, level 7
  in *Matrix Cubed*;
- the options for any of the original's behaviours the user makes optional;
- the Spanish text for every new screen and message;
- the ending films.

Verified by: `opengold_default_party_tests`, `tests/startup_tests.gd`, the
localization check and the Godot creation tests through the real screens.

Risks: the ship-combat and solar-map screens are new UI, and need the user's
review of mockups.

## Cost

Champions took one commit: 69 files, about 10,300 lines added. About 3,300
were C++ in `src` (2,100 in the original-rules module, 570 in Core, 440 in
Formats). About 4,000 were tests and 2,700 translation catalogs. One owner
took about nine hours from start to an earned ending. It reused the AD&D
classes, spells, camp and combat almost whole.

The Buck Rogers games reuse the engine but not the AD&D rules. A rough guess:

- ***Countdown*:** two to three times Champions. The rules (careers, 84
  skills, the seven-ability record, the weapon and injury model) are about
  twice Champions' rules. Ship combat and the solar map are new modes with
  new screens, about as large again. The route is about Champions' length.
- ***Matrix Cubed*:** a third to a half of Champions. The program is the same;
  the work is its longer route, the import, XMI music and the level 20
  tables.

The two games together come to roughly three Champions.

## Risks

- **Ship combat** has no counterpart in the engine. It is the biggest single
  piece, and its losses kill the whole crew.
- **The rules break with AD&D.** Skills replace spells and classes, and the
  shared combat session must take them without a second code path.
- **Random skill checks** gate story events. The route must play them as a
  player does.
- **The manuals and the source disagree** in places (*Matrix Cubed*'s
  starting level, the hidden Scout career). The source decides, and B2 checks
  doubtful cases in the original under DOSBox-X.
- ***Matrix Cubed*'s route** depends on *Countdown*'s winners.

## Difficulty

Decided by the user on 2026-11-20. The Buck Rogers games play the original's
five difficulty levels under their own names, and a sixth, Admiral, past
Ace. Each stands where one of OpenGoldBox's levels stands. The Options screen
shows these names while a Buck Rogers game plays:

| Level | OpenGoldBox's level | Party's to-hit | Monsters' Hit Points | Monsters' to-hit | Monsters' Armor Class |
|---|---|---|---|---|---|
| Novice | below Easy | +2 | 50% | -2 | 2 worse |
| Cadet | Easy | +1 | 75% | -2 | 2 worse |
| **Trooper** | **Normal** | — | — | — | — |
| Veteran | Hard | -1 | 125% | +1 | 1 better |
| Ace | Nightmare | -2 | 150% | +2 | 2 better |
| Admiral | Infernal | -3 | 200% | +3 | 3 better |

- **Trooper is the original exactly,** as Normal is in every game.
- **The party's to-hit and the monsters' Hit Points** are the original's
  (`fxengine.pas` `AttackRoll`; `eclvm.pas`, (level + 2) / 4, at least 1).
  Admiral goes one step past Ace for the party's to-hit. For Hit Points it
  takes OpenGoldBox's Infernal, twice them, as *Secret*'s levels do.
- **The monsters' to-hit and Armor Class** come from OpenGoldBox's difficulty
  table ([ORIGINAL-RULES.md](ORIGINAL-RULES.md), Difficulty), as in the other
  games whose original already scales Hit Points. Novice takes Easy's.
- **Novice** is a new value of the `difficulty` setting, `novice`, offered
  only while a Buck Rogers game plays. Any other game plays it as Easy.
- **The scripts' difficulty** (cell 4BB6h, which some scripts multiply by)
  holds the original's 0 for Novice to 4 for Ace. Admiral holds 5, one step
  past Ace (the user, 2026-11-20).

## Open questions

The sources settle everything else in this plan. Questions found in later
steps go to the user, numbered, one topic each.

None are open.

Decided:

1. **The party's to-hit at other difficulty levels** (the user, 2026-11-20):
   it changes as the original's does; see Difficulty.
2. **The skills a hail checks** (found in B1; the user, 2026-11-20: fix it
   as a bug). When the team hails a ship, the menu offers BLUFF, INTIMIDATE
   and ASK FOR HELP, and both games check skills 74 (Befriend Animal), 70
   (Hypnosis) and 69 (Paint/Draw) (*Countdown* `ECL2` script 34, *Matrix
   Cubed* `ECL1` script 19). No team member can learn Hypnosis or Paint/Draw
   (`gdconsts.pas` `skillDefinitions`), so two of the three choices can never
   succeed. Every other check names the skill its text asks for. Two numbers
   higher are the skills the choices name: 76 (Fast Talk/Convince), 72
   (Intimidate) and 71 (Act). OpenGoldBox checks those, and lists the change
   under Bugs.
3. **General skills for a character brought in from *Countdown*** (found in
   B2; the user, 2026-11-20: keep the original). In *Matrix Cubed* a
   character at level 7 may know twelve general skills, so one brought in
   below level 7 learns several in the one level it trains to 7, and so
   matches a new character. Above level 7 it learns one a level, as every
   character does.
4. **Ship combat** (found in B2; the user, 2026-11-20, for both games;
   [games/countdown.md](games/countdown.md), "Ship combat", Decisions).
   Three of the original's ways are options, set per game on the Options
   screen while a Buck Rogers game plays, each defaulting to the Rule Book
   or to OpenGoldBox's choice ([CONFIGURATION.md](CONFIGURATION.md)):
   COMMAND only while the pilot is down (COMMAND every round on plays the
   original's once a round), an empty helm dodging with 0 (Empty helm
   dodges on plays the ship's own pilot value, 100 for the Maelstrom Rider),
   and a boarded ship's fuel transferred (Fuel from a boarded ship off plays
   the original's none). A fight no one can end, the team helpless and the
   enemy unable to finish the ship or leave, ends as if the enemy broke off
   (a bug, fixed). The enemy moving first each round after the team wins the
   pilot roll, and its boosting without limit, stay as in the original.
5. **A Rogue from behind** (`cbtacts.pas` `RollAttacks`; found in B2; the
   user, 2026-11-20: fix it as a bug). When a Rogue strikes from behind, the
   original makes the number it must reach 2 higher, so a Rogue hits from
   behind less often than anyone else. The earlier games give a thief 4
   better from behind (Curse's `RollAttacks`), and these games count Armor
   Class upward, so the 2 is that bonus with its sign left unturned. The
   manuals give no figure. OpenGoldBox makes the number 2 lower, and lists
   the change under Bugs.
6. **How far a stray shot may land** (`CombatLaunchProjectile`; found in B2;
   the user, 2026-11-20: keep the original). Of the four tries at a stray
   landing, the first must lie within about four squares of the shooter,
   and each later try may reach twice as far as the last try walked, so a
   stray long throw usually comes down on its target. The original reuses
   the walked distance as the next try's reach; OpenGoldBox does the same.
7. **Upkeep** (found in B2; the user, 2026-11-20: keep the original, both
   games; [games/countdown.md](games/countdown.md), "Upkeep and money"). A
   Medic treating its own wounds finds only Treat Critical Wounds harder.
   Salvation's doctor heals even members who are gone. Credits left in the
   pool are lost after one warning. Topping up ammunition charges a whole
   clip.
