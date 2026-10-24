#ifndef OPENGOLD_COMPANIONS_H
#define OPENGOLD_COMPANIONS_H
#include "opengold/character.h"
#include <optional>

namespace opengold
{
// The original game's NPC who joins the party through ADD NPC from the
// monster record `record` of MONnCHA archive `bank`, as an SRD character; none
// when the game has no conversion for it. Skullcrusher (MON4CHA 27) is a
// level-4 Fighter with his record's scores (CADORNA-1), and Mendor's
// Library's mad fighter (MON2CHA 25) a level-1 one (MENDOR-1), each unarmored
// and unarmed as found.
[[nodiscard]] std::optional<Character> original_companion(unsigned bank, unsigned record,
                                                          const rules::CharacterRules &creation,
                                                          const rules::RulesModule &rules);
} // namespace opengold
#endif
