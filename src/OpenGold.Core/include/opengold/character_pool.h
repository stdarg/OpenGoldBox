#ifndef OPENGOLD_CHARACTER_POOL_H
#define OPENGOLD_CHARACTER_POOL_H
#include "opengold/character.h"
namespace opengold
{
// Curated level-one drafts evaluated by the selected creation rules. Art is
// resolved from the user's loaded catalog; no original images are distributed.
[[nodiscard]] std::vector<Character> character_pool(const rules::CharacterRules &,
                                                    const por::CharacterArt &);
} // namespace opengold
#endif
