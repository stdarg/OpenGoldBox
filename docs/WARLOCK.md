# Warlock

Levels 1–4 of the SRD 5.2.1 Warlock (pp. 70–76). Charisma is the spellcasting
ability; the earlier cantrip work is in [Eldritch Blast](ELDRITCH-BLAST.md) and
[Warlock Poison Spray](WARLOCK-POISON-SPRAY.md).

## Pact Magic, Hex, Hellish Rebuke and Magical Cunning

- Warlocks prepare spells from the Warlock list: two cantrips (three from
  level 4; True Strike joins the cantrip choices) and 2/3/4/5 prepared spells.
  A Long Rest changes none. Pact Magic slots are all one level, and all return
  on a Short Rest: one level-1 slot at level 1, two at level 2, then two
  level-2 slots at levels 3–4, which also cast level-1 spells (offered as
  "(level 2 slot)"). The implemented Warlock spells through level 2 are on
  the list. Replacing a spell or cantrip on gaining a level is not offered yet.
- **Hex** (level 1, Bonus Action, Concentration): the cursed creature takes
  1d6 more Necrotic damage from each of the Warlock's attack-roll hits; once
  it drops, **Move Hex** curses another as a Bonus Action. The chosen
  ability's Disadvantage on checks has no use here.
- **Hellish Rebuke** (level 1, Reaction): when an attack damages the Warlock
  and the attacker is within 60 feet and seen, play stops: "You are hurt.
  Cast Hellish Rebuke at the attacker, or decline." (React/Decline). The
  attacker makes a Dexterity save against 2d10 Fire (3d10 from a level-2
  slot), half on a success. Damage from saving throws and other sources does
  not prompt it yet.
- **Magical Cunning** (level 2, once per Long Rest): from the Camp dialog's
  Cast / Use, a one-minute rite restores expended Pact Magic slots, up to half
  the maximum (rounded up). It shares Arcane Recovery's store.
- Eldritch Invocations are below; the Fiend Patron arrives next.
- Verification: `opengold_warlock_tests` (one slot and Hex's Bonus Action and
  Necrotic, the Short Rest, Hellish Rebuke's question, checkpoint, save and
  slot, two slots at level 2 and Magical Cunning at camp, level-2 slots and
  "(level 2 slot)" casting at level 3, level 4), `opengold_eldritch_blast_tests`.

## Eldritch Invocations

- One invocation at creation and two more at level 2 (three through level 4),
  chosen like other training. Level 1 offers Armor of Shadows, Eldritch Mind
  and Pact of the Blade; level 2 adds those with a level-2 prerequisite.
- **Agonizing Blast**: + Charisma to Eldritch Blast's damage.
- **Repelling Blast**: an Eldritch Blast hit pushes a Large or smaller
  creature 10 feet away.
- **Eldritch Spear**: Eldritch Blast reaches 30 feet farther per Warlock level.
  These three apply to Eldritch Blast, the Warlock cantrip they suit, and are
  taken once each (adaptation: no other cantrip, not repeated).
- **Armor of Shadows**: an Action casts Mage Armor on the Warlock without a slot.
- **Fiendish Vigor**: an Action casts False Life on the Warlock without a slot,
  for 12 Temporary Hit Points.
- **Eldritch Mind**: Advantage on Constitution saves to keep Concentration.
- **Devil's Sight**: the Warlock sees through magical Darkness, which is now
  its own zone (Fog Cloud still hides everything); it attacks into Darkness
  with Advantage while its target cannot see it.
- **Lessons of the First Ones**: Alert or Savage Attacker (Magic Initiate is
  unavailable and Skilled's further choices are not offered here).
- **Pact of the Blade**: the Warlock's melee weapon is its pact weapon:
  proficiency, and Charisma when that is better. Conjuring the weapon, the
  bond and the damage type choice are not modeled.
- Not offered: Pact of the Tome (pending), Pact of the Chain and the
  invocations that only cast removed spells (DM-3); replacing an invocation on
  gaining a level.
- Verification: `opengold_warlock_tests` (the level-1 options, Armor of
  Shadows, Agonizing and Repelling Blast, Fiendish Vigor, Lessons' Alert,
  Pact of the Blade's attack bonus, Devil's Sight in Darkness),
  `opengold_training_tests` (a Warlock's invocation completes Training).
