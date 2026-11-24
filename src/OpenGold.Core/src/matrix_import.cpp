#include "opengold/matrix_import.h"

#include "opengold/buck_rogers_characters.h"

#include <stdexcept>
#include <string>

namespace opengold
{

MemberId bring_countdown_member_into_matrix(CampaignParty &party, const PartyMember &from,
                                            const BuckRogersStock &matrix_stock,
                                            std::string_view from_game)
{
    // The member's record as it stands now, which its creation draft is not.
    auto record = original::buck_rogers_record_of(from.vitals.resources);
    if (!record)
    {
        throw std::invalid_argument("Matrix Cubed takes in only a Countdown character's record");
    }
    record->types = matrix_stock.types;
    Character character(*party.character_rules(), original::buck_rogers_draft(*record),
                        from.character.appearance());
    const auto id = from.npc_source.empty()
                        ? party.add_pc(std::move(character))
                        : party.recruit("from:" + std::string(from_game) + ":" + from.npc_source,
                                        std::move(character), from.morale);
    party.keep_game_record(id, original::encode_ground_record(*record));
    return id;
}

} // namespace opengold
