# Original dungeon combat geometry

Target: the locally supplied Pool of Radiance PC 1.3 executable. The observed
Steam `GAME.OVR` SHA-1 is `6f28ec7a62f5cfed1b40771c086c1834ebe35b15`.

Stephen S. Lee's [PC technical analysis](https://gamefaqs.gamespot.com/pc/564785-pool-of-radiance/faqs/73869)
identifies the 50 by 25 arena, dungeon tile artwork, the tile-property table,
the map accessors, and the combat setup overlay. It does not specify the full
dungeon geometry construction. The details below come from controlled local
execution of that original routine with synthetic map inputs.

## Observation method

The ignored `user-data/probe_battlefield.py` runs only the original dungeon
generator and original GEO accessors in a research-only CPU emulator. Its inputs
are synthetic 16 by 16 maps. It stops before random floor decoration, records
the arena, and can record the tiles contributed by one exploration cell.
No original instruction bytes, game maps or emulator are shipped with OpenGoldBox.
The native implementation is written from these geometric observations.

The probe tested all 27 combinations of open passage, solid wall and doorway
on the west, north and east edges, plus each original barrier code separately.
The resulting tile patterns establish these rules:

- The arena is north-up, 50 columns by 25 rows. Facing changes encounter
  placement, not the geometry.
- An exploration-cell offset `(dx,dy)` has tactical origin
  `(21 + 6*dx + 5*dy, 10 + 5*dy)`. Adjacent exploration rows are sheared.
- Cells from `dx=-6..6`, `dy=-2..2` contribute their floor and north/west walls,
  in row order. East edges select the north wall's right corner.
- Corner junctions also inspect west/east edges in the row above and north
  edges in the neighboring columns. An additional 162 local-corner probes
  distinguish continuing walls, wall ends, door posts and passable bases.
- North walls span six tiles; doorways leave a two-tile opening. West walls
  descend diagonally; their doorway leaves an opening in that diagonal.
- Each edge combines both neighboring cells. A doorway takes precedence over
  a solid wall when the two records disagree. Any nonzero original door code
  produces the doorway geometry; exploration lock handling is separate.
- Outside the 16 by 16 GEO, boundaries are solid except east/west edges on
  the party's current exploration row, which permit departure from the arena.
- Tile indices distinguish blocking wall faces from passable wall bases/shadows.
  Treating every non-floor artwork tile as solid would incorrectly narrow doors.

The deterministic floor tile is DUNGCOM tile 22. Original random floor decoration
uses passable tiles, so omitting that decoration preserves collision geometry.
Wilderness generation and original creature-placement/morale rules are separate
compatibility work; this geometry implementation does not claim those systems.

## Evidence limit

The arena geometry above was measured by running the original routine. Where
the party and the monsters start ([PLACE-1](SRD-DECISIONS.md#place-1-2026-10-10-starting-positions-as-the-original-game-had-them))
and how far ahead the party sees (the encounter distance, stopping at a wall or
door) were read from the coab reimplementation and START.EXE's placement
tables, not measured in a running fight. No battlefield has been compared with
a fight in the running original. The user accepted this evidence on 2026-10-24
for closing issue #3; records from the running original are added as the
[DOS probe](../dev_tools/docs/dos-probe.md) makes them.

## Validation and inspection

The native suite compares 35 complete-arena hashes obtained from original
execution on synthetic inputs, then checks collision behavior at a wall and door.
A separate installed-data comparison matched all 1,250 tile IDs at 13 Slums
positions, including all six cells carrying event 1 and all four map corners.

The existing native map inspector can emit the generated tile IDs and collision
grid, for example from PowerShell after setting the installation environment:

```powershell
.\build\opengold_maps.exe $env:OPENGOLD_GAME_DIR --battlefield GEO2.DAX 20 12 1
```

The shared campaign uses this geometry when the Slums script requests a supported
fight. See [the expedition adapter](EXPEDITION.md) for formation, creature and
reward conversion boundaries.
