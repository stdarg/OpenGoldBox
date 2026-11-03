#ifndef OPENGOLD_COMPANIONS_H
#define OPENGOLD_COMPANIONS_H
#include "opengold/character.h"
#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace opengold
{
// An original NPC who joins the party through ADD NPC, as an SRD character:
// data/companions/companions.json, which the Creature Stats Editor writes
// (dev_tools/docs/creature-stats-editor-interface.md). Skullcrusher (MON4CHA
// 27) is a level-4 Fighter with his record's scores (CADORNA-1).
struct CompanionDefinition
{
    unsigned bank{}, record{}; // the MONnCHA record ADD NPC names
    std::string name, race, gender, character_class, alignment, background;
    // The scores rolled, Strength first, before the background's increase of
    // +2 to one ability and +1 to another.
    std::array<int, 6> rolled{};
    rules::Ability plus_two{}, plus_one{};
    // Reached from level 1 with the rules' default choice at each level.
    int level{1};
    std::vector<NewItem> items; // added in order after advancing
};

// Reads data/companions. Throws std::runtime_error, naming the file, the
// companion and the field, on a file the game cannot use.
[[nodiscard]] std::vector<CompanionDefinition> load_companions(const std::filesystem::path &folder);

// The companion `definition` describes. Throws when the rules refuse it, such
// as an unknown class or a level the class cannot reach.
[[nodiscard]] Character build_companion(const CompanionDefinition &definition,
                                        const rules::CharacterRules &creation,
                                        const rules::RulesModule &rules);

// The companion of the monster record `record` of MONnCHA archive `bank`;
// none when `companions` has no definition for it.
[[nodiscard]] std::optional<Character>
original_companion(unsigned bank, unsigned record, std::span<const CompanionDefinition> companions,
                   const rules::CharacterRules &creation, const rules::RulesModule &rules);
} // namespace opengold
#endif
