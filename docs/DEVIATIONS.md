# Deviations from the original game

OpenGoldBox follows Pool of Radiance first ([PRD](PRD.md)). This page lists
every deliberate departure from the original game, with the decision that
records it. Changes the SRD makes to a rule (a spell left out, no spell
components) are not listed here; [SRD-DECISIONS.md](SRD-DECISIONS.md) records
those. Missing features and bugs are not deviations.

Add an entry whenever a new departure is decided.

## Rules and combat

- **Rules system** (2026-09-06). The original used AD&D combat calculations,
  class restrictions and spell memorization. OpenGoldBox uses SRD 5.2.1 behind
  a replaceable rules library. Campaign scripts and encounter identities and
  counts are kept through conversion adapters.
  [PRD §1](PRD.md#1-product-summary), [RULES.md](RULES.md).
- **Monsters** (2026-10-02). The original's AD&D monster records become SRD
  stat blocks, unchanged.
  [MON-1](SRD-DECISIONS.md#mon-1-2026-10-02-original-monsters-as-srd-stat-blocks).
- **Encounter size** (2026-10-02). The original ran its groups as written.
  OpenGoldBox shrinks an encounter over the SRD budget or with more creatures
  than living characters, and never grows one. Experience and treasure stay the
  original encounter's. [MON-1](SRD-DECISIONS.md#mon-1-2026-10-02-original-monsters-as-srd-stat-blocks).
- **Empty fights** (2026-10-10). With no room for a whole band, the original
  started an empty fight the party won at once. OpenGoldBox places the band's
  first monster on the nearest free square the party can reach.
  [PLACE-1](SRD-DECISIONS.md#place-1-2026-10-10-starting-positions-as-the-original-game-had-them).
- **Flee button** (2026-10-08). The original had no flee command; a member
  fled by moving off the battlefield's edge, which OpenGoldBox keeps. A
  **Flee** button beside End turn hands the party to the AI to run for the
  edges. [FLEE-1](SRD-DECISIONS.md#flee-1-2026-10-08-fleeing-a-fight-as-the-original-game-did).
- **Fire and Acid** (2026-10-08). New Phlan's general store also sells Acid and
  Alchemist's Fire at SRD prices, and every starting kit has a Torch, so any
  party can keep a troll down.
  [GEAR-1](SRD-DECISIONS.md#gear-1-2026-10-08-fire-and-acid-for-every-party).

## Party and parley

- **Party leader** (2026-10-08). The original asked who speaks at every parley.
  OpenGoldBox has a standing leader, who parleys with twice their Charisma and
  buys first in shops.
  [LEADER-1](SRD-DECISIONS.md#leader-1-2026-10-08-a-party-leader-speaks-for-the-party).
- **Ready-made characters** (2026-10-12). The original had only the characters
  a player created. OpenGoldBox adds a Character Pool of ready-made
  characters, who start with gold and no equipment like any new character.
  [START-1](SRD-DECISIONS.md#start-1-2026-10-11-starting-gold-as-the-original-game-gave-it).

## Money

- **Starting gold** (2026-10-11). Every class starts with the manual's
  3d6 × 10 gp, not START.EXE's per-class dice, whose use in Pool of Radiance is
  unconfirmed. [START-1](SRD-DECISIONS.md#start-1-2026-10-11-starting-gold-as-the-original-game-gave-it).
- **Party gold** (2026-10-13). The original kept a purse per character, pooled
  for party purchases. OpenGoldBox keeps one purse for the party that pays for
  everything; scripts see it as every member's money
  ([#232](https://github.com/stdarg/OpenGold/issues/232)).
  [TRAINING-HALL.md](TRAINING-HALL.md#decisions-2026-10-13),
  [PHLAN.md](PHLAN.md#coin-payments).

## Advancement and training

All decided 2026-10-13 and recorded in
[TRAINING-HALL.md](TRAINING-HALL.md#decisions-2026-10-13). The halls are not
yet implemented ([#9](https://github.com/stdarg/OpenGold/issues/9)).

- **Levels anywhere.** The original raised levels only at a training hall, for
  1,000 gp. OpenGoldBox raises them from the roster's Advance button anywhere.
- **Halls sell experience.** One session per level grants half the experience
  to the next level, for 1,000 gp at level 1, doubling at each level.
- **Halls train skills.** Skills from the hall's classes' lists, tools and
  languages: proficiency 250 gp, Expertise 500 gp, one per level.
- **Experience kept.** The original clamped experience to just below the next
  threshold after training. OpenGoldBox keeps it all.
- **Hall classes.** The original's four halls train the 12 SRD classes by
  AD&D 2nd edition's class groups.

## Rest

- **Short and Long Rests** (2026-09-30). Camp offers the SRD's Short and Long
  Rests in place of the original's rest. An interrupted rest ends and grants
  nothing. Camp restrictions and the paid inn rest stay the original's.
  [Rest workflow approval](SRD-DECISIONS.md#rest-workflow-approval),
  [SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation).
- **Camp interruptions.** The original reset its interruption count on loading
  and rolled with the DOS random generator. OpenGoldBox saves the count and
  rolls with the saved script random state, so reloading cannot reroll a camp.
  [RECOVERY.md](RECOVERY.md#camp-interruption-evidence).

## Interface and saves

- **Modern shell.** A Godot interface that follows the Gold Box screens'
  structure without their DOS-era limits.
  [PRD §5.2](PRD.md#52-modern-shell-original-game).
- **Save files.** OpenGoldBox uses its own versioned save format. Importing
  original saves is wanted but not yet supported. [PRD §14](PRD.md#14-save-games), [SAVES.md](SAVES.md).
