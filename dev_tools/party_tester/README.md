# Party tester

Tests levelling up and combat with the game's own code. Build a party from the
game's Character Pool, level everyone to a chosen level through the game's
level-up dialog, then fight monsters and NPCs you choose on an area's
battlefield, on the game's combat screen.

Run from the repository root:

```bash
OPENGOLD_GAME_DIR="$HOME/POOLRAD" dev_tools/party_tester/run.sh
```

`OPENGOLD_GAME_DIR` points to the original game files. Set `GODOT_BIN` if Godot
4.5 or newer is not at `/Applications/Godot_mono.app/Contents/MacOS/Godot`. The
launcher builds a separate GDExtension from the game's sources and stages the
game's Godot project, data and the tester's screen in `build/party-tester/project`.
It does not change the game's project. The first build compiles the whole game
and takes a few minutes.

## Party

- **Character Pool:** select one to eight of the pool's 48 characters (Cmd- or
  Ctrl-click to select several) and press **Build party**. The game's roster
  holds six player characters and two NPCs; in this tool all eight are player
  characters. Pool characters join with no equipment, as in the game, so each
  gets its class's kit, the one the game's combat demo uses: a weapon, the
  armor and shield it is trained with, a bow or crossbow and a torch.
- **Level up:** choose a level from 1 to 15 and press **Level up**. Every member
  below that level gets the experience for it, and the game's level-up dialog
  opens for the first member with a level to take. Each confirmed level opens
  the dialog again, for the same member's next level or the next member.
  **Use defaults for the rest** takes every remaining level with the game's
  default choices instead. To go to a lower level, build the party again.
- **Sheet** (or double-click a member) shows the game's character sheet.

## Fight

- **Area:** any area's map. An area with several maps lists each.
- **Square seed:** picks the square and facing on the map. The same seed gives
  the same battlefield. As in the game, the party faces open squares, and the
  game's own placement stands both sides, one exploration square apart.
- **Difficulty:** what the game's difficulty does to the monsters (Normal by
  default). The fight is not sized to the party: you get exactly the creatures
  you add.
- **Monsters and NPCs:** every creature an area's scripts load, named by area,
  with a count; add as many groups as you like, up to 56 creatures. A creature
  the scripts draw with more than one icon is listed once per icon. A creature
  that fights for the party, such as Valhingen Graveyard's efreeti, joins the
  party's side.
- **Fight** opens the game's combat screen. Once the fight ends, **Return to
  setup** goes back. Each fight is fought by a copy of the party, so the party
  starts every fight levelled, at full Hit Points and with every resource.

A few creatures are not listed: the scripts load them only from a variable,
as wandering groups are, so the game knows their combat icon only while a
script runs, and their record number has no icon of its own. The screen names
them when it opens.

## Check

```bash
OPENGOLD_GAME_DIR="$HOME/POOLRAD" dev_tools/party_tester/check.sh
```

The headless check uses the screen's own controls. It builds a party of eight,
levels one member through the dialog and the rest by defaults, and plays a
Slums fight to the end with Quick combat. It then checks that the party is
unchanged afterwards, and that every listed creature can be put in a fight.

## Level sweep

```bash
OPENGOLD_GAME_DIR="$HOME/POOLRAD" dev_tools/party_tester/sweep.sh
```

The sweep (`godot/level_sweep.gd`) takes 444 characters from level 1 to 15
through the game's level-up dialog, as a player does:

- every pool character in its own class;
- every pair of classes, the second class first taken at level 2, at level 9
  and at level 15. Each pair uses the pool character of the first class
  nearest to the 13s both classes need. Where a score is short, the
  character's Ability Score Improvements raise it before the second class.

On each level it picks from the Class row, the feats or ability points,
training, Fighting Styles, the checkbox pages, a replaced cantrip, spell or
invocation on every fourth level, and the spell pages. The picks vary from
character to character and level to level. After every level it checks the
sheet against SRD 5.2.1's tables:

- Hit Points: the starting class's whole die, then each level's class
  average, with the Constitution modifier, a Dwarf's Toughness and Draconic
  Resilience;
- the Proficiency Bonus by total level;
- spell slots: one class's own table, or the Multiclass Spellcaster table with
  Paladin and Ranger levels halved and rounded up, plus Pact Magic;
- Hit Dice by class, as the sheet shows them ("8d12 + 1d10").

Each party of eight fights the Slums' orcs and then Valjevo Castle's fire
giant and trolls at levels 1, 5, 10 and 15, on Quick, until the fight ends.

It fails on a level the dialog will not confirm, a sheet that disagrees with
the tables, or a fight that does not end in four minutes. Two picks the dialog
refuses together and names, such as a blast invocation whose cantrip is
swapped away, are counted, not failed, and the level is taken again with the
first choice of everything. So is a spell choice the game leaves pending for
want of spells (the Cleric's spells decision in `docs/SRD-DECISIONS.md`).

The full sweep takes about 5 minutes without fights. Settings:

- `OPENGOLD_SWEEP_FIGHTS=N`: fight with every Nth party; 0 for none.
- `OPENGOLD_SWEEP_PLANS=N`: stop after N characters.
- `OPENGOLD_SWEEP_MATCH=text`: only the characters whose plan names it.
- `OPENGOLD_SWEEP_PARTY=N`, `OPENGOLD_SWEEP_FROM=N`, `OPENGOLD_SWEEP_UNTIL=N`:
  one party or a range of them; each failure names its party.
- `OPENGOLD_SWEEP_VERBOSE=1`: print each page's picks.

## Code

The tester runs the game's code, not a copy of it:

- the game's Godot classes (`register_game_classes`);
- the combat screen (`campaign_combat_scene`);
- the pool (`character_pool`, `add_pool_member`, `outfit_pool_member`);
- the level-up dialog;
- the area data (`RolfTourSession::load`);
- creatures staged as the scripts stage them (`stage_area_creature`);
- the difficulty (`apply_difficulty`).

The battlefield search, `dev_tools/shared/seeded_battlefield.cpp`, is shared
with the Combat tester. `CampaignParty::open_npc_positions_to_player_characters`
lets the party hold eight player characters. The game never calls it, and the
game's own check refuses such a party.

The icon is drawn by `tools/make_icon.py`.
