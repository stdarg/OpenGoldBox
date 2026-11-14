# The Gold Box games

OpenGoldBox's goal is to play every SSI Gold Box game. *Pool of Radiance* came
first. The next step is the rest of the *Forgotten Realms* series. The *Krynn*
games and *Buck Rogers* games follow later with the same method.

This page records the decisions, what works today and the plan. It grows as
each game is built.

## Decisions

The user decided these on 2026-11-12:

- **All Gold Box games are the goal.** This replaces the PRD's "avoid a
  premature multi-game engine". The PRD's method still holds: code becomes
  shared only when a second game proves the need.
- **The new games play by the original rules** ([ORIGINAL-RULES.md](ORIGINAL-RULES.md)).
  The SRD 5.2.1 rules come to them only after every game is playable, as
  content work (monster conversions, shops, art mappings) for each game.
- **The games come in release order.** Each one builds on the one before.
- **Unlimited Adventures:** OpenGoldBox plays adventures made with it and
  offers an updated construction kit.

## The games

| Order | Game | Released | Folder | Status |
|---|---|---|---|---|
| 1 | *Pool of Radiance* | 1988 | `POOLRAD` | Playable |
| 2 | *Curse of the Azure Bonds* | 1989 | `CURSE` | Played to its end by the route test, with stand-ins ([curse.md](games/curse.md)) |
| 3 | *Secret of the Silver Blades* | 1990 | `SECRET` | Every file decodes ([secret.md](games/secret.md)) |
| 4 | *Gateway to the Savage Frontier* | 1991 | `GATEWAY` | Every file decodes ([gateway.md](games/gateway.md)) |
| 5 | *Pools of Darkness* | 1991 | `DARKNESS` | Its opening plays and its walk passes ([darkness.md](games/darkness.md)) |
| 6 | *Treasures of the Savage Frontier* | 1992 | `TREASURE` | Its opening plays and the walk passes every map and fight ([treasure.md](games/treasure.md)) |
| 7 | *Forgotten Realms: Unlimited Adventures* | 1993 | `UA` | A design made with the kit plays: its walls, events and fights ([ua.md](games/ua.md)) |

The developer's copies live in `/Users/edmond/GoldBox/<folder>`.

The original let a party travel on: *Curse* imports a *Pool of Radiance*
party, *Secret* a *Curse* party and *Pools of Darkness* a *Secret* party.
*Treasures* imports a *Gateway* party. OpenGoldBox follows the original.

Each game also shipped a pre-rolled party, for beginning to play at once.
OpenGoldBox's startup menu offers Play with Default Party instead with its own
parties, one per game under the original rules and under the SRD 5.2.1 rules
(`data/default_parties`, listed in [DEFAULT-PARTIES.md](DEFAULT-PARTIES.md)),
made by the game's own creation rules; it never reads the player's `SAVE`
folder, whose saved games may have been played on.

## What the games share

The reconstructed DOS sources of each game (kept outside this repository) show
one engine family. Games 1 to 6 are Turbo Pascal programs with the same units:
the script machine, combat, magic, character creation, saving and the rest.
Each game grows the one before, from about 33,000 lines in *Pool of Radiance*
to about 42,000 in *Treasures*. *Unlimited Adventures* was rewritten in C; it
is a construction kit, and its data comes in `DISK1` to `DISK3`.

Those sources carry no license, so OpenGoldBox learns from them and writes its
own code and text, as with the *Pool of Radiance* source
([ORIGINAL-RULES.md](ORIGINAL-RULES.md)). They also describe nearby versions,
not always the exact copies: the developer's `GAME.OVR` of *Pool of Radiance*,
*Curse* and *Pools of Darkness*, *Secret*'s `START.EXE` and `GAME.OVR`, and
*Gateway*'s `GAME.EXE` differ from the files the sources rebuild, probably
because the store releases patched them. OpenGoldBox reads only the data files,
so this matters only where a behavior is in doubt.

### What our readers decode today

The census (2026-11-12) puts every file through the decoders:

| Game | Files that decode | What does not decode yet |
|---|---|---|
| *Pool of Radiance* | All | — |
| *Curse* | All | — |
| *Secret* | All (2026-11-13) | — |
| *Gateway* | All (2026-11-13) | — |
| *Pools of Darkness* | All (2026-11-13) | — |
| *Treasures* | All (2026-11-13) | — |
| *Unlimited Adventures* | All (2026-11-13): its libraries of glyphs and records, the font, items, sounds, music and art templates in `DISK1` to `DISK3` and `ART` | — |

The archives (DAX) and maps are shared by all six campaigns. Scripts,
creatures, items and pictures change from game to game.

## How the work stays safe

- **One game at a time, reading before playing.** Each game starts with
  read-only work whose result can be measured.
- **The *Pool of Radiance* tests guard what exists.** Every shared change keeps
  them passing, and *Pool of Radiance* stays the default game.
- **Code becomes shared when a second game needs it**, not before.
- **Every step ends with a check that runs the real game.** Tests read the
  original files from the game's folder and skip when it is missing, as the
  *Pool of Radiance* tests do.
- **Each game's differences are written down** in `docs/games/<game>.md`, in
  our own words.

## Plan

