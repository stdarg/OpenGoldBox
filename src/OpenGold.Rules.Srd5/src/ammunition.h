#ifndef OPENGOLD_SRD5_AMMUNITION_H
#define OPENGOLD_SRD5_AMMUNITION_H

namespace opengold::srd5::detail
{
// What a ranged weapon shoots (SRD 5.2.1 pp. 89, 91, 96). OpenGoldBox has no
// ammunition items and never counts shots (AMMO-UNLIMITED); the kind still
// tells a firearm from a bow.
enum class Ammunition
{
    none,
    arrow,
    bolt,
    sling_bullet,
    firearm_bullet,
    needle
};
} // namespace opengold::srd5::detail
#endif
