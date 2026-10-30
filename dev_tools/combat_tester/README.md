# Combat tester

Run from the repository root:

```bash
OPENGOLD_GAME_DIR="$HOME/POOLRAD" dev_tools/combat_tester/run.sh
```

`OPENGOLD_GAME_DIR` points to the original game files. Set `GODOT_BIN` if Godot 4.5 or newer is not at `/Applications/Godot_mono.app/Contents/MacOS/Godot`. The launcher builds a separate GDExtension from the game's own C++ combat view and Core sources, stages the game's Godot scene and rules, then opens the setup screen. It does not change the main game project.

Choose one of the 29 game maps, a monster type and count, optional leaders where available, and a seed. The seed repeats map placement. Each fight starts with full HP and resources. The party uses the game's six member formation, with the strongest AC/HP members closest to the enemies. Set Quick on a member or the whole party; Quick magic is a separate switch. Combat uses the game's combat screen. The result button returns to setup with the session edits intact.

The 15 presets in `config/parties.json` have six members each, one preset at every level from 1 through 15. A preset's classes were randomly chosen once and saved. Editing a member affects the next fight. **Save party** writes that preset to the JSON file; **Restore preset** discards unsaved edits to that preset. Class kit defaults come from the game. You can edit class, level, species, name, base scores, weapons, armor, shield, other gear, starting cantrips and learned spells, and prepared spells. The subclass label reflects the game's default subclass; the current rules implementation offers one subclass per class. Starting spell groups and prepared spells must satisfy the game's choice rules. **Restore class kit** resets equipment and spell overrides for that member.

After launching once, focused checks can run against the staged project:

```bash
export OPENGOLD_GAME_DIR="$HOME/POOLRAD"
export OPENGOLD_COMBAT_TEST_CONFIG="$PWD/dev_tools/combat_tester/config/parties.json"
/Applications/Godot_mono.app/Contents/MacOS/Godot --headless --path build/combat-tester/project --script res://combat_tester/test_smoke.gd
/Applications/Godot_mono.app/Contents/MacOS/Godot --headless --path build/combat-tester/project --script res://combat_tester/test_maps.gd
/Applications/Godot_mono.app/Contents/MacOS/Godot --headless --path build/combat-tester/project --script res://combat_tester/test_quick.gd
/Applications/Godot_mono.app/Contents/MacOS/Godot --headless --path build/combat-tester/project --script res://combat_tester/test_save.gd
```