### 0. Groundwork

- The PRD's goal says all Gold Box games, and this page tracks them.
- The game recognizes each of the seven folders by its files.
- The settings hold one folder for each game.
- A save records which game it belongs to (a campaign format change).

Done when every test passes and an old save still loads as *Pool of Radiance*.

### 1. A reading census for every game

A census tool decodes every file of every game: pictures, walls, sprites,
fonts, monsters, items, scripts, sounds and, in the later games, music. It runs
as a test, with one row for each game and kind of file. The facts it needs
(record sizes, where scripts load, file names, new script commands) come from
the reconstructed sources and go into `docs/games/<game>.md`.

Done for a game when its census decodes every file.

**Status:** done for *Pool of Radiance* and *Curse* (2026-11-12), and for
*Secret*, *Gateway*, *Pools of Darkness*, *Treasures* and *Unlimited
Adventures* (2026-11-13): every game's census is complete. Run it with
`opengold_census FOLDER...` (in `build/macos-universal` after `./build.sh`,
or `build` in a native build), where a folder is one game's or one holding
several (`/Users/edmond/GoldBox`). It prints a row for each kind of file: how
many pieces decoded and the first that did not. The test
`opengold_game_census` requires every finished game to decode completely. It
reads the games from `OPENGOLD_GOLDBOX_DIR` (the folder holding `POOLRAD`,
`CURSE` and the rest) and from `OPENGOLD_GAME_DIR`, and skips when neither is
set. A game joins its list in `tests/game_census.cmake` when its census
completes.

Two notes on what "decodes" means:

- A script decodes when every command reachable from its entries decodes. A
  path the walker reaches only by running past the end of an ON GOTO's list is
  not counted: the scripts keep the choice in range, and data may follow.
- Decoding is not playing. What *Curse*'s new script commands do, and which of
  its creatures' fields the game uses how, belong to step 3.

### 2. The engine apart from *Pool of Radiance*

What the code knows about *Pool of Radiance* moves into per-game data or
modules: the Phlan session, Rolf's tour, teleports, script secrets, the script
host's special cases, shops, companions and encounter tables.

Done when every test passes with no visible change.

A survey (2026-11-12) found *Pool of Radiance* knowledge in about 30 files, of
two kinds. **Engine code with *Pool of Radiance*'s values built in:** the script
machine's memory window and commands, about 40 fixed script-memory addresses,
the 285-byte character record read by its byte offsets, and file names with
disk numbers. **The campaign itself:** the Phlan session, Rolf's tour, the
wilderness, teleports, secret doors and traps, NPC portraits and place names.
The first kind becomes per game; the second stays *Pool of Radiance*'s, behind
one seam, and *Curse* gets its own.

The steps, each pushed on its own with every test passing:

1. **The script machine runs a game's scripts:** its memory window and commands
   come from the game, not from *Pool of Radiance*'s constants. *Done
   2026-11-12.*
2. **The script memory map per game:** the fixed addresses (party position,
   clock, current character, combat and rest cells) become a table for each
   game; *Curse*'s comes from its reconstructed source. *Done 2026-11-12:*
   *Curse* moved *Pool of Radiance*'s zones whole, so one translation
   (`game_address`) serves every cell but the current character's.
3. **One creature model:** the code stops reading the 285-byte record by its
   offsets and reads named fields, which *Curse*'s 422-byte record fills too. *Done
   2026-11-12:* `decode_character(bytes, game)` fills the same fields from
   either layout; the size, morale and side bytes have names.
4. **Files by game:** disks and banks are counted from the folder (*Pool of
   Radiance* has eight, *Curse* six), and the names, sounds and wilderness in
   `START.EXE` are read per game. *Done 2026-11-12:* `game_disks` lists each game's
   disks; the creature, magic item and portrait catalogs and the sound bank
   take the game. The wilderness in `START.EXE` is *Pool of Radiance*'s own
   content and moves with step 6.
