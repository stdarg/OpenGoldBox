# Training halls

New Phlan has four training halls, one for each of Pool of Radiance's class
groups. In OpenGoldBox a hall does not raise levels: characters level up from
the roster's Advance button anywhere, as in other D&D games. A hall instead
sells **sessions of experience** and teaches **skill lessons**, all paid from
the party's gold.

**Status.** The halls' rules are in the game and saved with the campaign
([#9](https://github.com/stdarg/OpenGold/issues/9)). The hall screen is not:
answering YES at a hall is still refused as unsupported until the proposed
controls are approved (see [Not in the game yet](#not-in-the-game-yet)).

## How training works

### Which hall trains whom

Each hall trains the SRD classes of one AD&D 2nd edition class group. A member
of another group's class is turned away.

| Hall | Trains |
| --- | --- |
| Fighters' | Fighter, Barbarian, Paladin, Ranger |
| Clerics' | Cleric, Druid |
| Magic-users' | Wizard, Sorcerer, Warlock |
| Thieves' | Rogue, Bard, Monk |

Only active, living party members train. Reserve members and the dead do not.

### Sessions of experience

A session grants **half the experience between the character's level and the
next**. The other half has to be earned adventuring, so gold alone never gains
a level. A character can buy **one session per level**, even with no
experience toward the next level yet. The fee is **1,000 gp at level 1 and
doubles at each level**.

| Level | Experience granted | Fee |
| --- | --- | --- |
| 1 → 2 | 150 | 1,000 gp |
| 2 → 3 | 300 | 2,000 gp |
| 3 → 4 | 900 | 4,000 gp |
| 4 → 5 | 1,900 | 8,000 gp |
| 5 → 6 | 3,750 | 16,000 gp |
| 6 → 7 | 4,500 | 32,000 gp |
| 7 → 8 | 5,500 | 64,000 gp |
| 8 → 9 | 7,000 | 128,000 gp |

The game stops at level 4 for now, so a level-4 character is refused (the
highest level). The later rows apply once higher levels arrive.

A session never pushes a character past a level it could already take: a
member who already has the experience for the next level is told to advance
first. Experience is never taken away afterwards; the original's clamp to just
below the next level is not kept.

For example, Arden, a level-1 Fighter with no experience, buys a session at
the Fighters' hall for 1,000 gp and has 150 experience. After earning 150 more
in the Slums, Arden advances to level 2 from the roster. At level 2 a new
session costs 2,000 gp and grants 300.

### Skill lessons

A hall teaches **one lesson per level**:

- **Proficiency** in a skill on any of the hall's classes' skill lists that the
  character lacks: **250 gp**. The Fighters' hall therefore also teaches the
  Ranger's Stealth and the Paladin's Medicine. The Thieves' hall teaches the
  Bard's whole list, which is every skill.
- **Expertise** in a skill the character is already proficient in: **500 gp**.
  Expertise doubles the proficiency bonus for that skill.

A skill or Expertise the character already has is not offered. A Wizard's
Scholar, at level 2, then leaves out Expertise the hall has already taught.

Tools and languages, which the halls were also to teach, were removed from the
game earlier
([DM-1](SRD-DECISIONS.md#dm-1-2026-09-30-tools-languages-and-dm-adjudicated-spells)).
The halls teach skills and Expertise only unless that decision changes.

### Paying

Every fee comes from the party's gold coins, like the shops' and the temple's
prices ([party gold](DEVIATIONS.md#money)). A purchase is all or nothing: a
refused session or lesson charges nothing and changes nothing.

### When a hall says no

| Reason | Session | Lesson |
| --- | --- | --- |
| The member's class trains at another hall | yes | yes |
| Not an active, living member | yes | yes |
| Already at the highest level | yes | no |
| Already has the experience for the next level: advance first | yes | no |
| Already bought this level's session, or learned this level's lesson | yes | yes |
| The party's gold cannot pay the fee | yes | yes |

### What is saved

Each lesson is part of the character's history: a reloaded character
rebuilds its skills in the same order and from the same lessons. Each member
also keeps the level of the last session it bought, so reloading never sells a
second session at the same level.

## Not in the game yet

- **The hall screen.** A prototype puts a **Train** button beside each member
  the hall trains, opening a Training window with the session and the lessons.
  Its screenshots and three open questions are on the review page,
  `user-data/reviews/training-hall/index.html` in the main checkout.
- **`PROGRAM 0`.** The original hall script runs it after YES. Once the screen
  is approved, it will open the hall, charge only confirmed purchases and then
  let the script continue, as the temple and inn services do. Until then the
  hall's YES is reported as unsupported and the event rolls back.

## What the original does

- **Four halls.** The cluebook lists separate training halls for clerics,
  magic-users, fighters and thieves (New Phlan map, entries 6–9). Each hall's
  script (`ECL3.DAX` record 11) stores its class word and asks "WE TRAIN ONLY
  FIGHTERS HERE. DO YOU WANT TO TRAIN?". **YES** runs `PROGRAM 0`, the
  engine's training service; the script then clears the picture and exits.
- **The service.** The manual (Training Hall, Train Character): from the party
  creation menu, choose who trains; the game checks that character's
  experience and takes the cost from that character's money. The character
  gains one level and no game time passes. Train Character is also available
  "at the start of an adventure".
- **Cost and eligibility.** The
  [wish-goldbox measurements](https://wish-goldbox.readthedocs.io/en/latest/api/generated/goldbox.levelup.html)
  of 29 trainings found a flat 1,000 gp at every level, training offered once
  experience reached the next threshold, and experience clamped to just below
  the following threshold afterwards.
- **Level limits.** The manual gives Phlan's highest levels: fighter 8, thief
  9, cleric and magic-user 6.

## Decisions (2026-10-13)

The user decided, departing from the original
([DEVIATIONS.md](DEVIATIONS.md#advancement-and-training)):

1. Halls train the SRD classes by AD&D 2nd edition class group.
2. Levels are gained anywhere; halls grant experience and teach skills.
3. All gold belongs to the party
   ([#232](https://github.com/stdarg/OpenGold/issues/232), done).
4. Experience is kept after training.
5. The hall's services sit on the party roster; screenshots before code.
6. One session per level: half the experience to the next level, 1,000 gp at
   level 1, doubling each level.
7. One lesson per level: proficiency 250 gp, Expertise 500 gp, from the hall's
   classes' lists (tools and languages wait on DM-1).

## For developers

- `src/OpenGold.Core/include/opengold/training_hall.h`: `TrainingHall`,
  `training_hall_for_class` (by class ID, not label), `hall_classes`,
  `training_hall_named` (the script's word in `0x982C`: "FIGHTERS ",
  "CLERICS ", "MAGIC USERS ", "THIEVES "), `experience_session_fee`,
  `skill_training_fee` and `TrainingDenial`.
- `CampaignParty`: `experience_offer` and `buy_experience`;
  `skill_training_denial`, `skill_lessons` and `train_skill`. Purchases are
  atomic. `PartyMember::hall_experience_level` records the last session.
- Rules: a lesson is a grant sourced `training_hall` at the character's level,
  inserted after the last skill or Expertise grant so a save's replay rebuilds
  the same order. `RulesModule::hall_training_options` and `train_at_hall`;
  the rules reject a second lesson at one level, a duplicate and Expertise
  without proficiency. `Character::hall_training()` is the saved history.
- Saves: campaign format 27, rules 0.6.159.
- Tests: `opengold_training_hall_tests`.
