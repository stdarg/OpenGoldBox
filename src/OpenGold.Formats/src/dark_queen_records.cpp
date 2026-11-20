#include "opengold/dark_queen_records.h"

#include "little_endian.h"
#include "ua_detail.h"

#include <algorithm>
#include <utility>

namespace opengold::dark_queen
{
namespace
{

int signed_byte(std::uint8_t value)
{
    return value < 128 ? value : static_cast<int>(value) - 256;
}

por::ClassLevels levels(std::span<const std::uint8_t> bytes, std::size_t at)
{
    // Dark Queen replaces the unused druid level with the knight's; the
    // common profile keeps knights in its eighth class slot.
    return {
        bytes[at],    0, bytes[at + 2], bytes[at + 3], bytes[at + 4], bytes[at + 5], bytes[at + 6],
        bytes[at + 1]};
}

por::DamageDice damage(std::span<const std::uint8_t> bytes, std::size_t at, std::size_t attack)
{
    return {bytes[at + attack], bytes[at + 2 + attack], signed_byte(bytes[at + 4 + attack])};
}

} // namespace

int import_spell_number(int number) noexcept
{
    constexpr std::pair<int, int> moved[]{
        {2, 90},    {4, 96},   {7, 105},   {17, 102},  {40, 56},   {53, 36},   {56, 110},
        {57, 127},  {59, 53},  {60, 111},  {61, 130},  {62, 131},  {63, 112},  {64, 133},
        {65, 134},  {66, 113}, {72, 114},  {90, 119},  {95, 135},  {96, 132},  {97, 136},
        {98, 118},  {99, 137}, {101, 57},  {102, 59},  {103, 60},  {104, 61},  {105, 62},
        {106, 63},  {107, 64}, {108, 115}, {109, 116}, {110, 117}, {111, 120}, {112, 122},
        {113, 121}, {114, 123}};
    const auto found = std::ranges::find(moved, number, &std::pair<int, int>::first);
    if (found != std::end(moved))
    {
        number = found->second;
    }
    return number;
}

int import_effect_number(int number) noexcept
{
    constexpr std::pair<int, int> moved[]{
        {2, 129},   {6, 130},   {7, 131},   {9, 74},    {13, 71},   {32, 69},  {34, 32},  {48, 13},
        {57, 118},  {58, 123},  {59, 57},   {60, 122},  {61, 68},   {65, 6},   {66, 60},  {67, 101},
        {68, 102},  {69, 132},  {70, 133},  {74, 121},  {75, 134},  {76, 135}, {77, 136}, {79, 137},
        {80, 138},  {81, 124},  {87, 79},   {91, 140},  {93, 66},   {96, 141}, {97, 142}, {99, 143},
        {114, 200}, {115, 145}, {116, 146}, {117, 147}, {118, 148}, {40, 0},   {78, 0},   {85, 0},
        {90, 0},    {100, 0},   {104, 0},   {106, 0},   {108, 0},   {120, 0},  {121, 0}};
    const auto found = std::ranges::find(moved, number, &std::pair<int, int>::first);
    return found == std::end(moved) ? number : found->second;
}

int import_item_word(int number) noexcept
{
    constexpr std::pair<int, int> moved[]{{12, 138},  {38, 139}, {40, 140}, {52, 141},  {59, 142},
                                          {68, 143},  {74, 144}, {75, 145}, {76, 146},  {84, 147},
                                          {85, 148},  {86, 149}, {96, 150}, {119, 151}, {120, 152},
                                          {124, 153}, {67, 126}, {69, 127}, {77, 129},  {105, 135}};
    const auto found = std::ranges::find(moved, number, &std::pair<int, int>::first);
    return found == std::end(moved) ? number : found->second;
}

std::optional<std::vector<por::ItemRecord>> decode_items(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() % 17 != 0)
    {
        return std::nullopt;
    }
    std::vector<std::uint8_t> common(bytes.size() / 17 * 63);
    for (std::size_t n = 0; n < bytes.size() / 17; ++n)
    {
        const auto item = bytes.subspan(n * 17, 17);
        auto out = std::span{common}.subspan(n * 63, 63);
        std::copy_n(item.begin(), 4, out.begin() + 46);
        out[50] = item[8];
        out[51] = item[9];
        out[52] = item[10];
        out[53] = item[11];
        out[54] = item[12];
        std::copy_n(item.begin() + 4, 2, out.begin() + 55);
        out[57] = item[13];
        std::copy_n(item.begin() + 6, 2, out.begin() + 58);
        std::copy_n(item.begin() + 14, 3, out.begin() + 60);
    }
    return por::decode_items(common);
}

