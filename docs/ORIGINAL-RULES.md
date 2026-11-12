# The original rules

OpenGoldBox plays by the original *Pool of Radiance* rules by default: AD&D 1st
edition as the game applied them. The SRD 5.2.1 rules (see [RULES.md](RULES.md))
are an option, chosen when a new game begins. This page
records the decisions and the plan. It grows as each part is built.

The reference for the original rules is the reconstructed source of
*Pool of Radiance* 1.3 (github.com/talonius/pool-of-radiance). It is derived
from SSI's code and grants no license, so OpenGoldBox learns the rules from it
and writes its own code and text. It never copies the source.

## Decisions

The user decided these on 2026-11-11 and 2026-11-12:

- **The original rules are the default.** The SRD rules are an option.
- **The difficulty levels keep their names** (Easy, Normal, Hard, Nightmare,
  Infernal), but each set of rules has its own effects for them. The balance
  settings built so far (DIFF-2's table, `encounter_challenge`, the `[monsters]`
  ability switches) belong to the SRD rules. The original rules' levels are
  designed from the original game; see [Difficulty](#difficulty).
- **Obvious bugs in the original are fixed, not kept.** Each one is put to the
  user for direction and then recorded in [DEVIATIONS.md](DEVIATIONS.md).
- **The rules are chosen for each game** (2026-11-12): a new game's first
  question is which rules it plays by, and the choice is kept for that save.
  The player does not switch mid-game. A character belongs to its game's rules,
  and the Character Pool offers only characters made under the same rules.
  The choice stays hidden, with the SRD rules in effect, until the original
  rules can play a fight.
- **Races:** the original rules offer only the original's six races.
- **Until the original rules' difficulty levels are built** (step 7), every
  level plays the original as it was. The Options screen shows
  `encounter_challenge` and the `[monsters]` ability switches only under the
  SRD rules.
- **Dexterity above 18 is worth more in a fight** (2026-11-12): each step of
  its Armor Class and missile bonuses comes two points sooner than in the
  original, so an elf's natural 19 beats 18. Every departure from the original
  rules is listed in [DEVIATIONS.md](DEVIATIONS.md#the-original-rules).
- **Thief skills** (2026-11-12): the two out-of-step Dexterity adjustments are
  corrected, Dexterity beyond the original's table (9 to 19) uses its ends,
  and no chance falls below 0.
- **Aging is chosen for each game** (2026-11-12), when it starts, and kept for
  that save:

  | | Off | As the original (default) | AD&D |
  |---|---|---|---|
  | Age changes ability scores | Never | Once, by the starting age | By the current age |
  | Table | — | The original's | The original's, with AD&D's +1 Constitution from young adulthood on |
  | Ages with time | — | Haste only | Haste and every year the game's clock passes |

  - **Scores are worked out, not stored:** a character keeps its rolls and its
    starting age, so a Character Pool character fits any game's setting.
  - **Every character has an age,** rolled from the original's table even
    under the SRD rules, where it changes nothing.
  - **Under AD&D aging,** each older age group changes the scores again,
    keeping them between 3 and the race's maximum; they may fall below the
    class's minimum.
  - **No one dies of old age.**

## What the original rules cover

These are the parts of the reconstructed source that hold rules, about 8,000
lines in all:

- **Characters** (`chargen.pas`, `charhlp.pas`, `classcal.pas`, the tables in
  `gdconsts.pas`):
  - six races to choose from; the tables also hold a seventh, the half-orc,
    which the race menu skips;
  - Cleric, Fighter, Magic-User and Thief, with the multi-classes each race
    allows;
  - each ability the best of six 3d6 rolls, adjusted for age, kept within the
    race's limits and raised to the class's minimums;
  - the *Modify character* screen before the first experience;
  - Hit Points, starting gold and starting spells.
- **Advancement:** the experience tables, the level limits, and training (one
  level per visit, 1,000 gold pieces).
- **Combat** (`combat.pas`, `cbtacts.pas`, `combatai.pas`, `fxengine.pas`):
  - d6 initiative, THAC0 and descending Armor Class;
  - half-attacks, fighters' sweeps against weak foes, backstab and attacks
    from behind;
  - range bands;
  - dying to −10 and bandaging, Guard and Delay;
  - fleeing, morale, and the monsters' tactics.
- **Saving throws:** five categories, by class and level.
- **Magic** (`magic.pas`, `effhndlr.pas`): about 45 cleric and magic-user
  spells of levels 1–3, memorized in camp and scribed from scrolls, and about a
  hundred effects.
- **Monsters:** taken whole from the game's files (`MON*CHA`, `MON*SPC` and
  `MON*ITM`), with no conversion.
- **Items:** taken from the `ITEMS` file.
- **Other:** thief skills, turning undead, the temple's services and prices,
  and resting.

## Plan

1. **The original rules' character rules**, a library of their own
   (`src/OpenGold.Rules.Original`) with tests: the tables, creation,
   THAC0, saving throws, levels and spell slots.
2. **Choosing the rules.**
   - The new game's first question: the rules, then (under the original
     rules) the Aging setting.
   - The game's direct calls to the SRD module replaced by the selected rules.
   - Each save keeping its rules, and the Character Pool keeping each
     character to its rules.
   - The difficulty settings split per set of rules.
