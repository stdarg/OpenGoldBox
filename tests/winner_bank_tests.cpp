// The winner bank (docs/SAVES.md, The winning party): a game's party is
// banked at its win as an ordinary save that reloads byte for byte, and the
// series' next game brings in its winners, its permanent NPCs among them.
#include "opengold/buck_rogers_characters.h"
#include "opengold/buck_rogers_combat.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/campaign_session.h"
#include "opengold/character_creator.h"
#include "opengold/creature_catalog.h"
#include "opengold/default_party.h"
#include "opengold/original.h"
#include "opengold/original_content.h"
#include "opengold/pool_radiance.h"
#include "opengold/winner_bank.h"
#include "installed_games.h"
#include "original_creation_support.h"
#include "test_support.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using opengold::GoldBoxGame;
using opengold::MemberId;
using opengold::test::check;
namespace og = opengold::original;

const std::filesystem::path source_dir(OPENGOLD_SOURCE_DIR);

// A game played under the original rules, from its installed folder.
struct Game
{
    GoldBoxGame game{};
    std::filesystem::path folder;
    std::shared_ptr<const opengold::rules::CharacterRules> creation;

    [[nodiscard]] std::shared_ptr<opengold::CampaignParty> party() const
    {
        return std::make_shared<opengold::CampaignParty>(
            og::rules_module(opengold::load_original_content(folder, game)), creation);
    }

    [[nodiscard]] std::string assets() const
    {
        return opengold::campaign_asset_identity(folder);
    }
};

std::optional<Game> installed(GoldBoxGame game)
{
    const auto folder = opengold::test::installed_game_folder(game);
    if (folder.empty())
    {
        std::cout << opengold::game_title(game) << "'s files are not here.\n";
        return std::nullopt;
    }
    return Game{game, folder, og::character_rules(og::default_aging, game)};
}

// The member in `party` named `name`.
const opengold::PartyMember &named(const opengold::PartyState &party, std::string_view name)
{
    const auto found = std::ranges::find_if(party.roster,
                                            [&](const opengold::PartyMember &member)
                                            {
                                                return member.character.sheet().name == name;
                                            });
    check(found != party.roster.end(), "The party has " + std::string(name));
    return *found;
}

const opengold::PartyMember &member_of(const opengold::PartyState &party, MemberId id)
{
    return *std::ranges::find(party.roster, id, &opengold::PartyMember::id);
}

// The NPC creature record `record` of `bank`, joined as ADD NPC joins it
// under the original rules: its record, its morale and its own items.
MemberId join_npc(opengold::CampaignParty &party, const Game &game, unsigned bank, unsigned record)
{
    const auto catalog = opengold::por::CreatureCatalog::load(game.folder, game.game);
    const auto found =
        catalog.find({static_cast<std::uint8_t>(bank), static_cast<std::uint8_t>(record)});
    check(found.has_value(), "The game has its NPC's creature record");
    const auto &creature = found->get();
    auto bytes = creature.stored.raw;
    if (game.game == GoldBoxGame::death_knights_of_krynn)
    {
        bytes.insert(bytes.end(), creature.stored.raw_spell_block.begin(),
                     creature.stored.raw_spell_block.end());
    }
    const auto draft = game.creation->record_draft(bytes);
    check(draft.has_value(), "The NPC's record makes a character");
    const auto id =
        party.recruit("por:MON" + std::to_string(bank) + "CHA:" + std::to_string(record),
                      opengold::Character(*game.creation, *draft, {}), 90);
    for (const auto &item : creature.equipment)
    {
        party.take_item(id, item);
    }
    return id;
}

// Player characters made by the game's creation rules.
void make_player_characters(opengold::CampaignParty &party, const Game &game, unsigned count = 2)
{
    const bool krynn = og::krynn_game(game.game);
    for (unsigned index = 0; index < count; ++index)
    {
        opengold::CharacterCreator creator(game.creation, 40 + party.state().roster.size());
        creator.select(opengold::rules::CreationField::race, krynn ? "krynn_human" : "human");
        creator.select(opengold::rules::CreationField::character_class, "fighter");
        creator.name("Winner " + std::to_string(party.state().roster.size() + 1));
        opengold::test::walk_original_creation(creator);
        (void)party.add_pc(creator.create_character());
    }
}

// A party at its win: its default party's characters where the game has one
// (data/default_parties), else two of its creation's, and an NPC of its own
// records in an NPC position; with money to share.
std::shared_ptr<opengold::CampaignParty> winning_party(const Game &game, unsigned npc_bank,
                                                       unsigned npc_record)
{
    auto party = game.party();
    const auto defaults = source_dir / "data/default_parties" /
                          (std::string(opengold::game_folder(game.game)) + ".json");
    if (std::filesystem::exists(defaults))
    {
        opengold::add_default_party(*party, opengold::load_default_party(defaults));
    }
    else
    {
        make_player_characters(*party, game);
    }
    (void)join_npc(*party, game, npc_bank, npc_record);
    auto wealth = party->state().wealth;
    wealth[static_cast<std::size_t>(opengold::Coin::platinum)] = 600;
    wealth[static_cast<std::size_t>(opengold::Coin::gems)] = 12;
    wealth[static_cast<std::size_t>(opengold::Coin::gold)] = 3000;
    party->set_wealth(wealth);
    return party;
}

