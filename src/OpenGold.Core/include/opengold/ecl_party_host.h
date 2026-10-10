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
inline constexpr std::array<std::uint16_t, 7> ecl_coin_addresses{0x6BBB, 0x6BBD, 0x6BBF, 0x6BC1,
    0x6BC3, 0x6BC5, 0x6BC7};

// Whether an active member carries an item of the original type.
[[nodiscard]] bool party_has_item(const CampaignParty &party, unsigned original_type);
// The original PARTY STRENGTH value.
[[nodiscard]] unsigned party_strength(const CampaignParty &party);
// CHECK PARTY: the lowest, highest and average of a party attribute.
[[nodiscard]] std::array<unsigned, 4> check_party(const CampaignParty &party, unsigned address,
        unsigned effect);
// The script memory writes describing the member in a party slot.
[[nodiscard]] EclHostReply party_character_reply(const CampaignParty &party, PartySlot slot);
} // namespace opengold::por
#endif
