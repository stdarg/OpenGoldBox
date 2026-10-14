# Tile Art Viewer and Map Browser

This is a standalone Godot tool for browsing Pool of Radiance's original
first person wall artwork and exploration maps. It is a separate Godot project under `dev_tools` and
is not linked into the game. It reads your installed DAX files at runtime; no
original artwork is included in this repository.

The **Art Catalog** tab's contact grid shows every stored perspective piece from `WALLDEF1` through
`WALLDEF8`, including doors, gateways, waterfront/dock art, and other area
variations. Each item identifies its source archive, record, appearance number,
and view. The preview uses the source pixels with nearest neighbor scaling.
Export PNG writes the **original size** piece with its transparent pixels, not
the scaled thumbnail or preview. Phlan (`WALLDEF3.DAX:0`) has descriptive names;
other records keep their source identifiers because appearance numbers are local
to a wall set.

The **Map Browser** tab loads all 29 decoded GEO maps, including New Phlan,
Slums, and Kuto's Well. Pick a map, then use the arrow keys as in exploration:
Left and Right turn, Up moves forward, and Down turns around. Forward movement
obeys wall, door, and map-edge collisions. Click the top-down map to jump to any
cell for inspection. The map shows walls in blue, doors in gold, event bytes as
purple dots, and the current pose in yellow.

The first-person view uses the game's original 88 × 88 perspective compositor.
Click a visible wall or door in that view. The panel below it shows the exact
source piece that supplied that pixel: map cell, face, wall and door codes, event
byte, source archive and record, appearance, perspective, dimensions, and a
preview. Sky and floor pixels have no source wall piece. The **Visible pieces**
control gives keyboard users another way to select one. Export PNG saves that
piece at its source resolution with transparency. Only the clicked or selected
piece appears in the details panel; changing position or facing clears it.

The map's artwork selector shows inspection profiles derived from decoded area
scripts and their `LOAD PIECES` instructions. Some maps inherit a prior wall set
or have unresolved script operands; Lizard Men Keep exposes two observed sets.
The profile label and source are shown in the tool. These profiles help inspect
the original assets; they do not claim to resolve every conditional runtime
script path.

## Build and run on macOS or Linux

From the repository root, with CMake, Ninja, Python, Git and Godot 4.5+ installed:

```bash
cmake -S . -B build/tile-art-viewer -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENGOLD_BUILD_GODOT=ON -DOPENGOLD_BUILD_TESTS=OFF
cmake --build build/tile-art-viewer --target opengold_tile_art_viewer -j6
godot --headless --editor --path dev_tools/tile_art_viewer/godot --import --quit
OPENGOLD_GAME_DIR=/path/to/POOLRAD godot --path dev_tools/tile_art_viewer/godot
```

On this Mac, replace `godot` with
`/Applications/Godot_mono.app/Contents/MacOS/Godot` if it is not on `PATH`.
The import step is needed once per checkout: Godot loads the native extension
only after an import lists it in `.godot/extension_list.cfg`. Without it the
viewer stops with "Could not find base class WallArtCatalog". (The headless
import may crash on exit after writing the list; that is harmless.)
`OPENGOLD_GAME_DIR` must point to the folder containing the `WALLDEF*.DAX` and
`8X8D*.DAX` files. Archive names are matched without case sensitivity.

In Art Catalog, use the archive and view filters or the Find field to narrow the contact grid.
Select a thumbnail to inspect it. Previous and Next move through the currently
visible items; Left and Right are shortcuts when the Find field is not focused.
Export PNG opens a save dialog with a filename that includes source, record,
appearance and view.

The native extension links `opengold_core` and `godot-cpp`; it reuses the game's
map loader and first-person compositor but does not start campaign state. It also reuses
`decode_dax_archive`, `decode_wall_tiles`, and `decode_wall_art`, including their
palette and transparency rules. The known Pool of Radiance installation has
2,100 perspective pieces from 210 appearances. Images are the stored artwork
fragments, so a single exported piece does not include the game's sky, floor,
neighboring walls, or encounter sprites.

## Check

After building, this command loads all wall records, exercises Previous and
Next, finds the waterfront art, exports its near-front image, and checks the PNG
pixel data:

```bash
OPENGOLD_GAME_DIR=/path/to/POOLRAD godot --headless --path dev_tools/tile_art_viewer/godot -- --viewer-check=/tmp/opengold-waterfront.png
```

The output PNG is local test output. The viewer never writes into the original
game directory unless that path is explicitly chosen in the export dialog.

The map check loads all 29 maps, renders their first-person views, checks the
alternate artwork profile, clicks a source piece, verifies PNG pixels, and jumps
on the top-down map:

```bash
OPENGOLD_GAME_DIR=/path/to/POOLRAD godot --headless --path dev_tools/tile_art_viewer/godot -- --map-check=/tmp/opengold-map-piece.png
```

For a visual review, run Godot with a display and append
`-- --map-capture=/tmp/opengold-map-browser.png` instead. It opens the map tab,
selects one visible piece, saves the full tool window as a PNG, and exits.
