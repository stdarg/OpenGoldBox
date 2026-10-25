# Central Wilderness audit (2026-10-24)

What travelling the wilderness map needs, starting with its central window,
from the user's DOS installation and the current engine. Sources:
`opengold_scripts --inspect GAME ECL7.DAX 26` (and New Phlan's harbor in
`ECL3.DAX 0`), `opengold_creatures GAME`, START.EXE, `SQRPACI.DAX`,
`WILDCOM.DAX`, `RANDCOM.DAX`, `ICON.DAX`, the manual (p. 13: the wilderness
view, eight directions, half a day a square), the cluebook's wilderness maps
(pp. 21-25, reference only), the Pool of Radiance decode at
github.com/DrEvil-TitaniumHelix/pool-of-radiance-remake (its `decode/pseudo`
notes on StepOverland, DrawOverlandWindow, RefreshView,
ecl_RunAfterNewBlock, BuildWildernessField and ecl_op2D_Call, used as facts
about the original, not copied), and SRD 5.2.1.

## The wilderness map

- **The grid** lives in START.EXE's data, not a DAX archive: 36 rows of 44
  bytes, each a SQRPACI tile number (tile n is art n - 1; SQRPACI's two
  records hold tiles 0-127 and 128-255). Three windows of 16 columns look
  onto it, starting at columns 0, 13 and 26: the western (`ECL6:25`), central
  (`ECL7:26`) and eastern (`ECL8:27`) wilderness. A script's x is in its
  window (0-15), y in the grid (0-35).
