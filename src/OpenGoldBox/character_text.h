#ifndef OPENGOLDBOX_CHARACTER_TEXT_H
#define OPENGOLDBOX_CHARACTER_TEXT_H
#include "localization.h"
#include "opengold/character_rules.h"
#include <array>

namespace i18n
{
inline constexpr std::array<const char *, 6> ability_names{N_("Strength"),     N_("Dexterity"),
    N_("Constitution"), N_("Intelligence"),
    N_("Wisdom"),       N_("Charisma")};
inline constexpr std::array<const char *, 6> ability_short{N_("STR"), N_("DEX"), N_("CON"),
    N_("INT"), N_("WIS"), N_("CHA")};

inline godot::String requirements(const opengold::rules::ClassRequirements &requirement)
{
    godot::String result;
    for (const auto ability : requirement.abilities)
    {
        if (!result.is_empty())
            result += requirement.any ? i18n::text(" or ") : i18n::text(" and ");
        const auto name = text(ability_short.at(opengold::rules::ability_index(ability)));
        result += i18n::format("{ability} {minimum}", {{"ability", name},
            {"minimum", requirement.minimum}
        });
    }
    return result;
}

inline godot::String adjustment(const opengold::rules::ScoreAdjustment &adjustment)
{
    godot::String result;
    for (unsigned i = 0; i < 6; ++i)
        if (adjustment.bonuses[i])
        {
            if (!result.is_empty())
                result += ", ";
            result += i18n::format("{ability} +{bonus}", {{"ability", text(ability_short[i])},
                {"bonus", adjustment.bonuses[i]}
            });
        }
    return result;
}
} // namespace i18n
#endif
