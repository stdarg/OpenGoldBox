# Warlock

Levels 1–15 of the SRD 5.2.1 Warlock (pp. 70–76). Charisma is the spellcasting
ability; the earlier cantrip work is in [Eldritch Blast](ELDRITCH-BLAST.md) and
[Warlock Poison Spray](WARLOCK-POISON-SPRAY.md).

## Pact Magic, Hex, Hellish Rebuke and Magical Cunning

- Warlocks prepare spells from the Warlock list: two cantrips (three from
  level 4; True Strike joins the cantrip choices) and 2/3/4/5 prepared spells.
  A Long Rest changes none. Pact Magic slots are all one level, and all return
  on a Short Rest: one level-1 slot at level 1, two at level 2, then two
  level-2 slots at levels 3–4, which also cast level-1 spells (offered as
  "(level 2 slot)"). The implemented Warlock spells through level 2 are on
  the list. On gaining a level one cantrip, one spell and one invocation may be
  replaced ([Replacing choices](LEVEL-SWAPS.md)).
- **Hex** (level 1, Bonus Action, Concentration): the cursed creature takes
  1d6 more Necrotic damage from each of the Warlock's attack-roll hits; once
  it drops, **Move Hex** curses another as a Bonus Action. The chosen
  ability's Disadvantage on checks has no use here.
- **Hellish Rebuke** (level 1, Reaction): when a creature within 60 feet that
  the Warlock sees damages it, play stops: "You are hurt. Cast Hellish Rebuke
  at the attacker, or decline." (React/Decline). The creature makes a
  Dexterity save against 2d10 Fire (3d10 from a level-2 slot), half on a
  success. An attack asks right after it; a spell or other damage, once the
  command is done ([Reactions to damage](REACTIONS.md)). Only one spell slot
  may be spent per turn: a Warlock who spent one on its own turn is not asked
  before that turn ends.
- **Magical Cunning** (level 2, once per Long Rest): from the Camp dialog's
  Cast / Use, a one-minute rite restores expended Pact Magic slots, up to half
  the maximum (rounded up). It shares Arcane Recovery's store.
- Eldritch Invocations and the Fiend Patron are below.
- Verification: `opengold_warlock_tests` (one slot and Hex's Bonus Action and
  Necrotic, the Short Rest, Hellish Rebuke's question, checkpoint, save and
  slot, two slots at level 2 and Magical Cunning at camp, once per Long Rest
  and not back after a Short Rest, level-2 slots and
  "(level 2 slot)" casting at level 3, level 4, Hellish Rebuke after Magic
  Missile), `opengold_eldritch_blast_tests`, and `opengold_character_tests`
  (a Warlock made page by page in the ordinary creator gets Pact Magic, its
  invocation, two skills, two cantrips and two spells).

## Eldritch Invocations

- One invocation at creation and two more at level 2 (three through level 4),
  chosen like other training. Level 1 offers Armor of Shadows, Eldritch Mind,
  Pact of the Blade and Pact of the Tome; level 2 adds those with a level-2
  prerequisite.
- **Agonizing Blast**: + Charisma to the chosen cantrip's damage.
- **Repelling Blast**: a hit with the chosen cantrip pushes a Large or smaller
  creature 10 feet away.
- **Eldritch Spear**: the chosen cantrip reaches 30 feet farther per Warlock
  level.
  Each names a damaging Warlock cantrip the Warlock knows (Pact of the Tome's
  count), offered as "Agonizing Blast (Fire Bolt)" and so on, and may be taken
  again for another cantrip. Repelling Blast is offered only for a cantrip with
  an attack roll, the only kind that hits, and Eldritch Spear only for one with
  a range of 10 feet or more. Replacing the cantrip is refused while a blast
  invocation still names it.
- **Armor of Shadows**: an Action casts Mage Armor on the Warlock without a slot.
- **Fiendish Vigor**: an Action casts False Life on the Warlock without a slot,
  for 12 Temporary Hit Points.
- **Eldritch Mind**: Advantage on Constitution saves to keep Concentration.
- **Devil's Sight**: the Warlock sees through magical Darkness, which is now
  its own zone (Fog Cloud still hides everything); it attacks into Darkness
  with Advantage while its target cannot see it.
