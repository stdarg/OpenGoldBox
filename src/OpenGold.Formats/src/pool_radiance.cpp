#include "opengold/pool_radiance.h"
#include "little_endian.h"

#include <algorithm>
#include <string_view>
#include <utility>

namespace opengold::por
{
namespace
{
constexpr int signed_byte(std::uint8_t value)
{
    return value < 128 ? value : static_cast<int>(value) - 256;
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

struct StringTable
{
    std::size_t start{}, slot{};
};

std::vector<std::uint8_t> pascal(std::string_view text)
{
    std::vector<std::uint8_t> bytes{static_cast<std::uint8_t>(text.size())};
    bytes.insert(bytes.end(), text.begin(), text.end());
    return bytes;
}

// START.EXE keeps each name table as Pascal strings in fixed-size slots. The tables have no
// header, so one is found by its first two entries; their distance is the slot size.
std::optional<StringTable> find_string_table(std::span<const std::uint8_t> image,
                                             std::string_view first, std::string_view second)
{
    const auto head = pascal(first), next = pascal(second);

    for (auto at = std::search(image.begin(), image.end(), head.begin(), head.end());
         at != image.end(); at = std::search(at + 1, image.end(), head.begin(), head.end()))
    {
        const auto start = static_cast<std::size_t>(at - image.begin());

        for (std::size_t slot = head.size(); slot <= 256 && start + slot < image.size(); ++slot)
        {
            const auto candidate = image.subspan(start + slot);

            if (candidate.size() >= next.size() &&
                std::equal(next.begin(), next.end(), candidate.begin()))
            {
                return StringTable{start, slot};
            }
        }
    }

    return std::nullopt;
}

// nullopt when the slot is outside the image or does not hold a printable string.
std::optional<std::string> slot_text(std::span<const std::uint8_t> image, const StringTable &table,
                                     std::size_t index)
{
    const auto offset = table.start + index * table.slot;

    if (offset + table.slot > image.size() || image[offset] >= table.slot)
    {
        return std::nullopt;
    }

    const auto text = image.subspan(offset + 1, image[offset]);

    if (!std::all_of(text.begin(), text.end(),
                     [](auto c)
                     {
                         return c >= 32 && c < 127;
                     }))
    {
        return std::nullopt;
    }

    return std::string(text.begin(), text.end());
}
} // namespace

std::optional<CharacterRecord> decode_character(std::span<const std::uint8_t> b)
{
    if (b.size() != 285 || b[0] > 15)
    {
        return std::nullopt;
    }
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
    {
        c.base_attacks[i] = {b[161 + i], damage(b, 163 + i, 2)};
    }
    c.saves = {b[109], b[110], b[111], b[112], b[113]};
    c.levels = {b[150], b[151], b[152], b[153], b[154], b[155], b[156], b[157]};
    c.race = b[46];
    c.character_class = b[47];
    c.age = format::read_u16(b, 48);
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
    c.experience = format::read_u32(b, 172);
    c.base_xp_award = format::read_u16(b, 184);
    c.xp_per_hit_point = b[186];
    for (std::size_t i = 0; i < c.wealth.size(); ++i)
    {
        c.wealth[i] = format::read_u16(b, 136 + 2 * i);
    }
    c.strength_bonus_flag_raw = b[170];
    c.item_count = b[199];
    c.item_limit = b[176];
    c.hands_equipped = b[256];
    c.save_bonus = signed_byte(b[257]);
    c.encumbrance = format::read_u16(b, 258);
    c.status = b[268];
    c.enabled = b[269];
    c.hostile = b[270];
    c.quick_fight = b[271];
    c.current.thac0 = 60 - b[272];
    c.current.armor_class = 60 - b[273];
    c.current.rear_armor_class = 60 - b[274];
    c.current.attacks_raw = {b[275], b[276]};
    for (std::size_t i = 0; i < 2; ++i)
    {
        c.current.damage[i] = damage(b, 277 + i, 2);
    }
    c.current.hit_points_raw = b[283];
    c.current.movement = b[284];
    return c;
}

std::optional<std::vector<ItemRecord>> decode_items(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() % 63 != 0)
    {
        return std::nullopt;
    }
    std::vector<ItemRecord> result;
    for (std::size_t p = 0; p < bytes.size(); p += 63)
    {
        const auto b = bytes.subspan(p, 63);
        if (b[0] > 45)
        {
            return std::nullopt;
        }
        ItemRecord item;
        copy(b, 0, item.raw);
        item.stored_name.assign(b.begin() + 1, b.begin() + 1 + b[0]);
        item.type = b[46];
        item.name_components = {b[47], b[48], b[49]};
        item.magic_bonus = signed_byte(b[50]);
        item.save_bonus = signed_byte(b[51]);
        item.readied_raw = b[52];
        item.hidden_name_words = b[53];
        item.cursed_raw = b[54];
        item.weight = format::read_u16(b, 55);
        item.stack_size = b[57];
        item.value = format::read_u16(b, 58);
        item.effect_codes = {b[60], b[61], b[62]};
        result.push_back(std::move(item));
    }
    return result;
}

std::optional<std::vector<EffectRecord>> decode_effects(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() % 9 != 0)
    {
        return std::nullopt;
    }
    std::vector<EffectRecord> result;
    for (std::size_t p = 0; p < bytes.size(); p += 9)
    {
        const auto b = bytes.subspan(p, 9);
        EffectRecord effect;
        copy(b, 0, effect.raw);
        effect.code = b[0];
        effect.duration_raw = format::read_u16(b, 1);
        effect.data = b[3];
        effect.table_flag = b[4];
        effect.next_pointer_raw = format::read_u32(b, 5);
        result.push_back(effect);
    }
    return result;
}

std::optional<std::vector<ItemTemplate>> decode_item_templates(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < 18 || (bytes.size() - 2) % 16 != 0 || (bytes.size() - 2) / 16 > 256)
    {
        return std::nullopt;
    }
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

std::optional<std::vector<std::string>> decode_item_words(std::span<const std::uint8_t> image)
{
    const auto table = find_string_table(image, "Battle Axe", "Hand Axe");

    if (!table)
    {
        return std::nullopt;
    }

    std::vector<std::string> words{""};

    for (std::size_t index = 0; index < 255; ++index)
    {
        auto text = slot_text(image, *table, index);

        if (!text)
        {
            return std::nullopt;
        }

        words.push_back(std::move(*text));
    }

    return words;
}

std::optional<std::vector<std::string>> decode_spell_names(std::span<const std::uint8_t> image)
{
    const auto table = find_string_table(image, "Bless", "Curse");

    if (!table)
    {
        return std::nullopt;
    }

    // The table ends at the first slot without a name.
    std::vector<std::string> spells{""};

    for (std::size_t index = 0;; ++index)
    {
        auto text = slot_text(image, *table, index);

        if (!text || text->empty())
        {
            return spells;
        }

        spells.push_back(std::move(*text));
    }
}

std::string item_name(const ItemRecord &item, std::span<const std::string> words,
                      std::uint8_t hidden)
{
    std::string name;

    for (std::size_t n = item.name_components.size(); n-- > 0;)
    {
        const auto component = item.name_components[n];
        const auto mask = static_cast<std::uint8_t>(4 >> n);
        if (component == 0 || component >= words.size() || words[component].empty() ||
            (hidden & mask))
        {
            continue;
        }

        if (!name.empty())
        {
            name += ' ';
        }

        name += words[component];
    }

    return name;
}
} // namespace opengold::por