5. **A save names its game** (phase 0's save change; a campaign format change).
   An old save loads as *Pool of Radiance*. *Done 2026-11-12:* campaign format 48.
6. **The campaign behind one seam:** the game reaches *Pool of Radiance*'s own
   content (the Phlan session, the wilderness, teleports, secrets, portraits,
   place names) through one interface chosen by the game, so *Curse*'s can sit
   beside it. *Moved into phase 3, step 1 (2026-11-12):* the game
   accepts only a *Pool of Radiance* 1.3 folder today (the settings check its
   files' checksums), so no other game reaches the session yet. The seam is
   built with *Curse*'s session, its second user, rather than designed ahead
   of it.

### 3. *Curse of the Azure Bonds*

In small steps, each with its check:

1. Choosing *Curse* at New Game shows its title and a first map. In the
   engine first, then the screens:
   1. The engine routines a script calls (redraw the view, play a sound) by
      their address in each game: *Curse*'s redraw is CALL 2E10.
   2. The loading of an area's map, walls and pictures shared by both games.
   3. The opening's script cells in the game's memory (`game_address`).
   4. *Curse*'s opening: script 1 of disk 2 runs its load entry only; the 3D
      view on and no portrait to start.
   5. `load_campaign(folder, game)`, the one way the game starts a campaign.
   6. A test walks *Curse*'s first map: the awakening text, picture 10, the
      party at (7,13) facing east, the view drawn.

   Steps 1 to 6 done 2026-11-12: `load_campaign` starts *Curse*'s campaign,
   and `opengold_gold_box_games_tests` plays its opening on the real files.
   7. A folder for each game in the settings; New Game asks for the game;
      the title pictures; the exploration screen, area names and saves by
      game. *Started 2026-11-12, to the user's design:* startup shows a
      title picture (a placeholder), the language dialog, then a menu with
      Options, a button for each game and Exit to desktop. Choosing *Curse*
      asks for its folder, and the exploration screen plays *Curse*'s
      campaign, its first map named Tilverton (`opengold_godot_startup_menu`
      checks it on screen). Since 2026-11-14 every area of *Curse* has its
      name ([curse.md](games/curse.md), Area names). *Title pictures and saves by game done
      2026-11-14:* choosing a game shows its own title pictures from its
      TITLE.DAX, as its original's Intro shows them before its menu
      (`load_title_screens`): *Curse*'s picture 1 for 5, its name (3) over picture 2
      for 10, and picture 4 with its sound for 10; *Secret*'s 1, 2, 3 over 2
      and 4 over 2 again for 15, 50, 50 and 60. A key or click passes a
      screen, in *Secret* the whole title. *Startup's menus done 2026-11-14, to
      the user's design:* OpenGoldBox's title for 2 seconds, then a main menu
      (Continue, which loads the newest save of any game, Pick game, Options,
      Exit to desktop). Picking a game shows its title (*Pool of Radiance*
      keeps its new title, 5 seconds; the others their own pictures, without
      the credits screens), then the game's menu: Continue (its newest save),
      Load game, Options, Play with default party (the game's default
      party, [DEFAULT-PARTIES.md](DEFAULT-PARTIES.md)), Create new party (the new game's question,
      then the creation screens), Main menu and Exit to desktop. On a title
      any key or click passes it; Escape goes back, from a title or the
      game's menu to the games, from the games to the main menu
      (`opengold_godot_startup`, `opengold_godot_startup_menu`). Each game
      keeps its saves in its own folder (*Pool of Radiance*'s where they always
      were, the others' in `saves/CURSE` and so on), its save and load screens
      list only its own, a save records its game's files, and loading a save
      of another game starts that game (`opengold_godot_saves_by_game`).
2. Walking the maps, with walls, doors and events. *In progress
   (2026-11-12):* every script and map of *Curse* loads with its disk, and
   LOAD PIECES loads the view's walls as the original does. Tilverton's
   streets walk from the inn: the innkeeper, the shops' greetings, the
   training hall, the sage and the street talk, and the current character's
   cells. Events that start a fight wait for step 5.
3. The scripts' text and menus, and the commands *Curse* adds. The script
   coverage ([ECL-COVERAGE.md](ECL-COVERAGE.md)) is kept for each game.
4. Making a party, and bringing a *Pool of Radiance* party across. *In
   progress (2026-11-13):* a party made for *Curse* under the original rules
   follows *Curse*'s creation and tables (ORIGINAL-RULES.md, *Curse of the
   Azure Bonds*): paladins and rangers, 25,000 experience trained up at once,
   300 platinum. *Curse*'s NPCs join as their 422-byte records. A *Pool of
   Radiance* character comes across as *Curse*'s `ConvertPoolRadPlayer`
   brings it (`CampaignParty::bring_across`): its scores within *Curse*'s
   limits, its levels, Hit Points, experience and drained levels, its
   magic-user spells but Animate Dead, its items as they were readied, and
   300 platinum; its money stays behind. The party screen's Add from Pool of
   Radiance (*Curse* under the original rules) lists the *Pool of Radiance*
   saves made under the original rules, shows each save's characters with
   their classes and levels, and brings them in while the six PC positions
   last (`opengold_godot_curse_from_pool_curse`).
5. Combat with *Curse*'s monsters, spells and items. *In progress
   (2026-11-13):* *Curse*'s scripts load its own creatures and fight them as
   their records under the original rules; `opengold_gold_box_games_tests
   --curse-walk` (the check `opengold_curse_walk`) walks every square of
   every map with a party made for *Curse* and plays each event and fight,
   failing on any the session cannot run. Every event runs: *Curse*'s halls
   (every class, its class mask), its pictures from every disk, DESTROY
   ITEMS, its NPCs as their records, CHECK PARTY on its own cells, and a
   COMBAT with no one loaded.
6. Shops, temples, training and the higher level limits. *In progress
   (2026-11-13):* a *Curse* shop sells the item list its script names with
   TREASURE before it opens; its temple adds Heal; its halls train every
   class; the level limits are *Curse*'s (ORIGINAL-RULES.md).
7. A route test that plays from the start to the end, like the
   *Pool of Radiance* expedition checks. *In progress (2026-11-13):*
   `opengold_curse_route` plays Tilverton with the cluebook's party (two
   paladins, a ranger, two clerics and a magic-user, on Quick with Quick
   magic on): the shops, the sage, the king's carriage and jail, the
   Thieves' Guild, the sewers and the Fire Knives' hideout, to the leader's
   fight, the first bond's end and the exile to the wilderness; it trains to
   levels 10 and 11 at the guild's secret hall on test experience, and carries
   +2 weapons and +1 armor, as the Pool of Radiance expedition does, for a
   party as strong as one brought from Pool of Radiance; it camps in the wilderness and journeys by Shadow Gap's
   displacer beasts to Shadowdale, by boat to Ashabenford, past the Fire
   Knives' ambush to the Standing Stones, to Essembra, and through three
   black dragons to Hap, where it hunts four dark elf patrols (healed by the
   test between them, as camps in Hap draw more) and wins the barn; it
   follows the efreeti's map out of Hap's gate to the caves, beats their
   guards, is taken up by Crimdrac the dracolich to the Wizard's Tower,
   parleys with the dragons on its roof, and kills Dracandros in the
   courtyard for the Helm of Dragons (the second bond). Each fight is played
   with up to 40 dice and the best play kept, as a player reloads. It leaves
   the tower by its secret passage to Hap and has its dead raised at the
   Temple of Sune; it leaves Hap, beats Crimdrac on the road, passes the
   bridge's old man and the Standing Stones and the Fire Knives' ambush, and
   reaches Yulash by Hillsfar. In Yulash it asks the Red Plumes' leave, is
   taken to their commander, fights the Zhentil spies from his waiting room
   and pleases him, enters the Pit of Moander, takes Alias and Dragonbait
   into the party, and goes down to the Pit's second level. It beats the shambling mounds over the dead
   cleric for the Wand of Defoliation, which its magic-user takes and
   readies. The route plays as a player would where the original's AI would
   not: its magic-user learns the spells a player takes, its clerics raise
   the dead with Raise Dead after a rest, and each fight is tried with every
   member casting, with the magic-user alone casting, and with the wand and
   Hold Monster used at the right targets. Its player aims Hold Person at people only
   and uses the wand on the mounds, and so beats Mogion, her six cultists and
   five shambling mounds (ECL3.DAX 18 loads them together, as the original
   does), but not yet the three pseudopods that follow at once (140 Hit
   Points each, engulfing): the test sets what their fall sets (the Gauntlet
   of Moander, cells 0x4C5B and 0x4C2D) and the route goes on. It beats the
   cultists' last stand by the Pit's way out (a fight its player may lose is
   won by the test, a stand-in named in the route), leaves by the west door
   at 0,11, where Alias and Dragonbait leave the party (DUMP), raises its
   dead at Voonlar's temple, camps at Yulash's edge and enters Zhentil Keep,
   where Olive Ruskettle leads it by a secret way into the Shrine of Bane.
   It takes Dimswart the sage from his cell and follows the hooded woman
   (who shows herself every other time it passes) to Dexam the Beholder,
   who slays Fzoul and so frees it of his bond, and into the Caves of the
   Beholder. It beats Dexam and his minions there for the Amulet of
   Lathander (his lair meets the party only while cell 0x4C03 is 0, which
   the hooded woman set to 1 and nothing on the way clears; until it is
   known what clears it in the original, the test does: Curse's
   reconstruction in ~/src/Unsafe reads COMPARE and IF as OpenGoldBox does,
   its engine never writes the cell and clears zone 0 only when a game
   starts, no script writes it but the woman's SAVE, and only Dexam's fight
   sets the Amulet's cell 0x4C5A, so this copy's ECL4.DAX would leave the
   Amulet out of reach), and leaves by
   stepping off the map south of 11,15 (LEAVE_MAP), where Olive takes
   Dimswart away, to Zhentil Keep's edge. With the three
   artifacts it goes back by Yulash and Hillsfar to the Standing Stones,
   where the grey man reveals himself as Tyranthraxus ("meet me at Myth
   Drannor"), and journeys on to the Burial Glen (map 64). It leaves the
   Glen east by the path to the ruins of Myth Drannor, wins the head
   Rakshasa over with a haughty parley and goes by the sewer grate into the
   ruined temple (map 67). There Tyranthraxus takes the bonded party,
   Nameless frees it and dies, and the party climbs to the second floor and
   fights the final battle (Tyranthraxus, high priests and margoyles, won by
   the test for now); the game is won, and Curse's own ending is told, read
   from the player's GAME.OVR as its WinGame shows it. The route plays Curse
   from start to end; its stand-ins (test experience and gear, heals where
   no rest is allowed, the pseudopods' and some fights' wins, the cell
   before Dexam) are where the route's player or OpenGoldBox still falls
   short.

### 4. *Secret of the Silver Blades*

The steps of *Curse*'s phase 3, with *Secret*'s differences
([secret.md](games/secret.md)):

1. The census: every file decodes (2026-11-13).
2. The opening: `load_campaign` plays script 17's vision and reaches New
   Verdigris (map 16) with its walls drawn, where a party made under
   *Secret*'s rules hears the mayor and is given the town's equipment
   (`opengold_gold_box_games_tests`, 2026-11-13). The walls of every game are now drawn as the original draws
   them, from one space of symbols for the three slots. *From the startup
   menu* (*Curse*'s step 1.7), since 2026-11-14: the menu offers *Secret*,
   asks for its folder (`SECRET`) and the rules, and the party screen's party
   plays the vision on to New Verdigris, every area named as the cluebook's
   maps name it ([secret.md](games/secret.md), Area names;
   `opengold_godot_startup_menu_secret` checks it on screen), after
   *Secret*'s title pictures, over its title music (SOUND.DAX's piece 2,
   2026-11-14); its saves are kept apart as each game's are
   (*Curse*'s step 1.7).
3. Making a party under *Secret*'s rules: 200,000 experience for a single
   class (100,000 dual, 66,667 triple) trained up at once, its classes,
   level limits and 117 spells; bringing a *Curse* party across.
4. Walking every map and playing every event and fight (the walk), then
   the cluebook's route to the end. *The walk passes (2026-11-13):*
   `opengold_secret_walk` walks every square of every map with six fighters
   made under *Secret*'s rules and runs each event and fight. Its script
   host answers AWARD EXPERIENCE, *Secret*'s ADD NPC (one creature file, no
   one added to a full party), DAMAGE's "hurt even on a save", CHECK PARTY's
   race, Charisma and class levels, and SPELL's Dispel Magic. The fights
   use *Secret*'s combat rules (its effects, creature flags and magic
   resistance, 2026-11-14). *The route (started 2026-11-14):* the party takes
   the mayor's teleporter to the Well of Knowledge, kills the Ancient Red
   Dragon, returns, survives Marcus's welcome and his pets, is healed at the
   Temple of Gond, and leaves town by its east gate (15,8) into the ruins.
   *Further (2026-11-14):* `opengold_secret_route` trains first at the
   town's hall on the experience a party brought from *Curse* carries (a
   stand-in, given four times since an award stops short of the level after
   next: paladins, ranger and clerics reach level 12, the magic-user 13), as
   *Curse*'s route stands in for a party from *Pool of Radiance*. It walks the
   ruins' passages ([secret.md](games/secret.md), Rooms joined by passages)
   west through sectors 5 and 7 to the Old Red Dragon's lair (location 13,
   sector 9), kills it for the Amulet of Eldamar (cell 4C53) and takes its
   gateway to the Well. Rested at the Well, it walks sectors 6 and 8 to the
   Black Circle Headquarters (map 34), beats the captain at its door, the
   commander and his troops, takes the key from his desk, rests at the Well
   by the office's gateway and comes back, finds the secret door by a search,
   bashes the locked door beyond, and beats the hatchlings of the pool and
   the Inner Sanctum (cell 4C4D at 255), whose gateway takes it to the Well.
   Back through the ruins to the mine's entrance (location 12, unguarded now),
   it beats the minotaurs at the Temple of Tyr's doors, meets Derf Strongarm
   (the Ring of Tyr, cell 4C85), takes the wheel lift to each of levels 1 to
   8 for the eight pieces of Oswulf's staff (cell 4C84 at 255; level 2's
   trapdoor noticed in search mode, its trapped chest opened; Vala joins on
   level 6), has Derf mend the staff (cell 4C86), takes level 8's
   malfunctioning teleporter to level 9 and its umber hulks, takes down the
   force wall across the lift's shaft (cell 4C70), rests in the Temple of
   Tyr (the lift's rests are interrupted), and walks level 10 to the hole the
   miners broke into the Dungeon (map 64). Every walk inside a map is played
   step by step as the planning aid `OPENGOLD_SECRET_FIND` found it (each
   arrival's event runs; the party answers NO where it only passes by, and a
   walk may carry the answers it is to give on the way); the riddles and
   spoken words are answered as the cluebook answers them. At the town's
   armoury it buys each member a mirror, which turns the basilisks' and
   medusae's gazes back. A member turned to stone stays a statue until Stone
   to Flesh (ORIGINAL-RULES.md, Stone): from the Dungeon's entrance the party
   goes by the Dungeon's teleporter (location 7) to the Well and the Temple
   of Gond (Raise Dead, Stone to Flesh) and back by the Well's gate 15. It
   takes the Brass Key (level 7, the Amulet revealing it), the Silver Key
   (levels 6 to 3, the pyro-hydras) and the Gold Key (levels 2 and 1, the
   purple worms; cells 4CDE, 4CDF, 4CE0), leaves the Dungeon's top into the
   Western Crevasses (map 80; its places told apart by cells 4C05 and
   4C6B), walks them to the Frost Giant Village (map 81), bashes its
   blockaded door open, wins the king over by fighting the Black Circle
   beside him, has his guards lead it east into the Eastern Crevasses (map
   82), and passes the castle's gate, where Oswulf's spirit blesses the
   staff's bearers, into the Castle Entrance (map 96). *The castle (since
   2026-11-14):* the castle's guards break every camp, so the hurt party
   walks along the great hall, attacking Sargatha at once when she offers
   her scroll (signing sends it robbed to the Dungeon), through the rotating
   alcove at 5,9 to the gateway at 6,9, rests at the Well and comes back by
   its gate 13; pulls the lever at 5,15 that turns that room's alcove back,
   takes the alcove at 8,9 into the next room and the flagstone at 9,15 to
   13,0, and climbs the stairway at 15,0 to the Middle Level (map 97). Its
   maze leads to the stairway up at 0,15; its iron golems are told STEELEYE,
   also where a magic mouth calls the password FOOL. In the Dreadlord's
   Sanctum (map 98) the party lets the Dreadlord's illusion attack first,
   which ends it, walks on hurt to the gateway at 12,11 to rest at the Well
   and comes back by its gate 6; bashes through the illusory lightning at
   13,9, opens the door at 12,0 with the Silver Key, answers OSWULF at the
   door asking for the Dreadlord's brother (8,9), beats Sargatha's ambush,
   opens the door at 2,9 with the Gold Key, beats the warrior in golden
   armor at 7,1 and opens the door at 6,1 with the Brass Key onto the
   Dreadlord's rooms (script 99 on the Sanctum's map): the hydras, the
   Dreadlord himself, a secret door found by searching at 1,3, and the
   chamber of his soul, explored, where the iron golems and medusae fall
   and the gem is shattered (cell 4CD9 set to 1): "Eldamar's soul is free,
   and the Dreadlord is no more. You have won." (`game_won`,
   [secret.md](games/secret.md), The ending). `secret_route_ends_the_game`
   is true: the check asks for the win, every member alive, as *Curse*'s
   does. *Stand-ins:* the experience above; and from the mine on, a fight
   on the way (or in a camp on the way) that the route's player loses with
   every dice it tries, or wins only with a member dead or turned to stone,
   is won by the test (42 fights in the last run, most in the
   crevasses and the castle, the castle's last fights all of them: the
   warrior in golden armor, the hydras, the Dreadlord and the soul gem's
   guards). *The route's own play:* a party below half its Hit Points rests
   before passing on, flees monsters met in camp while hurt, rests to raise
   its dead while a cleric stands, and gives up resting on a map where
   monsters break its camp three times. *Planning aids:*
   `OPENGOLD_ROUTE_SAVE=FILE` saves the game where the route ends,
   `OPENGOLD_ROUTE_RESUME=FILE` plays on from such a save, and
   `OPENGOLD_ROUTE_BEATS=N` plays only the first N beats, so a search
   (`OPENGOLD_SECRET_FIND`) starts in seconds where the route ends.

