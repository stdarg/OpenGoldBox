#ifndef OPENGOLD_NPC_PORTRAITS_H
#define OPENGOLD_NPC_PORTRAITS_H
#include "opengold/rolf_tour.h"
#include <string_view>

namespace opengold
{
// The complete NPC portrait (docs/PORTRAITS.md), relative to the installed
// portraits folder, that replaces the view while that NPC has walked up and
// speaks; empty when none does. Frame 0 is an encounter sprite's nearest pose.
[[nodiscard]] inline std::string_view speaking_npc_portrait(const por::TourSnapshot &state)
{
    if (state.sprite_frame != 0 || state.dialogue.empty())
        return {};
    if (!state.tour_finished)
        return "NPCs/rolf.png";
    // Kuto's Well (ECL8:29) shows sprite 16 only for Norris the Gray's ambush.
    if (state.script_id == 29 && state.sprite_id == 16)
        return "NPCs/norris-the-gray.png";
    return {};
}
} // namespace opengold
#endif
