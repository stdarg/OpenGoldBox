#include "equipment_rules.h"
#include "armor.h"
#include "feature_grants.h"
#include "training.h"
#include "weapons.h"
#include "worn_items.h"
#include "opengold/item_key.h"
#include <cstdlib>
#include <stdexcept>

namespace opengold::srd5::detail
{
using namespace rules;

std::string weapon_label(std::string_view key)
{
    std::string label;
    if (const auto *item = detail::weapon(key))
    {
        label = item->label;
    }
    else if (const auto *armor_item = detail::armor(key))
    {
        label = armor_item->label;
    }
    else if (const auto *worn = detail::worn_item(key))
    {
        label = rules::parse_item_key(key).unidentified ? worn->unidentified_label : worn->label;
    }
    else if (key == "stone_of_good_luck")
    {
        return "Stone of Good Luck";
    }
    else
    {
        return std::string(key);
    }
    // An unidentified item is named without its enchantment (#262).
    if (const auto enchantment = rules::parse_item_key(key);
        enchantment.bonus && !enchantment.unidentified)
    {
        const int bonus = enchantment.bonus;
        label += (bonus > 0 ? " +" : " -") + std::to_string(std::abs(bonus));
    }
    return label;
}

std::vector<Message> enchantment_messages(std::string_view key)
{
    const auto enchantment = rules::parse_item_key(key);
    std::vector<Message> messages;
    if (enchantment.unidentified)
    {
        return messages;
    }
    const auto bonus = (enchantment.bonus > 0 ? "+" : "") + std::to_string(enchantment.bonus);
    const auto *item = detail::weapon(key);
    if (enchantment.bonus && item)
    {
        messages.push_back(
            {"Source: equipped {item}. Its magic adds {bonus} to attack and damage rolls.",
             {{"item", weapon_label(key), true}, {"bonus", bonus}}});
    }
    else if (enchantment.bonus && detail::armor(key))
    {
        messages.push_back({"Source: equipped {item}. Its magic adds {bonus} to Armor Class.",
                            {{"item", weapon_label(key), true}, {"bonus", bonus}}});
    }
    else if (const auto *worn = detail::worn_item(key))
    {
        messages.push_back(
            {"Source: worn {item}. {note}",
             {{"item", weapon_label(key), true}, {"note", std::string(worn->note), true}}});
    }
    if (item && enchantment.power == rules::ItemPower::bane_undead)
    {
        messages.push_back({"Source: equipped {item}. Against Undead it adds 2 more.",
                            {{"item", weapon_label(key), true}}});
    }
    if (enchantment.power == rules::ItemPower::cursed)
    {
        messages.push_back(
            {"Source: equipped {item}. It is cursed: it cannot be unequipped until Remove Curse is cast on its wielder.",
             {{"item", weapon_label(key), true}}});
    }
    if (item && enchantment.power == rules::ItemPower::flame_tongue)
    {
        messages.push_back(
            {"Source: equipped {item}, a Flame Tongue. A Bonus Action lights the blade; while it is lit, hits deal 2d6 more Fire damage.",
             {{"item", weapon_label(key), true}}});
    }
    return messages;
}

std::string attack_ability(std::string_view key)
{
    const auto *item = detail::weapon(key);
    return item && item->finesse  ? "higher of Strength or Dexterity"
           : item && item->ranged ? "Dexterity"
                                  : "Strength";
}

bool reads_arcane_scrolls(CharacterClass klass)
{
    return klass == CharacterClass::wizard || klass == CharacterClass::sorcerer ||
           klass == CharacterClass::warlock || klass == CharacterClass::bard;
}

bool reads_divine_scrolls(CharacterClass klass)
{
    return klass == CharacterClass::cleric || klass == CharacterClass::paladin ||
           klass == CharacterClass::druid || klass == CharacterClass::ranger;
}

MulticlassTraining multiclass_training(CharacterClass klass)
{
    switch (klass)
    {
    case CharacterClass::barbarian:
        return {true, false, false, true};
    case CharacterClass::fighter:
    case CharacterClass::paladin:
    case CharacterClass::ranger:
        return {true, true, true, true};
    case CharacterClass::cleric:
        return {false, true, true, true};
    case CharacterClass::druid:
        return {false, true, false, true};
    case CharacterClass::bard:
    case CharacterClass::rogue:
    case CharacterClass::warlock:
        return {false, true, false, false};
    default:
        return {};
    }
}

bool trained(CharacterClass klass, std::span<const FeatureGrant> grants, std::string_view key)
{
    // Starting-class grants, SRD 5.2.1 pp. 49 and 61, plus the Protector Divine
    // Order's Martial weapon and Heavy armor training.
    // The Druid's Warden Primal Order: Martial weapons and Medium armor.
    const bool protector = detail::has_grant(grants, "order:protector");
    const bool warden = detail::has_grant(grants, "order:warden");
    // A class taken later gives part of its training (MULTI-1).
    MulticlassTraining later;
    for (const auto &grant : grants)
    {
        if (grant.id.starts_with(detail::multiclass_prefix))
        {
            const auto entry = multiclass_training(detail::class_from_id(
                std::string_view(grant.id).substr(detail::multiclass_prefix.size())));
            later.martial_weapons |= entry.martial_weapons;
            later.light_armor |= entry.light_armor;
            later.medium_armor |= entry.medium_armor;
            later.shields |= entry.shields;
        }
    }
    if (const auto *weapon = detail::weapon(key))
    {
        return detail::weapon_proficient(klass, *weapon) ||
               ((protector || warden || later.martial_weapons) && weapon->martial);
    }
    if (const auto *armor = detail::armor(key))
    {
        return detail::armor_trained(klass, armor->category) ||
               (protector && armor->category == detail::ArmorCategory::heavy) ||
               ((warden || later.medium_armor) &&
                armor->category == detail::ArmorCategory::medium) ||
               (later.light_armor && armor->category == detail::ArmorCategory::light) ||
               (later.shields && armor->category == detail::ArmorCategory::shield);
    }
    // Anyone can wear a ring, bracers, a cloak, gauntlets or a necklace, and
    // carry Magic Lock Picks or a Stone of Good Luck.
    if (detail::worn_item(key) || key == magic_lock_picks || key == stone_of_good_luck)
    {
        return true;
    }
    throw std::runtime_error("Unsupported equipment conversion: " + std::string(key));
}

std::string fighting_style_grant(std::string_view style)
{
    return style == "blessed_warrior" || style == "druidic_warrior"
               ? "feature:" + std::string(style)
               : "feat:" + std::string(style);
}
} // namespace opengold::srd5::detail
