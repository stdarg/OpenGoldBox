// Play with Default Party: each game's own default party
// (data/default_parties, docs/DEFAULT-PARTIES.md), made by the game's creation
// rules under the original rules and under the SRD. The original rules read
// the game's files: Pool of Radiance's from OPENGOLD_GAME_DIR, Curse's and
// Secret's from OPENGOLD_GOLDBOX_DIR; a game whose files are not here is
// skipped. The SRD parties need no game files.
#include "opengold/campaign_party.h"
#include "opengold/character_art.h"
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
#include <optional>
#include <string>
#include <vector>

namespace
{
using opengold::GoldBoxGame;
using opengold::test::check;
using opengold::test::rejects;

const std::filesystem::path source_dir(OPENGOLD_SOURCE_DIR);

[[nodiscard]] std::vector<opengold::DefaultPartyMember> party_of(GoldBoxGame game)
{
    return opengold::load_default_party(source_dir / "data/default_parties" /
                                        (std::string(opengold::game_folder(game)) + ".json"));
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

[[nodiscard]] std::vector<opengold::MemberId> members_of(const opengold::CampaignParty &party)
{
    std::vector<opengold::MemberId> ids;
    for (const auto id : party.state().slots)
    {
        if (id)
        {
            ids.push_back(id);
        }
    }
    return ids;
}

// What every default party has: its six members in the file's order, with
// full Hit Points, their looks, their readied items in use, and money (gold,
// or Curse's platinum).
void check_party(const opengold::CampaignParty &party,
                 const std::vector<opengold::DefaultPartyMember> &file, const std::string &what,
                 const opengold::por::CharacterArt *art)
{
    const auto ids = members_of(party);
    check(ids.size() == 6, what + " has six members");
    for (std::size_t n = 0; n < ids.size(); ++n)
    {
        const auto &member = party.member(ids[n]);
        const auto &sheet = member.character.sheet();
        const auto &wanted = file[n];
        check(sheet.name == wanted.name, what + ": member " + std::to_string(n + 1) + " is " +
                                             wanted.name + ", not " + sheet.name);
        check(sheet.hit_points > 0 && member.vitals.hit_points == sheet.hit_points,
              what + ": " + sheet.name + " starts with full Hit Points");
        check(member.character.appearance().portrait_head == wanted.appearance.portrait_head,
              what + ": " + sheet.name + " has the file's portrait");
        if (art)
        {
            art->validate(member.character.appearance());
        }
        const auto &items = sheet.original ? wanted.original.items : wanted.srd.items;
        std::size_t readied = 0;
        for (const auto &item : items)
        {
            readied += item.readied ? 1 : 0;
        }
        check(member.character.inventory().items().size() == items.size(),
              what + ": " + sheet.name + " carries the file's items");
        check(member.equipped.size() == readied,
              what + ": " + sheet.name + " has its readied items in use: " +
                  std::to_string(member.equipped.size()) + " of " + std::to_string(readied));
    }
    const auto &purse = party.state().wealth;
    check(purse[3] > 0 || purse[4] > 0, what + " brings its money");
}

// Under the original rules each game's creation sets the starting level:
// level 1 in Pool of Radiance, trained up from 25,000 experience in Curse and
// further in Secret; the casters have their spells memorized.
void original_party(GoldBoxGame game, const opengold::por::CharacterArt *art)
{
    const auto folder = developer_folder(game);
    if (folder.empty())
    {
        std::cout << opengold::game_title(game)
                  << "'s files are not here; its original party is not made.\n";
        return;
    }
    const auto file = party_of(game);
    opengold::CampaignParty party(
        opengold::original::rules_module(opengold::load_original_content(folder, game)),
        opengold::original::character_rules(opengold::original::default_aging, game));
    opengold::add_default_party(party, file);
    const auto what = std::string(opengold::game_title(game)) + "'s original party";
    check_party(party, file, what, art);
    const auto ids = members_of(party);
    for (std::size_t n = 0; n < ids.size(); ++n)
    {
        const auto &member = party.member(ids[n]);
        const auto &sheet = member.character.sheet();
        check(sheet.original.has_value(), what + ": " + sheet.name + " is an original character");
        check(game == GoldBoxGame::pool_of_radiance ? sheet.level == 1 && member.experience == 0
                                                    : sheet.level >= 4 && member.experience > 0,
              what + ": " + sheet.name + " starts at the game's own level");
        check(sheet.prepared_spells.size() == file[n].original.spells.size(),
              what + ": " + sheet.name + " has its spells memorized");
    }
}

// Under the SRD the same six people are level-1 characters of the nearest
// species and class.
void srd_party(GoldBoxGame game, const opengold::por::CharacterArt *art)
{
    const auto file = party_of(game);
    opengold::CampaignParty party(
        opengold::srd5::load(source_dir / "data/rules/srd-5.2.1/combat.rules"),
        opengold::srd5::character_rules());
    opengold::add_default_party(party, file);
    const auto what = std::string(opengold::game_title(game)) + "'s SRD party";
    check_party(party, file, what, art);
    const auto ids = members_of(party);
    for (std::size_t n = 0; n < ids.size(); ++n)
    {
        const auto &character = party.member(ids[n]).character;
        const auto &sheet = character.sheet();
        check(!sheet.original && sheet.level == 1, what + ": " + sheet.name + " is level 1");
        // Each listed spell is a cantrip it knows, one it learned or one it
        // prepared.
        const auto &draft = character.creation_data();
        for (const auto &spell : file[n].srd.character.spells)
        {
            bool found = draft.cantrips &&
                         std::ranges::find(*draft.cantrips, spell) != draft.cantrips->end();
            found = found ||
                    std::ranges::find(sheet.prepared_spells, spell) != sheet.prepared_spells.end();
            if (draft.spells)
            {
                for (const auto &[group, picks] : draft.spells->learning)
                {
                    found = found || std::ranges::find(picks, spell) != picks.end();
                }
            }
            check(found, what + ": " + sheet.name + " has " + spell);
        }
    }
}

// A member the rules refuse adds no one.
void a_refused_member_adds_no_one()
{
    auto file = party_of(GoldBoxGame::pool_of_radiance);
    file[5].srd.character.character_class = "no_such_class";
    opengold::CampaignParty party(
        opengold::srd5::load(source_dir / "data/rules/srd-5.2.1/combat.rules"),
        opengold::srd5::character_rules());
    rejects(
        [&]
        {
            opengold::add_default_party(party, file);
        },
        "An unknown class is refused");
    check(members_of(party).empty(), "A refused default party adds no one");
}

// A file the game cannot use names what is wrong.
void an_unusable_file_is_refused()
{
    rejects(
        [&]
        {
            (void)opengold::load_default_party(source_dir / "data/default_parties/NONE.json");
        },
        "A missing file is refused");
}
} // namespace

int main()
{
    try
    {
        std::optional<opengold::por::CharacterArt> art;
        if (const auto pool = developer_folder(GoldBoxGame::pool_of_radiance); !pool.empty())
        {
            art = opengold::por::CharacterArt::load(pool);
        }
        for (const auto game :
             {GoldBoxGame::pool_of_radiance, GoldBoxGame::curse_of_the_azure_bonds,
              GoldBoxGame::secret_of_the_silver_blades})
        {
            srd_party(game, art ? &*art : nullptr);
            original_party(game, art ? &*art : nullptr);
        }
        a_refused_member_adds_no_one();
        an_unusable_file_is_refused();
        std::cout << "Default party tests passed\n";
        return 0;
    }
    catch (const std::exception &failure)
    {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
