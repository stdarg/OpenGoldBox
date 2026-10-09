#ifndef OPENGOLD_NPC_PORTRAITS_H
#define OPENGOLD_NPC_PORTRAITS_H
#include "opengold/rolf_tour.h"
#include <array>
#include <string_view>

namespace opengold
{
// The cluebook marks twelve separate New Phlan shops. Their map cells, rather
// than merchandise lists, identify the individual people who keep them. The
// original shop offer has a two-choice "THE SHOP..." dialogue before the host
// enters shopping; show the same person for that greeting and the entire shop.
[[nodiscard]] inline std::string_view phlan_shopkeeper_portrait(const por::TourSnapshot &state)
{
    const bool greeting = state.phase == por::TourPhase::awaiting_continue &&
                          state.choices.size() == 2 && state.dialogue.starts_with("THE SHOP");
    if (state.area_id != 0 || !state.tour_finished ||
            (state.phase != por::TourPhase::shopping && !greeting))
        return {};
    struct Shop
    {
        unsigned x, y;
        std::string_view portrait;
    };
    constexpr std::array shops{
        Shop{13, 8, "NPCs/phlan-arms-13-08.png"},
        Shop{8, 11, "NPCs/phlan-arms-08-11.png"},
        Shop{11, 12, "NPCs/phlan-arms-11-12.png"},
        Shop{9, 13, "NPCs/phlan-arms-09-13.png"},
        Shop{15, 8, "NPCs/phlan-general-15-08.png"},
        Shop{9, 10, "NPCs/phlan-general-09-10.png"},
        Shop{12, 10, "NPCs/phlan-general-12-10.png"},
        Shop{9, 11, "NPCs/phlan-general-09-11.png"},
        Shop{11, 11, "NPCs/phlan-general-11-11.png"},
        Shop{11, 10, "NPCs/phlan-silver-11-10.png"},
        Shop{10, 13, "NPCs/phlan-silver-10-13.png"},
        Shop{8, 10, "NPCs/phlan-jeweler-08-10.png"},
    };
    for (const auto &shop : shops)
        if (state.pose.x == shop.x && state.pose.y == shop.y)
            return shop.portrait;
    return {};
}

// The complete NPC portrait (docs/PORTRAITS.md), relative to the installed
// portraits folder. Shopkeepers appear at their greeting and throughout
// shopping; other NPCs replace the view while nearby and speaking. Empty when
// none does. Frame 0 is an encounter sprite's nearest pose.
[[nodiscard]] inline std::string_view speaking_npc_portrait(const por::TourSnapshot &state)
{
    if (const auto shopkeeper = phlan_shopkeeper_portrait(state); !shopkeeper.empty())
        return shopkeeper;
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
