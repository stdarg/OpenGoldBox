#include "opengold/buck_rogers_creation.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace opengold::original
{
namespace
{
using rules::CharacterDraft;
using rules::CreationChoice;
using rules::CreationField;

// Every career some race may take, in the menus' order: the human races'.
[[nodiscard]] std::span<const Profession> offered_careers()
{
    return buck_rogers_careers(Race::terran);
}

[[nodiscard]] std::optional<Gender> gender_named(std::string_view id)
{
    if (id == "male")
    {
        return Gender::male;
    }
    if (id == "female")
    {
        return Gender::female;
    }
    return std::nullopt;
}

// Whether a rolled record is still the one of the draft's choices.
[[nodiscard]] bool record_matches(const GroundRecord &record, const CharacterDraft &draft)
{
    const auto made = buck_rogers_draft(record);
    return made.race == draft.race && made.gender == draft.gender &&
           made.character_class == draft.character_class;
}
} // namespace

std::vector<CreationChoice> buck_rogers_creation_choices(CreationField field)
{
    std::vector<CreationChoice> listed;
    switch (field)
    {
    case CreationField::race:
        for (const auto race : buck_rogers_creation_races())
        {
            listed.push_back({std::string(buck_rogers_race_id(race)),
                              std::string(buck_rogers_race_name(race)),
                              {}});
        }
        break;
    case CreationField::gender:
        listed = {{"male", "Male", {}}, {"female", "Female", {}}};
        break;
    case CreationField::character_class:
        for (const auto career : offered_careers())
        {
            listed.push_back({std::string(buck_rogers_career_id(career)),
                              std::string(buck_rogers_career_name(career)),
                              {}});
        }
        break;
    case CreationField::alignment:
    case CreationField::background:
        break;
    }
    return listed;
}

std::optional<Race> buck_rogers_race_named(std::string_view id)
{
    for (const auto race : buck_rogers_creation_races())
    {
        if (buck_rogers_race_id(race) == id)
        {
            return race;
        }
    }
    return std::nullopt;
}

std::optional<Profession> buck_rogers_career_named(std::string_view id)
{
    for (const auto career : offered_careers())
    {
        if (buck_rogers_career_id(career) == id)
        {
            return career;
        }
    }
    return std::nullopt;
}

bool buck_rogers_creation_allows(const CharacterDraft &draft, CreationField field,
                                 std::string_view id, GoldBoxGame game)
{
    switch (field)
    {
    case CreationField::race:
        return buck_rogers_race_named(id).has_value();
    case CreationField::gender:
        return gender_named(id).has_value();
    case CreationField::character_class:
    {
        const auto race = buck_rogers_race_named(draft.race);
        const auto career = buck_rogers_career_named(id);
        if (!race || !career)
        {
            return false;
        }
        const auto careers = professions_for(*race, game);
        return std::ranges::find(careers, *career) != careers.end();
    }
    case CreationField::alignment:
    case CreationField::background:
        return false;
    }
    return false;
}

BuckRogersDraftChoices buck_rogers_draft_choices(const CharacterDraft &draft, GoldBoxGame game)
{
    const auto race = buck_rogers_race_named(draft.race);
    const auto gender = gender_named(draft.gender);
    const auto career = buck_rogers_career_named(draft.character_class);
    if (!race || !gender || !career ||
        !buck_rogers_creation_allows(draft, CreationField::character_class, draft.character_class,
                                     game))
    {
        throw std::invalid_argument("Creation does not offer that race, gender and career");
    }
    return {*race, *gender, *career};
}

void roll_buck_rogers_draft(CharacterDraft &draft, GoldBoxGame game, RandomState &random)
{
    const auto chosen = buck_rogers_draft_choices(draft, game);
    auto rolled = draft.rolled ? buck_rogers_record_of(draft.rules_data) : std::nullopt;
    if (rolled && record_matches(*rolled, draft))
    {
        auto &creature = rolled->creature;
        creature.skills = {};
        creature.specializations = {};
        reroll_buck_rogers_character(creature, game, random);
    }
    else
    {
        auto creature =
            new_buck_rogers_character(chosen.race, chosen.gender, chosen.career, game, random);
        creature.icon = default_icon(chosen.race, chosen.gender, chosen.career, random);
        rolled = GroundRecord{std::move(creature), {}, {}};
    }
    rolled->creature.name = draft.name;
    draft.rules_data = buck_rogers_draft(*rolled).rules_data;
    draft.rolled = true;
}

CharacterDraft named_buck_rogers_draft(const CharacterDraft &draft, std::string name)
{
    if (name.size() > buck_rogers::Creature::name_capacity)
    {
        throw std::invalid_argument("A Buck Rogers name holds at most 15 letters");
    }
    auto renamed = draft;
    if (auto record = buck_rogers_record_of(draft.rules_data))
    {
        record->creature.name = name;
        renamed.rules_data = buck_rogers_draft(*record).rules_data;
    }
    renamed.name = std::move(name);
    return renamed;
}

rules::InPlacePages buck_rogers_creation_pages(const CharacterDraft &draft, GoldBoxGame game)
{
    const auto career = buck_rogers_career_named(draft.character_class);
    const auto record = buck_rogers_record_of(draft.rules_data);
    const int chosen =
        record ? static_cast<int>(std::ranges::count_if(record->creature.specializations,
                                                        [](std::uint8_t type)
                                                        {
                                                            return type != 0;
                                                        }))
               : 0;
    return {false, false, true, career ? creation_specializations(*career, game) : 0, chosen};
}
} // namespace opengold::original
