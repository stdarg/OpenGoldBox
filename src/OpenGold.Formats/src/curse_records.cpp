#include "opengold/curse_records.h"
#include "little_endian.h"

#include <algorithm>

namespace opengold::curse
{
namespace
{

constexpr std::size_t creature_size = 422;

constexpr int signed_byte(std::uint8_t value)
{
    return value < 128 ? value : static_cast<int>(value) - 256;
}

template <std::size_t N>
void copy(std::span<const std::uint8_t> bytes, std::size_t offset, std::array<std::uint8_t, N> &out)
{
    std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset), N, out.begin());
}

por::ClassLevels class_levels(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return {bytes[offset],     bytes[offset + 1], bytes[offset + 2], bytes[offset + 3],
            bytes[offset + 4], bytes[offset + 5], bytes[offset + 6], bytes[offset + 7]};
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
    c.game = GoldBoxGame::curse_of_the_azure_bonds;
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
    c.memorized_spells.assign(b.begin() + 30, b.begin() + 30 + 84);
    c.spell_byte_44_raw = b[114];
    c.base_thac0 = 60 - b[115];
    c.race = b[116];
    c.character_class = b[117];
    c.age = format::read_u16(b, 118);
    c.max_hit_points = b[120];
    c.spell_book.assign(b.begin() + 121, b.begin() + 121 + 100);
    c.attack_level = b[221];
    c.icon_size_raw = b[222];
    c.saves = {b[223], b[224], b[225], b[226], b[227]};
    c.base_movement = b[228];
    c.highest_level_raw = b[229];
    c.drained_levels = b[231];
    c.drained_hit_points = b[232];
    c.undead_level = b[233];
    copy(b, 234, c.thief_skills);
    c.npc_flag = b[247];
    for (std::size_t i = 0; i < c.wealth.size(); ++i)
    {
        c.wealth[i] = format::read_u16(b, 251 + 2 * i);
    }
    c.levels = class_levels(b, 265);
    c.levels_before_class_change = class_levels(b, 273);
    c.gender = b[281];
    c.type = b[282];
    c.alignment = b[283];
    for (std::size_t i = 0; i < c.base_attacks.size(); ++i)
    {
        c.base_attacks[i] = {b[284 + i], attack_damage(b, 286, i)};
    }
    c.base_armor_class = 60 - b[292];
    c.strength_bonus_flag_raw = b[293];
    c.experience = format::read_u32(b, 295);
    c.item_limit = b[299];
    c.rolled_hit_points = b[300];
    for (std::size_t list = 0; list < c.spell_slots.size(); ++list)
    {
        std::copy_n(b.begin() + static_cast<std::ptrdiff_t>(301 + 5 * list), 5,
                    c.spell_slots[list].begin());
    }
    c.base_xp_award = format::read_u16(b, 316);
    c.xp_per_hit_point = b[318];
    c.item_count = b[332];
    c.hands_equipped = b[389];
    c.save_bonus = signed_byte(b[390]);
    c.encumbrance = format::read_u16(b, 391);
    c.status = b[405];
    c.enabled = b[406];
    c.hostile = b[407];
    c.quick_fight = b[408];
    c.current.thac0 = 60 - b[409];
    c.current.armor_class = 60 - b[410];
    c.current.rear_armor_class = 60 - b[411];
    c.current.attacks_raw = {b[412], b[413]};
    for (std::size_t i = 0; i < c.current.damage.size(); ++i)
    {
        c.current.damage[i] = attack_damage(b, 414, i);
    }
    c.current.hit_points_raw = b[420];
    c.current.movement = b[421];
    return c;
}

} // namespace opengold::curse