// Diagnostic availability fixtures use legal created PCs and actual NPC records;
// they are not earned campaign wins. Curse's two companions fill both NPC slots.
std::shared_ptr<opengold::CampaignParty> full_winning_party(const Game &game, unsigned bank,
                                                            unsigned record)
{
    auto party = winning_party(game, bank, record);
    const auto pcs =
        static_cast<unsigned>(std::ranges::count_if(party->state().roster,
                                                    [](const auto &member)
                                                    {
                                                        return member.npc_source.empty();
                                                    }));
    check(pcs <= 6, "The source fixture fits the six PC positions");
    make_player_characters(*party, game, 6 - pcs);
    if (game.game == GoldBoxGame::curse_of_the_azure_bonds)
    {
        const auto catalog = opengold::por::CreatureCatalog::load(game.folder, game.game);
        const auto dragonbait = catalog.find_by_name("Dragonbait");
        check(!dragonbait.empty(), "Curse has its actual Dragonbait record");
        const auto id = dragonbait.front().get().id;
        (void)join_npc(*party, game, id.bank, id.record);
    }
    return party;
}

// The bank's save decoded under its game's rules, after checking that it
// reloads and saves again byte for byte.
opengold::SavedCampaign reloaded_bank(const Game &game, const std::string &bytes)
{
    auto world = [&]
    {
        return opengold::load_campaign(game.folder, game.game);
    };
    auto saved = opengold::decode_campaign(
        bytes, *game.creation,
        *og::rules_module(opengold::load_original_content(game.folder, game.game)), game.assets(),
        world);
    auto again = game.party();
    again->restore(saved.party);
    std::optional<opengold::por::RolfTourSession> town;
    if (saved.town)
    {
        town = std::move(*saved.town);
        town->attach_restored_party(again);
    }
    check(opengold::encode_campaign(*again, town ? &*town : nullptr, game.assets(), game.game) ==
              bytes,
          std::string(opengold::game_title(game.game)) +
              "'s winning party reloads and saves again byte for byte");
    saved.party = again->state();
    saved.town = std::move(town);
    return saved;
}

// Brings every winner of `bank` into a new party of `sequel`, as its party
// screen does, and checks each arrives where its kind belongs. Returns the
// sequel's party.
std::shared_ptr<opengold::CampaignParty> bring_winners(const Game &earlier, const Game &sequel,
                                                       const opengold::PartyState &bank)
{
    const auto winners = opengold::winners(bank);
    const auto player_characters = static_cast<std::size_t>(
        std::ranges::count_if(winners,
                              [&](MemberId id)
                              {
                                  return member_of(bank, id).npc_source.empty();
                              }));
    auto party = sequel.party();
    std::size_t place = 0;
    for (const auto id : winners)
    {
        const auto &from = member_of(bank, id);
        const bool npc = !from.npc_source.empty();
        const auto arrived = opengold::bring_into_sequel(*party, from,
                                                         {.earlier = earlier.game,
                                                          .earlier_folder = earlier.folder,
                                                          .sequel_folder = sequel.folder,
                                                          .source_wealth = bank.wealth,
                                                          .player_character = place,
                                                          .player_characters = player_characters,
                                                          .gateway_money_kept = true});
        place += npc ? 0 : 1;
        const auto &member = party->member(arrived);
        const auto position =
            std::ranges::find(party->state().slots, arrived) - party->state().slots.begin();
        check(member.character.sheet().name == from.character.sheet().name,
              "A winner arrives under its own name");
        check(npc ? position >= 6 &&
                        member.npc_source ==
                            "from:" + std::string(opengold::game_folder(earlier.game)) + ":" +
                                from.npc_source
                  : position < 6 && member.npc_source.empty(),
              "A player character takes a PC position and a permanent NPC an NPC position, "
              "as its game's NPC");
        check(og::game_of(member.character.sheet().identity) == sequel.game,
              "The winner arrives under the sequel's rules");
        check(member.experience == from.experience || sequel.game == GoldBoxGame::pools_of_darkness,
              "The winner keeps its experience");
    }
    check(winners.size() == bank.roster.size(),
          "Every member of the winning party is a winner and comes across");
    // The sequel's game saves and reloads them as any party.
    const auto bytes = opengold::encode_campaign(*party, nullptr, sequel.assets(), sequel.game);
    auto saved = opengold::decode_campaign(bytes, *sequel.creation, party->rule_module(),
                                           sequel.assets(), nullptr);
    auto again = sequel.party();
    again->restore(std::move(saved.party));
    check(opengold::encode_campaign(*again, nullptr, sequel.assets(), sequel.game) == bytes,
          std::string(opengold::game_title(sequel.game)) +
              "'s party of winners saves and reloads byte for byte");
    std::cout << opengold::game_title(earlier.game) << " -> " << opengold::game_title(sequel.game)
              << ": " << winners.size() << " winners came across, "
              << winners.size() - player_characters << " of them NPCs.\n";
    return party;
}

