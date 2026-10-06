#ifndef OPENGOLD_SRD5_CREATURE_EQUIPMENT_H
#define OPENGOLD_SRD5_CREATURE_EQUIPMENT_H
#include <string_view>

namespace opengold::srd5::detail
{
// Whether a monster or NPC, by combat creature key, wears metal armor. A stub
// until creature equipment metadata exists
// (https://github.com/stdarg/OpenGold/issues/231): no creature is known to.
inline bool creature_wears_metal(std::string_view creature_key)
{
    (void)creature_key;
    return false;
}
} // namespace opengold::srd5::detail
#endif
