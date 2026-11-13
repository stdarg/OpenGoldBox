# Curse of the Azure Bonds: how its files differ

*Curse of the Azure Bonds* (1989) runs on the next version of *Pool of
Radiance*'s engine. This page records what its files change, as the census
(`opengold_census`, [GAMES.md](../GAMES.md)) reads them. The facts come from the
reconstructed source of the DOS release 1.3 and from the files themselves;
the wording is our own.

Since 2026-11-12 every file of the game decodes.

## The files

| Files | What they hold | Change from *Pool of Radiance* |
|---|---|---|
| `GEO2`–`GEO6` | 16 maps | None |
| `ECL1`–`ECL6` | 25 scripts | A new memory bank and three new commands (below) |
| `MON1CHA`–`MON6CHA` | 81 creatures | A new record of 422 bytes (below) |
| `MON*ITM`, `ITEM*` | Creatures' items, item lists | None: 63 bytes an item |
| `MON*SPC` | Creatures' effects | None: 9 bytes an effect |
| `ITEMS` | Item types | None |
| `HEAD2`–`HEAD6`, `BODY2`–`BODY6`, `PIC*` | Each disk's portraits and close-ups for the scripts | Same format; a number can mean another picture on another disk, and none are for the player |
| `TITLE` | Title pictures | Four pictures instead of two |
| `BIGPIC1`, `2`, `6` | Four large pictures, such as the overland map | New file; ordinary pictures |
| `SKY` | The north sky, the sun and the horizon (records 250 to 252) | New file; ordinary pictures |
| `TILES` | The copy protection's symbols: sets of 26 and 22 symbols, 24 by 24 pixels | New file; the combat icons' format |
| `CHEAD`, `CBODY`, `CPIC*`, `COMSPR` | Combat icons | None |
| `DUNGCOM`, `WILDCOM`, `RANDCOM` | Battlefield tiles | None |
| `SPRIT*` | Monsters seen at a distance | None |
| `8X8D1` | The font (201), the symbols of set 4 (202) and the shared wall tiles (203) | Holds only those three |
| `8X8D2`–`8X8D6`, `WALLDEF2`–`WALLDEF6` | Each disk's wall tiles and walls | None: the same pairing of blocks and tiles |
| `START.EXE` | Item and spell names, and the sound effects | The sound driver moved (below) |

*Pool of Radiance*'s `ICON`, `BACPAC`, `SQRPACI` and `FINAL` files have no
counterpart.

The original shows a portrait from the current disk's `HEAD` and `BODY`
files. *Curse*'s characters are not given portraits: the record's portrait
fields come only from a *Pool of Radiance* character brought across.

## Scripts

- A script loads at **8000h** and may run to 9DFFh. *Pool of Radiance*'s load at
  9900h and run to B6FFh. Everything else about a record is the same: two
  header bytes, five entry jumps, then the body.
- Operands are encoded as in *Pool of Radiance*.
- **ECL CLOCK** (34h) takes two operands instead of one.
- Three commands are new:

| Code | Command | Operands |
|---|---|---|
| 3Eh | DUMP | none |
| 3Fh | FIND SPECIAL | 1 |
| 40h | DESTROY ITEMS | 1 |

