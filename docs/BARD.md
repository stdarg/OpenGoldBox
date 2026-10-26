# Bard

Levels 1–15 of the SRD 5.2.1 Bard (pp. 31–35). Charisma is the spellcasting
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
  bonus.
- **Expertise** (level 2): two skills the Bard is proficient in gain
  Expertise, chosen at level-up (LEVELS-3 replaced SCOPE-2's skip). A saved
  Bard already past 2 keeps loading and simply has none.
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

## Levels 5–15

The Bard advances to level 15, the cap
([LEVELS-1](SRD-DECISIONS.md#levels-1-2026-10-14-levels-510),
[LEVELS-2](SRD-DECISIONS.md#levels-2-2026-10-15-level-cap-15)).

- **Spellcasting** (SRD 5.2.1 p. 31): a full caster's slots; prepared spells
  9, 10, 11, 12, 14, 15, 16, 16, 17, 17 and 18 at levels 5–15; a fourth
  cantrip at 10. The kept Bard spells open with their slots: at 5 Bestow
  Curse, Dispel Magic, Fear, Hypnotic Pattern, Mass Healing Word, Plant
  Growth, Stinking Cloud and Tiny Hut; at 7 Charm Monster, Compulsion,
  Confusion, Dimension Door, Freedom of Movement, Greater Invisibility,
  Phantasmal Killer and Polymorph; at 9 Animate Objects, Dominate Person, Greater Restoration,
  Hold Monster, Mass Cure Wounds, Mislead and Raise Dead; at 11 Eyebite,
  Heroes' Feast, Irresistible Dance and True Seeing; at 13 Arcane Sword,
  Forcecage, Prismatic Spray, Regenerate and Resurrection; at 15
  Befuddlement, Dominate Monster, Mind Blank and Power Word Stun; Slow at 5,
  as for the Wizard. Teleport waits for travel beyond New Phlan.
- **Compulsion** (level 4, since 2026-10-25): the Bard picks creatures it can
  see within 30 feet one at a time, then casts; each makes a Wisdom save or is
  Charmed (Compulsion) while the Bard concentrates. A Bonus Action, "Compulsion:
  name a direction", aims at a square as an area spell is aimed (the user's
  choice, 2026-10-25); the direction from the Bard to it, to the nearest of
  eight, is the way they go. The aim starts on the nearest foe, so the
  computer sends the charmed away from its Bard. On its next
  turn each charmed creature's only move is the one taking it furthest that
  way, and then it saves again, ending the charm on a success. One that cannot
  move that way saves at the end of its turn. Adaptation: "the safest route"
  is not weighed. `opengold_bard_levels_tests` (compulsion) checks the
  choice, the aimed direction, the computer's aim, the named move, the second
  save and a checkpoint; `opengold_godot_compulsion` aims it through the
  combat window in English and Spanish.
- **Bardic Inspiration's die:** d8 from 5, d10 from 10, d12 from 15, for
  Bardic Inspiration and Cutting Words alike.
- **Font of Inspiration** (5): Bardic Inspiration's uses come back on a Short
  Rest, and **Font of Inspiration (a level-N slot)**, no action, turns a spell
  slot into one more use while some are spent.
- **Magical Discoveries** (Lore 6): a level-up choice of two spells from the
  Cleric, Druid and Wizard lists, cantrips or of level 1 to 3, always
  prepared. On each later level-up the Replace page offers to replace one with
  another from those lists, a cantrip or a spell of a level the Bard has slots
  for ([Replacing choices](LEVEL-SWAPS.md)).
- **Countercharm** (7): when the Bard or a creature of its side within 30
  feet fails a save against a spell that Charms or Frightens (Charm Person,
  Charm Monster, the Dominate spells, Hypnotic Pattern, Fear, Eyebite's
  panic, Irresistible Dance, Magic Jar), the Bard's Reaction rerolls it with
  Advantage, without asking, as roll effects are used.
- **Expertise** (9): two more skills.
- **Magical Secrets** (10): every spell chosen to prepare, then and on later
  level-ups, may come from the Bard, Cleric, Druid or Wizard lists.
- **Ability Score Improvements** at 8 and 12.
- **Peerless Skill** (Lore 14): when the Bard's own attack roll or ability
  check (an Athletics escape, hiding) fails by no more than its die, the die is
  rolled and added without asking; the use is spent only if it turns the roll
  into a success.
- Verification: `opengold_bard_levels_tests` (the campaign to 15, prepared
  counts, Expertise at 2 and 9, two Magical Discoveries, the features'
  grants, level 5's description, a level-15 round trip, Bardic Inspiration at
  each die, Font of Inspiration, Countercharm against a rival's Charm Person,
  Magical Secrets' Fireball and Spirit Guardians).
