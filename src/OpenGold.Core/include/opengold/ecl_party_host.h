#ifndef OPENGOLD_ECL_PARTY_HOST_H
#define OPENGOLD_ECL_PARTY_HOST_H
#include "opengold/campaign_party.h"
#include "opengold/ecl_machine.h"
#include <array>
#include <cstdint>

// The original scripts' view of the campaign party: the memory layout, coin
// addresses and queries of Pool of Radiance's ECL host. These need only the
// party's public interface, so they are not members of CampaignParty
// (Effective C++ Item 23).
namespace opengold::por
{
// Where a character's coins sit in the original script memory, copper first.
inline constexpr std::array<EclAddress, 7> ecl_coin_addresses{
    EclAddress{0x6BBB}, EclAddress{0x6BBD}, EclAddress{0x6BBF}, EclAddress{0x6BC1},
    EclAddress{0x6BC3}, EclAddress{0x6BC5}, EclAddress{0x6BC7}};

// Whether an active member carries an item of the original type.
[[nodiscard]] bool party_has_item(const CampaignParty &party, unsigned original_type);
// Whether a member, living or not, has a Ranger level, as PARTY SURPRISE asks.
[[nodiscard]] bool has_ranger(const CampaignParty &party);
// The original PARTY STRENGTH value.
[[nodiscard]] unsigned party_strength(const CampaignParty &party);
// CHECK PARTY: the lowest, highest and average of a party attribute (movement
// or a thief skill), or, for attribute 0, whether a spell effect is on the
// party in the fourth value (coab CMD_CheckParty). Secret also asks whether a
// race is in the party, and for Charisma and class levels.
[[nodiscard]] std::array<unsigned, 4> check_party(const CampaignParty &party, EclAddress attribute,
                                                  unsigned effect,
                                                  GoldBoxGame game = GoldBoxGame::pool_of_radiance);
// The reaction score (0x6DCF) the scripts add to a parley's reaction roll,
// from the speaker's Charisma, as the original's table gives it.
[[nodiscard]] unsigned reaction_score(int charisma);
// The script memory writes describing the member in a party slot.
[[nodiscard]] EclHostReply party_character_reply(const CampaignParty &party, PartySlot slot);
} // namespace opengold::por
#endif
