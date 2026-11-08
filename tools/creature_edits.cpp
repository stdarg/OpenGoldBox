#include "creature_edits.h"
#include "../src/OpenGold.Core/src/json_document.h"
#include "opengold/companions.h"
#include "opengold/encounter_data.h"
#include "opengold/rolf_tour.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iterator>
#include <map>
#include <regex>
#include <set>
#include <span>
#include <sstream>

namespace opengold::edits
{
namespace
{
using detail::Json;
namespace fs = std::filesystem;

const fs::path rules_path = "data/rules/srd-5.2.1/combat.rules";
const fs::path conversions_path = "data/encounters/conversions.json";
const fs::path overrides_path = "data/encounters/overrides.json";
const fs::path chances_path = "data/encounters/chances.json";
const fs::path companions_path = "data/companions/companions.json";
const fs::path srd5_path = "src/OpenGold.Rules.Srd5/src/srd5.cpp";
const fs::path saves_path = "docs/SAVES.md";
const fs::path deviations_path = "docs/DEVIATIONS.md";

[[noreturn]] void refuse(const std::string &where, const std::string &problem)
{
    throw ImportError(where + ": " + problem);
}

std::string read_text(const fs::path &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw ImportError("Cannot read " + path.string());
    }
    return {std::istreambuf_iterator<char>(in), {}};
}

// Written beside the file and renamed over it, so a failed write leaves the
// old file.
void write_text(const fs::path &path, const std::string &text)
{
    const auto temporary = fs::path(path.string() + ".import");
    {
        std::ofstream out(temporary, std::ios::binary);
        out << text;
        if (!out)
        {
            throw ImportError("Cannot write " + path.string());
        }
    }
    fs::rename(temporary, path);
}

Json *member(Json &object, std::string_view key)
{
    auto *members = std::get_if<Json::Object>(&object.value);
    if (!members)
    {
        return nullptr;
    }
    const auto found = std::find_if(members->begin(), members->end(),
                                    [&](const auto &entry)
                                    {
                                        return entry.name == key;
                                    });
    return found == members->end() ? nullptr : &found->value;
}

Json::Array &array_member(Json &object, std::string_view key, const std::string &file)
{
    auto *found = member(object, key);
    auto *array = found ? std::get_if<Json::Array>(&found->value) : nullptr;
    if (!array)
    {
        refuse(file, "has no " + std::string(key) + " list");
    }
    return *array;
}

std::optional<double> number_member(const Json &object, std::string_view key)
{
    const auto *found = object.find(key);
    const auto *number = found ? std::get_if<double>(&found->value) : nullptr;
    return number ? std::optional(*number) : std::nullopt;
}

std::string text_member(const Json &object, std::string_view key)
{
    const auto *found = object.find(key);
    const auto *text = found ? std::get_if<std::string>(&found->value) : nullptr;
    return text ? *text : std::string();
}

bool is_null(const Json &value)
{
    return std::holds_alternative<std::nullptr_t>(value.value);
}

// --- combat.rules -------------------------------------------------------------

// The rules file as lines. A creature is its `creature` row and every row
// whose second field names it.
class RulesText
{
  public:
    explicit RulesText(const std::string &text)
    {
        std::istringstream in(text);
        for (std::string line; std::getline(in, line);)
        {
            lines_.push_back(line);
        }
    }

    [[nodiscard]] std::string text() const
    {
        std::string out;
        for (const auto &line : lines_)
        {
            out += line + "\n";
        }
        return out;
    }

    [[nodiscard]] std::set<std::string> creatures() const
    {
        std::set<std::string> keys;
        for (const auto &line : lines_)
        {
            if (const auto fields = split(line); fields.size() > 1 && fields[0] == "creature")
            {
                keys.insert(fields[1]);
            }
        }
        return keys;
    }

    [[nodiscard]] std::vector<std::string> creature_fields(const std::string &key) const
    {
        return split(lines_.at(creature_row(key)));
    }

    // Every creature's fields, for the spellcasting profiles in use.
    [[nodiscard]] std::vector<std::vector<std::string>> all_creature_fields() const
    {
        std::vector<std::vector<std::string>> rows;
        for (const auto &line : lines_)
        {
            if (auto fields = split(line); fields.size() > 1 && fields[0] == "creature")
            {
                rows.push_back(std::move(fields));
            }
        }
        return rows;
    }

