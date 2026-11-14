#ifndef OPENGOLD_ORIGINAL_COMBAT_SESSION_H
#define OPENGOLD_ORIGINAL_COMBAT_SESSION_H
#include "opengold/original_combat.h"

#include <memory>
#include <string_view>

namespace opengold::original::detail
{

// A fight under the original rules (combat_session.cpp). A participant fights
// with the numbers its character profile encodes, or its definition's in the
// bestiary.
[[nodiscard]] std::unique_ptr<rules::CombatSession> create_session(rules::Identity identity,
                                                                   rules::Encounter encounter,
                                                                   std::uint64_t seed,
                                                                   const Content &content);
// A fight saved with CombatSession::save; its items' types are the game's.
[[nodiscard]] std::unique_ptr<rules::CombatSession>
restore_session(rules::Identity identity, std::string_view checkpoint, const Content &content);

} // namespace opengold::original::detail
#endif