std::optional<por::CharacterRecord> decode_character(std::span<const std::uint8_t> raw)
{
    constexpr std::size_t character_size = 414;
    if (raw.size() != character_size)
    {
        return std::nullopt;
    }
    por::CharacterRecord record;
    SpellBlock spells;
    record.game = GoldBoxGame::dark_queen_of_krynn;
    record.raw.assign(raw.begin(), raw.begin() + character_size);
    const auto name = raw.subspan(0x68, 16);
    const auto end = std::ranges::find(name, std::uint8_t{});
    if (end == name.end())
    {
        return std::nullopt;
    }
    record.name.assign(name.begin(), end);
    record.abilities = {raw[0x78], raw[0x7A], raw[0x7C], raw[0x7E],
                        raw[0x80], raw[0x82], raw[0x85]};
    record.current_abilities = {raw[0x79], raw[0x7B], raw[0x7D], raw[0x7F],
                                raw[0x81], raw[0x83], raw[0x84]};
    record.experience = format::read_u32(raw, 0x44);
    record.base_xp_award = format::read_u16(raw, 0x54);
    record.encumbrance = format::read_u16(raw, 0x56);
    record.age = format::read_u16(raw, 0x52);
    record.race = static_cast<std::uint8_t>(format::read_u16(raw, 0x58));
    record.character_class = static_cast<std::uint8_t>(format::read_u16(raw, 0x5A));
    record.undead_level = static_cast<std::uint8_t>(format::read_u16(raw, 0x5C));
    record.gender = static_cast<std::uint8_t>(format::read_u16(raw, 0x60));
    record.alignment = static_cast<std::uint8_t>(format::read_u16(raw, 0x62));
    record.status = static_cast<std::uint8_t>(format::read_u16(raw, 0x64));
    record.hostile = static_cast<std::uint8_t>(format::read_u16(raw, 0x66));
    record.spell_byte_44_raw = raw[0x86];
    record.base_thac0 = 60 - raw[0x87];
    record.max_hit_points = raw[0x89];
    record.icon_size_raw = raw[0x8A];
    record.saves = {raw[0x8B], raw[0x8C], raw[0x8D], raw[0x8E], raw[0x8F]};
    record.base_movement = raw[0x90];
    record.highest_level_raw = raw[0x91];
    std::copy_n(raw.begin() + 0x93, record.thief_skills.size(), record.thief_skills.begin());
    record.npc_flag = raw[0x9B];
    record.levels = levels(raw, 0xA5);
    record.levels_before_class_change = levels(raw, 0xAC);
    for (std::size_t attack = 0; attack < 2; ++attack)
    {
        record.base_attacks[attack] = {raw[0xB3 + attack], damage(raw, 0xB5, attack)};
        record.current.damage[attack] = damage(raw, 0x192, attack);
    }
    record.base_armor_class = 60 - raw[0xBB];
    record.strength_bonus_flag_raw = raw[0xBC];
    record.rolled_hit_points = record.xp_per_hit_point = raw[0xC0];
    record.creature_type_flags = record.type = raw[0xC7];
    record.target_flags = raw[0xC8];
    record.item_count = raw[0xC9];
    record.hands_equipped = raw[0xCA];
    record.save_bonus = signed_byte(raw[0xCB]);
    record.magic_resistance = raw[0xCC];
    record.enabled = raw[0x18B];
    record.quick_fight = raw[0x18C];
    record.current.thac0 = 60 - raw[0x18D];
    record.current.armor_class = 60 - raw[0x18E];
    record.current.rear_armor_class = 60 - raw[0x18F];
    record.current.attacks_raw = {raw[0x190], raw[0x191]};
    record.current.hit_points_raw = raw[0x198];
    record.current.movement = raw[0x199];
    record.knight_order = raw[0x19A];
    record.deity = static_cast<std::uint8_t>(format::read_u16(raw, 0x19B));
    record.robe_order = raw[0x19D];
    for (std::size_t coin = 0; coin < 3; ++coin)
    {
        record.wealth[4 + coin] = format::read_u16(raw, 0x4C + 2 * coin);
    }
    std::copy_n(raw.begin() + 0xCE, spells.memorized.size(), spells.memorized.begin());
    std::copy_n(raw.begin() + 0x157, spells.known_bits.size(), spells.known_bits.begin());
    record.memorized_spells.assign(spells.memorized.begin(), spells.memorized.end());
    for (std::size_t spell = 0; spell < 128; ++spell)
    {
        record.spell_book.push_back((spells.known_bits[spell / 8] >> (spell % 8)) & 1);
    }
    record.raw_spell_block.assign(raw.begin() + 0xCE, raw.begin() + 0x18B);
    for (std::size_t row = 0; row < 4; ++row)
    {
        std::copy_n(raw.begin() + 0x167 + row * 9, 9, spells.slots[row].begin());
        auto &common = row < 3 ? record.spell_slots[row] : record.deity_spell_slots;
        std::copy_n(spells.slots[row].begin(), common.size(), common.begin());
    }
    return record;
}

