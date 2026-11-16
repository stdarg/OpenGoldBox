#pragma once

#include "opengold/character_creator.h"

#include <algorithm>
#include <functional>
#include <span>
#include <string_view>

namespace opengold::test
{

// Walks a character made under the original rules from its race page to its
// sheet: it is rolled on the attributes page, and on a page of choices
// (Treasures' magic-user's spells) each group takes the options naming a
// `preferred` word first, then the first offered, as a player fills it. With
// `keep`, the character is rolled again (up to 1,000 times) until it holds,
// as a player rolls again until the scores suit the class.
inline void
walk_original_creation(CharacterCreator &creator, std::span<const std::string_view> preferred = {},
                       const std::function<bool(const rules::CharacterSheet &)> &keep = {})
{
    while (creator.step() != CreationStep::sheet)
    {
        if (creator.step() == CreationStep::attributes)
        {
            creator.roll();
            for (int rolls = 1; keep && !keep(creator.sheet()) && rolls < 1000; ++rolls)
            {
                creator.roll();
            }
        }
        if (creator.step() == CreationStep::training)
        {
            for (const auto &group : creator.training_options())
            {
                auto options = group.options;
                std::ranges::stable_partition(options,
                                              [&](const rules::CreationChoice &option)
                                              {
                                                  return std::ranges::any_of(
                                                      preferred,
                                                      [&](std::string_view word)
                                                      {
                                                          return option.id.find(word) !=
                                                                 std::string::npos;
                                                      });
                                              });
                for (std::size_t n = 0; n < group.count && n < options.size(); ++n)
                {
                    creator.training_choice({group.id, options[n].id, true});
                }
            }
        }
        creator.next();
    }
}

} // namespace opengold::test
