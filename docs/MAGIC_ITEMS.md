# Original magic items

Every magic item Pool of Radiance can give the party, decoded from the user's
original files for issue
[#234](https://github.com/stdarg/OpenGold/issues/234). The game's documents
name only a few of them (the kobolds' bracers, rings of protection, the
graveyard sword, Ohlo's potion), so this list comes from the data itself.

## Sources

- **Fixed treasure.** `ITEM1.DAX` to `ITEM8.DAX` hold item lists. A TREASURE
  instruction in an area script names a list by number (below 128) from the
  bank with the same digit: `ECL4.DAX` takes lists from `ITEM4.DAX`. See
  [QUESTS.md](QUESTS.md#reward) for TREASURE operands. Lists were matched to
  the instructions shown by `opengold_scripts --inspect`. A few scripts load
  the number from a register or a table, and those were traced by hand.
- **Creature gear.** `MONnITM.DAX` record *r* is what creature `MONnCHA.DAX`
  record *r* carries ([creature-catalog.md](creature-catalog.md)). The party
  gets it as loot after the fight. A creature's areas are the scripts whose
  LOAD MONSTER names its record as a constant.
- **Random treasure.** TREASURE item code 128 + *n* rolls *n* items
  ([Random treasure](#random-treasure)).
- **Names.** An item's name is up to three words, each an index into a
  255-word table in `START.EXE` (EXEPACK-compressed; 21-byte Pascal strings,
  word 1 = "Battle Axe"). The words are shown last to first: components
  `(162, 224, 66)` read "Ring Of Protection +1". Before identification a magic
  item shows only its base word ("Ring", "Wand").
- **Spells.** Scroll and wand spell numbers index a second table in
  `START.EXE`, spells 1 to 56: 1 Bless, 3 Cure Light Wounds, 15 Magic Missile,
  47 Fireball, 51 Lightning Bolt, 56 Restoration, and so on.

The shops sell no magic items: none of the shop lists (`ITEM3.DAX` 52 to 55,
`ITEM1.DAX` 53) contains one.

### Regenerating the list

`opengold_magic_items GAME_DIR` prints every magic item in the item lists and
creature gear, with each record's type, bonus, curse, charges and spells, and
the scripts that name the list or creature by a constant. Lists it reports as
named by no constant were traced by hand below. `opengold_magic_item_tests`
checks the totals and the key facts on this page when `OPENGOLD_GAME_DIR` is
set.

### Record fields used here

Item records are 63 bytes ([creature-catalog.md](creature-catalog.md)).

| Byte | Meaning |
| --- | --- |
| 46 | Item type: an `ITEMS` template index (weapon 1-47, armor 50-58, shield 59, magic-user scroll 61, cleric scroll 62, bracers 77, wands 78-79, ring of protection 93, and others). |
| 47-49 | Name components. |
| 50 | Magic bonus: the +N of weapons, armor, shields and rings. On bracers it is the AC improvement (bracers +6 are "AC 4"). On wands, potions and other items it is not an enchantment. |
| 53 | Name words hidden until the item is identified: bit 4 hides byte 47's word, bit 2 byte 48's, bit 1 byte 49's. Most weapons, shields, rings, potions, wands and scrolls have 6 (only the base word shows: "Long Sword"); body armor has 4 (its "+N" is hidden); the Cursed Necklace has 5 ("Necklace"). Mundane items and a few magic ones (the anti-undead sword, Plate Mail +1, Leather Armor +1, Ring of Protection +1, the Efreeti Bottle, most two-spell clerical scrolls) have 0. |
| 54 | Cursed (1). |
| 60 | Charges or uses: wand charges, potion doses, the necklace's missiles. |
| 61 | Spell or power. 1-56 is a spell from the table above. Higher numbers are powers that only items have (see the tables). |
| 62 | Zero on charged items. With bit 0x80 set, it is an effect the item applies while readied, and byte 61 is the effect's argument (see [Byte 62](#byte-62-effects)). |

### Byte 62 effects

When byte 62 has bit 0x80 set, Pool of Radiance applies an effect while the
item is readied, and byte 61 is the effect's argument. This comes from
[por-go](https://github.com/retro-ware/por-go), a port of the game's code
(`readyItem` and `itemAffect`). Curse of the Azure Bonds reads the byte
differently, so coab is not a guide here.

| Item | Byte 62 | Byte 61 | Effect |
| --- | --- | --- | --- |
| Ring of Fire Resistance | 0x81 | 61 | Grants affect 61, fire resistance. |
| Gauntlets of Ogre Power | 0x83 | 38 | Strength 18/00 if that is higher. |
| Long Sword +2, the knight's tomb | 0x84 | 240 | Lawful Good only; another wielder takes 15 damage and loses it (#267). |
| Long Swords +3, Valjevo Castle | 0x84 | 82 | Lawful Evil only, 5 damage (#267). |
| Cloak of Displacement | 0x85 | 89 | Grants affect 89: the first attack roll against the wearer each fight misses. |
| Two-Handed Sword +1 +3 vs Undead | 0x88 | 3 | +2 to attack and damage rolls against the Undead. |

For 0x84, byte 61's low four bits are the alignment and its high four bits the
damage.

## Catalog

"Fixed" means a scripted treasure (the area and what happens there).
"Carried" means a creature that has the item. "Random" means the random
treasure generator can produce it.

### Weapons

| Item | Where |
| --- | --- |
| Long Sword +1 | Fixed: equipment stands in Sokal Keep (with Mace +2, Chain Mail +1, Shield +1); Lizard Man Keep hidden treasure. Carried: Norris the Gray (Kuto's Well); 4th-level fighters (Podol Plaza, New Phlan, Valjevo Castle, Buccaneer Base, Nomad Camp); 6th-level fighters (Stojanow Gate, Valjevo Castle); dwarven fighter (Zhentil Keep Outpost). Random. |
| Long Sword +2 | Fixed: the knight's tomb in Valhingen Graveyard (byte 62 = 0x84, byte 61 = 240). Carried: Hassad (Nomad Camp); the commandant (Zhentil Keep Outpost). Random. |
| Long Sword +2 Flame Tongue | Fixed: at the bottom of a deep well in Valjevo Castle NW, found by diving. |
| Long Sword +3 (two) | Fixed: two crossed swords behind an altar, Valjevo Castle SW (byte 62 = 0x84, byte 61 = 82). |
| Long Sword +4 | Carried: the buccaneer captain (Buccaneer Base). |
| Long Sword +5 | Carried: Tyranthraxus (record name TYRANITHRAXUS, Valjevo Castle). |
| Two-Handed Sword +1 +3 vs Undead | Fixed: City Hall, on accepting the Valhingen Graveyard commission (with eight Restoration scrolls' worth, see [Scrolls](#scrolls)). |
| Two-Handed Sword +2 | Fixed: the nomad shaman's reward (Nomad Camp). Carried: 6th-level fighter (New Phlan, Valjevo Castle NE); 8th-level fighter (New Phlan, Valjevo Castle tower); envoys (Valjevo Castle NE, Kobold Caves). |
| Two-Handed Sword +1 | Carried: "Hero" (no area traced). Random. |
| Broad Sword +1 | Carried: orc leader (Slums). Random. |
| Broad Sword -2, cursed | Fixed: the weapon pile in Kovel Mansion, guarded by a gas trap. |
| Short Sword +1 | Fixed: Temple of Bane hidden treasure; Kovel Mansion weapon pile. Carried: "Robber" (no area traced). Random. |
| Short Sword +2 | Fixed: the loot pile in Kovel Mansion. |
| Dagger +1 | Fixed: Temple of Bane; Kovel Mansion weapon pile (two). Random. |
| Dagger +2 | Carried: the nomad shaman; a 6th-level magic-user (no area traced). Random. |
| Mace +1 | Fixed: Temple of Bane. Carried: Mace, the half-orc cleric (Temple of Bane). Random. |
| Mace +2 | Fixed: Sokal Keep equipment stands. Carried: 7th-level cleric (New Phlan). Random. |
| Mace +3 | Carried: 5th-level cleric (Valjevo Castle SW). |
| Hammer +1 | Fixed: Temple of Bane; Kovel Mansion weapon pile; the wrecked smithy in Sokal Keep. Random. |
| Hammer +3 | Fixed: a fully equipped warrior's body in a Valhingen Graveyard crypt. |
| Morning Star +1, Scimitar +1 | Fixed: Temple of Bane; Kovel Mansion weapon pile. Random. |
| Spear +1, Hand Axe +1 | Fixed: Temple of Bane. Random. |
| Flail +1 | Carried: orc leader (Slums). Random. |
| Quarter Staff +1 | Fixed: a compartment under a rug at Kuto's Well. Random. |
| Lucern Hammer +1 | Carried: 7th-level dwarf fighter (no area traced). Random. |
| Javelin of Lightning | Carried: 6th-level fighter (New Phlan, Valjevo Castle NE); the commandant (Zhentil Keep Outpost). One use, power 83. Random (a preset). |
| Short Bow +1 | Fixed: Slums, the valuables cache and the treasure room (two lists). Random. |
| Arrow +1 | Fixed: Slums treasure room. Random. |
| Sling of Seeking +2 | Fixed: the warrior's body in the Valhingen Graveyard crypt. |

### Armor and shields

| Item | Where |
| --- | --- |
| Leather Armor +1 | Carried: goblin leader (Slums); "Robber" (no area traced). Random. |
| Leather Armor +4 | Fixed: Kovel Mansion loot pile. |
| Scale Mail +2 | Carried: Hassad (Nomad Camp). Random. |
| Chain Mail +1 | Fixed: Sokal Keep equipment stands; Kovel Mansion weapon pile. Carried: orc leader (Slums); 4th- and 6th-level fighters in many areas; dwarven fighter (Zhentil Keep Outpost). Random. |
| Banded Mail +1 | Fixed: Kuto's Well rug compartment. Random. |
| Banded Armor +2 | Carried: 7th-level cleric (New Phlan). Random. |
| Plate Mail +1 | Carried: 5th-level cleric (Valjevo Castle SW); "Hero" (no area traced). Random. |
| Plate Mail +2 | Fixed: the knight's tomb in Valhingen Graveyard. Carried: 8th-level fighter (New Phlan, Valjevo Castle tower); the commandant (Zhentil Keep Outpost). Random. |
| Plate Mail +3 | Carried: the buccaneer captain. |
| Shield +1 | Fixed: a chest under the Slums floorboards; Sokal Keep equipment stands; the knight's tomb and the vampire's hoard in Valhingen Graveyard. Carried: Hassad; the commandant. Random. |
| Shield +2 | Fixed: Kovel Mansion weapon pile; Lizard Man Keep hidden treasure. Carried: the buccaneer captain. |

### Rings and worn items

| Item | Where |
| --- | --- |
| Ring of Protection +1 | Fixed: Slums valuables cache. Carried: "Theurgist" (no area traced). Random. |
| Ring of Protection +2 | Carried: 6th-level magic-users (Stojanow Gate, Valhingen Graveyard). Random. |
| Ring of Protection +3 | Carried: Tyranthraxus; 8th-level fighter (New Phlan, Valjevo Castle tower); a 6th-level magic-user (no area traced). |
| Ring of Fire Resistance | Fixed: the knight's tomb in Valhingen Graveyard. Carried: Al-Hyam Dazid (Valjevo Castle SE, Zhentil Keep Outpost); the commandant. |
| Ring of Feather Falling | Fixed: under a trap door in Mace's mansion (Wealthy Area). |
| Bracers AC 6 | Carried: kobold leader (Slums). These are the Cluebook's kobold bracers. Random. |
| Bracers AC 4 | Fixed: Kuto's Well rug compartment. Carried: Yarash (Sorcerer's Island pyramid). Random. |
| Bracers AC 3 | Carried: Al-Hyam Dazid. |
| Bracers AC 2 | Carried: the nomad shaman. Its bonus byte is 6, the same as AC 4 bracers. |
| Cloak of Displacement | Fixed: Mendor's Library, where a reptilian monster wakes. Carried: a 6th-level magic-user (no area traced). Power 89. |
| Gauntlets of Ogre Power | Fixed: the Cadorna family chest (Cadorna Textile House). Carried: Tyranthraxus; dwarven fighter (Zhentil Keep Outpost). Random (a preset). |
| Necklace of Missiles | Carried: 5th-level cleric (Valjevo Castle SW). Six missiles, power 87. |
| Cursed Necklace (-5) | Fixed: the hobgoblin shaman's altar (Cadorna Textile House). |

### Wands

| Item | Charges, power | Where |
| --- | --- | --- |
| Wand of Magic Missiles | 20 to 67, power 88 | Fixed: Temple of Bane (20); the vampire's hoard in Valhingen Graveyard (33); the nomad shaman's reward (35). Carried: 3rd-level magic-users (Slums, New Phlan, 67); the nomad shaman (50); 7th-level thief (no area traced, 23). Random. |
| Wand of Lightning Bolt | 3, spell 51 | Fixed: scattered objects in Valhingen Graveyard. |
| Wand of Lightning | 20 to 67, spell 51 | Carried: Al-Hyam Dazid (20); Genheeris (Valjevo Castle tower, 56); a 6th-level magic-user (no area traced, 67). |
| Wand of Paralyzation | 23, power 84 | Carried: Yarash. |

### Potions and other charged items

| Item | Uses, power | Where |
| --- | --- | --- |
| Potion of Healing | 1, power 85 | Fixed: Temple of Bane; Mendor's Library; Valhingen Graveyard (scattered objects, vampire's hoard); the hobgoblin shaman's altar. Carried: 7th-level cleric (New Phlan); Yarash. Random (a preset). |
| Potion Extra Healing | 3, spell 3 (Cure Light Wounds) | Fixed: Mace's mansion; a jar under Mendor's Library floorboards (three); the warrior's body in the graveyard crypt. Carried: the commandant. Random (a preset). |
| Potion of Speed | 1, power 80 | Fixed: Temple of Bane; under a loose stone in Valjevo Castle NW. Carried: Yarash. |
| Potion of Giant Strength | 1, power 82 | Fixed: Temple of Bane; Mendor's Library. |
| Dust of Disappearance | 1, power 86 | Fixed: Temple of Bane. |
| Manual of Bodily Health | 1, power 90 | Fixed: a book buried in rubbish in Mendor's Library. |
| Efreeti Bottle | none recorded | Fixed: Kobold Caves. Opening it releases an efreeti who answers only when a vampire is near. |

### Scrolls

Each scroll holds one to three spells. The party can cast from a scroll of its
own kind of magic, and a magic-user can copy magic-user spells into the
spellbook.

| Where | Scrolls (spells) |
| --- | --- |
| City Hall, graveyard commission | Four cleric scrolls: Restoration, Restoration. |
| Valhingen Graveyard | Sixteen cleric scrolls of Restoration, Restoration: the crypt warrior (1), scattered objects (2), the knight's tomb (4), the vampire's hoard (5), a marble chest (4). The crypt warrior also has a magic-user scroll: Slow, Hold Person, Invisibility 10' Radius. |
| Mendor's Library | Cleric: Restoration, Restoration. |
| Slums, chest under the floor | Magic-user: Magic Missile. |
| Slums, orcs (carried) | Cleric: Cure Light Wounds, Cure Light Wounds. The Cluebook's detect magic hint about the orcs' loot. |
| Mace's mansion, trap door | Cleric: Animate Dead, Cure Blindness. |
| Temple of Bane | Magic-user: Fireball, Ray of Enfeeblement, Stinking Cloud. Cleric: Snake Charm, Silence 15' Radius, Bless. Cleric: Animate Dead, Cause Disease, Prayer. |
| Kovel Mansion, scroll cases | Magic-user: Blink, Burning Hands, Mirror Image. Magic-user: Fireball, Detect Invisibility, Reduce. Magic-user: Blink, Ray of Enfeeblement, Slow. Magic-user: Fireball, Haste, Hold Person. Cleric: Prayer, Remove Curse, Animate Dead. Cleric: Hold Person, Cure Blindness, Bestow Curse. |
| Cadorna Textile House, hobgoblin shaman | Magic-user: Blink, Burning Hands, Mirror Image. Magic-user: Fireball, Detect Invisibility, Reduce. |
| Kobold Caves, the kobolds' hoard | Magic-user: Friends, Invisibility, Protection From Normal Missiles. Magic-user: Charm Person, Protection From Evil, Strength. |
| 6th-level magic-user (carried, no area traced) | Magic-user: Fireball, Blink, Blink. |

## Treasure by area

Fixed treasure, with the list that holds it (`bank:record`).

| Area | Event | List |
| --- | --- | --- |
| Slums | Valuables cache: Short Bow +1, Ring of Protection +1 | 2:40 |
| Slums | Leather bag and the treasure room: Short Bow +1, Arrow +1 | 2:41 |
| Slums | Chest under a floorboard: Magic Missile scroll, Shield +1 | 2:42 |
| Mendor's Library | Book in the rubbish: Manual of Bodily Health | 2:1 |
| Mendor's Library | A waking reptilian monster: Restoration scroll, Cloak of Displacement, Potion of Healing, Potion of Giant Strength | 2:3 |
| Mendor's Library | Jar under the floor: three Potions Extra Healing | 2:5 |
| City Hall | Graveyard commission: Two-Handed Sword +1 +3 vs Undead, four Restoration scrolls | 3:51 |
| Kovel Mansion | Scroll cases: six scrolls | 3:25 |
| Kovel Mansion | Weapon pile (gas trap): Broad Sword -2 cursed, Chain Mail +1, Shield +2, Short Sword +1, Scimitar +1, Morning Star +1, Hammer +1, two Daggers +1 | 3:27 |
| Kovel Mansion | Loot pile: Leather Armor +4, Short Sword +2 | 3:56 |
| Wealthy Area, Mace's mansion | Trap door: Ring of Feather Falling, Potion Extra Healing, cleric scroll | 1:6 |
| Temple of Bane | The three hidden treasures: three scrolls (1:8); Dust of Disappearance, Wand of Magic Missiles, Potions of Speed, Healing and Giant Strength (1:9); Short Sword, Spear, Scimitar, Morning Star, Mace, Hammer, Dagger and Hand Axe, each +1 (1:17) | 1:8, 1:9, 1:17 |
| Sokal Keep | Untarnished equipment on stands in a glowing corner: Mace +2, Chain Mail +1, Long Sword +1, Shield +1 | 4:21 |
| Sokal Keep | Wrecked smithy: Hammer +1 | 4:19 |
| Cadorna Textile House | Hobgoblin shaman's altar: two scrolls, Potion of Healing, Cursed Necklace | 4:38 |
| Cadorna Textile House | Cadorna family chest: Gauntlets of Ogre Power | 4:39 |
| Valhingen Graveyard | Crypt warrior: Restoration scroll, Sling of Seeking +2, Hammer +3, Potion Extra Healing, magic-user scroll | 4:26 |
| Valhingen Graveyard | Scattered objects: two Restoration scrolls, Wand of Lightning Bolt, Potion of Healing | 4:28 |
| Valhingen Graveyard | Knight's tomb: four Restoration scrolls, Shield +1, Ring of Fire Resistance, Long Sword +2, Plate Mail +2 | 4:29 |
| Valhingen Graveyard | Vampire's hoard: five Restoration scrolls, Shield +1, Potion of Healing, Wand of Magic Missiles | 4:30 |
| Valhingen Graveyard | Marble chest: four Restoration scrolls | 4:59 |
| Valjevo Castle NW | Loose stone: Potion of Speed | 5:34 |
| Valjevo Castle NW | Bottom of a deep well, found by diving: Long Sword +2 Flame Tongue | 5:36 |
| Valjevo Castle SW | Crossed swords behind the altar: two Long Swords +3 | 5:35 |
| Nomad Camp | The shaman's reward: Wand of Magic Missiles, Two-Handed Sword +2 | 7:37 |
| Kobold Caves | The kobolds' hoard: two scrolls | 8:43 |
| Kobold Caves | Efreeti Bottle | 8:50 |
| Lizard Man Keep | Hidden treasure (from a table at `0xAC51`): Shield +2, Long Sword +1 | 8:31, 8:32 |
| Kuto's Well | Compartment under a rug: Bracers AC 4, Quarter Staff +1, Banded Mail +1 | 8:58 |

Three lists hold magic but were not reached from any script:

- `ITEM3.DAX` 26 is a copy of the crypt warrior's list (4:26).
- `ITEM4.DAX` 51 is a copy of the graveyard commission (3:51).
- The Bivant rescue reward in City Hall also sets item list 51 ("trinkets"),
  so as written it would hand out the commission's sword and scrolls again.
  This has not been checked in play.

## Random treasure

`src/OpenGold.Core/src/random_treasure.cpp` (see
[QUESTS.md](QUESTS.md#random-items)) can roll:

- +1 or +2 weapons, armor, shields and arrows.
- Rings of protection +1 or +2.
- Bracers AC 4 or AC 6.
- Magic-user and cleric scrolls of one to three spells.
- Potions of Healing, Extra Healing or Giant Strength (presets).
- The Wand of Magic Missiles, the Gauntlets of Ogre Power and the Javelin of
  Lightning (presets).

Its tables follow coab's reconstruction of Curse of the Azure Bonds. Two
details disagree with Pool of Radiance's own records:

- The presets' effect bytes differ. For example, the PoR Potion Extra Healing
  has `(3, 3, 0)` and the preset has `(3, 0x63, 0)`.
- In PoR, type 67 is the Cloak of Displacement, while the generator treats 67
  like gauntlets.

Any random item can come from Ohlo's reward (one item), the Slums treasure
(three), the Sorcerer's Island pyramid (three), the Kobold Caves (two) and the
wilderness lairs.

## In OpenGoldBox

Issue [#234](https://github.com/stdarg/OpenGold/issues/234) tracks the work;
[MAGIC-1](SRD-DECISIONS.md#magic-1-2026-10-17-the-originals-enchanted-weapons-and-armor)
records the decisions.

- **Enchanted weapons, ammunition, armor and shields** (#261) convert to an item
  key that carries the enchantment: `longsword+2`, `chain_mail+1`, `arrow+1`.
  A special weapon adds a power: `longsword+2:flame_tongue`,
  `greatsword+1:undead`, `longsword-2:cursed`. The format is in
  `src/OpenGold.Rules/include/opengold/item_key.h`. The weapon's bonus adds
  to attack and damage rolls; armor's and a shield's add to Armor Class.
  Magic Weapon adds nothing to a weapon that has its own bonus. Ammunition is
  not used up in a fight, so the best enchanted arrows or quarrels carried
  always add their bonus.
- **No item needs attunement**
  ([MAGIC-4](SRD-DECISIONS.md#magic-4-2026-10-20-no-attunement)), and there is
  no limit on how many magic items a character uses.
- **Flame Tongue**: its wielder gets a Bonus Action to light the blade (and
  another to douse it); while it is lit, each hit deals an extra 2d6 Fire
  damage. Every fight starts with the blade unlit, and its light is not
  modeled.
- **The cursed Broad Sword -2** cannot be unequipped, or replaced by another
  weapon, once equipped; in a fight its wielder cannot put it down to draw a
  bow or a Torch. Remove Curse, cast on the wielder in camp or in a fight,
  lets the wielder put the sword down, as in the original, and it is lost: at
  once in camp, or when the fight ends. In that fight the wielder may already
  draw another weapon.
- The special weapons are known by their name words: 241 Flame Tongue, 253 "+3
  vs Undead" and 255 Cursed (byte 54). The Long Swords +2 and +3 bound to an
  alignment by byte 62 have only their +N until
  [#267](https://github.com/stdarg/OpenGold/issues/267).
- **Wands, potions and other charged items** (#263,
  [MAGIC-3](SRD-DECISIONS.md#magic-3-2026-10-19-wands-potions-and-other-charged-items))
  do what their SRD counterparts do and keep the original's charges, which
  never recharge. Each converts by its spell or item power (byte 61): wands to
  a plain wand with a power (`wand:magic_missiles`, `wand:lightning_bolts`,
  `wand:paralysis`), the Javelin of Lightning to `javelin:lightning`, and the
  rest to items of their own. The inventory's quantity counts what is left
  ("23 charges", "6 missiles", or potions; Potion Extra Healing's three doses
  are three Potions of Greater Healing).
  - In a fight, from the Items menu: a potion is drunk, or given to a creature
    within 5 feet, as a Bonus Action; a held wand casts its spell with the
    Magic action at DC 15; a missile of the Necklace of Missiles explodes as an
    8d6 Fireball at DC 15; Dust of Disappearance, thrown with the Utilize
    action, turns its user and allies within 10 feet Invisible; the Javelin of
    Lightning's bolt (4d6 in a line, DC 13, then 4d6 more on its hit) works once
    until a Long Rest. Potions, missiles and dust are not spells, so
    Counterspell cannot stop them and a raging creature can use them.
  - A Thief's Fast Hands uses a wand, the necklace or dust as a Bonus Action.
  - Outside combat, the inventory's Use drinks a healing potion or a Potion of
    Giant Strength (whose hour carries into the next fight) and reads the
    Manual of Bodily Health: Constitution and its maximum rise by 2, and the Hit
    Point maximum follows as though the new modifier had counted from level 1.
  - Quick characters use their items, whether or not Quick magic is on.
  - An item with no charge or dose left is gone. Nothing needs attunement. The
    Efreeti Bottle stays the original's scripted quest item.
- **Rings, bracers, the cloak, gauntlets and necklaces** (#264,
  [MAGIC-5](SRD-DECISIONS.md#magic-5-2026-10-20-rings-bracers-and-other-worn-items))
  are worn from the inventory: two rings and one of each other kind, needing
  no hand. The character sheet lists them as "Worn" and adds their saving
  throw change to its saving throws. Each converts by its item type, or by the
  effect byte 62 applies when it is readied: `ring_of_protection+2`,
  `ring_of_fire_resistance`, `ring_of_feather_falling`, `bracers+4` (the
  original's Bracers AC 6), `cloak_of_displacement`,
  `gauntlets_of_ogre_power` and `necklace-5:cursed`.
  - A Ring of Protection adds its bonus to Armor Class and saving throws; only
    the better of two counts.
  - Bracers make Armor Class 10 + their improvement + Dexterity unless the
    armor's is better; a shield still adds.
  - The Cloak of Displacement adds 2 to Armor Class, and the first attack
    roll against its wearer in each fight misses.
  - Gauntlets of Ogre Power make Strength 19.
  - The Ring of Fire Resistance stops ordinary fire (a Torch, Alchemist's
    Fire, burning oil) and takes 2 per die from magical fire, at least 1 per
    die; saves against magical fire gain +4.
  - The Ring of Feather Falling does nothing.
  - The Cursed Necklace takes 5 from Armor Class and saves and stays on until
    Remove Curse, after which it is lost.
  - Unidentified, a worn item's note says only that it needs no hand.
  - Monsters keep their authored stat blocks; the worn items they carry come
    to the party as loot.
- **Scrolls** (#265,
  [MAGIC-6](SRD-DECISIONS.md#magic-6-2026-10-20-spell-scrolls)) convert with
  the spells their bytes 60-62 name, mapped to SRD 5.2.1 spells or stand-ins:
  `scroll:arcane:fireball,ray_of_enfeeblement,stinking_cloud`,
  `scroll:divine:greater_restoration,greater_restoration`,
  `scroll:arcane:invisibility@3` (Invisibility, 10' Radius). The format is in
  `src/OpenGold.Rules/include/opengold/scroll_key.h`. A spell number above 56,
  which random treasure can roll, holds nothing.
  - An arcane scroll is read by a Bard, Sorcerer, Warlock or Wizard, a divine
    one by a Cleric, Druid, Paladin or Ranger, and any by a level-13 Thief
    (Use Magic Device, with an Arcana check above level 1), once it is
    identified; the inventory lists an identified scroll's spells.
  - Quick members read a scroll's spell at an enemy, or to heal a creature at
    half its Hit Points or lower.
  - In a fight a spell is read from the Items menu ("Fireball (scroll)"), with
    the SRD scroll's DC. Outside combat, the inventory's Use opens the Cast
    menu with the spells that work there, and, for a Wizard, "Copy … into the
    spellbook" for each spell it can copy.
  - Reading or copying a spell erases it from the original record; the scroll
    goes with its last spell. A Wizard's copied spells are kept with the hall
    training (`boon:scroll:<spell>`), so a save rebuilds the spellbook.
  - Magic Ink is sold by the general store, 1000 gp a bottle; copying takes a
    bottle and two hours of game time for each spell level.
  - Shield, a reaction, can be copied from a scroll but not read from one: a
    reaction offers only a member's own spells.
- Fixed treasure lists show magic items under their original names ("Long
  Sword +2 Flame Tongue").
- **Identification** (#262,
  [MAGIC-2](SRD-DECISIONS.md#magic-2-2026-10-18-identifying-magic-items)): a
  found item's name leaves out the words byte 53 hides, everywhere it is
  shown, including the rules' labels and the character sheet. The campaign
  hands the rules such an item's key with `:unidentified` added, so the rules
  name it without its enchantment and leave out its sheet notes; its bonus,
  power and curse still work. A shop's ID, in the inventory while shopping,
  reveals any item's name for 200 gp from the party purse, even an item with
  nothing hidden, as in the original.
- **Identify and Detect Magic** are cast outside combat. When a member can
  cast Identify, the inventory offers "Cast Identify" on the selected item,
  any item, so the offer does not tell magic items from plain ones. Detect
  Magic is cast from the exploration screen's Cast menu (or Camp's Cast /
  Use); for ten minutes of game time the inventory marks each magic item
  (an enchantment, a curse, charges or a spell) with "* ". The Cast menu
  offers the selected member's spells and features that Camp's Cast / Use
  offers.

## Open questions

1. **Item powers above spell 56.** Powers 80 to 90 are named only by the items
   that use them (80 speed, 82 giant strength, 83 the javelin's lightning, 84
   paralysis, 85 healing, 86 disappearance, 87 the necklace's missiles, 88
   magic missiles, 89 displacement, 90 bodily health). What each one does in
   the original executable is not traced.
2. **Creatures with no traced area.** "Hero", "Theurgist", "Robber", the
   6th-level magic-user with the cloak and wand, the 7th-level thief and the
   7th-level dwarf fighter are never loaded by a constant record number. They
   may come from random encounter tables or registers.
3. **Pre-made parties.** The `SAVE` folder's characters were not checked for
   magic items.