### 5 to 7. The other campaigns

*Gateway to the Savage Frontier*, *Pools of Darkness* and *Treasures of the
Savage Frontier*, in that order. Each repeats steps 1 and 3 and adds only its
own differences. *Pools of Darkness*
and *Treasures* bring 256-color VGA pictures and music, so their census needs
new decoders.

*Gateway to the Savage Frontier* ([gateway.md](games/gateway.md)):

1. The census: every file decodes (2026-11-13).
2. The opening: `load_campaign` plays script 1 of disk 2, the night of
   celebration over picture 5 and the morning's loss, and reaches Yartar
   (map 1) with the walls of its three large sets drawn, the party in the
   inn's room at (1,10) facing north (`opengold_gateway_tests`, 2026-11-14).
   Its files without a disk number are read first, its pictures in the VGA
   mode's colors, its item words from `GAME.EXE`, and CALL 880Ah and 880Dh
   ask for Enter and for Yes or No.
3. Walking every map and playing every event and fight (the walk). *The
   walk passes (2026-11-14):* `opengold_gateway_walk` walks every square of
   every map with six fighters made under Gateway's rules. Its fights load
   Gateway's own combat icons; the overland (script 21, maps 21 to 26 by the
   cell 4C24h) loads its symbol sets alone and is drawn from above with the
   party's arrow, and its fights are on grass with a river, trees and
   features from `GAME.EXE`'s terrain table ([gateway.md](games/gateway.md),
   The overland). CALL 8810h changes the coins up (2026-11-14).
