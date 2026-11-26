#ifndef OPENGOLDBOX_CHARACTER_SHEET_TEXT_H
#define OPENGOLDBOX_CHARACTER_SHEET_TEXT_H
#include "opengold/campaign_party.h"
#include <godot_cpp/variant/string.hpp>

namespace settings
{
class Settings;
} // namespace settings

// A character sheet as BBCode. Shared by the creation view, the party panel,
// the character pool and the town's member sheet, so none of them needs
// another's class to show one (Effective C++ Item 31). The settings name the
// game whose files a Buck Rogers character's sheet reads.
namespace presentation
{
// A created character on its own.
[[nodiscard]] godot::String sheet_text(const settings::Settings &settings,
                                       const opengold::Character &character);
// A party member, with what the party adds: current HP, gold, XP, AC, where
// each item is held and whether the member is ready to level up.
[[nodiscard]] godot::String sheet_text(const settings::Settings &settings,
                                       const opengold::CampaignParty &party,
                                       const opengold::PartyMember &member);

// A party member's sheet in the town's two parts: the name, hit points and
// attributes beside the portrait, then the rest of the sheet below it.
struct SheetSections
{
    godot::String beside_portrait;
    godot::String below_portrait;
};

[[nodiscard]] SheetSections sheet_sections(const settings::Settings &settings,
                                           const opengold::CampaignParty &party,
                                           const opengold::PartyMember &member);
} // namespace presentation
#endif