    void set_creature_fields(const std::string &key, const std::vector<std::string> &fields)
    {
        std::string line;
        for (const auto &field : fields)
        {
            line += (line.empty() ? "" : " ") + field;
        }
        lines_.at(creature_row(key)) = line;
    }

    // The row above the creature says why it was last changed.
    void note(const std::string &key, const std::string &date, const std::string &reason)
    {
        constexpr std::string_view prefix = "# Creature Stats Editor (";
        const auto row = creature_row(key);
        const auto line = std::string(prefix) + date + "): " + reason;
        if (row > 0 && lines_[row - 1].starts_with(prefix))
        {
            lines_[row - 1] = line;
            return;
        }
        lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(row), line);
    }

    // Copies `from`'s rows under the key `to`, after the rows that follow
    // `from`'s creature row.
    void copy(const std::string &from, const std::string &to)
    {
        std::vector<std::string> copies;
        for (const auto &line : lines_)
        {
            auto fields = split(line);
            if (fields.size() > 1 && fields[1] == from)
            {
                fields[1] = to;
                std::string copied;
                for (const auto &field : fields)
                {
                    copied += (copied.empty() ? "" : " ") + field;
                }
                copies.push_back(copied);
            }
        }
        auto at = creature_row(from) + 1;
        while (at < lines_.size() && split(lines_[at]).size() > 1 && split(lines_[at])[1] == from)
        {
            ++at;
        }
        lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(at), copies.begin(),
                      copies.end());
    }

  private:
    std::vector<std::string> lines_;

    [[nodiscard]] static std::vector<std::string> split(const std::string &line)
    {
        if (line.empty() || line.starts_with('#'))
        {
            return {};
        }
        std::vector<std::string> fields;
        std::istringstream in(line);
        for (std::string field; in >> field;)
        {
            fields.push_back(field);
        }
        return fields;
    }

    [[nodiscard]] std::size_t creature_row(const std::string &key) const
    {
        for (std::size_t row = 0; row < lines_.size(); ++row)
        {
            if (const auto fields = split(lines_[row]);
                fields.size() > 1 && fields[0] == "creature" && fields[1] == key)
            {
                return row;
            }
        }
        refuse("combat.rules", "no creature " + key);
    }
};

// Field positions in a `creature` row (row 2 of combat.rules names them).
constexpr std::size_t hit_points_field = 3;
constexpr std::array<std::size_t, 3> spell_profile_fields{17, 18, 19};

// --- The repository's files, as the import changes them -------------------------

struct Files
{
    RulesText rules;
    Json conversions, overrides, chances, companions;
    std::string srd5, saves, deviations;
    bool rules_changed{}, conversions_changed{}, overrides_changed{}, chances_changed{},
        companions_changed{};
};

Json parse(const std::string &text, const std::string &file)
{
    try
    {
        return detail::parse_json(text);
    }
    catch (const std::exception &)
    {
        throw ImportError(file + " is not valid JSON");
    }
}

Files read_files(const fs::path &repository)
{
    // An optional file the import may create: its list starts empty.
    const auto optional = [&](const fs::path &path, const std::string &empty)
    {
        return fs::exists(repository / path) ? parse(read_text(repository / path), path.string())
                                             : parse(empty, path.string());
    };
    return {RulesText(read_text(repository / rules_path)),
            parse(read_text(repository / conversions_path), conversions_path.string()),
            optional(overrides_path,
                     R"({"format": "opengold-encounter-overrides-v1", "overrides": []})"),
            optional(chances_path, R"({"format": "opengold-encounter-chances-v1", "chances": []})"),
            parse(read_text(repository / companions_path), companions_path.string()),
            read_text(repository / srd5_path),
            read_text(repository / saves_path),
            read_text(repository / deviations_path)};
}

// --- The edits ----------------------------------------------------------------

struct Edit
{
    const Json &json;
    std::string where, change, reason;
};

// What an edit changes: its status, and for docs/DEVIATIONS.md its subject,
// what it did and the marker that finds its entry again.
struct Applied
{
    EditResult result;
    std::string marker;
};

std::string number_text(double value)
{
    return detail::json_text(Json{value});
}

const Json &required(const Edit &edit, std::string_view key)
{
    const auto *found = edit.json.find(key);
    if (!found)
    {
        refuse(edit.where, "missing " + std::string(key));
    }
    return *found;
}

