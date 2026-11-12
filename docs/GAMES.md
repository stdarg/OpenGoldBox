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
| 2 | *Curse of the Azure Bonds* | 1989 | `CURSE` | Every file decodes ([curse.md](games/curse.md)) |
| 3 | *Secret of the Silver Blades* | 1990 | `SECRET` | Not started |
| 4 | *Gateway to the Savage Frontier* | 1991 | `GATEWAY` | Not started |
| 5 | *Pools of Darkness* | 1991 | `DARKNESS` | Not started |
| 6 | *Treasures of the Savage Frontier* | 1992 | `TREASURE` | Not started |
| 7 | *Forgotten Realms: Unlimited Adventures* | 1993 | `UA` | Not started |

The developer's copies live in `/Users/edmond/GoldBox/<folder>`.

The original let a party travel on: *Curse* imports a *Pool of Radiance*
party, *Secret* a *Curse* party and *Pools of Darkness* a *Secret* party.
*Treasures* imports a *Gateway* party. OpenGoldBox follows the original.

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
| *Secret* | Maps, pictures, combat icons, sprites, item lists, effects, item types | Scripts, creatures, some creatures' items, some walls, the font, the name tables and sounds, `SOUND.DAX` |
| *Gateway* | Maps, combat heads and bodies, effects, some walls and items | Scripts, creatures, pictures, combat icons, `ITEMS.1`, music and sounds |
| *Pools of Darkness* | Maps, effects | Nearly everything else: its pictures are 256-color VGA, and it adds new files |
| *Treasures* | Maps, effects | As for *Pools of Darkness* |
| *Unlimited Adventures* | — | Everything: its data sits in `DISK1` to `DISK3` |

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

**Status:** done for *Pool of Radiance* and *Curse* (2026-11-12). Run it with
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
   An old save loads as *Pool of Radiance*.
6. **The campaign behind one seam:** the game reaches *Pool of Radiance*'s own
   content (the Phlan session, the wilderness, teleports, secrets, portraits,
   place names) through one interface chosen by the game, so *Curse*'s can sit
   beside it.

### 3. *Curse of the Azure Bonds*

In small steps, each with its check:

1. Choosing *Curse* at New Game shows its title and a first map.
2. Walking the maps, with walls, doors and events.
3. The scripts' text and menus, and the commands *Curse* adds. The script
   coverage ([ECL-COVERAGE.md](ECL-COVERAGE.md)) is kept for each game.
4. Making a party, and bringing a *Pool of Radiance* party across.
5. Combat with *Curse*'s monsters, spells and items.
6. Shops, temples, training and the higher level limits.
7. A route test that plays from the start to the end, like the
   *Pool of Radiance* expedition checks.

### 4 to 7. The other campaigns

*Secret of the Silver Blades*, *Gateway to the Savage Frontier*,
*Pools of Darkness* and *Treasures of the Savage Frontier*, in that order. Each
repeats steps 1 and 3 and adds only its own differences. *Pools of Darkness*
and *Treasures* bring 256-color VGA pictures and music, so their census needs
new decoders.

### 8. *Unlimited Adventures*

1. Playing adventures made with the original construction kit.
2. An updated construction kit for making new ones.

### 9. The SRD rules for the new games

Once every game plays: monster conversions, shops and art mappings for each
game, as *Pool of Radiance* has.

### Later: *Krynn* and *Buck Rogers*

*Champions of Krynn*, *Death Knights of Krynn*, *The Dark Queen of Krynn*,
*Buck Rogers: Countdown to Doomsday* and *Buck Rogers: Matrix Cubed*, with the
same method.
