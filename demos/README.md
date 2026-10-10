# Reference demos

These are the existing OpenGoldBox demos and research views, preserved for
reference while the game is developed. They are not the game's entry point.
Change these files only when maintaining or explicitly working on a demo.

- `godot/`: demo project, scenes, scripts, shaders, and theme.
- `src/OpenGold.Godot/`: C++ GDExtension presentation used by the demos.
- `build-rolf.cmd`: builds the demo extension and runs the native tests.
- `review-*.cmd`: launch character/party, combat, Rolf/town, art, map, and sound demos.

From PowerShell at the repository root:

```powershell
.\demos\build-rolf.cmd
.\demos\review-character.cmd
```

The other launchers work the same way. Map inspection also needs the native
exporter built by `.\build.cmd`. Open `demos/godot/project.godot` for editor use.

The demos reuse the engine and rules in the root `src/`, authored assets in
`art/` and `data/`, and tests in `tests/`. Native inspection utilities remain in
`tools/`. Build output remains in `build/godot/`; the demo DLL and imported
portrait copies go to `demos/godot/bin/`. Captures and isolated test profiles
remain under the root `user-data/`. Original game files are still supplied by
the user through `OPENGOLD_GAME_DIR` or the demo project's existing setting.

The game now has a separate source copy in [src/OpenGoldBox](../src/OpenGoldBox/README.md).
See the root [README](../README.md) and
[feature documentation](../docs/CHARACTER-CREATION.md) for demo limitations.

The [combat sprite scale demo](../docs/SPRITE-DEMO.md) runs in this demo project.
Launch it with `bash demos/review-sprites.sh` on macOS/Linux or
`demos\review-sprites.cmd` on Windows after building the demo extension.

### Equipment sprite demo

Run `.\demos\review-equipment.cmd` from PowerShell after building the demo
extension. Double-click a weapon in the left list (or press Enter) to equip it.
Use the separate **Shield: On/Off** button to toggle the shield.
The **Equip** / **Unequip** buttons also remain available;
the enlarged **Ready** and **Action** previews update immediately. Arrow keys
select items, and Tab/Enter reach and activate the buttons.

The temporary inventory contains one of all 47 weapon types in the artwork
catalog (including Wand) and one Shield. Unequip the weapon for Unarmed or
shield-only previews. Equipping another weapon replaces the current weapon;
two-handed weapons and shields cannot be equipped together, in either order.
Compatible shields stay equipped during weapon swaps. A rejected swap leaves
the previous weapon, shield, and both previews unchanged.
The demo creates a temporary real `CampaignParty`. Its buttons call the same
`CampaignParty::equip` / `unequip` operations as the game, including SRD weapon
metadata, transactional swaps, hand limits, and profile validation. All 47
reviewer weapon types have rules conversions. The initial fixture inventory is
populated before joining the party, so the shop's 16-item purchase limit does
not restrict this review. No player saves or artwork assignments are changed.

Equipment rendering preserves the saved head, torso, clothing, legs, colors,
and size. The catalog supplies weapon-appropriate arms and equipment, not a
replacement character body. The game party preview, campaign/training combat,
combat showcase and this demo all call `ResolvedCombatAppearance::icon`.

The original game path uses `OPENGOLD_GAME_DIR` or the existing demo project
setting. Missing/deleted artwork mappings visibly report an unarmed fallback on the
saved body. Different weapons may share equipment artwork. Close with Windows X or Ctrl+X.

`opengold_godot_equipment_demo` tests all weapons and shield combinations with
authored fixtures. To check the real local artwork and capture the screen:

```powershell
godot --path demos/godot --script "$PWD/tests/equipment_sprite_demo_tests.gd" -- --original --capture
```

Use an absolute script path if launching outside the repository root. Captures
are written to `build/equipment-demo-screenshots/` and remain untracked.

## Relocation validation

The demo build and all 12 native tests pass. Headless character, training combat,
map inspector, and sound-board checks pass from this directory.
The standalone Rolf `--tour-check` starts and passes.
