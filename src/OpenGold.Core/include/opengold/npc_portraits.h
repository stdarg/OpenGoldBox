#ifndef OPENGOLD_NPC_PORTRAITS_H
#define OPENGOLD_NPC_PORTRAITS_H
#include <string_view>

// The portrait tables change with content; keeping them out of the header means
// an edit recompiles one file, and callers need only the snapshot's name
// (Effective C++ Items 30 and 31).
namespace opengold::por
{
struct TourSnapshot;
} // namespace opengold::por

namespace opengold
{
// The cluebook marks twelve separate New Phlan shops. Their map cells, rather
// than merchandise lists, identify the individual people who keep them. The
// original shop offer has a two-choice "THE SHOP..." dialogue before the host
// enters shopping; show the same person for that greeting and the entire shop.
[[nodiscard]] std::string_view phlan_shopkeeper_portrait(const por::TourSnapshot &state);

// The complete NPC portrait (docs/PORTRAITS.md), relative to the installed
// portraits folder. Shopkeepers appear at their greeting and throughout
// shopping. Story portraits use script-specific speaker cues, not the shared
// sprite ID: several characters use the same original sprite. Frame 0 is the
// encounter sprite's nearest pose; -1 means no active approach sprite.
[[nodiscard]] std::string_view speaking_npc_portrait(const por::TourSnapshot &state);
} // namespace opengold
#endif
