# OpenGoldBox splash artwork provenance

## Current game presentation - 2026-09-15

The game targets **1920 x 1080**. Both splash screens use exactly the same
`OpenGoldBoxSplashBackground.png` texture, loaded once by `StartupView`.
Only the transparent lettering overlay changes when the user advances. This guarantees
pixel-identical background rendering without independent AI variations.

- `OpenGoldBoxSplashBackground.png`: the shared, text-free background, generated
  with the built-in image tool. Actual source dimensions: 1672 x 941. The tool
  did not produce the requested 1920 x 1080 raster. This file is unmodified;
  Godot fits it uniformly to the display, preserving natural proportions.
- `OpenGoldBoxSplash.png`: 1920 x 1080 capture of the final engine screen.
- `OpenGoldBoxPoolOfRadiance.png`: 1920 x 1080 capture of the final game screen.
  These two files are review snapshots. Runtime uses the shared background
  and transparent lettering overlays, not separate screen textures.

The engine screen reads "OpenGoldBox" and "An open-source role-playing game
engine for Gold Box games." The game screen reads "POOL OF RADIANCE",
"An unofficial adaptation powered by OpenGoldBox", and "Not affiliated with
or endorsed by Wizards of the Coast."

Typography uses generated transparent overlays, matching the user's reference
`exec-ea5a6b9d-cb6e-4113-b9c6-366dda33d13c.png`: textured metallic gold,
beveled rims and dark shadows, with slender ivory supporting text.
`OpenGoldBoxEngineLettering.png` and `OpenGoldBoxGameLettering.png` are both
1672 x 941 RGBA PNGs, copied unchanged from the built-in generator. Godot
fits them proportionally with comfortable margins over the existing backdrop.
No system fonts or local image resampling are used.
See [lettering prompts](OpenGoldBoxScreens.lettering-prompts.md) for the exact
prompts and source filenames.

Spanish uses separate `.es.png` lettering overlays and the same unchanged
background. The approved Spanish game title is **Estanque de Resplandor**.
See [Spanish prompts and hashes](OpenGoldBoxScreens.spanish-prompts.md) and
[localization architecture](../docs/LOCALIZATION.md).

See [generation prompts](OpenGoldBoxScreens.fullhd-prompts.md) for the exact
fresh composition and text-removal prompts. The design continues the earlier
user-supplied `art/TitleScreen.png` direction: stone wall, left torch and red
banner, dragon shadow, blue water, and warm/cool lighting.

The earlier independently generated full-screen images were superseded to meet
the user's requirement that only the text change between screens. The rendered
startup check hides the text and compares the entire background pixel buffer
across both screens.

The five runtime PNGs are tracked in the repository. The build copies the
shared backdrop and lettering overlays into the game package. Background rights and
trademark clearance have not been independently verified; provenance is not
legal clearance.

`OpenGoldBoxIcon.png`, the app icon, is the dragon-shadow corner of
`OpenGoldBoxSplashBackground.png` cropped to a square, enlarged to 824 x 824 and
given rounded corners on a transparent 1024 x 1024 canvas, the margin macOS
expects. It carries the same unverified rights status as the background.
