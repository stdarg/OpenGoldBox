# UI audit

`tests/ui_audit.gd` checks the game's screens for broken layout, the problems a
player sees but a feature test does not look for. `tests/ui_audit_checks.gd`
holds the checks, so a play-test can audit any screen it reaches.

For every visible control on a screen it reports:

- **overlap**: two controls that show text or take input cover each other,
  measured where a label actually draws its text, not its whole box;
- **text cut**: a label, button or non-scrolling text area too small for its
  text;
- **off-window**: a control drawing outside the window;
- **no room**: a scrolling text area (the combat log) shorter than one line.

Overlays opened on purpose (the hover panel, menus, dialogs) are left out of
the overlap check. A screen opened over another (the party, the town, a
campaign fight) is audited alone, since the one beneath stays in the scene
tree but out of sight.

Each screen is audited at 1120×800 (the game's minimum window), 1600×1000 and
1920×1080, in English and Spanish. On success the audit prints the screens it
covered, so a run that silently skipped one cannot pass unnoticed.

## Screens

- Always: the startup screen and the combat screen, empty and in the play-test
  fights (`gear-torch`, `gear-ally`, `bandage`, `morale-panic`, `flee`), and
  on a member's turn the computer plays (Quick). The isolated level-up review
  app, its character sheet, save/load dialogs, first level-up page, and Wizard
  spell-choice page are also audited.
- With the original files (`OPENGOLD_GAME_DIR`): each character-creation page
  the audit can reach by taking the first choice (through Training), and the
  party.
- With Slums saves (`--slums-fixtures` or `OPENGOLD_SLUMS_FIXTURES`): the town,
  the hobgoblins' story choice and their fight on a party turn, then under Quick.

## Running

CTest runs `opengold_godot_ui_audit` with every build and
`opengold_godot_slums_ui_audit` when `OPENGOLD_GAME_DIR` is set; the latter
writes fresh Slums saves with `opengold_expedition_tests` first. By hand, with
each screen's control tree written as JSON for inspection:

```sh
godot --headless --path src/OpenGoldBox/godot --script $PWD/tests/ui_audit.gd -- \
    --playtest-fixtures=$PWD/build/playtest-fixtures --audit-out=/tmp/ui-audit
```

## Screenshots before and after a change

`tools/ui_snapshots.py` keeps a screenshot of every screen the audit covers and
compares two sets, to catch visual changes nobody asked for:

```sh
python3 tools/ui_snapshots.py capture before --game-dir /path/to/POOLRAD
# change the game and rebuild
python3 tools/ui_snapshots.py capture after --game-dir /path/to/POOLRAD
python3 tools/ui_snapshots.py compare before after
```

`capture` runs the audit in a window with fresh saves (character creation with
`--fixed-seed`, so its rolls repeat). `compare` (`tests/ui_compare.gd`) lists
each screen that changed and by how much, and any that appeared or went, and
writes for each changed screen the new screenshot dimmed with the changed
pixels in red. The screenshots show the original game's art, so they stay in
`user-data/ui-snapshots` and never go in the repository.
