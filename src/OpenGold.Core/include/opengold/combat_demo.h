#ifndef OPENGOLD_COMBAT_DEMO_H
#define OPENGOLD_COMBAT_DEMO_H
#include "opengold/campaign_encounter.h"
#include "opengold/rules.h"
#include "opengold/ecl_machine.h"
#include "opengold/creature_catalog.h"
#include "opengold/formats.h"
#include "opengold/campaign_party.h"
#include "opengold/dungeon_battlefield.h"
#include <filesystem>
#include <string>
#include <vector>

namespace opengold
{
// A bounded demonstration/campaign adapter. It depends on the rules interface,
// never on a specific edition. The application supplies the selected module.
class CombatDemo
{
  public:
    explicit CombatDemo(std::unique_ptr<rules::RulesModule> module);
    ~CombatDemo();
    void campaign_party(std::shared_ptr<CampaignParty> party);
    void training(std::uint64_t seed = 42, bool conditions = false);
    void slums(const std::filesystem::path &game_directory, std::uint64_t seed = 42);
    void encounter(CampaignEncounter encounter, std::uint64_t seed);

    [[nodiscard]] const auto &battlefield_tiles() const
    {
        return battlefield_tiles_;
    }

    [[nodiscard]] const auto &terrain_art() const
    {
        return terrain_art_;
    }

    [[nodiscard]] const rules::CombatSession &combat() const;
    bool submit(const rules::Command &command);
    void continue_script();
    void revisit();
    [[nodiscard]] std::string save_combat() const;
    void restore_combat(std::string_view checkpoint);

    [[nodiscard]] const std::string &dialogue() const noexcept
    {
        return dialogue_;
    }

    [[nodiscard]] const std::string &status() const noexcept
    {
        return status_;
    }

    [[nodiscard]] bool waiting() const noexcept
    {
        return menu_ticket_ != 0;
    }

    [[nodiscard]] bool has_combat() const noexcept
    {
        return combat_ != nullptr;
    }

    [[nodiscard]] bool is_slums() const noexcept
    {
        return vm_.has_value();
    }

    [[nodiscard]] bool script_complete() const noexcept
    {
        return vm_ && vm_->state() == por::EclState::completed;
    }

    [[nodiscard]] unsigned script_variable(por::EclAddress address) const;

    [[nodiscard]] const auto &art() const noexcept
    {
        return art_;
    }

  private:
    class CampaignCombat
    {
      public:
        explicit CampaignCombat(std::shared_ptr<CampaignParty> party);
        ~CampaignCombat();
        CampaignCombat(const CampaignCombat &) = delete;
        CampaignCombat &operator=(const CampaignCombat &) = delete;
        CampaignCombat(CampaignCombat &&) noexcept = default;
        CampaignCombat &operator=(CampaignCombat &&) = delete;

      private:
        std::shared_ptr<CampaignParty> party_;
    };

    std::unique_ptr<rules::RulesModule> module_;
    std::unique_ptr<rules::CombatSession> combat_;
    std::shared_ptr<CampaignParty> campaign_;
    std::optional<CampaignCombat> campaign_combat_;
    void install_combat(std::unique_ptr<rules::CombatSession> next, std::string reward_id);
    void finish_campaign_combat(rules::Outcome outcome);
    void start_encounter(std::vector<rules::Participant> enemies, std::string reward_id);
    void synchronize_party();
    std::optional<por::EclMachine> vm_;
    std::optional<por::CreatureCatalog> creatures_;
    std::vector<rules::Participant> enemies_;
    std::vector<CombatArt> art_;
    std::vector<std::uint8_t> battlefield_tiles_;
    std::vector<Image> terrain_art_;
    std::filesystem::path game_directory_;
    std::string dialogue_, status_;
    std::uint64_t menu_ticket_{}, combat_ticket_{}, seed_{};
    unsigned encounters_{};
    std::string reward_id_;
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
};
[[nodiscard]] CombatDemoSetup make_combat_demo(std::unique_ptr<rules::RulesModule> rules,
        const rules::CharacterRules &characters,
        const std::filesystem::path &game_directory,
        const CombatDemoOptions &options = {});
// Demonstration AI consumes only public state/commands. No rolls or damage here.
[[nodiscard]] rules::Command choose_demo_command(const rules::CombatSession &session);
// The party's flee policy: a member fast enough to flee runs for the nearest
// open edge of the field (Dashing when it must, whatever reactions it
// provokes) and steps off it; everyone else acts as choose_demo_command does.
[[nodiscard]] rules::Command choose_flee_command(const rules::CombatSession &session);
// Quick combat, as in the original: the computer plays the active party
// member as choose_demo_command does, casting no spells unless `magic`.
[[nodiscard]] rules::Command choose_quick_command(const rules::CombatSession &session, bool magic);
} // namespace opengold
#endif
