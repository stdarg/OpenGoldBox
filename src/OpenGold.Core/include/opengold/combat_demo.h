#ifndef OPENGOLD_COMBAT_DEMO_H
#define OPENGOLD_COMBAT_DEMO_H
#include "opengold/campaign_encounter.h"
#include "opengold/rules.h"
#include "opengold/character_rules.h"
#include "opengold/difficulty.h"
#include "opengold/ecl_address.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace opengold
{
namespace por
{
struct MonsterBundle;
}

class CampaignParty;

// A bounded demonstration/campaign adapter. It depends on the rules interface,
// never on a specific edition. The application supplies the selected module.
class CombatDemo
{
  public:
    explicit CombatDemo(std::unique_ptr<rules::RulesModule> module);
    ~CombatDemo();
    // A demo holds the campaign's edit lock while a fight runs, so it is
    // neither copied nor moved: its owners keep it in a std::unique_ptr.
    CombatDemo(const CombatDemo &) = delete;
    CombatDemo &operator=(const CombatDemo &) = delete;
    CombatDemo(CombatDemo &&) = delete;
    CombatDemo &operator=(CombatDemo &&) = delete;
    void campaign_party(std::shared_ptr<CampaignParty> party);

    // When a campaign fight ends every member is back under the player's
    // control, unless the Options screen keeps Quick for the next fight.
    void keep_quick(bool keep) noexcept;

    void training(std::uint64_t seed = 42, bool conditions = false);
    void slums(const std::filesystem::path &game_directory, std::uint64_t seed = 42,
               std::shared_ptr<const por::MonsterBundle> monsters = {},
               Difficulty difficulty = default_difficulty);
    void encounter(CampaignEncounter encounter, std::uint64_t seed);

    [[nodiscard]] const std::vector<std::uint8_t> &battlefield_tiles() const;
    [[nodiscard]] const std::vector<Image> &terrain_art() const;

    [[nodiscard]] const rules::CombatSession &combat() const;
    bool submit(const rules::Command &command);
    // The original's "Attack Ally", once the player says yes: the attacker's
    // own side loses every companion and every creature fighting beside it
    // that still stands, who fight for the opposition from now on.
    void attack_allies(rules::EntityId attacker);
    void continue_script();
    void revisit();
    [[nodiscard]] std::string save_combat() const;
    void restore_combat(std::string_view checkpoint);

    [[nodiscard]] const std::string &dialogue() const noexcept;
    [[nodiscard]] const std::string &status() const noexcept;
    [[nodiscard]] bool waiting() const noexcept;
    [[nodiscard]] bool has_combat() const noexcept;
    [[nodiscard]] bool is_slums() const noexcept;
    [[nodiscard]] bool script_complete() const noexcept;
    [[nodiscard]] unsigned script_variable(por::EclAddress address) const;
    [[nodiscard]] const std::vector<CombatArt> &art() const noexcept;

  private:
    // The fight's state lives in the .cpp so includers do not compile the
    // script machine, the catalogs or the party (Effective Modern C++ Item 22).
    class CampaignCombat;
    struct Impl;

    std::unique_ptr<Impl> impl_;
    void install_combat(std::unique_ptr<rules::CombatSession> next, std::string reward_id);
    void finish_campaign_combat(rules::Outcome outcome);
    void start_encounter(std::vector<rules::Participant> enemies, std::string reward_id);
    void synchronize_party();
    void pump();
    void finish_combat();
};

struct CombatDemoSetup
{
    std::shared_ptr<CampaignParty> party;
    CampaignEncounter encounter;
};

// The showcase party, or up to six pool characters of the given classes in their
// starting kits, advanced to `level`, for play-testing. Play-testing can also
// replace the kobolds with `enemies` (combat definitions such as "troll" and
// "ogre") and give each member one of each `gear` item ("oil",
// "alchemists_fire", "acid").
// A play-test's choices: the body catalog to dress the party from, its classes
// in their kits at one level, the enemies it faces and the gear it carries.
// Named fields keep the lists of names (enemies, gear) from being swapped
// (Effective C++ Item 18).
struct CombatDemoOptions
{
    std::filesystem::path body_catalog_file;
    std::vector<std::string> classes;
    unsigned level{1};
    std::vector<std::string> enemies, gear;
    std::shared_ptr<const por::MonsterBundle> monsters;
    Difficulty difficulty{default_difficulty};
};

[[nodiscard]] CombatDemoSetup make_combat_demo(std::unique_ptr<rules::RulesModule> rules,
                                               const rules::CharacterRules &characters,
                                               const std::filesystem::path &game_directory,
                                               const CombatDemoOptions &options = {});
// Whom the computer goes looking for when its creature sees no enemy: no one,
// as in the game (docs/AI.md); or, in an automated run with no player to
// break a standoff, a stand-in for the player that searches for the enemy;
// or, in a check that needs every fight to end, monsters too.
enum class Searching
{
    by_sight,
    party,
    everyone
};

// Demonstration AI consumes only public state/commands. No rolls or damage here.
// Above Normal difficulty it targets the weakest enemies, and on Nightmare and
// Infernal finishes downed ones (CLOSE-1).
[[nodiscard]] rules::Command choose_demo_command(const rules::CombatSession &session,
                                                 Searching searching = Searching::by_sight,
                                                 Difficulty difficulty = default_difficulty);
// The same choice from a snapshot and the commands already read, so a caller
// that has them does not build them again.
[[nodiscard]] rules::Command choose_demo_command(const rules::Snapshot &state,
                                                 const std::vector<rules::Command> &offered,
                                                 Searching searching = Searching::by_sight,
                                                 Difficulty difficulty = default_difficulty);
// The party's flee policy: a member fast enough to flee runs for the nearest
// open edge of the field (Dashing when it must, whatever reactions it
// provokes) and steps off it; a member who cannot acts as choose_quick_command
// does, casting no spells unless `magic` (Quick magic), and a monster plays by
// `difficulty` as choose_demo_command does.
[[nodiscard]] rules::Command choose_flee_command(const rules::CombatSession &session,
                                                 bool magic = true,
                                                 Searching searching = Searching::by_sight,
                                                 Difficulty difficulty = default_difficulty);
// Quick combat, as in the original: the computer plays the active party
// member as choose_demo_command does, casting no spells unless `magic`. A Quick
// member plays at the highest difficulty whatever the setting: the setting
// makes the party's enemies harder, not its own members worse.
[[nodiscard]] rules::Command choose_quick_command(const rules::CombatSession &session, bool magic,
                                                  Searching searching = Searching::by_sight);
} // namespace opengold
#endif
