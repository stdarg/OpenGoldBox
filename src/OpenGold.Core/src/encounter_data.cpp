#include "opengold/encounter_data.h"
#include "json_document.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <set>
#include <stdexcept>
#include <string_view>

namespace opengold::por
{
namespace
{
using detail::Json;

constexpr std::string_view conversions_format = "opengold-conversions-v1";
constexpr std::string_view overrides_format = "opengold-encounter-overrides-v1";
constexpr double most_xp = 1'000'000;
// More than a fight holds (56); the game stops loading at its limit.
constexpr double most_loaded = 250;

// Names the file, the entry and the problem.
class Problems
{
  public:
    explicit Problems(std::string file) : file_(std::move(file))
    {
    }

    [[noreturn]] void reject(const std::string &where, std::string_view problem) const
    {
        throw std::runtime_error("data/encounters/" + file_ + ": " + where + ": " +
                                 std::string(problem));
    }

    // A whole number in [low, high].
    [[nodiscard]] unsigned whole(const Json &value, const std::string &where, std::string_view key,
                                 double low, double high) const
    {
        const auto *number = std::get_if<double>(&value.value);
        if (!number || *number != std::floor(*number) || *number < low || *number > high)
        {
            reject(where, std::string(key) + " must be a whole number from " +
                              std::to_string(static_cast<long>(low)) + " to " +
                              std::to_string(static_cast<long>(high)));
        }
        return static_cast<unsigned>(*number);
    }

    [[nodiscard]] unsigned whole_member(const Json &object, std::string_view key,
                                        const std::string &where, double low, double high) const
    {
        const auto *found = object.find(key);
        if (!found)
        {
            reject(where, "missing " + std::string(key));
        }
        return whole(*found, where, key, low, high);
    }

    // A member that may be left out.
    [[nodiscard]] std::optional<unsigned> optional_whole(const Json &object, std::string_view key,
                                                         const std::string &where, double low,
                                                         double high) const
    {
        const auto *found = object.find(key);
        return found ? std::optional(whole(*found, where, key, low, high)) : std::nullopt;
    }

    template <typename T>
    [[nodiscard]] const T &member(const Json &object, std::string_view key,
                                  const std::string &where) const
    {
        const auto *found = object.find(key);
        const auto *value = found ? std::get_if<T>(&found->value) : nullptr;
        if (!value)
        {
            reject(where, "missing or invalid " + std::string(key));
        }
        return *value;
    }

    void expect_format(const Json &document, std::string_view format) const
    {
        const auto *found = document.find("format");
        if (!found || !std::holds_alternative<std::string>(found->value) ||
            std::get<std::string>(found->value) != format)
        {
            reject("format", "must be " + std::string(format));
        }
    }

