# OpenGoldBox UI themes

The game project uses Godot's built-in Theme resource at `src/OpenGoldBox/godot/themes/opengold.tres`. `src/OpenGoldBox/godot/project.godot` assigns it as `gui/theme/custom`, so controls in every game scene inherit it. Theme edits are resource edits: they do not require recompiling the C++ GDExtension. Restart a running game after changing a `.tres` or `.tscn` file; in the Godot editor, saved resource edits can be previewed there.

## What goes where

| Change | Edit |
| --- | --- |
| Control location, width, height, anchors, or visibility | The corresponding `src/OpenGoldBox/godot/scenes/*.tscn` scene. Its Godot layout script or animation handles conditional placement. |
| Shared colors, font sizes, spacing, borders, padding, and control states | `src/OpenGoldBox/godot/themes/opengold.tres`. |
| A player preference such as combat zoom | Application settings, not the theme. |
| Portrait and sprite palette colors | Art/content data, not the UI theme. |

The theme defines normal, hover, pressed, disabled, selected, and keyboard focus styles for standard buttons and lists. Preserve all of these states when changing a control family. Give custom variations the same clear state coverage so buttons remain visibly actionable and keyboard focus remains visible.

## Theme variations

Scenes set `theme_type_variation` on controls. For example, the level-up scene uses `LabelText24` for its title and `VBoxContainerGap10` for grouped rows. `DialogSurface` supplies the background through a themed `Panel`. The character creation and combat scenes use the same shared variations for matching text sizes and surfaces.

Choose an existing variation when a control should change with its peers. Add a new named variation when its meaning or treatment differs. A variation declares its `base_type` and only the theme items it changes; Godot falls back to the base control's theme items for the rest. Keep scene-level `theme_override_*` properties out of shared styling, because they take precedence over the project theme and prevent a central change from affecting that control.

The `OpenGoldPalette/colors/*` entries supply named colors for custom drawing and state-dependent text. `OpenGoldMetrics/constants/*` holds sizes used by swatches, combat party rows, and character-sheet BBCode. For example, combat and map canvas drawing request colors by name; changing the palette entry changes the next game launch without recompilation. `scenes/combat_backdrop.gd` draws the combat surface from the scene's scroll bounds. `scenes/combat_party_rows.gd` places the party display using scene bounds and theme metrics; C++ supplies character data and art. The C++ view still decides *when* to draw a selected battlefield cell or a warning. The theme decides *what color* it has. `BuyerRow`, `ScoreNormal`, and `ScoreUnmet` are state-dependent variations selected by the view.

The separate review project at `demos/godot` has its own `themes/opengold.tres`, including a few variations for review-only scenes. Keep shared entries in that copy aligned with the game theme when changing them. The playable game's theme is the copy under `src/OpenGoldBox/godot`.

Some character appearance swatches must display a color chosen from the game's EGA art palette. Their fill is content data; `scenes/creation_swatch.gd` reads border, disabled, padding, and text contrast settings from the theme. `scenes/creation_score.gd` reads score colors from the theme. `scenes/creation_canvas.gd` draws the panels and previews at scene-authored bounds with theme colors. Edit those resources to change the styling without recompiling. Temporary animation opacity, such as the startup fade, remains controlled by runtime behavior.

## Editing and checking a change

1. Open `src/OpenGoldBox/godot/project.godot` in Godot and edit `themes/opengold.tres`, or edit the text resource directly. Change one theme item or variation at a time.
2. For position and size, edit the control's `.tscn` scene. Inspect its attached Godot layout script or animation if a scene value changes during play. Custom drawing, such as combat tiles and the exploration map, still calculates runtime positions in C++ from the scene bounds and theme metrics.
3. Save, restart the game or demo scene, and check normal, hover, pressed, disabled, selected, and keyboard focus states where relevant. Check long Spanish labels as well as English.
4. Run the project's relevant Godot UI checks from the repository root, for example `ctest --test-dir build-game --output-on-failure -R '^opengold_godot_(ui_audit|level_up_lab|screenshots|creation_character|combat_demo_scene|startup)$'`. Set `OPENGOLD_GAME_DIR=/path/to/POOLRAD` for checks that need the original assets. Use `ctest --test-dir build-game -N` to see the exact configured names. The full UI audit is described in `docs/UI-AUDIT.md` in the repository.

The theme is part of the game project and is included with the game's Godot resources. If using an exported build, rebuild the export to package revised resource files; the native C++ library need not be recompiled for a theme-only change.
