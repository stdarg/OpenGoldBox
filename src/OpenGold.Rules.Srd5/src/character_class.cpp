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
constexpr std::array<ClassName, 12> class_names{
    {{CharacterClass::barbarian, "barbarian", "Barbarian"},
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
     {CharacterClass::wizard, "wizard", "Wizard"}}};

// Rows are indexed by CharacterClass, so each must sit at its class's position.
static_assert(
    []
    {
        for (std::size_t row = 0; row < class_names.size(); ++row)
        {
            if (static_cast<std::size_t>(class_names[row].character_class) != row)
            {
                return false;
            }
        }
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
                                    [&](const ClassName &names)
                                    {
                                        return names.label == label;
                                    });
    if (found == class_names.end())
    {
        return std::nullopt;
    }
    return found->character_class;
}

CharacterClass class_from_id(std::string_view id)
{
    const auto found = std::find_if(class_names.begin(), class_names.end(),
                                    [&](const ClassName &names)
                                    {
                                        return names.id == id;
                                    });
    if (found == class_names.end())
    {
        throw std::runtime_error("Unknown class");
    }
    return found->character_class;
}

std::optional<CharacterClass> draft_class(const rules::CharacterDraft &draft)
{
    if (draft.character_class.empty())
    {
        return std::nullopt;
    }
    return class_from_id(draft.character_class);
}

CharacterClass class_of(const rules::CharacterSheet &sheet)
{
    const auto character_class = class_from_label(sheet.character_class);
    if (!character_class)
    {
        throw std::runtime_error("Unknown class");
    }
    return *character_class;
}

ClassLevels::ClassLevels(std::span<const std::string> level_classes)
{
    for (const auto &id : level_classes)
    {
        add(class_from_id(id));
    }
}

int ClassLevels::total() const noexcept
{
    int sum = 0;
    for (const auto levels : levels_)
    {
        sum += levels;
    }
    return sum;
}

std::size_t ClassLevels::count() const noexcept
{
    return static_cast<std::size_t>(
        std::count_if(levels_.begin(), levels_.end(),
                      [](int levels)
                      {
                          return levels > 0;
                      }));
}

ClassHistory::ClassHistory(std::span<const std::string> level_classes)
{
    if (level_classes.empty())
    {
        throw std::runtime_error("A character has at least one level");
    }
    for (const auto &id : level_classes)
    {
        classes_.push_back(class_from_id(id));
        levels_.add(classes_.back());
    }
}

CharacterClass ClassHistory::taken_at(unsigned level) const
{
    if (level < 1 || level > classes_.size())
    {
        throw std::runtime_error("A level the character has not reached");
    }
    return classes_[level - 1];
}

unsigned ClassHistory::class_level_at(unsigned level) const
{
    const auto klass = taken_at(level);
    return static_cast<unsigned>(std::count(classes_.begin(), classes_.begin() + level, klass));
}

unsigned ClassHistory::reached(CharacterClass klass, unsigned class_level) const noexcept
{
    unsigned held = 0;
    for (std::size_t at = 0; at < classes_.size(); ++at)
    {
        if (classes_[at] == klass && ++held == class_level)
        {
            return static_cast<unsigned>(at + 1);
        }
    }
    return 0;
}

unsigned ClassHistory::class_level_by(CharacterClass klass, unsigned level) const noexcept
{
    const auto end = classes_.begin() + std::min<std::size_t>(level, classes_.size());
    return static_cast<unsigned>(std::count(classes_.begin(), end, klass));
}

ClassLevels class_levels(const rules::CharacterSheet &sheet)
{
    return ClassLevels(level_class_ids(sheet));
}

std::vector<std::string> level_class_ids(const rules::CharacterSheet &sheet)
{
    const auto level = static_cast<std::size_t>(std::max(sheet.level, 0));
    if (sheet.level_classes.size() == level)
    {
        return sheet.level_classes;
    }
    // A sheet whose levels all name its starting class has them all in it,
    // however many it records; a multiclass sheet must record each (MULTI-1).
    const auto start = std::string(class_id(class_of(sheet)));
    if (std::any_of(sheet.level_classes.begin(), sheet.level_classes.end(),
                    [&](const std::string &id)
                    {
                        return id != start;
                    }))
    {
        throw std::runtime_error("A multiclass sheet records the class of each level");
    }
    return std::vector<std::string>(level, start);
}

int class_hit_die(CharacterClass character_class) noexcept
{
    switch (character_class)
    {
    case CharacterClass::barbarian:
        return 12;
    case CharacterClass::fighter:
    case CharacterClass::paladin:
    case CharacterClass::ranger:
        return 10;
    case CharacterClass::sorcerer:
    case CharacterClass::wizard:
        return 6;
    default:
        return 8;
    }
}

unsigned level_cap(CharacterClass character_class)
{
    return character_class == CharacterClass::fighter ||
                   character_class == CharacterClass::barbarian ||
                   character_class == CharacterClass::monk ||
                   character_class == CharacterClass::rogue ||
                   character_class == CharacterClass::paladin ||
                   character_class == CharacterClass::ranger ||
                   character_class == CharacterClass::cleric ||
                   character_class == CharacterClass::wizard ||
                   character_class == CharacterClass::sorcerer ||
                   character_class == CharacterClass::warlock ||
                   character_class == CharacterClass::bard ||
                   character_class == CharacterClass::druid
               ? 15
               : 4;
}

bool gains_feat(CharacterClass character_class, unsigned level)
{
    return level == 4 || level == 8 || level == 12 || level == 16 || level == 19 ||
           (character_class == CharacterClass::fighter && (level == 6 || level == 14)) ||
           (character_class == CharacterClass::rogue && level == 10);
}

unsigned feats_by(CharacterClass character_class, unsigned level)
{
    unsigned count = 0;
    for (unsigned n = 1; n <= level; ++n)
    {
        count += gains_feat(character_class, n) ? 1 : 0;
    }
    return count;
}
} // namespace opengold::srd5::detail
