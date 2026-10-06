# Bard

Levels 1–4 of the SRD 5.2.1 Bard (pp. 31–35). Charisma is the spellcasting
ability.

## Spellcasting, Jack of All Trades and the College of Lore

- Bards prepare spells from the Bard list like Clerics: two cantrips (three
  from level 4) and 4/5/6/7 prepared spells, with full-caster slots, chosen at
  creation and on gaining a level; a Long Rest changes none. The implemented
  Bard spells through level 2 are on the list. Replacing a spell or cantrip
  on gaining a level is not offered yet.
- **Vicious Mockery** (cantrip): a Wisdom save against 1d6 Psychic; a failure
  also gives Disadvantage on the target's next attack roll before the end of
  its next turn.
- **Starry Wisp** (cantrip, also Druid): a spell attack for 1d8 Radiant; the
  target cannot be Invisible until the end of the Bard's next turn ("Lit").
- **Dissonant Whispers** (level 1): a Wisdom save against 3d6 Psychic, half on
  a success; on a failure the target spends its Reaction moving as far from
  the Bard as its Speed allows. Adaptation: that flight provokes no
  Opportunity Attacks yet.
- **Faerie Fire** (level 1, also Druid, Concentration): a 20-foot cube; each
  creature in it that fails a Dexterity save is **Outlined**: attack rolls
  against it have Advantage when the attacker sees it, and it cannot be
  Invisible.
- **Jack of All Trades** (level 2): + 1 (half the Proficiency Bonus) to
  Initiative, as SRD-DECISIONS keeps it, unless Alert already adds the whole
  bonus. Bard Expertise is not pursued (SRD-DECISIONS).
- **College of Lore** (level 3): Bonus Proficiencies in three skills chosen at
  level-up, and Cutting Words (below).
- Verification: `opengold_bard_tests` (each spell's save or attack and its
  effect, Advantage against an outlined enemy, the grants at levels 2–4, seven
  prepared spells), `opengold_spell_table_tests` (Bard probes).

## Bardic Inspiration and Cutting Words

- **Bardic Inspiration** (level 1, Bonus Action): a d6 for an ally within 60
  feet; uses equal the Charisma modifier (at least one), all back on a Long
  Rest. They share Arcane Recovery's store, which no Bard has. A creature holds
  one die at a time, shown as **Inspired**.
- When an inspired creature misses with an attack roll or fails a saving
  throw, play stops and it is asked: "The roll fails. Add your Bardic
  Inspiration die, or decline." (React/Decline, through the same question as
  Shield). The die is added and the roll checked again. Ability checks and the
  repeated saves made at the end of a turn do not ask yet; a creature that
  declines is not asked again during the same command.
- **Cutting Words** (College of Lore, level 3, Reaction): when an enemy within
  60 feet that the Bard sees hits with an attack roll (a critical hit aside),
  the Bard is asked to subtract its Bardic Inspiration die from the roll,
  spending a use; the attack may then miss. Using it on a damage roll or an
  ability check is not modeled.
- The computer uses both whenever asked.
- Verification: `opengold_bard_tests` (four uses at Charisma 18, the Bonus
  Action, the ally asked after a miss, Cutting Words' Reaction and use).
