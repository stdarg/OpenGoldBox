# Slums encounter balance audit

Audit date: 2026-10-01. Baseline: `c7a4e8b`. No gameplay changes were made for
this audit.

The opt-in Ohlo route ([QUESTS.md](../QUESTS.md#validation-and-limits)) fails
in the Old Rope Guild even after the party camps back to full HP. This audit
asks whether the error is on the party side (party strength scaled too high,
which a global factor would fix) or on the monster side (individual creature
conversions, which a global factor would not fix).

## Method

`opengold_encounter_balance GAME_DIR [SEEDS [CONTENT_PACK]]`
(`tools/encounter_balance.cpp`) builds the expedition test's party: six created
human fighters in long sword, chain mail and shield, at level 1 (strength 6)
or level 2 (strength 12). It then fights original Slums roaming encounters
through `CombatDemo` on the original battlefields, at `(14,7)` (street) and
`(6,14)` (Old Rope Guild). Both sides use the automated demo policy.

The encounter mix follows `ECL2.DAX` record 20:

- **Base count:** party strength (×1) in the Rope Guild (`0xadde` →
  `0x9b8a`); strength / 3 × 2 on streets and at camp (`0x9b68`).
- **Leaders:** three kobold or four goblin leaders from strength 8, four orc
  leaders from 14 (`0xb157`–`0xb193`).
- **Bugbear:** added above 18 (`0xb199`).

Each cell is 40 seeds. A scaling factor multiplies party strength before the
script sees it. Columns: win rate, average party deaths and members left at
0 HP per fight, and the party's HP left.

## Current conversions

Level 2, factor 1.0 (the route's situation):

| Context | Enemies | Win | Deaths | Down | HP left |
| --- | --- | --- | --- | --- | --- |
| street | 3 kobold leaders, 8 kobolds | 100% | 0.07 | 0.10 | 80% |
| street | 4 goblin leaders, 8 goblins | 92% | 0.82 | 0.90 | 52% |
| street | 8 orcs | 100% | 0.35 | 0.40 | 70% |
| guild | 3 kobold leaders, 12 kobolds | 100% | 0.40 | 0.42 | 63% |
| guild | 4 goblin leaders, 12 goblins | 18% | 1.18 | 4.30 | 6% |
| guild | 12 orcs | 68% | 1.35 | 2.00 | 31% |

Scaling party strength down does not even out the types. Goblins need a factor
of 0.5 to be won reliably, and at that factor kobolds, and goblins without
leaders, cost almost nothing. At level 1 every type is won at every factor.

## Monster-side differences

Original records (`opengold_creatures`, `MON2CHA.DAX`) against the current SRD
conversions in `data/rules/srd-5.2.1/combat.rules`:

| Record | Original | Current conversion |
| --- | --- | --- |
| 0 Kobold | 3 HP, 1d4, no bow | 5 HP, +4 1d4+2 |
| 2 Goblin guard | 4 HP, 1d6, short sword, **no bow** | 7 HP, +4 1d6+2, **bow +4 1d6+2** |
| 3 Goblin leader | 7 HP, 1d6, long bow | 14 HP, +4 1d6+2, bow +4 1d6+2 |
| 4 Orc | 5 HP, 1d8, no items | 12 HP, +4 1d8+2, ranged +3 1d6+1 |
| 5 Orc leader | 8 HP, 1d8, long bow | 18 HP, +5 1d8+3, ranged +3 1d6+1 |
| 63 Bugbear | 16 HP, 2d4 | 27 HP, +4 1d8+2 |

The conversions multiply HP by 1.7–2.4, add +2 or +3 damage to every attack,
and give bows to goblins and orcs that the original records do not carry. The
party's own conversion is milder: the level-two fighters average about 19 HP,
roughly 1.5× an AD&D level-two fighter.

Removing only the plain goblins' bow raises the Rope Guild goblin fight from 18%
to 50% wins (1.05 deaths); the remaining gap is HP and damage.

## Proportional candidate

A candidate pack keeps the current AC and attack bonuses. It sets HP to the
original × 1.5 (the party's own ratio), drops the flat damage modifiers (the
originals have no Strength bonus) and keeps ranged attacks only where the
original record carries a bow: kobold, goblin and orc leaders, and the bugbear.

| Creature | AC | HP | Melee | Ranged |
| --- | --- | --- | --- | --- |
| kobold | 12 | 5 | +4 1d4 | — |
| kobold leader | 13 | 6 | +4 1d4 | +4 1d6 |
| kobold leader (sword) | 13 | 6 | +4 1d6 | — |
| goblin | 14 | 6 | +4 1d6 | — |
| goblin leader | 15 | 11 | +4 1d6 | +4 1d6 |
| orc | 13 | 8 | +4 1d8 | — |
| orc leader | 14 | 12 | +5 1d8 | +3 1d6 |
| bugbear | 15 | 24 | +4 1d8 | +4 1d6 |

At factor 1.0 the party wins all 1,920 fights in the matrix, and the
encounters still cost something. Level 2:

| Context | Enemies | Win | Deaths | Down | HP left |
| --- | --- | --- | --- | --- | --- |
| street | 3 kobold leaders, 8 kobolds | 100% | 0.03 | 0.03 | 85% |
| street | 4 goblin leaders, 8 goblins | 100% | 0.20 | 0.15 | 77% |
| street | 8 orcs | 100% | 0.05 | 0.07 | 84% |
| guild | 3 kobold leaders, 12 kobolds | 100% | 0.10 | 0.12 | 79% |
| guild | 4 goblin leaders, 12 goblins | 100% | 0.38 | 0.38 | 64% |
| guild | 12 orcs | 100% | 0.28 | 0.35 | 70% |

With this pack swapped in, `OPENGOLD_OHLO_ROUTE=1 opengold_expedition_tests`
completes the whole route: commission, booth, reward, return, and the
fresh-process reload and revisit.

## Conclusion

The imbalance is on the monster side, and it is uneven across types, so a
global party-strength factor cannot fix it. Bringing the creature conversions
back to the originals' proportions balances every type at the original scaling.

Limits: one party composition (six fighters), two battlefields, and an
automated policy on both sides. The ×1.5 HP ratio is measured from these
fighters' HP; it is not a derived rule.

## SRD stat blocks and encounter challenge (2026-10-02)

Following this audit, the user chose SRD stat blocks over proportional
conversions and asked that encounters be scaled back instead
([MON-1](../SRD-DECISIONS.md#mon-1-2026-10-02-original-monsters-as-srd-stat-blocks)).
The tool now takes `opengold_encounter_balance GAME_DIR [SEEDS [CONTENT_PACK]]`,
fits each encounter exactly as the session does, and sweeps the encounter
challenge instead of a strength factor. It runs two parties: the six fighters,
and a mixed party of two fighters, two clerics (mace, scale mail, shield,
Sacred Flame), a rogue (shortsword, leather) and a wizard (quarterstaff,
Magic Missile). Campaign clerics cannot yet prepare leveled spells (#91), so the
mixed party has no healing magic. The demo policy now also stabilizes a dying
ally when no enemy is within reach, and uses Aggressive before moving.

**The SRD XP budget alone does not trim mobs.** At Moderate, six level-2
characters have 900 XP; three kobold leaders and twelve kobolds cost 375 and
four goblin leaders and twelve goblins 800, so neither shrinks. The fighters
won 28% and 0% of those Rope Guild fights, the mixed party none. With Pack
Tactics and the SRD's own warning about more than two creatures per character
(p. 203), a crowd limit is needed. Capping at two per character barely helped;
one per character made every kobold and goblin fight winnable.

**Orcs set the budget.** Even within one per character, six orcs (600 XP, the
SRD's Low budget for this party) beat the mixed party half the time:

| Level 2, Rope Guild | Challenge | Enemies | Fighters win | Mixed win | Mixed deaths |
| --- | --- | --- | --- | --- | --- |
| orcs | 100 (Moderate) | 6 orcs | 90% | 45% | 0.80 |
| orcs | 50 | 4 orcs | 100% | 95% | 0.35 |
| orcs | 33 | 2 orcs | 100% | 100% | 0.20 |

At the chosen default of 33 every cell is won at least 98% of the time by both
parties, levels 1 and 2, with at most 0.23 deaths per fight on average
(level 1 Rope Guild kobolds: 98% and 0.20 for the mixed party). At 50 the
level-1 mixed party loses 0.35–0.55 characters per fight against goblins and
orcs. With this default the whole Ohlo route passes in the normal expedition
test.

Limits as before: the demo policy on both sides, two battlefields, and party
compositions that the game's missing leveled cleric spells make weaker than the
SRD assumes. The default should be measured again when #91 lands.

## Re-measured with prepared Cleric spells (2026-10-02)

With [#91](https://github.com/stdarg/OpenGold/issues/91) the mixed party's two
clerics prepare Cure Wounds, Healing Word and Inflict Wounds at creation, and
Blindness as well at level three. The tool was run again with 40 seeds:

| Challenge | Fighters: lowest win% / most deaths | Mixed: lowest win% / most deaths |
| --- | --- | --- |
| 33 (default) | 100 / 0.15 | 100 / 0.23 |
| 50 | 98 / 0.17 | 98 / 0.23 |
| 100 (Moderate) | 90 / 0.38 | 85 / 0.72 |

Healing closes most of the gap the earlier run attributed to the missing
spells. Against six orcs at challenge 100 the mixed party now wins 85–88%
instead of 45%; at 50 the level-1 mixed party loses at most 0.23 characters
per fight instead of 0.35–0.55. The default stays 33; raising it to 50 now
keeps every cell at 98% or better for both parties.
