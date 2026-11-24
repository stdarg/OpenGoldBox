#include "opengold/death_knights_records.h"
#include "little_endian.h"

#include <algorithm>

namespace opengold::death_knights
{
namespace
{
int signed_byte(std::uint8_t value)
{
    return value < 128 ? value : static_cast<int>(value) - 256;
}

por::ClassLevels levels(std::span<const std::uint8_t> bytes, std::size_t at)
{
    return {bytes[at],     bytes[at + 1], bytes[at + 2], bytes[at + 3],
            bytes[at + 4], bytes[at + 5], bytes[at + 6], bytes[at + 7]};
}

por::DamageDice damage(std::span<const std::uint8_t> bytes, std::size_t at, std::size_t attack)
{
    return {bytes[at + attack], bytes[at + 2 + attack], signed_byte(bytes[at + 4 + attack])};
}
} // namespace

std::optional<por::CharacterRecord> decode_creature(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() != 216 || bytes[0] > 15)
    {
        return std::nullopt;
    }
    por::CharacterRecord record;
    record.game = GoldBoxGame::death_knights_of_krynn;
    record.raw.assign(bytes.begin(), bytes.end());
    record.name.assign(bytes.begin() + 1, bytes.begin() + 1 + bytes[0]);
    record.abilities = {bytes[16], bytes[18], bytes[20], bytes[22],
                        bytes[24], bytes[26], bytes[29]};
    record.current_abilities = {bytes[17], bytes[19], bytes[21], bytes[23],
                                bytes[25], bytes[27], bytes[28]};
    record.spell_byte_44_raw = bytes[30];
    record.base_thac0 = 60 - bytes[31];
    record.race = bytes[32];
    record.character_class = bytes[33];
    record.knight_order = bytes[34];
    record.deity = bytes[35];
    record.robe_order = bytes[36];
    record.age = format::read_u16(bytes, 38);
    record.max_hit_points = bytes[40];
    record.icon_size_raw = bytes[41];
    record.saves = {bytes[42], bytes[43], bytes[44], bytes[45], bytes[46]};
    record.base_movement = bytes[47];
    record.highest_level_raw = bytes[48];
    record.undead_level = bytes[49];
    std::copy_n(bytes.begin() + 50, record.thief_skills.size(), record.thief_skills.begin());
    record.npc_flag = bytes[62];
    for (std::size_t at = 0; at < 3; ++at)
    {
        record.wealth[4 + at] = format::read_u16(bytes, 63 + 2 * at);
    }
    record.levels = levels(bytes, 69);
    record.levels_before_class_change = levels(bytes, 77);
    record.gender = bytes[85];
    record.alignment = bytes[86];
    for (std::size_t at = 0; at < record.base_attacks.size(); ++at)
    {
        record.base_attacks[at] = {bytes[87 + at], damage(bytes, 89, at)};
    }
    record.base_armor_class = 60 - bytes[95];
    record.strength_bonus_flag_raw = bytes[96];
    record.experience = format::read_u32(bytes, 98);
    // This base HP byte doubles as a monster's XP award per HP.
    record.rolled_hit_points = bytes[108];
    record.xp_per_hit_point = bytes[108];
    record.base_xp_award = format::read_u16(bytes, 109);
    record.creature_type_flags = bytes[122];
    record.type = bytes[122];
    record.target_flags = bytes[123];
    record.item_count = bytes[124];
    record.hands_equipped = bytes[181];
    record.save_bonus = signed_byte(bytes[182]);
    record.encumbrance = format::read_u16(bytes, 183);
    record.magic_resistance = bytes[193];
    record.status = bytes[199];
    record.enabled = bytes[200];
    record.hostile = bytes[201];
    record.quick_fight = bytes[202];
    record.current.thac0 = 60 - bytes[203];
    record.current.armor_class = 60 - bytes[204];
    record.current.rear_armor_class = 60 - bytes[205];
    record.current.attacks_raw = {bytes[206], bytes[207]};
    for (std::size_t at = 0; at < record.current.damage.size(); ++at)
    {
        record.current.damage[at] = damage(bytes, 208, at);
    }
    record.current.hit_points_raw = bytes[214];
    record.current.movement = bytes[215];
    return record;
}

std::optional<SpellBlock> decode_spells(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() != 220)
    {
        return std::nullopt;
    }
    SpellBlock spells;
    std::copy_n(bytes.begin(), spells.memorized.size(), spells.memorized.begin());
    std::copy_n(bytes.begin() + 74, spells.known.size(), spells.known.begin());
    for (std::size_t row = 0; row < spells.slots.size(); ++row)
    {
        std::copy_n(bytes.begin() + 188 + 8 * row, 8, spells.slots[row].begin());
    }
    return spells;
}

bool attach_spells(por::CharacterRecord &record, std::span<const std::uint8_t> bytes)
{
    const auto spells = decode_spells(bytes);
    if (record.game != GoldBoxGame::death_knights_of_krynn || !spells)
    {
        return false;
    }
    record.raw_spell_block.assign(bytes.begin(), bytes.end());
    record.memorized_spells.assign(spells->memorized.begin(), spells->memorized.end());
    record.spell_book.assign(spells->known.begin(), spells->known.end());
    std::copy_n(spells->slots[0].begin(), 5, record.spell_slots[0].begin());
    std::copy_n(spells->slots[1].begin(), 5, record.spell_slots[1].begin());
    std::copy_n(spells->slots[2].begin(), 5, record.deity_spell_slots.begin());
    std::copy_n(spells->slots[3].begin(), 5, record.spell_slots[2].begin());
    return true;
}

std::optional<std::vector<por::ItemRecord>> decode_item_list(std::span<const std::uint8_t> bytes)
{
    if (bytes.empty() || bytes.size() % 17 != 0 || bytes.size() / 17 > 127)
    {
        return std::nullopt;
    }
    std::vector<std::uint8_t> items(bytes.size() / 17 * 63);
    for (std::size_t index = 0; index < bytes.size() / 17; ++index)
    {
        std::copy_n(bytes.begin() + 17 * index, 17, items.begin() + 63 * index + 46);
    }
    return por::decode_items(items);
}

std::optional<std::vector<std::vector<std::uint8_t>>>
decode_stores(std::span<const std::uint8_t> program)
{
    static constexpr std::array<std::uint8_t, 16> shape{1,  16, 23, 29, 31, 34, 37, 40,
                                                        15, 7,  6,  2,  3,  3,  3,  30};
    constexpr std::size_t stock_count = 69;
    const auto at = std::search(program.begin(), program.end(), shape.begin(), shape.end());
    if (at == program.end() ||
        static_cast<std::size_t>(program.end() - at) < shape.size() + stock_count)
    {
        return std::nullopt;
    }
    const auto stock = std::span{at + shape.size(), stock_count};
    if (std::ranges::any_of(stock,
                            [](auto id)
                            {
                                return id == 0 || id > 96;
                            }))
    {
        return std::nullopt;
    }
    std::vector<std::vector<std::uint8_t>> result;
    for (std::size_t shop = 0; shop < 8; ++shop)
    {
        const auto first = shape[shop] - 1;
        result.emplace_back(stock.begin() + first, stock.begin() + first + shape[8 + shop]);
    }
    return result;
}
} // namespace opengold::death_knights
