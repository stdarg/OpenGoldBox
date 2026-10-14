# Tile Art Viewer and Map Browser

This is a standalone Godot tool for browsing Pool of Radiance's original
first person wall artwork and exploration maps. It is a separate Godot project under `dev_tools` and
is not linked into the game. It reads your installed DAX files at runtime; no
original artwork is included in this repository.

The **Art Catalog** tab's contact grid uses five columns at the default window
size. It shows every stored perspective piece from `WALLDEF1` through
`WALLDEF8`, including doors, gateways, waterfront/dock art, and other area
variations. Each item identifies its source archive, record, appearance number,
and view. The preview uses the source pixels with nearest neighbor scaling.
Export PNG writes the **original size** piece with its transparent pixels, not
the scaled thumbnail or preview. Every piece has a concise description in
[`data/tile_art_catalog.csv`](data/tile_art_catalog.csv), alongside
its source metadata and one exact map location where available. The CSV has one
row per perspective image. New Phlan's existing named appearances are also
retained in a separate `name` column. Descriptions may repeat when source pixels are
identical across wall sets. Select an image, edit its **Description**, and press
**Save descriptions to CSV**. Unsaved edits remain available while browsing;
Save writes them together to the CSV in this source checkout.

**See in Map** opens the Map Browser at the saved player position and facing,
then selects the exact source piece visible at the saved pixel. The tool checks
the saved location against the current renderer before jumping. Of the 2,100
catalog images, 1,552 are visibly rendered somewhere in the 29 available maps
and inspection profiles. The other 548 are labeled **Unused in available
maps/profiles**, have blank map-location columns, and cannot use See in Map.
Some maps have unresolved conditional artwork paths, so "unused" is scoped to
the profiles currently available to this tool.

The **Map Browser** tab loads all 29 decoded GEO maps, including New Phlan,
Slums, and Kuto's Well. Pick a map, then use the arrow keys as in exploration:
Left and Right turn, Up moves forward, and Down turns around. Forward movement
passes through walls and doors for inspection, stopping only at map edges.
Click the top-down map to jump to any cell. The map shows walls in blue, doors
in gold, cells with a nonzero event or indoor byte as purple dots, and the
current pose in yellow. A legend below the map explains each marker.

The first-person view uses the game's original 88 × 88 perspective compositor.
Click a visible wall or door in that view. The panel below it shows the exact
source piece that supplied that pixel: map cell, face, wall and door codes, event
byte, source archive and record, appearance, perspective, dimensions, and a
preview. Sky and floor pixels have no source wall piece. The **Visible pieces**
control gives keyboard users another way to select one. Export PNG saves that
piece at its source resolution with transparency. Only the clicked or selected
piece appears in the details panel; changing position or facing clears it.

For new art review, the Map Browser can load replacement sets exported by the
separate [Replacement Art Creator](../replacement_art_creator/README.md). Press
**Load replacement pack…** and select the parent folder of exported sets, then
toggle **Show new art**. The 352 × 352 preview follows the game's draw order;
missing appearances still use artwork decoded locally from your installation.
Switch back to the original view for exact source-piece picking and export.
The replacement pack contains only the new PNGs and their manifests.

The map's artwork selector shows inspection profiles derived from decoded area
scripts and their `LOAD PIECES` instructions. Some maps inherit a prior wall set
or have unresolved script operands; Lizard Men Keep exposes two observed sets.
The profile label and source are shown in the tool. These profiles help inspect
the original assets; they do not claim to resolve every conditional runtime
script path.

The CSV identifies each image by its catalog index, wall archive, record,
appearance, view and pixel dimensions. For visible images it also records the
map archive, record and name; artwork profile and source; player position and
facing; clicked 88 × 88 view pixel; source map cell and face; and wall, door and
event values. The map-location fields are empty for unused images. The viewer
validates the CSV against decoded artwork when it starts and reports mismatches
instead of pairing descriptions with the wrong images.

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
visible items; Left and Right move one piece, while Up and Down move one row
in the same column when the Find field and filter controls are not focused.
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

After building, this command loads all wall records, checks the five-column
contact sheet and arrow navigation, finds the waterfront art, exports its
near-front image, and checks the PNG pixel data:

```bash
OPENGOLD_GAME_DIR=/path/to/POOLRAD godot --headless --path dev_tools/tile_art_viewer/godot -- --viewer-check=/tmp/opengold-waterfront.png
```

The output PNG is local test output. The viewer never writes into the original
game directory unless that path is explicitly chosen in the export dialog.

The map check loads all 29 maps, renders their first-person views, checks
traversal through a wall and the alternate artwork profile, clicks a source
piece, verifies PNG pixels, and jumps on the top-down map:

```bash
OPENGOLD_GAME_DIR=/path/to/POOLRAD godot --headless --path dev_tools/tile_art_viewer/godot -- --map-check=/tmp/opengold-map-piece.png
```

The metadata check verifies all CSV rows and exact visible locations, exercises
See in Map and the unused state, and writes an edited description to a temporary
CSV to check quoting without changing the catalog:

```bash
OPENGOLD_GAME_DIR=/path/to/POOLRAD godot --headless --path dev_tools/tile_art_viewer/godot -- --metadata-check=/tmp/opengold-tile-metadata-check.csv
```

For a visual review, run Godot with a display and append
`-- --viewer-capture=/tmp/opengold-art-catalog.png` or
`-- --map-capture=/tmp/opengold-map-browser.png`. Each saves the full tool window
as a PNG and exits; the map capture selects one visible piece first.
