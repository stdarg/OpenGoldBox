# Measuring the computer's play

`opengold_ai_lab` replays real campaign fights through the game's own combat and
policy, so a change to the computer's play can be measured in seconds. It answers
two questions: do the difficulty levels get harder in order, and does a change to
[`opengold/ai_tuning.h`](../../src/OpenGold.Core/include/opengold/ai_tuning.h) help?

The lab lives in [`dev_tools/ai_lab`](../ai_lab) and is a command-line program. Its
script builds an optimised copy in `build/ai-lab`, since a debug build is many times
slower, and passes on what it is given:

```
dev_tools/ai_lab/run.sh --list
dev_tools/ai_lab/run.sh --party-of 0026 --min-foes 4 --seeds 16
```

## The scenario pack

The expedition test plays the campaign's route and can keep every fight it meets:
the party as it stood, and the enemies as the game built them, gear included. The
script's first run does this, from `OPENGOLD_GAME_DIR` or `~/POOLRAD`. It takes about
a minute and a half.

A pack comes from your original game files, so it is never committed. Replaying one
does not need them. Make a new pack with `run.sh --new-pack` when the route, the
monsters or the party's gear change, or when a replay reports that a party save no
longer loads.

## Replay

`--party-of` matters: the route's party grows too strong for most fights to tell
anything, so give every fight an earlier party. Running the lab with no options
lists them all. Some common runs:

| Question | Options |
|---|---|
| Do monsters get harder in order? | `--party-of 0026 --min-foes 4 --seeds 16` |
| Does a Quick party play best at the top level? | `--party easy,normal,hard,nightmare,infernal --monsters normal --base normal:normal` plus the above |
| How do six bandit captains do on this field? | `--fights 0026 --enemies 'bandit-captain*6' --seeds 100` |
| One kind of fight only | `--only lizardfolk --by-fight` |

A run with `--every 2 --seeds 4`, 2,200 games, takes about a minute on 10 threads and
is enough to see a large effect. The whole of the first run takes several minutes;
replaying the route itself took 10 minutes for 160 fights.

## Reading the results

The first table gives each pairing's win rate, the party's Hit Points left, and
members down and dead at the end. **Do not judge by Hit Points left alone**: it once
made the top levels look easy when they were killing far more characters.

The second table plays the same fights with the same dice against one pairing, so
luck largely cancels. The `±` is one standard error: a difference under about
twice that may be noise.

A **stall** is a fight no one finished within 4,000 commands. There should be none
(MORALE-2); one means a change broke something.

## Accuracy

The fights run through the game's own combat, computer play and difficulty rules;
only the choice of fights and the tables are the lab's. To keep it honest,
`opengold_expedition_tests` replays every twentieth fight of the route through the
lab with the route's dice at Normal, and fails unless it comes out the same: the
same commands, winner, Hit Points left, members down and members dead.

## Limits

- Each level replays the fight as the game would stage it for that party and level
  (DIFF-2): the band sized at the default encounter challenge, and the monsters'
  Hit Points, attack bonus, morale and items set by the level, with Easy's safety
  net. Only monster abilities a level switches off are not replayed.
- Party members play with Quick magic on, and go looking for enemies out of sight
  (`Searching::party`), as the game's own checks do.
- Each fight starts from the party's save, so it does not measure a whole campaign:
  `opengold_campaign_sim` ([CAMPAIGN-SIM.md](../../docs/CAMPAIGN-SIM.md)) does that.
