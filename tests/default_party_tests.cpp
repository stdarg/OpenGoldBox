// Play with Default Party: each original game's pre-rolled party, its saved
// game A, read from the player's game folder (opengold::add_default_party).
// The real files are read when OPENGOLD_GAME_DIR holds Pool of Radiance and
// OPENGOLD_GOLDBOX_DIR the CURSE and SECRET folders; otherwise those checks
// are skipped.
#include "opengold/campaign_party.h"
#include "opengold/default_party.h"
#include "opengold/game_census.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/original_content.h"
#include "opengold/srd5.h"
#include "test_support.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
using opengold::GoldBoxGame;
using opengold::test::check;
using opengold::test::rejects;

[[nodiscard]] std::unique_ptr<opengold::CampaignParty>
original_party(const std::filesystem::path &folder, GoldBoxGame game)
{
    return std::make_unique<opengold::CampaignParty>(
        opengold::original::rules_module(opengold::load_original_content(folder, game)),
        opengold::original::character_rules(opengold::original::default_aging, game));
}

[[nodiscard]] std::vector<std::string> member_names(const opengold::CampaignParty &party)
{
    std::vector<std::string> names;
    for (const auto id : party.state().slots)
    {
        if (id)
        {
            names.push_back(party.member(id).character.sheet().name);
        }
    }
    return names;
}

// The folder of `game` the developer has, or empty.
[[nodiscard]] std::filesystem::path developer_folder(GoldBoxGame game)
{
    const char *variable =
        game == GoldBoxGame::pool_of_radiance ? "OPENGOLD_GAME_DIR" : "OPENGOLD_GOLDBOX_DIR";
    const char *value = std::getenv(variable);
    if (!value)
    {
        return {};
    }
    auto folder = std::filesystem::path(value);
    if (game != GoldBoxGame::pool_of_radiance)
    {
        folder /= std::string(opengold::game_folder(game));
    }
    return opengold::identify_game(folder) == game ? folder : std::filesystem::path();
}

// Pool of Radiance's saved game A: six level-one characters with their
// items, the weapons and armor they readied in hand and worn, and their money.
void pool_of_radiance_default_party(const std::filesystem::path &folder)
{
    constexpr auto pool = GoldBoxGame::pool_of_radiance;
    auto party = original_party(folder, pool);
    opengold::add_default_party(*party, pool, folder);
    const auto names = member_names(*party);
    check(names ==
              std::vector<std::string>{"Dramene", "Willow", "Liza", "Vaeril", "Thanes", "Senna"},
          "Pool of Radiance's saved game A holds its six characters in order");
    std::size_t items = 0, readied = 0;
    for (const auto id : party->state().slots)
    {
        if (!id)
        {
            continue;
        }
        const auto &member = party->member(id);
        check(member.character.sheet().original.has_value(),
              member.character.sheet().name + " plays under the original rules");
        check(member.vitals.hit_points == member.character.sheet().hit_points &&
                  member.character.sheet().hit_points > 0,
              member.character.sheet().name + " starts with full Hit Points");
        items += member.character.inventory().items().size();
        readied += member.equipped.size();
    }
    check(items == (630 + 630 + 441 + 693 + 504 + 756) / 63,
          "Every item of the six .ITM files is carried: " + std::to_string(items));
    check(readied > 6, "The items each readied are readied: " + std::to_string(readied));
    const auto &purse = party->state().wealth;
    check(std::any_of(purse.begin(), purse.end(),
                      [](auto coins)
                      {
                          return coins > 0;
                      }),
          "The characters' money is in the party's purse");
}

// Curse's and Secret's saved game A: six characters each, level 4 and more,
// with the experience they have; their saves hold no items.
void later_default_party(const std::filesystem::path &folder, GoldBoxGame game,
                         const std::vector<std::string> &expected)
{
    auto party = original_party(folder, game);
    opengold::add_default_party(*party, game, folder);
    check(member_names(*party) == expected,
          std::string(opengold::game_title(game)) + "'s saved game A holds its six characters");
    for (const auto id : party->state().slots)
    {
        if (id)
        {
            const auto &member = party->member(id);
            check(member.experience > 0 && member.character.sheet().level >= 4,
                  member.character.sheet().name + " keeps its experience and levels");
        }
    }
}

// Rules that take no original record (the SRD's) add no one, and say so.
void the_srd_takes_no_default_party(const std::filesystem::path &folder)
{
    opengold::CampaignParty party(opengold::srd5::load(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                                                       "data/rules/srd-5.2.1/combat.rules"),
                                  opengold::srd5::character_rules());
    rejects(
        [&]
        {
            opengold::add_default_party(party, GoldBoxGame::pool_of_radiance, folder);
        },
        "The SRD's rules take no default party");
    check(member_names(party).empty(), "The SRD's party stays empty");
}

// A folder without a saved game A adds no one, and says so.
void a_missing_saved_game_adds_no_one(const std::filesystem::path &pool_folder)
{
    const auto folder = std::filesystem::temp_directory_path() / "opengold-default-party";
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder / "SAVE");
    auto party = original_party(pool_folder, GoldBoxGame::pool_of_radiance);
    rejects(
        [&]
        {
            opengold::add_default_party(*party, GoldBoxGame::pool_of_radiance, folder);
        },
        "A SAVE folder without SAVGAMA.DAT is refused");
    std::filesystem::remove_all(folder / "SAVE");
    rejects(
        [&]
        {
            opengold::add_default_party(*party, GoldBoxGame::pool_of_radiance, folder);
        },
        "A game folder without a SAVE folder is refused");
    check(member_names(*party).empty(), "A refused default party adds no one");
    std::filesystem::remove_all(folder);
}
} // namespace

int main()
{
    try
    {
        const auto pool = developer_folder(GoldBoxGame::pool_of_radiance);
        if (pool.empty())
        {
            std::cout << "Skipped: OPENGOLD_GAME_DIR does not hold Pool of Radiance\n";
            return 0;
        }
        pool_of_radiance_default_party(pool);
        the_srd_takes_no_default_party(pool);
        a_missing_saved_game_adds_no_one(pool);
        if (const auto curse = developer_folder(GoldBoxGame::curse_of_the_azure_bonds);
            !curse.empty())
        {
            later_default_party(curse, GoldBoxGame::curse_of_the_azure_bonds,
                                {"Mathew", "Mark", "Travis", "Ledera", "Shara", "Philippe"});
        }
        if (const auto secret = developer_folder(GoldBoxGame::secret_of_the_silver_blades);
            !secret.empty())
        {
            later_default_party(
                secret, GoldBoxGame::secret_of_the_silver_blades,
                {"Guy De Valois", "Paine", "Epona", "Malachite", "Dominic", "Morgaine"});
        }
        std::cout << "Default party tests passed\n";
        return 0;
    }
    catch (const std::exception &failure)
    {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
