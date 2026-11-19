# The Krynn games

The plan for OpenGoldBox to play the three *Dragonlance* Gold Box games from
start to finish: *Champions of Krynn* (1990), *Death Knights of Krynn* (1991)
and *The Dark Queen of Krynn* (1992). It follows the method of the *Forgotten
Realms* games ([GAMES.md](GAMES.md)). The active all-twelve-game request
supersedes the earlier wait for another instruction. Champions' fresh earned
ending is verified (123 battles, zero defeats, all six PCs alive), with native,
Godot and Linux package checks complete. Death Knights' fresh imported campaign
also reaches its authored ending, with all six imported PCs alive and a
byte-identical ordinary winning-save roundtrip. Its native integration,
application and Linux package checks pass. See the implementation records for
[Champions](games/champions.md), [Death Knights](games/death-knights.md) and
[playability](PLAYABILITY.md). Dark Queen follows Death Knights; the current
task does not authorize SRD work or delegation.

## Where the files are

| Game | Folder | Game files | Manual, cluebook, journal | Reconstructed source |
|---|---|---|---|---|
| *Champions of Krynn* | `COK` | `~/GoldBox/COK` | `~/GoldBox/docs/COK` | `~/src/unsafe/champions-of-krynn` |
| *Death Knights of Krynn* | `DKK` | `~/GoldBox/DKK` | `~/GoldBox/docs/DKK` | `~/src/unsafe/death-knights-of-krynn` |
| *The Dark Queen of Krynn* | `DQK` | `~/GoldBox/DQK` | `~/GoldBox/docs/DQK` | `~/src/unsafe/dark-queen-of-krynn` |

Each docs folder holds the Manual, Cluebook, Journal, Codes and Controls. As
with the other games, the sources carry no license. OpenGoldBox learns from
them and writes its own code and text, and copies neither.

## What the games are made of

- ***Champions* and *Death Knights*** are Turbo Pascal programs of about
  35,000 lines each, with the same units as *Curse* and *Secret*: the script
  machine (`eclvm`), combat, magic, effects, camp, shops, temples, character
  creation and saving. Their data is the familiar DAX family:
  - shared with the Realms games: `ECL`, `GEO`, `WALLDEF`, `8X8D`, `PIC`,
    `BIGPIC`, `CPIC`, `SPRIT`, the monster files `MONnCHA`, `MONnITM` and
    `MONnSPC`, `ITEMS`, `SOUND`;
  - *Champions*' own: `SKY`, `TILES`, `MIDI`, `KRYNN.CFG`;
  - *Death Knights*' own: `BACK`, `SKYGRND`, `MUSIC`, `MON1WIZ`, `DKK.CFG`.

  The first readers for these come from the Realms games' readers.
- ***The Dark Queen*** is a C program of about 45,000 lines, the 256-color
  engine that *Unlimited Adventures* was built from. Its data comes as `DISK1`
  to `DISK3`, the same resource libraries (`TLB`, `GLB`) that OpenGoldBox
  already reads for *Unlimited Adventures*:
  - scripts: `ECL.GLB`;
  - maps: `GEO.GLB`;
  - monsters: `MONCHA.GLB`;
  - items: `ITEMS.DAT`;
  - pictures, walls, sprites and frames: `TLB`s;
  - music and sound: `XMI`, `VOC`, `SOUNDS.GLB`;
  - the vault: `VAULTA.DAT`.

  Its reconstruction is 88% C; the rest is mostly the linker's own structures.
  It plays under DOS, though it is not byte-identical.

## What is new in Krynn

The rules differ from the Realms games more than any Realms game differs from
another. Each item below is checked against the sources and manuals in its
step, and written up in `docs/games/<game>.md` in our own words.

- **Races:** Silvanesti and Qualinesti elves, half-elves, hill and mountain
  dwarves, kender and humans. The census settles each game's exact list and its
  limits.
- **Knights of Solamnia,** a class with three orders (Crown, Sword and Rose),
  requirements to enter and rise, promotion in the halls, and training of its
  own.
