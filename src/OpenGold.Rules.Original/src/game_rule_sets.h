#ifndef OPENGOLD_GAME_RULE_SETS_H
#define OPENGOLD_GAME_RULE_SETS_H
#include "opengold/original_rules.h"
#include <array>
#include <optional>
#include <span>
#include <vector>

namespace opengold::original
{

// The character rules a game plays by where the games differ: the Krynn
// games and the Buck Rogers games have their own tables in their own files,
// and the Realms games share one set of functions that read the game for the
// differences among them (Pool, Curse, Secret, Gateway, Pools of Darkness,
// Treasures and Unlimited Adventures). Each function takes the game last, as
// the public function it answers does.
struct GameRuleSet
{
    std::span<const Profession> (*professions_for)(Race race, GoldBoxGame game){};
    AbilityLimits (*ability_limits)(Race race, Gender gender, GoldBoxGame game){};
    rules::AbilityArray<int> (*class_minimums)(Profession profession, GoldBoxGame game){};
    std::vector<StartingItem> (*starting_items)(const ClassLevels &levels, GoldBoxGame game){};
    int (*starting_age)(Race race, Profession profession, RandomState &random, GoldBoxGame game){};
    int (*thac0)(const ClassLevels &levels, GoldBoxGame game){};
    std::array<int, 5> (*saving_throws)(const ClassLevels &levels, GoldBoxGame game){};
    std::optional<int> (*experience_for_level)(ClassSlot slot, int level, GoldBoxGame game){};
    std::vector<int> (*known_spells)(const ClassLevels &levels, int wisdom,
                                     std::span<const int> learned, GoldBoxGame game){};
    std::optional<int> (*racial_level_limit)(Race race, ClassSlot slot,
                                             const rules::AbilityArray<int> &scores,
                                             GoldBoxGame game){};
    SpellSlots (*spell_slots)(ClassSlot slot, int level, int wisdom, int intelligence,
                              GoldBoxGame game){};
    CasterSlots (*caster_slots)(const ClassLevels &levels, int wisdom, int intelligence,
                                GoldBoxGame game){};
};

// The rules the game plays by; every game has a row.
[[nodiscard]] const GameRuleSet &rule_set(GoldBoxGame game) noexcept;

} // namespace opengold::original

#endif
