#ifndef OPENGOLD_DARK_QUEEN_EFFECTS_H
#define OPENGOLD_DARK_QUEEN_EFFECTS_H

#include <algorithm>
#include <array>

namespace opengold::original::dark_queen_effect
{
// Separate native behaviors from every released game's numeric effect bank.
// Shared behavior keeps its existing canonical ID instead.
inline constexpr int offset = 2000;
inline constexpr int mask_16_save_bonus = offset + 126, disintegrating_ray = offset + 198,
                     enchanted_paralysis = offset + 199, steam_breath = offset + 204,
                     blinding_ray = offset + 209, poison_bite = offset + 211,
                     anemone_acid = offset + 212;
inline constexpr int disruption = offset + 147, dragon_slayer = offset + 214;
inline constexpr int disruption_double_trait = 0x1000;
inline constexpr int underwater_weapons = offset + 149;
inline constexpr int free_action = offset + 150;
inline constexpr int green_fire = offset + 196, green_fire_touch = offset + 201;
inline constexpr int spore_burst = offset + 182, spore_touch = offset + 184,
                     spore_infection = offset + 185, ice_explosion = offset + 188,
                     meteor_explosion = offset + 189;
inline constexpr int disarming_body = offset + 187, baldric_breath = offset + 202,
                     constricting_touch = offset + 208;
inline constexpr int enchanted_draconian = offset + 191;
inline constexpr int mummy_touch = offset + 170, rot_recovery = offset + 215;
inline constexpr int cold_harms_more = offset + 192, mundane_blows_fail = offset + 197,
                     resists_fire = offset + 205, venom_touch = offset + 206,
                     venomed = offset + 207;
// Engine-owned combat control state, outside the original byte-sized effect bank.
inline constexpr int led_by_knight = offset + 300;

[[nodiscard]] inline bool unimplemented(int canonical) noexcept
{
    if (canonical < offset || canonical > offset + 255)
    {
        return false;
    }
    // Registered empty handlers and passive markers. They must survive in
    // a profile without becoming an older game's active ability.
    static constexpr std::array no_op{5,   7,   19,  48,  59,  72,  87,  109, 133, 140, 146,
                               150, 164, 183, 190, 191, 194, 210, 213, 216, 217, 218,
                               219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229};
    static constexpr std::array implemented{disruption,         dragon_slayer,      underwater_weapons,
                                     green_fire,         green_fire_touch,   spore_burst,
                                     spore_touch,        spore_infection,    ice_explosion,
                                     meteor_explosion,   disarming_body,     baldric_breath,
                                     constricting_touch, mummy_touch,        rot_recovery,
                                     mask_16_save_bonus, disintegrating_ray, enchanted_paralysis,
                                     steam_breath,       blinding_ray,       poison_bite,
                                     anemone_acid,       cold_harms_more,    mundane_blows_fail,
                                     resists_fire,       venom_touch,        venomed};
    return std::ranges::find(no_op, canonical - offset) == no_op.end() &&
           std::ranges::find(implemented, canonical) == implemented.end();
}
} // namespace opengold::original::dark_queen_effect

#endif
