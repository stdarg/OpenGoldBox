#include "opengold/buck_rogers_records.h"
#include "little_endian.h"

#include <algorithm>
#include <stdexcept>

namespace opengold::buck_rogers
{
namespace
{

using format::read_u16;
using format::read_u32;
using Bytes = std::vector<std::uint8_t>;

int signed_byte(std::uint8_t value)
{
    return value < 128 ? value : static_cast<int>(value) - 256;
}

int signed_word(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    const auto value = read_u16(bytes, offset);
    return value < 0x8000 ? value : static_cast<int>(value) - 0x10000;
}

std::uint8_t byte_of(int value)
{
    return static_cast<std::uint8_t>(value & 0xFF);
}

void write_u16(Bytes &bytes, std::size_t offset, unsigned value)
{
    bytes[offset] = byte_of(static_cast<int>(value & 0xFF));
    bytes[offset + 1] = byte_of(static_cast<int>((value >> 8) & 0xFF));
}

void write_u32(Bytes &bytes, std::size_t offset, std::uint32_t value)
{
    write_u16(bytes, offset, value & 0xFFFF);
    write_u16(bytes, offset + 2, value >> 16);
}

template <std::size_t N>
std::array<std::uint8_t, N> read_bytes(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    std::array<std::uint8_t, N> values{};
    std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset), N, values.begin());
    return values;
}

template <std::size_t N>
void write_bytes(Bytes &bytes, std::size_t offset, const std::array<std::uint8_t, N> &values)
{
    std::copy(values.begin(), values.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset));
}

// A Pascal String[capacity]: its length byte, then the characters.
std::optional<std::string> pascal_text(std::span<const std::uint8_t> bytes, std::size_t offset,
                                       std::size_t capacity)
{
    const auto length = bytes[offset];
    if (length > capacity)
    {
        return std::nullopt;
    }
    const auto text = bytes.subspan(offset + 1, length);
    if (!std::all_of(text.begin(), text.end(),
                     [](std::uint8_t c)
                     {
                         return c >= 32 && c < 127;
                     }))
    {
        return std::nullopt;
    }
    return std::string(text.begin(), text.end());
}

// The bytes a template holds, at the start of a 16-byte template or 46 bytes
// into a 62-byte item (types.pas TItemTemplate); nullopt for a type past the
// 129 rows of item types.
std::optional<Item> read_template(std::span<const std::uint8_t> b)
{
    constexpr std::uint8_t item_type_rows = 129;
    if (b[0] >= item_type_rows && b[0] != Item::no_type)
    {
        return std::nullopt;
    }
    Item item;
    item.type = b[0];
    item.name_parts = {b[1], b[2], b[3]};
    item.plus = signed_byte(b[4]);
    item.save_bonus = signed_byte(b[5]);
    item.flags = b[6];
    item.unidentified = b[7];
    item.weight = read_u16(b, 8);
    item.quantity = b[10];
    item.value = read_u16(b, 11);
    item.special = {b[13], b[14], b[15]};
    return item;
}

constexpr std::size_t template_size = 16;
// types.pas: TCareer runs to CareerScout (6), TRace to RaceLowlander (8),
// TStatus to StUsingJetpack (14).
constexpr std::uint8_t last_career = 6, last_race = 8, last_status = 14;

// Where TCharacter's fields sit (types.pas).
namespace field
{
constexpr std::size_t abilities = 16, current_abilities = 23, saves = 30, gender = 38, race = 39,
                      career = 40, level = 41, credits = 43, experience = 47, base_combat = 51,
                      icon_dimensions = 52, base_movement = 54, base_armor_class = 55, age = 56,
                      unarmed_attacks = 59, attacks = 60, unarmed_dice_count = 61,
                      unarmed_dice_sides = 63, unarmed_damage_bonus = 65, hit_point_maximum = 69,
                      armor_flags = 72, companion = 76, skills = 77, experience_award = 178,
                      icon = 184, order_number = 185, specializations = 186, icon_sequence = 196,
                      status = 212, enabled = 213, side = 214, quickfight = 215, attack_bonus = 216,
                      armor_class = 217, hit_points = 227, current_movement = 229, npc = 231,
                      encumbrance = 232, pilot = 234;
} // namespace field

} // namespace

