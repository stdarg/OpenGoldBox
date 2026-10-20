#ifndef OPENGOLD_SRD5_WORN_ITEMS_H
#define OPENGOLD_SRD5_WORN_ITEMS_H
#include "opengold/item_key.h"
#include <array>
#include <string_view>

namespace opengold::srd5::detail
{
// Where a worn item goes. A character wears two rings and one of each other
// kind, as in the original (MAGIC-4).
enum class WornKind
{
    ring,
    bracers,
    cloak,
    gauntlets,
    necklace
};

// The original's rings, bracers, cloak, gauntlets and necklaces (MAGIC-4). A
// key's enchantment is the item's number: "ring_of_protection+2",
// "bracers+4" (the original's Bracers AC 6), "necklace-5:cursed". None needs
// attunement, and none needs a hand.
struct WornItem
{
    std::string_view key, label;
    WornKind kind;
    // What the rules call it before it is identified.
    std::string_view unidentified_label;
    // What it does, for the inventory and the character sheet.
    std::string_view note;
};

inline constexpr std::array worn_items{
    WornItem{"ring_of_protection", "Ring of Protection", WornKind::ring, "Ring",
             "Worn: adds its bonus to Armor Class and saving throws. Only the better of two "
             "Rings of Protection counts."},
    WornItem{"ring_of_fire_resistance", "Ring of Fire Resistance", WornKind::ring, "Ring",
             "Worn: ordinary fire deals no damage; magical fire deals 2 less per damage die (at "
             "least 1 per die), and saves against it gain +4."},
    WornItem{"ring_of_feather_falling", "Ring of Feather Falling", WornKind::ring, "Ring",
             "Worn: it would slow a fall, but no fall in this adventure needs it."},
    WornItem{"bracers", "Bracers", WornKind::bracers, "Bracers",
             "Worn: Armor Class is 10 + its bonus + the Dexterity modifier, or the armor's if "
             "better. A shield still adds."},
    WornItem{"cloak_of_displacement", "Cloak of Displacement", WornKind::cloak, "Cloak",
             "Worn: +2 to Armor Class, and the first attack roll against the wearer in each fight "
             "misses."},
    WornItem{"gauntlets_of_ogre_power", "Gauntlets of Ogre Power", WornKind::gauntlets,
             "Gauntlets", "Worn: Strength is 19 unless it is already higher."},
    WornItem{"necklace", "Necklace", WornKind::necklace, "Necklace",
             "Worn: its curse adds its penalty to Armor Class and saving throws."}};

// The worn item a key names, with or without its enchantment and flags.
inline constexpr const WornItem *worn_item(std::string_view key)
{
    const auto base = rules::base_item_key(key);
    for (const auto &item : worn_items)
    {
        if (item.key == base)
        {
            return &item;
        }
    }
    return nullptr;
}
} // namespace opengold::srd5::detail
#endif
