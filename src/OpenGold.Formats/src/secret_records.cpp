#include "opengold/secret_records.h"
#include "little_endian.h"

#include <algorithm>

namespace opengold::secret
{
namespace
{

constexpr std::size_t creature_size = 439;

// The spell slot lists by their place in the record: cleric, druid, an unused
// list, magic-user; each holds seven spell levels.
constexpr std::size_t spell_slots_offset = 306, levels_a_list = 7;
constexpr std::array<std::size_t, 3> slot_lists{0, 1, 3};

constexpr int signed_byte(std::uint8_t value)
{
    return value < 128 ? value : static_cast<int>(value) - 256;
}

// Seven classes (cleric, druid, fighter, paladin, ranger, magic-user, thief);
// Secret has no monks.
por::ClassLevels class_levels(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return {bytes[offset],     bytes[offset + 1], bytes[offset + 2], bytes[offset + 3],
            bytes[offset + 4], bytes[offset + 5], bytes[offset + 6], 0};
}

// The two attacks' dice are kept field by field: both counts, both sides, both modifiers.
por::DamageDice attack_damage(std::span<const std::uint8_t> bytes, std::size_t counts,
                              std::size_t attack)
{
    return {bytes[counts + attack], bytes[counts + 2 + attack],
            signed_byte(bytes[counts + 4 + attack])};
}

} // namespace

std::optional<por::CharacterRecord> decode_creature(std::span<const std::uint8_t> b)
{
    if (b.size() != creature_size || b[0] > 15)
    {
        return std::nullopt;
    }
    por::CharacterRecord c;
    c.game = GoldBoxGame::secret_of_the_silver_blades;
    c.raw.assign(b.begin(), b.end());
    c.name.assign(b.begin() + 1, b.begin() + 1 + b[0]);
    // Each ability keeps a permanent score, then the current one.
    const auto ability = [&b](std::size_t index, std::size_t which)
    {
        return b[16 + 2 * index + which];
    };
    for (const std::size_t which : {0U, 1U})
    {
        auto &scores = which == 0 ? c.abilities : c.current_abilities;
        scores = {ability(0, which), ability(1, which), ability(2, which), ability(3, which),
                  ability(4, which), ability(5, which), b[29 - which]};
    }
    c.memorized_spells.assign(b.begin() + 30, b.begin() + 30 + 75);
    c.spell_byte_44_raw = b[105];
    c.base_thac0 = 60 - b[106];
    c.race = b[107];
    c.character_class = b[108];
    c.age = format::read_u16(b, 110);
    c.max_hit_points = b[112];
    c.spell_book.assign(b.begin() + 113, b.begin() + 113 + 117);
    c.attack_level = b[230];
    c.icon_size_raw = b[231];
    c.saves = {b[232], b[233], b[234], b[235], b[236]};
    c.base_movement = b[237];
    c.highest_level_raw = b[238];
    c.drained_levels = b[240];
    c.drained_hit_points = b[241];
    std::copy_n(b.begin() + 242, c.thief_skills.size(), c.thief_skills.begin());
    c.npc_flag = b[255];
    for (std::size_t i = 0; i < c.wealth.size(); ++i)
    {
        c.wealth[i] = format::read_u16(b, 259 + 2 * i);
    }
    c.levels = class_levels(b, 273);
    c.levels_before_class_change = class_levels(b, 280);
    c.gender = b[287];
    c.alignment = b[288];
    for (std::size_t i = 0; i < c.base_attacks.size(); ++i)
    {
        c.base_attacks[i] = {b[289 + i], attack_damage(b, 291, i)};
    }
    c.base_armor_class = 60 - b[297];
    c.strength_bonus_flag_raw = b[298];
    c.experience = format::read_u32(b, 300);
    c.rolled_hit_points = b[305];
    for (std::size_t list = 0; list < c.spell_slots.size(); ++list)
    {
        const auto first = spell_slots_offset + levels_a_list * slot_lists[list];
        std::copy_n(b.begin() + static_cast<std::ptrdiff_t>(first), c.spell_slots[list].size(),
                    c.spell_slots[list].begin());
    }
    c.base_xp_award = format::read_u16(b, 334);
    c.xp_per_hit_point = b[336];
    c.item_count = b[352];
    c.hands_equipped = b[409];
    c.save_bonus = signed_byte(b[410]);
    c.encumbrance = format::read_u16(b, 411);
    c.status = b[422];
    c.enabled = b[423];
    c.hostile = b[424];
    c.quick_fight = b[425];
    c.current.thac0 = 60 - b[426];
    c.current.armor_class = 60 - b[427];
    c.current.rear_armor_class = 60 - b[428];
    c.current.attacks_raw = {b[429], b[430]};
    for (std::size_t i = 0; i < c.current.damage.size(); ++i)
    {
        c.current.damage[i] = attack_damage(b, 431, i);
    }
    c.current.hit_points_raw = b[437];
    c.current.movement = b[438];
    return c;
}

} // namespace opengold::secret
