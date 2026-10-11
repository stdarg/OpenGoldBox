#include "opengold/random_treasure.h"
#include <array>
#include <optional>
#include <stdexcept>

namespace opengold::por
{
namespace
{
// Original item type IDs used by the generator (ITEMS template indices).
enum ItemType : unsigned
{
    javelin = 21,
    quarrel = 28,
    bastard_sword = 34,
    broad_sword = 35,
    long_sword = 36,
    short_sword = 37,
    two_handed_sword = 38,
    heavy_crossbow = 45,
    leather_armor = 50,
    padded_armor = 51,
    studded_leather = 52,
    ring_mail = 53,
    scale_mail = 54,
    chain_mail = 55,
    splint_mail = 56,
    banded_mail = 57,
    plate_mail = 58,
    shield = 59,
    magic_user_scroll = 61,
    cleric_scroll = 62,
    gauntlets = 63,
    type_67 = 67,
    potion = 71,
    arrow = 73,
    bracers = 77,
    wand_a = 78,
    wand_b = 79,
    type_84 = 84,
    type_89 = 89,
    cloak = 92,
    ring_of_protection = 93
};

// The engine's 63-byte item record, before it is decoded.
struct GeneratedItem
{
    unsigned type{}, name1{}, name2{}, name3{};
    int plus{};
    unsigned plus_save{}, hidden_names{6}, weight{}, count{}, value{};
    std::array<unsigned, 3> effects{};
};

// Preset records: name components 1-3, weight, value and three effects.
struct PresetItem
{
    unsigned name1, name2, name3, weight, value;
    std::array<unsigned, 3> effects;
};

constexpr std::array<PresetItem, 7> presets{{
        {0xB9, 0xBB, 0x40, 1, 800, {3, 0x63, 0}},
        {0xEF, 0xA7, 0x40, 1, 1100, {1, 0x3B, 0}},
        {0xB9, 0xA7, 0x40, 1, 400, {1, 3, 0}},
        {0xAD, 0xA7, 0x40, 1, 450, {1, 0x30, 0}},
        {0xCE, 0xA7, 0x45, 1, 11000, {0x1E, 0x0F, 0}},
        {0xE2, 0xA7, 0x64, 10, 15000, {0, 0x26, 0x83}},
        {0x9D, 0xA7, 0x15, 20, 3000, {1, 0x33, 0}}
    }
};

unsigned rolled(const DieRoller &roll, unsigned sides)
{
    const auto value = roll(sides);
    if (value < 1 || value > sides)
        throw std::runtime_error("Treasure die roll out of range");
    return value;
}

unsigned random_item_type(const DieRoller &roll)
{
    const auto kind = rolled(roll, 100);
    if (kind <= 60)
    {
        const auto weapon = rolled(roll, 100);
        if ((weapon >= 1 && weapon <= 47) || (weapon >= 50 && weapon <= 59))
            return weapon == heavy_crossbow ? shield : weapon;
        if (weapon >= 60 && weapon <= 90)
        {
            const auto sword = rolled(roll, 10);
            return sword <= 4 ? long_sword
                   : sword <= 7 ? broad_sword
                   : sword == 8 ? bastard_sword
                   : sword == 9 ? short_sword
                   : two_handed_sword;
        }
        if (weapon >= 91 && weapon <= 94)
            return arrow;
        if (weapon >= 95 && weapon <= 97)
            return ring_of_protection;
        if (weapon >= 98)
            return bracers;
        return shield; // 48 and 49.
    }
    if (kind <= 85)
        return magic_user_scroll;
    if (kind <= 92)
        return cleric_scroll;
    if (kind <= 98)
    {
        const auto charged = rolled(roll, 15);
        return charged <= 9 ? potion : charged == 10 ? type_84 : wand_b;
    }
    return shield;
}

unsigned item_weight(unsigned type, unsigned &count)
{
    switch (type)
    {
    case 1:
    case 13:
    case 14:
    case broad_sword:
        return 75;
    case 2:
    case 20:
    case 29:
    case 31:
    case 32:
    case 33:
    case 39:
    case 42:
    case 44:
    case 46:
    case shield:
        return 50;
    case 3:
    case 24:
    case 40:
        return 125;
    case 4:
    case 15:
    case 23:
    case bastard_sword:
    case 43:
    case heavy_crossbow:
    case padded_armor:
        return 100;
    case 5:
    case 12:
    case 17:
    case 19:
    case leather_armor:
        return 150;
    case 6:
        return 15;
    case 7:
        return 30;
    case 8:
    case bracers:
        return 10;
    case 9:
        count = 5;
        return 25;
    case 10:
    case 26:
    case long_sword:
        return 60;
    case 11:
    case 16:
    case 25:
    case 27:
    case 41:
    case 47:
        return 80;
    case 18:
        return 175;
    case javelin:
        return 20;
    case 22:
    case 30:
        return 40;
    case short_sword:
        return 35;
    case two_handed_sword:
    case ring_mail:
        return 250;
    case studded_leather:
        return 200;
    case scale_mail:
    case splint_mail:
        return 400;
    case chain_mail:
        return 300;
    case banded_mail:
        return 350;
    case plate_mail:
        return 450;
    case ring_of_protection:
        return 1;
    default:
        count = 10;
        return 40;
    }
}

unsigned enchanted_value(unsigned type, int plus)
{
    const auto per_plus = type == shield ? 2500
                          : type == arrow || type == quarrel ? 150
                          : type == ring_mail || type == scale_mail ? 3000
                          : type == chain_mail || type == splint_mail ? 3500
                          : type == banded_mail ? 4000
                          : type == plate_mail ? 5000
                          : type == bracers ? 3000
                          : 2000;
    return static_cast<unsigned>(plus * per_plus);
}

// Weapons, armor, arrows, bracers and protection rings: +1 or +2.
std::optional<unsigned> enchant(GeneratedItem &item, const DieRoller &roll)
{
    std::optional<unsigned> preset;
    item.plus = rolled(roll, 20) <= 14 ? 1 : 2;
    const unsigned plus_name = static_cast<unsigned>(item.plus) + 0xA1;
    if (item.type == javelin)
    {
        if (rolled(roll, 5) == 5)
            preset = 6;
        item.name3 = 0x15;
        item.name2 = plus_name;
    }
    else if (item.type == quarrel)
    {
        item.name3 = 0x1C;
        item.name2 = plus_name;
    }
    else if (item.type >= leather_armor && item.type <= plate_mail)
    {
        item.name3 = item.type;
        item.name2 = item.type == studded_leather ? 0x32 : item.type <= padded_armor ? 0x31 : 0x30;
        item.name1 = plus_name;
        item.hidden_names = 4;
    }
    else if (item.type == arrow)
    {
        item.name3 = 0x3D;
        item.name2 = plus_name;
    }
    else if (item.type == bracers)
    {
        item.name3 = 0x4F;
        item.name2 = 0xA7;
        item.plus = item.plus * 2 + 2;
        item.name1 = item.plus == 4 ? 0xDD : item.plus == 6 ? 0xDE : 0;
    }
    else if (item.type == ring_of_protection)
    {
        item.name3 = 0x42;
        item.name2 = 0xE0;
        item.name1 = plus_name;
    }
    else
    {
        item.name3 = item.type;
        item.name2 = plus_name;
    }
    item.weight = item_weight(item.type, item.count);
    item.value = enchanted_value(item.type, item.plus);
    return preset;
}

void write_scroll(GeneratedItem &item, const DieRoller &roll)
{
    const auto spells = rolled(roll, 3);
    const bool magic_user = item.type == magic_user_scroll;
    item.name3 = magic_user ? 0xD1 : 0xD0;
    item.name2 = spells + 0xD1;
    item.plus = 1;
    item.weight = 25;
    for (unsigned n = 0; n < spells; ++n)
    {
        const auto level = rolled(roll, 5);
        static constexpr std::array<std::array<unsigned, 2>, 5> magic_user_spells{
            {{13, 8}, {7, 28}, {11, 44}, {9, 80}, {4, 90}}};
        static constexpr std::array<std::array<unsigned, 2>, 5> cleric_spells{
            {{8, 0}, {7, 0x15}, {8, 0x24}, {5, 0x41}, {6, 0x46}}};
        const auto &[sides, first] = (magic_user ? magic_user_spells : cleric_spells)[level - 1];
        item.effects[n] = rolled(roll, sides) + first;
        item.value += level * 300;
    }
}

ItemRecord generate_item(unsigned type, const DieRoller &roll)
{
    GeneratedItem item;
    item.type = type;
    std::optional<unsigned> preset;
    if ((type >= 1 && type <= shield) || type == arrow || type == bracers ||
            type == ring_of_protection)
        preset = enchant(item, roll);
    else if (type == magic_user_scroll || type == cleric_scroll)
        write_scroll(item, roll);
    else if (type == gauntlets || type == type_67)
        preset = 5;
    else if (type == wand_a || type == wand_b)
        preset = 4;
    else if (type == type_89 || type == cloak)
        preset = 1;
    else if (type == potion)
        preset = rolled(roll, 8) <= 5 ? 2 : 0;
    if (preset)
    {
        const auto &p = presets[*preset];
        item.name1 = p.name1;
        item.name2 = p.name2;
        item.name3 = p.name3;
        item.plus = 1;
        item.plus_save = 1;
        item.weight = p.weight;
        item.count = 0;
        item.value = p.value;
        item.effects = p.effects;
    }

    std::array<std::uint8_t, 63> raw{};
    raw[46] = static_cast<std::uint8_t>(item.type);
    raw[47] = static_cast<std::uint8_t>(item.name1);
    raw[48] = static_cast<std::uint8_t>(item.name2);
    raw[49] = static_cast<std::uint8_t>(item.name3);
    raw[50] = static_cast<std::uint8_t>(item.plus);
    raw[51] = static_cast<std::uint8_t>(item.plus_save);
    raw[53] = static_cast<std::uint8_t>(item.hidden_names);
    raw[55] = static_cast<std::uint8_t>(item.weight & 255);
    raw[56] = static_cast<std::uint8_t>(item.weight >> 8);
    raw[57] = static_cast<std::uint8_t>(item.count);
    raw[58] = static_cast<std::uint8_t>(item.value & 255);
    raw[59] = static_cast<std::uint8_t>((item.value >> 8) & 255);
    for (unsigned n = 0; n < 3; ++n)
        raw[60 + n] = static_cast<std::uint8_t>(item.effects[n]);
    auto decoded = decode_items(raw);
    if (!decoded || decoded->size() != 1)
        throw std::runtime_error("Generated treasure item is not a valid record");
    return std::move(decoded->front());
}
} // namespace

std::vector<ItemRecord> random_treasure_items(unsigned count, const DieRoller &roll)
{
    if (count > 127)
        throw std::runtime_error("Too many random treasure items");
    std::vector<ItemRecord> items;
    for (unsigned n = 0; n < count; ++n)
        items.push_back(generate_item(random_item_type(roll), roll));
    return items;
}
} // namespace opengold::por
