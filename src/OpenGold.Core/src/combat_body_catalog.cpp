#include "opengold/combat_body_catalog.h"
#include "opengold/campaign_party.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string_view>

namespace opengold::por
{
namespace
{
unsigned index_field(std::string_view field, unsigned limit)
{
    if (field.empty() || field.find_first_not_of("0123456789") != std::string_view::npos)
        throw std::runtime_error("Invalid combat catalog number");
    const auto value = std::stoul(std::string(field));
    if (value >= limit)
        throw std::runtime_error("Combat catalog number out of range");
    return static_cast<unsigned>(value);
}
std::string strip_cr(std::string text)
{
    if (!text.empty() && text.back() == '\r')
        text.pop_back();
    return text;
}

} // namespace
CombatBodyCatalog CombatBodyCatalog::load(const std::filesystem::path &assignments,
                                          const std::filesystem::path &options_file)
{
    CombatBodyCatalog result;
    std::ifstream options(options_file);
    if (!options)
        throw std::runtime_error("Cannot open combat look options: " + options_file.string());
    std::set<std::string> keys{"unreviewed"};
    std::string line;
    std::set<int> item_types;
    while (std::getline(options, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        const auto first = line.find('\t'), second = line.find('\t', first + 1),
                   third = line.find('\t', second + 1);
        if (first == std::string::npos || second == std::string::npos ||
            third == std::string::npos || line.find('\t', third + 1) != std::string::npos)
            throw std::runtime_error("Invalid combat look option row");
        const auto variant = strip_cr(line.substr(third + 1));
        CombatLookOption option{
            line.substr(0, first), line.substr(first + 1, second - first - 1),
            static_cast<int>(
                index_field(std::string_view(line).substr(second + 1, third - second - 1), 256))};
        if (variant != "ordinary" || option.id.empty() || option.label.empty() ||
            !keys.insert(option.id).second || !keys.insert(option.id + "_shield").second ||
            !item_types.emplace(option.original_type).second)
            throw std::runtime_error("Duplicate or invalid combat look option");
        result.options.push_back(std::move(option));
    }
    if (!options.eof() || result.options.empty())
        throw std::runtime_error("Invalid combat look options file");
    std::ifstream in(assignments);
    if (!in)
        throw std::runtime_error("Cannot open combat body catalog: " + assignments.string());
    std::array<bool, 35> seen{};
    unsigned count = 0;
    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        const auto tab = line.find('\t');
        if (tab == std::string::npos || line.find('\t', tab + 1) != std::string::npos)
            throw std::runtime_error("Invalid combat body catalog row");
        if (line.substr(0, tab) == "deleted")
        {
            const auto key = strip_cr(line.substr(tab + 1));
            if (key == "unreviewed" || !keys.contains(key) || !result.deleted.insert(key).second)
                throw std::runtime_error("Invalid deleted combat combination");
            continue;
        }
        const auto index = index_field(std::string_view(line).substr(0, tab), 35);
        auto values = strip_cr(line.substr(tab + 1));
        if (seen[index] || values.empty())
            throw std::runtime_error("Invalid or duplicate combat body assignment");
        if (values != "unreviewed")
            for (std::size_t begin = 0;;)
            {
                const auto end = values.find(',', begin);
                auto key = values.substr(begin, end == std::string::npos ? end : end - begin);
                if (key.starts_with("silver_"))
                    key = "type_" + key.substr(7);
                if (key == "unreviewed" || !keys.contains(key))
                    throw std::runtime_error("Unknown combat body assignment");
                result.bodies[index].insert(std::move(key));
                if (end == std::string::npos)
                    break;
                begin = end + 1;
            }
        seen[index] = true;
        ++count;
    }
    if (!in.eof() || count < 33 ||
        !std::all_of(
            seen.begin(), seen.begin() + 33,
            [](bool present)
            {
                return present;
            }))
        throw std::runtime_error("Combat body catalog must contain all original 33 bodies");
    for (auto &body : result.bodies)
        for (const auto &key : result.deleted)
            body.erase(key);
    return result;
}
ResolvedCombatAppearance resolve_combat_appearance(const PartyMember &member,
                                                   const CombatBodyCatalog &catalog)
{
    std::vector<CombatEquipment> equipped;
    equipped.reserve(member.equipped.size());
    for (const auto id : member.equipped)
    {
        const auto item = member.character.inventory().find(id);
        if (!item)
            throw std::runtime_error("Equipped item is missing");
        equipped.push_back(
            {item->get().original_type, item->get().name, item->get().definition_id});
    }
    auto appearance = member.character.appearance();
    auto selection = catalog.choose(equipped, appearance.combat_body);
    return {std::move(appearance), std::move(selection)};
}
Image ResolvedCombatAppearance::icon(const CharacterArt &art, bool action) const
{
    // Missing artwork must not falsely show the weapon baked into the saved body.
    return art.equipped_icon(appearance, selection.matched ? selection.body : 0, action);
}
CombatBodySelection CombatBodyCatalog::choose(std::span<const CombatEquipment> equipped,
                                              unsigned fallback) const
{
    bool shield = false;
    const CombatEquipment *weapon = nullptr;
    int weapon_type = -1;
    for (const auto &item : equipped)
    {
        if (item.original_type == 59 || item.definition_id == "shield")
            shield = true;
        else
        {
            int type = item.original_type;
            if (type < 0)
            {
                if (item.definition_id == "dagger")
                    type = 8;
                else if (item.definition_id == "mace")
                    type = 23;
                else if (item.definition_id == "quarterstaff")
                    type = 33;
                else if (item.definition_id == "longsword")
                    type = 36;
                else if (item.definition_id == "longbow")
                    type = 43;
                else if (item.definition_id == "shortbow")
                    type = 44;
            }
            if (std::any_of(options.begin(), options.end(),
                            [&](const auto &option)
                            {
                                return option.original_type == type;
                            }))
            {
                weapon = &item;
                weapon_type = type;
            }
        }
    }
    const auto type = weapon ? weapon_type : 0;
    const auto option = std::find_if(options.begin(), options.end(),
                                     [&](const auto &value)
                                     {
                                         return value.original_type == type;
                                     });
    const auto key = (option == options.end() ? "type_" + std::to_string(type) : option->id) +
                     (shield ? "_shield" : "");
    const auto label =
        (option == options.end() ? "Unknown weapon" : option->label) + (shield ? " & Shield" : "");
    if (deleted.contains(key))
        return {fallback, false, key, label};
    if (fallback < bodies.size() && bodies[fallback].contains(key))
        return {fallback, true, key, label};
    for (unsigned id = 0; id < bodies.size(); ++id)
        if (bodies[id].contains(key))
            return {id, true, key, label};
    return {fallback, false, key, label};
}
} // namespace opengold::por
