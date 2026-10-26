# Pool of Radiance areas: outdoor or indoor battles (for Plant Growth's Overgrowth)

Survey of 2026-10-14 for [Plant Growth](LEVELS-5-10-INVENTORY.md)'s Overgrowth, which needs ordinary plants.

## Strongest evidence: the original's own per-square interior flag

Every GEO map square stores an "interior" bit, the high bit of its event byte.
The ImHex Gold Box GEO pattern names it `IsInterior`; see `docs/MAPS.md`, "High
bit: evidence for an indoor flag". OpenGoldBox already reads it as
`MapCell::event_high_bit()` (`src/OpenGold.Formats/include/opengold/geo_map.h`).
`src/OpenGold.Core/src/exploration_view.cpp` uses it to paint a gray ceiling
(interior) or a cyan sky (exterior) in the 3D view. So the original's designers
marked, square by square, which squares have a roof and which have open sky.

I exported every map with the existing tool (`build-game/opengold_maps
$OPENGOLD_GAME_DIR`) and counted the flag. I also counted it over the
walkable region (a flood fill across open edges and doors), because unused
"rock" around dungeons is often left unflagged. Map-to-script links come from
`build-game/opengold_scripts --inspect` (`LOAD FILES`, `LOAD PIECES`, text).

**Implication:** an area-level table is a summary only. Most city blocks are
MIXED: streets are open and buildings are roofed. The exact per-square answer
is already in the data. A rule like "Overgrowth works if the party's square,
or the target square's source exploration cell, is not interior" would follow
the original exactly, without a hand-made table. That design choice is yours.
A combat arena covers 13 x 5 exploration cells around the party
(`docs/combat-geometry.md`), so one fight can include both kinds of square.

Identifiers: `GEOn.DAX:r` is the map and `ECLn.DAX:r` its script. Area names are
Gold Box Explorer reference labels (`demos/godot/scripts/por_map_names.gd`),
except where script text below confirms or corrects them. Cluebook pages are
the book's printed page numbers.

## Table

"Interior squares" = flagged squares out of the walkable region (out of all 256).

