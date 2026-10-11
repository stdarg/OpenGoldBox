#ifndef OPENGOLDBOX_CHARACTER_SHEET_TEXT_H
#define OPENGOLDBOX_CHARACTER_SHEET_TEXT_H
#include "opengold/campaign_party.h"
#include <godot_cpp/variant/string.hpp>

// A character sheet as BBCode. Shared by the creation view, the party panel,
// the character pool and the town's member sheet, so none of them needs
// another's class to show one (Effective C++ Item 31).
namespace presentation
{
// A created character on its own.
[[nodiscard]] godot::String sheet_text(const opengold::Character &character);
// A party member, with what the party adds: current HP, gold, XP, AC, where
// each item is held and whether the member is ready to level up.
[[nodiscard]] godot::String sheet_text(const opengold::CampaignParty &party,
                                       const opengold::PartyMember &member);
} // namespace presentation
#endif
