#ifndef OPENGOLD_NPC_PORTRAITS_H
#define OPENGOLD_NPC_PORTRAITS_H
#include "opengold/shop_data.h"
#include <string_view>
#include <vector>

// The portrait tables change with content; keeping them out of the header means
// an edit recompiles one file, and callers need only the snapshot's name
// (Effective C++ Items 30 and 31).
namespace opengold::por
{
struct TourSnapshot;
} // namespace opengold::por

namespace opengold
{
// The New Phlan shop the party stands in, from its data/shops record, while
// its shopkeeper speaks: the original shop offer has a two-choice "THE
// SHOP..." dialogue before the host enters shopping, and the same person
// greets the party and serves it. None at any other time.
[[nodiscard]] const ShopSite *phlan_shop_serving(const por::TourSnapshot &state,
                                                 const std::vector<ShopSite> &shops);

// The portrait of the shopkeeper phlan_shop_serving finds.
[[nodiscard]] std::string_view phlan_shopkeeper_portrait(const por::TourSnapshot &state,
                                                         const std::vector<ShopSite> &shops);

// The priest of the temple the party stands in, from its data/shops record,
// for everything the temple says there: its greeting and its Heal list.
[[nodiscard]] std::string_view temple_priest_portrait(const por::TourSnapshot &state,
                                                      const std::vector<TempleSite> &temples);

// The complete NPC portrait (docs/PORTRAITS.md), relative to the installed
// portraits folder. Shopkeepers appear at their greeting and throughout
// shopping, and priests while their temple speaks. Story portraits use script-specific speaker
// cues, not the shared sprite ID: several characters use the same original sprite. Frame 0 is the
// encounter sprite's nearest pose; -1 means no active approach sprite.
[[nodiscard]] std::string_view speaking_npc_portrait(const por::TourSnapshot &state,
                                                     const std::vector<ShopSite> &shops = {},
                                                     const std::vector<TempleSite> &temples = {});
} // namespace opengold
#endif
