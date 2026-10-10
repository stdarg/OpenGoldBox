#include "opengold/ecl_party_host.h"
#include "opengold/coin_purse.h"
#include <algorithm>
#include <map>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace opengold::por
{
namespace
{
// The original class groups whose THAC0 tables PARTY STRENGTH reads.
enum class AdndGroup
{
    fighter,
    cleric,
    magic_user,
    thief
};

AdndGroup adnd_group(std::string_view character_class)
{
    if (character_class == "Cleric" || character_class == "Druid" || character_class == "Monk")
        return AdndGroup::cleric;
    if (character_class == "Wizard" || character_class == "Sorcerer" ||
            character_class == "Warlock")
        return AdndGroup::magic_user;
    if (character_class == "Rogue" || character_class == "Bard")
        return AdndGroup::thief;
    return AdndGroup::fighter;
}

// Gold Box THAC0 by level 1..12 (coab engine/ovr018.cs thac0_table, stored
// there as 60 - THAC0). Higher levels keep the level-12 value: the original
// table ends there and nothing is extrapolated.
int adnd_thac0(AdndGroup group, unsigned level)
{
    static constexpr std::array<std::array<int, 12>, 4> table{{
            {20, 20, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9},
            {20, 20, 20, 18, 18, 18, 16, 16, 16, 14, 14, 14},
            {21, 21, 21, 21, 21, 19, 19, 19, 19, 19, 17, 17},
            {20, 20, 20, 20, 19, 19, 19, 19, 16, 16, 16, 16}
        }
    };
    const auto row = static_cast<std::size_t>(group);
    return table[row][std::clamp(level, 1u, 12u) - 1];
}

// AD&D descending AC of worn armor (nearest original suit for SRD-only suits),
// or nullopt for items that are not armor. A shield improves AC by one.
std::optional<int> adnd_armor(std::string_view definition)
{
    static const std::map<std::string_view, int> suits{
        {"padded", 8},      {"leather", 8},     {"studded_leather", 7},
        {"hide", 6},        {"chain_shirt", 6}, {"scale_mail", 6},
        {"breastplate", 5}, {"half_plate", 4},  {"ring_mail", 7},
        {"chain_mail", 5},  {"splint", 4},      {"plate", 3}};
    const auto found = suits.find(definition);
    if (found == suits.end())
        return std::nullopt;
    return found->second;
}

// PARTY STRENGTH reads an equivalent AD&D character: THAC0 by class and level,
// AC from armor and shield worn, with original magic bonuses on equipped items.
struct AdndCombatValues
{
    int thac0{20}, armor_class{10};
};

AdndCombatValues adnd_combat_values(const PartyMember &m)
{
    const auto &sheet = m.character.sheet();
    AdndCombatValues values;
    values.thac0 = adnd_thac0(adnd_group(sheet.character_class), sheet.level);
    int weapon_bonus = 0;
    for (const auto equipped : m.equipped)
    {
        const auto item = m.character.inventory().find(equipped);
        if (!item)
            throw std::runtime_error("Equipped item is missing");
        const auto source = m.item_sources.find(equipped);
        const int magic = source == m.item_sources.end() ? 0 : source->second.stored.magic_bonus;
        const auto &definition = item->get().definition_id;
        if (definition == "shield")
            values.armor_class -= 1 + magic;
        else if (const auto suit = adnd_armor(definition))
            values.armor_class -= 10 - *suit + magic;
        else
            weapon_bonus = std::max(weapon_bonus, magic);
    }
    values.thac0 -= weapon_bonus;
    return values;
}
} // namespace

bool party_has_item(const CampaignParty &party, unsigned type)
{
    for (auto id : party.state().slots)
        if (id)
            for (const auto &item : party.member(id).character.inventory().items())
                if (std::cmp_equal(item.original_type, type))
                    return true;
    return false;
}

unsigned party_strength(const CampaignParty &party)
{
    unsigned result = 0;
    for (auto id : party.state().slots)
        if (id)
        {
            const auto &m = party.member(id);
            if (m.vitals.dead)
                continue;
            // The original per-member term (coab CMD_PartyStrength): Cleric level x 4,
            // magic-user level x 8, current HP, 5 per point of AC below 0 and
            // 5 per point of THAC0 below 21.
            const auto adnd = adnd_combat_values(m);
            const auto &sheet = m.character.sheet();
            const auto group = adnd_group(sheet.character_class);
            const int cleric_levels =
                sheet.character_class == "Cleric" ? static_cast<int>(sheet.level) : 0;
            const int magic_user_levels =
                group == AdndGroup::magic_user ? static_cast<int>(sheet.level) : 0;
            result += static_cast<unsigned>(cleric_levels * 4 + m.vitals.hit_points +
                                            5 * std::max(0, -adnd.armor_class) +
                                            5 * std::max(0, 21 - adnd.thac0) +
                                            magic_user_levels * 8) /
                      10;
        }
    return result & 255;
}

std::array<unsigned, 4> check_party(const CampaignParty &party, unsigned address,
                                    unsigned effect)
{
    if (address != 0x6C1B || effect)
        throw std::runtime_error("Unsupported CHECK PARTY attribute/effect conversion");
    unsigned count = 0, low = 255, high = 0, total = 0;
    for (auto id : party.state().slots)
        if (id)
        {
            const auto &m = party.member(id);
            const unsigned move =
                m.vitals.dead || m.vitals.hit_points == 0 ? 0 : party.profile(id).movement_feet * 2 / 5;
            low = std::min(low, move);
            high = std::max(high, move);
            total += move;
            ++count;
        }
    // The fourth output is not recoverable from the scripts; the Slums' Run after
    // a parley compares it with the monsters' speed, as the encounter menu's
    // Flee compares the slowest member, so it is the slowest member too.
    return count ? std::array<unsigned, 4> {low, high, total / count, low} :
           std::array<unsigned, 4> {};
}

EclHostReply party_character_reply(const CampaignParty &party, unsigned slot)
{
    if (slot >= 8)
        throw std::runtime_error("Invalid ECL party position");
    std::array<std::uint16_t, 285> fields{};
    EclHostReply reply;
    if (const auto id = party.state().slots[slot])
    {
        const auto &m = party.member(id);
        const auto &sheet = m.character.sheet();
        const auto &name = sheet.name;
        for (std::size_t n = 0; n < name.size() && n < 15; ++n)
            fields[n] = static_cast<unsigned char>(name[n]);
        fields[0x18] = sheet.scores[2];
        fields[0x100] = m.vitals.dead ? 0 : 1;
        fields[0x119] = m.vitals.hit_points;
        const auto coins = script_coins(m.wealth);
        for (unsigned n = 0; n < 7; ++n)
            fields[ecl_coin_addresses[n] - 0x6B00] = coins[n];
    }
    for (unsigned n = 0; n < fields.size(); ++n)
        reply.writes.push_back({static_cast<std::uint16_t>(0x6B00 + n), fields[n]});
    reply.writes.push_back({0x6DB1, static_cast<std::uint16_t>(slot)});
    reply.writes.push_back({0x6DB4, static_cast<std::uint16_t>(slot)});
    return reply;
}
} // namespace opengold::por