std::optional<Creature> decode_creature(std::span<const std::uint8_t> b)
{
    // Countdown's creature entries carry four bytes past the record, which the
    // game never reads (saveload.pas LoadCreatureData moves SizeOf(TCharacter)).
    if (b.size() != Creature::size && b.size() != Creature::size + 4)
    {
        return std::nullopt;
    }
    auto name = pascal_text(b, 0, Creature::name_capacity);
    if (!name || b[field::gender] > 1 || b[field::race] > last_race ||
        b[field::career] > last_career || b[field::status] > last_status)
    {
        return std::nullopt;
    }
    Creature c;
    c.name = std::move(*name);
    c.abilities = read_bytes<7>(b, field::abilities);
    c.current_abilities = read_bytes<7>(b, field::current_abilities);
    c.saves = read_bytes<8>(b, field::saves);
    c.gender = b[field::gender];
    c.race = b[field::race];
    c.career = b[field::career];
    c.level = signed_byte(b[field::level]);
    c.credits = read_u32(b, field::credits);
    c.experience = read_u32(b, field::experience);
    c.base_combat = b[field::base_combat];
    c.icon_dimensions = b[field::icon_dimensions];
    c.base_movement = b[field::base_movement];
    c.base_armor_class = signed_byte(b[field::base_armor_class]);
    c.age = b[field::age];
    c.unarmed_attacks_doubled = b[field::unarmed_attacks];
    c.attacks_doubled = b[field::attacks];
    c.unarmed_dice_count = read_bytes<2>(b, field::unarmed_dice_count);
    c.unarmed_dice_sides = read_bytes<2>(b, field::unarmed_dice_sides);
    c.unarmed_damage_bonus = {signed_byte(b[field::unarmed_damage_bonus]),
                              signed_byte(b[field::unarmed_damage_bonus + 1])};
    c.hit_point_maximum = read_u16(b, field::hit_point_maximum);
    c.armor_flags = b[field::armor_flags];
    c.companion = b[field::companion];
    c.skills = read_bytes<84>(b, field::skills);
    c.experience_award = read_u16(b, field::experience_award);
    c.icon = b[field::icon];
    c.order_number = b[field::order_number];
    c.specializations = read_bytes<10>(b, field::specializations);
    c.icon_sequence = read_bytes<16>(b, field::icon_sequence);
    c.status = b[field::status];
    c.enabled = b[field::enabled];
    c.side = signed_byte(b[field::side]);
    c.quickfight = b[field::quickfight];
    c.attack_bonus = b[field::attack_bonus];
    c.armor_class = read_bytes<2>(b, field::armor_class);
    c.hit_points = read_u16(b, field::hit_points);
    c.current_movement = b[field::current_movement];
    c.npc = b[field::npc];
    c.encumbrance = read_u16(b, field::encumbrance);
    c.pilot = b[field::pilot];
    c.raw.assign(b.begin(), b.begin() + Creature::size);
    return c;
}

