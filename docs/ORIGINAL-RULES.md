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
   "helpless" (sleep, paralysis) comes with the effects in step 6. A fight
   ends only as a round ends (`EndOfRound`): the creatures with turns left
   still take them after a side is down, and the dying bleed first. On a full
   tie of initiative and d100 the later creature acts first (`NextCreature`).
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
   backstabs, sweeps, bandaging. *Done 2026-11-12:* creatures face one of
   eight directions and turn toward an attacker until a third attacks them in
   a turn; the attacks on a creature and their angles count toward an attack
   from behind (its Armor Class from behind) and a thief's backstab (that
   Armor Class less 2, level div 4 + 2 times the damage). Stepping away from
   an enemy draws its free attack; a guard strikes an enemy coming next to it;
   Delay acts again at initiative 1; a fighter among more level-0 enemies than
   its attacks sweeps through up to its level of them; bandaging needs the
   dying member next to the one bandaging (the user, 2026-11-12). Guard is
   offered to every creature until step 4 tells ranged weapons apart: the
   original offers it only without one, or with a throwable one.
4. Missiles: rates of fire, ammunition, distance, thrown weapons. *Done
   2026-11-12:* a readied weapon whose type's range is above 1 is a missile
   weapon (`original::Missile`). It reaches its range less 1, counted as the
   original counts a line (2 a straight step, 3 a diagonal one, halved), at
   a target it sees, with no penalty for distance (the user, 2026-11-12). It
   attacks at its rate of fire (at least once a round), no more often than it
   has shots. A bow shoots the readied arrows, a crossbow the quarrels, a
   sling needs nothing, and a thrown weapon is its own ammunition: each shot
   spends one, and the last thrown weapon leaves its thrower empty-handed.
   With an enemy next to it a creature cannot shoot, unless its weapon is one
   it throws with Strength (a dagger, hand axe, spear or javelin), which then
   strikes as in melee; only such a weapon, or none, can guard. Two things
   wait for step 7: sight on the shared battlefield is blocked by its
   obstacles, where the original also weighs each square's height, so the
   original's terrain heights come with its battlefields; and the shots spent
   leave the party's inventory, as the original drops a thrown weapon in the
   loot.
5. Morale, fleeing and surrender. *Done 2026-11-12:* a creature whose
   record's NPC byte is 0x80 or more checks its morale as its turn starts
   (the player's characters never do). It holds while its morale (twice the
   byte's low 7 bits, 0 past 102) is at least the share of Hit Points it has
   lost, and an enemy of the party also holds while the opposition's Hit
   Points left (in steps of 5, as of the round's start) are at least 100 less
   the encounter's morale. Broken, it flees in panic when no enemy is faster
   (the reconstruction's comment says "twice as fast", but its arithmetic
   compares the movements themselves, and this follows the arithmetic),
   surrenders with Intelligence 6 or more, and fights on otherwise; it may
   rally at its next turn. A party NPC without a morale of its own takes the
   encounter's. A creature on the field's edge with a step left can flee: it
   gets away with no enemy in sight or more movement than the fastest enemy,
   or on 1 in 2 when they tie. With none of the party left fighting, the
   party has fled when one of its own characters got away, and is defeated
   otherwise. Left for later: where a panicked creature runs is the AI's
   (step 9), which runs monsters along the party's facing; the script's
   change to the party's movement (PARTY_MOVE_MODIFIER) comes with step 7, as
   does what the game does after a flight: the members left behind are lost,
   as in the original.
6. Saving throws, then the monsters' special abilities (MONnSPC). In parts:
   - 6a. *Done 2026-11-12:* a creature's effects (a monster's from its
     record) with their minutes, which a round ages by one; those that end at
     0 Hit Points end as a creature goes down or leaves the field. Saving
     throws: a natural 1 fails, a 20 saves, otherwise the roll with the
     readied items' save bonus and the modifier against the creature's
     number. A helpless creature (asleep, paralysed, nauseous or charmed by a
     snake) loses its turns and makes no free attack or guard strike; an
     attack on it is one blow of its Hit Points and 5 more, which leaves it
     dying, and one made helpless partway through an attack is hit by the
     rest.
   - 6b-1. *Done 2026-11-12:* what a hit that leaves its target standing
     carries, by the attack that hit (a secondary attack resolves before the
     primary): poison, which kills unless saved against; paralysis (a ghoul's
     sparing elves); mummy rot; a stirge's drinking until it has had 12 to 15
     and flies off; a holding bite on a victim of Strength below 19, which
     bites from behind with 3d4 each turn; and a rear-claw rake after both
     claws hit. Immunity to paralysis and poison saves but on a natural 1.
   - 6b-2. *Done 2026-11-12:* level drain, with no save. Each level takes
     the Hit Points of one of the creature's levels from its maximum and its
     current; below level 2 it is dead forever. Otherwise the class whose
     level took the most experience loses a level, with its THAC0 and saves,
     and a victim left with no Hit Points is killed. A creature's numbers as
     the fight leaves them (levels, effects still running) go back to the
     game with its Hit Points (`VitalState::resources`).
   - 6c. *Done 2026-11-12:* defenses. A creature's blows land with its
     striking item (a thrown weapon, a launcher's arrows or quarrels, else
     its readied weapon; none for natural attacks), whose magic, silver and
     weapon class the defenses weigh: magic needed against characters and
     creatures below level 4, silver or magic, a wraith's (half from
     silver), halving by slashing, piercing, bludgeoning or magic, missile
     and boulder evasion from afar, holy water and oil. A troll regenerates 3
     a round from the third round after its first wound and rises again,
     whole, 3d6 rounds after a weapon downs it; plain regeneration heals 3 a
     round. One that fights while down stands again with 6 less its wounds
     and falls dead 1d4+1 rounds later. Displacement turns aside the first
     attack each round, blinking every attack until the blinker has acted.
     An invisible (camouflaged) creature cannot be singled out but by one
     that sees the invisible, and is 4 harder to hit; attacking makes it
     visible. A gazing monster cannot single out one holding up a mirror or
     the wooden holy symbol. A mummy's fear paralyses its enemies for 1d4
     rounds as the fight starts, unless saved against (+1 for a side of 6, +2
     for a human) or immune to paralysis.
   - The gazes, the lightning breath and the ankheg's acid fire only when
     the original's AI acts for a monster with a target (MONSTER_SPECIAL), so
     they come with step 9.
