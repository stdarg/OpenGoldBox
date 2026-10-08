# Cluebook check (2026-10-08)

The original Pool of Radiance cluebook (`GameDocs/`, kept out of the
repository) was compared with what OpenGoldBox plays so far: the Slums and
Kuto's Well. It is used for facts only; nothing here is quoted from it.

## Confirmed

- **Kuto's Well's fights:** sickly kobolds wandering among the buildings,
  kobold guards with reinforcements out of the well, a lizardman with giant
  lizards guarding a door, an archer ambush in the catacombs that lasts until
  Norris falls, and Norris's band. These match the script and the area table.
- **Resting:** none at or under the well until Norris is beaten; afterwards the
  catacombs are safe. The script's rest profiles already do this.
- **Search mode:** the guards attack a party that searches near their
  buildings or the well; the hag's compartment and Norris's loot are found by
  searching. (The script itself hands over Norris's loot on stepping into his
  room once he is beaten; it does not check for searching.) In `ECL8:29`, `0x6DCA` = 1 raises the odds of finding a secret
  door and of meeting monsters, and gates the rug, the plaza ambush and the
  guard attacks, so it is the original Search mode (no longer a guess).
- **The Slums:** the four arguing orcs and Ohlo's potion errand match.

## Corrected

- The catacombs' arrows come from Norris's **kobold** archers, not bandits:
  each arrow now attacks at the SRD Kobold Warrior's +4 (was +3).

## Not done yet

- **Search mode** (an exploration command; `0x6DCA` = 1). Without it the hag's
  arms and armor, the plaza ambush and the guards' search trigger cannot be
  reached.
- **Norris's position:** he waits wherever the party first reaches one of
  several spots; only the first spot the tests reach is checked.
- **Slums set encounters** whose creatures have no SRD conversion yet, so they
  stop with a diagnostic: hobgoblins arguing over gold (`MON2CHA` 6), the
  monster leaders (ogre 8, gnolls 73, hobgoblin leaders 7), the trolls and
  ogres tossing things (troll 31, ogres 8), and Ohlo's or the potion seller's
  defenders if attacked (hobgoblins 6, a level-3 magic-user 94).
- **Clearing rewards:** the council pays for a cleared block (the Slums after
  its random encounters and set fights; Kuto's Well after Norris). Not checked
  or supported yet.
- **The simulator's Slums arc** covers roaming fights and the four orcs only,
  so its Slums success rates leave out the hard set fights above.
