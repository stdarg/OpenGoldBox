# Combat demo

From PowerShell, after building the Windows package:

```powershell
.\win-package\opengoldbox.exe -- --combat-demo
```

The normal language and game-folder setup runs first. The demo then opens the
same campaign combat screen and encounter handoff with six level-one heroes:
Fighter, Paladin, Cleric,
Ranger, Rogue, and Bard. The Fighter is a Goliath. Each hero has a class-trained
weapon and armor equipped. Fourteen Kobolds occupy every square of the outer
ring around the compact party formation. The demo uses the SRD 5.2.1 combat
rules, the portable `combat_zoom` setting, and art decoded at runtime from the
player's installed Pool of Radiance files. No original assets are included in
the repository or package.

Use the existing combat buttons and click highlighted targets. Every class's
level 1-4 features are available: class actions are in the A-key action cycle
and the Bonus Action dropdown, and Space uses the selected action.

## Play-testing a chosen party

Two optional flags replace the six showcase heroes with up to six pool
characters of chosen classes, in their starting kits, at a chosen level (1-4):

```sh
opengoldbox -- --combat-demo --combat-demo-party=druid,warlock,sorcerer,monk,barbarian,bard --combat-demo-level=4
```

A repeated class takes that class's next pool character.

`tests/playtest_capture.gd` plays such a demo without a person: on each party
member's first turn it saves a screenshot and lists the Bonus Action dropdown,
the Spell dropdown and the A-key action cycle in `report.txt`; it also tries a
Druid's Wolf form and screenshots the first reaction prompt. It needs a window
(not `--headless`) and the original game folder:

```sh
OPENGOLD_GAME_DIR=/path/to/POOLRAD godot --path src/OpenGoldBox/godot \
    --script tests/playtest_capture.gd -- --combat-demo \
    --combat-demo-party=wizard,cleric,paladin,ranger,fighter,rogue \
    --combat-demo-level=4 --playtest-out=/tmp/playtest
```
