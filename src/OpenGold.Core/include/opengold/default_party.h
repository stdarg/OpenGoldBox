#ifndef OPENGOLD_DEFAULT_PARTY_H
#define OPENGOLD_DEFAULT_PARTY_H
#include "opengold/campaign_party.h"
#include "opengold/gold_box_game.h"
#include <filesystem>

namespace opengold
{
// The pre-rolled party each original game ships as its saved game A, for
// beginning to play at once (Pool of Radiance's Quick Reference Card, To Begin
// Playing Quickly): the characters SAVE/SAVGAMA.DAT names, CHRDATA1 to 6,
// read as the original's Load Game reads them (saveload.pas LoadGame).
//
// Adds them to `party` as player characters under the party's rules, each
// with its record's scores, levels, Hit Points, experience, known and
// memorized spells, portrait and combat icon, its items (.ITM, readied ones
// readied) and its money in the party's purse. Their effects (.SPC, .FX,
// .SFX) are not read. Curse's and Secret's saved game A hold no item files.
//
// Throws std::runtime_error, leaving the party as it was, when the folder's
// saved game A is missing or unreadable, or when the party's rules take no
// original record (the SRD's).
void add_default_party(CampaignParty &party, GoldBoxGame game,
                       const std::filesystem::path &game_folder);
} // namespace opengold
#endif
