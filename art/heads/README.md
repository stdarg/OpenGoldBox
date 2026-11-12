# Additional combat heads

Ten original head designs for Tieflings, Goliaths, Gnomes, Dragonborn, and Orcs.
Each transparent source sheet contains two right-facing heads, ordered left to
right. These are combat head components, separate from the chest-up artwork in
`new-art-pack/portraits`.

## Source artwork

| Species | Source sheet | Left design | Right design |
| --- | --- | --- | --- |
| Tiefling | [tiefling.png](sources/tiefling.png) | `tiefling-01`: upright horns | `tiefling-02`: swept horn |
| Goliath | [goliath.png](sources/goliath.png) | `goliath-01`: diagonal temple marking | `goliath-02`: brow and cheek markings |
| Gnome | [gnome.png](sources/gnome.png) | `gnome-01`: swept hair | `gnome-02`: short beard |
| Dragonborn | [dragonborn.png](sources/dragonborn.png) | `dragonborn-01`: swept crest | `dragonborn-02`: blunt horn and longer snout |
| Orc | [orc.png](sources/orc.png) | `orc-01`: topknot | `orc-02`: shaved scalp and marking |

These identifiers name artwork variants; they are not assigned game record IDs.
The source PNGs preserve the generator's original resolution and alpha.

## Native exports

There are **40 indexed PNG components** in this directory: ten designs, each
with tall/short and ready/action versions. For example,
`goliath-01-tall-ready.png` is a 24 × 8 head overlay. The corresponding action
head has identical pixels; the body provides the change of pose.

[review.png](review.png) shows all twenty design/size combinations at 10× and
1×. It contains only the new head artwork. The checkerboard belongs to the
review sheet; each component has real transparency.

[heads.json](heads.json) is the editable source for the final native pixels.
It records source crop bounds, component offsets, filenames, semantic pixel
rows, and default feature/skin colors. Crop bounds use exclusive right/bottom
edges. Pixel rows use `.` for transparency, `f` for features, `#` for black, and
`s` for skin. The exporter places these rows at their cataloged offsets inside
the component canvas.

The game does not yet register or load these additional combat heads. These
exports provide assets and metadata for that integration. When reading the PNGs
for customization, retain their palette **indices**, not just their preview RGB
values. PNG index 8 is black even though character-color choice 8 is dark gray.
The catalog's default colors are character-color choices, separately from the
semantic indices in the images. Dragonborn crest previews use the existing
dark-gray color choice; the generated source used a darker green outside that
palette.

## Native component constraints

- Each design has components for both short and tall bodies, with ready and action
  variants. The existing composer overlays nonzero head pixels onto the top
  rows of a 24 × 24 body.
- Tall head canvases are **24 × 8 pixels**; short head canvases are **24 × 10
  pixels**. Preserve the neck position and transparent canvas padding.
- **Goliath heads must be the same native size as human heads.** The inspected
  human reference has a 5 × 6 visible footprint at `(10, 2)` on the tall canvas
  and a 7 × 5 footprint at `(9, 5)` on the short canvas. Coordinates are
  zero-based; both footprints reach their canvas's bottom edge. Do not enlarge
  the Goliath head asset because the character is taller.
- Horns, ears, and crests must fit within the component canvas. Check the tiny
  silhouettes after reduction; generated pixel-art styling does not guarantee
  a uniform native pixel grid.
- Preserve semantic combat indices: `0` is transparent, `8` is opaque black,
  `4` is the head's Color-1 region, and `12` is its Color-2 region. Hair, horns,
  or facial markings belong to the feature region; skin belongs to the skin
  region. Do not collapse black outlines into transparency or use body-region
  indices for head shading.
- All components end with skin at canvas columns 11 and 12, aligning the neck
  with the existing body components.
- Keep head placement stable between ready and action poses. The inspected
  original head pairs use identical pixels for these poses; the body supplies
  the action animation.

The native asset dimensions above are separate from the game's approved
[whole-sprite Goliath rendering](../../docs/TECH.md#goliath-combat-sprites-and-draw-order).
The composer and color-region mapping are in
[`character_art.cpp`](../../src/OpenGold.Core/src/character_art.cpp); the combat
image format is documented in [graphics-format.md](../../docs/graphics-format.md).

## Provenance and checks

Created on 2026-09-18 using the built-in image generator, one call per species
source sheet. [prompts.json](prompts.json) records the exact prompts. No original
game images were supplied as generation inputs or copied into this directory.

All five PNGs were visually inspected. Their PNG chunk checksums, decoded pixel
data lengths, RGBA format, and actual transparent and opaque pixels were
verified.

Native preparation used Python/Pillow with user approval: split and crop the
two source heads, reduce with nearest-neighbor sampling, map to semantic
colors, then adjust individual pixels for readable species silhouettes, eyes,
horns, markings, and neck joins. The reviewed rows in `heads.json` preserve
these final adjustments; regeneration does not require another model call.

All 40 exports passed dimension, palette, transparency, neck-alignment,
Goliath human-size, design-uniqueness, and ready/action consistency checks.
All twenty design/size combinations were visually checked as individual
heads and composed with original bodies 0 and 31 in both ready/action poses.
The body-composition review is local only at
`../../user-data/head-review/body-composition.png`; it is not distributed.

## Reproduce and verify

`export.py` is an offline asset utility, requiring Pillow 10.1 or newer; it is
not part of the game runtime. With Pillow available in your Python environment,
run from the repository root:

```bash
python3 art/heads/export.py --check
```

To regenerate the 40 component PNGs and the review sheet from `heads.json`, run
the same command without `--check`. This intentionally replaces these derived
exports and leaves the generator's source sheets untouched. Native preparation
was performed with Pillow 12.3.0.
