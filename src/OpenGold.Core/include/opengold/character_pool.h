#ifndef OPENGOLD_CHARACTER_POOL_H
#define OPENGOLD_CHARACTER_POOL_H
#include "opengold/campaign_party.h"
#include "opengold/character.h"

namespace opengold
{
// Curated level-one drafts evaluated by the selected creation rules. Art is
// resolved from the user's loaded catalog; no original images are distributed.
[[nodiscard]] std::vector<Character> character_pool(const rules::CharacterRules &,
                                                    const por::CharacterArt &);

// Gives a member its class's kit for free: a held melee weapon, the armor and
// shield the class is trained with, a bow or crossbow (which needs no
// ammunition) and a torch in the pack. The combat demo and the simulator field ready
// parties with it; characters a player adds buy their own gear (START-1).
void outfit_pool_member(CampaignParty &party, MemberId member);

// Adds pool character `pool_index` to the party, recording where it came from.
// Like a newly created character in the original game it brings 3d6 x 10 gp,
// rolled from the party's random state, and no equipment. The changes are made
// all or nothing: if one fails the party is left as it was (Effective C++ Item 29).
MemberId add_pool_member(CampaignParty &party, const Character &character, unsigned pool_index);
} // namespace opengold
#endif
