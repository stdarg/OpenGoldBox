# Campaign-arc simulator

`opengold_campaign_sim` estimates how often a party gets through the Slums and
then Kuto's Well. Each run is one seed: a level-one party from the curated
character pool, in its class starting kits, plays a fixed arc of twenty-two
fights and two arrow volleys and gains levels up to four. Both sides use the automated
demo policy (`choose_demo_command`).

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
`tools/encounter_balance.cpp` documents it). Then the Slums' set encounters, each
in its own room: the hobgoblins arguing over gold (5 hobgoblins), the monster
leaders (an ogre, 2 gnolls, 2 hobgoblin leaders) and the trolls and ogres (2
ogres, 4 trolls). As in the original, a downed troll stays down once the fight
is won, and during it gets up again unless Acid or Fire hit it or someone stands
on it.

Kuto's Well follows (its fights on `GEO8:29` and `GEO8:32`): plaza gnolls,
the sickly kobolds (2 kobolds, 4 leaders), plaza kobolds, the well's kobolds
(6 and 3 leaders), the lizardman patrol (a lizardfolk and 4 giant lizards),
plaza lizardmen, the catacombs' volley of 4 arrows and the dim archer's 1,
then Norris the Gray's band (Norris, 5 lizardfolk, 9 kobold leaders). Roaming
groups follow ECL8:29's table at `0xAFA2`: gnolls at a third of the party's
strength, kobolds at half with three kobold leaders, lizardmen at a quarter.
Arrows are the campaign's `DAMAGE` attacks (+4, 1d6 piercing on a random
conscious member); a volley that leaves nobody standing is a defeat. Every encounter, the four-orc
search included, is fitted to the XP budget at the default encounter challenge
and to one creature per living member, as the campaign session does. Each living member gains the original encounter's
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
- `summary.csv`: per party: the share of runs that cleared the Slums, success
  rate (the whole arc), flawless rate (complete with no
  deaths), average fights won, deaths and level.
- `usage.csv`: how often each class chose each command, to check the policy
  uses the class features.
