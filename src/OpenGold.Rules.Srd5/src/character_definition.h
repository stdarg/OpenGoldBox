#ifndef OPENGOLD_SRD5_CHARACTER_DEFINITION_H
#define OPENGOLD_SRD5_CHARACTER_DEFINITION_H
#include "definition.h"
#include "character_class.h"
#include "opengold/character_rules.h"
#include "opengold/rules.h"
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// How a Definition is made: a character's from its profile, cached by profile
// and equipment, and a monster's as a fight fields it.
namespace opengold::srd5::detail
{
using rules::FeatureGrant;

// `draconic`: a Draconic Sorcerer gains its level in Hit Points from level 3.
// The Manual of Bodily Health (MAGIC-3): each one read adds this grant, from
// this source, and 2 to Constitution and its maximum.
inline constexpr std::string_view bodily_health = "boon:bodily_health";

// A spell a Wizard copied from a scroll (MAGIC-6): "boon:scroll:fireball".
inline constexpr std::string_view copied_spell = "boon:scroll:";

inline constexpr std::string_view manual_source = "item:manual_of_bodily_health";

// The grants a class, species, background or feat gives, without a Manual's.
[[nodiscard]] std::vector<FeatureGrant> without_manuals(std::vector<FeatureGrant> grants);

// The starting class's whole die at level 1, then each level's class's
// average (SRD 5.2.1 pp. 23 and 25, MULTI-1), with Constitution's history, a
// Dwarf's Toughness and Draconic Resilience's 1 per Sorcerer level from 3.
[[nodiscard]] int maximum_hit_points(const detail::ClassHistory &history, bool dwarf,
                                     std::span<const int> modifiers);

// Module-owned character recipe. Original item IDs never enter this layer.
[[nodiscard]] Definition
character_definition(std::string_view bytes,
                     std::optional<std::span<const std::string>> equipment_override = std::nullopt);

// A creature holding the weapons its art shows (#274): their dice, damage
// types, enchantment and range in place of its stat block's, with the stat
// block's own attack and damage bonuses. Reach stays 5 feet (MELEE-1).
// A monster as the difficulty made it (DIFF-2).
[[nodiscard]] Definition strengthened(Definition d, const rules::Participant &who);

[[nodiscard]] Definition wielding(Definition d, std::string_view melee_key,
                                  std::string_view ranged_key);

// A monster as a fight fields it: its stat line, the weapons it holds and
// what the difficulty does to it.
[[nodiscard]] Definition fielded(const Content &content, const rules::Participant &who);

// A character's statistics after taking off its shield: the same equipment
// without it.
[[nodiscard]] Definition without_shield(std::string_view profile, const Definition &d);

// A monster's statistics after taking off its shield, which gives +2 Armor Class.
[[nodiscard]] Definition monster_without_shield(Definition d);

// What the computer weighs a melee weapon by, over its attacks a turn.
[[nodiscard]] int melee_rating(const Definition &d);

[[nodiscard]] bool wears_shield(const Definition &d);

// Character definitions by profile and equipment, each parsed once. A pure
// function of its inputs, so copies of a session share one cache; a session
// is used from one thread at a time (Effective Modern C++ Item 16).
class ProfileDefinitions
{
  public:
    // `keys` replaces the profile's own equipment when given.
    Definition of(std::string_view profile,
                  std::optional<std::span<const std::string>> keys = std::nullopt)
    {
        std::string key(profile);
        key += keys ? '\x1e' : '\x1f';
        if (keys)
        {
            for (const auto &item : *keys)
            {
                (key += item) += '\x1f';
            }
        }
        if (const auto found = parsed_.find(key); found != parsed_.end())
        {
            return found->second;
        }
        // A long fight with many changes of gear keeps the cache bounded.
        if (parsed_.size() >= 512)
        {
            parsed_.clear();
        }
        return parsed_.emplace(std::move(key), character_definition(profile, keys)).first->second;
    }

  private:
    std::unordered_map<std::string, Definition> parsed_;
};
} // namespace opengold::srd5::detail
#endif
