#include "opengold/champions_records.h"
#include "little_endian.h"

#include <algorithm>

namespace opengold::champions
{
namespace
{
int signed_byte(std::uint8_t value)
{
    return value < 128 ? value : static_cast<int>(value) - 256;
}

por::ClassLevels levels(std::span<const std::uint8_t> b, std::size_t at)
{
    return {b[at], b[at + 1], b[at + 2], b[at + 3], b[at + 4], b[at + 5], b[at + 6], b[at + 7]};
}

por::DamageDice damage(std::span<const std::uint8_t> b, std::size_t at, std::size_t attack)
{
    return {b[at + attack], b[at + 2 + attack], signed_byte(b[at + 4 + attack])};
}
} // namespace

std::optional<por::CharacterRecord> decode_creature(std::span<const std::uint8_t> b)
{
    if (b.size() != 409 || b[0] > 15)
    {
        return std::nullopt;
    }
    por::CharacterRecord c;
    c.game = GoldBoxGame::champions_of_krynn;
    c.raw.assign(b.begin(), b.end());
    c.name.assign(b.begin() + 1, b.begin() + 1 + b[0]);
    c.abilities = {b[16], b[18], b[20], b[22], b[24], b[26], b[29]};
    c.current_abilities = {b[17], b[19], b[21], b[23], b[25], b[27], b[28]};
    c.memorized_spells.assign(b.begin() + 30, b.begin() + 88);
    c.spell_byte_44_raw = b[88];
    c.base_thac0 = 60 - b[89];
    c.race = b[90];
    c.character_class = b[91];
    c.knight_order = b[92];
    c.deity = b[93];
    c.robe_order = b[94];
    c.age = format::read_u16(b, 96);
    c.max_hit_points = b[98];
    c.spell_book.assign(b.begin() + 99, b.begin() + 206);
    c.attack_level = b[206];
    c.icon_size_raw = b[207];
    c.saves = {b[208], b[209], b[210], b[211], b[212]};
    c.base_movement = b[213];
    c.highest_level_raw = b[214];
    c.drained_levels = b[216];
    c.drained_hit_points = b[217];
    c.undead_level = b[218];
    std::copy_n(b.begin() + 219, c.thief_skills.size(), c.thief_skills.begin());
    c.npc_flag = b[231];
    for (std::size_t i = 0; i < c.wealth.size(); ++i)
    {
        c.wealth[i] = format::read_u16(b, 235 + 2 * i);
    }
    c.levels = levels(b, 249);
    c.levels_before_class_change = levels(b, 257);
    c.gender = b[265];
    c.alignment = b[266];
    for (std::size_t i = 0; i < c.base_attacks.size(); ++i)
    {
        c.base_attacks[i] = {b[267 + i], damage(b, 269, i)};
    }
    c.base_armor_class = 60 - b[275];
    c.strength_bonus_flag_raw = b[276];
    c.experience = format::read_u32(b, 278);
    c.item_limit = b[282];
    c.rolled_hit_points = b[283];
    std::copy_n(b.begin() + 284, 5, c.spell_slots[0].begin());
    std::copy_n(b.begin() + 289, 5, c.spell_slots[1].begin());
    std::copy_n(b.begin() + 294, 5, c.deity_spell_slots.begin());
    std::copy_n(b.begin() + 299, 5, c.spell_slots[2].begin());
    c.base_xp_award = format::read_u16(b, 304);
    c.xp_per_hit_point = b[306];
    c.creature_type_flags = b[319];
    c.type = b[319]; // Champions uses the same byte for its type tests and flags.
    c.target_flags = b[320];
    c.item_count = b[322];
    c.hands_equipped = b[379];
    c.save_bonus = signed_byte(b[380]);
    c.encumbrance = format::read_u16(b, 381);
    c.magic_resistance = b[391];
    c.status = b[392];
    c.enabled = b[393];
    c.hostile = b[394];
    c.quick_fight = b[395];
    c.current.thac0 = 60 - b[396];
    c.current.armor_class = 60 - b[397];
    c.current.rear_armor_class = 60 - b[398];
    c.current.attacks_raw = {b[399], b[400]};
    for (std::size_t i = 0; i < c.current.damage.size(); ++i)
    {
        c.current.damage[i] = damage(b, 401, i);
    }
    c.current.hit_points_raw = b[407];
    c.current.movement = b[408];
    return c;
}
} // namespace opengold::champions
