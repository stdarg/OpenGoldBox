# Heavy weapons

[EQ03 / #55](https://github.com/stdarg/OpenGold/issues/55) implements the Heavy
property from [SRD 5.2.1 pp. 89, 91](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf).
A Heavy melee weapon requires Strength 13; a Heavy ranged weapon requires
Dexterity 13. Below that score, attacks with the weapon have Disadvantage.
This does not prevent equipping or attacking. It does not change damage,
initiative, saving throws, movement, spell attacks or fallback unarmed strikes.
Small species do not receive the obsolete 2014 Heavy penalty.

Glaive, Greataxe, Greatsword, Halberd, Lance, Maul, Pike, Heavy Crossbow and
Longbow carry the property. [The complete catalog](WEAPON-CATALOG.md) added the
four previously missing Heavy weapons in #54. Weapon category selects the required
ability, independently of the ability used to make an attack.

The same attack resolver handles normal and opportunity attacks. Heavy combines
with other Disadvantage sources without additional dice, and Advantage cancels
all Disadvantage sources. Existing Modifiers text names the weapon, required
ability, current score and result in English and Spanish. No controls change.
Equipping another weapon and ability increases recompute the requirement.

The penalty is derived from the existing character recipe, so it adds no saved
mutable state. Player saves remain restricted to camp or inn; checkpoints are
internal tests.

[heavy_weapon_tests.cpp](../tests/heavy_weapon_tests.cpp) covers all twelve
classes and all nine Heavy weapons, scores 12/13, independent seeded
normal/critical/miss damage, Small species, non-Heavy two-handed weapons,
spell/unarmed exceptions, multiple Disadvantage sources, cancellation,
opportunity attacks, stale commands, equipment rejection, level-four ASI and
campaign reconstruction. Below-threshold
creation cases respect the project's existing class-entry house prerequisites.
