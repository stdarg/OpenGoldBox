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
