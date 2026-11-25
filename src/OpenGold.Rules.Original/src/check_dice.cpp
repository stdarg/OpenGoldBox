#include "check_dice.h"

#include "rules_module.h"

#include <algorithm>

namespace opengold::original
{

using namespace rules;

// A thief's skill in percent, 0 for a character with no thief levels.
unsigned thief_chance(const CharacterSheet &sheet, ThiefSkill skill)
{
    return static_cast<unsigned>(
        std::max(skill_chance(original_numbers(sheet).thief_skills, skill), 0));
}

// The chance in percent to force a door open by Strength, on the original's
// open-doors table (explore.pas, BashDoor). The original's table has no row
// for Strength 19 and above, which never opened a door; it opens as 18/00
// does (docs/ORIGINAL-RULES.md, Bugs).
unsigned bash_chance(const CharacterSheet &sheet)
{
    const int strength = sheet.scores[Ability::strength];
    const int exceptional = original_numbers(sheet).exceptional_strength;
    const int sixths = strength <= 7        ? 1
                       : strength <= 15     ? 2
                       : strength <= 17     ? 3
                       : strength > 18      ? 5
                       : exceptional == 100 ? 5
                       : exceptional > 50   ? 4
                                            : 3;
    return static_cast<unsigned>(sixths * 100 / 6);
}

// The chance a game's check stands for: picking a lock (Sleight of Hand) by
// Open Locks, climbing (Athletics with Dexterity) by Climb Walls, forcing a
// door (Athletics with Strength) by Strength; nothing else succeeds.
unsigned original_chance(const CharacterSheet &sheet, Ability ability, std::string_view skill)
{
    if (skill == "sleight_of_hand")
    {
        return thief_chance(sheet, ThiefSkill::open_locks);
    }
    if (skill == "athletics")
    {
        return ability == Ability::strength ? bash_chance(sheet)
                                            : thief_chance(sheet, ThiefSkill::climb_walls);
    }
    return 0;
}

// Krynn's BashDoor uses separate exact dice for ordinary and magical
// barriers. Exceptional Strength 18/91 is the first that can force magic.
std::pair<unsigned, unsigned> krynn_door_die(const CharacterSheet &sheet, bool magical)
{
    const int strength = sheet.scores[Ability::strength];
    const int exceptional = original_numbers(sheet).exceptional_strength;
    if (magical)
    {
        if (strength < 18 || (strength == 18 && exceptional < 91))
        {
            return {0, 6};
        }
        if (strength == 18)
        {
            return {exceptional == 100 ? 2U : 1U, 6};
        }
        if (strength <= 20)
        {
            return {3, 6};
        }
        if (strength <= 22)
        {
            return {4, 6};
        }
        if (strength == 23)
        {
            return {5, 6};
        }
        if (strength == 24)
        {
            return {7, 8};
        }
        return {1, 1};
    }
    if (strength <= 7)
    {
        return {1, 6};
    }
    if (strength <= 15)
    {
        return {2, 6};
    }
    if (strength <= 17)
    {
        return {3, 6};
    }
    if (strength == 18)
    {
        return {exceptional == 100 ? 5U : exceptional > 50 ? 4U : 3U, 6};
    }
    if (strength <= 20)
    {
        return {7, 8};
    }
    if (strength == 21)
    {
        return {9, 10};
    }
    if (strength <= 23)
    {
        return {11, 12};
    }
    if (strength == 24)
    {
        return {19, 20};
    }
    return {1, 1};
}
} // namespace opengold::original
