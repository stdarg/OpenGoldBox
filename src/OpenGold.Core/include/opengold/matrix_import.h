#ifndef OPENGOLD_MATRIX_IMPORT_H
#define OPENGOLD_MATRIX_IMPORT_H

#include "opengold/campaign_party.h"
#include "opengold/original_content.h"

#include <string_view>

namespace opengold
{

// How Matrix Cubed takes in a Countdown to Doomsday character, its Add
// Character's Countdown (docs/BUCK-ROGERS.md, Matrix Cubed brings in
// Countdown characters; saveload.pas LoadChar): nothing is converted. Its
// whole record comes as it was, its level, experience, credits, skills,
// items, second item list and effects, and its items' types are read from
// Matrix Cubed's ITEMS (`matrix_stock`), as that game's program looks them
// up by number. A permanent NPC of a winning party joins in an NPC position
// with its morale, its source `from_game`'s (CampaignParty::bring_npc_across).
// Throws for a member without a Buck Rogers record.
MemberId bring_countdown_member_into_matrix(CampaignParty &party, const PartyMember &from,
                                            const BuckRogersStock &matrix_stock,
                                            std::string_view from_game);

} // namespace opengold

#endif