void every_occupied_winner_is_available()
{
    const auto curse = installed(GoldBoxGame::curse_of_the_azure_bonds);
    const auto secret = installed(GoldBoxGame::secret_of_the_silver_blades);
    if (!curse || !secret)
    {
        return;
    }
    const auto party = full_winning_party(*curse, 3, 22);
    auto state = party->state();
    const auto pc = state.slots[0], npc = state.slots[6];
    auto &dead_pc = *std::ranges::find(state.roster, pc, &opengold::PartyMember::id);
    dead_pc.vitals = {0, true, {}};
    party->restore(state);
    const auto bank = reloaded_bank(
        *curse, opengold::encode_campaign(*party, nullptr, curse->assets(), curse->game));
    check(
        member_of(bank.party, pc).vitals.dead &&
            opengold::winners(bank.party) ==
                std::vector<MemberId>(state.slots.begin(), state.slots.end()),
        "All six occupied PCs, including a dead PC, and both living NPCs are offered in slot order");
    const auto sequel = bring_winners(*curse, *secret, bank.party);
    check(sequel->state().slots[5] && sequel->state().slots[6] && sequel->state().slots[7],
          "The complete six-PC/two-NPC winning roster fits the sequel's actual positions");

    auto fallen = bank.party;
    auto &dead_npc = *std::ranges::find(fallen.roster, npc, &opengold::PartyMember::id);
    dead_npc.vitals = {0, true, {}};
    const auto living = opengold::winners(fallen);
    check(living.size() == 7 && std::ranges::find(living, npc) == living.end() &&
              std::ranges::find(living, pc) != living.end(),
          "A fallen NPC is excluded while the occupied dead PC is still available");

    party->remove(npc);
    const auto historical = reloaded_bank(
        *curse, opengold::encode_campaign(*party, nullptr, curse->assets(), curse->game));
    const auto offered = opengold::winners(historical.party);
    check(historical.party.roster.size() == 8 && offered.size() == 7 &&
              std::ranges::find(offered, npc) == offered.end(),
          "A departed NPC remains in saved history but is not offered as a winner");
}

// The capture's moments: the party alone at the first moment the game is
// won, the party with its world once the game is idle again, and nothing
// banked twice; winners changed after the win leave the party alone banked.
void the_capture_follows_the_win()
{
    const auto curse = installed(GoldBoxGame::curse_of_the_azure_bonds);
    if (!curse)
    {
        return;
    }
    const auto party = winning_party(*curse, 3, 22);
    const auto at_win = opengold::encode_campaign(*party, nullptr, curse->assets(), curse->game);
    unsigned alone = 0, with_world = 0;
    const opengold::WinnerCapture::Saves saves{
        [&]
        {
            ++alone;
            return opengold::encode_campaign(*party, nullptr, curse->assets(), curse->game);
        },
        [&]
        {
            ++with_world;
            return std::string("world");
        }};
    opengold::WinnerCapture capture;
    check(!capture.observe(false, true, party->state(), saves), "Nothing is banked before a win");
    check(capture.observe(true, false, party->state(), saves) == at_win,
          "The win banks the party alone, before Continue");
    check(!capture.observe(true, false, party->state(), saves) &&
              !capture.observe(false, false, party->state(), saves),
          "The win is banked once while its scene plays on");
    check(capture.observe(false, true, party->state(), saves) == "world",
          "The game idle again banks the party with its world");
    check(!capture.observe(false, true, party->state(), saves) && alone == 2 && with_world == 1,
          "Nothing more is banked until the next win");

    opengold::WinnerCapture departed;
    check(departed.observe(true, false, party->state(), saves) == at_win,
          "Another win banks its party alone");
    const auto npc = party->state().slots[6];
    party->remove(npc);
    check(!departed.observe(false, true, party->state(), saves),
          "A winner gone after the win leaves the party banked at the win");

    opengold::WinnerCapture idle;
    check(idle.observe(true, true, party->state(), saves) == "world",
          "A game won while idle (Matrix Cubed's) banks its world at once");
    opengold::WinnerCapture loaded(true);
    check(!loaded.observe(true, true, party->state(), saves),
          "A game loaded already won is not banked again");

    // Its permanent NPC is a winner; one fallen is not.
    const auto restored = winning_party(*curse, 3, 22);
    check(opengold::winners(restored->state()).size() == restored->state().roster.size(),
          "Every PC and the living NPC of the party are its winners");
    auto fallen = restored->state();
    const auto npc_id = fallen.slots[6];
    std::ranges::find(fallen.roster, npc_id, &opengold::PartyMember::id)->vitals.dead = true;
    check(opengold::winners(fallen).size() == fallen.roster.size() - 1,
          "An NPC fallen at the win is no winner");
    std::cout << "The capture follows the win.\n";
}