4. *From the startup menu (2026-11-14):* the menu offers *Gateway*, asks for
   its folder (`GATEWAY`) and the rules, and shows its five title pictures
   (3, 3, 10, 20 and 20 seconds, each on a cleared screen; a key on the first ends the title, a later key passes one)
   with its title music from the second, as its Intro does; the party
   screen's party plays the opening on to Yartar
   (`opengold_godot_startup_menu_gateway`).
5. The cluebook's route to the end. *Plays to the win (2026-11-14):*
   `opengold_gateway_route` plays the game with the cluebook's party (two
   paladins, a ranger, two clerics and a magic-user, made under Gateway's
   rules at 3,000 experience), carrying +2 weapons and +1 armor (a stand-in,
   the route's gear in every game). In Yartar it saves Krevish from the
   thugs and takes the captain's mission; it goes up the river by boat to
   Nesme and kills the Banite cleric in his hidden temple; walks overland to
   Silverymoon, beats the trolls and frees Erek; walks to Everlund, passes
   the owlbears' stable and the passage under the Rauvin, and rescues
   Amanitas (the Ring of Reversal, cell 4C21; Erek leaves with him). Back in
   Yartar it trains twice on test experience and gold (a stand-in for the
   fights a player would have fought by then: 6,000 each a visit), is pulled
   into the Kraken complex, where Krevish climbs through the cell's grating,
   beats the captain and crosses the squid tank. It walks overland, fleeing
   the wandering monsters it can (as a player does), to Neverwinter and
   Luskan, enters the Hosttower, takes the trap door down to its dungeon,
   frees Brinshaar and follows the maze to the statuette of the East (cell
   4C27 bit 1), beats Brinshaar's ambush, takes the Brazen Pennant's ship to
   Tuern and on to Gundarlun, trains twice more there on test experience
   (25,000 each a visit, to level 6, the stand-in again), has its dead
   raised, and takes Gundbarg's ship, which the kraken wrecks on the Purple
   Rocks; it crosses to the Kraken headquarters. There it races south through
   the fortress out of search mode, its guards held from fleeing, to the
   Pearl Chest's room and looks about (ten minutes a Look) until the chest's
   guards come 30 minutes after the landing: the statuette of the West (bit
   2). The hidden boat takes it to Neverwinter, where it beats the Zhentarim
   ambush, and it walks overland to Secomber and on to Llorkh, whose tavern
   drops it into the arena; it wins the arena's three fights, rests in its
   cell, Krevish snatches the jailer's keys, Muthtur joins, and it fights
   through the barracks to the commander's room: the statuette of the North
   (bit 3) and the others back. Rested in Llorkh, it walks to the foot of the
   Star Mounts, saves the aarakocra from the giant snakes and shambling
   mounds, is flown to Ceptienne's aerie and through its maze to her chamber:
   the statuette of the South (bit 4). Flown down, it walks to Sundabar and
   from its west gate overland round the mountains to Ascore, opens
   Vaalgamon's gate with the four statuettes past its spiders, and takes his
   challenge: in his maze of fights it wins the first outright and runs north
   out of the next three, the last his own room, to the plaza, where the
   statuettes are set in the altar. Script 28 ends with the closing words of
   the player's `GAME.OVR` and "You have won." as a win (`game_won`), which
   the route's check asks for ([gateway.md](games/gateway.md), The ending).
   The ships sail at 0800 and 0900: the test passes the time to the sailing,
   a stand-in for a player's wait in camp, which OpenGoldBox's camp has no
   hours for. Every walk is played step by step as the planning aid
   (`OPENGOLD_SECRET_FIND`, with `OPENGOLD_FIND_YES` for the questions it must
   answer Yes) finds it. The stand-ins are the test experience and gold, the
   gear and the waits for the ships; no fight is won by the test. It rests on
   the gate's square before the maze: Ascore's camp script (ECL6:20 0x818D)
   lets a party sleep only on squares of events 4 and 107 on (but 121), and
   breaks every other camp with undead, the rooms just east of the gate too,
   which the cluebook calls safe; OpenGoldBox follows the script. What the
   route needed of the engine is in [gateway.md](games/gateway.md), What the
   route needed: the square's event read after a map's load entry, Look's ten
   minutes, effect 84 a charming gaze in Pool of Radiance only, and the maze
   fights that one member's escape ends.

*Pools of Darkness* ([darkness.md](games/darkness.md)):

1. The census: every file decodes (2026-11-13).
2. The opening: `load_campaign` plays script 16, New Phlan's docks (map 16,
   wall set 1), where Rolf the portmaster offers a tour over picture 148;
   declined, he walks the party out to (11,2) facing south, the docks' walls
   drawn in their own 256 colors (`opengold_darkness_tests`, 2026-11-14). Its
   script memory is its own (darkness.md, Script memory), and the machine runs
   its new commands.