std::vector<std::uint8_t> encode_creature(const Creature &c)
{
    if (c.name.size() > Creature::name_capacity)
    {
        throw std::invalid_argument("A Buck Rogers name holds at most 15 letters");
    }
    Bytes b = c.raw.size() == Creature::size ? c.raw : Bytes(Creature::size);
    b[0] = static_cast<std::uint8_t>(c.name.size());
    std::copy(c.name.begin(), c.name.end(), b.begin() + 1);
    write_bytes(b, field::abilities, c.abilities);
    write_bytes(b, field::current_abilities, c.current_abilities);
    write_bytes(b, field::saves, c.saves);
    b[field::gender] = c.gender;
    b[field::race] = c.race;
    b[field::career] = c.career;
    b[field::level] = byte_of(c.level);
    write_u32(b, field::credits, c.credits);
    write_u32(b, field::experience, c.experience);
    b[field::base_combat] = c.base_combat;
    b[field::icon_dimensions] = c.icon_dimensions;
    b[field::base_movement] = c.base_movement;
    b[field::base_armor_class] = byte_of(c.base_armor_class);
    b[field::age] = c.age;
    b[field::unarmed_attacks] = c.unarmed_attacks_doubled;
    b[field::attacks] = c.attacks_doubled;
    write_bytes(b, field::unarmed_dice_count, c.unarmed_dice_count);
    write_bytes(b, field::unarmed_dice_sides, c.unarmed_dice_sides);
    b[field::unarmed_damage_bonus] = byte_of(c.unarmed_damage_bonus[0]);
    b[field::unarmed_damage_bonus + 1] = byte_of(c.unarmed_damage_bonus[1]);
    write_u16(b, field::hit_point_maximum, c.hit_point_maximum);
    b[field::armor_flags] = c.armor_flags;
    b[field::companion] = c.companion;
    write_bytes(b, field::skills, c.skills);
    write_u16(b, field::experience_award, c.experience_award);
    b[field::icon] = c.icon;
    b[field::order_number] = c.order_number;
    write_bytes(b, field::specializations, c.specializations);
    write_bytes(b, field::icon_sequence, c.icon_sequence);
    b[field::status] = c.status;
    b[field::enabled] = c.enabled;
    b[field::side] = byte_of(c.side);
    b[field::quickfight] = c.quickfight;
    b[field::attack_bonus] = c.attack_bonus;
    write_bytes(b, field::armor_class, c.armor_class);
    write_u16(b, field::hit_points, c.hit_points);
    b[field::current_movement] = c.current_movement;
    b[field::npc] = c.npc;
    write_u16(b, field::encumbrance, c.encumbrance);
    b[field::pilot] = c.pilot;
    return b;
}

std::optional<std::vector<Item>> decode_items(std::span<const std::uint8_t> bytes)
{
    if (bytes.empty() || bytes.size() % Item::size != 0)
    {
        return std::nullopt;
    }
    std::vector<Item> items;
    for (std::size_t start = 0; start < bytes.size(); start += Item::size)
    {
        auto item = read_template(bytes.subspan(start + Item::list_size, template_size));
        if (!item)
        {
            return std::nullopt;
        }
        item->list_bytes = read_bytes<Item::list_size>(bytes, start);
        items.push_back(*item);
    }
    return items;
}

std::vector<std::uint8_t> encode_items(std::span<const Item> items)
{
    Bytes bytes;
    bytes.reserve(items.size() * Item::size);
    for (const auto &item : items)
    {
        bytes.insert(bytes.end(), item.list_bytes.begin(), item.list_bytes.end());
        const std::array<std::uint8_t, template_size> fields{item.type,
                                                             item.name_parts[0],
                                                             item.name_parts[1],
                                                             item.name_parts[2],
                                                             byte_of(item.plus),
                                                             byte_of(item.save_bonus),
                                                             item.flags,
                                                             item.unidentified,
                                                             byte_of(item.weight & 0xFF),
                                                             byte_of(item.weight >> 8),
                                                             item.quantity,
                                                             byte_of(item.value & 0xFF),
                                                             byte_of(item.value >> 8),
                                                             item.special[0],
                                                             item.special[1],
                                                             item.special[2]};
        bytes.insert(bytes.end(), fields.begin(), fields.end());
    }
    return bytes;
}

std::optional<std::vector<Item>> decode_item_templates(std::span<const std::uint8_t> bytes)
{
    // The game keeps at most 127 (eclmenu.pas LoadItemTemplates).
    if (bytes.empty() || bytes.size() % template_size != 0 || bytes.size() / template_size > 127)
    {
        return std::nullopt;
    }
    std::vector<Item> templates;
    for (std::size_t start = 0; start < bytes.size(); start += template_size)
    {
        const auto item = read_template(bytes.subspan(start, template_size));
        if (!item)
        {
            return std::nullopt;
        }
        templates.push_back(*item);
    }
    return templates;
}

std::optional<std::vector<ItemType>> decode_item_types(std::span<const std::uint8_t> bytes)
{
    // The game asks for 129 rows (gameinit.pas); the file may hold fewer.
    if (bytes.empty() || bytes.size() % ItemType::size != 0 || bytes.size() / ItemType::size > 129)
    {
        return std::nullopt;
    }
    std::vector<ItemType> types;
    for (std::size_t start = 0; start < bytes.size(); start += ItemType::size)
    {
        const auto b = bytes.subspan(start, ItemType::size);
        types.push_back({b[0], b[1], b[2], b[3], b[4], b[5], signed_byte(b[6]), b[7], b[8]});
    }
    return types;
}

