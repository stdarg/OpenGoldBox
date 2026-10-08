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

## Added since

- **Slums set encounters (2026-10-08):** the hobgoblins arguing over gold, the
  monster leaders (ogre, gnolls, hobgoblin leaders) and the trolls and ogres
  now fight with SRD conversions. Treasure a script adds to a fight is held
  until victory, and fixed item lists load from each area's `ITEMn.DAX`; the
  Slums' fixed lists now run without diagnostics, but no test yet wins the
  trolls' fight to see its treasure awarded. The expedition test
  fights the first two and brings the third to combat; the simulator plays all
  three. A party without Acid or Fire cannot kill a troll (Regeneration).

## Not done yet

- **Search mode** (an exploration command; `0x6DCA` = 1). Without it the hag's
  arms and armor, the plaza ambush and the guards' search trigger cannot be
  reached.
- **Norris's position:** he waits wherever the party first reaches one of
  several spots; only the first spot the tests reach is checked.
- **Clearing rewards:** the council pays for a cleared block (the Slums after
  its random encounters and set fights; Kuto's Well after Norris). Not checked
  or supported yet.
