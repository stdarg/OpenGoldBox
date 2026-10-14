# Replacement Art Creator

This separate Godot tool lists 210 technical appearance IDs, stores independent
briefs, imports newly created PNGs, projects each appearance into the ten
renderer slots, previews a synthetic corridor, and exports replacement sets.
The included [`replacement_pack`](replacement_pack) covers all 168 appearances
visible in the current map catalog, with ten new perspective PNGs per
appearance. Four earlier pilot appearances are retained. Their prompts and
hashes are in [`data/provenance.json`](data/provenance.json); the 45 new source
PNGs and exact Codex imagegen prompts are recorded in
[`data/source_art_provenance.json`](data/source_art_provenance.json).

The **Reference** tab decodes original artwork from your local game files with
the existing Tile Art Viewer extension. It shows all ten source pieces for the
selected appearance and lets you edit the corresponding description in the
existing [`tile_art_catalog.csv`](../tile_art_viewer/data/tile_art_catalog.csv).
The **Create** tab keeps its independent brief and prompt fields separate.
Original pixels and catalog descriptions are never inserted into prompts or
replacement exports. Use the reference to verify a broad function such as
wall, door, or passage; do not use original PNGs as image-generation inputs or
trace their distinctive ornament, silhouette, or color arrangement.

## Run

From the repository root, run:

```bash
./dev_tools/replacement_art_creator/run.sh
```

This builds the native extension, registers it with Godot on first run, and
opens the creator. It uses `OPENGOLD_GAME_DIR` or `~/POOLRAD` when available
for the Reference tab. Pass another game-data directory as an argument, or set
`GODOT_BIN` if Godot is outside `PATH` and the standard macOS app location:

```bash
./dev_tools/replacement_art_creator/run.sh /path/to/POOLRAD
```

The manual commands to build the shared native decoder and register it with
this Godot project are:

```bash
cmake -S . -B build/tile-art-viewer -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENGOLD_BUILD_GODOT=ON -DOPENGOLD_BUILD_TESTS=OFF
cmake --build build/tile-art-viewer --target opengold_replacement_art_creator -j6
/Applications/Godot_mono.app/Contents/MacOS/Godot --headless --editor --path dev_tools/replacement_art_creator/godot --import --quit
OPENGOLD_GAME_DIR=/path/to/POOLRAD /Applications/Godot_mono.app/Contents/MacOS/Godot --path dev_tools/replacement_art_creator/godot
```

On this Mac, `/Users/edmond/POOLRAD` is the game-file path. On Linux,
substitute the installed `godot` executable. The creator needs Godot 4.5 or
newer. A first headless editor import may crash on exit after registering the
extension; the viewer's README describes the same behavior. If
`OPENGOLD_GAME_DIR` is unset or unavailable, the Create tab still works and
the Reference tab explains why original art is unavailable.

## Review and correct a description

The appearance list puts **UNUSED** before an appearance when none of its ten
pieces is visible in the Art Catalog's available maps and profiles. Partly used
appearances show a count such as **3/10 used**. Use the menu above the list to
show all, used, or unused appearances. The Create tab repeats the selected
appearance's status, and the Reference tab marks each unused perspective piece.
This can help prioritize new art: the current catalog has 42 wholly unused and
37 partly used appearances. "Unused" describes the available map inspection
profiles; it does not prove that no conditional or future game path can use the
art. The creator still allows you to work on an unused appearance intentionally.

1. Select an appearance on the left, then open **Reference**. Its ten thumbnails
   are the original perspective pieces from your local game files.
2. Select a piece. The large preview, source size, map-use label, and existing
   Art Catalog description update together.
3. Edit the description and press **Save descriptions to CSV**. Changes to
   several pieces can be saved together. Saving updates the existing Art
   Catalog CSV in this checkout; no original PNG is written to the repository.
   If another window changes the CSV meanwhile, reopen the creator before
   saving so those edits are not overwritten.

Descriptions are per perspective piece, so the ten pieces of one appearance
may have separate text. The selected appearance remains the same when switching
between Reference and Create.

## Create a replacement

1. Choose an appearance on the left. Used appearances have assigned new art;
   wholly unused appearances remain blank.
2. Write a new title and brief, choose the appropriate art kind, and
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

The logical first-person canvas is 88 × 88. The pack works at 4× resolution:
352 × 352 pixels for a composed view and 4× the original width and height for
each perspective slot. The geometry follows
`src/OpenGold.Core/src/exploration_view.cpp`; the imported material and feature
are new art. The tool builds front views by nearest-neighbor scaling and side
views by projecting the same wall onto tapered side planes. Passage forms cut
an independent opening in the wall before projection. These are a first art
pipeline, so inspect every exported view and refine the source art when seams,
feature distortion, or unwanted repetition appear.

## Review in a real map

The Tile Art Viewer automatically loads sets from
`dev_tools/replacement_art_creator/replacement_pack` at startup. Its **Show new
art** toggle controls both Art Catalog and Map Browser; it starts on. To review
another export, place its appearance folder under `replacement_pack` and restart
the viewer. The map preview uses the same draw order at 4×
resolution. Appearances without a new set display the user's locally decoded
original art for context; the included pack has sets for every appearance
marked used by the current catalog. It contains only new art. The latest
numbered export of each appearance wins when several versions are present.
Click a visible piece in either view to inspect it, then use **See in Art
Catalog** to open its entry. Turn off **Show new art** to compare with the
original artwork.

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
python3 dev_tools/replacement_art_creator/assign_used_art.py --check-pack
OPENGOLD_GAME_DIR=/path/to/POOLRAD /Applications/Godot_mono.app/Contents/MacOS/Godot --headless --path dev_tools/replacement_art_creator/godot -- --reference-check=/tmp/opengold-reference-check
OPENGOLD_GAME_DIR=/path/to/POOLRAD /Applications/Godot_mono.app/Contents/MacOS/Godot --headless --path dev_tools/replacement_art_creator/godot -- --usage-check
OPENGOLD_GAME_DIR=/path/to/POOLRAD /Applications/Godot_mono.app/Contents/MacOS/Godot --headless --path dev_tools/tile_art_viewer/godot -- --replacement-check=/path/to/replacement_pack
```

The first check builds four complete pilot sets, validates their dimensions and
the arch transparency, and writes previews to `/tmp`. The reference check
loads all original pieces and saves a description into a temporary CSV copy.
The usage check verifies the list filters, appearance status, and unused-piece
labels against the catalog metadata.
The pack audit checks all 168 used appearances, 168 manifests, and 1,680
perspective PNG dimensions. The viewer check loads the replacement pack,
renders it at a mapped location, and checks that switching back restores the
original inspector.

To rebuild the pack from the new source sprites and briefs, export to an empty
folder:

```bash
/Applications/Godot_mono.app/Contents/MacOS/Godot --headless --path dev_tools/replacement_art_creator/godot -- --export-used=/path/to/new/export/folder
```

This tool and pack preview do not yet change the game's runtime art loader.
The 42 appearances marked wholly unused by current inspection profiles have
no replacement sets. Conditional or future map paths may require more art.
