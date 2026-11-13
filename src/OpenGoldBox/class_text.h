#ifndef OPENGOLDBOX_CLASS_TEXT_H
#define OPENGOLDBOX_CLASS_TEXT_H
#include "localization.h"
#include "opengold/character_rules.h"
#include "opengold/original.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace i18n
{
// A class's label by its stable ID ("rogue"), translated.
inline godot::String class_label(std::string_view id)
{
    // The SRD's classes, then the original rules' classes and multi-classes.
    static const auto choices = []
    {
        auto all = opengold::srd5::character_rules()->choices(
            opengold::rules::CreationField::character_class);
        for (auto &choice : opengold::original::character_rules()->choices(
                 opengold::rules::CreationField::character_class))
        {
            all.push_back(std::move(choice));
        }
        return all;
    }();
    for (const auto &choice : choices)
    {
        if (choice.id == id)
        {
            return text(choice.label);
        }
    }
    return godot::String::utf8(std::string(id).c_str());
}

// A character's classes as the sheet names them (MULTI-1): "Fighter 3 /
// Rogue 1" for several, in the order taken; the class's label alone for one.
inline godot::String class_summary(std::span<const std::string> level_classes,
                                   const godot::String &separator = " / ")
{
    std::vector<std::pair<std::string, int>> held;
    for (const auto &id : level_classes)
    {
        auto found = std::find_if(held.begin(), held.end(),
                                  [&](const auto &entry)
                                  {
                                      return entry.first == id;
                                  });
        if (found == held.end())
        {
            held.emplace_back(id, 1);
        }
        else
        {
            ++found->second;
        }
    }
    if (held.size() == 1)
    {
        return class_label(held.front().first);
    }
    godot::String result;
    for (const auto &[id, levels] : held)
    {
        if (!result.is_empty())
        {
            result += separator;
        }
        result += class_label(id) + " " + godot::String::num_int64(levels);
    }
    return result;
}

inline godot::String class_summary(const opengold::rules::CharacterSheet &sheet)
{
    return sheet.level_classes.empty() ? text(sheet.character_class)
                                       : class_summary(sheet.level_classes);
}
} // namespace i18n
#endif
