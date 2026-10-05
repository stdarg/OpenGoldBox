# Cleric

Milestone [#8](https://github.com/stdarg/OpenGold/issues/8), delivered class by
class ([CLASS-1](SRD-DECISIONS.md#class-1-2026-10-04-finishing-levels-14-for-all-classes)).
SRD 5.2.1 pp. 34–37. Spell preparation, cantrips and Divine Order are described
in [Cleric preparation](CLERIC-PREPARATION.md).

| Level | Feature | Status |
| --- | --- | --- |
| 1 | Spellcasting, Divine Order | Delivered ([Cleric preparation](CLERIC-PREPARATION.md)) |
| 2 | Channel Divinity | Delivered (rules 0.6.82) |
| 3 | Cleric Subclass (Life Domain) | Not yet |
| 4 | Ability Score Improvement | Delivered earlier |

## Channel Divinity

- Two uses from level two; a Short Rest restores one, a Long Rest all
  ("Channel Divinity" in the rest and combat resources).
- **Divine Spark** (Action, another creature the Cleric can see within 30
  feet): roll 1d8 + Wisdom modifier. An ally regains that many Hit Points; an
  enemy makes a Constitution save and takes that much Radiant damage, half on a
  success. Necrotic is used instead when the target resists Radiant more (the SRD
  leaves the choice to the Cleric; this takes the better one).
- **Turn Undead** (Action, offered when an Undead enemy is within 30 feet): each
  Undead enemy within 30 feet makes a Wisdom save or is turned for 1 minute:
  Frightened and Incapacitated, it can only move as far from the Cleric as it
  can, takes no Action or Reaction, and the effect ends when it takes damage or
  the Cleric drops to 0 Hit Points. The current content has no Undead yet.
- In the game: A cycles to **Divine Spark** (click the creature) or **Turn
  Undead** (Space). The party AI uses Divine Spark on an ally below half Hit
  Points before spending a spell slot, and Turn Undead whenever it is offered.

Verification: `opengold_cleric_channel_tests` (no Channel Divinity at level one,
two uses, healing, harming with a save, Turn Undead needing an Undead, a turned
zombie fleeing then ending its turn, a checkpoint, damage ending it) and
`opengold_godot_divine_spark` (A cycle and click, English and Spanish).