std::optional<Ship> decode_ship(std::span<const std::uint8_t> b)
{
    if (b.size() != Ship::size)
    {
        return std::nullopt;
    }
    auto name = pascal_text(b, 0, 15);
    if (!name)
    {
        return std::nullopt;
    }
    Ship ship;
    ship.name = std::move(*name);
    ship.tonnage = signed_word(b, 16);
    ship.length = signed_word(b, 18);
    ship.width = signed_word(b, 20);
    for (std::size_t system = 0; system < Ship::systems; ++system)
    {
        ship.system_status[system] = signed_word(b, 22 + 2 * system);
        ship.system_maximum[system] = signed_word(b, 34 + 2 * system);
    }
    ship.cargo = signed_word(b, 46);
    ship.crew = signed_word(b, 48);
    ship.armor_class = signed_byte(b[50]);
    ship.pilot_skill = b[51];
    ship.system_skill = b[52];
    ship.gunnery = b[53];
    ship.speed = signed_byte(b[54]);
    ship.reaction = signed_byte(b[55]);
    ship.maneuver = signed_byte(b[56]);
    ship.type = b[57];
    ship.race = b[58];
    for (std::size_t mount = 0; mount < ship.weapons.size(); ++mount)
    {
        const auto w = b.subspan(60 + 6 * mount, 6);
        ship.weapons[mount] = {w[0], w[1], w[2], w[4], w[5]};
    }
    ship.raw.assign(b.begin(), b.end());
    return ship;
}

std::optional<std::vector<std::uint8_t>> encode_ship(const Ship &ship)
{
    const auto fits_signed_byte = [](int value)
    {
        return value >= -128 && value <= 127;
    };
    const auto fits_signed_word = [](int value)
    {
        return value >= -0x8000 && value <= 0x7FFF;
    };
    std::vector<int> words{ship.tonnage, ship.length, ship.width, ship.cargo, ship.crew};
    words.insert(words.end(), ship.system_status.begin(), ship.system_status.end());
    words.insert(words.end(), ship.system_maximum.begin(), ship.system_maximum.end());
    if (ship.name.size() > 15 || !std::all_of(words.begin(), words.end(), fits_signed_word) ||
        !fits_signed_byte(ship.armor_class) || !fits_signed_byte(ship.speed) ||
        !fits_signed_byte(ship.reaction) || !fits_signed_byte(ship.maneuver))
    {
        return std::nullopt;
    }

    Bytes b = ship.raw;
    b.resize(Ship::size);
    const auto put_word = [&b](std::size_t offset, int value)
    {
        write_u16(b, offset, static_cast<std::uint16_t>(value));
    };
    b[0] = static_cast<std::uint8_t>(ship.name.size());
    std::copy(ship.name.begin(), ship.name.end(), b.begin() + 1);
    put_word(16, ship.tonnage);
    put_word(18, ship.length);
    put_word(20, ship.width);
    for (std::size_t system = 0; system < Ship::systems; ++system)
    {
        put_word(22 + 2 * system, ship.system_status[system]);
        put_word(34 + 2 * system, ship.system_maximum[system]);
    }
    put_word(46, ship.cargo);
    put_word(48, ship.crew);
    b[50] = byte_of(ship.armor_class);
    b[51] = ship.pilot_skill;
    b[52] = ship.system_skill;
    b[53] = ship.gunnery;
    b[54] = byte_of(ship.speed);
    b[55] = byte_of(ship.reaction);
    b[56] = byte_of(ship.maneuver);
    b[57] = ship.type;
    b[58] = ship.race;
    for (std::size_t mount = 0; mount < ship.weapons.size(); ++mount)
    {
        const auto &w = ship.weapons[mount];
        const auto at = 60 + 6 * mount;
        b[at] = w.kind;
        b[at + 1] = w.maximum;
        b[at + 2] = w.status;
        b[at + 4] = w.ammunition;
        b[at + 5] = w.reloads;
    }
    return b;
}

} // namespace opengold::buck_rogers
