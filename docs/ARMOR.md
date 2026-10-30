# SRD armor catalog and starting training

[EQ01 / #53](https://github.com/stdarg/OpenGoldBox/issues/53) implements the twelve
armor suits and Shield in [SRD 5.2.1 p. 92](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf).
The private rules catalog records category, AC, Strength threshold, Stealth
penalty, weight, copper-piece cost and don/doff time. Existing saved keys for
Leather, Chain Mail and Shield remain unchanged.

Light armor adds the full Dexterity modifier. Medium armor adds no more than
+2, while retaining negative modifiers. Heavy armor ignores Dexterity. Chain
Mail reduces speed by 10 feet below Strength 13; Splint and Plate do so below
15. Ring Mail has no Strength threshold. These rules apply to Dwarves too.
A trained Shield adds +2 AC; an untrained Shield gives no AC bonus and does not
prevent spellcasting. One suit and one Shield are allowed, subject to free hands
for wielding; spells need no free hand ([CLASS-11](SRD-DECISIONS.md#class-11-2026-10-06-no-spell-components)).
Armor replaces unarmored class AC calculations. Barbarian unarmored AC still
works with a Shield; Monk unarmored AC does not.

Starting training follows the twelve class tables:

| Classes | Training |
| --- | --- |
| Fighter, Paladin | Light, Medium, Heavy, Shield |
| Barbarian, Cleric, Ranger | Light, Medium, Shield |
| Druid | Light, Shield |
| Bard, Rogue, Warlock | Light |
| Monk, Sorcerer, Wizard | None |

Druid Warden and Cleric Protector add training as class choices; this table
describes starting core traits. Multiclassing is not planned.
Untrained suits impose Disadvantage on Strength and
Dexterity D20 Tests, including actual initiative and weapon attacks, and
prevent casting. Saving throws use the existing shared modifier resolver.
The equipped ability-check query combines training with these penalties and
armor-specific Dexterity (Stealth) Disadvantage. Alternate-ability Stealth
checks do not receive the armor-specific penalty. Sourced Advantage remains a
separate flag so a roll can cancel opposing modifiers. Campaign skill
actions consuming this query were dropped ([#172](https://github.com/stdarg/OpenGoldBox/issues/172), SCOPE-2).

Existing inventory equip/unequip and Modifiers presentation support every
catalog key. Explanations and names are localized in English and Spanish.
Campaign tests equip each entry, reject conflicting armor, retain spent
resources and wounds, save/reconstruct, and adapt the same equipment into the
next encounter. Original campaign item conversions and prices are preserved;
this increment does not reinterpret unsupported original armor or award
starting packages; characters buy their gear
([START-1](SRD-DECISIONS.md#start-1-2026-10-11-starting-gold-as-the-original-game-gave-it)).

Donning/removal is instantaneous in the inventory flow. Don/doff timing is not
planned ([#198](https://github.com/stdarg/OpenGoldBox/issues/198), SCOPE-1). In
combat, taking off a shield to use a bow takes the Utilize action.
No new controls or in-combat save access are introduced here.

The previously missing Chain Mail Stealth penalty derives from the equipped
item; no saved resources or choices are replaced.

[armor_catalog_tests.cpp](../tests/armor_catalog_tests.cpp) checks the independent
[source table](../tests/fixtures/armor-srd-5.2.1.tsv), all 156 class/equipment
combinations in real combat, AC/Strength boundaries, ability checks, saving-throw
penalties, spell restrictions, shields, stale commands and campaign continuation.
The Godot localization checks cover readable armor names and Stealth explanations.
