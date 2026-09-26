#include "opengold/pool_radiance.h"

#include <algorithm>
#include <utility>

namespace opengold::por
{
namespace
{
int signed_byte(std::uint8_t value)
{
    return value < 128 ? value : static_cast<int>(value) - 256;
}
std::uint16_t u16(std::span<const std::uint8_t> b, std::size_t p)
{
    return static_cast<std::uint16_t>(b[p] | (b[p + 1] << 8));
}
std::uint32_t u32(std::span<const std::uint8_t> b, std::size_t p)
{
    return u16(b, p) | (static_cast<std::uint32_t>(u16(b, p + 2)) << 16);
}
template <std::size_t N>
void copy(std::span<const std::uint8_t> b, std::size_t p, std::array<std::uint8_t, N> &out)
{
    std::copy_n(b.begin() + p, N, out.begin());
}
DamageDice damage(std::span<const std::uint8_t> b, std::size_t p, std::size_t stride = 1)
{
    return {b[p], b[p + stride], signed_byte(b[p + stride * 2])};
}
} // namespace

std::optional<CharacterRecord> decode_character(std::span<const std::uint8_t> b)
{
    if (b.size() != 285 || b[0] > 15)
        return std::nullopt;
    CharacterRecord c;
    copy(b, 0, c.raw);
    c.name.assign(b.begin() + 1, b.begin() + 1 + b[0]);
    c.abilities = {b[16], b[17], b[18], b[19], b[20], b[21], b[22]};
    c.base_thac0 = 60 - b[45];
    c.base_armor_class = 60 - b[169];
    c.max_hit_points = b[50];
    c.rolled_hit_points = b[177];
    c.base_movement = b[114];
    for (std::size_t i = 0; i < 2; ++i)
        c.base_attacks[i] = {b[161 + i], damage(b, 163 + i, 2)};
    c.saves = {b[109], b[110], b[111], b[112], b[113]};
    c.levels = {b[150], b[151], b[152], b[153], b[154], b[155], b[156], b[157]};
    c.race = b[46];
    c.character_class = b[47];
    c.age = u16(b, 48);
    c.gender = b[158];
    c.alignment = b[160];
    c.type = b[159];
    c.npc_flag = b[132];
    c.attack_level = b[107];
    c.highest_level_raw = b[115];
    c.drained_levels = b[116];
    c.drained_hit_points = b[117];
    c.undead_level = b[118];
    copy(b, 119, c.thief_skills);
    copy(b, 51, c.spell_book);
    copy(b, 23, c.memorized_spells);
    copy(b, 178, c.cleric_spell_slots);
    copy(b, 181, c.mage_spell_slots);
    c.spell_byte_44_raw = b[44];
    c.experience = u32(b, 172);
    c.base_xp_award = u16(b, 184);
    c.xp_per_hit_point = b[186];
    for (std::size_t i = 0; i < c.wealth.size(); ++i)
        c.wealth[i] = u16(b, 136 + 2 * i);
    c.strength_bonus_flag_raw = b[170];
    c.item_count = b[199];
    c.item_limit = b[176];
    c.hands_equipped = b[256];
    c.save_bonus = signed_byte(b[257]);
    c.encumbrance = u16(b, 258);
    c.status = b[268];
    c.enabled = b[269];
    c.hostile = b[270];
    c.quick_fight = b[271];
    c.current.thac0 = 60 - b[272];
    c.current.armor_class = 60 - b[273];
    c.current.rear_armor_class = 60 - b[274];
    c.current.attacks_raw = {b[275], b[276]};
    for (std::size_t i = 0; i < 2; ++i)
        c.current.damage[i] = damage(b, 277 + i, 2);
    c.current.hit_points_raw = b[283];
    c.current.movement = b[284];
    return c;
}

std::optional<std::vector<ItemRecord>> decode_items(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() % 63 != 0)
        return std::nullopt;
    std::vector<ItemRecord> result;
    for (std::size_t p = 0; p < bytes.size(); p += 63)
    {
        const auto b = bytes.subspan(p, 63);
        if (b[0] > 45)
            return std::nullopt;
        ItemRecord item;
        copy(b, 0, item.raw);
        item.stored_name.assign(b.begin() + 1, b.begin() + 1 + b[0]);
        item.type = b[46];
        item.name_components = {b[47], b[48], b[49]};
        item.magic_bonus = signed_byte(b[50]);
        item.save_bonus = signed_byte(b[51]);
        item.readied_raw = b[52];
        item.revealed_components = b[53];
        item.cursed_raw = b[54];
        item.weight = u16(b, 55);
        item.stack_size = b[57];
        item.value = u16(b, 58);
        item.effect_codes = {b[60], b[61], b[62]};
        result.push_back(std::move(item));
    }
    return result;
}

std::optional<std::vector<EffectRecord>> decode_effects(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() % 9 != 0)
        return std::nullopt;
    std::vector<EffectRecord> result;
    for (std::size_t p = 0; p < bytes.size(); p += 9)
    {
        const auto b = bytes.subspan(p, 9);
        EffectRecord effect;
        copy(b, 0, effect.raw);
        effect.code = b[0];
        effect.duration_raw = u16(b, 1);
        effect.data = b[3];
        effect.table_flag = b[4];
        effect.next_pointer_raw = u32(b, 5);
        result.push_back(effect);
    }
    return result;
}

std::optional<std::vector<ItemTemplate>> decode_item_templates(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < 18 || (bytes.size() - 2) % 16 != 0 || (bytes.size() - 2) / 16 > 256)
        return std::nullopt;
    std::vector<ItemTemplate> result;
    for (std::size_t p = 2; p < bytes.size(); p += 16)
    {
        const auto b = bytes.subspan(p, 16);
        ItemTemplate item;
        copy(b, 0, item.raw);
        item.worn_location = b[0];
        item.hands = b[1];
        item.large_damage = damage(b, 2);
        item.rate_of_fire = b[5];
        item.protection_raw = b[6];
        item.damage_type = b[7];
        item.melee_flag = b[8];
        item.small_medium_damage = damage(b, 9);
        item.range = b[12];
        item.class_restrictions = b[13];
        item.ammunition_type = b[14];
        result.push_back(item);
    }
    return result;
}
} // namespace opengold::por
