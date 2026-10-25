# Temple of Bane audit (2026-10-24)

What playing the Temple of Bane needs, from the user's DOS installation and
the current engine. Sources: `opengold_scripts --inspect GAME ECL1.DAX 24`,
New Phlan's `ECL3.DAX 0` (the bishop) and City Hall's `ECL3.DAX 8` (the
summons), `opengold_creatures GAME`, `opengold_maps GAME`, coab's
`CMD_LoadFiles` and the player record's `control_morale`, and SRD 5.2.1.

## The area

- **Maps:** one script, `ECL1.DAX:24`, with two maps: `GEO1.DAX:31`, the
  street east of the temple with the orcs' mansion, and `GEO1.DAX:24`, the
  temple's block. Both use wall banks `LOAD PIECES 1,3,5` (`0xB6A3`). The
  entry loads map 24 if it was the last map loaded, else map 31; the script
  tells them apart by the cell the original's `LOAD FILES` keeps the loaded
  map's number in (`0x49C5`).
- **The way in:** the wilderness map's eastern city edge (13,27) offers south,
  which leads to map 31 at (15,4) facing west; Kovel Mansion's south edge
  leads here too.
- **The way out** (the step entry `0x9914`, by the table at `0x99A8`): map 31's
  north edge to Kovel Mansion, its east edge to the wilderness, its west edge
  to map 24; map 24's east edge back to map 31.
- **Camps:** until the half-orc cleric and the mansion's orcs are beaten, a
  camp may be broken up by ten orcs.
- **The quest:** City Hall summons the party to Tyr's bishop once its earlier
  commissions are done (`0x4A9B` = 254); the bishop introduces Dirten, who
  asks the party along (`ADD NPC 107`, `0x4A9C` = 255).
- **Map 31's events** (the jump table at `0x99D2`):
  - **5** (10,9), (10,10): the mansion's door. The first time, a large group
    of orcs attacks; with a holy symbol of Bane (item type 74) they are
    surprised, and the encounter menu may avoid the fight (`0x4AA8` = 254).
  - **1, 2** (four squares): rubble with a tapestry (item list 4), a burned
    dining room with jewelry on a skeleton.
  - **3, 4** (six squares): the great hall, whose trap door a search may find
    (item list 6), and the room of bodies with jewelry.
  - **6** (6,8): six orcs and an ogre; a paper for journal entry 53.
  - **7** (14,8): goblin slaves. Let go, one says a holy symbol of Bane opens
    the temple.
  - **8** (8,11): the mansion's walls, painted with black hands.
  - **9** (8,12): eight orc guards.
  - **10** (10,14), (9,15): hay beds; searching them brings eight orcs.
  - **11, 12**: a cell, and a scratched message.
- **Map 24's events** (the jump table at `0xA3DA`):
  - **4** (2,7), (2,8): the temple's doors, an old blind orc and eight guards.
    He feels the party for a holy symbol of Bane: with one he lets them in,
    without one (or fought) the guards attack (`0x4A0F` = 1). Leaving the
    temple afterwards brings the horde (below).
  - **1, 2, 3** (5,5), (9,12), (12,9): trapdoors, found by searching (each
    member rolls d6 under the search flag, plus one for an elf or half-elf)
    and opened: item lists 8, 9 and 17. With all three found, the horde
    charges in.
  - **7** (14,7), (14,8): the altar. Destroyed, it brings the horde; after the
    horde, the unease lifts.
- **The horde:** five orc leaders, the half-orc cleric, twenty orc archers and
  thirty orcs. Beaten, the cleric carries a paper (journal entry 25), the
  temple is free (`0x4ABD` = 254), and Dirten thanks the party and stays: the
  script blanks his record and stores it.
- **Random groups** (`0xAD50`): goblins, orcs, hobgoblins or ogres with
  leaders, by the party's strength, or a patrol of four orcs that may let a
  harmless-looking party pass.

## Fights

| Fight | `LOAD MONSTER` (MON1CHA record × count) |
| --- | --- |
| The mansion | Orc (4) × 10 or 20 |
| Six orcs and an ogre | Orc (4) × 6, Ogre (8) × 1 |
| The orc guards | Orc (4) × 6, Orc Leader (5) × 2 |
| The beds | Orc (4) × 8 |
| The temple's guards | Orc (4) × 8 |
| The horde | Orc Leader (5) × 5, half-orc cleric (33) × 1, orc archer (44) × 20, Orc (4) × 30 |
| Camps | Orc (4) × 10 |
| Random groups | Goblins (2, 3), orcs (4, 5), hobgoblins (6, 7), ogres (8, 9) |

## Monster conversions

MON1CHA 2-8 are Podol Plaza's conversions.

| Record | Original | Conversion |
| --- | --- | --- |
| 9 | Ogre Leader (HP 32) | the Ogre, a leader's double XP |
| 33 | the half-orc cleric (level 5, HP 25, Mace +1, chain, shield) | **new:** SRD 5.2.1 Priest with 25 Hit Points, as Grishnak |
| 44 | Orc archer (HP 5, short bow) | **new:** the Orc (SRD 5.1) shooting a Shortbow |

## The companion

Dirten (MON3CHA 107): a level-5 cleric with Strength 10, Intelligence 15,
Wisdom 16, Dexterity 10, Constitution 9 and Charisma 15, carrying a wooden
holy symbol, a mace and chain armor. He joins as a level-5 Cleric with those
scores (an Acolyte), his Mace and a Chain Shirt for his chain armor, both
worn.

## Engine support

Missing before this audit, now done:

1. **The area row** with both maps and the conversions.
2. **The loaded map's number** (`0x49C5`), which `LOAD FILES` now keeps.
3. **Dirten** (`ADD NPC 107`), with his gear put on as he joins.
4. **A companion's control byte** (`0xB8`, high bit for a companion) and
   **leaving**: a script that blanks a companion's name and status and stores
   the record sends the companion away.
5. **Treasure from one instruction with several lists**: each list is awarded
   once.

## Party level and balance

The council sends the party here after Sokal Keep and Podol Plaza; a level-5
party with Dirten clears it at the default encounter challenge, the horde
trimmed to the party's budget.

## Status

- **Increment 1 (done, 2026-10-24):** all of the above, by
  [BANE-1](../SRD-DECISIONS.md#bane-1-2026-10-24-the-temple-of-bane). Played
  (`opengold_expedition_tests`, a new level-5 party): City Hall's summons,
  Dirten joining at the bishop's, the bay's boat and the city's eastern edge,
  a save in the street, the mansion's orcs, the six orcs and their ogre, the
  goblin slaves let go, the temple's guards, the three trapdoors found by
  searching, the horde and its cleric, and Dirten staying behind.
- **The whole map (2026-10-25):** played by one party from Mendor's Library,
  earning its commissions at City Hall ([the whole map](whole-map.md)). Leaving
  the block east onto the wilderness now loads the wilderness script's map
  before handing over to it.