std::string required_text(const Edit &edit, std::string_view key)
{
    const auto text = text_member(edit.json, key);
    if (text.empty())
    {
        refuse(edit.where, "missing " + std::string(key));
    }
    return text;
}

double required_number(const Edit &edit, std::string_view key)
{
    const auto number = number_member(edit.json, key);
    if (!number || *number != std::floor(*number))
    {
        refuse(edit.where, std::string(key) + " must be a whole number");
    }
    return *number;
}

// Whether the data already holds `to`; refuses when it holds neither `from`
// nor `to`, as when someone changed it since the editor read it.
bool already_applied(const Edit &edit, const Json &current, const Json &from, const Json &to)
{
    if (detail::json_equal(current, to))
    {
        return true;
    }
    if (!detail::json_equal(current, from))
    {
        refuse(edit.where, "the data holds " + detail::json_text(current) +
                               ", not the edit's from " + detail::json_text(from));
    }
    return false;
}

Applied hit_points(Files &files, const Edit &edit, const std::string &date)
{
    const auto key = required_text(edit, "definition");
    const auto from = required_number(edit, "from"), to = required_number(edit, "to");
    if (to < 1 || to > 10000)
    {
        refuse(edit.where, "Hit Points must be 1 to 10000");
    }
    auto fields = files.rules.creature_fields(key);
    const Json current{std::stod(fields.at(hit_points_field))};
    const auto summary = "Hit Points " + number_text(from) + " to " + number_text(to);
    Applied applied{{edit.change, key, summary, EditStatus::already_applied}, "hit_points " + key};
    if (already_applied(edit, current, Json{from}, Json{to}))
    {
        return applied;
    }
    fields[hit_points_field] = number_text(to);
    files.rules.set_creature_fields(key, fields);
    files.rules.note(key, date, edit.reason);
    files.rules_changed = true;
    applied.result.status = EditStatus::applied;
    return applied;
}

Json profile_json(const std::vector<std::string> &fields)
{
    return Json{Json::Object{{"spell_slots", Json{std::stod(fields.at(spell_profile_fields[0]))}},
                             {"casting_bonus", Json{std::stod(fields.at(spell_profile_fields[1]))}},
                             {"level", Json{std::stod(fields.at(spell_profile_fields[2]))}}}};
}

std::string profile_text(const Json &profile)
{
    return "level " + number_text(number_member(profile, "level").value_or(0)) + " (" +
           number_text(number_member(profile, "spell_slots").value_or(0)) + " spell slots, +" +
           number_text(number_member(profile, "casting_bonus").value_or(0)) + ")";
}

Applied spell_profile(Files &files, const Edit &edit, const std::string &date)
{
    const auto key = required_text(edit, "definition");
    const auto &from = required(edit, "from");
    const auto &to = required(edit, "to");
    auto fields = files.rules.creature_fields(key);
    Applied applied{{edit.change, key,
                     "spellcasting " + profile_text(from) + " to " + profile_text(to),
                     EditStatus::already_applied},
                    "spell_profile " + key};
    if (already_applied(edit, profile_json(fields), from, to))
    {
        return applied;
    }
    // The rules' own profiles only (requirement 11): another creature has it.
    const auto rows = files.rules.all_creature_fields();
    if (std::none_of(rows.begin(), rows.end(),
                     [&](const auto &row)
                     {
                         return row.size() > spell_profile_fields.back() &&
                                detail::json_equal(profile_json(row), to);
                     }))
    {
        refuse(edit.where, "no creature in combat.rules casts at " + profile_text(to));
    }
    for (std::size_t n = 0; n < spell_profile_fields.size(); ++n)
    {
        const char *names[] = {"spell_slots", "casting_bonus", "level"};
        fields[spell_profile_fields[n]] = number_text(*number_member(to, names[n]));
    }
    files.rules.set_creature_fields(key, fields);
    files.rules.note(key, date, edit.reason);
    files.rules_changed = true;
    applied.result.status = EditStatus::applied;
    return applied;
}

