# First Person Tile Art Viewer

This is a standalone Godot editor tool for browsing Pool of Radiance's original
first person wall artwork. It is a separate Godot project under `dev_tools` and
is not linked into the game. It reads your installed DAX files at runtime; no
original artwork is included in this repository.

The contact grid shows every stored perspective piece from `WALLDEF1` through
`WALLDEF8`, including doors, gateways, waterfront/dock art, and other area
variations. Each item identifies its source archive, record, appearance number,
and view. The preview uses the source pixels with nearest neighbor scaling.
Export PNG writes the **original size** piece with its transparent pixels, not
the scaled thumbnail or preview. Phlan (`WALLDEF3.DAX:0`) has descriptive names;
other records keep their source identifiers because appearance numbers are local
to a wall set.

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

Use the archive and view filters or the Find field to narrow the contact grid.
Select a thumbnail to inspect it. Previous and Next move through the currently
visible items; Left and Right are shortcuts when the Find field is not focused.
Export PNG opens a save dialog with a filename that includes source, record,
appearance and view.

The native extension links only `opengold_formats` and `godot-cpp`; the game's
runtime and campaign state are not loaded. The project reuses
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