  private:
    std::string file_;
};

std::pair<unsigned, EncounterConversion>
read_creature(const Problems &problems, const Json &creature, const std::string &area)
{
    const auto record = problems.whole_member(creature, "record", area, 0, 255);
    const auto where = area + ", record " + std::to_string(record);
    EncounterConversion conversion{problems.member<std::string>(creature, "definition", where),
                                   problems.whole_member(creature, "fit_xp", where, 0, most_xp),
                                   problems.whole_member(creature, "award_xp", where, 0, most_xp)};
    if (conversion.definition.empty())
    {
        problems.reject(where, "empty definition");
    }
    return {record, std::move(conversion)};
}

AreaConversions read_area(const Problems &problems, const Json &entry, std::size_t index)
{
    auto where = "area " + std::to_string(index + 1);
    AreaConversions area;
    area.name = problems.member<std::string>(entry, "name", where);
    where += " (" + area.name + ")";
    for (const auto &script : problems.member<Json::Array>(entry, "scripts", where))
    {
        area.scripts.push_back(problems.whole(script, where, "scripts", 0, 255));
    }
    if (area.scripts.empty())
    {
        problems.reject(where, "no scripts");
    }
    area.bank = problems.whole_member(entry, "bank", where, 1, 255);
    std::set<unsigned> records;
    for (const auto &creature : problems.member<Json::Array>(entry, "creatures", where))
    {
        auto conversion = read_creature(problems, creature, where);
        if (!records.insert(conversion.first).second)
        {
            problems.reject(where, "record " + std::to_string(conversion.first) + " listed twice");
        }
        area.creatures.push_back(std::move(conversion));
    }
    return area;
}

std::vector<AreaConversions> read_conversions(const std::filesystem::path &folder)
{
    const Problems problems("conversions.json");
    const auto document = detail::read_json_file(folder / "conversions.json", "encounter data");
    problems.expect_format(document, conversions_format);
    std::vector<AreaConversions> areas;
    std::set<unsigned> scripts;
    const auto &entries = problems.member<Json::Array>(document, "areas", "file");
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        auto area = read_area(problems, entries[index], index);
        for (const auto script : area.scripts)
        {
            if (!scripts.insert(script).second)
            {
                problems.reject(area.name, "script " + std::to_string(script) + " is in two areas");
            }
        }
        areas.push_back(std::move(area));
    }
    return areas;
}

// "ECL<bank>.DAX:<script>:0x<address>", or nothing.
std::optional<EncounterSite> parse_site(std::string_view name)
{
    const auto number = [&](std::size_t length, int base) -> std::optional<unsigned>
    {
        unsigned value{};
        const auto [end, error] = std::from_chars(name.data(), name.data() + length, value, base);
        if (error != std::errc{} || end != name.data() + length || length == 0)
        {
            return std::nullopt;
        }
        name.remove_prefix(length);
        return value;
    };
    if (!name.starts_with("ECL"))
    {
        return std::nullopt;
    }
    name.remove_prefix(3);
    const auto bank = number(name.find(".DAX:"), 10);
    if (!bank || !name.starts_with(".DAX:"))
    {
        return std::nullopt;
    }
    name.remove_prefix(5);
    const auto script = number(name.find(":0x"), 10);
    if (!script || !name.starts_with(":0x"))
    {
        return std::nullopt;
    }
    name.remove_prefix(3);
    const auto address = number(name.size(), 16);
    if (!address || *bank == 0 || *bank > 255 || *script > 255 || *address > 0xFFFF)
    {
        return std::nullopt;
    }
    return EncounterSite{*bank, *script, static_cast<std::uint16_t>(*address)};
}

EncounterOverride read_override(const Problems &problems, const Json &entry, std::size_t index)
{
    auto where = "override " + std::to_string(index + 1);
    const auto &name = problems.member<std::string>(entry, "site", where);
    const auto site = parse_site(name);
    if (!site)
    {
        problems.reject(where, "site must be like ECL5.DAX:7:0xA60D, not " + name);
    }
    where = site_name(*site);
    const auto *original = entry.find("original");
    if (!original || !std::holds_alternative<Json::Object>(original->value))
    {
        problems.reject(where, "missing or invalid original");
    }
    EncounterOverride change{*site,
                             problems.whole_member(*original, "record", where, 0, 255),
                             problems.whole_member(*original, "count", where, 1, most_loaded),
                             problems.optional_whole(entry, "record", where, 0, 255),
                             problems.optional_whole(entry, "count", where, 1, most_loaded),
                             problems.optional_whole(entry, "icon", where, 0, 255),
                             problems.member<std::string>(entry, "reason", where)};
    if (!change.record && !change.count && !change.icon)
    {
        problems.reject(where, "changes nothing");
    }
    if (change.reason.empty())
    {
        problems.reject(where, "needs a reason");
    }
    return change;
}

std::vector<EncounterOverride> read_overrides(const std::filesystem::path &folder)
{
    const auto path = folder / "overrides.json";
    if (!std::filesystem::exists(path))
    {
        return {};
    }
    const Problems problems("overrides.json");
    const auto document = detail::read_json_file(path, "encounter overrides");
    problems.expect_format(document, overrides_format);
    std::vector<EncounterOverride> overrides;
    std::set<EncounterSite> sites;
    const auto &entries = problems.member<Json::Array>(document, "overrides", "file");
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        auto change = read_override(problems, entries[index], index);
        if (!sites.insert(change.site).second)
        {
            problems.reject(site_name(change.site), "site listed twice");
        }
        overrides.push_back(std::move(change));
    }
    return overrides;
}
} // namespace

std::string site_name(const EncounterSite &site)
{
    char address[8];
    std::snprintf(address, sizeof address, "%04X", static_cast<unsigned>(site.address));
    return "ECL" + std::to_string(site.bank) + ".DAX:" + std::to_string(site.script) + ":0x" +
           address;
}

const AreaConversions *EncounterData::area(unsigned script) const
{
    const auto found = std::find_if(areas.begin(), areas.end(),
                                    [script](const AreaConversions &area)
                                    {
                                        return std::find(area.scripts.begin(), area.scripts.end(),
                                                         script) != area.scripts.end();
                                    });
    return found == areas.end() ? nullptr : &*found;
}

EncounterData load_encounter_data(const std::filesystem::path &folder)
{
    return {read_conversions(folder), read_overrides(folder)};
}
} // namespace opengold::por
