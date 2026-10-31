# Monsters without a shot: who lost it, and what it did to the fights

Audit date: 2026-10-31, on rules 0.6.264. It looks at the rule of 2026-10-30
([DEVIATIONS](../DEVIATIONS.md)): a human-shaped catalog monster shoots or throws
only with a ranged weapon in its gear. No game behaviour was changed by the audit;
the one bug it found is under [Side findings](#side-findings).

## Method

Two throwaway tools, not kept in the repository, ran on the game's own code: the
area loader, `wielded_items`, the SRD rules and the combat AI.

- **The list** asks the rules whether each area's converted creature has a ranged
  attack with no weapon key (the old rule) and with the key staging now gives it.
- **The fights** replay each creature's fight under both rules, on the same 100
  seeds, against the balance tool's mixed party of six. There are 152 setups:
  each creature on the Slums' street and Old Rope Guild battlefields, at the
  default challenge (33%) and at 100%, plus the Slums' leader-and-follower mixes.

The limits matter. Groups are up to 12 of one creature fitted to the XP budget,
not each script's real encounter. The party's level was chosen from the creature's
XP (25: 1, 50: 2, 100: 3, 200: 4, 450: 5, more: 7). The AI plays the party, and
every fight is on a Slums battlefield. Read the results as direction, not as a
measure of a particular encounter.

## Who lost the shot

Of 143 distinct creature records, 36 rows below could shoot before and cannot
now. No creature gained a shot. "Carries" is its weapons in the catalog.

| Creature | Rules definition | Carries | Where |
| --- | --- | --- | --- |
| 4th Lvl Fighter | `buccaneer` | Long sword +1 | Podol Plaza |
| Buccaneer | `buccaneer-raider` | Broad sword | Buccaneers' base |
| 8th Lvl Fighter | `castle-knight` | Two-handed sword +2 | Valjevo Castle, Valjevo old town |
| Gnoll | `gnoll-warrior` | flail | Slums |
| Gnoll | `gnoll-warrior` | Morning star | Eastern wilderness, Kuto's Well |
| Hassad | `hassad` | Long sword +2 | Nomad camp |
| Tyranithraxus | `impostor` | Long sword +5 | Valjevo Castle, Valjevo old town |
| 6th Lvl Thief | `kovel-master-thief` | Short sword | Valjevo Castle, Valjevo old town |
| 6th Lvl Thief | `kovel-master-thief` | Long sword +1 | Kovel Mansion |
| 6th Lvl Thief | `kovel-master-thief` | Long sword | Western wilderness |
| 1st Lvl Thief | `kovel-thief` | Long sword | Kovel Mansion, Valjevo Castle, Valjevo old town, Western wilderness |
| Buccaneer | `kovel-thief` | Broad sword | Western wilderness |
| Lizardman | `lizardfolk` | nothing | Central wilderness, Eastern wilderness, Kuto's Well, Lizard men's keep, Pyramid |
| Mad Man | `mad-man` | Great sword | Mendor's Library |
| Drythh | `mutant-lizardfolk` | Trident | Lizard men's keep |
| Mutant Liz-Man | `mutant-lizardfolk` | nothing | Central wilderness, Pyramid |
| Ogre | `ogre` | nothing | Cadorna Textile House, Slums |
| Ogre | `ogre` | club | Podol Plaza, Temple of Bane |
| Skeleton | `skeleton` | nothing | Cadorna Textile House, Sokal Keep, Valhingen Graveyard |
| Skullcrusher | `skullcrusher` | Hammer | Cadorna Textile House |
| Bugbear | `slums-bugbear` | battle axe | Slums, Stojanow Gate |
| Goblin Guard | `slums-goblin` | Short sword | Slums |
| Goblin Leader | `slums-goblin-leader` | Short sword | Slums |
| Hobgoblin | `slums-hobgoblin` | Long sword | Cadorna Textile House, Slums, Sokal Keep |
| Kobold | `slums-kobold` | Short sword | Central wilderness, Eastern wilderness, Kobold caves, Kuto's Well, Mendor's Library, Nomad camp, Slums |
| Kobold Leader | `slums-kobold-leader-sword` | Short sword | Slums |
| Orc | `slums-orc` | scimitar | Slums |
| Thri-Kreen | `thri-kreen` | nothing | Western wilderness |
| 6th Lvl Fighter | `trapped-fighter` | Long sword +1 | Pyramid, Stojanow Gate, Valjevo Castle, Valjevo old town |
| Envoy | `trapped-fighter` | Two-handed sword +2 | Kobold caves, Valjevo Castle, Valjevo old town |
| Princess Fatima | `trapped-fighter` | Short sword | Kobold caves |
| 7th Lvl Dw Figh | `trapped-veteran` | Lucern hammer +1 | Pyramid |
| 7th Lvl Fighter | `trapped-veteran` | Two-handed sword | Pyramid |
| Dwarven Fighter | `trapped-veteran` | Battle axe | Zhentil outpost |
| Wight | `wight` | nothing | Cadorna Textile House, Valhingen Graveyard |
| Corporal | `zhent-corporal` | Long sword | Western wilderness, Zhentil outpost |

## What it did to the fights

**Removing the shot did not make fights easier, and a few got harder.** Of
152 setups, 140 moved the party's remaining HP by under 4 points. 10 got harder by 4 or more;
2 got easier by that much, and those by 5 points at most.

The likely reason, inferred from the numbers and not traced turn by turn: the AI
shooting from afar deals little, while a monster with no shot walks up and hits
with its melee weapon. The strongest melee weapons show the largest change.

Setups that moved 4 points of HP or 3 of win rate, hardest first. "Fallen" is party
members dead or at 0 HP per fight:

| Fight | Party level | Challenge | Field | Enemies | Win % | Fallen | HP left % |
| --- | --- | --- | --- | --- | --- | --- | --- |
| impostor with longsword+5 | 7 | 100% | street | 6 | 100 → 83 | 0.08 → 1.33 | 72 → 54 |
| impostor with longsword+5 | 7 | 100% | guild | 6 | 100 → 87 | 0.08 → 1.05 | 74 → 57 |
| mad-man with greatsword | 1 | 100% | guild | 6 | 95 → 88 | 0.86 → 1.45 | 74 → 67 |
| castle-knight with greatsword+2 | 7 | 100% | guild | 6 | 13 → 0 | 5.34 → 6.00 | 7 → 0 |
| mad-man with greatsword | 1 | 33% | guild | 5 | 100 → 96 | 0.43 → 0.82 | 83 → 76 |
| castle-knight with greatsword+2 | 7 | 100% | street | 6 | 15 → 0 | 5.36 → 6.00 | 6 → 0 |
| trapped-fighter with greatsword+2 | 7 | 100% | guild | 6 | 99 → 96 | 0.10 → 0.34 | 74 → 69 |
| zhent-corporal with longsword | 1 | 33% | guild | 5 | 98 → 95 | 0.65 → 0.87 | 78 → 74 |
| trapped-fighter with greatsword+2 | 7 | 100% | street | 6 | 100 → 96 | 0.06 → 0.41 | 73 → 69 |
| mutant-lizardfolk with trident | 3 | 100% | guild | 6 | 100 → 99 | 0.01 → 0.13 | 82 → 78 |
| slums-kobold with shortsword | 1 | 100% | guild | 6 | 99 → 92 | 0.83 → 1.15 | 69 → 66 |
| slums-kobold with shortsword | 1 | 100% | street | 6 | 97 → 93 | 0.81 → 1.10 | 70 → 67 |
| slums-kobold-leader-sword with shortsword | 1 | 100% | street | 6 | 97 → 93 | 0.81 → 1.10 | 70 → 67 |
| slums-kobold-leader-sword with shortsword | 1 | 100% | guild | 6 | 99 → 92 | 0.83 → 1.15 | 69 → 66 |
| zhent-corporal with longsword | 1 | 100% | guild | 6 | 93 → 89 | 1.47 → 1.66 | 61 → 59 |
| zhent-corporal with longsword | 1 | 100% | street | 6 | 97 → 93 | 1.30 → 1.57 | 62 → 62 |
| slums-bugbear with battleaxe | 4 | 100% | street | 6 | 100 → 100 | 0.00 → 0.00 | 79 → 84 |
| slums-bugbear with battleaxe | 4 | 100% | guild | 6 | 100 → 100 | 0.00 → 0.00 | 79 → 84 |

Followers in the Slums' mixes barely matter: their leaders keep their bows.

## Side findings

- **Fixed: casting a damaging area spell from a higher slot failed.** Shatter from
  a level 3 slot threw "A die needs at least one side": the upcast added a die to a
  second damage roll the spell does not have. It was there under both rules. Rules
  0.6.265 fixes it, and `wizard_spell_tests` covers it. Before the fix, 53 fights
  in these runs stopped on it and are left out of the numbers above.
- **Not changed: two carried javelins cannot be thrown.** The Commandant
  (`MON6CHA:50`, Javelin of Lightning) and a 6th-level fighter (`MON5CHA:84`,
  Javelin +1) convert to the key `javelin:lightning`, which is no ranged weapon.
- **Not changed: a heavy crossbow shoots as a long bow.** The bandit captain
  (`MON6CHA:100`) and the buccaneer mate (`MON6CHA:101`) carry a heavy crossbow,
  whose item type converts to `longbow`.
- **Not changed: the catalog left out two original short bows.** The original records of
  the Giant Skeleton (`MON4CHA:22`) and the Vampire (`MON4CHA:23`) list a short
  bow; the catalog gives them none.