// Encoders can fail without marking an unencoded capture as already done.
void failed_encoders_can_retry()
{
    const opengold::PartyState party;
    bool fail_party = true, fail_world = true;
    const opengold::WinnerCapture::Saves saves{
        [&]
        {
            if (fail_party)
            {
                throw std::runtime_error("party encoder failed");
            }
            return std::string("exact party");
        },
        [&]
        {
            if (fail_world)
            {
                throw std::runtime_error("world encoder failed");
            }
            return std::string("exact world");
        }};
    opengold::WinnerCapture capture;
    bool failed = false;
    try
    {
        (void)capture.observe(true, false, party, saves);
    }
    catch (const std::runtime_error &)
    {
        failed = true;
    }
    check(failed, "The initial encoder failure reaches the host");
    fail_party = false;
    check(capture.observe(true, false, party, saves) == "exact party",
          "A failed party encoder has not consumed the win");
    failed = false;
    try
    {
        (void)capture.observe(true, true, party, saves);
    }
    catch (const std::runtime_error &)
    {
        failed = true;
    }
    check(failed, "The world encoder failure reaches the host");
    fail_world = false;
    check(capture.observe(true, true, party, saves) == "exact world",
          "A failed world encoder has not consumed the unchanged-party upgrade");

    opengold::WinnerCapture idle;
    fail_world = true;
    failed = false;
    try
    {
        (void)idle.observe(true, true, party, saves);
    }
    catch (const std::runtime_error &)
    {
        failed = true;
    }
    check(failed, "An idle-at-win encoder failure reaches the host");
    fail_world = false;
    check(idle.observe(true, true, party, saves) == "exact world",
          "An idle-at-win encoder failure has not consumed the win");
}

// A durable-write failure keeps the already encoded payload, even when
// the native character or epilogue changes before the next refresh.
void failed_writes_retry_the_original_payload()
{
    const opengold::PartyState party;
    for (const bool idle_at_win : {false, true})
    {
        opengold::WinnerCapture capture;
        std::string party_bytes = "exact at-win party", world_bytes = "exact at-win world";
        std::string durable;
        unsigned attempts = 0;
        bool fail_write = true;
        const opengold::WinnerCapture::Saves saves{[&]
                                                   {
                                                       return party_bytes;
                                                   },
                                                   [&]
                                                   {
                                                       return world_bytes;
                                                   }};
        const auto write = [&](std::string_view bytes)
        {
            ++attempts;
            if (fail_write)
            {
                throw std::runtime_error("durable writer failed");
            }
            durable = bytes;
        };
        bool failed = false;
        try
        {
            capture.bank(true, idle_at_win, party, saves, write);
        }
        catch (const std::runtime_error &)
        {
            failed = true;
        }
        check(failed && durable.empty(), "A failed durable write reaches the host");
        party_bytes = "changed epilogue party";
        world_bytes = "changed epilogue world";
        fail_write = false;
        capture.bank(true, true, party, saves, write);
        check(attempts == 2 &&
                  durable == (idle_at_win ? "exact at-win world" : "exact at-win party"),
              "The retry durably writes the original bytes, never the changed epilogue");
        capture.bank(true, true, party, saves, write);
        check(attempts == 2, "The successful retry is not written again");
    }

    opengold::WinnerCapture capture;
    std::string party_bytes = "unchanged party", world_bytes = "unchanged-party world", durable;
    bool fail_write = false;
    const opengold::WinnerCapture::Saves saves{[&]
                                               {
                                                   return party_bytes;
                                               },
                                               [&]
                                               {
                                                   return world_bytes;
                                               }};
    const auto write = [&](std::string_view bytes)
    {
        if (fail_write)
        {
            throw std::runtime_error("world write failed");
        }
        durable = bytes;
    };
    capture.bank(true, false, party, saves, write);
    check(durable == "unchanged party", "The party-only capture is durably written first");
    fail_write = true;
    bool failed = false;
    try
    {
        capture.bank(true, true, party, saves, write);
    }
    catch (const std::runtime_error &)
    {
        failed = true;
    }
    check(failed && durable == "unchanged party", "A failed world write preserves the first bank");
    party_bytes = "later party";
    world_bytes = "later world";
    fail_write = false;
    capture.bank(true, true, party, saves, write);
    check(durable == "unchanged-party world", "The world upgrade retries its exact encoded bytes");
    opengold::WinnerCapture loaded(true);
    loaded.bank(true, true, party, saves,
                [&](std::string_view)
                {
                    check(false, "Loading a won game must not write another bank");
                });
}

