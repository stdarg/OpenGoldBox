#include "opengold/monster_bundle.h"
#include "json_document.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace opengold::por
{
namespace
{
using detail::Json;
constexpr std::uint16_t art_size = 128;

template <typename T> const T &field(const Json &object, std::string_view key)
{
    return detail::json_field<T>(object, key, "Monster data");
}

int whole(const Json &object, std::string_view key, int low, int high)
{
    const double number = field<double>(object, key);
    if (number != std::floor(number) || number < low || number > high)
    {
        throw std::runtime_error("Invalid monster data " + std::string(key));
    }
    return static_cast<int>(number);
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw std::runtime_error("Cannot read monster art " + path.string());
    }
    return {std::istreambuf_iterator<char>(in), {}};
}

// A pose file inside the creature's folder.
Image pose(const std::filesystem::path &folder, const std::string &relative,
           const PngDecoder &decode)
{
    const std::filesystem::path path(relative);
    if (path.is_absolute() || std::ranges::find(path, "..") != path.end())
    {
        throw std::runtime_error("Monster art outside its folder: " + relative);
    }
    auto image = decode(read_bytes(folder / path));
    if (!image || image->width != art_size || image->height != art_size ||
        image->rgba.size() != std::size_t{art_size} * art_size * 4)
    {
        throw std::runtime_error("Monster art is not 128 x 128: " + (folder / path).string());
    }
    return std::move(*image);
}

MonsterItem item(const Json &entry)
{
    MonsterItem result;
    result.id = field<std::string>(entry, "id");
    result.name = field<std::string>(entry, "name");
    result.kind = field<std::string>(entry, "kind");
    result.weapon_role = field<std::string>(entry, "weapon_role");
    result.readied = field<bool>(entry, "readied");
    result.source_type = whole(entry, "source_type", -1, 255);
    result.source_index = whole(entry, "source_index", -1, 255);
    if (result.kind != "weapon" && result.kind != "shield" && result.kind != "gear")
    {
        throw std::runtime_error("Unknown monster item kind " + result.kind);
    }
    return result;
}

ExportedMonster monster(const std::filesystem::path &folder, const PngDecoder &decode)
{
    const auto file = detail::read_json_file(folder / "monster.json", "monster data");
    if (field<std::string>(file, "format") != "opengold-monster-v1")
    {
        throw std::runtime_error("Unknown monster data format in " + folder.string());
    }
    ExportedMonster result;
    result.key = field<std::string>(file, "key");
    result.creature_archive = field<std::string>(file, "creature_archive");
    result.creature_record = static_cast<unsigned>(whole(file, "creature_record", 0, 255));
    if (file.find("copied_from"))
    {
        result.copied_from = field<std::string>(file, "copied_from");
        result.origin_key = field<std::string>(file, "origin_key");
    }
    result.name = field<std::string>(file, "name");
    if (const auto *size = file.find("sprite_size"); size && !std::holds_alternative<std::nullptr_t>(size->value))
    {
        result.width_squares = static_cast<unsigned>(whole(*size, "width_squares", 1, 3));
        result.height_squares = static_cast<unsigned>(whole(*size, "height_squares", 1, 3));
    }
    result.humanoid_size = field<std::string>(file, "humanoid_size");
    for (const auto &entry : field<Json::Array>(file, "equipment"))
    {
        result.equipment.push_back(item(entry));
    }
    for (const auto &entry : field<Json::Array>(file, "variants"))
    {
        result.variants.push_back({field<std::string>(entry, "id"),
                                   field<std::string>(entry, "weapon_id"),
                                   field<std::string>(entry, "shield_id"),
                                   pose(folder, field<std::string>(entry, "ready"), decode),
                                   pose(folder, field<std::string>(entry, "action"), decode)});
    }
    if (!result.creature_archive.starts_with("MON") || result.name.empty())
    {
        throw std::runtime_error("Monster data names no creature in " + folder.string());
    }
    return result;
}

const MonsterItem *readied(const ExportedMonster &monster, std::string_view kind)
{
    const auto found = std::ranges::find_if(monster.equipment,
                                            [&](const auto &item)
                                            {
                                                return item.readied && item.kind == kind;
                                            });
    return found == monster.equipment.end() ? nullptr : &*found;
}
} // namespace

const ExportedMonster *MonsterBundle::creature(unsigned bank, unsigned record) const
{
    const auto found =
        monsters.find("MON" + std::to_string(bank) + "CHA.DAX:" + std::to_string(record));
    return found == monsters.end() ? nullptr : &found->second;
}

MonsterBundle load_monster_bundle(const std::filesystem::path &folder, const PngDecoder &decode)
{
    MonsterBundle bundle;
    if (!std::filesystem::is_directory(folder))
    {
        return bundle;
    }
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.is_directory())
        {
            auto exported = monster(entry.path(), decode);
            const auto key = exported.key;
            if (!bundle.monsters.emplace(key, std::move(exported)).second)
            {
                throw std::runtime_error("Monster exported twice: " + key);
            }
        }
    }
    return bundle;
}

const MonsterVariant *readied_variant(const ExportedMonster &monster, bool archer)
{
    if (monster.variants.empty())
    {
        return nullptr;
    }
    const auto *shield = readied(monster, "shield");
    // The first set whose weapon is wanted, with the readied shield if any set
    // has it.
    const auto first_set = [&](auto wanted) -> const MonsterVariant *
    {
        const MonsterVariant *without_shield = nullptr;
        for (const auto &variant : monster.variants)
        {
            const auto weapon = std::ranges::find(monster.equipment, variant.weapon_id, &MonsterItem::id);
            if (weapon == monster.equipment.end() || !wanted(*weapon))
            {
                continue;
            }
            if (!shield || variant.shield_id == shield->id)
            {
                return &variant;
            }
            without_shield = without_shield ? without_shield : &variant;
        }
        return without_shield;
    };
    // An archer shoots whether or not the original readied its bow.
    if (const auto *bow = archer ? first_set([](const MonsterItem &weapon)
                                             {
                                                 return weapon.weapon_role == "ranged";
                                             })
                                 : nullptr)
    {
        return bow;
    }
    if (const auto *melee = first_set([](const MonsterItem &weapon)
                                      {
                                          return weapon.readied && weapon.weapon_role != "ranged";
                                      }))
    {
        return melee;
    }
    const auto *armed = first_set([](const MonsterItem &weapon)
                                  {
                                      return weapon.readied;
                                  });
    return armed ? armed : &monster.variants.front();
}

double art_scale(const ExportedMonster &monster)
{
    if (monster.humanoid_size == "small")
    {
        return 1.0;
    }
    if (monster.humanoid_size == "normal")
    {
        return 1.45;
    }
    if (monster.humanoid_size == "tall")
    {
        return 2.1;
    }
    return std::max(monster.width_squares, monster.height_squares);
}

unsigned planted_ground(const Image &pose)
{
    for (unsigned y = pose.height; y > 0; --y)
    {
        for (unsigned x = 0; x < pose.width; ++x)
        {
            if (pose.rgba[((y - 1) * pose.width + x) * 4 + 3] != 0)
            {
                return y;
            }
        }
    }
    return pose.height;
}
} // namespace opengold::por
