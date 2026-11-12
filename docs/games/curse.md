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
| `HEAD*`, `BODY*`, `PIC*` | Portraits and close-ups | None |
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

What these commands do is for step 3 of the plan, when the scripts run.

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
exception is the current character's record at the start of zone 1: it follows
the 422-byte layout below, not a shift.

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
files leave meaningless.

## Sound effects

The PC speaker's driver is linked into `START.EXE` as in *Pool of Radiance*, but
at a different place, so OpenGoldBox finds it by its first bytes (a tick divider
of 13 and a coded error message). Its addresses count from the start of the
code segment it is linked into, which its own instructions name: the voice
blocks lie 56h bytes into the driver, and the song table 332h bytes after them.
The table holds 21 songs of four voices; sound command *n* plays song *n* − 1.
The sequencer and its commands are *Pool of Radiance*'s, and so are the first
twelve sounds, tick for tick.
