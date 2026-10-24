#include "opengold/ecl_party_host.h"
#include "opengold/item_key.h"
#include "opengold/coin_purse.h"
#include <algorithm>
#include <array>
#include <cctype>
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

// Takes the class's stable ID, not its label, which is presentation only.
AdndGroup adnd_group(std::string_view class_id)
{
    if (class_id == "cleric" || class_id == "druid" || class_id == "monk")
    {
        return AdndGroup::cleric;
    }
    if (class_id == "wizard" || class_id == "sorcerer" || class_id == "warlock")
    {
        return AdndGroup::magic_user;
    }
    if (class_id == "rogue" || class_id == "bard")
    {
        return AdndGroup::thief;
    }
    return AdndGroup::fighter;
}

// The Gold Box race code that scripts read at character byte 0x72 (coab
// Classes/Enums.cs Race; 7 is human). Species the original lacks are
// non-human people, as its half-orc, never monsters (code 0).
std::uint16_t adnd_race_code(std::string_view species_id)
{
    using Code = std::pair<std::string_view, std::uint16_t>;
    static constexpr std::array<Code, 6> codes{
        {{"dwarf", 1}, {"elf", 2}, {"gnome", 3}, {"halfling", 5}, {"orc", 6}, {"human", 7}}};
    const auto match = std::ranges::find(codes, species_id, &Code::first);
    return match == codes.end() ? 6 : match->second;
}

// Gold Box THAC0 by level 1..12 (coab engine/ovr018.cs thac0_table, stored
// there as 60 - THAC0). Higher levels keep the level-12 value: the original
// table ends there and nothing is extrapolated.
int adnd_thac0(AdndGroup group, unsigned level)
{
    static constexpr std::array<std::array<int, 12>, 4> table{
        {{20, 20, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9},
         {20, 20, 20, 18, 18, 18, 16, 16, 16, 14, 14, 14},
         {21, 21, 21, 21, 21, 19, 19, 19, 19, 19, 17, 17},
         {20, 20, 20, 20, 19, 19, 19, 19, 16, 16, 16, 16}}};
    const auto row = static_cast<std::size_t>(group);
    return table[row][std::clamp(level, 1u, 12u) - 1];
}

// AD&D descending AC of worn armor (nearest original suit for SRD-only suits),
// or nullopt for items that are not armor. A shield improves AC by one.
std::optional<int> adnd_armor(std::string_view definition)
{
    static constexpr std::array<std::pair<std::string_view, int>, 12> suits{{{"padded", 8},
                                                                             {"leather", 8},
                                                                             {"studded_leather", 7},
                                                                             {"hide", 6},
                                                                             {"chain_shirt", 6},
                                                                             {"scale_mail", 6},
                                                                             {"breastplate", 5},
                                                                             {"half_plate", 4},
                                                                             {"ring_mail", 7},
                                                                             {"chain_mail", 5},
                                                                             {"splint", 4},
                                                                             {"plate", 3}}};
    const auto found = std::find_if(suits.begin(), suits.end(),
                                    [&](const auto &suit)
                                    {
                                        return suit.first == definition;
                                    });
    if (found == suits.end())
    {
        return std::nullopt;
    }
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
    values.thac0 = adnd_thac0(adnd_group(m.character.creation_data().character_class), sheet.level);
    int weapon_bonus = 0;
    for (const auto equipped : m.equipped)
    {
        const auto item = m.character.inventory().find(equipped);
        if (!item)
        {
            throw std::runtime_error("Equipped item is missing");
        }
        const auto source = m.item_sources.find(equipped);
        const int magic = source == m.item_sources.end() ? 0 : source->second.stored.magic_bonus;
        // The magic comes from the original record; the key's base names the item.
        const auto definition = rules::base_item_key(item->get().definition_id);
        if (definition == "shield")
        {
            values.armor_class -= 1 + magic;
        }
        else if (const auto suit = adnd_armor(definition))
        {
            values.armor_class -= 10 - *suit + magic;
        }
        else if (const auto type = source == m.item_sources.end() ? 0 : source->second.stored.type;
                 type <= 47 || type == 73)
        {
            // Weapon types: a ring's or bracers' bonus is not a weapon's (MAGIC-4).
            weapon_bonus = std::max(weapon_bonus, magic);
        }
    }
    values.thac0 -= weapon_bonus;
    return values;
}
} // namespace

bool party_has_item(const CampaignParty &party, unsigned type)
{
    for (auto id : party.state().slots)
    {
        if (id)
        {
            for (const auto &item : party.member(id).character.inventory().items())
            {
                if (std::cmp_equal(item.original_type, type))
                {
                    return true;
                }
            }
        }
    }
    return false;
}

unsigned party_strength(const CampaignParty &party)
{
    unsigned result = 0;
    for (auto id : party.state().slots)
    {
        if (id)
        {
            const auto &m = party.member(id);
            if (m.vitals.dead)
            {
                continue;
            }
            // The original per-member term (coab CMD_PartyStrength): Cleric level x 4,
            // magic-user level x 8, current HP, 5 per point of AC below 0 and
            // 5 per point of THAC0 below 21.
            const auto adnd = adnd_combat_values(m);
            const auto &sheet = m.character.sheet();
            const auto &class_id = m.character.creation_data().character_class;
            const auto group = adnd_group(class_id);
            const int cleric_levels = class_id == "cleric" ? static_cast<int>(sheet.level) : 0;
            const int magic_user_levels =
                group == AdndGroup::magic_user ? static_cast<int>(sheet.level) : 0;
            result +=
                static_cast<unsigned>(cleric_levels * 4 + m.vitals.hit_points +
                                      5 * std::max(0, -adnd.armor_class) +
                                      5 * std::max(0, 21 - adnd.thac0) + magic_user_levels * 8) /
                10;
        }
    }
    return result & 255;
}

