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

// Gives a pool member its class's free starting kit: a held melee weapon, the
// armor and shield the class is trained with, and a bow or crossbow with 20
// arrows or bolts carried in the pack.
void outfit_pool_member(CampaignParty &party, MemberId member);

// Adds pool character `pool_index` to the party with 250 gp and its starting
// kit, recording where it came from. That is a dozen party changes, made all
// or nothing: if one fails the party is left as it was (Effective C++ Item 29).
MemberId add_pool_member(CampaignParty &party, const Character &character, unsigned pool_index);
} // namespace opengold
#endif
