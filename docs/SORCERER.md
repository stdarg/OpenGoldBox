# Sorcerer

Levels 1–15 of the SRD 5.2.1 Sorcerer (pp. 64–70). Charisma is the
spellcasting ability; the earlier cantrip work is in
[Sorcerer cantrips](SORCERER-CANTRIPS.md).

## Spellcasting, Innate Sorcery and Draconic Sorcery

- Sorcerers now prepare spells from the Sorcerer list like Clerics: four
  cantrips (five from level 4) and 2/4/6/7 prepared spells at levels 1–4,
  chosen at creation and on gaining a level; a Long Rest changes none. Their
  slots match a Wizard's. The implemented Sorcerer spells through level 2 are
  on the list (Mind Spike included, as the inventory records). On gaining a
  level one cantrip and one spell may be replaced
  ([Replacing choices](LEVEL-SWAPS.md)).
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
  slot, converting one back, a checkpoint; at level 3 a made level-2 slot and
  one converted back, a Short Rest that restores no points and a Long Rest
  that restores them and removes the made slot).

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
  Disadvantage on every save against it while it lasts: the first one, the
  repeated saves of Blindness, Hold Person, Sleep, Hideous Laughter (also when
  damaged) and Ray of Enfeeblement, and later saves against Moonbeam, Flaming
  Sphere, Heat Metal, Gust of Wind, Grease and Web. The creature carries an
  unseen mark naming the caster and the spell for up to 10 minutes; casting the
  spell again replaces it.
- **Quickened** (2): Action spells are offered as Bonus Actions, including those aimed at any creature (Ray of Frost, Poison Spray).
- **Seeking** (1): a missed spell attack rolls its d20 again, once.
- **Subtle** is not offered: it only removed spell components, which the game
  does not have ([CLASS-11](SRD-DECISIONS.md#class-11-2026-10-06-no-spell-components)).
  A Sorcerer who chose it earlier keeps it, without effect.
- **Transmuted** (1): readied with a type (Acid, Cold, Fire, Lightning, Poison
  or Thunder); a spell dealing one of those deals the chosen one instead.
- **Twinned** (1): a spell that takes one more creature from a higher slot
  (Charm Person, for one) takes one more.
- Adaptations ([ADAPT-1](SRD-DECISIONS.md#adapt-1-2026-10-14-sorcerer-and-ranger-adaptations-approved)):
  Empowered's rerolls and Careful's allies are chosen for the player. A
  Metamagic option may be replaced on gaining a level
  ([Replacing choices](LEVEL-SWAPS.md)).
- The default level-up now fills every training choice it offers.
- Verification: `opengold_sorcerer_tests` (two options at level 2;
  readying and a checkpoint; Quickened's Bonus Action and cost; Seeking's
  reroll; Distant's Touch at 30 feet; Transmuted's Cold; Careful sparing an
  ally; Twinned's second creature; Heightened Sleep's mark through a
  checkpoint and the lower of two d20s on a repeated save).

## Levels 5–15

The Sorcerer advances to level 15, the cap
([LEVELS-1](SRD-DECISIONS.md#levels-1-2026-10-14-levels-510),
[LEVELS-2](SRD-DECISIONS.md#levels-2-2026-10-15-level-cap-15)).

- **Spellcasting** (SRD 5.2.1 p. 65): the Wizard's slots; prepared spells 9,
  10, 11, 12, 14, 15, 16, 16, 17, 17 and 18 at levels 5–15; a sixth cantrip
  at 10. Every kept Sorcerer spell of levels 3–8 opens with its slot: at 5
  Blink, Counterspell, Dispel Magic, Fear, Fireball, Haste, Hypnotic Pattern,
  Lightning Bolt, Protection from Energy, Sleet Storm, Stinking Cloud and
  Vampiric Touch; at 7 Banishment, Blight, Charm Monster, Confusion, Dimension
  Door, Dominate Beast, Fire Shield, Greater Invisibility, Ice Storm,
  Polymorph, Stoneskin, Vitriolic Sphere and Wall of Fire; at 9 Animate
  Objects, Arcane Hand, Cloudkill, Cone of Cold, Dominate Person, Hold
  Monster, Insect Plague, Telekinesis and Wall of Stone; at 11 Chain
  Lightning, Circle of Death, Disintegrate, Eyebite, Flesh to Stone, Freezing
  Sphere, Globe of Invulnerability, Sunbeam and True Seeing; at 13 Delayed
  Blast Fireball, Finger of Death, Fire Storm, Prismatic Spray and Reverse
  Gravity; at 15 Dominate Monster, Earthquake, Incendiary Cloud, Power Word
  Stun and Sunburst. They work as for the [Wizard](WIZARD.md#levels-515) and
  the [Cleric](CLERIC.md#levels-515). Slow waits for its decision (LEVELS-1);
  Teleport waits for travel beyond New Phlan; the cut spells stay cut.
- **Replacing choices:** on gaining any level up to 15, a cantrip, a spell
  and a Metamagic option may each be replaced
  ([Replacing choices](LEVEL-SWAPS.md)); the offers used to stop at level 4.
  The same holds for every class that prepares spells.
- **Sorcerous Restoration** (5): after a Short Rest, the rest's recovery
  choices offer **Sorcerous Restoration: regain N Sorcery Points**, half the
  Sorcerer level rounded down, up to the most it can hold; once per Long Rest.
- **Draconic Spells:** Fear is always prepared from 5, Charm Monster from 7
  and Summon Dragon from 9; Fly, Arcane Eye and Legend Lore are cut.
- **Elemental Affinity** (Draconic 6): a level-up choice of Acid, Cold, Fire,
  Lightning or Poison: Resistance to it, and the Charisma modifier added to the
  damage roll of a Sorcerer's spell of that type (an area spell's one roll; a
  single target's roll). Prismatic Spray's rays and a Breath Weapon take none.
- **Sorcery Incarnate** (7): with no Innate Sorcery use left, **Innate Sorcery
  (2 Sorcery Points)** starts it. While Innate Sorcery is active, a second
  Metamagic option may be readied for the same spell; each that applies is
  spent and paid when the spell is cast.
- **Font of Magic** creates slots up to level 5: level 3 from Sorcerer 5 (5
  Sorcery Points), level 4 from 7 (6) and level 5 from 9 (7); any slot turns
  into Sorcery Points equal to its level.
- **Metamagic** (10): two more options, none already known.
- **Ability Score Improvements** at 8 and 12.
- **Dragon Wings** (Draconic 14): **Dragon Wings**, a Bonus Action, spreads
  wings for an hour: the Sorcerer ignores Difficult Terrain and moves through
  enemies' squares, though it cannot stop in one (LEVELS-3: flight is cut).
  Once per Long Rest, then **Dragon Wings (3 Sorcery Points)**.
- Verification: `opengold_sorcerer_levels_tests` (the campaign to 15,
  prepared counts, the sixth cantrip, each feature's grant, Elemental
  Affinity's and the later Metamagic choices, a new slot level every two
  levels, a level-15 round trip, the Draconic spells, Fireball at 5, Font of
  Magic's level-5 slot both ways, Sorcerous Restoration once per Long Rest,
  two Metamagic options on one Fireball, Dragon Wings).