namespace
{
// A member's thief skill as the character record's byte `field` holds it:
// picking a lock (0xA5, which Kovel Mansion's locks read) and finding or
// removing traps (0xA7) are Thieves' Tools checks, climbing walls (0xAB) an
// Athletics check.
unsigned thief_skill(const CampaignParty &party, MemberId id, unsigned field)
{
    switch (field)
    {
    case 0xA5:
    case 0xA7:
        return party.thieves_tools_chance(id);
    case 0xAB:
        return party.climbing_chance(id);
    default:
        throw std::runtime_error("Unsupported thief skill " + std::to_string(field));
    }
}

unsigned member_movement(const CampaignParty &party, MemberId id)
{
    const auto &m = party.member(id);
    return m.vitals.dead || m.vitals.hit_points == 0 ? 0 : party.profile(id).movement_feet * 2 / 5;
}
} // namespace

std::array<unsigned, 4> check_party(const CampaignParty &party, EclAddress attribute,
                                    unsigned effect)
{
    // The original's effect 19 is Find Traps, which Kovel Mansion's traps ask
    // about (ECL3:14 0xB3B4).
    constexpr unsigned find_traps_effect = 19;
    if (attribute == EclAddress{0})
    {
        if (effect != find_traps_effect)
        {
            throw std::runtime_error("Unsupported CHECK PARTY effect " + std::to_string(effect));
        }
        return {0, 0, 0, party.finding_traps() ? 1U : 0U};
    }
    const auto field = attribute.location - ecl_character_record.location;
    const bool movement = attribute == EclAddress{0x6C1B};
    if (effect || (!movement && (field < 0xA5 || field > 0xAC)))
    {
        throw std::runtime_error("Unsupported CHECK PARTY attribute/effect conversion");
    }
    unsigned count = 0, low = 255, high = 0, total = 0;
    for (auto id : party.state().slots)
    {
        if (id)
        {
            const unsigned value =
                movement ? member_movement(party, id) : thief_skill(party, id, field);
            low = std::min(low, value);
            high = std::max(high, value);
            total += value;
            ++count;
        }
    }
    if (!count)
    {
        return {};
    }
    // The fourth output is not recoverable from the scripts for movement; the
    // Slums' Run after a parley compares it with the monsters' speed, as the
    // encounter menu's Flee compares the slowest member, so it is the slowest
    // member too. For a thief skill the original leaves it 0.
    return {low, high, total / count, movement ? low : 0};
}

EclHostReply party_character_reply(const CampaignParty &party, PartySlot slot)
{
    if (slot.index > ecl_scanned_slots)
    {
        throw std::runtime_error("Invalid ECL party position");
    }
    std::array<std::uint16_t, 285> fields{};
    EclHostReply reply;
    if (const auto id = slot.index < 8 ? party.state().slots[slot.index] : MemberId{})
    {
        const auto &m = party.member(id);
        const auto &sheet = m.character.sheet();
        const auto &name = sheet.name;
        // The original keeps names in capitals, as its scripts compare them
        // (Skullcrusher's search, ECL4:2 0xAAA5).
        for (std::size_t n = 0; n < name.size() && n < 15; ++n)
        {
            fields[n] = static_cast<unsigned char>(
                std::toupper(static_cast<unsigned char>(name[n])));
        }
        fields[0x18] = sheet.scores[rules::Ability::constitution];
        fields[0x72] = adnd_race_code(m.character.creation_data().race);
        // The thief's climb-walls chance, which the Cadorna Textile House's
        // dry well rolls (ECL4:2 0xACC5).
        fields[0xAB] = party.climbing_chance(id);
        // Picking a lock and removing traps, which Kovel Mansion's lock and
        // traps roll (ECL3:14 0x9F91, 0xB3A2).
        fields[0xA5] = fields[0xA7] = party.thieves_tools_chance(id);
        fields[0x100] = m.vitals.dead ? 0 : 1;
        fields[0x119] = m.vitals.hit_points;
        // Every member shows the party's money: the party owns it all.
        const auto coins = script_coins(party.state().wealth);
        for (unsigned n = 0; n < 7; ++n)
        {
            fields[ecl_coin_addresses[n].location - ecl_character_record.location] =
                static_cast<std::uint16_t>(coins[n]);
        }
    }
    for (unsigned n = 0; n < fields.size(); ++n)
    {
        reply.writes.push_back(
            {EclAddress(static_cast<std::uint16_t>(ecl_character_record.location + n)), fields[n]});
    }
    reply.writes.push_back({EclAddress{0x6DB1}, static_cast<std::uint16_t>(slot.index)});
    reply.writes.push_back({EclAddress{0x6DB4}, static_cast<std::uint16_t>(slot.index)});
    return reply;
}
} // namespace opengold::por
