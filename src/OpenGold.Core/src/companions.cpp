#include "opengold/companions.h"
#include "json_document.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace opengold
{
namespace
{
// A 4d6-drop-lowest roll for `score` (3 to 18): three kept dice and a 1.
rules::AbilityRoll roll_of(int score)
{
    const int high = std::min(6, score - 2);
    const int middle = std::min(6, score - high - 1);
    return {{high, middle, score - high - middle, 1}, 3};
}

// Each training group's first offered choices, refreshed after each group so
// a later group sees the choices before it, as the character pool chooses.
void choose_training(const rules::CharacterRules &creation, rules::CharacterDraft &draft)
{
    for (const auto &requested : creation.training_options(draft))
    {
        const auto current = creation.training_options(draft);
        const auto group = std::find_if(current.begin(), current.end(),
                                        [&](const auto &g)
                                        {
                                            return g.id == requested.id;
                                        });
        if (group == current.end() || group->options.size() < group->count)
        {
            throw std::runtime_error("Incomplete companion training options");
        }
        for (unsigned n = 0; n < group->count; ++n)
        {
            draft.training[group->id].push_back(group->options[n].id);
        }
    }
}

// The listed spells each of the draft's spell choices offers, in the list's
// order: its cantrips, what each group teaches (a feat's too) and the spells
// it prepares; a choice the list leaves short takes its first other options,
// as the character pool's do. A spell is learned once, though two groups may
// offer it.
void choose_spells(const rules::CharacterRules &creation, rules::CharacterDraft &draft,
                   const std::vector<std::string> &spells)
{
    std::set<std::string> learned;
    const auto picks = [&](const rules::TrainingChoiceGroup &group, bool learning)
    {
        std::vector<std::string> picked;
        const auto open = [&](const std::string &id)
        {
            return std::ranges::find(picked, id) == picked.end() &&
                   (!learning || !learned.contains(id));
        };
        const auto offered = [&](const std::string &id)
        {
            return std::ranges::any_of(group.options,
                                       [&](const auto &option)
                                       {
                                           return option.id == id;
                                       });
        };
        for (const auto &spell : spells)
        {
            if (offered(spell) && open(spell) && picked.size() < group.count)
            {
                picked.push_back(spell);
            }
        }
        for (const auto &option : group.options)
        {
            if (picked.size() < group.count && open(option.id))
            {
                picked.push_back(option.id);
            }
        }
        if (learning)
        {
            learned.insert(picked.begin(), picked.end());
        }
        return picked;
    };
    if (const auto cantrips = creation.cantrip_options(draft); cantrips.count)
    {
        draft.cantrips = picks(cantrips, true);
    }
    const auto options = creation.spell_choice_options(draft);
    if (options.learning.empty() && !options.may_prepare)
    {
        return;
    }
    draft.spells = rules::SpellChoices{};
    for (const auto &group : options.learning)
    {
        draft.spells->learning[group.id] = picks(group, true);
    }
    if (options.may_prepare)
    {
        // What it learned is offered now.
        const auto prepared = creation.spell_choice_options(draft);
        draft.spells->prepared = picks(
            {"prepared", "prepared spells", prepared.prepared_count, prepared.preparation}, false);
    }
}

// The background's adjustment of +2 to `raised` and +1 to `lifted`, by its index.
unsigned adjustment(const rules::CharacterRules &creation, const std::string &background,
                    rules::Ability raised, rules::Ability lifted)
{
    const auto adjustments = creation.adjustments(background);
    const auto found = std::find_if(adjustments.begin(), adjustments.end(),
                                    [&](const auto &a)
                                    {
                                        return a.bonuses[raised] == 2 && a.bonuses[lifted] == 1;
                                    });
    if (found == adjustments.end())
    {
        throw std::runtime_error("No such adjustment for a companion's background");
    }
    return static_cast<unsigned>(found - adjustments.begin());
}

// `draft` at `level`, each level after the first with the rules' default
// choices for the level it reaches.
Character advanced(const rules::CharacterRules &creation, const rules::RulesModule &rules,
                   const rules::CharacterDraft &draft, int level)
{
    std::vector<rules::AdvancementChoice> history;
    Character companion(creation, draft, {});
    while (companion.sheet().level < level)
    {
        const auto reached = companion.sheet().level;
        history.push_back(rules.default_advancement(companion.sheet()));
        companion = Character(creation, draft, {});
        companion.replay(rules, history, {}, {}, {});
        if (companion.sheet().level <= reached)
        {
            throw std::runtime_error(draft.name + " cannot reach level " + std::to_string(level));
        }
    }
    return companion;
}

using detail::Json;

[[noreturn]] void reject(const std::string &where, std::string_view problem)
{
    throw std::runtime_error("data/companions/companions.json: " + where + ": " +
                             std::string(problem));
}

template <typename T>
const T &member(const Json &object, std::string_view key, const std::string &where)
{
    const auto *found = object.find(key);
    const auto *value = found ? std::get_if<T>(&found->value) : nullptr;
    if (!value)
    {
        reject(where, "missing or invalid " + std::string(key));
    }
    return *value;
}

const std::string &text(const Json &object, std::string_view key, const std::string &where)
{
    const auto &value = member<std::string>(object, key, where);
    if (value.empty())
    {
        reject(where, "empty " + std::string(key));
    }
    return value;
}

// A whole number in [low, high].
int whole(const Json &value, const std::string &where, std::string_view key, int low, int high)
{
    const auto *number = std::get_if<double>(&value.value);
    if (!number || *number != std::floor(*number) || *number < low || *number > high)
    {
        reject(where, std::string(key) + " must be a whole number from " + std::to_string(low) +
                          " to " + std::to_string(high));
    }
    return static_cast<int>(*number);
}

int whole_member(const Json &object, std::string_view key, const std::string &where, int low,
                 int high)
{
    const auto *found = object.find(key);
    if (!found)
    {
        reject(where, "missing " + std::string(key));
    }
    return whole(*found, where, key, low, high);
}

rules::Ability ability(const Json &object, std::string_view key, const std::string &where)
{
    constexpr std::array<std::string_view, 6> names{"strength",     "dexterity", "constitution",
                                                    "intelligence", "wisdom",    "charisma"};
    const auto &name = member<std::string>(object, key, where);
    const auto found = std::find(names.begin(), names.end(), name);
    if (found == names.end())
    {
        reject(where, std::string(key) + " names no ability: " + name);
    }
    return rules::all_abilities[static_cast<std::size_t>(found - names.begin())];
}

NewItem read_item(const Json &item, const std::string &where)
{
    NewItem result{text(item, "key", where), text(item, "name", where)};
    const auto item_where = where + ", item " + result.definition_id;
    if (item.find("quantity"))
    {
        result.quantity =
            static_cast<std::uint32_t>(whole_member(item, "quantity", item_where, 1, 255));
    }
    if (item.find("original_type"))
    {
        result.original_type = whole_member(item, "original_type", item_where, 0, 255);
    }
    return result;
}

CompanionDefinition read_companion(const Json &entry, std::size_t index)
{
    auto where = "companion " + std::to_string(index + 1);
    CompanionDefinition companion;
    companion.name = text(entry, "name", where);
    where += " (" + companion.name + ")";
    companion.bank = static_cast<unsigned>(whole_member(entry, "bank", where, 1, 255));
    companion.record = static_cast<unsigned>(whole_member(entry, "record", where, 0, 255));
    companion.race = text(entry, "race", where);
    companion.gender = text(entry, "gender", where);
    companion.character_class = text(entry, "class", where);
    companion.alignment = text(entry, "alignment", where);
    companion.background = text(entry, "background", where);
    const auto &rolled = member<Json::Array>(entry, "rolled", where);
    if (rolled.size() != companion.rolled.size())
    {
        reject(where, "rolled needs six scores");
    }
    for (std::size_t n = 0; n < rolled.size(); ++n)
    {
        companion.rolled[n] = whole(rolled[n], where, "rolled", 3, 18);
    }
    const auto *increase = entry.find("background_increase");
    if (!increase || !std::holds_alternative<Json::Object>(increase->value))
    {
        reject(where, "missing or invalid background_increase");
    }
    companion.plus_two = ability(*increase, "plus_two", where);
    companion.plus_one = ability(*increase, "plus_one", where);
    if (companion.plus_two == companion.plus_one)
    {
        reject(where, "the background raises two different abilities");
    }
    // The SRD classes' highest level (character_class.cpp).
    constexpr int highest_level = 15;
    companion.level = whole_member(entry, "level", where, 1, highest_level);
    for (const auto &item : member<Json::Array>(entry, "items", where))
    {
        companion.items.push_back(read_item(item, where));
    }
    return companion;
}
} // namespace

std::vector<CompanionDefinition> load_companions(const std::filesystem::path &folder)
{
    const auto document = detail::read_json_file(folder / "companions.json", "companion data");
    const auto *format = document.find("format");
    if (!format || !std::holds_alternative<std::string>(format->value) ||
        std::get<std::string>(format->value) != "opengold-companions-v1")
    {
        reject("format", "must be opengold-companions-v1");
    }
    std::vector<CompanionDefinition> companions;
    std::set<std::pair<unsigned, unsigned>> records;
    // A named place, not a temporary: GCC 16 would take the reference into
    // document for one into the temporary string.
    const std::string file_where = "file";
    const auto &entries = member<Json::Array>(document, "companions", file_where);
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        auto companion = read_companion(entries[index], index);
        if (!records.insert({companion.bank, companion.record}).second)
        {
            reject(companion.name, "MON" + std::to_string(companion.bank) + "CHA " +
                                       std::to_string(companion.record) + " has two companions");
        }
        companions.push_back(std::move(companion));
    }
    return companions;
}

