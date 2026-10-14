# Replacement Art Creator: user guide

The Replacement Art Creator is a separate Godot tool for making new artwork for
the game's first-person map view. It lists 210 technical appearances. Each
appearance has ten perspective pieces; the tool builds those pieces from a new
wall image and, when needed, a separate feature image. You can inspect original
pieces from your own game files on the **Reference** tab, but the **Create** tab
uses an independent brief and newly generated images.

An exported replacement set can be previewed in the Tile Art Viewer's **Map
Browser**. Exporting does not install the art into the game. The game runtime
does not yet load these replacement sets.

## Quick start on this Mac

Open a terminal and run these commands from the repository root:

```bash
cd /Users/edmond/src/OpenGold
OPENGOLD_GAME_DIR=/Users/edmond/POOLRAD \
  /Applications/Godot_mono.app/Contents/MacOS/Godot \
  --path dev_tools/replacement_art_creator/godot
```

If the tool has not been built and imported in this checkout, complete the
[one-time setup](#one-time-setup) first. Close and relaunch an already open
creator after updating the tool. Save any pending brief or description edits
before closing it.

`OPENGOLD_GAME_DIR` must point to the directory containing your original
`WALLDEF*.DAX` files. The Create tab can still work if the directory is not
available, but the Reference tab cannot display original art or edit its
descriptions.

## One-time setup

Install Godot 4.5 or newer, CMake, Ninja, Python, Git, and a C++ build toolchain.
From the repository root, build the native extension and let Godot import this
project once:

```bash
cd /Users/edmond/src/OpenGold
cmake -S . -B build/tile-art-viewer -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DOPENGOLD_BUILD_GODOT=ON -DOPENGOLD_BUILD_TESTS=OFF
cmake --build build/tile-art-viewer --target opengold_replacement_art_creator -j6
/Applications/Godot_mono.app/Contents/MacOS/Godot --headless --editor \
  --path dev_tools/replacement_art_creator/godot --import --quit
```

Then use the quick-start command above. On Linux, substitute your installed
`godot` executable and the path to your game files. Godot's first headless
import may crash on exit after registering the native extension. If the normal
launch succeeds, the import completed. If the launch reports that it cannot
find `WallArtCatalog`, repeat the import step.

## Walkthrough: choose an appearance

1. Start the creator. The left column lists appearances by technical ID, such
   as `WALLDEF1.DAX:1:4`, and any saved new title. The number below the list
   shows how many appearances already have new wall art.
2. Use the menu above the list to choose **All appearances**, **Used**, or
   **Unused**. The search field narrows the list by archive, record,
   appearance, or saved title.
3. Select an appearance. Its selection stays in place as you switch between
   **Reference** and **Create**.

The usage labels help you choose work that will appear in the map preview:

| Label | Meaning |
| --- | --- |
| No prefix on a used entry | All ten pieces have a visible map location in the available inspection profiles. |
| `3/10 used` (the number varies) | That many of its ten perspective pieces have a visible location. |
| `UNUSED` | None of its ten pieces has a visible location in the available maps and profiles. |
| `UNKNOWN` | The tool could not load usage information from the Art Catalog CSV. |

**Unused** is limited to the maps and inspection profiles the tools can
currently inspect. It does not prove that conditional paths or future content
will never use the art. You can still create a replacement for an unused
appearance. On a partly used appearance, the Reference tab marks unused
individual pieces.

## Walkthrough: inspect and correct the reference

1. Select an appearance and open **Reference**. The left side of this tab shows
   ten original perspective thumbnails decoded from your local game files.
2. Select a thumbnail. The right side shows a larger preview, the view name,
   original pixel dimensions, map-use status, and the Art Catalog description.
   A used piece also names one map in which it can be seen.
3. If a description is inaccurate, edit **Art Catalog description for this
   piece**. You may inspect and edit several pieces before saving.
4. Press **Save descriptions to CSV**. The tab reports the save result.

Descriptions are stored per perspective piece in
[`dev_tools/tile_art_viewer/data/tile_art_catalog.csv`](../dev_tools/tile_art_viewer/data/tile_art_catalog.csv).
This action changes that CSV in your checkout; it does not change the original
game files. The creator detects an external CSV change and asks you to reopen
before saving, so another editor's changes are not overwritten.

Use this tab to understand a piece's broad function, such as a wall, door, or
passage. Write the replacement brief as a new design. Original pixels and
catalog descriptions are not inserted into generated prompts or replacement
exports.

## Walkthrough: create new art

1. Open **Create** with an appearance selected. Four pilot appearances already
   have new art; an untouched appearance starts without a wall image.
2. Enter a **New title**, choose the **Form**, and write an **Independent
   creative brief**. The form choices are **Solid**, **Door**, **Opening**, and
   **Landmark**. Describe the materials, mood, and distinguishing new design
   elements. Click **Save brief** to keep the text for the next session.
3. Click **Copy wall prompt**. Paste the clipboard text into a Codex chat that
   can generate images, generate a square wall material, and save the resulting
   PNG locally. The prompt asks for a plain wall without the door, opening, or
   landmark. Image generation runs outside this tool.
4. For **Door**, **Opening**, or **Landmark**, also click **Copy feature prompt**.
   Generate and save an isolated feature on a genuinely transparent PNG
   background. **Solid** does not need a feature image, and its feature prompt
   button is disabled.
5. Click **Import wall PNG…** and choose the new wall image. If you made a
   feature, click **Import feature PNG…** and choose it. The tool copies each
   image into its `godot/art` directory and saves its path in the brief record.
   The **Wall** and **Feature** line shows the current imports.
6. Click **Build ten views**. The right-hand grid contains the generated Far,
   Middle, Near, left, right, and extension pieces. Select each piece for a
   larger preview and its pixel dimensions.
7. Switch **Synthetic corridor** between **Far**, **Middle**, and **Near**.
   Inspect seams, projection, repetition, and transparency. If the result needs
   work, revise the new source PNG, import it again, and rebuild.
8. Click **Export selected set…** and choose a parent folder. The creator makes
   a child folder named for the appearance, containing ten PNGs and a
   `manifest.json`. Exporting the same appearance again creates a numbered
   child folder, leaving the earlier export intact.

The logical first-person canvas is 88 × 88 pixels. The creator exports each
perspective piece at four times its original dimensions and composes a 352 ×
352 corridor preview. Check all ten pieces before relying on the export;
projection can expose distortion or seams that are hard to see in the input.

## Walkthrough: preview the export in a map

Build and import the Tile Art Viewer once if needed:

```bash
cd /Users/edmond/src/OpenGold
cmake --build build/tile-art-viewer --target opengold_tile_art_viewer -j6
/Applications/Godot_mono.app/Contents/MacOS/Godot --headless --editor \
  --path dev_tools/tile_art_viewer/godot --import --quit
```

Launch it with the same game-file directory:

```bash
cd /Users/edmond/src/OpenGold
OPENGOLD_GAME_DIR=/Users/edmond/POOLRAD \
  /Applications/Godot_mono.app/Contents/MacOS/Godot \
  --path dev_tools/tile_art_viewer/godot
```

1. Open **Map Browser** and choose a map. The viewer automatically loads art
   from `dev_tools/replacement_art_creator/replacement_pack` and starts with
   **Show new art** enabled. To review another export, place its appearance
   folder under `replacement_pack` and restart the viewer.
2. The first-person view displays replacements where
   the pack has them and locally decoded originals for missing appearances.
   If you exported an appearance more than once, the latest numbered export
   wins.
3. Navigate with the arrow keys: **Left** and **Right** turn, **Up** moves
   forward, and **Down** turns around. You can move through walls in this
   inspection tool. You can also click the top-down map to jump to a cell.
4. Turn off **Show new art** to return to the original view. Click a visible
   source piece there if you need its exact original metadata. The replacement
   view is for visual review; source-piece picking works in the original view.

See the [Tile Art Viewer guide](../dev_tools/tile_art_viewer/README.md) for its
Art Catalog, map controls, and source-piece export features.

## Where your work goes

| Action | File or location |
| --- | --- |
| Save brief | `dev_tools/replacement_art_creator/data/briefs.json` |
| Import a new PNG | `dev_tools/replacement_art_creator/godot/art/` and its path in `briefs.json` |
| Save a reference description | `dev_tools/tile_art_viewer/data/tile_art_catalog.csv` |
| Export a set | A new child folder inside the parent folder you choose |

The source game files are read from `OPENGOLD_GAME_DIR`; these operations do
not write to that directory. The imported images and brief file are in your
checkout, so check your Git changes if you want to keep or share them. The
exported pack contains only new art and its manifest.

## Common problems

- **Reference art is unavailable:** Check that `OPENGOLD_GAME_DIR` points to
  the folder containing the original `WALLDEF*.DAX` files, then relaunch.
- **`WallArtCatalog` cannot be found:** Run the one-time native build and Godot
  import commands above for this checkout.
- **Usage information is unavailable:** Check that the Art Catalog CSV exists
  and has its expected columns. The Reference tab reports loading errors.
- **A description will not save:** Descriptions cannot be empty. If the CSV
  changed outside this window, close and reopen the creator before editing
  and saving again.
- **Build reports an incomplete set:** Confirm the imported wall file is a
  readable PNG. Reimport any missing or damaged feature PNG, then build again.
- **New art is absent in Map Browser:** Load the parent export folder, enable
  **Show new art**, and visit a map location that actually uses that appearance.
  The **Unused** label can help explain why an appearance is hard to find.

For developer details and optional headless checks, see the
[Replacement Art Creator README](../dev_tools/replacement_art_creator/README.md).
