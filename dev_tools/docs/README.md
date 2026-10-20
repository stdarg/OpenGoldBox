# Developer tools documentation

This folder is the entry point for the three standalone tools in `dev_tools/`.
Run the commands in these guides from the repository root. The tools use their
own Godot projects and do not start the game. Original Pool of Radiance files
are read from a local installation; they are not included here.

| Tool | What it does | Guide |
| --- | --- | --- |
| Tile Art Viewer | Browse original wall pieces and maps, edit piece descriptions, and inspect replacement art in map context. | [Tile Art Viewer](tile-art-viewer.md) |
| Replacement Art Creator | Prepare new wall and feature art, project it into perspective views, and export replacement sets. | [Replacement Art Creator](replacement-art-creator.md) |
| Combat Sprite Catalog | Inspect monster/NPC and character combat images, edit monster metadata and visual height tiers, and review ready/action replacement sprites. | [Combat Sprite Catalog](combat-sprite-catalog.md) |

## Documents in this folder

- [Tile Art Viewer](tile-art-viewer.md): launch, catalog and map workflows, data, and checks.
- [Replacement Art Creator](replacement-art-creator.md): briefs, source art, perspective exports, and checks.
- [Combat Sprite Catalog](combat-sprite-catalog.md): metadata, sprite footprint and humanoid height, weapon sets, animation review, and checks.

## Detailed tool documents

- [Tile Art Viewer README](../tile_art_viewer/README.md)
- [Replacement Art Creator README](../replacement_art_creator/README.md)
- [Combat Sprite Catalog README](../combat_sprite_catalog/README.md)
- [Combat sprite art inventory](../combat_sprite_catalog/art/README.md)
- [Combat sprite pose guide](../combat_sprite_catalog/art/pose-guide/README.md)
- [Combat sprite generation and review workflow](../combat_sprite_catalog/generation/README.md)
- [Combat sprite rules](../combat_sprite_catalog/generation/rules.txt), [ready prompt template](../combat_sprite_catalog/generation/ready.txt), and [action prompt template](../combat_sprite_catalog/generation/action.txt)

For combat sprite generation, the generation workflow and rules above are
authoritative. The art inventory records an earlier production pass and does not
replace the per-weapon-set review procedure.
