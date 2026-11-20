# Combat sprite scale demo

This standalone review scene belongs to the existing `demos/godot` project and
its C++ `opengold_godot` extension. It uses the original asset decoders and
character customization without launching or extending the game application.

## Launch

Build the demo extension, then launch the demo. From macOS bash at the repository
root:

```bash
cmake -S . -B build/sprite-demo -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DOPENGOLD_BUILD_GODOT=ON -DOPENGOLD_BUILD_GAME=OFF
cmake --build build/sprite-demo --target opengold_godot -j 8
bash demos/review-sprites.sh
```

The launcher locates Godot on PATH or in its standard macOS application folder;
`GODOT_BIN` can select another executable. On Windows, run
`demos\review-sprites.cmd`; it builds and checks the demo extension before
opening the scene so an older DLL cannot hide `CombatSpriteDemo`.

The demo finds the original game files as the game does
([TECH.md](TECH.md#finding-the-games-folders)); `OPENGOLD_GAME_DIR` names a
folder to look in first. It has no game startup or settings flow.
You can also launch the scene directly:

```bash
godot --path demos/godot --resolution 1920x1080 res://scenes/combat_sprite_demo.tscn
```

## Controls and measurements

- The map shows short and human players, two labeled Goliath comparisons,
  and five nearby monsters. All nine figures alternate ready/action every second.
- Head, weapon, and both banks of six region colors update all four players.
  Regions absent from both player sizes are disabled. The numbered palette has
  color name tooltips; buttons support keyboard focus and activation.
- **−100%, −10%, +10%, +100%** change zoom by percentage points, from 10% to
  1000%. The initial zoom is 250%. At 100%, one source pixel is one rendered
  viewport pixel. Map and sprites scale together; controls retain their size.
- Scrollbars and the mouse wheel pan the map. Zoom retains the map location at
  the center where the scroll limits allow it.
- The readout lists source and displayed pixel dimensions for every sprite.
  Player measurements also include the visible, nontransparent bounds for the
  current pose. At 250%, a normal 24 × 24 sprite occupies 60 × 60 pixels;
  both Goliaths have a visible height of 75 pixels. The stretched version is
  exactly 60 pixels wide; the proportional version's width follows the art's
  aspect ratio.
- **Ctrl+S** saves a timestamped PNG in `user://sprite-demo-screenshots`.
  Set `OPENGOLD_SCREENSHOT_DIR` to an absolute folder to choose another location.
  The demo exposes `request_capture()` and `capture_completed(path, error)` for
  automated visual checks, without loading the game's screenshot autoload.

## Goliath comparisons

Both Goliaths have a guide one square wide and two squares tall. Their visible
feet sit on the bottom edge, and their visible height is exactly **1.25 squares**:
one full lower square plus the bottom 25% of the upper square. That occupied
portion of the upper square is shaded, with a line at the intended top. Captions
and readout colors identify each version.

1. **Stretched:** scale width and height independently so the visible artwork
   fills one square's width and reaches the target height.
2. **Proportional:** use the same scale on both axes to reach the target height.
   Center the figure horizontally and allow wider artwork to extend beyond the
   guide. There is no width cap or clipping at the guide edges.

The **stretched** treatment was selected for the game. A Goliath occupies only
its lower square; monsters may occupy the square above. The game's combat
sprites render from lower rows to upper rows. See the approved
[occupancy and drawing rules](TECH.md#goliath-combat-sprites-and-draw-order).

Each pose is fitted from its own nontransparent bounds, so transparent padding
does not affect foot alignment or height. Head, weapon, and color changes apply
to both comparisons. These are visual sizing experiments within the demo.

## Assets and checks

Players reuse the original short/tall `CHEAD.DAX` and `CBODY.DAX` banks and the
same indexed recoloring as character creation. Both banks have a 24 × 24 canvas;
the short figure's visible pixels occupy less of it. Both Goliaths use the same
tall artwork with the two scaling treatments above. Monster dimensions retain
their original proportions.

The authored room uses the normal dungeon battlefield generator and locally
decoded `DUNGCOM.DAX` tiles. The five deliberate `CPIC2.DAX` art-review samples
are records 0, 2, 4, 26, and 31 (Kobold, Goblin, Orc, Basilisk, Troll), with 128
added for action poses. These selections do not change gameplay monster bindings.
Original artwork and screenshots are not included in the repository.

After building the demo extension, run:

```bash
ctest --test-dir build/sprite-demo -L demo --output-on-failure
```

The `opengold_godot_sprite_demo` check uses wholly synthetic archives. It checks
customization, transparency, both zoom step sizes and limits, Goliath heights,
bottom alignment, proportional width overflow, size readouts, the minimum window
layout, and synchronized one-second poses.
To review those checks with installed art and capture both poses:

```bash
OPENGOLD_GAME_DIR=/path/to/POOLRAD godot \
  --path demos/godot --script ../../tests/combat_sprite_demo_tests.gd \
  -- --installed --capture
```
