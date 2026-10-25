# Western wilderness sites audit (2026-10-25)

What playing the western wilderness window's three sites needs, from the
user's DOS installation and the current engine. Sources: `opengold_scripts
--inspect GAME ECL6.DAX` 1, 19 and 28, the western window's `ECL6.DAX 25`
(the ways in), `opengold_creatures GAME`, `opengold_maps GAME`, and SRD
5.2.1.

## The dragon's cave (`ECL6:19`)

- **No map of its own:** the script tells its story over the wilderness view
  and returns the party to the window (`NEW ECL 25`).
- **The way in:** the window's (10,9), where the dragon seen over the
  mountains lands; the party may enter the large cave or ignore it.
- **Diogenes**, a silver dragon, speaks through a magic mouth, then in
  person. Asked for advice, pledged to, or offered a gift, he sends the party
  for the kobolds' silver bottle; once it has the bottle, he tells it the
  efreeti inside will help against Valhingen Graveyard's vampire. Challenged
  as Phlan's evil leader, he takes offence: apologized to, he helps; followed
  and attacked, he hurls a fireball (11d6) and is gone.

## The Zhentil outpost (`ECL6:28`)

- **Map:** `GEO6:28`, with `LOAD PIECES 1,2,9`.
- **The way in:** the window's (3,32), where Zhentil riders take a party
  carrying City Hall's message for the Zhentil keep (`0x4A98` = 255) for New
  Phlan's envoys and escort it to the commandant, who reads the papers.
- **The visit:** a tour, quarters the party must not leave (guards turn it
  back, take a bribe, or fight), a dinner where the commandant fishes for
  news of New Phlan and Old Phlan, and a night attack in the quarters (asleep,
  the party is struck as it sleeps).
- **The ways out:** escaping through the front gate in the commotion, being
  caught, robbed and thrown out, or fighting the garrison, its dwarven
  captain and at last the commandant with Al-Hyam Dazid, after which the
  outpost surrenders.

## The Buccaneers' base (`ECL6:1`)

- **Map:** `GEO6:1`, with `LOAD PIECES 2,6,6`.
- **The way in:** the window's (12,31), for a party with City Hall's
  commission to rescue the Bivant heir (`0x4A8C` = 255); buccaneers escort it
  to the merchants' camp.
- **The base:** the slave pen, where a boy with a heart-shaped birthmark
  waits and a merchant means to buy him; guard towers, barracks, animal pens
  (released, they stampede); a seller of passes and the captain's quarters.
- **The rescue:** the pen's guards beaten and its locked door bashed, the
  party takes the boy (`0x4AA9` = 254); waves of buccaneers follow, and their
  captain offers ransom (the party robbed and escorted out) or a fight.

## Monster conversions (MON6CHA)

| Record | Original | Conversion |
| --- | --- | --- |
| 41 | 4th Level Fighter (HP 30) | the nomad guards' Bandit Captain |
| 48 | Dwarven Fighter (HP 50) | the Pyramid's Warrior Veteran with 50 Hit Points |
| 50 | Commandant (HP 100) | **new:** SRD 5.2.1 Guard Captain with 100 Hit Points |
| 53, 100 | Aides, 3rd Level Fighter (HP 18) | the Central Wilderness's Bandit Captain |
| 54 | Corporal (HP 13) | the western wilderness's Guard |
| 86 | Al-Hyam Dazid (HP 22) | Valjevo Castle's mage profile |
| 96 | Guard (HP 4) | **new:** SRD 5.2.1 Guard with 4 Hit Points |
| 99 | Buccaneer (HP 4) | **new:** SRD 5.2.1 Pirate with 4 Hit Points |
| 101 | 5th Level Fighter (HP 39) | **new:** the Warrior Veteran with 39 Hit Points |
| 102 | Captain (HP 110) | **new:** SRD 5.2.1 Pirate Captain with 110 Hit Points (Captain's Charm, Riposte and the Rapier's Advantage not modeled) |

## Engine support

Missing before this audit, now done:

1. **The area rows** and their conversions.
2. **A script with no map of its own** (the dragon's cave): each area script
   carries its archive bank for `NEW ECL`.
3. **The sites' damage types:** Diogenes's fireball is fire; the guards'
   strokes on a sleeping party are slashing.
4. **The location label** names the base and the outpost.
5. **No area is refused any more:** every script the map's areas lead to is in
   the game.

## Party level and balance

With rests between fights, a level-7 party survives the outpost's night and
the base's waves and captain at the default encounter challenge.

## Status

- **Increment 1 (done, 2026-10-25):** all of the above, by
  [SITES-1](../SRD-DECISIONS.md#sites-1-2026-10-25-the-western-wilderness-sites).
  Played (`opengold_expedition_tests`, a new level-7 party with both City Hall
  commissions): Diogenes's advice, the outpost's escort, dinner, night attack
  and escape, and the base's slave pen, the Bivant heir freed, the waves and
  the captain fought, and the way out with him.
- **The whole map (2026-10-25):** played by one party from Mendor's Library,
  earning its commissions at City Hall ([the whole map](whole-map.md)). A party
  that opened Cadorna's family treasure gets the Zhentil keep's message only
  after the castle's evidence against him.
