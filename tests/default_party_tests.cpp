// Play with Default Party: each game's own default party
// (data/default_parties, docs/DEFAULT-PARTIES.md), made by the game's creation
// rules under the original rules and under the SRD. The original rules read
// the game's files, each game's folder found by its files
// (installed_games.h); a game whose files are not here is skipped. The SRD parties need no game
// files; Gateway, Pools of Darkness and Treasures have none yet.
#include "opengold/campaign_party.h"
#include "opengold/character_art.h"
#include "opengold/default_party.h"
#include "opengold/game_census.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/original_content.h"
#include "opengold/original_spells.h"
#include "opengold/srd5.h"
#include "opengold/ua_design.h"
#include "opengold/ua_session.h"
#include "installed_games.h"
#include "test_support.h"
#include <algorithm>
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
    return opengold::test::installed_game_folder(game);
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
        const auto &items = sheet.original ? wanted.original.items : wanted.srd->items;
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

// The experience each game's creation starts a single-class character with.
[[nodiscard]] unsigned starting_experience(GoldBoxGame game)
{
    switch (game)
    {
    case GoldBoxGame::curse_of_the_azure_bonds:
        return 25000;
    case GoldBoxGame::secret_of_the_silver_blades:
        return 200000;
    case GoldBoxGame::gateway_to_the_savage_frontier:
        return 3000;
    case GoldBoxGame::pools_of_darkness:
        return 1500001;
    case GoldBoxGame::treasures_of_the_savage_frontier:
        return 22600;
    default:
        return 0;
    }
}

// Under the original rules each game's creation sets the starting level:
// level 1 in Pool of Radiance, trained up from each later game's starting
// experience (Gateway's 3,000 to Pools of Darkness's 1,500,001); the casters
// have their spells memorized, and Treasures' magic-user knows the spells
// chosen on its creation's page. Pools of Darkness's creation then gives each
// its equipment, as the game does.
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
        // Every member after Pool of Radiance's is of one class.
        check(game == GoldBoxGame::pool_of_radiance
                  ? sheet.level == 1 && member.experience == 0
                  : sheet.level >= 2 && member.experience == starting_experience(game),
              what + ": " + sheet.name + " starts at the game's own level, not level " +
                  std::to_string(sheet.level) + " with " + std::to_string(member.experience));
        check(sheet.prepared_spells.size() == file[n].original.spells.size(),
              what + ": " + sheet.name + " has its spells memorized");
        for (const auto &spell : file[n].original.learned)
        {
            const auto &known = sheet.original->known_spells;
            const auto number = opengold::original::spell_number(spell);
            check(number && std::ranges::find(known, *number) != known.end(),
                  what + ": " + sheet.name + " knows " + spell + ", chosen at creation");
        }
    }
    if (game != GoldBoxGame::pools_of_darkness)
    {
        return;
    }
    const auto definitions = opengold::load_item_definitions(folder, game);
    for (const auto id : ids)
    {
        opengold::outfit_original_character(party, id, definitions);
        check(!party.member(id).equipped.empty(), what + ": " +
                                                      party.member(id).character.sheet().name +
                                                      " readies its creation's equipment");
    }
}

// A game whose SRD party is not made yet refuses the SRD rules, naming why,
// and adds no one.
void srd_party_not_made(GoldBoxGame game)
{
    const auto file = party_of(game);
    opengold::CampaignParty party(
        opengold::srd5::load(source_dir / "data/rules/srd-5.2.1/combat.rules"),
        opengold::srd5::character_rules());
    rejects(
        [&]
        {
            opengold::add_default_party(party, file);
        },
        std::string(opengold::game_title(game)) + "'s default party has no SRD characters");
    check(members_of(party).empty(),
          std::string(opengold::game_title(game)) + ": a refused SRD party adds no one");
}

// Treasures' magic-user must list as many spells as its creation's page asks.
void treasures_spell_choice_is_whole()
{
    const auto folder = developer_folder(GoldBoxGame::treasures_of_the_savage_frontier);
    if (folder.empty())
    {
        return;
    }
    auto file = party_of(GoldBoxGame::treasures_of_the_savage_frontier);
    auto mage = std::ranges::find(file, std::string("magic_user"),
                                  [](const opengold::DefaultPartyMember &member)
                                  {
                                      return member.original.character_class;
                                  });
    check(mage != file.end() && !mage->original.learned.empty(),
          "Treasures' party has a magic-user who chooses spells");
    mage->original.learned.pop_back();
    opengold::CampaignParty party(
        opengold::original::rules_module(
            opengold::load_original_content(folder, GoldBoxGame::treasures_of_the_savage_frontier)),
        opengold::original::character_rules(opengold::original::default_aging,
                                            GoldBoxGame::treasures_of_the_savage_frontier));
    rejects(
        [&]
        {
            opengold::add_default_party(party, file);
        },
        "A Treasures magic-user short of its creation's spell choices is refused");
}

// Unlimited Adventures' party plays a design, here the kit's own templates:
// made under the kit's rules with the design's start, then given its gems,
// jewelry and set of equipment as every new character of the design is
// (ua::outfit_new_character), its casters' first-level spells memorized.
void ua_original_party(const opengold::por::CharacterArt *art)
{
    const auto folder = developer_folder(GoldBoxGame::unlimited_adventures);
    if (folder.empty() || !opengold::ua::is_kit_folder(folder))
    {
        std::cout << "Unlimited Adventures' kit is not here; its original party is not made.\n";
        return;
    }
    const opengold::ua::DesignFiles files(folder, {});
    const auto file = party_of(GoldBoxGame::unlimited_adventures);
    opengold::CampaignParty party(
        opengold::original::rules_module(opengold::ua::load_design_content(files)),
        opengold::original::character_rules(opengold::original::default_aging,
                                            GoldBoxGame::unlimited_adventures));
    opengold::add_default_party(party, file);
    const std::string what = "Unlimited Adventures' original party";
    check_party(party, file, what, art);
    for (std::size_t n = 0; n < file.size(); ++n)
    {
        const auto id = members_of(party)[n];
        const auto &sheet = party.member(id).character.sheet();
        check(sheet.original && sheet.level >= 4,
              what + ": " + sheet.name + " starts at the design's level");
        check(sheet.prepared_spells.size() == file[n].original.spells.size(),
              what + ": " + sheet.name + " has its spells memorized");
        opengold::ua::outfit_new_character(party, id, files);
    }
    check(!party.member(members_of(party)[1]).character.inventory().items().empty(),
          what + ": the design's equipment arms its fighter");
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
        for (const auto &spell : file[n].srd->character.spells)
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
    file[5].srd->character.character_class = "no_such_class";
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
        for (const auto game :
             {GoldBoxGame::gateway_to_the_savage_frontier, GoldBoxGame::pools_of_darkness,
              GoldBoxGame::treasures_of_the_savage_frontier})
        {
            srd_party_not_made(game);
            original_party(game, art ? &*art : nullptr);
        }
        treasures_spell_choice_is_whole();
        srd_party(GoldBoxGame::unlimited_adventures, art ? &*art : nullptr);
        ua_original_party(art ? &*art : nullptr);
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
