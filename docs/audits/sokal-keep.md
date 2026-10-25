# Sokal Keep audit (2026-10-24)

What playing Sokal Keep needs, from the user's DOS installation and the
current engine. Sources: `opengold_scripts --inspect GAME ECL4.DAX 21` (and New
Phlan's `ECL3.DAX 0` for the harbor), `opengold_creatures GAME`,
`opengold_maps GAME`, the item lists `ITEM4.DAX` 19 and 21, coab's
`CMD_EncounterMenu` and `CMD_Protection`, the cluebook's Sokal Keep page
(reference only) and SRD 5.2.1.

## The area

- **Map:** `GEO4.DAX:21`, the haunted keep on Thorn Island. One script,
  `ECL4.DAX:21`: `LOAD FILES 21,2,255` (`0x9A7C`) and wall banks `LOAD PIECES
  1,5,9`. The boat lands the party at (8,14) facing north (`0x9A92`, once:
  `0x4A02`).
- **The way there:** New Phlan's harbor master stands where the dock meets
  the pier, met stepping north onto (11,1) (`ECL3:0`, `0x9C43`). While the keep
  is haunted (`0x4AA7` < 254) the only boat goes to Sokal Keep, free
  (`0x4A01` = 1); later he sells passage east, west, to the bay or to the keep
  for 1 platinum piece. The boat leaves from the pier's end at (15,1)
  (`0x9BA6`, picture 41): `NEW ECL 21`, bank 4.
- **The way back:** stepping off the island's edge (`0x9918`): "Do you want to
  take a boat back to Phlan?" Yes: New Phlan at (15,1) facing west.
- **Events** (the event byte's low seven bits index the jump table at
  `0x99E6`):
  - **1** (6,13): a dead elf. Searching (Look, or the menu's Search) finds a
    scroll of three words in elvish runes (`PROTECTION`), LUX, SAMOSUD and
    SHESTNI, "the last part eaten away". Attacked, the body is hacked apart
    (`0x4A00` = 255).
  - **2** (7,13), (8,13): the gates; footsteps within, reeds to the west.
  - **3** (48 squares, the courtyards): an undead patrol, one chance in six
    each step while fewer than four have been beaten (`0x4A03`): skeletons
    and zombies. With the password known (`0x4A19`) they ask for it; else the
    encounter menu, and a parley asks for a word: SHESTNI before Ferran is
    freed, SAMOSUD after. The right word sends them off; a wrong one or a
    fight brings `LOAD MONSTER` skeletons × 6 and zombies × 6.
  - **5, 6, 9, 10, 11, 16** text: the stable's mold, a fungus room, the
    smithy, the armory, the chapel's husks, the hall after the raid.
  - **6** (the smithy): on the third visit, frogs (`LOAD MONSTER 38` × 4).
  - **15** (15,6): searching the smithy finds an old hammer, `TREASURE` list
    19 (Hammer +1, `0x4A28`).
  - **7** (the barracks): on the fourth visit, haunts rise; parleyed, they ask
    for a word: LUX tells of the diary (journal entry 3) and 5 gems under a
    floorboard (`0x4A27`, `0x4A29`).
  - **8** (2,11): insects (`LOAD MONSTER 39` × 2, the Huge Scorpion).
  - **9** (the hall): the orc and hobgoblin raid, `LOAD MONSTER` hobgoblins ×
    15, orc leaders × 4, orcs × 31 (morale 50); a note becomes journal entry
    57 (`0x4AD7`).
  - **13, 14, 17** (behind the armory's illusory wall): a glow and stands
    holding `TREASURE` list 21: Mace +2, Chain Mail +1, Long Sword +1, Shield
    +1 (`0x4A21`).
  - **12** (7,10), (8,10): Ferran Martinez's pale form (encounter menu with
    the Spectre's sprite and his own picture 19). Parleyed, he asks for a
    word: LUX, and he asks whether the city is free. The truth: his tale, and
    the spirits are freed (`0x4A26` = 255, the council's flag `0x4AA7` =
    254, the password reset for SAMOSUD). A lie or a wrong word and he fades
    for good; fought (`LOAD MONSTER 19`), his death frees the keep too.
- **City Hall** pays its reward for the keep from `0x4AA7` = 254, and the
  harbor opens its other routes.

## Fights

| Fight | `LOAD MONSTER` (MON4CHA record × count) |
| --- | --- |
| Frogs (smithy) | Poisonous Frog (38) × 4 |
| Insects | Huge Scorpion (39) × 2 |
| Undead patrol | Skeleton (34) × 6, Zombie (35) × 6 |
| The raid | Hobgoblin (6) × 15, Orc Leader (5) × 4, Orc (4) × 31 |
| Ferran Martinez | Ferran Martinez (19) × 1 |

## Monster conversions

MON4CHA 4-6 are the Slums' MON2CHA 4-6; 34, 35, 38 and 39 have the Cadorna
Textile House's conversions. Ferran Martinez (MON4CHA 19) has the Spectre's
record exactly (HP 38, AC 2, level drain, magic weapons to hit), so he is the
SRD Specter with 38 Hit Points, as Mendor's Spectre is.

## Engine support

Missing before this audit, now done:

1. **The area row** and its conversions; New Phlan's boat already ran
   `NEW ECL 21`.
2. **The boat's picture** (PIC3 41) is an animation; the town did not load
   PIC3's animations.
3. **`LOAD FILES 255,16,255` and `255,2,255`**, the rune glyphs and the keep's
   own art again around the scroll, keep the current map.
4. **`PROTECTION`**: the rune line at its operand. The glyphs are letters (A
   at tile 0x40, two tiles a letter, S on from 0x67); they are shown
   translated, and kept above the text the next `PRINTCLEAR` writes, until the
   next pause.
5. **The encounter menu's sprite** was read from its third operand (the
   picture); coab reads it from the first. Only Ferran's menu has them differ.

## Party level and balance

The council sends new adventurers here early; a level-3 party clears it at the
default encounter challenge. The raid is trimmed to the party's budget.

## Status

- **Increment 1 (done, 2026-10-24):** all of the above, by
  [SOKAL-1](../SRD-DECISIONS.md#sokal-1-2026-10-24-sokal-keep). Played
  (`opengold_expedition_tests`, a new level-3 party from New Phlan): the
  harbor master's passage and the boat, a save on the island, the scroll's
  words, the patrol sent off with SHESTNI, the haunts' gems for LUX, the
  frogs and the Hammer +1, the insects, the raid, the arms behind the wall,
  Ferran freed with LUX and the truth, a patrol passed with SAMOSUD, and the
  boat home.
- **The whole map (2026-10-25):** played by one party from Mendor's Library,
  earning its commissions at City Hall ([the whole map](whole-map.md)).
