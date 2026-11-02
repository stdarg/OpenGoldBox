#ifndef OPENGOLD_AI_SCENARIO_H
#define OPENGOLD_AI_SCENARIO_H
#include "opengold/campaign_encounter.h"
#include "opengold/difficulty.h"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace opengold
{
class CampaignParty;
struct PartyState;

namespace rules
{
class RulesModule;
}

// One fight of the campaign kept for replaying (docs/AI-LAB.md): the party as it
// stood before the fight and the encounter as the game built it, gear included,
// but no art. A scenario pack is generated from the player's original files, so it
// is never committed.
struct AiScenario
{
    std::string name;       // The file's stem, such as "0035".
    std::string party_save; // encode_campaign's bytes, with no town.
    CampaignEncounter encounter;
};

// The identity a pack's party saves carry in place of the original files' hash:
// replaying needs no original files.
inline constexpr std::string_view ai_scenario_assets = "ai-scenario";

// Text form of an encounter's rules-relevant parts. Throws on what it cannot keep,
// so a pack never holds a fight that differs from the game's.
[[nodiscard]] std::string encode_ai_encounter(const CampaignEncounter &encounter);
[[nodiscard]] CampaignEncounter decode_ai_encounter(std::string_view text);

// Writes <folder>/<name>.ogs and <folder>/<name>.fight.
void write_ai_scenario(const std::filesystem::path &folder, std::string_view name,
                       const CampaignParty &party, const CampaignEncounter &encounter);

// Every scenario in the folder, in name order.
[[nodiscard]] std::vector<AiScenario> load_ai_scenarios(const std::filesystem::path &folder);

struct AiFightResult
{
    bool won{}, stalled{};
    int hit_points_left{}, members_down{}, members_dead{};
    unsigned commands{};
};

using RulesModuleFactory = std::function<std::unique_ptr<rules::RulesModule>()>;

// The fight as `difficulty` meets it (DIFF-2): sized from its band for these
// members, when it has one, and with the monsters strengthened.
[[nodiscard]] CampaignEncounter encounter_at_difficulty(CampaignEncounter encounter,
                                                        std::span<const unsigned> member_levels,
                                                        Difficulty difficulty);

// Plays one fight to its end through the game's own combat and policy, with no
// player: party members at `party_level`, and monsters, those on the party's side
// included, at `monster_level`, sized and strengthened by that level. `party` is
// not changed.
[[nodiscard]] AiFightResult play_ai_fight(const RulesModuleFactory &make_module,
                                          const PartyState &party,
                                          const CampaignEncounter &encounter,
                                          Difficulty party_level, Difficulty monster_level,
                                          std::uint64_t seed);
} // namespace opengold
#endif