// One link of a series: the earlier game's party at its win, banked as the
// game banks it (with no world: a game is won mid-event), comes across to the
// sequel whole.
void a_link_carries_its_winners(GoldBoxGame earlier_game, GoldBoxGame sequel_game,
                                unsigned npc_bank, unsigned npc_record)
{
    const auto earlier = installed(earlier_game);
    const auto sequel = installed(sequel_game);
    if (!earlier || !sequel)
    {
        return;
    }
    const auto party = winning_party(*earlier, npc_bank, npc_record);
    const auto bytes = opengold::encode_campaign(*party, nullptr, earlier->assets(), earlier_game);
    const auto bank = reloaded_bank(*earlier, bytes);
    check(bank.game == earlier_game &&
              opengold::winners(bank.party).size() == bank.party.roster.size(),
          "The bank holds the game's winners");
    (void)bring_winners(*earlier, *sequel, bank.party);
}

// Secret takes in a Curse character's items by their names: its arms and
// armor by their words, a wand by the Secret item of the same name; what
// Secret has no words for stays behind.
void secret_renames_curse_items()
{
    const auto curse = installed(GoldBoxGame::curse_of_the_azure_bonds);
    const auto secret = installed(GoldBoxGame::secret_of_the_silver_blades);
    if (!curse || !secret)
    {
        return;
    }
    auto party = curse->party();
    make_player_characters(*party, *curse);
    const auto id = party->state().slots[0];
    const auto item = [](std::uint8_t type, std::array<std::uint8_t, 3> words, std::int8_t bonus)
    {
        std::array<std::uint8_t, 63> raw{};
        raw[46] = type;
        std::ranges::copy(words, raw.begin() + 47);
        raw[50] = static_cast<std::uint8_t>(bonus);
        opengold::por::Equipment equipment;
        equipment.stored = opengold::por::decode_items(raw)->front();
        return equipment;
    };
    // Curse's long sword +1 (type 36, "+1" its word 162), its plate mail
    // (58, "Mail" 48) and its wand of fireballs (79: "Fireballs" 242, "of"
    // 167, "Wand" 69).
    party->take_item(id, item(36, {0, 162, 36}, 1));
    (void)party->equip(id, party->member(id).character.inventory().items().back().id);
    party->take_item(id, item(58, {0, 48, 58}, 0));
    party->take_item(id, item(79, {242, 167, 69}, 0));
    const auto bank = reloaded_bank(
        *curse, opengold::encode_campaign(*party, nullptr, curse->assets(), curse->game));
    const auto sequel = bring_winners(*curse, *secret, bank.party);
    const auto &veteran = named(sequel->state(), party->member(id).character.sheet().name);
    std::vector<std::pair<unsigned, std::string>> arrived;
    for (const auto &carried : veteran.character.inventory().items())
    {
        const auto &stored = veteran.item_sources.at(carried.id);
        arrived.emplace_back(stored.stored.type, stored.decoded_name);
        std::cout << "  arrived: type " << unsigned(stored.stored.type) << " "
                  << stored.decoded_name << "\n";
    }
    const auto has = [&](unsigned type, std::string_view word)
    {
        return std::ranges::any_of(arrived,
                                   [&](const auto &entry)
                                   {
                                       return entry.first == type &&
                                              entry.second.find(word) != std::string::npos;
                                   });
    };
    check(has(18, "Long Sword") && has(36, "Plate") && has(52, "Fireballs"),
          "The long sword, plate mail and wand arrive as Secret's own types");
    check(veteran.equipped.size() == 1, "The readied sword stays readied");
    std::cout << "Secret renames Curse's items.\n";
}

// A real winning party banked by an earned ending (OPENGOLD_*_WINNERS),
// with its world when it has one, comes across whole.
void real_winners_come_across(const char *variable, GoldBoxGame earlier_game,
                              GoldBoxGame sequel_game)
{
    const char *file = std::getenv(variable);
    if (!file || !*file || !std::filesystem::exists(file))
    {
        std::cout << "No real " << opengold::game_title(earlier_game) << " winners: set "
                  << variable << ".\n";
        return;
    }
    const auto earlier = installed(earlier_game);
    const auto sequel = installed(sequel_game);
    if (!earlier || !sequel)
    {
        return;
    }
    const auto bank = reloaded_bank(*earlier, opengold::read_campaign_file(file));
    check(bank.game == earlier_game, "The real winners are the game's");
    for (const auto id : opengold::winners(bank.party))
    {
        const auto &member = member_of(bank.party, id);
        std::cout << "  winner " << member.character.sheet().name
                  << (member.npc_source.empty() ? "" : " (NPC " + member.npc_source + ")") << "\n";
    }
    const auto party =
        bring_winners(*earlier, *sequel,
                      [&]
                      {
                          // Only the winners: the roster also
                          // keeps those who left before.
                          auto winners_only = bank.party;
                          std::erase_if(winners_only.roster,
                                        [&](const opengold::PartyMember &member)
                                        {
                                            return !opengold::is_winner(bank.party, member);
                                        });
                          return winners_only;
                      }());
    if (sequel_game == GoldBoxGame::matrix_cubed)
    {
        // Nothing is converted: each record arrives as it was, its item
        // types Matrix Cubed's.
        for (const auto id : opengold::winners(bank.party))
        {
            const auto &from = member_of(bank.party, id);
            const auto &to = named(party->state(), from.character.sheet().name);
            const auto before = og::buck_rogers_record_of(from.vitals.resources);
            const auto after = og::buck_rogers_record_of(party->game_record(to.id));
            check(before && after && after->creature.level == before->creature.level &&
                      after->creature.experience == before->creature.experience &&
                      after->creature.credits == before->creature.credits &&
                      after->items.size() == before->items.size(),
                  "A Countdown record arrives in Matrix Cubed as it was");
        }
    }
}

