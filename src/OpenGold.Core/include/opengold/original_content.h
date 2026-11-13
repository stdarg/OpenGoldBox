#ifndef OPENGOLD_ORIGINAL_CONTENT_H
#define OPENGOLD_ORIGINAL_CONTENT_H

#include "opengold/gold_box_game.h"
#include "opengold/original_combat.h"

#include <filesystem>

namespace opengold
{

// What the original rules need of a game's folder: every creature as its
// record fights, with its items and effects, and the item types. Throws when
// the folder's creatures or ITEMS cannot be read.
[[nodiscard]] original::Content load_original_content(const std::filesystem::path &folder,
                                                      GoldBoxGame game);

} // namespace opengold

#endif
