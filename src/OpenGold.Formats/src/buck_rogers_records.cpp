#include "opengold/buck_rogers_records.h"
#include "little_endian.h"

#include <algorithm>

namespace opengold::buck_rogers
{
namespace
{

using format::read_u16;
using format::read_u32;

int signed_byte(std::uint8_t value)
{
    return value < 128 ? value : static_cast<int>(value) - 256;
}

int signed_word(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    const auto value = read_u16(bytes, offset);
    return value < 0x8000 ? value : static_cast<int>(value) - 0x10000;
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

} // namespace

std::optional<Creature> decode_creature(std::span<const std::uint8_t> b)
{
    // Countdown's creature entries carry four bytes past the record, which the
    // game never reads (saveload.pas LoadCreatureData moves SizeOf(TCharacter)).
    if (b.size() != Creature::size && b.size() != Creature::size + 4)
    {
        return std::nullopt;
    }
    auto name = pascal_text(b, 0, 15);
    if (!name || b[38] > 1 || b[39] > last_race || b[40] > last_career || b[212] > last_status)
    {
        return std::nullopt;
    }
    Creature c;
    c.name = std::move(*name);
    std::copy_n(b.begin() + 16, 7, c.abilities.begin());
    std::copy_n(b.begin() + 23, 7, c.current_abilities.begin());
    std::copy_n(b.begin() + 30, 8, c.saves.begin());
    c.gender = b[38];
    c.race = b[39];
    c.career = b[40];
    c.level = signed_byte(b[41]);
    c.credits = read_u32(b, 43);
    c.experience = read_u32(b, 47);
    c.base_combat = b[51];
    c.base_movement = b[54];
    c.base_armor_class = signed_byte(b[55]);
    c.hit_point_maximum = read_u16(b, 69);
    std::copy_n(b.begin() + 77, c.skills.size(), c.skills.begin());
    c.experience_award = read_u16(b, 178);
    c.status = b[212];
    c.hit_points = read_u16(b, 227);
    c.npc = b[231];
    c.raw.assign(b.begin(), b.begin() + Creature::size);
    return c;
}

std::optional<std::vector<Item>> decode_items(std::span<const std::uint8_t> bytes)
{
    if (bytes.empty() || bytes.size() % Item::size != 0)
    {
        return std::nullopt;
    }
    std::vector<Item> items;
    constexpr std::size_t template_offset = Item::size - template_size;
    for (std::size_t at = 0; at < bytes.size(); at += Item::size)
    {
        const auto item = read_template(bytes.subspan(at + template_offset, template_size));
        if (!item)
        {
            return std::nullopt;
        }
        items.push_back(*item);
    }
    return items;
}

std::optional<std::vector<Item>> decode_item_templates(std::span<const std::uint8_t> bytes)
{
    // The game keeps at most 127 (eclmenu.pas LoadItemTemplates).
    if (bytes.empty() || bytes.size() % template_size != 0 || bytes.size() / template_size > 127)
    {
        return std::nullopt;
    }
    std::vector<Item> templates;
    for (std::size_t at = 0; at < bytes.size(); at += template_size)
    {
        const auto item = read_template(bytes.subspan(at, template_size));
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
    for (std::size_t at = 0; at < bytes.size(); at += ItemType::size)
    {
        const auto b = bytes.subspan(at, ItemType::size);
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
    ship.maneuver = b[56];
    ship.type = b[57];
    ship.race = b[58];
    for (std::size_t mount = 0; mount < ship.weapons.size(); ++mount)
    {
        const auto w = b.subspan(60 + 6 * mount, 6);
        ship.weapons[mount] = {w[0], w[1], w[2], w[4], w[5]};
    }
    return ship;
}

} // namespace opengold::buck_rogers