Applied new_stat_line(Files &files, const Edit &edit, const std::string &date)
{
    const auto key = required_text(edit, "definition");
    const auto copy_of = required_text(edit, "copy_of");
    Applied applied{{edit.change, key, "a copy of " + copy_of, EditStatus::already_applied},
                    "new_stat_line " + key};
    const auto keys = files.rules.creatures();
    if (keys.contains(key))
    {
        return applied;
    }
    if (!keys.contains(copy_of))
    {
        refuse(edit.where, "combat.rules has no creature " + copy_of);
    }
    if (key.find_first_of(" \t#") != std::string::npos)
    {
        refuse(edit.where, "a stat line's key is one word");
    }
    files.rules.copy(copy_of, key);
    files.rules.note(key, date, edit.reason);
    files.rules_changed = true;
    applied.result.status = EditStatus::applied;
    return applied;
}

// Replaces, removes or adds the entry of `list` that `matches`, by the edit's
// `from` and `to` (each the entry as the file holds it, or null).
template <typename Matches> bool change_entry(const Edit &edit, Json::Array &list, Matches matches)
{
    const auto found = std::find_if(list.begin(), list.end(), matches);
    const Json current = found == list.end() ? Json{} : *found;
    const auto &from = required(edit, "from");
    const auto &to = required(edit, "to");
    if (!is_null(to) && !matches(to))
    {
        refuse(edit.where, "the edit's to is for another entry");
    }
    if (already_applied(edit, current, from, to))
    {
        return false;
    }
    if (is_null(to))
    {
        list.erase(found);
    }
    else if (found == list.end())
    {
        list.push_back(to);
    }
    else
    {
        *found = to;
    }
    return true;
}

std::string conversion_text(const Json &entry)
{
    if (is_null(entry))
    {
        return "none";
    }
    return text_member(entry, "definition") + ", rated " +
           number_text(number_member(entry, "fit_xp").value_or(0)) + " XP, awarding " +
           number_text(number_member(entry, "award_xp").value_or(0));
}

Applied conversion(Files &files, const Edit &edit, const std::string &)
{
    const auto script = required_number(edit, "script");
    const auto record = required_number(edit, "record");
    auto &areas = array_member(files.conversions, "areas", conversions_path.string());
    const auto area =
        std::find_if(areas.begin(), areas.end(),
                     [&](const Json &entry)
                     {
                         const auto *scripts = entry.find("scripts");
                         const auto *list =
                             scripts ? std::get_if<Json::Array>(&scripts->value) : nullptr;
                         return list && std::any_of(list->begin(), list->end(),
                                                    [&](const Json &s)
                                                    {
                                                        return detail::json_equal(s, Json{script});
                                                    });
                     });
    if (area == areas.end())
    {
        refuse(edit.where, "conversions.json has no area with script " + number_text(script));
    }
    const auto subject = text_member(*area, "name") + ", record " + number_text(record);
    Applied applied{
        {edit.change, subject,
         conversion_text(required(edit, "from")) + " to " + conversion_text(required(edit, "to")),
         EditStatus::already_applied},
        "conversion " + number_text(script) + ":" + number_text(record)};
    auto &creatures = array_member(*area, "creatures", conversions_path.string());
    if (change_entry(edit, creatures,
                     [&](const Json &entry)
                     {
                         return number_member(entry, "record") == record;
                     }))
    {
        files.conversions_changed = true;
        applied.result.status = EditStatus::applied;
    }
    return applied;
}

std::string override_text(const Json &entry)
{
    if (is_null(entry))
    {
        return "the original's";
    }
    const auto *original = entry.find("original");
    const auto count = number_member(entry, "count")
                           .value_or(original ? number_member(*original, "count").value_or(0) : 0);
    const auto record =
        number_member(entry, "record")
            .value_or(original ? number_member(*original, "record").value_or(0) : 0);
    return number_text(count) + " of record " + number_text(record);
}

Applied fight_override(Files &files, const Edit &edit, const std::string &)
{
    const auto site = required_text(edit, "site");
    Applied applied{{edit.change, site,
                     "loads " + override_text(required(edit, "from")) + " to " +
                         override_text(required(edit, "to")),
                     EditStatus::already_applied},
                    "override " + site};
    auto &overrides = array_member(files.overrides, "overrides", overrides_path.string());
    if (change_entry(edit, overrides,
                     [&](const Json &entry)
                     {
                         return text_member(entry, "site") == site;
                     }))
    {
        files.overrides_changed = true;
        applied.result.status = EditStatus::applied;
    }
    return applied;
}

std::string chance_text(const Json &entry)
{
    return is_null(entry) ? "the original's"
                          : number_text(number_member(entry, "value").value_or(0));
}