3. Making a party under *Pools of Darkness*'s rules (ORIGINAL-RULES.md,
   *Pools of Darkness*): 1,500,001 experience a single class (750,000 dual,
   500,000 triple) trained up at once, 200 platinum, its tables to level 40
   and its creation spells (2026-11-14, `opengold_original_creation_tests`).
   Bringing a *Secret* party across, which the original offers, is still to
   come.
4. Walking every map and playing every event and fight (the walk). *The walk
   passes (2026-11-14):* `opengold_darkness_walk` walks every square of every
   map a script loads with six fighters made under its rules. Its script
   host reads and writes the current character's record byte for byte, hands
   out TREASURE from `ITEM0`'s definitions, opens the stores its `GAME.EXE`
   lists, and answers FIND NPC, SPELL and its DAMAGE forms; its sprites and
   combat icons are drawn in their own colors (darkness.md, The walk). Still
   to come: the startup menu. A fight that ends
   without a result fails any game's walk (2026-11-14).
5. The route from the opening to the win (`opengold_darkness_route`). *Begun
   (2026-11-14):* the route plays the prologue: Rolf's tour, Sasha's escort
   duty at West Phlan's gate, the storm that takes Phlan, Elminster in Limbo,
   and steps on the area map, where the party moves on a 38 by 15 grid
   (darkness.md, The area map), across the Lands of Thar to Taydome's Keep,
   where it beats Kimarr's first attack in the hall, frees Bostell and
   beats Kimarr in his chambers, so Quil breaks the great spell and Sasha is
   safe; then Vala's cave, where it takes the four items with Vala and turns
   back the Vaasans; then Priam leads it into the Temple of Tyr, which it
   frees: the catacombs' tests and the Lamp of Vigilance, the four braziers
   lit, the bells rung so Laurellin and Brimwulf are reunited, the plunder
   left alone; then the Hill Giant Steading, where it wins its way to the
   inner steading for Kardal, rescues him and beats the Hill Giant Shaman,
   and goes through into the Fire Giant Cave. The win is
   PROGRAM 1 after Gothmenes falls (darkness.md, The win and the route).
   *Stand-ins:* the test wins the fight with the Vaasans who get through
   Vala's breach (their mages hold the party with Hold Monster). Until the
   route reaches the win it passes when it plays every beat to the Fire
   Giant Cave (map 32). *Where it stops (2026-11-14):* at the Fire Giant
   Cave's entrance (map 32, 3,15); next, the cave (Ungleow's endless
   corridor and the ring in the skull, darkness.md), the Dragons' Aerie,
   Thorne's cave, then counter-clockwise round the area map to Dark Phlan. *The area map is drawn (2026-11-14):* the big picture with
   the party's marker, as the original draws it. Still to come: spell levels 8 and 9
   and the new spells, starting equipment, skies, combat ground tiles, sound
   and music.