| Area | Game identifier | Verdict | Evidence | Confidence |
| --- | --- | --- | --- | --- |
| Civilized Area, New Phlan | GEO3:0, ECL3:0 (City Hall ECL3:8, training hall ECL3:11 are interiors run on the same map) | MIXED: streets, docks and City Park (#11) outdoor; shops, inns, taverns, temples, City Hall indoor | 82 interior of 223 walkable. Cluebook p3 lists a city park. Wall art includes ivy-covered walls (`docs/phlan-wall-art.md`, IDs 4-5). Few fights here: tavern brawls and duels (indoor), guards (either) | High |
| Slums | GEO2:20, ECL2:20 | MIXED: north/middle streets outdoor; buildings, plus all of rows 11-15 (Old Rope Guild) indoor | 157/256 interior. Cluebook p5: random encounters wander the streets; the rope guild is an interior with tougher encounters. Journal (PDF p6): Old Phlan outside the stockade is wild ruins | High |
| Kuto's Well (plaza) | GEO8:29, ECL8:29 | MOSTLY OUTDOOR: plaza and well outdoor; surrounding buildings indoor | 61/256 interior. Cluebook p9: neutral watering zone around the well; kobold guards in nearby buildings | High |
| Kuto's Well Catacombs | GEO8:32 (via ECL8:29 `LOAD FILES 32`) | INDOOR | Walkable catacomb region 80 of 95 interior. The 137 unflagged squares are a sealed outer void with no access. Cluebook p9: catacombs under the well | High |
| Sokal Keep (Thorn Island) | GEO4:21, ECL4:21 | MIXED, mostly outdoor: courtyards and grounds open; a roofed block in rows 2-4 plus a few buildings | 72/256 interior. Cluebook p7: a haunted fortress with an "inner keep", rooms (fungus, frogs, armory, chapel). Several "rooms" sit on open-sky squares, so the keep seems ruined or roofless there | Medium |
| Mendor's Library | GEO2:15, ECL2:15 | MIXED: library interior indoor; surrounding street ring and the walled Garden (SW block, about x0-6, y9-13) outdoor | 127/256 interior; the SW garden block is unflagged. Cluebook p11: the Garden (#9) has dangerous denizens and is the one place you cannot rest | High (garden location inferred from the flag pattern + cluebook) |
| Podol Plaza | GEO1:18, ECL1:18 | MIXED: open plaza (auction, most random encounters) outdoor; temples and buildings indoor | 120/256 interior. Cluebook p13: hug the walls and stay out of the plaza to avoid encounters; the Ilmater temple is behind doors. Journal (PDF p6): center of the old trading district | High |
| Cadorna Textile House | GEO4:2, ECL4:2 | MIXED, roughly 40% indoor: textile-house buildings and hobgoblin barracks indoor; yards and the dry well outdoor | 104 of 252 walkable interior. Cluebook p15 | Medium-high |
| Wealthy Area | GEO1:31 (script ECL1:24 loads both 24 and 31) | MIXED: streets outdoor (wandering patrols); mansions and manors indoor (guard fights #1, #4, #5) | 168/256 interior. Cluebook p17. Journal (PDF p6): wealthy old noble houses | High |
| Temple of Bane | GEO1:24, ECL1:24 | MIXED, mostly indoor: the temple itself indoor; an outer ring/courtyard outdoor | 166/256 interior. Cluebook p17: the blind orc at the entrance; Mace's attack inside | High |
| Kovel Mansion | GEO3:14, ECL3:14 | MOSTLY INDOOR: the mansion indoor (all set fights are ambushes in rooms); a thin ring of grounds outdoor | 170/256 interior. Cluebook p19 | High |
| Valhingen Graveyard | GEO4:10, ECL4:10 | MOSTLY OUTDOOR: graves and grounds open; mausoleums, towers and crypts indoor | 49/256 interior. Cluebook p41: tombstones key; wandering undead outside; many set fights inside mausoleums/towers (indoor) | High |
| Stojanow Gate | GEO2:9, ECL2:9 | MIXED: approach on the earthen dam and the north side outdoor; gate wall band and towers indoor | 111/256 interior. Cluebook p43: the gate sits on an earthen dam; bugbears patrol in front; ettins in the towers | High |
| Valjevo Castle, NW / NE / SE / SW | GEO5:3, 5:4, 5:5, 5:6 (ECL5:3-6) | MIXED, mostly outdoor: outer ring of buildings (barracks, armory, smithy, etc.) indoor; yards, castle wall line and the HEDGE MAZE outdoor | 57-77 interior per map. Squares inside the castle wall carry a dense grid of hedge walls (IDs 7/8/9) and none is interior. Cluebook p45: inside the wall is a maze of poisonous hedges; p49 key shows HEDGE | High. Note: the maze walls are hedges, i.e. plants |
| Valjevo Castle, Inner Tower | GEO5:7, ECL5:7 | INDOOR (one small unflagged 4x4 room, x1-4/y8-11, purpose unknown) | Tower levels interior; cluebook p51: two-level tower, throne room, Tyranthraxus' lair. Final battle is indoor | Medium-high |
| Kobold Caves (incl. small and large cave) | GEO8:13, ECL8:13 | INDOOR | 256/256 interior. Cluebook p29: narrow caves with low ceilings that hamper combat | High |
| Lizard Man Keep (surface) | GEO8:16, ECL8:16 | OUTDOOR (whole map) | 0/256 interior. Cluebook p35: a ruined keep in a forest patch in the swamp; fights "on the surface (in the swampy ground)"; pools. Its script also shows a ruined castle in quicksand reached by a drawbridge. **Caveat:** the cluebook says a field over the keep restricts spellcasting | High |
| Lizard Man Catacombs | GEO8:30 (via ECL8:16 `LOAD FILES 30`) | INDOOR | Main region interior; cluebook p35: soggy floors, low ceilings. Some unflagged squares at the map edges/hole shafts | High |
| Nomad Camp | GEO7:17, ECL7:17 | OUTDOOR (huts/tents, about 13 squares, indoor) | 13/256 interior. Cluebook p27 key: stream, trees; the Forest (#3); the kobold army battle is outdoor | High |
| Buccaneer Base | GEO6:1, ECL6:1 | MOSTLY OUTDOOR: compound, animal pen, merchant camp open; barracks, guard towers, captain's quarters indoor | 47/256 interior. Cluebook p37 | High |
| Outpost of Zhentil Keep | GEO6:28, ECL6:28 | MIXED: central courtyard outdoor (the escape run to the gate); towers, barracks, quarters, stables indoor | 105/256 interior. Cluebook p39 | High |
| Sorcerer's Island: pyramid levels 1-3 | GEO7:22 (L1), GEO7:23 (L2-3); ECL7:22/23 | INDOOR | 256/256 interior on both. Cluebook p31-34: a pyramid, a teleporter maze, Yarash's lab | High |
| Sorcerer's Island surface | no map; Central Wilderness events in ECL7:26 (rowboat in the reeds, pyramid's secret door) | OUTDOOR (overland) | Script text; cluebook p23: island in Lake Kuto | Medium (any fight there is an overland one) |
| Central Wilderness grove and ruined huts (GBE "Unknown Zone") | GEO7:26, loaded by the Central Wilderness script ECL7:26 | OUTDOOR (a few hut squares indoor) | 7/256 interior. Script text: a small wooded grove, a pool, walking through trees, ruined lizard-man huts | High |
| Western Wilderness cave lairs (GBE "Unknown Lair") | GEO6:25, loaded by the Western Wilderness script ECL6:25 | INDOOR (caves) | 255/256 interior. Script text: small dark caves with boars, an anhkheg, thri-kreen, a giant snake | High |
| Eastern Wilderness cave lairs (GBE "Unknown Lair") | GEO8:27, loaded by the Eastern Wilderness script ECL8:27 | INDOOR (caves) | 256/256 interior. Script text: dark caves with a wyvern, giant lizards and minotaurs, gnolls and trolls | High |
| Silver Dragon's Den | ECL6:19 (no map of its own) | INDOOR (cave) | Cluebook pp21-22: there is no map; you talk to the dragon. Script: a cave in the mountains with two levels. A fight is unlikely (he flies off) | Medium |
| Overland wilderness: Western, Central, Eastern, including random encounters, kobold patrols, the lizard-man approach | Scripts ECL6:25 (W), ECL7:26 (C), ECL8:27 (E); wilderness travel view, not a GEO block; combat tiles in WILDCOM.DAX | OUTDOOR | Manual p10: the wilderness has its own view, separate from 3D town/underground. Cluebook pp21-25 terrain keys: mountains, hills, forest, swamp, water, river, deathgrass (W), scoured lands (C). Cluebook p57 contrasts fights "inside a building" with "an open field" with trees. Journal (PDF p8) mentions grassy plains west of Phlan | High for outdoor. Medium for plants everywhere: see notes |

## Notes and uncertainties

1. **Scoured Lands (Central Wilderness).** Cluebook p31: Yarash's poison killed
   the Stojanow/Barren River valley, the Scoured Lands. After he is defeated, the
   valley turns green again (the ECL7:26 text agrees). The original's barren
   ground might not support "normal plants" until that mission is done. A
   possible ruling, not something the original decides.
2. **Deathgrass (Western Wilderness key).** Its meaning is unclear: plant hazard
   or terrain. It is plant life, but perhaps not "normal".
3. **Wilderness battlefield terrain.** I could not confirm whether the
   original's random-encounter arena changes with the overland terrain type
   (swamp, mountain, water). There is no decoder for `WILDCOM.DAX` and no repo
   doc for the wilderness generator (`docs/combat-geometry.md` says it is
   separate, unimplemented work). Treat every overland fight as outdoor.
4. **What the flag means in combat.** The ImHex note says the bit also affects
   combat tile selection. OpenGoldBox's dungeon arena (`DUNGCOM`) does not yet use
   it, and its effect in combat is not verified against the original.
   Roof/sky in the 3D view is verified in our renderer.
5. **Sokal Keep and parts of the Slums** have cluebook "rooms" on open-sky
   squares. The flag treats them as roofless ruins. Trust the flag over the
   word "room".
6. **Mendor's Library garden**: placing it in the unflagged SW block is my
   inference. The garden is the only open area inside the block.
7. **Valjevo hedge maze**: the walls themselves are hedges. That may matter if
   Plant Growth or similar spells interact with the plants that form walls.
8. **Lizard Man Keep spell-restriction field** (cluebook p35) may limit
   casting there at all. Check before relying on Overgrowth in that area.
9. GBE area names for GEO6:25, GEO7:26 and GEO8:27 were placeholders. The table
   corrects them from script text.

## Reproduce

Scratchpad scripts (data analysis only, not repo tools):
`indoor.py maps.json [grid]` (counts and grids), `reach.py maps.json` (walkable
components), `wid.py maps.json GEOn.DAX:r` (flag and wall IDs per square), after
`build-game/opengold_maps $OPENGOLD_GAME_DIR > maps.json`.
