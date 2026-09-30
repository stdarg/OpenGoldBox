# SRD weapon catalog

[EQ02 / #54](https://github.com/stdarg/OpenGold/issues/54) supplies all **38**
weapons in the [SRD 5.2.1 table, p. 91](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
plus the existing plain Wand focus. New entries are Greatclub, Sickle,
Greataxe, Lance, Maul, Rapier, Whip, Blowgun, Hand Crossbow, Heavy Crossbow,
Musket and Pistol. Existing class proficiency rules apply to every entry,
including Rogue Finesse/Light martial weapons and Monk Light martial weapons.

Each definition records weapon category, damage dice/type or fixed damage,
range, reach, hands, Finesse, Light, Heavy, Thrown, Loading, Versatile,
ammunition kind, mastery, weight and cost. Weights use exact quarter-pounds
and costs use copper pieces; the Sling's dash weight is represented as zero.
These catalog prices do not replace original campaign shop prices.
All weapon names and equipment explanations have English/Spanish translations.

The existing attack resolver applies ability/proficiency bonuses, damage,
critical hits, range, reach, handedness, Versatile and Heavy requirements.
Blowgun deals **1 Piercing damage** on a hit, with no ability modifier and no
damage dice to double on a critical or reroll with Savage Attacker (SRD p. 16).
Its attack roll still uses Dexterity and applicable proficiency.

Catalog metadata does not grant unimplemented property actions. Loading (#56),
Light extra attacks (#59), mastery actions (#60), and starting-package choices
remain tracked separately. [Ammunition tracking (#57)](AMMUNITION.md) was explicitly
declined: ranged ammunition is unlimited. [Thrown inventory (#58)](THROWN-WEAPONS.md)
remains physical weapon inventory. Lance records its one-handed mounted exception;
current actors are unmounted and use two hands. Mounted companion state and the
exception's activation belong to #173 before mounted play is claimed. Missing
weapon artwork uses the existing fallback; this increment adds no art or UI.

Original campaign conversions and prices remain intact. Type **45** keeps its
Fine Composite Long Bow / SRD Longbow interpretation, as shown by the reviewed
[artwork catalog](../data/art/combat-weapon-options.tsv) and the original ITEMS
weapon profile. A legacy generic type label saying Heavy Crossbow is not proof
that the item should use the newly added SRD definition. New native Heavy
Crossbows have their own `heavy_crossbow` key. Existing shop purchase/equip and
saved original provenance are regression-tested to prevent accidental remapping.

Combat recipes lack original-item provenance and retain their saved weapon and
exact outcome.
Player saving remains restricted to camp/inn.

[weapon_catalog_tests.cpp](../tests/weapon_catalog_tests.cpp) checks the complete
independent [source table](../tests/fixtures/weapons-srd-5.2.1.tsv), all twelve
classes using all 38 weapons, fixed normal/critical/miss rolls, damage, ranges,
reach, shields, rejected commands, normal purchases and campaign reconstruction.
Long-range limits beyond the 64-cell battlefield are checked as source metadata;
combat checks exercise reachable normal/long-range boundaries. Blowgun tests
include low/high Dexterity, critical hits and Savage Attacker. All nine Heavy
weapons also pass the [threshold suite](../tests/heavy_weapon_tests.cpp), and all
38 use their source damage types in actual immunity tests.
