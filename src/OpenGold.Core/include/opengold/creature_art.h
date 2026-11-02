#ifndef OPENGOLD_CREATURE_ART_H
#define OPENGOLD_CREATURE_ART_H

#include "opengold/campaign_encounter.h"
#include "opengold/creature_catalog.h"
#include "opengold/difficulty.h"
#include "opengold/monster_bundle.h"
#include <cstdint>
#include <optional>
#include <string>
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

// A creature as any fight stages it, the one way the game, demos and tools do:
// the squares it fills, its morale and Intelligence from its original record and
// the gear its export gives it. A human-shaped creature fights with the weapons
// it holds (#274), shoots only with a ranged weapon it carries and holds its
// readied shield. On Hard and above it brings its potions, wands and scrolls.
[[nodiscard]] rules::Participant
staged_fighter(rules::EntityId id, std::string definition, std::string name, rules::Side side,
               const Creature &creature, const ExportedMonster *exported, Difficulty difficulty);

// The original gear an export kept, by its place in the creature's gear; a place
// the export left out is empty.
[[nodiscard]] std::vector<std::optional<Equipment>> carried_gear(const Creature &creature,
                                                                 const ExportedMonster *exported);

// The potions, wands and scrolls a monster can bring into a fight, each known by
// its place in the creature's gear, plus one.
[[nodiscard]] std::vector<rules::CarriedEquipment>
fighting_gear(const Creature &creature, const ExportedMonster *exported);
} // namespace opengold::por

#endif