Applied chance(Files &files, const Edit &edit, const std::string &)
{
    const auto site = required_text(edit, "site");
    const auto operand = required_number(edit, "operand");
    Applied applied{{edit.change, site + " operand " + number_text(operand),
                     "chance number " + chance_text(required(edit, "from")) + " to " +
                         chance_text(required(edit, "to")),
                     EditStatus::already_applied},
                    "chance " + site + ":" + number_text(operand)};
    auto &chances = array_member(files.chances, "chances", chances_path.string());
    if (change_entry(edit, chances,
                     [&](const Json &entry)
                     {
                         return text_member(entry, "site") == site &&
                                number_member(entry, "operand") == operand;
                     }))
    {
        files.chances_changed = true;
        applied.result.status = EditStatus::applied;
    }
    return applied;
}

// The members whose values `from` and `to` differ in.
std::string changed_members(const Json &from, const Json &to)
{
    if (is_null(from))
    {
        return "added";
    }
    if (is_null(to))
    {
        return "removed";
    }
    std::string names;
    for (const auto &[key, value] : std::get<Json::Object>(to.value))
    {
        const auto *before = from.find(key);
        if (!before || !detail::json_equal(*before, value))
        {
            names += (names.empty() ? "" : ", ") + key;
        }
    }
    return names.empty() ? "unchanged" : names + " changed";
}

Applied companion(Files &files, const Edit &edit, const std::string &)
{
    const auto bank = required_number(edit, "bank");
    const auto record = required_number(edit, "record");
    const auto &from = required(edit, "from");
    const auto &to = required(edit, "to");
    const auto name = text_member(is_null(to) ? from : to, "name");
    Applied applied{{edit.change,
                     name.empty() ? "MON" + number_text(bank) + "CHA " + number_text(record) : name,
                     changed_members(from, to), EditStatus::already_applied},
                    "companion " + number_text(bank) + ":" + number_text(record)};
    auto &companions = array_member(files.companions, "companions", companions_path.string());
    if (change_entry(edit, companions,
                     [&](const Json &entry)
                     {
                         return number_member(entry, "bank") == bank &&
                                number_member(entry, "record") == record;
                     }))
    {
        files.companions_changed = true;
        applied.result.status = EditStatus::applied;
    }
    return applied;
}

Applied apply(Files &files, const Edit &edit, const std::string &date)
{
    using Change = Applied (*)(Files &, const Edit &, const std::string &);
    static const std::map<std::string, Change, std::less<>> changes{
        {"hit_points", hit_points},       {"spell_profile", spell_profile},
        {"new_stat_line", new_stat_line}, {"conversion", conversion},
        {"override", fight_override},     {"chance", chance},
        {"companion", companion}};
    const auto found = changes.find(edit.change);
    if (found == changes.end())
    {
        refuse(edit.where, "unknown change " + edit.change);
    }
    return found->second(files, edit, date);
}

// --- The rules version and docs/DEVIATIONS.md ----------------------------------

unsigned raise_rules_version(Files &files)
{
    const std::regex code(R"re(("opengold\.srd5", "0\.6\.)(\d+)("))re");
    const std::regex saves(R"re((`opengold\.srd5` 0\.6\.)(\d+))re");
    std::smatch in_code, in_saves;
    if (!std::regex_search(files.srd5, in_code, code) ||
        !std::regex_search(files.saves, in_saves, saves) || in_code[2] != in_saves[2])
    {
        throw ImportError(
            "The rules version is not where the importer looks for it: \"0.6.N\" in " +
            srd5_path.string() + " and `opengold.srd5` 0.6.N in " + saves_path.string());
    }
    const auto version = static_cast<unsigned>(std::stoul(in_code[2].str())) + 1;
    files.srd5.replace(static_cast<std::size_t>(in_code.position(2)),
                       static_cast<std::size_t>(in_code.length(2)), std::to_string(version));
    files.saves.replace(static_cast<std::size_t>(in_saves.position(2)),
                        static_cast<std::size_t>(in_saves.length(2)), std::to_string(version));
    return version;
}

// A list item wrapped to 79 columns, ending with the marker comment, which
// stays on one line so a later import finds it.
std::vector<std::string> entry_lines(const std::string &text, const std::string &marker)
{
    std::istringstream in(text);
    std::vector<std::string> words{std::istream_iterator<std::string>(in), {}};
    words.push_back("<!-- creature-edit " + marker + " -->");
    std::vector<std::string> lines{"-"};
    for (const auto &word : words)
    {
        if (lines.back().size() + 1 + word.size() > 79 && lines.back().size() > 2)
        {
            lines.emplace_back(" ");
        }
        lines.back() += " " + word;
    }
    return lines;
}

