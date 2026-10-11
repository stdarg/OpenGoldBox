#include "character_class.h"
#include "opengold/character_rules.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace opengold::srd5::detail
{
namespace
{
struct ClassName
{
    CharacterClass character_class;
    std::string_view id, label;
};

// The one place a class's ID and label are spelt.
constexpr std::array<ClassName, 12> class_names{{
        {CharacterClass::barbarian, "barbarian", "Barbarian"},
        {CharacterClass::bard, "bard", "Bard"},
        {CharacterClass::cleric, "cleric", "Cleric"},
        {CharacterClass::druid, "druid", "Druid"},
        {CharacterClass::fighter, "fighter", "Fighter"},
        {CharacterClass::monk, "monk", "Monk"},
        {CharacterClass::paladin, "paladin", "Paladin"},
        {CharacterClass::ranger, "ranger", "Ranger"},
        {CharacterClass::rogue, "rogue", "Rogue"},
        {CharacterClass::sorcerer, "sorcerer", "Sorcerer"},
        {CharacterClass::warlock, "warlock", "Warlock"},
        {CharacterClass::wizard, "wizard", "Wizard"}
    }};

// Rows are indexed by CharacterClass, so each must sit at its class's position.
static_assert([]
{
    for (std::size_t row = 0; row < class_names.size(); ++row)
        if (static_cast<std::size_t>(class_names[row].character_class) != row)
            return false;
    return true;
}());

const ClassName &names_of(CharacterClass character_class)
{
    return class_names.at(static_cast<std::size_t>(character_class));
}
} // namespace

std::string_view class_id(CharacterClass character_class)
{
    return names_of(character_class).id;
}

std::string_view class_label(CharacterClass character_class)
{
    return names_of(character_class).label;
}

std::optional<CharacterClass> class_from_label(std::string_view label)
{
    const auto found = std::find_if(class_names.begin(), class_names.end(),
                                    [&](const ClassName & names)
    {
        return names.label == label;
    });
    if (found == class_names.end())
        return std::nullopt;
    return found->character_class;
}

CharacterClass class_from_id(std::string_view id)
{
    const auto found = std::find_if(class_names.begin(), class_names.end(),
                                    [&](const ClassName & names)
    {
        return names.id == id;
    });
    if (found == class_names.end())
        throw std::runtime_error("Unknown class");
    return found->character_class;
}

std::optional<CharacterClass> draft_class(const rules::CharacterDraft &draft)
{
    if (draft.character_class.empty())
        return std::nullopt;
    return class_from_id(draft.character_class);
}

CharacterClass class_of(const rules::CharacterSheet &sheet)
{
    const auto character_class = class_from_label(sheet.character_class);
    if (!character_class)
        throw std::runtime_error("Unknown class");
    return *character_class;
}
} // namespace opengold::srd5::detail
