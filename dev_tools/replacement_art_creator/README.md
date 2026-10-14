# Replacement Art Creator

This separate Godot tool builds new first-person wall art without opening the
original game files. It lists the 210 technical appearance IDs, stores
independent briefs, imports newly created PNGs, projects each appearance into
the ten renderer slots, previews a synthetic corridor, and exports replacement
sets. The four included pilot appearances use newly generated art; their input
prompts and file hashes are in [`data/provenance.json`](data/provenance.json).

The creator does **not** read the original tile-art descriptions or PNGs. The
technical IDs in `data/appearance_index.csv` link a new design to a game slot.
They contain no source image descriptions. Refer to the original catalog only
when assigning a broad function such as wall, door, or passage; write a new
brief before creating artwork. Do not use original PNGs as image-generation
inputs or trace their distinctive ornament, silhouette, or color arrangement.

## Run

From the repository root:

```bash
/Applications/Godot_mono.app/Contents/MacOS/Godot --path dev_tools/replacement_art_creator/godot
```

On Linux, substitute the installed `godot` executable. The creator needs
Godot 4.5 or newer. It needs no game files, CMake build, API key, or network
connection.

1. Choose an appearance on the left. The four pilot entries already have new
   art; all other entries start blank.
2. Write a new title and brief, choose Solid, Door, Opening, or Landmark, and
   press **Save brief**. **Copy wall prompt** and **Copy feature prompt** copy
   source-independent prompts for Codex's built-in imagegen. The image
   generation runs through Codex, outside this app.
3. Import a square new wall-material PNG. Doors, passages, and landmarks can
   also import an isolated PNG with transparency as their feature overlay.
   The app copies imports into its `godot/art` directory and records those paths
   when it saves the brief.
4. Press **Build ten views**. Select a perspective piece to inspect it; use
   Far, Middle, and Near above the synthetic corridor to check alignment and
   transparent openings. **Export selected set** writes ten PNGs and a manifest
   into a subfolder of the chosen directory. Repeated exports create numbered
   folders, preserving earlier versions.

The logical first-person canvas is 88 × 88. This pilot works at 4× resolution:
352 × 352 pixels for a composed view and 4× the original width and height for
each perspective slot. The geometry follows
`src/OpenGold.Core/src/exploration_view.cpp`; the imported material and feature
are new art. The tool builds front views by nearest-neighbor scaling and side
views by projecting the same wall onto tapered side planes. Passage forms cut
an independent opening in the wall before projection. These are a first art
pipeline, so inspect every exported view and refine the source art when seams,
feature distortion, or unwanted repetition appear.

## Review in a real map

The existing Tile Art Viewer can load a folder of exported sets. In its **Map
Browser**, press **Load replacement pack…**, choose the parent export folder,
then toggle **Show new art**. The map preview uses the same draw order at 4×
resolution. Appearances without a new set display the user's locally decoded
original art for context; the exported pack contains only new art. The latest
numbered export of each appearance wins when several versions are present.
Turn off **Show new art** to restore the original clickable source-piece
inspection. The replacement view is a visual preview; source-piece picking is
available in the original view.

To build and run the viewer on macOS:

```bash
cmake -S . -B build/tile-art-viewer -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENGOLD_BUILD_GODOT=ON -DOPENGOLD_BUILD_TESTS=OFF
cmake --build build/tile-art-viewer --target opengold_tile_art_viewer -j6
/Applications/Godot_mono.app/Contents/MacOS/Godot --headless --editor --path dev_tools/tile_art_viewer/godot --import --quit
OPENGOLD_GAME_DIR=/path/to/POOLRAD /Applications/Godot_mono.app/Contents/MacOS/Godot --path dev_tools/tile_art_viewer/godot
```

The viewer's first headless import may crash on exit after writing its
GDExtension registration; its existing README describes this behavior.

## Checks

```bash
/Applications/Godot_mono.app/Contents/MacOS/Godot --headless --path dev_tools/replacement_art_creator/godot -- --creator-check=/tmp/opengold-replacement-pilot
OPENGOLD_GAME_DIR=/path/to/POOLRAD /Applications/Godot_mono.app/Contents/MacOS/Godot --headless --path dev_tools/tile_art_viewer/godot -- --replacement-check=/tmp/opengold-replacement-pilot
```

The first check builds four complete pilot sets, validates their dimensions and
the arch transparency, and writes previews to `/tmp`. The second loads that
pack, renders it at a mapped location, and checks that switching back restores
the original inspector.

This tool and pack preview do not yet change the game's runtime art loader.
Game integration should follow visual review of the pilot and a decision on
how a replacement pack is selected at launch.
