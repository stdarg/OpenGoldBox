# OpenGoldBox UI themes

The game project uses Godot's built-in Theme resource at `src/OpenGoldBox/godot/themes/opengold.tres`. `src/OpenGoldBox/godot/project.godot` assigns it as `gui/theme/custom`, so controls in every game scene inherit it. Theme edits are resource edits: they do not require recompiling the C++ GDExtension. Restart a running game after changing a `.tres` or `.tscn` file; in the Godot editor, saved resource edits can be previewed there.

## What goes where

| Change | Edit |
| --- | --- |
| Control location, width, height, anchors, or visibility | The corresponding `src/OpenGoldBox/godot/scenes/*.tscn` scene. Its Godot layout script or animation handles conditional placement. |
| Shared colors, font sizes, spacing, borders, padding, and control states | `src/OpenGoldBox/godot/themes/opengold.tres`. |
| A player preference such as combat zoom | Application settings, not the theme. |
| Character swatch colors | `OpenGoldPalette/colors/character_palette_0` through `_15` in the theme. The sprite art itself keeps the original game palette. |

The theme defines normal, hover, pressed, disabled, selected, and keyboard focus styles for standard buttons and lists. Preserve all of these states when changing a control family. Give custom variations the same clear state coverage so buttons remain visibly actionable and keyboard focus remains visible.

## Theme variations

Scenes set `theme_type_variation` on controls. For example, the level-up scene uses `LabelText24` for its title and `VBoxContainerGap10` for grouped rows. `DialogSurface` supplies the background through a themed `Panel`. The character creation and combat scenes use the same shared variations for matching text sizes and surfaces.

Choose an existing variation when a control should change with its peers. Add a new named variation when its meaning or treatment differs. A variation declares its `base_type` and only the theme items it changes; Godot falls back to the base control's theme items for the rest. Keep scene-level `theme_override_*` properties out of shared styling, because they take precedence over the project theme and prevent a central change from affecting that control.

The `OpenGoldPalette/colors/*` entries supply named colors for custom drawing and state-dependent text. `OpenGoldMetrics/constants/*` holds sizes used by swatches, combat party rows, and character-sheet text. Combat and map canvases request colors by name; changing a palette entry changes the next game launch without recompilation. `scenes/combat_battlefield.gd`, `scenes/tour_visuals.gd`, and `scenes/creation_canvas.gd` calculate their drawing geometry in Godot. `scenes/combat_party_rows.gd` places the party display using scene bounds and theme metrics; C++ supplies character data and art. `scenes/themed_rich_text.gd` resolves semantic score and HP colors, character-sheet font sizes, and table header spacing from the theme (`sheet_header_pad_chars`). `BuyerRow`, `ScoreNormal`, and `ScoreUnmet` are state-dependent variations selected by Godot scripts.

The separate review project at `demos/godot` has its own `themes/opengold.tres`, including variations for review-only scenes. Keep shared entries in that copy aligned with the game theme when changing them. The playable game's theme is the copy under `src/OpenGoldBox/godot`. The review project's combat, tour, creation, party, level-up, equipment, and sound controls now get their rectangles from its scenes and attached Godot scripts. For example, `demos/godot/scenes/legacy_combat_layout.gd` handles battlefield-dependent placement, while `demos/godot/scenes/combat_sprite_canvas.gd` sizes art previews from theme metrics. Edit those resources to adjust the review UI without recompiling the extension.

Choice groups created when a character's options change use scenes in each project's `scenes/control_templates` directory. `spell_rows.tscn`, `spell_group.tscn`, `training_group.tscn`, and `training_replacement_rows.tscn` hold their container layout and label wrapping. Keep the game and review copies aligned when changing shared controls.

Character appearance swatches use the theme's 16 `character_palette_*` entries, initially matched to the EGA art palette. `scenes/creation_swatch.gd` also reads border, disabled, padding, and text contrast settings from the theme. `scenes/creation_score.gd` reads score colors from the theme. `scenes/startup_fade.gd` applies the theme tint and fade duration. Edit those resources to change the styling without recompiling.

## Editing and checking a change

1. Open `src/OpenGoldBox/godot/project.godot` in Godot and edit `themes/opengold.tres`, or edit the text resource directly. Change one theme item or variation at a time.
2. For position and size, edit the control's `.tscn` scene. Inspect its attached Godot layout script or animation if a scene value changes during play. Combat tiles, sprites, and the exploration map calculate runtime positions in Godot scripts from scene bounds and theme metrics. Dialog root rectangles set window size through `dialog_window_size.gd`.
3. Save, restart the game or demo scene, and check normal, hover, pressed, disabled, selected, and keyboard focus states where relevant. Check long Spanish labels as well as English.
4. Run the project's relevant Godot UI checks from the repository root, for example `ctest --test-dir build-game --output-on-failure -R '^opengold_godot_(ui_audit|level_up_lab|screenshots|creation_character|combat_demo_scene|startup)$'`. Set `OPENGOLD_GAME_DIR=/path/to/POOLRAD` for checks that need the original assets. Use `ctest --test-dir build-game -N` to see the exact configured names. The full UI audit is described in `docs/UI-AUDIT.md` in the repository.

For review scenes, build `opengold_godot` in a demo-enabled CMake build, then launch the relevant `.tscn` under `demos/godot/scenes`. The review project's tests include `tests/combat_sprite_demo_tests.gd` and `tests/equipment_sprite_demo_tests.gd`; the creation, advancement, party, combat, tour, town, defeat, and sound-board scenes also expose their `--*-check` command-line checks. This review project has its own theme, so edit its copy for those screens.

The theme is part of the game project and is included with the game's Godot resources. If using an exported build, rebuild the export to package revised resource files; the native C++ library need not be recompiled for a theme-only change.
