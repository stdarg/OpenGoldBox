# Training halls (proposal, #9)

Pool of Radiance levels characters up only at New Phlan's training halls. In
OpenGoldBox, characters advance from the party roster's Advance button
anywhere. This page records what the original game does and the decisions
needed before [#9](https://github.com/stdarg/OpenGold/issues/9) can implement
it. Nothing here is implemented or approved yet.

## What the original does

- **Four halls.** The cluebook lists separate training halls for clerics,
  magic-users, fighters and thieves (New Phlan map, entries 6–9).
  - Each hall's script (`ECL3.DAX` record 11) stores its class name and asks
    "WE TRAIN ONLY FIGHTERS HERE. DO YOU WANT TO TRAIN?". The class word comes
    from the hall.
  - **YES** runs `PROGRAM 0`, the engine's training service. The script then
    clears the picture and exits.
  - OpenGoldBox reports `PROGRAM 0` as unsupported and rolls the event back.
- **The service.** The manual (Training Hall, Train Character) describes it:
  - It shows the party creation menu, from which Train Character is used.
  - It asks who will train, checks that character's experience and subtracts
    the cost from that character's money.
  - The character gains one level, and no game time passes.
  - Train Character is also available "at the start of an adventure", from the
    party creation menu.
- **Cost and eligibility.** The [wish-goldbox measurements](https://wish-goldbox.readthedocs.io/en/latest/api/generated/goldbox.levelup.html)
  cover 29 trainings. They found:
  - A flat 1,000 gp at every level.
  - The character's remaining coins converted to platinum.
  - Training offered once experience reaches the next level's threshold.
  - Experience clamped to just below the following threshold after training.
- **Level limits.** The manual gives Phlan's highest levels: fighter 8,
  thief 9, cleric and magic-user 6. OpenGoldBox stops at level 4 for now.

## Decisions needed

1. **Which hall trains each SRD class?** A proposal following AD&D 2nd
   edition's class groups:
   - Fighters: Fighter, Barbarian, Paladin, Ranger.
   - Clerics: Cleric, Druid.
   - Magic-users: Wizard, Sorcerer, Warlock.
   - Thieves: Rogue, Bard, Monk.
2. **Does training replace the roster's Advance button**, as in the original,
   or sit beside it? If it replaces it, should the Character Pool and party
   creation still allow training "at the start of an adventure"?
3. **Cost.** 1,000 gp per level, from the trainee's own purse (the original),
   or from any member as the inn's payment works?
4. **Experience after training.** Clamp to just below the next threshold, as
   the original did, or keep the SRD's running total?
5. **Controls.** The original shows its party creation menu. A proposal is the
   party roster with Train in place of Advance, so only members of the hall's
   classes who have the experience and the gold can train. Screenshots of
   this would come before any code.

## What the answers unlock

`PROGRAM 0` as a supported service:

- It opens the existing level-up dialog for an eligible member and charges
  the fee only when the level is confirmed.
- Cancel, an ineligible member or too little gold change nothing.
- The script then resumes, as the temple and inn services already do.
- Training and payments are saved like other campaign state ([#10](https://github.com/stdarg/OpenGold/issues/10)).
