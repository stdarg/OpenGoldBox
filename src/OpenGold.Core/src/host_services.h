#ifndef OPENGOLD_HOST_SERVICES_H
#define OPENGOLD_HOST_SERVICES_H

// The helpers RolfTourSession's town services and script hosts share:
// phlan_session.cpp, encounter_host.cpp and the host_*.cpp files that hold
// its halls, temples, shops, treasure and camp.

#include "opengold/ecl_machine.h"
#include "opengold/formats.h"
#include "opengold/gold_box_game.h"
#include "opengold/message.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace opengold::por
{
struct PhlanResources;
}

namespace opengold::por::host_services
{
using opengold::rules::Message;

// The English text of host messages, one per line, for logs and tests.
[[nodiscard]] std::string english(const std::vector<Message> &messages);
// Thousands separated as the halls' fees are written ("1,000").
[[nodiscard]] std::string amount(std::uint32_t value);
// The 256-color engine's scripts (Pools of Darkness's and Treasures') read a
// fight's result with IF right after COMBAT: its PostCombat sets "equal" when
// the party won or came through, and "not equal" when it fled (postcbt.pas).
// The other games keep the flags and leave the result in a cell.
[[nodiscard]] std::optional<EclConditions> vga_combat_conditions(GoldBoxGame game,
                                                                 bool party_stood);
// The original's close-up portrait: an 88 x 40 HEAD record above an 88 x 48
// BODY record.
[[nodiscard]] Image close_up_portrait(const PhlanResources &resources, unsigned head_id,
                                      unsigned body_id);
// The minute of the day on the campaign clock, which starts at noon.
[[nodiscard]] std::uint64_t clock_minute_of_day(std::uint64_t minutes);
} // namespace opengold::por::host_services
#endif
