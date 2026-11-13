# Secret of the Silver Blades: how its files differ

*Secret of the Silver Blades* (1990) runs on the engine *Curse of the Azure
Bonds* grew. This page records what its files change, as the census
(`opengold_census`, [GAMES.md](../GAMES.md)) reads them. The facts come from the
reconstructed source of the DOS release 1.3 and from the files themselves; the
wording is our own.

Since 2026-11-13 every file decodes but `SOUND.DAX`, the title music (below).

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
| `SOUND.DAX` | The title music: a driver program (1) and its song (2) | New file |

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

CALL's engine routines moved: the view's redraw is 2DCBh and the sound B203h
(*Curse*'s addresses less 7FFFh are compared, as there), the wall ahead and the
step forward keep *Curse*'s, and the duels are gone.

## Creatures

The record grew from 422 to 439 bytes. A character now knows up to 117 spells,
holds 75 memorized ones and has spell slots for seven spell levels; there are
seven classes (no monks), and type flags take the place of the type and
undead level. Byte offsets:

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
| 242–250 | Thief skills |
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
some of its pieces from slot 3. The census decodes each set with the slots it
does not fill left blank; to draw *Secret*'s walls as the original does, the
loader must put the three slots' tiles together first.

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

## The title music

`SOUND.DAX` holds the music of the introduction: a driver program of its own
(record 1) and the song it plays (record 2). No reconstructed source covers
that driver, so OpenGoldBox does not read it yet; the introduction plays
without music.
