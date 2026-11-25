#include "opengold/item_conversion.h"
#include "opengold/authored_items.h"
#include "opengold/item_key.h"
#include "opengold/magic_item_catalog.h"
#include "opengold/scroll_key.h"
#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace opengold
{
namespace
{
// The original's scroll item types (MAGIC-6).
constexpr std::uint8_t magic_user_scroll = 61, cleric_scroll = 62;

// The catalog key of an original weapon, ammunition, armor or shield type,
// the items that can be enchanted; empty for any other type.
std::string_view enchantable_conversion(int type)
{
    switch (type)
    {
    case 1:
        return "battleaxe";
    case 2:
        return "handaxe";
    case 3:
    case 5:
    case 10:
    case 11:
    case 14:
    case 15:
    case 16:
    case 17:
    case 40:
        return "glaive";
    case 4:
    case 18:
    case 19:
        return "halberd";
    case 6:
    case 22:
    case 33:
        return "quarterstaff";
    case 7:
        return "club";
    case 8:
        return "dagger";
    case 9:
        return "dart";
    case 12:
        return "flail";
    case 13:
    case 25:
    case 27:
    case 29:
    case 32:
        return "pike";
    case 20:
        return "warhammer";
    case 21:
        return "javelin";
    case 23:
        return "mace";
    case 24:
        return "morningstar";
    case 26:
        return "war_pick";
    case 30:
        return "scimitar";
    case 31:
        return "spear";
    case 34:
    case 35:
    case 36:
        return "longsword";
    case 37:
        return "shortsword";
    case 38:
        return "greatsword";
    case 39:
        return "trident";
    case 41:
    case 43:
    case 45:
        return "longbow";
    case 42:
    case 44:
        return "shortbow";
    case 46:
        return "light_crossbow";
    case 47:
        return "sling";
    case 50:
        return "leather";
    case 51:
        return "padded";
    case 52:
        return "studded_leather";
    case 53:
        return "ring_mail";
    case 54:
        return "scale_mail";
    case 55:
        return "chain_mail";
    // SRD 5.2.1 has no Banded Mail; Splint is its heavy armor with the same AD&D AC 4.
    case 56:
    case 57:
        return "splint";
    case 58:
        return "plate";
    case 59:
        return "shield";
    default:
        return {};
    }
}

// The original's special weapons, known by the name word START.EXE gives them.
constexpr std::uint8_t flame_tongue_word = 241, bane_undead_word = 253;
// Byte 62 of a sword bound to an alignment (#267).
constexpr std::uint8_t bound_to_alignment = 0x84;

rules::ItemPower item_power(const por::ItemRecord &raw)
{
    const auto named = [&](std::uint8_t word)
    {
        return std::find(raw.name_components.begin(), raw.name_components.end(), word) !=
               raw.name_components.end();
    };
    if (raw.cursed_raw)
    {
        return rules::ItemPower::cursed;
    }
    if (named(flame_tongue_word))
    {
        return rules::ItemPower::flame_tongue;
    }
    if (named(bane_undead_word))
    {
        return rules::ItemPower::bane_undead;
    }
    // The original's alignment-bound swords (#267): byte 62 is 0x84 and byte
    // 61's low four bits the alignment, in the original's order Lawful Good,
    // Lawful Neutral, Lawful Evil, Neutral Good and so on: a remainder of 0 by
    // 3 is Good, 2 Evil. Only the good-evil half binds (the user, 2026-10-22).
    if (raw.effect_codes[2] == bound_to_alignment)
    {
        switch ((raw.effect_codes[1] & 0x0F) % 3)
        {
        case 0:
            return rules::ItemPower::good;
        case 2:
            return rules::ItemPower::evil;
        default:
            break;
        }
    }
    return rules::ItemPower::none;
}

// The original's charged and single-use items, known by their spell or item
// power (byte 61), become their SRD counterparts (MAGIC-3); empty for any other
// record. Byte 62's bit 0x80 marks an effect applied when readied, not a use.
std::string_view charged_conversion(const por::ItemRecord &raw)
{
    constexpr std::uint8_t cure_light_wounds = 3, lightning_bolt = 51, speed = 80,
                           giant_strength = 82, javelin_lightning = 83, paralysis = 84,
                           healing = 85, disappearance = 86, missiles = 87, magic_missiles = 88,
                           bodily_health = 90;
    constexpr std::uint8_t wand_type = 78;
    const auto [charges, power, flags] = raw.effect_codes;
    // A scroll's bytes are its spells (scroll_conversion).
    if (!charges || (flags & 0x80) || raw.type == magic_user_scroll || raw.type == cleric_scroll)
    {
        return {};
    }
    switch (power)
    {
    case magic_missiles:
        return "wand:magic_missiles";
    case lightning_bolt:
        return raw.type == wand_type ? "wand:lightning_bolts" : "";
    case paralysis:
        return "wand:paralysis";
    case healing:
        return "potion_of_healing";
    // Potion Extra Healing's doses are Potions of Greater Healing.
    case cure_light_wounds:
        return "potion_of_greater_healing";
    case speed:
        return "potion_of_speed";
    case giant_strength:
        return "potion_of_giant_strength";
    case disappearance:
        return "dust_of_disappearance";
    case missiles:
        return "necklace_of_missiles";
    case javelin_lightning:
        return "javelin:lightning";
    case bodily_health:
        return "manual_of_bodily_health";
    default:
        return {};
    }
}

// The original's spells by number (START.EXE's table, MAGIC_ITEMS.md) as SRD
// 5.2.1 spells or stand-ins (MAGIC-6): "invisibility@3" is cast from a 3rd-level
// slot. Empty for a number no spell has, which random treasure can roll.
std::string_view scroll_spell(std::uint8_t number)
{
    static constexpr std::array<std::string_view, 57> spells{
        "",
        "bless",
        "bane",                          // Curse
        "cure_wounds",                   // Cure Light Wounds
        "inflict_wounds",                // Cause Light Wounds
        "detect_magic",                  // cleric
        "protection_from_evil_and_good", // Protection From Evil, cleric
        "protection_from_evil_and_good", // Protection From Good, cleric
        "protection_from_energy",        // Resist Cold
        "burning_hands",
        "charm_person",
        "detect_magic", // magic-user
        "enlarge_reduce",
        "enlarge_reduce", // Reduce
        "friends",
        "magic_missile",
        "protection_from_evil_and_good",
        "protection_from_evil_and_good",
        "read_magic",
        "shield",
        "shocking_grasp",
        "sleep",
        "find_traps",
        "hold_person",            // cleric
        "protection_from_energy", // Resist Fire
        "silence",
        "protection_from_poison", // Slow Poison
        "snake_charm",
        "spiritual_weapon", // Spiritual Hammer
        "see_invisibility", // Detect Invisibility
        "invisibility",
        "knock",
        "mirror_image",
        "ray_of_enfeeblement",
        "stinking_cloud",
        "enhance_ability", // Strength
        "animate_dead",
        "lesser_restoration", // Cure Blindness
        "blindness",          // Cause Blindness
        "lesser_restoration", // Cure Disease
        "contagion",          // Cause Disease
        "dispel_magic",       // cleric
        "bless",              // Prayer
        "remove_curse",
        "bestow_curse",
        "blink",
        "dispel_magic", // magic-user
        "fireball",
        "haste",
        "hold_person",    // magic-user
        "invisibility@3", // Invisibility, 10' Radius: two creatures
        "lightning_bolt",
        "magic_circle", // Protection From Evil, 10' Radius
        "magic_circle", // Protection From Good, 10' Radius
        "protection_from_normal_missiles",
        "slow",
        "greater_restoration"}; // Restoration
    return number < spells.size() ? spells[number] : std::string_view{};
}

// The original's magic-user and cleric scrolls (MAGIC-6): bytes 60-62 are the
// spells on it, and an arcane scroll is a magic-user's. Empty for another
// record; "por:unsupported" for a scroll with no spell left.
std::string scroll_conversion(const por::ItemRecord &raw)
{
    if (raw.type != magic_user_scroll && raw.type != cleric_scroll)
    {
        return {};
    }
    std::vector<std::string> spells;
    for (const auto number : raw.effect_codes)
    {
        if (const auto spell = scroll_spell(number); !spell.empty())
        {
            spells.emplace_back(spell);
        }
    }
    return spells.empty() ? "por:unsupported:" + std::to_string(raw.type)
                          : rules::scroll_key(raw.type == magic_user_scroll, spells);
}

// The original's rings, bracers, cloak, gauntlets and necklace (MAGIC-4), known
// by their item type or by the effect byte 62 applies when they are readied
// (byte 61 is its argument); empty for any other record.
std::string worn_conversion(const por::ItemRecord &raw)
{
    constexpr std::uint8_t gauntlets_type = 63, fire_resistance_type = 69,
                           cursed_necklace_type = 76, bracers_type = 77, feather_falling_type = 90,
                           cloak_type = 92, protection_type = 93;
    constexpr std::uint8_t ogre_power = 0x83, grants_effect = 0x81, cloak_effect = 0x85;
    constexpr std::uint8_t fire_resistance = 61, displacement = 89;
    const auto [charges, argument, effect] = raw.effect_codes;
    if (effect == ogre_power || raw.type == gauntlets_type)
    {
        return "gauntlets_of_ogre_power";
    }
    if ((effect == grants_effect && argument == fire_resistance) ||
        raw.type == fire_resistance_type)
    {
        return "ring_of_fire_resistance";
    }
    // A second Cloak of Displacement record has another item type.
    if ((effect == cloak_effect && argument == displacement) || raw.type == cloak_type)
    {
        return "cloak_of_displacement";
    }
    switch (raw.type)
    {
    case protection_type:
        return rules::item_key("ring_of_protection", raw.magic_bonus);
    case bracers_type:
        return rules::item_key("bracers", raw.magic_bonus);
    case feather_falling_type:
        return "ring_of_feather_falling";
    case cursed_necklace_type:
        return rules::item_key("necklace", raw.magic_bonus,
                               raw.cursed_raw ? rules::ItemPower::cursed : rules::ItemPower::none);
    default:
        return {};
    }
}

bool has_effects(const por::ItemRecord &raw)
{
    return std::any_of(raw.effect_codes.begin(), raw.effect_codes.end(),
                       [](auto n)
                       {
                           return n != 0;
                       });
}
} // namespace

std::string rules_key(std::string definition, const por::Equipment &source)
{
    if (source.identified())
    {
        return definition;
    }
    const auto enchantment = rules::parse_item_key(definition);
    const auto &raw = source.stored;
    const bool worn_magic = por::is_magic(raw) && !raw.effect_codes[0];
    const bool scroll = rules::parse_scroll_key(definition).has_value();
    if (enchantment.bonus || enchantment.power != rules::ItemPower::none || worn_magic || scroll)
    {
        definition += ":unidentified";
    }
    return definition;
}

unsigned carried_quantity(const por::Equipment &item)
{
    // A stack counts its items; a single charged item, its charges or doses.
    const unsigned stack = item.stored.stack_size;
    return stack > 1                                 ? stack
           : charged_conversion(item.stored).empty() ? 1u
                                                     : item.stored.effect_codes[0];
}

unsigned carried_quantity(const rules::EquipmentRules &rules, const por::Equipment &item)
{
    if (!rules.plays_original_records())
    {
        return carried_quantity(item);
    }
    // The original's charged items: an item, not a scroll, with a spell or
    // power and its charges (viewchar.pas, UseItem); 0 charges never run out.
    const auto &raw = item.stored;
    const auto &codes = raw.effect_codes;
    const bool scroll = item.base.worn_location > 10 && item.base.worn_location < 14;
    const bool charged = !scroll && codes[1] > 0 && codes[2] < 0x80 && codes[0] > 0;
    return raw.stack_size > 1 ? raw.stack_size : charged ? codes[0] : 1u;
}

std::optional<por::Equipment> item_after_uses(por::Equipment item, unsigned left)
{
    auto &raw = item.stored;
    if (!scroll_conversion(raw).empty())
    {
        unsigned entry = 0;
        for (std::size_t n = 0; n < raw.effect_codes.size(); ++n)
        {
            // A number no spell has is no entry on the scroll.
            if (scroll_spell(raw.effect_codes[n]).empty())
            {
                continue;
            }
            if (!(left & (1U << entry)))
            {
                raw.effect_codes[n] = 0;
                raw.raw[60 + n] = 0;
            }
            ++entry;
        }
        return rules::parse_scroll_key(scroll_conversion(raw)) ? std::optional{std::move(item)}
                                                               : std::nullopt;
    }
    if (!left)
    {
        return std::nullopt;
    }
    if (raw.stack_size > 1)
    {
        raw.stack_size = static_cast<std::uint8_t>(left);
    }
    else
    {
        raw.effect_codes[0] = static_cast<std::uint8_t>(left);
        raw.raw[60] = static_cast<std::uint8_t>(left);
    }
    return item;
}

std::string item_definition(const rules::EquipmentRules &rules, const por::Equipment &item)
{
    return rules.plays_original_records() ? por::original_item_key(item.stored)
                                          : equipment_conversion(item);
}

std::string equipment_conversion(const por::Equipment &item)
{
    const auto &raw = item.stored;
    const auto unsupported = "por:unsupported:" + std::to_string(raw.type);
    if (auto scroll = scroll_conversion(raw); !scroll.empty())
    {
        return scroll;
    }
    if (const auto charged = charged_conversion(raw); !charged.empty())
    {
        return std::string(charged);
    }
    if (auto worn = worn_conversion(raw); !worn.empty())
    {
        return worn;
    }
    if (const auto base = enchantable_conversion(raw.type); !base.empty())
    {
        const auto power = item_power(raw);
        // Charges (the Javelin of Lightning) are a power of their own, not an
        // enchantment. Flame Tongue's byte 61 and the bit-0x80 effects in byte
        // 62 are not decoded; the name word says what the weapon does.
        const bool decoded = !has_effects(raw) || power == rules::ItemPower::flame_tongue ||
                             (raw.effect_codes[0] == 0 && (raw.effect_codes[2] & 0x80));
        return decoded ? rules::item_key(base, raw.magic_bonus, power) : unsupported;
    }
    if (raw.magic_bonus || raw.cursed_raw || has_effects(raw))
    {
        return unsupported;
    }
    if (const auto ink = authored_item::magic_ink_level(raw.type))
    {
        return rules::magic_ink_key(ink);
    }
    switch (raw.type)
    {
    case 79:
        return "wand";
    case original_item::flask_of_oil:
        return "oil";
    case authored_item::torch:
        return "torch";
    case authored_item::acid:
        return "acid";
    case authored_item::alchemists_fire:
        return "alchemists_fire";
    case authored_item::poisoners_kit:
        return "poisoners_kit";
    case authored_item::basic_poison:
        return "basic_poison";
    case authored_item::thieves_tools:
        return "thieves_tools";
    case authored_item::magic_lock_picks:
        return "magic_lock_picks";
    case authored_item::boots_of_elvenkind:
        return "boots_of_elvenkind";
    case authored_item::cloak_of_elvenkind:
        return "cloak_of_elvenkind";
    case authored_item::stone_of_good_luck:
        return "stone_of_good_luck";
    case authored_item::wand_of_secrets:
        return "wand_of_secrets";
    default:
        return unsupported;
    }
}
} // namespace opengold
