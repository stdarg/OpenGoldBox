# Tile Art Viewer

The [Tile Art Viewer](../tile_art_viewer/README.md) is a standalone Godot tool
with an **Art Catalog** and **Map Browser**. It reads the locally installed
`WALLDEF*.DAX` wall pieces and `8X8D*.DAX` maps. The Art Catalog can show all
perspective pieces, including entries unused in the currently decoded map
profiles. The Map Browser shows those pieces in a first-person view and lets
you jump from a visible piece to its catalog entry.

From the repository root, launch it with:

```bash
./dev_tools/tile_art_viewer/run.sh [POOLRAD_DIRECTORY]
```

The optional directory overrides `OPENGOLD_GAME_DIR` and `~/POOLRAD`. Set
`GODOT_BIN` if Godot is outside `PATH` and the standard macOS location. On
Windows, run it from Git Bash. The launcher builds the native extension and
imports its Godot project when needed.

## Files and workflow

- [`data/tile_art_catalog.csv`](../tile_art_viewer/data/tile_art_catalog.csv) stores a description and source metadata for each perspective piece, plus a verified map location where available. Edit descriptions in the catalog and use **Save descriptions to CSV** to write them.
- **Flag for Regen** saves a per-piece review flag to the CSV. It does not create art.
- **Show new art** displays the game's new wall art from [`data/art/walls`](../../data/art/walls), composed by the game's own code; appearances without a set show locally decoded original art.
- [`godot/`](../tile_art_viewer/godot) contains the standalone UI. [`native/`](../tile_art_viewer/native) contains its Godot extension and wall/map catalog implementation.

The [tool README](../tile_art_viewer/README.md) explains the filters, map
navigation, original-size PNG export, metadata fields, build requirements, and
headless checks (`--viewer-check`, `--map-check`, `--metadata-check`, and
`--replacement-check`).