- **Landmarks:** nine (x, y, tile) entries beside the grid; seven of them
  change with the story: a site found (the Buccaneers' base, the Zhentil
  outpost's approach), the pyramid valley healed.
- **The view:** the 5 x 5 squares around the party, 24 pixels each, the party
  as ICON 0 in the middle except at the map's edges.
- **Travel mode:** after a script's entry, if it is a wilderness script and
  its 3D-area cell `0x49E6` is clear, the party is on the map (modes 2-4);
  otherwise in a 3D area (mode 1). The central script uses `GEO7:26` as a
  3D area for its grove and ruined huts.
- **A step:** the arrow keys set the direction (0 north and on clockwise to
  7) and step; the script's step entry runs first and may cancel it
  (`0x6DC9` = 255) or leave by `NEW ECL`. The engine then moves the party one
  square, kept on the window, unless the square is impassable (the sea and
  some mountain, cliff and city tiles) or a poisoned river while the Pyramid's
  sorcerer lives (`0x4AA2` = 0: "The river's stench drives you back."). A
  step takes half a day. The arrival entry then runs.
- **Engine calls:** the original engine runs only CALL 0x2C90, 0x8000,
  0x8001, 0xBA03, 0xC01E and 0xC018 and does nothing for any other address;
  the wilderness scripts call 0x0806, 0x2C4E, 0x2C51, 0xC003, 0xC009 and
  0xC01B, so those do nothing. Their terrain cell `0x035F` is never set.

## The central window (`ECL7:26`)

- **Entry** (`0xB112`): on the map (`0x49E6` = 0, `LOAD FILES 5,5,0`) unless
  the party is inside the grove or huts (`0x4A9E` = 255: `GEO7:26`, wall banks
  7, 8, 23).
- **Edges** (the step entry `0x9914`): west of column 2 to the western
  wilderness at column 14, east of column 15 to the eastern at column 3
  (except a few rows).
- **Sites** (`0x9AE0`, by the tables at `0xB04A`-`0xB06A`):
  - (12,10), (13,10), (11,11), (13,11), (12,12), (13,12): a recently abandoned
    camp; (12,11): the nomad camp, entered to `ECL7:17`.
  - (12,26), (13,26): the city's north edge: the northern gates (`ECL3:14`,
    Kovel Mansion's block) or the graveyard (`ECL4:10`).
  - (13,27): the city's east edge: north (`ECL3:14`), south (`ECL1:24`, the
    Temple of Bane's block) or the boat to the civilized area (`ECL3:0`).
  - (11,28): the city's west edge: north into Podol Plaza (`ECL1:18`), south
    into the Cadorna Textile House (`ECL4:2`).
  - (6,14): a rowboat in the reeds to the Pyramid; (6,16): the Pyramid's
    secret door (`ECL7:22`).
  - (7,29): a boat back to the civilized area.
- **Roaming groups** (`0xA0E7`): one step in 20, a group by `0xAB15`'s roll:
  kobolds (MON7CHA 123 with a leader per 40 strength), displacer beasts (68),
  driders (69), stirges (61), lizard men (57), bandits (97) with aides (53),
  nomads (40) with aides, merchants (98) with aides, sized by the party's
  strength. Surprise picks one of four openings and the encounter menu;
  bandits, nomads and merchants may be parleyed with (the roll plus the
  spokesman's reaction), merchants and nomads greet a friendly party.
- **The grove or the ruined huts** (one roaming roll in 21): entered, the
  party walks `GEO7:26`: a pool with six lizard men, two displacer beasts,
  four chained kobolds to free or kill, two mutant lizard men, huts of four
  lizard men each, stirges; its edge squares lead back out to the map.
- **New Phlan's harbor:** once Sokal Keep is freed, the harbor master sells
  passage west (the wilderness at (7,29)), to the bay ((13,27)) or east (the
  eastern wilderness at (9,29)).

## Fights on the wilderness map

The original grows the field from the overland tile under the party
(BuildWildernessField): every square open ground, then a road or ridge with
boulders on some terrain, trees by the terrain's density, bushes, rocks and
grass, and rare debris. Each battlefield tile has a movement cost (255
impassable) and an art frame in START.EXE's combat terrain table; the
wilderness frames are WILDCOM's 34 and RANDCOM's 6. The field is drawn north
up.

## Monster conversions (MON7CHA)

| Record | Original | Conversion |
| --- | --- | --- |
| 0, 123 | Kobold | `slums-kobold` |
| 1 | Kobold Leader | `slums-kobold-leader` |
| 57 | Lizardman (HP 11) | `lizardfolk` (SRD 5.1, as in Kuto's Well) |
| 58 | Mutant Lizard Man (HP 18) | **new:** the Lizardfolk with 18 Hit Points |
| 61 | Stirge (HP 5, blood drain) | **new:** SRD 5.2.1 Stirge: Proboscis attaches and drains 2d4 Necrotic a turn |
| 68 | Displacer Beast (HP 30, two 2d4 tentacles) | **new:** not in SRD 5.2.1; the Owlbear's stat block (two Rends) with 30 Hit Points |
| 69 | Drider (HP 36) | **new:** SRD 5.2.1 Drider with 36 Hit Points, Magic of the Spider Queen (Recharge 5-6) |
| 97, 40, 98 | Bandit, Nomad, Merchant (HP 6) | **new:** SRD 5.2.1 Bandit with 6 Hit Points |
| 53 | Aides (HP 18) | **new:** SRD 5.2.1 Bandit Captain with 18 Hit Points, without Pistol and Parry (as Norris) |

## Status

- **Increment 1 (done, 2026-10-24):** the wilderness map from START.EXE and
  SQRPACI, eight-direction travel, the view, saves on the map (campaign
  format 39), the central window's sites, edges and roaming groups, fights on
  wilderness fields, the grove and huts, and the conversions above, by
  [OVERLAND-1](../SRD-DECISIONS.md#overland-1-2026-10-24-the-wilderness-map). The
  sites lead to the areas already in the game (New Phlan, Podol Plaza, the
  Cadorna Textile House, and since 2026-10-24 Kovel Mansion, the Temple of
  Bane, the nomad camp and the Pyramid); the others are
  refused with a notice. Played
  (`opengold_expedition_tests`, a new level-5 party): the western boat from
  the harbor, a save on the map, the sea stopping a step, half a day a step,
  a roaming group fought, the grove or huts entered and left, and the city's
  western edge into the Cadorna Textile House; Podol Plaza's and the textile
  house's west edges lead onto the map and back.
