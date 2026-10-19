# Replacement Art Creator

The [Replacement Art Creator](../replacement_art_creator/README.md) is a
standalone Godot tool for new wall and feature art. Its **Reference** tab shows
locally decoded original pieces and their catalog descriptions. Its **Create**
tab stores independent briefs, imports new PNGs, projects each appearance into
ten perspective slots, previews a synthetic corridor, and exports a replacement
set. Original game pixels are not image-generation inputs.

From the repository root, launch it with:

```bash
./dev_tools/replacement_art_creator/run.sh [POOLRAD_DIRECTORY]
```

The game-data directory is optional for the Create tab; the Reference tab
needs it. The launcher uses `OPENGOLD_GAME_DIR` or `~/POOLRAD` when available
and accepts `GODOT_BIN` for a different Godot executable.

## Files and workflow

- [`data/briefs.json`](../replacement_art_creator/data/briefs.json) records the appearance briefs and source-art choices. [`data/appearance_index.csv`](../replacement_art_creator/data/appearance_index.csv) maps appearance IDs.
- [`godot/art/`](../replacement_art_creator/godot/art) holds imported new source PNGs. [`data/provenance.json`](../replacement_art_creator/data/provenance.json) and [`data/source_art_provenance.json`](../replacement_art_creator/data/source_art_provenance.json) record prompt and source-image provenance.
- [`replacement_pack/`](../replacement_art_creator/replacement_pack) contains exported perspective PNGs and manifests. The Tile Art Viewer loads this pack for map-context review.
- [`assign_used_art.py`](../replacement_art_creator/assign_used_art.py) assigns or checks replacement coverage for used appearances; `--check-pack` audits the pack.

The [tool README](../replacement_art_creator/README.md) gives the full
description-editing and creation steps, export behavior, and headless checks
(`--creator-check`, `--reference-check`, `--usage-check`, and `--export-used`).