DUMP sends the selected character (the last LOAD CHARACTER's) out of the
party with what it carries, as Alias and Dragonbait leave at the Pit of
Moander's way out; its slot stays empty. DESTROY ITEMS takes every item of a
type from the party. FIND SPECIAL is not run yet.

### Script memory

Scripts read and write the game through numbered cells. *Curse* moved
*Pool of Radiance*'s zones of cells whole, and each cell kept its place within
its zone (`game_address`):

| Zone | *Pool of Radiance* | *Curse* |
|---|---|---|
| The game's state (map, clock, flags) | 4900h–4CFFh | 4B00h–4EFFh |
| The current character, then the party's and the fight's cells | 6B00h–6EFFh | 7C00h–7FFFh |
| Strings | 9700h–98FFh | 7A00h–7BFFh |
| The script itself | 9900h–B6FFh | 8000h–9DFFh |

Every cell *Curse*'s source names (the map loaded, the clock, morale,
surprise, the party's size and the rest) is where this puts it. The cells
outside the zones, such as the party's position at C04Bh, did not move. The
current character's cells at the start of zone 1 are not the record's bytes:
the original serves each from a field of the record (SpecialReadZ1), at the
same offsets as *Pool of Radiance* (Constitution at +18h, race at +72h, thief
skills at +A5h, coins from +BBh, status at +100h, side at +10Ch), so they move
with the zone. *Curse*'s scripts also read Intelligence (+15h), class (+73h),
magic-user level (+C9h), gender (+D6h) and a script flag (+E4h); OpenGoldBox
gives them Intelligence and gender, and the class and level once the
characters carry the original's classes.

## Creatures

The record grew from 285 to 422 bytes: each ability now keeps a permanent and
a current score, a creature can know 100 spells and hold 84 memorized ones,
and a character remembers its levels before a change of class. Byte offsets:

| Offset | Field |
|---|---|
| 0 | Name: a length byte and up to 15 letters |
| 16–27 | Six abilities (Strength, Intelligence, Wisdom, Dexterity, Constitution, Charisma), each a permanent and a current score |
| 28, 29 | Exceptional Strength: current, permanent |
| 30–113 | Memorized spells, one a slot; bit 7 while still being memorized |
| 114 | Hours of rest before memorizing finishes |
| 115 | THAC0, as 60 minus the value |
| 116, 117 | Race, class |
| 118 | Age (two bytes) |
| 120 | Maximum hit points |
| 121–220 | Known spells |
| 221 | Attack level |
| 223–227 | Five saving throws |
| 228 | Movement |
| 229–233 | Highest level, level at the change of class, drained levels, drained hit points, undead level |
| 234–241 | Thief skills |
| 247 | NPC flag |
| 251–264 | Coins: copper, silver, electrum, gold, platinum, gems, jewelry (two bytes each) |
| 265–272 | Levels: cleric, druid, fighter, paladin, ranger, magic-user, thief, monk |
| 273–280 | The same levels before the change of class |
| 281–283 | Gender, type, alignment |
| 284, 285 | Attacks per two rounds, first and second attack |
| 286–291 | Their dice: two counts, two sides, two modifiers |
| 292 | Armor Class, as 60 minus the value |
| 295 | Experience (four bytes) |
| 300 | Hit points rolled |
| 301–315 | Spell slots: cleric, druid and magic-user, levels 1 to 5 |
| 316 | Experience for defeating it (two bytes), then 318 experience per hit point |
| 319–324 | Portrait head and body, combat icon head and body, order, icon size |
| 325–330 | Combat icon colors |
| 332 | Number of items |
| 389–392 | Hands in use, saving throw bonus, encumbrance (two bytes) |
| 405–408 | Status, in play, hostile, quick fight |
| 409–421 | Current THAC0, front and rear Armor Class, attacks, their dice, hit points and movement |

The other bytes hold pointers and the running game's bookkeeping, which the
files leave meaningless. `curse::decode_creature` reads the record into the
fields *Pool of Radiance*'s records fill (`por::CharacterRecord`), with the
current abilities, the levels before a change of class and the longer spell
lists besides.

## Sound effects

The PC speaker's driver is linked into `START.EXE` as in *Pool of Radiance*, but
at a different place, so OpenGoldBox finds it by its first bytes (a tick divider
of 13 and a coded error message). Its addresses count from the start of the
code segment it is linked into, which its own instructions name: the voice
blocks lie 56h bytes into the driver, and the song table 332h bytes after them.
The table holds 21 songs of four voices; sound command *n* plays song *n* − 1.
The sequencer and its commands are *Pool of Radiance*'s, and so are the first
twelve sounds, tick for tick.

## The opening

A new game starts on disk 2: the original runs script 1 of `ECL2.DAX`, and
only its load entry. That entry loads map `GEO2:1` with the wall banks 1, 2
and 3, puts the party on square (7,13) facing east, says the party wakes in a
small room, shows picture 10 of `PIC2.DAX` (the arm marked with the azure
bonds) and redraws the view (CALL 2E10h). The script's other entries run when
the party first steps. Before it runs, the original's script setup marks a 3D
map as shown and no portrait up (cells 4BE6h and 7EE1h).

`load_campaign(folder, GoldBoxGame::curse_of_the_azure_bonds)` plays it with
the engine *Pool of Radiance* uses: the shared area loader reads the map, its
walls, sprites and pictures, and the session binds the script memory where
*Curse* keeps it. *Curse*'s areas are listed in `campaign_session.cpp`; only the
opening room so far.

## Areas and walls

A script's number is the same on every disk: scripts 1 to 4 are on disk 2,
16 to 21 on disk 3, 32 to 37 on disk 4, 48 to 53 on disk 5, 64 to 69 on disk 6
and 80 to 82 on disk 1. Map numbers are unique too. A script loads maps of its
own disk with LOAD FILES, and the view's three wall slots with LOAD PIECES,
whatever the map: the same map can show other walls for another script (map 1
takes banks 1, 2 and 3 for script 1, and 1, 2 and 4 for script 2).
`campaign_session.cpp` reads every script's LOAD FILES and LOAD PIECES, loads
each map it names and every wall set, and LOAD PIECES switches between them.

## The ending

When Tyranthraxus falls the script runs PROGRAM 8. *Curse*'s WinGame tells
its ending in six screens of four lines, Pascal strings in GAME.OVR (at
265C6h in the release OpenGoldBox reads, 272,137 bytes); OpenGoldBox reads
them from the player's copy and tells them, followed by "You have won.".
Another release shows only that line.