3. **Character creation, the sheet and level-ups** under the original rules.
4. **Combat** under the original rules, with the original monsters' own
   statistics.
5. **Magic:** spells, memorizing, scribing and resting.
6. **Training, the temple, thief skills, turning undead and conditions,** and
   the original scripts reading the original values.
7. **The monsters' tactics and the original rules' difficulty levels.** The
   original rules become the default once they play from start to end.

## Combat (plan step 4)

Started 2026-11-12, needed by every later game (docs/GAMES.md). What the
original does, from *Pool of Radiance*'s combat units (`combat.pas`,
`cbtacts.pas`, `combatai.pas`, `fxengine.pas`, `charhlp.pas`, `postcbt.pas`):

- **The round.** Each creature's initiative is d6 plus its Dexterity missile
  adjustment, at least 1; a side taken by surprise loses 6 in the first round.
  The highest initiative left acts next, ties settled by a fresh d100. A
  creature acts until its actions end. Delay lets it act again later in the
  round; Guard ends its turn ready to strike. At the round's end the clock
  moves a minute, each dying creature bleeds 1 and dies past −9, and a side
  with no one able to act loses.
- **Movement** counts half-moves, twice the creature's movement: a straight
  step costs 2, a diagonal 3, times the ground's cost. Heavy armor caps it.
  Leaving an enemy's side gives it a free attack, against the mover's rear
  Armor Class; stepping next to a guarding enemy draws its strike.
- **To hit:** d20 against THAC0 minus the target's Armor Class; a natural 1
  misses and a natural 20 hits. A creature struck from several sides is hit
  from behind with its rear Armor Class (no Dexterity or shield). A thief in
  light armor with a light weapon backstabs such a target for multiplied
  damage. Missiles lose accuracy with distance in bands.
- **Attacks** are rates per two rounds; a monster has two attacks with their
  own dice; a readied weapon replaces the first, with Strength and its magic.
  A fighter sweeps through level-0 enemies around it, one attack each up to
  its level. Damage stops a spell being cast. Attacking a helpless creature
  kills it.
- **Down:** 0 Hit Points is unconscious, below 0 dying, more than 9 below 0
  dead. Bandage saves the first dying member. After a won fight the dying
  wake unconscious.
- **Morale:** monsters only, from the record's morale and the share of Hit
  Points lost, and the group's; a broken monster flees or, when it cannot
  outrun its enemies and is not stupid, surrenders.
- **Spells** with a casting time take effect at the caster's next action and
  are lost to damage before then. Saving throws: d20 with its bonuses against
  the category's number; 1 fails and 20 saves.
- **The original's AI** takes a stance, checks morale, then tries an item, a
  spell it began, turning undead, a spell, its best weapon, a target and its
  moves and attacks, and guards when nothing else is possible.

*Curse of the Azure Bonds* keeps this loop and turn order and adds monster
attacks (engulfing, eye rays, hugs, claws and rakes), dual-class levels and
many spells.

**Where it lives:** `src/OpenGold.Rules.Original` gains the combat session and
the rules module behind the interfaces the SRD rules implement (`RulesModule`,
`CombatSession`), so the game's fights, party and screens drive either. Monsters
fight with their records' own values (THAC0, Armor Class, Hit Points, attacks,
movement, saving throws, special abilities) and award their records'
experience; party members' numbers come from their original-rules sheets and
gear.

**Steps,** each with its tests:

0. Groundwork: Strength's to-hit and damage, armor movement, attack rates and
   the to-hit rule in the tables; the original's item types; the movement grid
   shared, with the original's step costs. *Done 2026-11-12:* the tables in
   `original_rules.h`, the item types' property bits
   (`ItemTemplate::properties`), and `opengold/movement_grid.h` with its step
   costs (`StepCosts`).
1. A melee fight between original-rules characters and a monster record
   resolves: initiative, turns, movement, to-hit, damage, unconscious, dying
   and death, the result, saving and restoring mid-fight. *Done
   2026-11-12:* `original::rules_module()` (`original_combat.h`) runs a fight
   behind the game's rules interface; a character fights unarmed as the
   original's characters start (1d2 with Strength), a monster with its
   record's numbers (`combatant_from_record`). `opengold_original_combat_tests`
   plays two fighters against three kobolds, checks every Hit Point against
   the log, and replays a fight from its seed and from a mid-fight save. A
   creature down is out of the fight and no target; the original's
   "helpless" (sleep, paralysis) comes with the effects in step 6.
