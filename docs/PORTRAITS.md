# Portraits in the view window

The view window is the 3D scene panel in the exploration screen
(`RolfTourView`). Beside the corridor view it shows three kinds of picture:

- complete NPC portraits, such as Rolf's;
- the original game's head-and-body portraits;
- the animated monster close-ups shown before an encounter's combat.

This document gives the window's size for anyone making portrait art, and
describes how each kind of picture reaches the screen.

Character portraits in creation, the party panel and the pool are a separate
system. They are square 1254x1254 PNGs in `art/portraits/`, described in
[CHARACTER-CREATION.md](CHARACTER-CREATION.md) and
`art/portraits/README.md`.

## Size of the view window

The original view is **88x88 EGA pixels**. DOS displayed those pixels 6/5 as
tall as wide, so the window keeps that shape on screen: **88 wide by 105.6
tall, a 5:6 (width:height) rectangle**.

`RolfTourView::layout()` sizes the panel as follows:

```text
view_height = min(main_width * 0.625, window_height - 410)   (490 while shopping)
view_width  = view_height / 1.2
```

`draw_scene()` then fits the 88x88 view into that panel with a continuous
scale and nearest filtering. At 1920x1080 the window is about 558x670 screen
pixels.

**Make view-window portraits 5:6.** The standard size is **1045x1254**. It is
the full height of a 1254x1254 generated square, with 104 px trimmed from the
left and 105 px from the right. It also gives about twice the resolution the
window needs at 1080p. Any other 5:6 size also works; the picture is stretched
to exactly fill the window.

A square image does not fit. Scaled to the window's width, a square leaves
strips of the corridor view visible above and below it. That is why Rolf's
portrait was cropped.

Image generators usually produce squares. When you generate one:

- Compose the subject in the middle five-sixths of the width.
- Crop it to 1045x1254.
- Record the crop next to the prompt, as `art/portraits/NPCs/prompts.json`
  does, so that regenerating the image does not bring the square back.

## Complete NPC portraits

This kind of portrait is new to OpenGoldBox. Rolf's is the first.

| Item | Location |
| --- | --- |
| Source | `art/portraits/NPCs/rolf.png` (1045x1254 RGB) |
| Name-to-file map | `art/portraits/NPCs/portraits.json` (no code reads it yet) |
| Generation prompts, style reference and crop | `art/portraits/NPCs/prompts.json` |
| Install rule | `src/OpenGoldBox/CMakeLists.txt` copies the file to `godot/bin/portraits/NPCs/rolf.png` |
| Loaded by | `RolfTourView::_ready()`, from `res://bin/portraits/NPCs/rolf.png` |

`draw_scene()` draws the portrait over the whole view rectangle. It is not
stretched 1.2 again, because the art is already made in the window's on-screen
shape.

The portrait appears when Rolf has walked up to the party and speaks. In
tour-state terms, that is `sprite_frame == 0` (his nearest sprite pose) with
dialogue showing and the tour not finished. At every other moment the small
encounter sprite is drawn instead.

For now each NPC portrait needs its own install rule and loading code. Add a
general table only once a second NPC needs one.

## Heads and bodies (legacy)

The original game builds portraits from two parts stacked vertically:

- a **head**, 88x40 pixels, from `HEAD1..8.DAX`;
- a **body**, 88x48 pixels, from `BODY1..8.DAX`.

The head's 40 rows sit directly on top of the body's 48 rows, with no overlap,
to make one 88x88 picture. That picture is the full view window and gets the
same 1.2 vertical stretch as the corridor view. Each part is a 16-color EGA
picture record, described in [graphics-format.md](graphics-format.md).

These parts are **being retired**. New art for the view window should be
complete 5:6 portraits, like Rolf's. The parts are still used in two places.

### Original story portraits in the view window

The original scripts show a picture in the view with ECL host op 14
(`phlan_session.cpp`).

- If variable `0x6DE1` is 255, the op shows a complete `PIC` picture from the
  area.
- Otherwise `0x6DE1` names the head and the op's argument names the body. The
  session stacks them into one 88x88 image.

Each area loads its own archives (`rolf_tour.cpp`). The town uses
`HEAD3`/`BODY3`/`PIC3`, and the Slums use `HEAD2`/`BODY2`/`PIC2`.

When a character from the original story gets a complete portrait, draw it in
place of that head-and-body picture, as Rolf's replaces his sprite.

### Player character appearance

`CharacterAppearance` (`character_art.h`) still stores `portrait_head` and
`portrait_body`. Ids 0-255 are original parts. Ids 256-265 are the extra
OpenGoldBox heads for Gnome, Orc, Goliath, Tiefling and Dragonborn:

- Their source PNGs are in `data/art/portraits/`.
- `prepare_portrait_head()` shrinks each one to 88x40.
- `fit_neck()` shifts and resizes its bottom rows to meet the neck of an
  original body.

Character creation no longer offers these parts. Characters use the complete
`appearance.portrait` PNG instead. `CharacterArt::portrait()` still stacks the
parts into an 88x88 image. That image is only a fallback in the combat roster,
used when a character has no loadable complete portrait.

The fields remain in saves for compatibility. Remove them, with a save-format
migration, once nothing reads them.

The small combat icons use their own parts: `CHEAD`/`CBODY`, 24 pixels wide,
with the head drawn over the body. They are not portraits and are not being
retired.

## Monster encounter close-ups

An encounter shows the monster in two stages.

**1. Approach sprite.** The script's `SETUP MONSTER sprite, distance, picture`
(ECL host op 12) loads three `SPRIT` sprites. They are separate near, middle
and far drawings, not animation frames. The session shows the one for the
given distance. `draw_scene()` draws it centered and resting on the bottom of
the view, at the view's pixel scale. Palette index 0 is transparent.

**2. Close-up.** The op's third operand names a `PIC` animation record. When
the encounter reaches COMBAT and that record exists, the session raises
`showing_monster_picture` and holds combat back (`pending_encounter()` is
empty). The view:

- builds one texture per frame (`sync_monster_picture()`);
- draws the current frame over the whole 88x88 view, with the same 1.2
  stretch, and draws nothing else on top;
- starts combat when the player presses any key.

A `PIC` animation record holds a frame count, then for each frame a 32-bit
delay and one EGA picture. All frames are 88x88 and opaque:

- Frame 0 is a complete picture.
- Every later frame is an XOR delta against **frame 0**, not against the
  previous frame.

`decode_ega_animation()` in `formats.cpp` reads the record.

Delays count ticks of the PC timer, 18.2 per second. `current_monster_frame()`
loops through the frames by their delays and skips frames with zero delay. The
Slums orc, `PIC2` record 4, has four frames with delays 15, 0, 7 and 0. It
holds its ready pose for about 0.82 s and its strike for about 0.38 s, over and
over.

Only the Slums load animations today (`PIC2.DAX`). No table maps monsters to
pictures: the script's operand is the only link. Records 1 (treasure chest)
and 29 (campfire) appear in every `PIC` archive.

Replacement close-up art follows the view-window size above: 5:6 frames that
fill the window. Every frame must be the same size.