- **Clerics serve a god** chosen at creation, and the god sets the cleric's
  alignment, spells and powers (the deity tables in the sources).
- **Mages wear a robe** (White, Red or Black), which sets their alignment and
  spells.
- **The moons:** from *Death Knights* on, Solinari, Lunitari and Nuitari wax
  and wane on the game's clock. Their phases change what each robe's mages can
  do. *Champions*' source has no moon code, and its census will confirm that.
- **Kender** taunt their foes, and **draconians** die in their own ways (the
  manuals' kinds, checked in K2).
- **Each game's own spell table, effects and monster powers,** as in each Realms
  game.
- **Moving a party between games:** *Death Knights* brings in *Champions*
  characters, and *The Dark Queen* brings in *Death Knights* characters, each
  game converting them by its own rules. *The Dark Queen* also has a vault for
  keeping items between games.
- **Copy protection** (the Codes) is not played, as with the other games.

## How the work stays safe

The Realms games' rules hold:

- **Read before playing.** A game is first decoded, read-only, with a
  measurable result.
- **Every test of the Realms games keeps passing.** *Pool of Radiance* stays
  the default game.
- **Code becomes shared only when a second game needs it.** Krynn's rules live
  in the original-rules module (`OpenGold.Rules.Original`), keyed by game, as
  *Pools of Darkness*' and *Treasures*' own tables are.
- **One code path.** The tests, the walks and the routes run the real game's
  code.
- **Every step ends in a check that reads the real files.** It skips when a
  game's folder is missing.

## Plan

The steps follow release order, and each game builds on the one before. Steps 1
and 2 can run for the three games in parallel, by separate agents in their own
worktrees. From step 3 on, each game's route needs the previous game's rules.

### K0. Groundwork

- `GoldBoxGame` gets `champions_of_krynn`, `death_knights_of_krynn` and
  `dark_queen_of_krynn`.
- The game recognizes the `COK`, `DKK` and `DQK` folders by their files, and
  the settings hold a folder for each.
- A save records its game. A campaign format change keeps every old save
  loading as its own game.
- The startup menu offers the three games. Each button stays disabled until
  its game plays.
- GAMES.md's "Later" section points here. Each game gets its
  `docs/games/<game>.md`.

Done when every test passes and the menu shows the three games.

### K1. A reading census for each game

`opengold_census` decodes every file of each game: pictures, walls, sprites,
fonts, monsters, items, scripts, maps, sounds and music.

- ***Champions* and *Death Knights*:** extend the DAX readers to the new kinds
  of file (`SKY`, `TILES`, `BACK`, `SKYGRND`, `MON1WIZ`, the music) and to any
  record that changed size. Decode every script with Krynn's new commands.
- ***The Dark Queen*:** reuse the *Unlimited Adventures* library readers, and
  add whatever the campaign's libraries hold that the kit's do not.

The facts each reader needs go into the game's doc: record sizes, where scripts
load, and new commands.

Done for a game when `opengold_game_census` decodes every file of it.

### K2. Krynn's rules (original rules)

Built in the original-rules module, keyed by game:

- races, classes and their limits, and character creation as each game has it,
  multi-classes included;
- Knights of Solamnia with their orders, promotion and training;
- clerics' gods, and mages' robes;
- the moons and their effect on spells (from *Death Knights* on);
- each game's experience, level limits, training and fees;
- each game's spell table and effects;
- combat: the computer's tactics, kender, draconians' deaths and the monsters'
  powers;
- shops, temples, the halls, and camp: rest, memorizing and scribing as the
  game has them.

Each rule gets a test that names the source routine it follows, as in
[ORIGINAL-RULES.md](ORIGINAL-RULES.md). Obvious bugs in the original are fixed
and listed under its Bugs. Questions the sources and manuals do not settle go
to the user.

Done for a game when a party it creates is one the original would create, and
the original-rules combat and camp tests pass for that game.

### K3. *Champions of Krynn*

1. **The engine plays the game:**
   - its 3D views and walls, and its overland map;
   - its script commands and events;
   - its fights on its own battlefields and art;
   - its shops, temples and halls.
2. **A walk test** visits every map and starts every fight its scripts can
   load.
3. **A route test** plays from the opening to the end with the game's default
   party, as the Realms routes do. It uses the same planning aids, per-beat
   dice and reloads where a player would reload. Certification requires actual
   wins, earned rewards and physically valid movement. Forced outcomes, grants,
   healing or placements remain diagnostic assistance and cannot certify a win.
4. The ending is shown, with room for a drop-in ending film
   (`endings/COK.ogv`, made by the user).

Done when the route test sets `game_won`.

### K4. *Death Knights of Krynn*

The same four parts, and:

- the moons on the game's clock;
- bringing in a *Champions* party by the game's own conversion, from an
  OpenGoldBox save of the end of *Champions*;
- the game's own music;
- its own way of starting a new party.

Done when the route test, starting from a party brought in from *Champions*,
sets `game_won`.

The fresh 331-beat imported route passes in 425.62 seconds and reaches the
native council's victory. All six PCs survive; ordinary healing after the
council leaves all six standing. The normal format-53 winning save reloads
byte-identically. Native integration and application/package checks pass; see
[the evidence](games/death-knights.md#fresh-earned-campaign).

### K5. *The Dark Queen of Krynn*

The same four parts on the 256-color engine. It shares the most with *Unlimited
Adventures*' play (resource libraries, glyphs, its 3D view and fights) and with
*Pools of Darkness* and *Treasures* (that engine generation's rules). It adds:

- bringing in a *Death Knights* party;
- the vault;
- the game's palette, music and digitized sound.

Done when the route test, starting from a party brought in from *Death
Knights*, sets `game_won`.

### K6. Each game's settings

These carry over from the Realms games, per game:

- the difficulty levels (Normal is the original);
- the default parties, one per ruleset, made by the game's own creation rules;
- the options for any of the original's behaviours the user makes optional;
- the Spanish text;
- the ending films.

### K7. The SRD rules for Krynn (later)

Not part of this goal (the user, 2026-11-16). No SRD work comes before every
Gold Box game, the *Buck Rogers* games included, plays and wins under its
original rules. Then,
as the Realms games' plan step 9 has it, they would come as content work:
monster conversions (each creature stays what its art shows), shops, art
mappings and an SRD form of the Krynn classes and races.

## Historical parallelization sketch

Current execution is sequential. This older sketch is not authorization to
spawn agents; the active session uses one implementation owner.

Each step runs in agents' own worktrees, claimed on the session board. The
agents share no file except the shared numbers (campaign format, combat
format, rules version), which are claimed before they are raised.

- **K0:** one agent.
- **K1:** three agents, one per game: the DAX readers for *Champions* and
  *Death Knights*, and the library readers for *The Dark Queen*.
- **K2:** *Champions*' rules first, then *Death Knights*' and *The Dark Queen*'s
  in parallel. Each game's rules sit in their own files and tables.
- **K3 to K5:** one route agent per game. *Death Knights*' route can start on
  new characters while *Champions*' route is still being finished, and switch
  to a brought-in party when *Champions* wins.
- **K6:** after each game's route wins.

The coordinator merges each agent's branch after the full native and game
suites pass, and pushes it.

## Risks

- ***The Dark Queen*'s source** is not byte-identical. About 8% of it is
  generated assembly, most of it the linker's own structures. Behaviour in
  doubt is checked against the manual and cluebook, and against the game run
  under DOSBox-X when needed.
- **The amount of new rules** (knights, gods, robes, moons) is larger than any
  Realms game added. K2 is the longest step.
- **Bringing parties between games** depends on the previous game's route
  winning, so *Death Knights*' and *The Dark Queen*'s routes finish last.
- **The startup menu's ten games** need a new layout (see Decisions).

## Decisions

The user's answers (2026-11-16):

1. **When:** the user decides, after the *Forgotten Realms* games are playable
   and winnable.
2. **The startup menu:** the user suggested a dropdown for the ten games. The
   layout is settled with mockups of the real screen in step K0.
3. **The SRD rules for Krynn:** not in this goal; they may come later (K7).