- **Lessons of the First Ones**: Alert, Savage Attacker, Skilled or
  [Magic Initiate](MAGIC-INITIATE.md) with a list the Warlock does not already
  have; its spells are chosen on the level-up dialog's spell page, and
  Skilled's three skills on a checkbox page after the invocations (or after
  the Replace page, when it replaces another invocation).
- **Pact of the Blade**: the Warlock's melee weapon is its pact weapon:
  proficiency, and Charisma when that is better. Each attack may deal Necrotic,
  Psychic or Radiant damage instead of the weapon's type: A cycles to "Melee
  attack (Necrotic)" and the others. Conjuring the weapon and the bond are not
  modeled.
- **Pact of the Tome** (rules 0.6.172): three cantrips and two level-1 spells
  from any class's list, chosen on the spell page like Magic Initiate's. They
  are always prepared Warlock spells: Charisma, and the Pact Magic slots for
  the level-1 ones, with no free cast. The SRD's ritual requirement is waived
  ([TOME-1](SRD-DECISIONS.md#tome-1-2026-10-13-pact-of-the-tome-without-rituals)).
  The book itself is not modeled.
- Not offered: Pact of the Chain and the invocations that only cast removed
  spells (DM-3). An invocation, cantrip or spell can be replaced on gaining a
  level ([Replacing choices](LEVEL-SWAPS.md)).
- Verification: `opengold_warlock_tests` (the level-1 options, Armor of
  Shadows, Agonizing and Repelling Blast, Fiendish Vigor, Lessons' Alert,
  Pact of the Blade's attack bonus, Devil's Sight in Darkness; blast
  invocations for each cantrip, Agonizing Blast on Chill Touch, Lessons'
  Skilled and the pact weapon's Radiant damage),
  `opengold_godot_level_up_lab` (Skilled's page after the invocations),
  `opengold_magic_initiate_tests` (Pact of the Tome's choices and casting),
  `opengold_training_tests` (a Warlock's invocation completes Training).

## The Fiend Patron (level 3)

- **Dark One's Blessing**: when an enemy drops to 0 Hit Points, a Fiend Warlock
  that dropped it, or stands within 10 feet of it, gains Temporary Hit Points
  equal to its Charisma modifier + Warlock level (at least 1). The better
  Temporary Hit Points are kept automatically, as with Heroism, and the log
  says so. Adaptation: the creature acting (or reacting) when the enemy drops
  counts as the one who dropped it.
- **Fiend Spells**: Burning Hands, Command and Scorching Ray are always
  prepared (Suggestion is removed by DM-3); at levels 3–4 the level-1 ones
  are cast with the level-2 Pact Magic slots.
- Verification: `opengold_warlock_tests` (the grants, the always-prepared
  spells, 7 Temporary Hit Points from dropping a weakling, and from an ally
  dropping one 10 feet away but not 15).

## Levels 5–15

The Warlock advances to level 15, the cap
([LEVELS-1](SRD-DECISIONS.md#levels-1-2026-10-14-levels-510),
[LEVELS-2](SRD-DECISIONS.md#levels-2-2026-10-15-level-cap-15)).

- **Pact Magic** (SRD 5.2.1 p. 71): two slots of level 3 from 5, level 4 from
  7 and level 5 from 9, all back on a Short Rest; prepared spells 6, 7, 8, 9,
  10 and 10 at levels 5–10; a fourth cantrip at 10. The Warlock's level-3
  spells open at 5 (Counterspell, Dispel Magic, Fear, Hypnotic Pattern, Magic
  Circle, Remove Curse, Vampiric Touch), its level-4 spells at 7 (Banishment,
  Blight, Charm Monster, Dimension Door) and its level-5 spells at 9 (Hold
  Monster, Mislead). The cut ones stay cut.
- **Fiend Spells:** Fireball and Stinking Cloud are always prepared from 5,
  Fire Shield and Wall of Fire from 7, Insect Plague from 9; Geas is cut.
- **Eldritch Invocations:** two more at 5, one more at 7, 9, 12 and 15 (five,
  six, seven, eight and nine in all), chosen on the level-up's checkbox page; one held
  invocation may be replaced on each level-up up to the cap. New ones:
  - **Eldritch Smite** (5, Pact of the Blade): once on each of the Warlock's
    turns, right after a pact weapon hit, **Eldritch Smite (a Pact Magic
    slot)** spends a slot for 1d8 Force damage and 1d8 more per slot level
    (dice doubled on a critical hit); a Huge or smaller creature falls Prone.
  - **Thirsting Blade** (5, Pact of the Blade): Extra Attack when the Attack
    action's first attack is with the pact weapon.
  - **Lifedrinker** (9, Pact of the Blade): once on each of the Warlock's
    turns, right after a pact weapon hit, **Lifedrinker (Necrotic)**,
    **(Psychic)** or **(Radiant)** adds 1d6 of that type; while the Warlock is
    wounded and has Hit Point Dice, the **… spend a Hit Point Die** entries
    also roll one and heal its roll plus the Constitution modifier.
  - **Devouring Blade** (12, Thirsting Blade): three attacks with the pact
    weapon.
  - **Witch Sight** (15): Truesight to 30 feet: Invisible creatures there are
    seen, and magical Darkness there hides nothing from the Warlock.
  - **Gift of the Protectors** (9, Pact of the Tome): every party member
    counts as written in the Book of Shadows, which is not modeled (TOME-1,
    LEVELS-5). The first one dropped to 0 Hit Points but not killed outright
    drops to 1 instead; then no one can until the Warlock's Long Rest.
  - Ascendant Step, Gift of the Depths, Investment of the Chain Master,
    Master of Myriad Forms, Whispers of the Grave and Visions of Distant
    Realms stay out with what they need. Gaze of Two Minds is cut: the party
    is always in view
    ([CLOSE-1](SRD-DECISIONS.md#close-1-2026-10-29-the-last-srd-choices)). One with Shadows is cut:
    battle maps have no light levels (LEVELS-5).
- **Dark One's Own Luck** (Fiend 6): a failed saving throw (Concentration's
  too) that 10 more could pass gains 1d10 without asking, as roll effects do,
  shown with the save; as many times as the Charisma modifier (at least once)
  per Long Rest. Ability checks in fights gain it the same way: Athletics to
  break free, Stealth to hide, Investigation to leave a Maze and the Dispel
  Magic check (#185).
- **Contact Patron** (9) is cut with Contact Other Plane (DM-2).
- **Fiendish Resilience** (Fiend 10): a level-up choice of any damage type but
  Force, resisted from then on; after any rest the rest's training choices
  offer to change it, as the Hunter's Prey's are.
- **Ability Score Improvements** (8, 12).
- **Mystic Arcanum** (11, 13, 15): a level-up choice of one Warlock spell of
  level 6, then 7, then 8 (Circle of Death, Create Undead, Eyebite or True
  Seeing; Finger of Death or Forcecage; Befuddlement, Dominate Monster or
  Power Word Stun), each offered as "<spell> (Mystic Arcanum)" and cast once
  per Long Rest as a Magic action without a slot. A third Pact Magic slot at
  11; prepared spells 11, 11, 12, 12 and 13 at levels 11–15.
- **Hurl Through Hell** (Fiend 14): once on each of the Warlock's turns, right
  after any attack roll of its hits (Eldritch Blast's too), **Hurl Through
  Hell**: the creature makes a Charisma save against the Warlock's spell DC.
  On a failure it takes 8d10 Psychic damage unless it is a Fiend, and is gone
  until the end of the Warlock's next turn, Incapacitated; then it returns to
  its square or the nearest free one. Once per Long Rest, then **Hurl Through
  Hell (a Pact Magic slot)**.
- Verification: `opengold_warlock_levels_tests` (the campaign to 15,
  prepared counts, the fourth cantrip, nine invocations, the Fiend features'
  and Mystic Arcanum's grants, a level-15 round trip, the Fiend Spells, Eldritch Smite's Prone,
  Lifedrinker, Thirsting Blade's second attack, Dark One's Own Luck on a
  Concentration save, Fiendish Resilience after a Short Rest, a Mystic
  Arcanum once per Long Rest, Hurl Through Hell and the return, Devouring
  Blade's three attacks).