Character build_companion(const CompanionDefinition &definition,
                          const rules::CharacterRules &creation, const rules::RulesModule &rules)
{
    rules::CharacterDraft draft;
    draft.name = definition.name;
    draft.race = definition.race;
    draft.gender = definition.gender;
    draft.character_class = definition.character_class;
    draft.alignment = definition.alignment;
    draft.background = definition.background;
    draft.rolled = true;
    for (unsigned ability = 0; ability < definition.rolled.size(); ++ability)
    {
        draft.rolls[ability] = roll_of(definition.rolled[ability]);
    }
    draft.adjustment =
        adjustment(creation, draft.background, definition.plus_two, definition.plus_one);
    choose_training(creation, draft);
    if (!definition.spells.empty())
    {
        choose_spells(creation, draft, definition.spells);
    }
    auto companion = advanced(creation, rules, draft, definition.level);
    for (const auto &item : definition.items)
    {
        companion.add_item(item);
    }
    return companion;
}

std::optional<Character> original_companion(unsigned bank, unsigned record,
                                            std::span<const CompanionDefinition> companions,
                                            const rules::CharacterRules &creation,
                                            const rules::RulesModule &rules)
{
    const auto found = std::find_if(companions.begin(), companions.end(),
                                    [&](const CompanionDefinition &companion)
                                    {
                                        return companion.bank == bank && companion.record == record;
                                    });
    if (found == companions.end())
    {
        return std::nullopt;
    }
    return build_companion(*found, creation, rules);
}
} // namespace opengold
