# Sorcerer

Levels 1–4 of the SRD 5.2.1 Sorcerer (pp. 64–70). Charisma is the
spellcasting ability; the earlier cantrip work is in
[Sorcerer cantrips](SORCERER-CANTRIPS.md).

## Spellcasting, Innate Sorcery and Draconic Sorcery

- Sorcerers now prepare spells from the Sorcerer list like Clerics: four
  cantrips (five from level 4) and 2/4/6/7 prepared spells at levels 1–4,
  chosen at creation and on gaining a level; a Long Rest changes none. Their
  slots match a Wizard's. The implemented Sorcerer spells through level 2 are
  on the list (Mind Spike included, as the inventory records). Replacing one
  prepared spell or cantrip on gaining a level is not offered yet.
- **Sorcerous Burst** (cantrip): a spell attack for 1d8 of a chosen type,
  offered once per type (Acid, Cold, Fire, Lightning, Poison, Psychic,
  Thunder); each 8 rolls another d8, up to the Charisma modifier in extra dice.
- **Innate Sorcery** (level 1, Bonus Action, twice per Long Rest): for a
  minute, +1 to the Sorcerer's spell save DC and Advantage on its spell attack
  rolls. Its uses share the Paladin's Smite store, which no Sorcerer has.
  Every spell save DC now comes from one place, which adds the +1.
- **Draconic Sorcery** (level 3): Draconic Resilience adds 3 Hit Points, and 1
  more per later level, and makes unarmored AC 10 + Dexterity + Charisma;
  Chromatic Orb, Command and Dragon's Breath are always prepared (Alter Self is
  removed by DM-3).
- Level 2's Font of Magic and Metamagic arrive in the next increments.
- Verification: `opengold_sorcerer_tests` (creation's prepared spells,
  Sorcerous Burst per type, no Long Rest preparation, Innate Sorcery's uses and
  Advantage, the Draconic grants, Hit Points and AC, always-prepared spells,
  seven prepared spells at level 4), `opengold_sorcerer_cantrip_tests`.
