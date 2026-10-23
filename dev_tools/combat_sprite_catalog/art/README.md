# Replacement monster combat sprites

**For new generation/regeneration:** follow the
[generation and review workflow](../generation/README.md). Its bipedal facing and
pose requirements supersede this historical generation summary. Existing images
have not been certified against the new specification. The new original
[pose guide](pose-guide/README.md) was approved by the user on 2026-10-17.

This set contains 128 x 128 transparent pixel-art PNGs for the **72 referenced
monster records** in the local Pool of Radiance catalog: **81 complete weapon
sets**, each with `ready.png` and `action.png` (162 images). The paths match the
`variants` entries in `../json/*.json`, so the Combat Sprite Catalog loads them
automatically when **Show new art when available** is enabled.

The selection excludes records with no decoded game reference, human and named
NPC records, and unidentified `CPIC` image pairs. The original character data
sets its NPC flag on all named monster records, so selection used creature type
and names, including the Hobgoblin and Lizardman records whose type field is
zero. NPC art remains for a later pass.

The artwork was made with Codex's built-in image generator from the monster
name, editable text description, and the selected equipment set in the catalog
JSON. No original game image was supplied as a visual reference. For each set,
the action pose uses its newly generated ready pose as a reference; a second
weapon set uses the first new ready pose to retain the same creature identity.
The prompt set calls for one original full-body fantasy monster, crisp pixel-art
color clusters, a transparent square canvas, complete visible equipment, and
consistent scale and foot placement. The generated working images were exported
to 128 x 128 with point sampling. The ready and action images were reviewed in
contact sheets at their final size.

The game never reads this folder: **Export to game** copies the reviewed sets
into [`data/monsters`](../../../data/monsters/README.md), which the game ships.