2. The real monster records, their readied weapons and their statistics. *Done
   2026-11-12:* `combatant_from_record(record, readied)` works out a
   creature's numbers as the original does at the start of every fight and
   turn (`RecalcStats`): Strength unarmed or with a weapon that takes it, the
   weapon's dice and magic, an elf's +1 with swords and bows, ammunition's
   magic, the best armor, shield, ring and protective magic, Armor Class from
   behind, and armor's weight on movement. Every one of *Pool of Radiance*'s
   172 records fights with sensible numbers, and the Slums' kobolds fight a
   party to the end.
3. The rest of melee: free attacks, guarding, delaying, attacks from behind,
   backstabs, sweeps, bandaging.
4. Missiles: rates of fire, ammunition, distance, thrown weapons.
5. Morale, fleeing and surrender.
6. Saving throws, then the monsters' special abilities (MONnSPC).
7. Into the game: fights in the campaign under the original rules, on screen.
8. Spells in combat.
9. The original's AI.

## Encumbrance

What a creature carries slows it: every item, by its weight times its
quantity, and every coin, one coins' weight each. A readied bag of holding
carries 5,000 coins' weight for nothing, though never less than what is
readied (`charhlp.pas`, `RecalcStats`). Strength allows a load before it
slows (`WeightAllowance`):

| Strength | 3 or less | 4–5 | 6–7 | 8–11 | 12–13 | 14–15 | 16 | 17 | 18 | 18/01–50 | 18/51–75 | 18/76–90 | 18/91–99 | 18/00 | 19 | 20 | 21 | 22 | 23 | 24 | 25 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Allowance | −350 | −250 | −150 | 0 | 100 | 200 | 350 | 500 | 750 | 1,000 | 1,250 | 1,500 | 2,000 | 3,000 | 4,000 | 5,000 | 6,000 | 7,500 | 9,000 | 12,000 | 15,000 |

Beyond it, the load sets a slowest movement (`EncumbranceAdjust`); a load
never speeds a creature up:

| Coins over the allowance | 0–512 | 513–768 | 769–1,024 | More |
|---|---|---|---|---|
| Movement at most | its own | 9 | 6 | 3 |

## Difficulty

The original had no difficulty levels: the camp's Alter menu offers only
Order, Drop, Speed, Icon and Pics. It does have the levers its designers used
to tune fights:

- **Party strength.** The scripts read the party's strength (the PARTY STRENGTH
  command: Hit Points, Armor Class, THAC0 and cleric and magic-user levels) and
  size encounters from it.
- **To-hit and movement.** A script can raise or lower the monsters' THAC0 and
  movement for a fight.
- **Morale.** Monsters flee or surrender when their morale breaks.

The original rules' levels will be built from these levers, and their numbers
tuned with the AI lab once the original rules play.

## Bugs in the original

| Bug | Direction |
|---|---|
| Creation never applies racial ability adjustments: a leftover variable makes every character human (`chargen.pas`, the ability roll) | Fixed (the user, 2026-11-11) |
| Age gives no young adult the +1 Constitution of AD&D's table, which every older age group keeps (`chargen.pas`, the ability roll). The other changes match AD&D's table added up | The Aging setting: As the original keeps it, AD&D adds it (the user, 2026-11-12) |
| The thief skill table's Dexterity adjustments break their own order at Dexterity 10 (pick pockets −19, between −15 at 9 and −5 at 11) and 16 (open locks −5, between 0 at 15 and +10 at 17) (`gdconsts.pas`, `dexAdj`) | Corrected to −10 and +5 (the user, 2026-11-12) |
| A thief's chance is kept in a byte, so a Dexterity penalty above it wraps around: a level 1 thief of Dexterity 9 moves silently 251% of the time (`classcal.pas`, the thief skills) | Stops at 0 |
| A hit's damage is added up in a byte, so dice plus a negative modifier below zero wrap around to more than 250 damage (`cbtacts.pas`, `CalcWeaponDamage`) | Counts as 0 damage (the user, 2026-11-12) |
| Armor of 151 to 399 coins sets movement to 9, and heavier armor to 6, whatever the creature's own movement, so a creature slower than that moves faster in armor (`charhlp.pas`, `ArmorMoveAdjust`) | Armor slows only by its weight in the load, which only slows (the user, 2026-11-12) |
| A missile weapon of range 3 or less counts every target as beyond two range bands, so its target's Armor Class is 5 better even next to the shooter (`cbtacts.pas`, `ApplyRangePenalty`) | No distance penalty within a weapon's range (the user, 2026-11-12) |
| Bandage saves the first dying party member anywhere on the field (`combat.pas`, `BandageDying`) | The one who bandages must stand next to the dying member (the user, 2026-11-12) |
