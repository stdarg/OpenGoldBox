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
experience, less a creature's share for each monster that got away.

Monsters check morale as in the game ([MORALE-1](SRD-DECISIONS.md#morale-1-2026-10-09-monster-morale-as-the-original-game-had-it)),
with each fight's morale from its script: the four-orc search 99, the arguing
hobgoblins 75, the monster leaders 80, the trolls and ogres 75, the sickly
kobolds 25, the well's kobolds 67, the lizardman patrol 75 and Norris's band 70.
A roaming group's morale depends in the scripts on how the meeting opened; the
simulator uses the neutral opening: in the Slums 50 (55 with leaders, 65 with a
bugbear: ECL2:20 at `0x9D40`, `0xB1BF` and `0xB1E9`), at Kuto's Well ECL8:29's
table at `0xAFB1` (gnolls 75, kobolds 50, lizardmen 90). No creature here has
a morale of its own; their Intelligence comes from their records (lizardfolk
and giant lizards 3, so they fight on when outpaced rather than surrender).

Measured with 50 runs per party (2026-10-09), morale changed success little
(within two runs either way; 8 defeats in 750 runs, against 10 without) and
lowered deaths: six Fighters went from 60% to 72% flawless, six Rogues from 48%
to 64%, six Monks from 70% to 76%. Most broken monsters surrender, about 20 a
run, since a party nearly always has someone faster; parties as slow as their
foes see them run instead (six Rogues: 3 escape a run, none surrender). Simulated
parties never flee: with so few defeats, fleeing would change nothing measurable.

Each member sets out with its class kit (a Torch included) and 2 Oil and 2
Alchemist's Fire, as if bought at New Phlan's general store; the simulator does
not track gold.

After a victory, dying members make their death saves and the Stable regain a
Hit Point over four hours. Members level up with the default choices, with
spells the policy uses well (Moonbeam, Spiritual Weapon, Hex, Healing Word and
so on) swapped in where the rules accept them. The party takes a Short Rest
when anyone is below half Hit Points and a Long Rest every third fight, when
still below half, or when fewer than half of the party's spell slots are left
(as a player would rest before going on; without it six Bards reached the
trolls with no slots and succeeded 28% of the time, with it 100%); rests are
never interrupted. A defeat ends the run; dead
members stay dead.

## Troll arena

`opengold_campaign_sim --troll-arena GAME_DIR [RUNS [OUT_DIR [PARTY]]]` plays
only the Slums' trolls-and-ogres fight (2 ogres and 4 trolls, fitted as the
campaign fits it, with its script's morale of 75) for each party at level four, rested, in open areas 1, 2, 4,
8 and 16 squares wide (ogres in front of trolls in a one-square corridor) and in
the trolls' own room in the Slums. Each party fights with four loadouts: the
starting kit (with its torch and bow), and 2 Oil, 2 Alchemist's Fire, or both,
per member. It prints win rates and writes `arena.csv`.

## Parties

`classic` (two fighters, cleric, rogue, wizard, paladin), `martial`, `casters`,
and `six-<class>` for each class.

## Output

- `runs.csv`: one row per run: outcome (`complete`, `defeated` or `stalled`),
  fights won, the fight lost, deaths and the classes that died, rests taken,
  final levels, and the monsters that escaped and that surrendered.
- `summary.csv`: per party: the share of runs that cleared the Slums, success
  rate (the whole arc), flawless rate (complete with no
  deaths), average fights won, deaths and level, and average monsters escaped
  and surrendered.
- `usage.csv`: how often each class chose each command, to check the policy
  uses the class features.