7. Into the game: fights in the campaign under the original rules, on screen.
   Readying items mid-fight (the View menu) changes a creature's numbers as
   the original recomputes them (`RecalcStats`, `RecalcAttacksLeft`), so a
   fight needs each creature's items, not only the profile worked out at its
   start.
8. Spells in combat. In parts:
   - 8a. *Done 2026-11-12:* the casting framework, with the first-level
     spells (`original_spells.h` holds the original's spell table). Cast
     offers each memorized spell; one with a casting time takes casting
     time / 3 off the caster's initiative and goes off at its next turn in
     the same round, aimed then, and damage before then loses it with its
     memorized copy. Targets: the caster, separate creatures in range and
     sight, or everyone within a radius of a square, allies too, found by
     lines from that square. Each target saves if the spell allows, a touch
     spell needs an attack roll, one damage roll is shared, and the effect
     goes through the immunities and magic resistance; an effect a creature
     has already is replaced by a longer one. Bless, Curse, Cure Light
     Wounds, Cause Light Wounds, Protection from Evil and Good, Resist Cold,
     Burning Hands, Charm Person, Magic Missile, Shield, Shocking Grasp and
     Sleep work; Detect Magic changes nothing in a fight; Enlarge and Reduce
     come with Strength in 8b. A character has its race's innate effects:
     the Constitution saves of dwarves, gnomes and halflings and the elf's
     and half-elf's resistance to sleep and charm. A dwarf strikes orcs,
     goblins and hobgoblins (and GAGOOL and MACE) 1 better and a gnome
     kobolds and goblins; giants (ogres, trolls, fire and hill giants) strike
     both 4 worse, and bugbears and gnolls a gnome, by the monster's record
     name (`kin_of`).
   - 8b. The second- and third-level spells.
   - 8c. *Done 2026-11-12:* turning undead, once a fight for one with cleric
     levels: one d20 for the whole attempt against up to 1d12 undead enemies
     in sight, the lowest kinds first and the nearest of those, by the
     original's table; each it beats is turned (forced to flee for the rest
     of the fight) or destroyed, and the first it fails ends the attempt.
     Destruction on a roll goes on to at least six; destruction without one
     (a 0 in the table) does not, as in the original.
   - 8d. Spells from items: wands, scrolls and potions.
9. The original's AI. It readies its best weapon and armor at each of its
   turns (`EquipWeaponAndArmor`): many monsters carry their weapons unreadied
   (Pool of Radiance's goblin guards their short swords). Its monsters' own
   specials fire as it acts: the petrifying and charming gazes (a mirror
   turning the first back), the lightning breath and the ankheg's acid.

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
| A saving throw is added up in a byte, so a total below 0 (a low roll with penalties) wraps around and saves (`fxengine.pas`, `RollSavingThrow`) | A total below 0 fails |
| A drider's paralysing touch passes a save penalty of 2 that the paralysis ignores, so it is no harder to save against than a thri-kreen's (`effhndlr.pas`, `EffDriderPara`) | The penalty of 2 counts |
| Displacement is readied only as the fight starts, so it turns aside one attack a fight, where its notes say one a round (`effhndlr.pas`, `EffDisplaced`) | One a round |
| Evading a missile or boulder takes the hit from the primary attacks' count whatever attack it was, a byte that wraps below 0 (`effhndlr.pas`, the missile evasion) | From the count of the attack evaded |
| A free attack's check that an attacker sees the invisible reads the creature leaving instead (`cbtacts.pas`, `AttacksOfOpportunity`) | The attacker's own sight |
| Magic Missile throws level div 2 missiles, none at level 1, where the manual promises one at levels 1 and 2 (`magic.pas`, the missile) | One missile for every two levels from level 1, as the manual says |
| Protection from Evil or Good takes 2 from an evil or good attacker's roll before that roll is made, which overwrites it (`effhndlr.pas`, `EffProtEvil`) | The 2 counts |
| A dwarf's or gnome's defense against giants judges the defender's own target instead of the attacker, and reads eight names from its list of five (`effhndlr.pas`, `EffDwarfGiantDefense`) | The attacker, among the five giants |
| A level drain divides by the victim's class levels, so a creature without any (a monster) divides by zero (`effhndlr.pas`, the level drain) | It loses its highest level's share |
| A level drain keeps the drained class's level as the victim's highest level, so a fighter 5 / thief 6 drained in fighter counts as level 4 (`effhndlr.pas`, the level drain) | The highest of its classes |
| Each hit of a holding bite's own bite starts another hold, stacking holds on the biter and immobility on the victim, which can outlast the fight (`effhndlr.pas`, `EffBiteHold`) | One hold at a time |
