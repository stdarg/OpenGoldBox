# Mendor's Library audit (2026-10-24)

What playing Mendor's Library needs, from the user's DOS installation and the
current engine. Sources: `opengold_scripts --inspect GAME ECL2.DAX 15` (and
City Hall's `ECL3.DAX 8`), `opengold_creatures GAME`, `opengold_maps GAME`, the
item lists `ITEM2.DAX` 1, 2, 3 and 5 decoded with `START.EXE`'s item words,
coab's `CMD_Damage` and search flags (`engine/ovr003.cs`, `engine/ovr015.cs`),
and SRD 5.2.1.

## The area

- **Map:** `GEO2.DAX:15`, a 16 x 16 block east of the Cadorna Textile House.
  One script, `ECL2.DAX:15`, drives it: `LOAD FILES 15,2,255` (`0x9BA0`) and
  wall banks `LOAD PIECES 1,19,23` (`0x9BA7`). The library (columns 2-14, rows
  2-13) stands in a yard open on the north and west edges. Its only doors are
  wizard locked: north at (8,2) and south at (9,13). Inside, (8,8)-(8,9) and
  (8,11)'s west door are locked.
- **Events** (low five bits of the event byte, through the tables at `0x9B5B`
  and `0x9B6E`; `0x9B2E` dispatches them):
  - **0** (173 squares): the yard and halls; nothing happens.
  - **1** (24): the weed-filled garden (columns 2-6, rows 9-13).
  - **2-5** (8 each): the stacks, signed Philosophy (columns 12-14, rows 6-8),
    Mathematics (2-4, 6-8), History (10-12, 2-4) and Rhetoric (4-6, 2-4).
  - **6, 7** (9,7), (7,7): rotted furniture; **8** (8,11): bones about a desk;
    **9** and **73** (8,4), (8,2): the frescoes; **10** and **74** (9,10),
    (10,10), (9,13): the writing tables; **11** (11,12): the mad fighter;
    **12** (12,12): the study; **13** (13,13): the study's trash; **14-16**
    (10,9), (8,10), (10,13): the illuminators' carrels; **17** (12 squares):
    small rooms; **18** (12,10): the storeroom.
  - Bit 64 (squares 73 and 74) marks the two doorways the Specter guards.
- **Entries:** the Cadorna Textile House's east edge (`ECL4:2`, `0x9989`) and
  Kuto's Well (`ECL8:29`) lead here. The entry (`0x9B8E`) loads the map unless
  the party came from the library itself.
- **Exits** (the step entry `0x99E9`, by facing at `0x9B09`/`0x9B0D` when the
  step leaves the map): north to Kuto's Well (bank 8, `NEW ECL 29`), west to
  the Cadorna Textile House (bank 4, `NEW ECL 2`); east and south are walled.
  Both are loaded already.
- **Script entries** (`0x9900`): `0x99E9` before each step (exits and the
  Specter), `0x9B11` on arrival (the event dispatch), `0x9B81` before camping
  (sets the camp profile `0x6DD2` = 0, `0x6DD3` = 10), `0x9B8D` (nothing) and
  `0x9B8E` on entering.

### Events

- **The stacks** (events 2-5, `0x9D62`): the first visit reads the sign over
  the door (`0x4A00`-`0x4A03`). Afterwards, with the search flag set (Look),
  the party scans random books:
  - Philosophy (`0x9F07`): 1 in 12 each, Fyerdetha's Discourses on Power
    (journal entry 7, `0x4A2F` bit 1, `0x4AAA` = 254), Urgund's Description of
    Darkness (entry 19, bit 2, `0x4AAB`), or one of five worthless titles
    (bit 128 and `0x4AAF` with the fifth).
  - History (`0xA187`): 1 in 18 each, Lex Geographica (entry 37, bit 4,
    `0x4AAC`), the History of the North (entry 8, bit 8, `0x4AAD`) and the
    Grand Historian's Records (entry 21, bit 16, `0x4AAE`).
  - Mathematics and Rhetoric: nothing useful.
  - The `0x4AAA`-`0x4AAF` flags at 254 are City Hall's: "The council is
    offering a reward for books, maps, tomes" (`ECL3:8`, `0xA927`).
- **The basilisk** (the Rhetoric stacks, `0xA3C3`, until `0x4A31` bit 64): a
  dull brown reptilian monster, met by surprise as the dice fall (four
  openings), then the encounter menu or Combat/Wait/Flee/Approach. Fought:
  `LOAD MONSTER 26` × 1 with `TREASURE` list 3: a Clerical Scroll of 2
  spells, a Cloak of Displacement, a Potion of Healing and a Potion of Giant
  Strength. Won, bit 64.
- **The bones and the jar** (event 8, `0xA5B8`): bones about a desk; looking
  (`0x6DCA` = 1) finds a jar under the floorboards: `TREASURE` list 5, three
  Potions of Extra Healing (`0x4A2F` bit 32).
- **The buried book** (event 13, `0xAB4C`): looking finds a book in the
  trash; "DO YOU TAKE IT?" Yes: `TREASURE` list 1, a Manual of Bodily Health
  (`0x4A2F` bit 64).
- **The illuminators' carrels** (events 14-16, `0xAB9C`): looking has an even
  chance of a box of gold foil, `TREASURE` list 2 (a Sheet of Gold, 3 gp),
  once per carrel.
- **The mad fighter** (event 11, `0xA7A6`, until `0x4A31` bit 1 or 2): a
  fighter with battered armor and wild eyes, cowering. Combat/Wait/Flee/
  Parlay/Ignore:
  - Combat: `LOAD MONSTER 25` × 1; won, bit 1.
  - Wait: he raves (one of four lines about a fiery, inhuman master in a
    "castle of flowers" on a hill), and the menu returns.
  - Ignore: he scrambles away (bit 1).
  - Parlay: the third answer leads on, unless the party is full (`0x6E3E` =
    8): "Do you invite him?", then "Do you insist?" Two yeses: he joins
    (bit 2, `ADD NPC 25,0`). The fifth answer fights; the others rave.
- **The kobolds** (event 18, `0xACB5`, until `0x4A31` bit 128): five kobolds
  at a table and the encounter menu. Parleyed, they beg to be spared; yes,
  and they describe the area (journal entry 10). Fought, `LOAD MONSTER 0` × 5.
  Either way bit 128.
- **The garden** (event 1, `0x9BAF`): on later steps, up to seven times, a 7
  in 26 chance of green slime: seen and avoided, or stepped into (by the
  party's surprise roll): `DAMAGE 160,1,4,0,0`, 1d4 to a random member who
  fails a saving throw (coab: form `0xA0` without bit `0x80` in the fifth
  operand is a random member, save bonus 0, the first save type).
- **The Specter** (the step entry, `0x9A1F`): leaving by either doorway (north
  from (8,2), south from (9,13)) while carrying any of the books (`0x4A2F`
  bits 1, 2, 4, 8, 16 or 128) and before it is destroyed (`0x4A31` bit 32):
  "Thief! I defended these books in life and I will defend them in death!"
  `LOAD MONSTER 17` × 1. Won, bit 32; fled, the party is put back inside.
- **City Hall** pays its clearing reward (slot 10) for the library once the
  Specter and the basilisk are both destroyed (`0x4A31` & 96 = 96,
  `0x9DC6`).
- **Art:** `WALLDEF2`/`8X8D2` (banks 1, 19, 23), `SPRIT2`, `CPIC2`, `MON2CHA`
  and `ITEM2` are present.

## Fights

| Fight | `LOAD MONSTER` (MON2CHA record × count) |
| --- | --- |
| The Specter (`0x9AC9`) | Spectre (17) × 1 |
| The basilisk (`0xA58A`) | Basilisk (26) × 1; `TREASURE` list 3 |
| The mad fighter (`0xAAF3`) | Mad Man (25) × 1 |
| The kobolds (`0xAE8C`) | Kobold (0) × 5 |

## Monster conversions

| Record | Original | Conversion |
| --- | --- | --- |
| MON2CHA 0 | Kobold | `slums-kobold` (the Slums' record 0), 25 XP |
| MON2CHA 17 | Spectre (HP 38, AC 2, 1d8, drains two levels, magic weapons to hit, immune to sleep, charm, hold, paralysis, poison and cold) | **new:** SRD 5.2.1 Specter (CR 1, 200 XP) with the original's 38 Hit Points |
| MON2CHA 26 | Basilisk (HP 31, AC 4, 1d10, petrifying gaze) | **new:** SRD 5.2.1 Basilisk (CR 3, 700 XP) with the original's 31 Hit Points |
| MON2CHA 25 | Mad Man (HP 7, AC 4, Strength 17, 1d6) | **new:** SRD 5.2.1 Guard (CR 1/8, 25 XP) with the original's 7 Hit Points; joining, a level-1 Fighter |

## Engine support

Supported before this audit: the area loader, `ENCOUNTER MENU`, `PARLAY`,
`HORIZONTAL MENU`, `SURPRISE`/`PARTY SURPRISE`, `APPROACH`, `SAVE TABLE`,
`TREASURE` with `ITEMn` lists, the Manual of Bodily Health and Cloak of
Displacement, wizard-locked doors (Knock or a DC 35 Bash), and City Hall's
reward lists.

Missing:

1. **The area row** and its four conversions.
2. **The Specter:** Life Drain on every hit, flight, Incorporeal Movement,
   eight damage Resistances, two Immunities and nine condition Immunities.
3. **The Basilisk's Petrifying Gaze:** a Bonus Action Cone that Restrains
   and then Petrifies, with Recharge 4-6.
4. **The mad fighter joining:** no conversion for MON2CHA 25.
5. **The green slime** (`DAMAGE 160,1,4,0,0`): the random-member form with a
   saving throw threw "Unsupported DAMAGE form".
6. **Look's search flag:** the engine set `0x6DCA` to 2 on Look; coab runs the
   search with the flag at 1, and every script tests only 0 and 1, so the jar
   and the buried book could not be found.
7. **Search mode** (the original's Search toggle, `0x6DCA` = 1 on every step,
   time doubled) is still missing; nothing in the library needs it beyond
   what Look reaches.

## Party level and balance

The library follows the Cadorna Textile House at about level 4-6. The Basilisk
(700 XP) and its gaze are the hardest fight; the Specter's Life Drain wears
down Hit Point maximums until a Long Rest.

## Status

- **Increment 1 (done, 2026-10-24):** the area row (`rolf_tour.cpp`), the
  Specter, Basilisk and Guard stat blocks (rules 0.6.251), condition
  immunities, flight and incorporeal movement, the Petrifying Gaze, the mad
  fighter as a level-1 Fighter (`companions.cpp`), the random-member `DAMAGE`
  form with a saving throw, and Look's search flag at 1, all by
  [MENDOR-1](../SRD-DECISIONS.md#mendor-1-2026-10-24-mendors-library). Played
  (`opengold_expedition_tests`, after the Cadorna Textile House): a save in
  the library, Knock on the north door, a book from the history stacks, the
  basilisk and its Cloak of Displacement, the jar and the Manual of Bodily
  Health found by looking, the kobolds spared, the mad fighter joining, the
  Specter at the door and the way back west. The garden's slime is random;
  its saving throw is checked in `opengold_kutos_well_tests`, the Specter and
  Basilisk in `opengold_slums_creatures_tests`.