*Treasures of the Savage Frontier* ([treasure.md](games/treasure.md)) runs on
the engine of *Pools of Darkness*, and OpenGoldBox plays both with one code
path (`vga_engine`): the same loader, script machine, memory map, record,
pictures, walls and script host, *Treasures* adding only what it changes.

1. The census: every file decodes (2026-11-13).
2. The opening: `load_campaign` plays script 16, the afternoon above Yartar
   and the waking in the dwarves' fortress over pictures 2 to 4, and reaches
   map 16 with the walls of its three wall sets drawn in 256 colors, the
   party at (10,7) facing north (`opengold_treasure_tests`, 2026-11-14).
3. Making a party under *Treasures*' rules: 22,600 experience (11,300 or
   7,533 for each class of two or three) trained at once and 200 platinum,
   on its engine's tables (ORIGINAL-RULES.md, *Treasures*). A *Gateway*
   character comes across as its LoadChar takes one in, its items renumbered
   (`gateway_items_for_treasures`); the party screen's choice of saves is to come.
4. Walking every map and playing every event and fight (the walk).
   `opengold_treasure_walk` (`opengold_gold_box_games_tests
   --treasure-walk`) walks every square of every map, the overland's too
   (script 51, maps 51 to 57, drawn from above from `OVER`'s pictures), with
   six fighters made under *Treasures*' rules, and runs each event and fight
   (treasure.md, The walk). Still to come: the music and sounds, the
   romances and luck readings (PROGRAM 3), the startup menu and the route.
   The win is in place: the event in which Freezefire dies (script 38 sets
   cell 25Ch to 1) ends as a win (`game_won`), the original having no ending
   of its own (treasure.md, The ending).

### 8. *Unlimited Adventures*

1. Playing adventures made with the original construction kit. *Started
   2026-11-14* ([ua.md](games/ua.md), Playing a design): a design is read
   from its folder over the kit's own files, and a session of its own
   (`ua::UaSession`, beside `RolfTourSession`, since the kit's events are
   not scripts) plays it through the view's interfaces: the same snapshot,
   commands and answers, its fights as a `CampaignEncounter` for the
   original rules' combat, and its 3D view drawn from its wall sets and
   backdrops. A party made under the kit's rules (*Curse*'s tables, no
   half-orcs, the kit's 40,001 experience and 200 platinum) starts at the
   game record's area and entry point; text, treasure, damage, transfers,
   menus, questions, quests, flags, experience, time, chains and fights with
   the kit's monsters run. No design ships with the developer's copy, so
   the tests build one from the kit's templates; `opengold_ua_walk` walks
   every square of its areas and plays every event and fight. Since
   2026-11-14 the game plays a design from the startup menu: *Unlimited
   Adventures*' button asks for the kit's folder and then the design's
   (`NAME.DSN`), Create new party makes characters under the kit's rules,
   and the exploration screen plays the design through `ExplorationSession`,
   the one view code path for a DAX campaign and a design; its fights open
   on the combat screen with the design's monsters
   (`opengold_godot_startup_menu_ua`). Since 2026-11-14 every event kind
   plays: training halls, taverns, shops, temples, vaults, inns and small
   towns in the kit's words and prices, guided tours, NPCs joining and
   leaving, encounters, who tries, who pays and passwords; treasure hands
   over its items, a trap's save halves its damage, and a new character
   starts with the design's own experience, money and equipment. Since
   2026-11-14 its fights are the kit's: its battlefields (the walls around
   the party in the area's DUNGCOM set, or WILDCOM's ground out of doors),
   its monsters' CPIC icons and sizes, their items readied, allies, the
   enemies' starting quarters, the distance a wall ahead cuts short, and
   the kit's experience and loot after a won fight. Still to come:
   overland maps, locked doors, monsters' effects, "no magic" fights, the
   treasure menu, the rest of its rules, the camp menu, the overhead map, a
   default party, and saves.
2. An updated construction kit for making new ones.

### 9. The SRD rules for the new games

Once every game plays: monster conversions, shops and art mappings for each
game, as *Pool of Radiance* has.

### Later: *Krynn* and *Buck Rogers*

*Champions of Krynn*, *Death Knights of Krynn*, *The Dark Queen of Krynn*,
*Buck Rogers: Countdown to Doomsday* and *Buck Rogers: Matrix Cubed*, with the
same method.