// Two Countdown characters made by its creation, with their starting items
// and credits.
std::shared_ptr<opengold::CampaignParty> countdown_team(const Game &countdown, unsigned count = 2)
{
    auto party = countdown.party();
    opengold::RandomState random{2026};
    const auto stock = opengold::load_buck_rogers_stock(countdown.folder, countdown.game);
    for (unsigned index = 0; index < count; ++index)
    {
        const auto race = index % 2 ? og::Race::martian : og::Race::terran;
        const auto career = index % 2 ? og::Profession::warrior : og::Profession::rocket_jock;
        const auto name = index == 0   ? "Jock"
                          : index == 1 ? "Shade"
                                       : "Winner " + std::to_string(index + 1);
        auto creature =
            og::new_buck_rogers_character(race, og::Gender::male, career, countdown.game, random);
        creature.name = name;
        creature.credits = 1234;
        const auto id = party->add_pc(opengold::Character(
            *countdown.creation, og::buck_rogers_draft(og::GroundRecord{creature, {}, {}}), {}));
        opengold::outfit_buck_rogers_character(*party, id, stock);
    }
    return party;
}

// Later changes that leave the old five-field fingerprint unchanged must
// never replace the exact party saved at the win with the epilogue's party.
void the_capture_keeps_every_at_win_byte()
{
    const auto countdown = installed(GoldBoxGame::countdown_to_doomsday);
    if (!countdown)
    {
        return;
    }
    using Change = std::function<void(opengold::CampaignParty &, MemberId)>;
    const auto native_change = [](const std::function<void(og::GroundRecord &)> &change) -> Change
    {
        return [change](auto &party, MemberId id)
        {
            auto record = og::buck_rogers_record_of(party.game_record(id));
            check(record.has_value(), "The winning character has its actual native record");
            change(*record);
            party.keep_game_record(id, og::encode_ground_record(*record));
        };
    };
    const std::vector<std::pair<std::string, Change>> changes{
        {"native credits", native_change(
                               [](auto &record)
                               {
                                   --record.creature.credits;
                               })},
        {"native status", native_change(
                              [](auto &record)
                              {
                                  record.creature.status =
                                      static_cast<std::uint8_t>(og::BuckRogersStatus::dodging);
                              })},
        {"native skills", native_change(
                              [](auto &record)
                              {
                                  ++record.creature.skills[0];
                              })},
        {"same-count item contents", native_change(
                                         [](auto &record)
                                         {
                                             ++record.items.at(0).plus;
                                         })},
        {"alternate gear", native_change(
                               [](auto &record)
                               {
                                   record.alternate_items.push_back(record.items.at(0));
                               })},
        {"party money",
         [](auto &party, MemberId)
         {
             auto wealth = party.state().wealth;
             ++wealth[static_cast<std::size_t>(opengold::Coin::gold)];
             party.set_wealth(wealth);
         }},
    };
    for (const auto &[description, change] : changes)
    {
        const auto party = countdown_team(*countdown);
        const auto id = party->state().slots[0];
        const auto before = party->member(id);
        const auto encode = [&]
        {
            return opengold::encode_campaign(*party, nullptr, countdown->assets(), countdown->game);
        };
        const auto at_win = encode();
        unsigned worlds = 0;
        const opengold::WinnerCapture::Saves saves{encode, [&]
                                                   {
                                                       ++worlds;
                                                       return std::string("later world");
                                                   }};
        opengold::WinnerCapture capture;
        check(capture.observe(true, false, party->state(), saves) == at_win,
              "The complete native party is captured before Continue");
        change(*party, id);
        const auto &after = party->member(id);
        check(before.experience == after.experience &&
                  before.vitals.hit_points == after.vitals.hit_points &&
                  before.vitals.dead == after.vitals.dead &&
                  before.character.inventory().items().size() ==
                      after.character.inventory().items().size(),
              description + " leaves the former fingerprint unchanged");
        check(encode() != at_win, description + " changes the complete saved character or party");
        check(!capture.observe(true, true, party->state(), saves) && worlds == 0,
              description + " must leave the exact at-win bank untouched");
    }
}

