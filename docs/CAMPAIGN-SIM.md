# Campaign-arc simulator

`opengold_campaign_sim` estimates how often a party gets through the Slums.
Each run is one seed: a level-one party from the curated character pool, in its
class starting kits, plays a fixed twelve-fight arc and gains levels up to four.
Both sides use the automated demo policy (`choose_demo_command`).

```sh
build/opengold_campaign_sim /path/to/POOLRAD 50 user-data/campaign-sim [PARTY]
```

Arguments: the original game directory, runs per party (default 20), the
output directory (default `user-data/campaign-sim`) and optionally one party
name.

## The arc

Fights, in order: street kobolds, the four-orc search event, street goblins,
street orcs, guild kobolds, street goblins, guild goblins, street orcs, guild
orcs, street goblins, guild orcs, guild orcs. Roaming groups follow the
original ECL2:20 mix from the party's strength (as
`tools/encounter_balance.cpp` documents it) and are fitted to the XP budget at
the default encounter challenge and to one creature per living member, as the
campaign session does. Each living member gains the original encounter's
experience.

After a victory, dying members make their death saves and the Stable regain a
Hit Point over four hours. Members level up with the default choices, with
spells the policy uses well (Moonbeam, Spiritual Weapon, Hex, Healing Word and
so on) swapped in where the rules accept them. The party takes a Short Rest
when anyone is below half Hit Points and a Long Rest every third fight or when
still below half; rests are never interrupted. A defeat ends the run; dead
members stay dead.

## Parties

`classic` (two fighters, cleric, rogue, wizard, paladin), `martial`, `casters`,
and `six-<class>` for each class.

## Output

- `runs.csv`: one row per run: outcome (`complete`, `defeated` or `stalled`),
  fights won, the fight lost, deaths and the classes that died, rests taken and
  final levels.
- `summary.csv`: per party: success rate, flawless rate (complete with no
  deaths), average fights won, deaths and level.
- `usage.csv`: how often each class chose each command, to check the policy
  uses the class features.
