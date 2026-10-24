#ifndef OPENGOLD_COMPANIONS_H
#define OPENGOLD_COMPANIONS_H
#include "opengold/character.h"
#include <optional>

namespace opengold
{
// The original game's NPC who joins the party through ADD NPC from the
// monster record `record` of MONnCHA archive `bank`, as an SRD character; none
// when the game has no conversion for it. Only Skullcrusher (MON4CHA 27) has
// one: a level-4 Fighter with his record's scores, unarmored and unarmed as
// found (CADORNA-1).
[[nodiscard]] std::optional<Character> original_companion(unsigned bank, unsigned record,
                                                          const rules::CharacterRules &creation,
                                                          const rules::RulesModule &rules);
} // namespace opengold
#endif
