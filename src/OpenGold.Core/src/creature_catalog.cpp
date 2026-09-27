#include "opengold/creature_catalog.h"
#include "opengold/formats.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <utility>

namespace opengold::por
{
namespace
{
std::string upper_ascii(std::string_view text)
{
    std::string result(text);
    for (auto &c : result)
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    return result;
}

std::vector<std::uint8_t> read_file(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw CatalogError("Cannot open " + path.string());
    // DAX has at most 256 records of at most 65535 bytes each. Bound allocations.
    const auto size = std::filesystem::file_size(path);
    if (size > 32 * 1024 * 1024)
        throw CatalogError("Asset exceeds size limit: " + path.string());
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
    if (input.bad() || bytes.size() != size)
        throw CatalogError("Cannot read " + path.string());
    return bytes;
}

using Archive = std::map<std::uint8_t, std::vector<std::uint8_t>>;

Archive read_archive(const std::filesystem::path &path)
{
    auto decoded = decode_dax_archive(read_file(path));
    if (!decoded)
        throw CatalogError("Invalid DAX archive: " + path.string());
    Archive records;
    for (auto &record : decoded.records)
        records.emplace(record.id, std::move(record.bytes));
    return records;
}

EquipmentBonuses equipment_bonuses(const ItemRecord &item, const ItemTemplate &base)
{
    EquipmentBonuses b;
    b.save_bonus = item.save_bonus;
    // Bonus bytes on wands/charged items are not weapon enchantments.
    if (item.type <= 47 || item.type == 73 || (base.melee_flag & 0x80) != 0)
    {
        b.weapon_to_hit = item.magic_bonus;
        b.weapon_damage = item.magic_bonus;
    }
    const int protection = base.protection_raw & 0x7f;
    if ((base.protection_raw & 0x80) != 0)
    {
        if (base.worn_location == 1)
        {
            b.ac_adjustment = -protection - item.magic_bonus;
        }
        else if (protection >= 40)
        {
            // Includes armor substitutes such as bracers as well as body armor.
            b.armor_base_ac = 60 - protection;
            b.ac_adjustment = -item.magic_bonus;
        }
        else if (protection == 0)
        {
            // Protection rings/cloaks/etc. remain separate contributions, not a sum.
            b.ac_adjustment = -item.magic_bonus;
        }
    }
    return b;
}

std::string_view item_type_name(std::uint8_t type)
{
    if (type == 73)
        return "Arrows";
    // Type IDs, not names of particular enchanted items. Name components remain exposed.
    constexpr std::string_view names[] = {"Unarmed",
                                          "Battle axe",
                                          "Hand axe",
                                          "Bardiche",
                                          "Bec de corbin",
                                          "Bill-guisarme",
                                          "Bo stick",
                                          "Club",
                                          "Dagger",
                                          "Dart",
                                          "Fauchard",
                                          "Fauchard-fork",
                                          "Flail",
                                          "Military fork",
                                          "Glaive",
                                          "Glaive-guisarme",
                                          "Guisarme",
                                          "Guisarme-voulge",
                                          "Halberd",
                                          "Lucern hammer",
                                          "Hammer",
                                          "Javelin",
                                          "Jo stick",
                                          "Mace",
                                          "Morning star",
                                          "Partisan",
                                          "Military pick",
                                          "Awl pike",
                                          "Quarrel",
                                          "Ranseur",
                                          "Scimitar",
                                          "Spear",
                                          "Spetum",
                                          "Quarterstaff",
                                          "Bastard sword",
                                          "Broad sword",
                                          "Long sword",
                                          "Short sword",
                                          "Two-handed sword",
                                          "Trident",
                                          "Voulge",
                                          "Composite long bow",
                                          "Composite short bow",
                                          "Long bow",
                                          "Short bow",
                                          "Heavy crossbow",
                                          "Light crossbow",
                                          "Sling",
                                          "Mail",
                                          "Armor",
                                          "Leather armor",
                                          "Padded armor",
                                          "Studded armor",
                                          "Ring armor",
                                          "Scale armor",
                                          "Chain armor",
                                          "Splint armor",
                                          "Banded armor",
                                          "Plate armor",
                                          "Shield"};
    return type < std::size(names) ? names[type] : std::string_view{};
}
} // namespace

std::string CreatureId::archive() const
{
    return "MON" + std::to_string(bank) + "CHA.DAX";
}

std::string CreatureId::key() const
{
    return archive() + ":" + std::to_string(record);
}

AbilityModifiers ability_modifiers(const CharacterRecord &c)
{
    AbilityModifiers result;
    const auto &a = c.abilities;
    // AD&D reference table; applicability to a monster's natural attack is separate.
    if (a.strength >= 3 && a.strength <= 25 && (a.strength != 18 || a.exceptional_strength <= 100))
    {
        int hit = 0, damage = 0;
        if (a.strength == 3)
        {
            hit = -3;
            damage = -1;
        }
        else if (a.strength <= 5)
        {
            hit = -2;
            damage = -1;
        }
        else if (a.strength <= 7)
        {
            hit = -1;
        }
        else if (a.strength <= 15)
        { /* no adjustment */
        }
        else if (a.strength == 16)
        {
            damage = 1;
        }
        else if (a.strength == 17)
        {
            hit = 1;
            damage = 1;
        }
        else if (a.strength == 18)
        {
            const auto exceptional = a.exceptional_strength;
            if (exceptional == 0)
            {
                hit = 1;
                damage = 2;
            }
            else if (exceptional <= 50)
            {
                hit = 1;
                damage = 3;
            }
            else if (exceptional <= 75)
            {
                hit = 2;
                damage = 3;
            }
            else if (exceptional <= 90)
            {
                hit = 2;
                damage = 4;
            }
            else if (exceptional <= 99)
            {
                hit = 2;
                damage = 5;
            }
            else
            {
                hit = 3;
                damage = 6;
            }
        }
        else
        {
            constexpr std::array<int, 7> hits{3, 3, 4, 4, 5, 6, 7};
            hit = hits[a.strength - 19];
            damage = a.strength == 25 ? 14 : a.strength - 12;
        }
        result.strength_to_hit = hit;
        result.strength_damage = damage;
    }
    if (a.dexterity >= 3 && a.dexterity <= 19)
    {
        int missile = 0, ac = 0;
        if (a.dexterity <= 6)
        {
            missile = std::min(0, static_cast<int>(a.dexterity) - 6);
            ac = 7 - a.dexterity;
        }
        else if (a.dexterity == 15)
            ac = -1;
        else if (a.dexterity >= 16)
        {
            missile = std::min(3, a.dexterity - 15);
            ac = -std::min(4, a.dexterity - 14);
        }
        result.dexterity_missile_to_hit = missile;
        result.dexterity_ac_adjustment = ac;
    }
    if (a.constitution >= 3 && a.constitution <= 18)
    {
        const int hp = a.constitution == 3    ? -2
                       : a.constitution <= 6  ? -1
                       : a.constitution <= 14 ? 0
                                              : a.constitution - 14;
        for (std::size_t cls = 0; cls < 8; ++cls)
            result.constitution_hp_per_hit_die_by_class[cls] =
                cls >= 2 && cls <= 4 ? hp : std::min(hp, 2);
        if (c.character_class < 8)
            result.constitution_hp_per_hit_die =
                result.constitution_hp_per_hit_die_by_class[c.character_class];
        else if (c.character_class == 17)
            result.constitution_hp_per_hit_die =
                std::min(hp, 2); // Non-warrior reference only, not monster HP generation.
    }
    if (c.strength_bonus_flag_raw <= 1)
        result.strength_bonus_allowed_hint = c.strength_bonus_flag_raw != 0;
    return result;
}

std::string Equipment::label() const
{
    if (!stored.stored_name.empty())
        return stored.stored_name;
    const auto type_name = item_type_name(stored.type);
    std::string name =
        type_name.empty() ? "Item type " + std::to_string(stored.type) : std::string(type_name);
    // Do not label charges on wands, scrolls, or potions as +N enchantments.
    if ((bonuses.weapon_to_hit || bonuses.ac_adjustment) && stored.magic_bonus != 0)
        name += (stored.magic_bonus > 0 ? " +" : " ") + std::to_string(stored.magic_bonus);
    return name;
}

CreatureCatalog CreatureCatalog::load(const std::filesystem::path &directory)
{
    try
    {
        // DOS names are case-insensitive; support copied installations on case-sensitive hosts too.
        std::map<std::string, std::filesystem::path> files;
        for (const auto &entry : std::filesystem::directory_iterator(directory))
        {
            if (!entry.is_regular_file())
                continue;
            const auto name = upper_ascii(entry.path().filename().string());
            if (name != "ITEMS" && !(name.starts_with("MON") && name.ends_with(".DAX")))
                continue;
            if (!files.emplace(name, entry.path()).second)
                throw CatalogError("Ambiguous asset filename: " + name);
        }
        const auto required = [&](const std::string &name) -> const std::filesystem::path &
        {
            const auto file = files.find(name);
            if (file == files.end())
                throw CatalogError("Missing required asset " + name + " in " + directory.string());
            return file->second;
        };
        const auto templates = decode_item_templates(read_file(required("ITEMS")));
        if (!templates)
            throw CatalogError("Invalid ITEMS template file");
        CreatureCatalog catalog;
        for (std::uint8_t bank = 1; bank <= 8; ++bank)
        {
            const std::string prefix = "MON" + std::to_string(bank);
            auto characters = read_archive(required(prefix + "CHA.DAX"));
            auto items = read_archive(required(prefix + "ITM.DAX"));
            Archive effects;
            const auto spc_name = prefix + "SPC.DAX";
            if (files.contains(spc_name) || (bank != 1 && bank != 3))
                effects = read_archive(required(spc_name));
            for (const auto &[id, bytes] : items)
                if (!characters.contains(id))
                    throw CatalogError(prefix + "ITM.DAX:" + std::to_string(id) +
                                       " has no matching character");
            for (const auto &[id, bytes] : effects)
                if (!characters.contains(id))
                    throw CatalogError(spc_name + ":" + std::to_string(id) +
                                       " has no matching character");
            for (const auto &[id, bytes] : characters)
            {
                Creature creature;
                creature.id = {bank, id};
                auto character = decode_character(bytes);
                if (!character)
                    throw CatalogError("Invalid character " + creature.id.key());
                creature.stored = std::move(*character);
                creature.abilities = ability_modifiers(creature.stored);
                if (const auto entry = items.find(id); entry != items.end())
                {
                    auto decoded = decode_items(entry->second);
                    if (!decoded)
                        throw CatalogError("Invalid items " + prefix +
                                           "ITM.DAX:" + std::to_string(id));
                    for (auto &item : *decoded)
                    {
                        if (item.type >= templates->size())
                            throw CatalogError("Missing item template " +
                                               std::to_string(item.type) + " for " +
                                               creature.id.key());
                        Equipment equipment;
                        equipment.index = creature.equipment.size();
                        equipment.base = (*templates)[item.type];
                        equipment.bonuses = equipment_bonuses(item, equipment.base);
                        equipment.stored = std::move(item);
                        creature.equipment.push_back(std::move(equipment));
                    }
                }
                if (creature.equipment.size() != creature.stored.item_count)
                    creature.interpretation_notes.emplace_back(
                        "CHA item count differs from the associated ITM record; all ITM items retained.");
                if (const auto entry = effects.find(id); entry != effects.end())
                {
                    auto decoded = decode_effects(entry->second);
                    if (!decoded)
                        throw CatalogError("Invalid effects " + spc_name + ":" +
                                           std::to_string(id));
                    for (auto &effect : *decoded)
                    {
                        const auto definition = describe_effect(effect.code);
                        if (definition.kind == EffectKind::unknown)
                            creature.interpretation_notes.push_back("Unknown SPC effect " +
                                                                    std::to_string(effect.code));
                        creature.effects.push_back({std::move(effect), definition});
                    }
                }
                if (!creature.abilities.strength_to_hit ||
                    !creature.abilities.dexterity_ac_adjustment ||
                    !creature.abilities.constitution_hp_per_hit_die_by_class[0])
                    creature.interpretation_notes.emplace_back(
                        "An ability score is outside the supported reference modifier tables.");
                catalog.creatures_.emplace(creature.id, std::move(creature));
            }
        }
        if (catalog.creatures_.empty())
            throw CatalogError("No monster/NPC records found");
        return catalog;
    }
    catch (const std::filesystem::filesystem_error &error)
    {
        throw CatalogError(error.what());
    }
}

std::optional<std::reference_wrapper<const Creature>>
CreatureCatalog::find(CreatureId id) const noexcept
{
    const auto found = creatures_.find(id);
    if (found == creatures_.end())
        return std::nullopt;
    return std::cref(found->second);
}

std::vector<std::reference_wrapper<const Creature>>
CreatureCatalog::find_by_name(std::string_view name) const
{
    const auto wanted = upper_ascii(name);
    std::vector<std::reference_wrapper<const Creature>> found;
    for (const auto &[id, creature] : creatures_)
        if (upper_ascii(creature.stored.name) == wanted)
            found.push_back(std::cref(creature));
    return found;
}
} // namespace opengold::por
