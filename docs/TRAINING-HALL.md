# Training halls (proposal, #9)

Pool of Radiance levels characters up only at New Phlan's training halls. In
OpenGoldBox, characters advance from the party roster's Advance button
anywhere. This page records what the original game does and the decisions
needed before [#9](https://github.com/stdarg/OpenGold/issues/9) can implement
it. The decisions below are approved; nothing is implemented yet.

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

## Decisions (2026-10-13)

1. **Halls by class**, following AD&D 2nd edition's class groups:
   - Fighters: Fighter, Barbarian, Paladin, Ranger.
   - Clerics: Cleric, Druid.
   - Magic-users: Wizard, Sorcerer, Warlock.
   - Thieves: Rogue, Bard, Monk.
2. **Levels are gained anywhere.** The roster's Advance button stays, as in
   other D&D games. This departs from the original. The halls instead grant
   experience and train skills.
3. **Gold belongs to the party**, not to a character. The original's
   per-character purses, pooled for party purchases, are not kept. Every
   payment, the halls' included, comes from the party's gold.
4. **Experience is kept.** Training does not clamp it, unlike the original.
5. **Controls.** The party roster, with the halls' services beside Advance.
   Screenshots of each state come before any code.

## Still open

- **Experience.** What a hall's experience costs, how much it grants and how
  often it can be bought.
- **Skills.** Which skills a hall teaches (its classes' skill lists?), at what
  cost, and how many a character can learn.
- **Party gold.** Replacing per-character purses touches shops, the temple,
  the inn, loot, the original scripts' money checks and the save format. It
  comes before the halls, since they spend the party's gold.

## What the decisions unlock

`PROGRAM 0` as a supported service:

- It opens the roster for the hall's services. A service is charged only when
  confirmed.
- Cancel, a member of another hall's class or too little gold change nothing.
- The script then resumes, as the temple and inn services already do.
- Training and payments are saved like other campaign state ([#10](https://github.com/stdarg/OpenGold/issues/10)).