// A Countdown party made by its creation comes to Matrix Cubed with its
// whole record.
void countdown_characters_come_to_matrix()
{
    const auto countdown = installed(GoldBoxGame::countdown_to_doomsday);
    const auto matrix = installed(GoldBoxGame::matrix_cubed);
    if (!countdown || !matrix)
    {
        return;
    }
    const auto party = countdown_team(*countdown);
    const auto bank =
        reloaded_bank(*countdown, opengold::encode_campaign(*party, nullptr, countdown->assets(),
                                                            countdown->game));
    const auto sequel = bring_winners(*countdown, *matrix, bank.party);
    for (const auto id : party->state().slots)
    {
        if (!id)
        {
            continue;
        }
        const auto before = og::buck_rogers_record_of(party->game_record(id));
        const auto &to = named(sequel->state(), party->member(id).character.sheet().name);
        const auto after = og::buck_rogers_record_of(sequel->game_record(to.id));
        check(before && after && after->creature.credits == 1234 &&
                  after->items.size() == before->items.size() &&
                  after->creature.level == before->creature.level,
              "A Countdown character keeps its credits, items and level in Matrix Cubed");
    }
}

// Source-derived availability fixtures for all seven sequel dialogs, with
// six legal PCs and their actual NPC record(s), never earned-route proof.
void write_screen_fixtures(const std::filesystem::path &directory)
{
    std::filesystem::create_directories(directory);
    const auto write = [&](const Game &game, const opengold::CampaignParty &party)
    {
        const auto stem = directory / std::string(opengold::game_folder(game.game));
        const auto bytes = opengold::encode_campaign(party, nullptr, game.assets(), game.game);
        opengold::write_campaign_file(stem.string() + "-winning-party.ogs", bytes);
        opengold::write_campaign_file(stem.string() + "-save.ogs", bytes);
        std::string offered;
        for (const auto id : opengold::winners(party.state()))
        {
            const auto &member = party.member(id);
            offered += (member.npc_source.empty() ? "PC\t" : "NPC\t") +
                       member.character.sheet().name + "\n";
        }
        opengold::write_campaign_file(stem.string() + "-winning-party.ogs.winners", offered);
    };
    for (const auto &[game, bank, record] :
         std::array{std::tuple{GoldBoxGame::pool_of_radiance, 3U, 109U},
                    std::tuple{GoldBoxGame::curse_of_the_azure_bonds, 3U, 22U},
                    std::tuple{GoldBoxGame::secret_of_the_silver_blades, 1U, 77U},
                    std::tuple{GoldBoxGame::gateway_to_the_savage_frontier, 2U, 62U},
                    std::tuple{GoldBoxGame::champions_of_krynn, 1U, 34U},
                    std::tuple{GoldBoxGame::death_knights_of_krynn, 1U, 74U}})
    {
        if (const auto installed_game = installed(game))
        {
            write(*installed_game, *full_winning_party(*installed_game, bank, record));
        }
    }
    if (const auto countdown = installed(GoldBoxGame::countdown_to_doomsday))
    {
        write(*countdown, *countdown_team(*countdown, 6));
    }
}

// Independently replay only the genuine earned fixture's final Forward and
// closing Continues. No combat, new choice, placement or generated party.
std::string matrix_reference_party(const Game &matrix, const std::filesystem::path &file)
{
    check(std::filesystem::exists(file), "The genuine Matrix pre-win reference must exist");
    auto saved = reloaded_bank(matrix, opengold::read_campaign_file(file));
    check(saved.town && !saved.town->snapshot().game_won && saved.town->can_leave(),
          "The earned reference is an ordinary idle save before the real win");
    auto party = matrix.party();
    party->restore(saved.party);
    auto &town = *saved.town;
    town.attach_restored_party(party);
    // Same public Easy as the isolated UI check: loading does not reseed.
    town.difficulty(opengold::Difficulty::easy);
    check(town.explore(opengold::por::ExplorationCommand::forward),
          "The reference uses its actual offered final Forward");
    for (unsigned step = 0; step < 256 && !town.snapshot().game_won; ++step)
    {
        const auto state = town.snapshot();
        if (state.phase == opengold::por::TourPhase::running)
        {
            town.advance(1.0);
        }
        else
        {
            check(state.phase == opengold::por::TourPhase::awaiting_continue &&
                      state.choices.size() == 1,
                  "The reference ending offers only Continue, never combat or a new choice");
            check(town.choose(state.continue_ticket, 0), "Continue the actual reference ending");
        }
    }
    check(town.snapshot().game_won && town.can_leave(),
          "The bounded public reference reaches Matrix's real idle PROGRAM 2 win");
    return opengold::encode_campaign(*party, nullptr, matrix.assets(), matrix.game);
}

