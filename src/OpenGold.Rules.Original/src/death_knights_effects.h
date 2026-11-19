#ifndef OPENGOLD_DEATH_KNIGHTS_EFFECTS_H
#define OPENGOLD_DEATH_KNIGHTS_EFFECTS_H

namespace opengold::original::death_knights_effect
{
// Native effects which differ from the existing games' mechanics. The range
// follows Champions' reserved effects (through 1654), preserving their IDs.
inline constexpr int offset = 1700;
inline constexpr int stunning_hit = offset + 2, dispel_evil_ward = offset + 4,
                     fixed_attack = offset + 7, constitution_regeneration = offset + 62,
                     needs_plus_three = offset + 66, immolation = offset + 70,
                     fire_touch = offset + 74, cold_vulnerable = offset + 75,
                     fire_heals = offset + 76, confusion_proof = offset + 80,
                     cold_electricity_resistance = offset + 83, regenerate_three = offset + 84,
                     death_breath = offset + 100, flesh_golem = offset + 101, smoke = offset + 106,
                     smoked = offset + 108;
// Derived robe/phase context, carried by the existing combat profile codec.
// Four times the robe plus its phase; this does not change actual class levels.
inline constexpr int casting_moon = offset + 250;
} // namespace opengold::original::death_knights_effect

#endif
