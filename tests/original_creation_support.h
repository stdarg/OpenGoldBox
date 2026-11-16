#pragma once

#include "opengold/campaign_party.h"
#include "opengold/character_creator.h"

#include <algorithm>
#include <span>
#include <string_view>

namespace opengold::test
{

// Walks a character made under the original rules from its race page to its
// sheet: it is rolled on the attributes page, and on a page of choices
// (Treasures' magic-user's spells) each group takes the options naming a
// `preferred` word first, then the first offered, as a player fills it.
inline void walk_original_creation(CharacterCreator &creator,
                                   std::span<const std::string_view> preferred = {})
{
    while (creator.step() != CreationStep::sheet)
    {
        if (creator.step() == CreationStep::attributes)
        {
            creator.roll();
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

// A new member made as a player makes a party meant to finish the game, as the
// cluebooks assume: on the party screen's Modify Character
// (docs/ORIGINAL-RULES.md, Modify Character) each score is raised to its
// race's and class's highest and the Hit Points to the game's highest.
inline void modify_to_the_highest(CampaignParty &party, MemberId id)
{
    using rules::ModifyRow;
    const auto &creation = *party.character_rules();
    auto draft = party.member(id).character.creation_data();
    // Constitution first, since the Hit Points' highest rises with it.
    for (const auto row :
         {ModifyRow::constitution, ModifyRow::strength, ModifyRow::dexterity,
          ModifyRow::intelligence, ModifyRow::wisdom, ModifyRow::charisma, ModifyRow::hit_points})
    {
        for (;;)
        {
            auto raised = creation.modified(draft, row, rules::ModifyStep::raise);
            if (raised.rules_data == draft.rules_data)
            {
                break;
            }
            draft = std::move(raised);
        }
    }
    party.modify(id, std::move(draft));
}

} // namespace opengold::test
