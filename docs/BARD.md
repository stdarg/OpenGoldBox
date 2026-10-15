# Bard

Levels 1–4 of the SRD 5.2.1 Bard (pp. 31–35). Charisma is the spellcasting
ability.

## Spellcasting, Jack of All Trades and the College of Lore

- Bards prepare spells from the Bard list like Clerics: two cantrips (three
  from level 4) and 4/5/6/7 prepared spells, with full-caster slots, chosen at
  creation and on gaining a level; a Long Rest changes none. The implemented
  Bard spells through level 2 are on the list. On gaining a level one cantrip
  and one spell may be replaced ([Replacing choices](LEVEL-SWAPS.md)).
- Creation: `opengold_character_tests` makes a Bard through the ordinary
  creator (three skills, two cantrips, four spells, Bardic Inspiration);
  `opengold_bard_tests` checks the counts and refuses a foreign, repeated,
  level-two or fifth spell.
- **Vicious Mockery** (cantrip): a Wisdom save against 1d6 Psychic; a failure
  also gives Disadvantage on the target's next attack roll before the end of
  its next turn.
- **Starry Wisp** (cantrip, also Druid): a spell attack for 1d8 Radiant; the
  target cannot be Invisible until the end of the Bard's next turn ("Lit").
- **Dissonant Whispers** (level 1): a Wisdom save against 3d6 Psychic, half on
  a success; on a failure the target spends its Reaction moving as far from
  the Bard as its Speed allows, by the safest route: of the farthest squares,
  the one reached past the fewest enemy reaches. Leaving a reach still
  provokes an Opportunity Attack, asked with React and Decline as for any
  move; the flight goes on after it, and then the Bard's turn.
- **Faerie Fire** (level 1, also Druid, Concentration): a 20-foot cube; each
  creature in it that fails a Dexterity save is **Outlined**: attack rolls
  against it have Advantage when the attacker sees it, and it cannot be
  Invisible.
- **Heat Metal** (level 2, also Druid) is on the Bard list from level 3; see
  [Druid](DRUID.md#flame-blade-moonbeam-spike-growth-and-heat-metal).
- **Jack of All Trades** (level 2): + 1 (half the Proficiency Bonus) to
  Initiative, as SRD-DECISIONS keeps it, unless Alert already adds the whole
  bonus. The level-2 Expertise is not pursued (SCOPE-2); the level-9 pick
  comes with levels 5–15 (LEVELS-1).
- **College of Lore** (level 3): Bonus Proficiencies in three skills chosen at
  level-up, and Cutting Words (below).
- Verification: `opengold_bard_tests` (each spell's save or attack and its
  effect, Advantage against an outlined enemy, the grants at levels 2–4, seven
  prepared spells, Jack of All Trades' +1 to Initiative at level 2 and none on
  top of Alert), `opengold_spell_table_tests` (Bard probes).

## Bardic Inspiration and Cutting Words

- **Bardic Inspiration** (level 1, Bonus Action): a d6 for an ally within 60
  feet; uses equal the Charisma modifier (at least one), all back on a Long
  Rest. They share Arcane Recovery's store, which no Bard has. A creature holds
  one die at a time, shown as **Inspired**.
- Nobody is asked about either die
  ([BARD-1](SRD-DECISIONS.md#bard-1-2026-10-13-bardic-inspiration-and-cutting-words-without-asking),
  rules 0.6.169). The log shows each use and what it changed.
- An inspired creature adds the die to its first failed roll that the die
  could turn into a success: an attack roll (a natural 1 aside), any saving
  throw (including the repeated saves at the end of a turn and Concentration
  saves), and the ability checks rolled in a fight (Medicine to stabilize,
  Athletics to break free of vines). The log reads, for example, "Bardic
  Inspiration adds 4 to Mira's roll, turning it into a success." A roll the d6
  cannot rescue keeps the die for later.
- **Cutting Words** (College of Lore, level 3, Reaction): when an enemy within
  60 feet that the Bard sees hits with an attack roll by less than 6 (a
  critical hit aside), the Bard subtracts its Bardic Inspiration die from the
  roll, spending its Reaction and a use. The log says whether the attack then
  misses. It also answers an enemy's ability check that succeeded by less
  than 6: an Athletics check to break free of vines or webs, or a Search that
  would find a hidden creature of the Bard's side (measured against the
  best-hidden one it would find); the log says whether the check then fails.
  On an enemy's damage roll it is used only when the damage would drop a
  member of the Bard's side, the Bard included, to 0 Hit Points and the d6
  could keep it standing ([CUT-1](SRD-DECISIONS.md#cut-1-2026-10-14-cutting-words-against-lethal-damage)).
  The estimate counts the target's resistances and Temporary Hit Points. It
  covers an attack's damage and a ranged hit's added poison, the damage rolls
  the game's monsters make.
- Verification: `opengold_bard_tests` (four uses at Charisma 18, the Bonus
  Action, the die used only on a miss it could rescue, Cutting Words' Reaction
  and use, Cutting Words against a monster's Search and a lethal damage roll,
  the repeated-save and Concentration helpers).