std::string sentence(std::string text)
{
    if (!text.empty() && text.back() != '.' && text.back() != '!' && text.back() != '?')
    {
        text += '.';
    }
    return text;
}

// Adds the edit's entry under "Rules and combat", or replaces the entry an
// earlier import wrote for the same subject.
void record_deviation(Files &files, const Applied &applied, const std::string &reason,
                      const std::string &date)
{
    std::vector<std::string> lines;
    {
        std::istringstream in(files.deviations);
        for (std::string line; std::getline(in, line);)
        {
            lines.push_back(line);
        }
    }
    const auto entry =
        entry_lines("**" + applied.result.subject + "** (" + date + "). " + sentence(reason) +
                        " Creature Stats Editor: " + sentence(applied.result.summary),
                    applied.marker);
    const auto marker = "<!-- creature-edit " + applied.marker + " -->";
    const auto marked = std::find_if(lines.begin(), lines.end(),
                                     [&](const std::string &line)
                                     {
                                         return line.find(marker) != std::string::npos;
                                     });
    if (marked != lines.end())
    {
        auto start = marked;
        while (start != lines.begin() && !start->starts_with("- "))
        {
            --start;
        }
        const auto at = lines.erase(start, std::next(marked));
        lines.insert(at, entry.begin(), entry.end());
    }
    else
    {
        const auto section = std::find(lines.begin(), lines.end(), "## Rules and combat");
        if (section == lines.end())
        {
            throw ImportError(deviations_path.string() + " has no \"## Rules and combat\"");
        }
        auto end = std::find_if(std::next(section), lines.end(),
                                [](const std::string &line)
                                {
                                    return line.starts_with("## ");
                                });
        while (end != std::next(section) && std::prev(end)->empty())
        {
            --end;
        }
        lines.insert(end, entry.begin(), entry.end());
    }
    files.deviations.clear();
    for (const auto &line : lines)
    {
        files.deviations += line + "\n";
    }
}

// --- Checking the result with the game's loaders ---------------------------------

// A folder of the edited encounter and companion files, removed afterwards.
struct Staging
{
    fs::path directory =
        fs::temp_directory_path() /
        ("opengold-import-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    explicit Staging(const Files &files)
    {
        fs::create_directories(directory / "encounters");
        fs::create_directories(directory / "companions");
        write_text(directory / "encounters/conversions.json",
                   detail::json_file_text(files.conversions));
        write_text(directory / "encounters/overrides.json",
                   detail::json_file_text(files.overrides));
        write_text(directory / "encounters/chances.json", detail::json_file_text(files.chances));
        write_text(directory / "companions/companions.json",
                   detail::json_file_text(files.companions));
    }

    Staging(const Staging &) = delete;
    Staging &operator=(const Staging &) = delete;

    ~Staging()
    {
        std::error_code ignored;
        fs::remove_all(directory, ignored);
    }
};

template <typename Check> void checked(const std::string &what, Check check)
{
    try
    {
        check();
    }
    catch (const ImportError &)
    {
        throw;
    }
    catch (const std::exception &error)
    {
        throw ImportError("The game would refuse the result (" + what + "): " + error.what());
    }
}

void check_result(const Files &files, const ImportOptions &options)
{
    const Staging staging(files);
    std::unique_ptr<rules::RulesModule> rules;
    checked("combat.rules",
            [&]
            {
                rules = srd5::parse_content(files.rules.text());
            });
    por::EncounterData encounters;
    checked("data/encounters",
            [&]
            {
                encounters = por::load_encounter_data(staging.directory / "encounters");
            });
    const auto keys = files.rules.creatures();
    for (const auto &area : encounters.areas)
    {
        for (const auto &[record, conversion] : area.creatures)
        {
            if (!keys.contains(conversion.definition))
            {
                throw ImportError("The game would refuse the result: " + area.name + ", record " +
                                  std::to_string(record) + " fights as " + conversion.definition +
                                  ", which combat.rules does not define");
            }
        }
    }
    checked("data/companions",
            [&]
            {
                const auto creation = srd5::character_rules();
                for (const auto &definition : load_companions(staging.directory / "companions"))
                {
                    (void)build_companion(definition, *creation, *rules);
                }
            });
    if (options.game_directory)
    {
        checked("the original's files",
                [&]
                {
                    (void)por::RolfTourSession::load(*options.game_directory, encounters);
                });
    }
}

// Each file the import changed, with its new text.
std::vector<std::pair<fs::path, std::string>> changed_files(const Files &files, const Files &before)
{
    std::vector<std::pair<fs::path, std::string>> changed;
    if (files.rules_changed)
    {
        changed.emplace_back(rules_path, files.rules.text());
        changed.emplace_back(srd5_path, files.srd5);
        changed.emplace_back(saves_path, files.saves);
    }
    if (files.conversions_changed)
    {
        changed.emplace_back(conversions_path, detail::json_file_text(files.conversions));
    }
    if (files.overrides_changed)
    {
        changed.emplace_back(overrides_path, detail::json_file_text(files.overrides));
    }
    if (files.chances_changed)
    {
        changed.emplace_back(chances_path, detail::json_file_text(files.chances));
    }
    if (files.companions_changed)
    {
        changed.emplace_back(companions_path, detail::json_file_text(files.companions));
    }
    if (files.deviations != before.deviations)
    {
        changed.emplace_back(deviations_path, files.deviations);
    }
    return changed;
}
} // namespace

