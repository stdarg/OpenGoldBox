#ifndef OPENGOLD_ECL_ADDRESS_H
#define OPENGOLD_ECL_ADDRESS_H
#include <compare>
#include <cstdint>

namespace opengold::por
{
// A cell of the original script memory. Addresses and the values stored in
// them are both 16-bit, so a distinct type with explicit construction keeps a
// swapped (address, value) pair from compiling (Effective C++ Item 18).
struct EclAddress
{
    constexpr explicit EclAddress(std::uint16_t cell) noexcept : location(cell)
    {
    }

    std::uint16_t location;
    auto operator<=>(const EclAddress &) const = default;
};

// The script cells the engine itself reads or writes. See docs/ROLF.md,
// docs/RECOVERY.md and docs/PHLAN.md for how each was identified.

// The party's map position and facing (0 north, 1 east, 2 south, 3 west), then
// the wall on the facing side of its cell and that cell's raw event byte.
inline constexpr EclAddress ecl_party_x{0xC04B};
inline constexpr EclAddress ecl_party_y{0xC04C};
inline constexpr EclAddress ecl_party_facing{0xC04D};
inline constexpr EclAddress ecl_wall_ahead{0xC04E};
inline constexpr EclAddress ecl_cell_event{0xC04F};

// The game clock: minute digits, hour, day of month and month (both from 1),
// and year.
inline constexpr EclAddress ecl_clock_minute_ones{0x49C7};
inline constexpr EclAddress ecl_clock_minute_tens{0x49C8};
inline constexpr EclAddress ecl_clock_hour{0x49C9};
inline constexpr EclAddress ecl_clock_day{0x49CA};
inline constexpr EclAddress ecl_clock_month{0x49CB};
inline constexpr EclAddress ecl_clock_year{0x49CC};

// The selected character's record fields start here; its hit points are one
// of them.
inline constexpr EclAddress ecl_character_record{0x6B00};
inline constexpr EclAddress ecl_character_hit_points{0x6C19};

// Combat: the morale the script sets beforehand, the surprise bits (1 the
// party, 2 the monsters), and the outcome reported back (0 won, 128 fled) with
// the number of monsters defeated.
inline constexpr EclAddress ecl_encounter_morale{0x6DC6};
inline constexpr EclAddress ecl_encounter_surprise{0x6DCB};
inline constexpr EclAddress ecl_combat_result{0x6DC7};
inline constexpr EclAddress ecl_monsters_defeated{0x6DC8};

// Camp: how many five-minute rest steps pass between interruption checks, and
// the d100 chance of one (255 forbids resting here).
inline constexpr EclAddress ecl_rest_check_interval{0x6DD2};
inline constexpr EclAddress ecl_rest_interruption_chance{0x6DD3};

// Set to 1 when the script's COMBAT command opens a temple or a shop instead.
inline constexpr EclAddress ecl_temple_service{0x6DE2};
inline constexpr EclAddress ecl_shop_service{0x6E6C};

// The archive bank (ECLn.DAX) of the script a NEW ECL loads.
inline constexpr EclAddress ecl_script_bank{0x6E12};

// Slums event 1 sets this to 255 once its four-orc fight is won.
inline constexpr EclAddress ecl_slums_orc_victory{0x4ACA};
} // namespace opengold::por
#endif
