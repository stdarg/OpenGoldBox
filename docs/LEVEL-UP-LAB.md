# Level-up review app

The review app is a separate entry scene in the **game** Godot project. It uses
the same `LevelUpDialog`, `CampaignParty`, and injected SRD rules module as the
game. It needs the built GDExtension and copied rules pack, but it does not need
original *Pool of Radiance* files or change player saves.

On macOS, from the repository root after configuring the game build:

```bash
cmake --build build-game --target opengoldbox_test_project -j6
/Applications/Godot_mono.app/Contents/MacOS/Godot --path src/OpenGoldBox/godot res://scenes/level_up_lab.tscn
```

In the Godot editor, open `src/OpenGoldBox/godot/project.godot` and run
`scenes/level_up_lab.tscn` directly. Other platforms use their installed Godot
4.x executable with the same `--path` and scene arguments after building the
game GDExtension.

Choose any offered class, race, and gender, plus a current level from 1 through
3. Enter a name or leave the default. **Create Character** generates complete,
rules-valid starting choices, grants enough XP for level 4, and advances through
earlier levels using the rules module's default choices. **Open Level-Up** opens
the production dialog. **Cancel** changes nothing; **Confirm** applies one level
to the isolated character and refreshes its summary. Create another character
to test a different route. The app keeps no saves.

## Editing the dialog

- `src/OpenGoldBox/godot/scenes/level_up_dialog.tscn` owns the window and fixed
  control rectangles. Edit it in the Godot scene editor or as text. Four hidden
  position markers define the training selector's two state-dependent positions.
- `src/OpenGoldBox/godot/themes/opengold.tres` owns shared control colors and the
  `LevelUpChoice` styles for generated choice rows. `LevelUpChoice` also exposes
  their minimum row height; `LevelUpSpellSection` exposes row spacing.
- `src/OpenGoldBox/godot/scenes/level_up_lab.tscn` owns the review app layout.

Scene and theme edits appear on the next run of the Godot project without
recompiling C++. An exported game packs those resources and needs a new export
to distribute edits. The proof check changed the dialog title position from
24 to 35, its width from 652 to 665, and the normal button red component from
0.1608 to 0.5000. The same native library hash was used before and after; the
reviewed values were then restored.

`opengold_godot_level_up_lab` exercises all 12 × 9 × 3 × 3 combinations of class,
race, gender, and starting level, plus Cancel, Wizard pages, and Fighter
confirmation. `opengold_godot_ui_audit` covers the review app and dialog in
English and Spanish at 1120×800, 1600×1000, and 1920×1080. The game's existing
advancement check remains a separate production-path check.