ImportReport import_edits(const ImportOptions &options, std::string_view text,
                          const std::string &source)
{
    ImportReport report;
    const auto document = parse(std::string(text), source);
    if (text_member(document, "format") != "opengold-creature-edits-v1")
    {
        throw ImportError(source + ": format must be opengold-creature-edits-v1");
    }
    const auto *list = document.find("edits");
    const auto *edits = list ? std::get_if<Json::Array>(&list->value) : nullptr;
    if (!edits)
    {
        throw ImportError(source + ": no edits list");
    }
    const auto before = read_files(options.repository);
    auto files = before;
    for (std::size_t index = 0; index < edits->size(); ++index)
    {
        const auto &json = (*edits)[index];
        Edit edit{json, "edit " + std::to_string(index + 1), text_member(json, "change"),
                  text_member(json, "reason")};
        if (edit.change.empty())
        {
            refuse(edit.where, "missing change");
        }
        edit.where += " (" + edit.change + ")";
        if (edit.reason.empty())
        {
            refuse(edit.where, "every edit gives its reason (requirement 17)");
        }
        const auto applied = apply(files, edit, options.date);
        if (applied.result.status == EditStatus::applied)
        {
            record_deviation(files, applied, edit.reason, options.date);
        }
        report.edits.push_back(applied.result);
    }
    if (files.rules_changed)
    {
        report.rules_version = raise_rules_version(files);
    }
    check_result(files, options);
    const auto changed = changed_files(files, before);
    for (const auto &[path, after] : changed)
    {
        const auto file = options.repository / path;
        report.files.push_back(
            {path.generic_string(), fs::exists(file) ? read_text(file) : std::string(), after});
    }
    if (options.write)
    {
        for (const auto &[path, after] : changed)
        {
            write_text(options.repository / path, after);
        }
    }
    return report;
}

ImportReport import_edits(const ImportOptions &options)
{
    if (!fs::exists(options.edits_file))
    {
        return {};
    }
    return import_edits(options, read_text(options.edits_file), options.edits_file.string());
}

std::vector<std::string> stat_line_keys(const std::string &rules_text)
{
    const auto keys = RulesText(rules_text).creatures();
    return {keys.begin(), keys.end()};
}

namespace
{
std::vector<std::string> lines_of(const std::string &text)
{
    std::vector<std::string> lines;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);)
    {
        lines.push_back(line);
    }
    return lines;
}

