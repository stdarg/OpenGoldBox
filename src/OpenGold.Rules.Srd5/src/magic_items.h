#ifndef OPENGOLD_SRD5_MAGIC_ITEMS_H
#define OPENGOLD_SRD5_MAGIC_ITEMS_H
#include "opengold/item_key.h"
#include <array>
#include <string_view>

namespace opengold::srd5::detail
{
// How an item is used (MAGIC-3).
enum class ItemUse
{
    drink,  // a potion: a Bonus Action, from the inventory
    wand,   // held: the Magic action spends a charge
    missile, // the Necklace of Missiles: a Magic action throws a missile
    dust,   // the Utilize action throws it into the air
    read,   // the Manual of Bodily Health, read in camp
    point   // the Wand of Secrets, pointed while exploring (SECRET-1)
};

// The original's charged and single-use items as their SRD 5.2.1 counterparts,
// and the Rogue's Shop's Wand of Secrets (SECRET-1). The Javelin of Lightning
// is a javelin with a power instead (item_key.h).
// The inventory's quantity counts the charges, missiles or potions left; `noun`
// names them when one item holds several ("23 charges", "6 missiles").
struct MagicItem
{
    std::string_view key, label;
    ItemUse use;
    std::string_view spell; // the spell or item effect it casts; empty for none
    std::string_view noun;
    std::string_view note;
};

inline constexpr std::array magic_items{
    MagicItem{"wand:magic_missiles", "Wand of Magic Missiles", ItemUse::wand, "magic_missile",
              "charges", "Equip it to cast Magic Missile with the Magic action, a charge each time."},
    MagicItem{"wand:lightning_bolts", "Wand of Lightning Bolts", ItemUse::wand,
              "lightning_bolt", "charges",
              "Equip it to cast Lightning Bolt (save DC 15) with the Magic action, a charge each "
              "time."},
    MagicItem{"wand:paralysis", "Wand of Paralysis", ItemUse::wand, "wand_of_paralysis",
              "charges",
              "Equip it to paralyze a creature within 60 feet with the Magic action, a charge each "
              "time: a Constitution save DC 15 or Paralyzed for up to a minute."},
    MagicItem{"potion_of_healing", "Potion of Healing", ItemUse::drink, "potion_of_healing", "",
              "Drink it as a Bonus Action to regain 2d4 + 2 Hit Points."},
    MagicItem{"potion_of_greater_healing", "Potion of Greater Healing", ItemUse::drink,
              "potion_of_greater_healing", "",
              "Drink it as a Bonus Action to regain 4d4 + 4 Hit Points."},
    MagicItem{"potion_of_speed", "Potion of Speed", ItemUse::drink, "potion_of_speed", "",
              "Drink it as a Bonus Action to be Hasted for a minute, without Concentration."},
    MagicItem{"potion_of_giant_strength", "Potion of Giant Strength", ItemUse::drink,
              "potion_of_giant_strength", "",
              "Drink it as a Bonus Action: your Strength becomes 21 for an hour."},
    MagicItem{"dust_of_disappearance", "Dust of Disappearance", ItemUse::dust,
              "dust_of_disappearance", "",
              "Throw it into the air with the Utilize action: you and allies within 10 feet are "
              "Invisible for 2d4 minutes."},
    MagicItem{"necklace_of_missiles", "Necklace of Missiles", ItemUse::missile, "fireball",
              "missiles",
              "Throw a missile with the Magic action: it explodes as a Fireball (save DC 15)."},
    MagicItem{"manual_of_bodily_health", "Manual of Bodily Health", ItemUse::read, "", "",
              "Read it in camp: Constitution and its maximum rise by 2. Its magic is then "
              "spent."},
    MagicItem{"wand_of_secrets", "Wand of Secrets", ItemUse::point, "", "charges",
              "Use it while exploring, a charge each time: it points out the nearest hidden "
              "secret door within 60 feet. Each Long Rest gives back 1d3 charges, up to 3."}};

// The item a key names, whether or not the campaign marked it ":unidentified".
inline const MagicItem *magic_item(std::string_view key)
{
    const auto parsed = rules::parse_item_key(key);
    const auto plain = rules::item_key(parsed.base, parsed.bonus, parsed.power);
    for (const auto &item : magic_items)
    {
        if (item.key == plain)
        {
            return &item;
        }
    }
    return nullptr;
}
} // namespace opengold::srd5::detail
#endif
