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
Gear a character can use right now (Torch attack, thrown Oil, Alchemist's Fire
and Acid with how many are left, Shoot, Take off shield) is also listed in an
Items row above the log: Use on an action aimed at a creature selects it for a
click, and Use on Take off shield acts at once. The prompt says what finishes
the selected action: click a highlighted square to move, click a highlighted
creature for an aimed action, or press Space for one without a target.
Clicking another party member shows it ("It is not ...'s turn"); choosing an
action returns to the character whose turn it is, so the next click acts.
Moving a party member off the edge of the field with an arrow key flees the
fight, as in the original; whether it gets away depends on the enemies' speed
([FLEE-1](SRD-DECISIONS.md#flee-1-2026-10-08-fleeing-a-fight-as-the-original-game-did)).
**Flee**, at the right of End turn's row, hands the whole party to the game AI:
those who can run for the nearest edge and off it; the rest fight on.

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

Two more flags set up gear play-tests: `--combat-demo-enemies=troll,ogre`
replaces the kobold ring with those combat definitions (in a row east of the
party, with their original Slums icons), and
`--combat-demo-gear=oil,alchemists_fire,acid` gives each member one of each.

`tests/playtest_gear.gd` plays the gear added against trolls
([GEAR-1](SRD-DECISIONS.md#gear-1-2026-10-08-fire-and-acid-for-every-party)):
from the `gear-torch`, `gear-flasks` and `gear-bow` saves it strikes with a
Torch, throws Oil, Alchemist's Fire and Acid, takes off a shield and shoots.
With `--campaign` and the flags above it plays the real campaign screen
instead, throws Alchemist's Fire at the troll and checks that hovering over the
troll shows it Burning:

```sh
godot --path src/OpenGoldBox/godot --script tests/playtest_gear.gd -- \
    --playtest-fixtures=build/playtest-fixtures --playtest-out=/tmp/playtest-gear
OPENGOLD_GAME_DIR=/path/to/POOLRAD godot --path src/OpenGoldBox/godot \
    --script tests/playtest_gear.gd -- --combat-demo \
    --combat-demo-party=fighter,fighter,wizard,cleric --combat-demo-level=4 \
    --combat-demo-enemies=troll,ogre --combat-demo-gear=oil,alchemists_fire,acid \
    --campaign --playtest-out=/tmp/playtest-gear-campaign
```

The hover panel lists a creature's conditions (Burning, Prone and the rest)
under its weapon.

`tests/playtest_actions.gd` plays aimed and targeted actions through the same
controls, using saves that `opengold_playtest_fixtures` writes to
`build/<preset>/playtest-fixtures`: Moonbeam (aim, cast, move), Spike Growth,
Land's Aid, Produce Flame and its hurl, Flame Blade, Heat Metal and its
repeat, and Bardic Inspiration from the Bonus Action dropdown. It screenshots
each step and reports what the combat log shows:

```sh
godot --path src/OpenGoldBox/godot --script tests/playtest_actions.gd -- \
    --playtest-fixtures=build/default/playtest-fixtures --playtest-out=/tmp/playtest-actions
```
