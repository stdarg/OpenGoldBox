#include "opengold/curse_records.h"
#include "little_endian.h"
#include "record_fields.h"

#include <algorithm>

namespace opengold::curse
{
namespace
{

constexpr std::size_t creature_size = 422;

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
    c.abilities = format::paired_abilities(b, 16, format::AbilityScore::permanent);
    c.current_abilities = format::paired_abilities(b, 16, format::AbilityScore::current);
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
    c.saves = format::saving_throws(b, 223);
    c.base_movement = b[228];
    c.highest_level_raw = b[229];
    c.drained_levels = b[231];
    c.drained_hit_points = b[232];
    c.undead_level = b[233];
    format::copy_bytes(b, 234, c.thief_skills);
    c.npc_flag = b[247];
    for (std::size_t i = 0; i < c.wealth.size(); ++i)
    {
        c.wealth[i] = format::read_u16(b, 251 + 2 * i);
    }
    c.levels = format::class_levels(b, 265);
    c.levels_before_class_change = format::class_levels(b, 273);
    c.gender = b[281];
    c.type = b[282];
    c.alignment = b[283];
    c.base_attacks = format::base_attacks(b, 284);
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
    c.save_bonus = format::signed_byte(b[390]);
    c.encumbrance = format::read_u16(b, 391);
    c.status = b[405];
    c.enabled = b[406];
    c.hostile = b[407];
    c.quick_fight = b[408];
    c.current = format::current_stats(b, 409);
    return c;
}

} // namespace opengold::curse
