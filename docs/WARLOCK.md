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
- Eldritch Invocations and the Fiend Patron arrive in the next increments.
- Verification: `opengold_warlock_tests` (one slot and Hex's Bonus Action and
  Necrotic, the Short Rest, Hellish Rebuke's question, checkpoint, save and
  slot, two slots at level 2 and Magical Cunning at camp, level-2 slots and
  "(level 2 slot)" casting at level 3, level 4), `opengold_eldritch_blast_tests`.