// The middle that differs, as removed and added lines in order: a longest
// common subsequence, small because the files' shared start and end are
// left out first.
std::vector<std::pair<char, std::string>> changed_middle(std::span<const std::string> old_lines,
                                                         std::span<const std::string> new_lines)
{
    const auto rows = old_lines.size(), columns = new_lines.size();
    std::vector<std::vector<unsigned>> common(rows + 1, std::vector<unsigned>(columns + 1));
    for (std::size_t r = rows; r-- > 0;)
    {
        for (std::size_t c = columns; c-- > 0;)
        {
            common[r][c] = old_lines[r] == new_lines[c]
                               ? common[r + 1][c + 1] + 1
                               : std::max(common[r + 1][c], common[r][c + 1]);
        }
    }
    std::vector<std::pair<char, std::string>> lines;
    std::size_t r = 0, c = 0;
    while (r < rows || c < columns)
    {
        if (r < rows && c < columns && old_lines[r] == new_lines[c])
        {
            lines.emplace_back(' ', old_lines[r++]);
            ++c;
        }
        else if (r < rows && (c == columns || common[r + 1][c] >= common[r][c + 1]))
        {
            lines.emplace_back('-', old_lines[r++]);
        }
        else
        {
            lines.emplace_back('+', new_lines[c++]);
        }
    }
    return lines;
}
} // namespace

std::string file_diff(const std::string &before, const std::string &after)
{
    constexpr std::size_t context = 2;
    const auto old_lines = lines_of(before), new_lines = lines_of(after);
    std::size_t start = 0;
    while (start < old_lines.size() && start < new_lines.size() &&
           old_lines[start] == new_lines[start])
    {
        ++start;
    }
    std::size_t end = 0;
    while (end < old_lines.size() - start && end < new_lines.size() - start &&
           old_lines[old_lines.size() - 1 - end] == new_lines[new_lines.size() - 1 - end])
    {
        ++end;
    }
    const auto lines =
        changed_middle(std::span(old_lines).subspan(start, old_lines.size() - start - end),
                       std::span(new_lines).subspan(start, new_lines.size() - start - end));
    // Each change with its context; unchanged runs longer than that are cut.
    std::string out;
    std::size_t last_shown = 0;
    bool shown_any = false;
    // A line is numbered as in the file it is from: an added line as in the
    // new file, every other as in the old one.
    const auto show =
        [&](std::size_t old_index, std::size_t number, char mark, const std::string &line)
    {
        if (shown_any && old_index > last_shown + 1)
        {
            out += "...\n";
        }
        out += "@" + std::to_string(number + 1) + " " + mark + " " + line + "\n";
        last_shown = old_index;
        shown_any = true;
    };
    for (std::size_t n = start >= context ? start - context : 0; n < start; ++n)
    {
        show(n, n, ' ', old_lines[n]);
    }
    std::size_t old_number = start, new_number = start;
    for (std::size_t n = 0; n < lines.size(); ++n)
    {
        const auto &[mark, line] = lines[n];
        const bool near_change = std::any_of(
            lines.begin() + static_cast<std::ptrdiff_t>(n >= context ? n - context : 0),
            lines.begin() + static_cast<std::ptrdiff_t>(std::min(lines.size(), n + context + 1)),
            [](const auto &other)
            {
                return other.first != ' ';
            });
        if (mark != ' ' || near_change)
        {
            show(mark == '+' && old_number ? old_number - 1 : old_number,
                 mark == '+' ? new_number : old_number, mark, line);
        }
        old_number += mark == '+' ? 0 : 1;
        new_number += mark == '-' ? 0 : 1;
    }
    for (std::size_t n = old_lines.size() - end;
         n < std::min(old_lines.size(), old_lines.size() - end + context); ++n)
    {
        show(n, n, ' ', old_lines[n]);
    }
    return out;
}

std::string report_json(const ImportReport &report, ReportFiles detail)
{
    Json::Array edits;
    for (const auto &edit : report.edits)
    {
        edits.push_back(Json{Json::Object{
            {"change", Json{edit.change}},
            {"subject", Json{edit.subject}},
            {"status",
             Json{std::string(edit.status == EditStatus::applied ? "applied" : "already applied")}},
            {"summary", Json{edit.summary}}}});
    }
    Json::Array files;
    for (const auto &file : report.files)
    {
        // Not a ?: : GCC 16 takes the object's initializer list in a ?: branch
        // for a dangling pointer.
        if (detail == ReportFiles::paths)
        {
            files.push_back(Json{file.path});
            continue;
        }

        files.push_back(Json{Json::Object{{"path", Json{file.path}},
                                          {"diff", Json{file_diff(file.before, file.after)}}}});
    }
    return detail::json_file_text(Json{Json::Object{
        {"ok", Json{true}},
        {"edits", Json{std::move(edits)}},
        {"files_changed", Json{std::move(files)}},
        {"rules_version",
         report.rules_version ? Json{"0.6." + std::to_string(*report.rules_version)} : Json{}}}});
}
} // namespace opengold::edits
