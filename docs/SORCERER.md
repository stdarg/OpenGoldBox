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
- Level 2's Font of Magic and Metamagic are below.
- Verification: `opengold_sorcerer_tests` (creation's prepared spells,
  Sorcerous Burst per type, no Long Rest preparation, Innate Sorcery's uses and
  Advantage, the Draconic grants, Hit Points and AC, always-prepared spells,
  seven prepared spells at level 4), `opengold_sorcerer_cantrip_tests`.

## Font of Magic (level 2)

- Sorcery Points equal the Sorcerer level, all back on a Long Rest. They share
  the Lay On Hands store, which no Sorcerer has.
- From the Bonus Action list: **Font of Magic: create a level-1 slot (2
  Sorcery Points)** and, from level 3, **a level-2 slot (3 Sorcery Points)**,
  each a Bonus Action. **A level-1 slot into 1 Sorcery Point** and **a
  level-2 slot into 2 Sorcery Points** take no action and are listed there
  too; they never raise the points past the maximum. Created slots may exceed
  the usual count and vanish on a Long Rest.
- Verification: `opengold_sorcerer_tests` (two points at level 2, creating a
  slot, converting one back, a checkpoint).

## Metamagic (level 2)

- At level 2 the Sorcerer picks two options (the level-up's Metamagic list).
  In combat, each known option appears in the Bonus Action list as
  **Metamagic: <option> (<cost>)**; choosing it readies the option, costing
  nothing and taking no action, and **Metamagic: cancel** drops it. The next
  spell this turn that the option can change spends the Sorcery Points and
  uses it; a readied option lapses when the turn ends. This follows the
  common CRPG toggle rather than asking during each cast.
- **Careful** (1): allies in the area of a save spell, up to the Charisma
  modifier, succeed and take no half damage (chosen automatically: the allies).
- **Distant** (1): double range, or Touch becomes 30 feet (offers and aiming).
- **Empowered** (1): rerolls the lowest damage dice that are below average, up
  to the Charisma modifier, for spell attacks and save-for-damage spells.
- **Extended** (1): a Concentration spell lasts twice as long, with Advantage
  on the saves to keep it.
- **Heightened** (2): the spell's target, or the first enemy in its area, has
  Disadvantage on its save against it (repeated saves later are not
  affected yet).
- **Quickened** (2): Action spells are offered as Bonus Actions.
- **Seeking** (1): a missed spell attack rolls its d20 again, once.
- **Subtle** is not offered: it only removed spell components, which the game
  does not have ([CLASS-11](SRD-DECISIONS.md#class-11-2026-10-06-no-spell-components)).
  A Sorcerer who chose it earlier keeps it, without effect.
- **Transmuted** (1): readied with a type (Acid, Cold, Fire, Lightning, Poison
  or Thunder); a spell dealing one of those deals the chosen one instead.
- **Twinned** (1): a spell that takes one more creature from a higher slot
  (Charm Person, for one) takes one more.
- Adaptations: Empowered's rerolls and Careful's allies are chosen for the
  player; replacing an option on gaining a level is not offered yet.
- The default level-up now fills every training choice it offers.
- Verification: `opengold_sorcerer_tests` (two options at level 2;
  readying and a checkpoint; Quickened's Bonus Action and cost; Seeking's
  reroll; Distant's Touch at 30 feet; Transmuted's Cold; Careful sparing an
  ally; Twinned's second creature).