std::optional<Creature> decode_creature(std::span<const std::uint8_t> bytes)
{
    constexpr std::size_t character_size = 414, buffer_size = 626;
    if (bytes.size() < 2)
    {
        return std::nullopt;
    }
    const auto expanded_size = ua::detail::read_big_u16(bytes, 0);
    if (expanded_size < character_size + 2 || expanded_size > buffer_size)
    {
        return std::nullopt;
    }
    Creature creature;
    std::size_t position = 2;
    if (!ua::detail::unpack_bits(bytes, position, expanded_size, creature.raw))
    {
        return std::nullopt;
    }
    const std::span<const std::uint8_t> raw = creature.raw;
    const auto effect_count = raw[character_size];
    const auto item_count = raw[character_size + 1];
    const auto content_size = character_size + 2 + 5 * effect_count + 17 * item_count;
    // mon_load uses the two counts and ignores the remaining bytes. Most
    // records have at most alignment padding; the shipped Sahuagin Lord
    // also retains eighteen unused bytes. Preserve them without granting items.
    if (content_size > raw.size())
    {
        return std::nullopt;
    }
    auto character = decode_character(raw.first(character_size));
    if (!character)
    {
        return std::nullopt;
    }
    creature.character = std::move(*character);
    std::copy_n(raw.begin() + 0xCE, creature.spells.memorized.size(),
                creature.spells.memorized.begin());
    std::copy_n(raw.begin() + 0x157, creature.spells.known_bits.size(),
                creature.spells.known_bits.begin());
    for (std::size_t row = 0; row < 4; ++row)
    {
        std::copy_n(raw.begin() + 0x167 + row * 9, 9, creature.spells.slots[row].begin());
    }
    for (std::size_t effect = 0; effect < effect_count; ++effect)
    {
        const auto at = character_size + 2 + 5 * effect;
        por::EffectRecord decoded;
        std::copy_n(raw.begin() + at, 5, decoded.raw.begin());
        decoded.code = raw[at];
        decoded.duration_raw = format::read_u16(raw, at + 1);
        decoded.data = raw[at + 3];
        decoded.table_flag = raw[at + 4];
        creature.effects.push_back(decoded);
    }
    auto items = decode_items(raw.subspan(character_size + 2 + 5 * effect_count, 17 * item_count));
    if (!items)
    {
        return std::nullopt;
    }
    creature.items = std::move(*items);
    return creature;
}

std::optional<Area> decode_area(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < 8 || !bytes[0] || !bytes[1])
    {
        return std::nullopt;
    }
    const std::size_t count = std::size_t{bytes[0]} * bytes[1];
    if (count > 784 || bytes.size() != 8 + 4 * count)
    {
        return std::nullopt;
    }
    Area area;
    area.rows = bytes[0];
    area.columns = bytes[1];
    area.wall_sets = {bytes[2], bytes[3], bytes[4]};
    area.backdrops = {bytes[5], bytes[6], bytes[7]};
    area.raw.assign(bytes.begin(), bytes.end());
    area.cells.resize(count);
    for (std::size_t cell = 0; cell < count; ++cell)
    {
        auto &decoded = area.cells[cell];
        decoded.walls = {static_cast<std::uint8_t>(bytes[8 + cell] >> 4),
                         static_cast<std::uint8_t>(bytes[8 + cell] & 15),
                         static_cast<std::uint8_t>(bytes[8 + count + cell] >> 4),
                         static_cast<std::uint8_t>(bytes[8 + count + cell] & 15)};
        decoded.event_raw = bytes[8 + 2 * count + cell];
        for (std::size_t direction = 0; direction < 4; ++direction)
        {
            decoded.doors[direction] = (bytes[8 + 3 * count + cell] >> (2 * direction)) & 1;
        }
    }
    return area;
}

} // namespace opengold::dark_queen
