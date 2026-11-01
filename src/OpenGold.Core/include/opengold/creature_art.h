#ifndef OPENGOLD_CREATURE_ART_H
#define OPENGOLD_CREATURE_ART_H

#include "opengold/campaign_encounter.h"
#include "opengold/creature_catalog.h"
#include "opengold/monster_bundle.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace opengold::por
{
// A creature's combat art, the one way any fight gets it: the export's weapon
// set for what it readies and the sets its melee attacks and shots show, drawn
// as the catalog previews them; or else the original icon with its action pose,
// the icon 128 records on. New art keeps the original, which the Combat art
// setting can show. Empty when `archive`, a CPICn.DAX, lacks the icon.
[[nodiscard]] std::optional<CombatArt> creature_art(const std::vector<std::uint8_t> &archive,
                                                    unsigned icon, const ExportedMonster *exported,
                                                    bool archer);

// A fighter of any fight takes the gear its export gives it, the one way any
// fight arms it: a human-shaped creature fights with the weapons it holds
// (#274), shoots only with a ranged weapon it carries and holds its readied
// shield. Without an export, or human shape, it keeps its stat block's attacks.
void arm_as_exported(rules::Participant &fighter, const Creature &creature,
                     const ExportedMonster *exported);
} // namespace opengold::por

#endif
