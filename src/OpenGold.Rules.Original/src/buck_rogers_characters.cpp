#include "opengold/buck_rogers_characters.h"
#include "opengold/original.h"

#include <array>
#include <stdexcept>

namespace opengold::original
{
namespace
{
using rules::Ability;

constexpr std::array<std::string_view, 9> race_names{"Monster",       "Terran",    "Martian",
                                                     "Venusian",      "Mercurian", "Tinker",
                                                     "Desert Runner", "Lunarian",  "Lowlander"};
constexpr std::array<std::string_view, 9> race_ids{"monster",       "terran",    "martian",
                                                   "venusian",      "mercurian", "tinker",
                                                   "desert_runner", "lunarian",  "lowlander"};
constexpr std::array<std::string_view, 6> career_names{"Rocket Jock", "Medic", "Warrior",
                                                       "Engineer",    "Rogue", "Scout"};
constexpr std::array<std::string_view, 6> career_ids{"rocket_jock", "medic", "warrior",
                                                     "engineer",    "rogue", "scout"};

[[nodiscard]] std::size_t race_place(Race race)
{
    const auto place = static_cast<std::size_t>(race) - static_cast<std::size_t>(Race::monster);
    if (!buck_rogers_race(race) || place >= race_names.size())
    {
        throw std::invalid_argument("Not a Buck Rogers race");
    }
    return place;
}

[[nodiscard]] std::size_t career_place(Profession career)
{
    if (!buck_rogers_career(career))
    {
        throw std::invalid_argument("Not a Buck Rogers career");
    }
    return static_cast<std::size_t>(career) - static_cast<std::size_t>(Profession::rocket_jock);
}

[[nodiscard]] Race record_race_of(const buck_rogers::Creature &creature)
{
    return static_cast<Race>(static_cast<unsigned>(Race::monster) + creature.race);
}

// A creature's career, or a Warrior's for a monster with none (its record's 0).
[[nodiscard]] Profession record_career_of(const buck_rogers::Creature &creature)
{
    return career_from_record(creature.career).value_or(Profession::warrior);
}
} // namespace

std::string_view buck_rogers_race_name(Race race)
{
    return race_names[race_place(race)];
}

std::string_view buck_rogers_career_name(Profession career)
{
    return career_names[career_place(career)];
}

std::string_view buck_rogers_race_id(Race race)
{
    return race_ids[race_place(race)];
}

std::string_view buck_rogers_career_id(Profession career)
{
    return career_ids[career_place(career)];
}

bool holds_buck_rogers_record(std::string_view rules_data) noexcept
{
    return buck_rogers_record_of(rules_data).has_value();
}

std::optional<GroundRecord> buck_rogers_record_of(std::string_view rules_data)
{
    return decode_ground_record(rules_data);
}

rules::CharacterDraft buck_rogers_draft(const GroundRecord &record)
{
    const auto &creature = record.creature;
    rules::CharacterDraft draft;
    draft.name = creature.name;
    draft.race = std::string(buck_rogers_race_id(record_race_of(creature)));
    draft.gender = creature.gender ? "female" : "male";
    draft.character_class = std::string(buck_rogers_career_id(record_career_of(creature)));
    draft.rules_data = encode_ground_record(record);
    draft.rolled = true;
    return draft;
}

rules::CharacterSheet buck_rogers_sheet(const rules::CharacterDraft &draft,
                                        const GroundRecord &record, rules::Identity identity)
{
    const auto &creature = record.creature;
    rules::CharacterSheet sheet;
    sheet.identity = std::move(identity);
    sheet.name = draft.name.empty() ? creature.name : draft.name;
    const auto race = record_race_of(creature);
    const auto career = record_career_of(creature);
    sheet.race = std::string(buck_rogers_race_name(race));
    sheet.gender = creature.gender ? "Female" : "Male";
    sheet.character_class = std::string(buck_rogers_career_name(career));
    constexpr std::array<Ability, 6> abilities{Ability::strength,     Ability::dexterity,
                                               Ability::constitution, Ability::intelligence,
                                               Ability::wisdom,       Ability::charisma};
    for (std::size_t n = 0; n < abilities.size(); ++n)
    {
        sheet.base[abilities[n]] = creature.abilities[n];
        sheet.scores[abilities[n]] = creature.current_abilities[n];
    }
    sheet.level = std::max(creature.level, 1);
    sheet.hit_points = std::max<int>(creature.hit_point_maximum, 1);
    rules::CharacterSheet::Original numbers;
    numbers.age = creature.age;
    numbers.experience = static_cast<int>(creature.experience);
    numbers.race = static_cast<int>(race);
    numbers.profession = static_cast<int>(career);
    numbers.game_record = draft.rules_data;
    sheet.original = std::move(numbers);
    return sheet;
}
} // namespace opengold::original
