# Phlan wall artwork

The Rolf demo uses `GEO3.DAX:0` appearance IDs and an explicit, verified
`WALLDEF3.DAX:0` profile. The same numbers can select different artwork in other
areas; they are not universal material IDs.

| ID | Appearance |
| --- | --- |
| 0 | No wall artwork |
| 1 | Open gateway/tunnel; used at Slums and dock entrances |
| 2 | Plain gray wall |
| 3 | Gray wall with wooden door |
| 4 | Ivy-covered gray wall |
| 5 | Ivy-covered gray wall with wooden door |
| 6 | Light stone masonry, pattern A |
| 7 | Light stone masonry with wooden door |
| 8 | Light stone masonry, pattern B |
| 9 | Stone shopfront: striped awning, door left, windows right |
| 10 | Stone facade: windows left, door right |
| 11 | City Hall: brick facade, blue/cyan columns, three doors and steps |
| 12 | Training hall: brick facade, double doors and steps |
| 13 | Temple entrance: brick arch, yellow columns, wooden door |
| 14 | Waterfront posts with hanging chains/ropes and water behind |
| 15 | Low stone waterfront wall with a blue water band beyond |

Names are descriptive labels from the decoded pixels, supplemented by original
tour dialogue for landmarks. IDs 6 and 8 are different masonry patterns: their
near-facing images differ at 1,169 of 3,584 pixels. The waterfront labels do not
establish collision or traversal rules.

## Resource assembly

The installed `WALLDEF3.DAX:0` record has 2,340 decompressed bytes: fifteen
156-byte definitions. Appearance `n` starts at `(n-1)*156`.

| Tile slots | Source |
| --- | --- |
| 0 | Blank, transparent tile |
| 1–45 | `8X8D1.DAX:203` |
| 46–115 | `8X8D3.DAX:101` |
| 116–185 | `8X8D3.DAX:102` |
| 186–255 | `8X8D3.DAX:103` |

The tile records have a 17-byte header followed by packed 8 x 8 images, high
nibble first. Walls use the normal EGA palette: black is opaque, index 8 is gray,
and pink is transparent when compositing. This differs from combat icons.

Each definition stores these views as row-major arrays of tile indices:

| View | Byte offset | Tile columns x rows |
| --- | --- | --- |
| Far front | 0 | 1 x 2 |
| Far left | 2 | 1 x 4 |
| Far right | 6 | 1 x 4 |
| Middle front | 10 | 3 x 4 |
| Middle left | 22 | 2 x 8 |
| Middle right | 38 | 2 x 8 |
| Near front | 54 | 7 x 8 |
| Near left | 110 | 2 x 11 |
| Near right | 132 | 2 x 11 |
| Far extension | 154 | 1 x 2 |

`OpenGold.Formats` validates and decodes these records. `RolfTourSession` resolves
the explicit Phlan resource profile and owns the resulting images. The native
`compose_exploration_view` function samples directional GEO edges and combines
their stored perspectives into an 88 x 88 image. The Godot wrapper uploads that
image only when the pose changes, applies pixel-aspect correction and overlays
Rolf's original encounter sprite. No original images are distributed.

## Verification and remaining limits

The native tour executes original ECL and confirms these landmark poses:

| Landmark | GEO position/facing | Appearance |
| --- | --- | --- |
| Temple of Tyr | (11,2), south | 13 |
| Dock-area entrance | (11,2), north | 1 |
| Training hall | (5,2), east | 12 |
| City Hall | (4,3), south | 11 |
| Slums entrance | (0,4), west | 1 |

The original City Hall screenshot, training-hall screenshot and gateway
screenshots agree with those positions and the rendered artwork. Original ivy,
ivy-door and shop crops also match IDs 4, 5 and 9. Local captured tour images
remain in ignored `user-data`; user-supplied screenshots are not committed.

The format and perspective-layout reference is Gold Box Explorer revision
`eac30abaa6ee66aea6f5d65ebe6d676b10015a8f`, specifically
[DaxWallDefFile.cs](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/Dax/DaxWallDefFile.cs)
and
[GeoDaxFileViewer.cs](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/GeoDax/GeoDaxFileViewer.cs).
The mappings were checked against installed resources and original screenshots.

The loader guards Phlan's `LOAD PIECES 127,127,127` at `ECL3:0/0x9B11`. Each
supported district's wall banks are loaded with it and checked against its
script's `LOAD PIECES` ([area transitions](MAPS.md#area-transitions)). Special-value
semantics, exact dynamic backgrounds and all distant occlusion cases remain
unverified.
