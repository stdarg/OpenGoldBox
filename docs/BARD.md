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
  level-up. Cutting Words arrives with Bardic Inspiration next.
- Verification: `opengold_bard_tests` (each spell's save or attack and its
  effect, Advantage against an outlined enemy, the grants at levels 2–4, seven
  prepared spells), `opengold_spell_table_tests` (Bard probes).
