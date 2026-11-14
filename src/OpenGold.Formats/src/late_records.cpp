#include "opengold/late_records.h"
#include "little_endian.h"

#include <algorithm>

namespace opengold::late
{
namespace
{

// Pools of Darkness numbers its races elf 0, half-elf 1, dwarf 2, gnome 3,
// halfling 4, human 5 and monster 6 (Secret's less 1); the shared fields keep
// the earlier games' numbers, monster 0 and dwarf, elf, gnome, half-elf,
// halfling, half-orc and human 1 to 7.
std::uint8_t shared_race(std::uint8_t race)
{
    constexpr std::array<std::uint8_t, 7> shared{2, 4, 1, 3, 5, 7, 0};
    return race < shared.size() ? shared[race] : 0;
}

constexpr std::size_t creature_size = 510;
constexpr std::size_t item_size = 17;
// An earlier game's item: a 46-byte name area, then these 17 bytes.
constexpr std::size_t named_item_size = 63;

// The spell slot lists (cleric, druid, magic-user), nine spell levels each.
constexpr std::size_t spell_slots_offset = 381, levels_a_list = 9;

constexpr int signed_byte(std::uint8_t value)
{
    return value < 128 ? value : static_cast<int>(value) - 256;
}

// Seven classes (cleric, druid, fighter, paladin, ranger, magic-user, thief).
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

std::optional<por::CharacterRecord> decode_creature(std::span<const std::uint8_t> b,
                                                    GoldBoxGame game)
{
    const bool darkness = game == GoldBoxGame::pools_of_darkness;
    if ((!darkness && game != GoldBoxGame::treasures_of_the_savage_frontier) ||
        b.size() != creature_size || b[0] > 15)
    {
        return std::nullopt;
    }
    por::CharacterRecord c;
    c.game = game;
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
    c.memorized_spells.assign(b.begin() + 30, b.begin() + 30 + 141);
    c.spell_byte_44_raw = b[171];
    c.base_thac0 = 60 - b[172];
    c.race = shared_race(b[173]);
    c.character_class = b[174];
    c.age = format::read_u16(b, 176);
    c.max_hit_points = b[178];
    // Treasures knows one spell more, in the byte Pools of Darkness gives its attack level.
    const std::size_t known_spells = darkness ? 125 : 126;
    c.spell_book.assign(b.begin() + 179,
                        b.begin() + 179 + static_cast<std::ptrdiff_t>(known_spells));
    if (darkness)
    {
        c.attack_level = b[304];
    }
    c.icon_size_raw = b[305];
    c.saves = {b[306], b[307], b[308], b[309], b[310]};
    c.base_movement = b[311];
    c.highest_level_raw = b[312];
    c.undead_level = b[314];
    std::copy_n(b.begin() + 315, c.thief_skills.size(), c.thief_skills.begin());
    c.npc_flag = b[327];
    // Only platinum, gems and jewelry are kept.
    for (std::size_t i = 0; i < 3; ++i)
    {
        c.wealth[4 + i] = format::read_u16(b, 331 + 2 * i);
    }
    c.levels = class_levels(b, 337);
    c.levels_before_class_change = class_levels(b, 344);
    c.gender = b[358];
    c.alignment = b[359];
    for (std::size_t i = 0; i < c.base_attacks.size(); ++i)
    {
        c.base_attacks[i] = {b[360 + i], attack_damage(b, 362, i)};
    }
    c.base_armor_class = 60 - b[368];
    c.experience = format::read_u32(b, 370);
    c.rolled_hit_points = b[380];
    for (std::size_t list = 0; list < c.spell_slots.size(); ++list)
    {
        const auto first = spell_slots_offset + levels_a_list * list;
        std::copy_n(b.begin() + static_cast<std::ptrdiff_t>(first), c.spell_slots[list].size(),
                    c.spell_slots[list].begin());
    }
    c.base_xp_award = format::read_u16(b, 408);
    c.item_count = b[422];
    c.hands_equipped = b[479];
    c.save_bonus = signed_byte(b[480]);
    c.encumbrance = format::read_u16(b, 481);
    c.status = b[493];
    c.enabled = b[494];
    c.hostile = b[495];
    c.quick_fight = b[496];
    c.current.thac0 = 60 - b[497];
    c.current.armor_class = 60 - b[498];
    c.current.rear_armor_class = 60 - b[499];
    c.current.attacks_raw = {b[500], b[501]};
    for (std::size_t i = 0; i < c.current.damage.size(); ++i)
    {
        c.current.damage[i] = attack_damage(b, 502, i);
    }
    c.current.hit_points_raw = b[508];
    c.current.movement = b[509];
    return c;
}

std::optional<std::vector<por::ItemRecord>> decode_items(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() % item_size != 0)
    {
        return std::nullopt;
    }
    constexpr auto name_area = named_item_size - item_size;
    std::vector<std::uint8_t> named(bytes.size() / item_size * named_item_size);
    for (std::size_t item = 0; item < bytes.size() / item_size; ++item)
    {
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(item * item_size), item_size,
                    named.begin() +
                        static_cast<std::ptrdiff_t>(item * named_item_size + name_area));
    }
    return por::decode_items(named);
}

} // namespace opengold::late
