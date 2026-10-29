#ifndef OPENGOLD_TELEPORT_H
#define OPENGOLD_TELEPORT_H
#include <algorithm>
#include <array>

namespace opengold::por
{
// Teleport (TELEPORT-1): each map's spot, an open square near its center
// with no event of its own, joined to the rest of the map. Maps are the GEO
// records the session's areas use; 0 is New Phlan.
struct TeleportSpot
{
    unsigned map{}, x{}, y{};
};

inline constexpr std::array teleport_spots{
    TeleportSpot{0, 7, 7},   // New Phlan
    TeleportSpot{20, 8, 8},  // Slums
    TeleportSpot{29, 7, 10}, // Kuto's Well, the plaza
    TeleportSpot{32, 7, 3},  // Kuto's Well, the catacombs
    TeleportSpot{18, 6, 7},  // Podol Plaza
    TeleportSpot{2, 7, 7},   // Cadorna Textile House
    TeleportSpot{15, 1, 7},  // Mendor's Library
    TeleportSpot{21, 7, 7},  // Sokal Keep
    TeleportSpot{14, 2, 7},  // Kovel Mansion
    TeleportSpot{31, 7, 7},  // the street east of the Temple of Bane
    TeleportSpot{24, 7, 2},  // the Temple of Bane's block
    TeleportSpot{26, 7, 9},  // the Central Wilderness grove
    TeleportSpot{17, 7, 7},  // Nomad Camp
    TeleportSpot{22, 7, 8},  // the Pyramid, the prison of souls
    TeleportSpot{23, 7, 7},  // the Pyramid, the laboratories
    TeleportSpot{25, 7, 6},  // the western wilderness's cave
    TeleportSpot{27, 7, 7},  // the eastern wilderness's cave
    TeleportSpot{13, 7, 7},  // Kobold Caves
    TeleportSpot{16, 5, 7},  // Lizard Men's Keep, the first level
    TeleportSpot{30, 7, 7},  // Lizard Men's Keep, the flooded level
    TeleportSpot{10, 6, 7},  // Valhingen Graveyard
    TeleportSpot{9, 7, 12},  // Stojanow Gate
    TeleportSpot{6, 3, 7},   // Old Town
    TeleportSpot{3, 7, 3},   // Valjevo Castle, the northwest quarter
    TeleportSpot{4, 7, 3},   // Valjevo Castle, the northeast quarter
    TeleportSpot{5, 8, 8},   // Valjevo Castle, the southeast quarter
    TeleportSpot{7, 7, 7},   // Valjevo Castle, the tower's upper level
    TeleportSpot{1, 8, 6},   // Buccaneers' Base
    TeleportSpot{28, 4, 7},  // Zhentil Outpost
};

// The map's spot; null for a map without one.
[[nodiscard]] constexpr const TeleportSpot *teleport_spot(unsigned map) noexcept
{
    const auto found = std::find_if(teleport_spots.begin(), teleport_spots.end(),
                                    [map](const TeleportSpot &spot)
                                    {
                                        return spot.map == map;
                                    });
    return found == teleport_spots.end() ? nullptr : &*found;
}
} // namespace opengold::por
#endif