// The bank the running game wrote at Matrix Cubed's win
// (winner_bank_capture_tests.gd): an ordinary save of the game won, with its
// world, reloading byte for byte, holding every winner of its party alive.
void the_running_game_banked_its_win(const std::filesystem::path &file)
{
    const auto matrix = installed(GoldBoxGame::matrix_cubed);
    check(matrix.has_value(), "Checking the actual Matrix bank requires its installed files");
    check(std::filesystem::exists(file), "The running game's bank was copied out");
    const auto bank = reloaded_bank(*matrix, opengold::read_campaign_file(file));
    check(bank.game == GoldBoxGame::matrix_cubed && bank.town && bank.town->snapshot().game_won,
          "The bank is Matrix Cubed's world at its win");
    const auto *reference = std::getenv("OPENGOLD_WINNER_BANK_REFERENCE");
    check(reference && *reference, "The actual bank requires its independent genuine reference");
    auto actual = matrix->party();
    actual->restore(bank.party);
    check(
        opengold::encode_campaign(*actual, nullptr, matrix->assets(), matrix->game) ==
            matrix_reference_party(*matrix, reference),
        "The UI bank exactly matches every independently earned at-win party byte, including all NPCs");
    unsigned members = 0, npcs = 0;
    for (const auto id : bank.party.slots)
    {
        if (!id)
        {
            continue;
        }
        const auto &member = member_of(bank.party, id);
        check(!member.vitals.dead, "Every member at the win is alive");
        ++members;
        npcs += member.npc_source.empty() ? 0 : 1;
        std::cout << "  winner " << member.character.sheet().name
                  << (member.npc_source.empty() ? "" : " (NPC)") << "\n";
    }
    check(members == opengold::winners(bank.party).size() && members - npcs == 6,
          "The bank holds the six player characters and the NPCs with them as winners");
    std::cout << "The running game banked " << members << " winners, " << npcs
              << " of them NPCs, with its world.\n";
}

} // namespace

int main()
{
    try
    {
        // OPENGOLD_WINNER_BANK_CHECK=FILE checks a bank the running game wrote.
        if (const char *bank = std::getenv("OPENGOLD_WINNER_BANK_CHECK"))
        {
            the_running_game_banked_its_win(bank);
            std::cout << "Winner bank checks passed.\n";
            return 0;
        }
        // OPENGOLD_WINNER_BANK_FIXTURES=DIRECTORY writes the party screen's
        // check's saves there.
        if (const char *fixtures = std::getenv("OPENGOLD_WINNER_BANK_FIXTURES"))
        {
            write_screen_fixtures(fixtures);
            return 0;
        }
        the_capture_follows_the_win();
        the_capture_keeps_every_at_win_byte();
        failed_encoders_can_retry();
        failed_writes_retry_the_original_payload();
        every_occupied_winner_is_available();
        // Each link, with an NPC of the earlier game's own records: Pool of
        // Radiance's hero, Curse's Alias, Secret's Vala, Gateway's Erek,
        // Champions' Strangbourn and Death Knights' Durfey.
        a_link_carries_its_winners(GoldBoxGame::pool_of_radiance,
                                   GoldBoxGame::curse_of_the_azure_bonds, 3, 109);
        a_link_carries_its_winners(GoldBoxGame::curse_of_the_azure_bonds,
                                   GoldBoxGame::secret_of_the_silver_blades, 3, 22);
        a_link_carries_its_winners(GoldBoxGame::secret_of_the_silver_blades,
                                   GoldBoxGame::pools_of_darkness, 1, 77);
        a_link_carries_its_winners(GoldBoxGame::gateway_to_the_savage_frontier,
                                   GoldBoxGame::treasures_of_the_savage_frontier, 2, 62);
        a_link_carries_its_winners(GoldBoxGame::champions_of_krynn,
                                   GoldBoxGame::death_knights_of_krynn, 1, 34);
        a_link_carries_its_winners(GoldBoxGame::death_knights_of_krynn,
                                   GoldBoxGame::dark_queen_of_krynn, 1, 74);
        secret_renames_curse_items();
        countdown_characters_come_to_matrix();
        real_winners_come_across("OPENGOLD_CHAMPIONS_WINNERS", GoldBoxGame::champions_of_krynn,
                                 GoldBoxGame::death_knights_of_krynn);
        real_winners_come_across("OPENGOLD_DEATH_KNIGHTS_WINNERS",
                                 GoldBoxGame::death_knights_of_krynn,
                                 GoldBoxGame::dark_queen_of_krynn);
        real_winners_come_across("OPENGOLD_COUNTDOWN_WINNERS", GoldBoxGame::countdown_to_doomsday,
                                 GoldBoxGame::matrix_cubed);
        std::cout << "Winner bank checks passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
