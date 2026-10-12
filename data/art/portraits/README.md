# Additional portrait heads

Ten new portrait heads inspired by the DOS Pool of Radiance portraits,
generated with the built-in image generation tool. Each PNG contains one adult
head on a black background, facing slightly right, without a body or costume.

| Species | Man | Woman |
| --- | --- | --- |
| Gnome | [Portrait](gnome-male.png) | [Portrait](gnome-female.png) |
| Orc | [Portrait](orc-male.png) | [Portrait](orc-female.png) |
| Goliath | [Portrait](goliath-male.png) | [Portrait](goliath-female.png) |
| Tiefling | [Portrait](tiefling-male.png) | [Portrait](tiefling-female.png) |
| Dragonborn | [Portrait](dragonborn-male.png) | [Portrait](dragonborn-female.png) |

The Goliath pair now uses slate-blue skin, russet-brown markings and amber
eyes. The color revision preserves their faces and framing; its edit prompts
are recorded alongside the original generation prompts.

The male Orc, Goliath, Tiefling and Dragonborn were subsequently edited with
the built-in image generation tool to straighten their necks and provide flat
bases. The Orc's full chin now has neck space below it before the armor join;
its former runtime crop has been removed. The exact edit prompts are recorded
in [neck-revision-prompts.json](neck-revision-prompts.json). These four updated
PNGs replace the previous versions at the same paths and retain their IDs.

These enlarged source PNGs are now used by the native character creator.
The game build copies them into `src/OpenGoldBox/godot/bin/portraits/`, and Godot
imports those copies as resources. At load time, native code fits each
head into the 88 x 40 panel with nearest-neighbor sampling and trims it at a
measured neck baseline. When a body is selected, native composition aligns the
head to its neck opening and fits the lowest neck rows to its width. Faces and
horns retain their horizontal proportions. The source PNGs stay intact. Their
approved colors are retained; they are not restricted to exactly sixteen EGA colors.

The portrait dropdown and head arrows include all ten new heads alongside the
original game's heads. Race/gender choices suggest a matching head until the
player makes a manual selection. See [Character creation](../../../docs/CHARACTER-CREATION.md).

The original portraits were used only as a local style/framing reference and
are not included here. [prompts.json](prompts.json) preserves the exact prompt
for each image. The PNGs preserve the generation outputs without resampling.
