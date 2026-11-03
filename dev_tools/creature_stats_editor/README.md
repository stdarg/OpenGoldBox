# Creature Stats Editor

Launch the Godot project in `godot/` or run:

```bash
/Applications/Godot_mono.app/Contents/MacOS/Godot --path dev_tools/creature_stats_editor/godot
```

The current screen searches Combat Sprite Catalog creature records by name,
record key or gear. It shows the catalog description, equipment and ready/action
combat art for each variant. The icon is specific to this tool.

This is an initial read-only screen. Combat values, map encounters, wandering
chance controls, diffs and saves require the game-owned queries and overrides
listed in `CLAUDE_PREREQUISITES.md`. The screen does not claim that catalog
metadata is the game's effective stat block or encounter state.
