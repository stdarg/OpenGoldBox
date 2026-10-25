#include "opengold/authored_items.h"
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character_creator.h"
#include "opengold/coin_purse.h"
#include "opengold/ecl_party_host.h"
#include "opengold/combat_demo.h"
#include "opengold/random_treasure.h"
#include "opengold/rolf_tour.h"
#include "opengold/scroll_key.h"
#include "opengold/shop_data.h"
#include "opengold/monster_bundle.h"
#include "opengold/npc_portraits.h"
#include "opengold/srd5.h"
#include "png_size_only.h"
#include "test_support.h"
#include "tour_access.h"
#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>

using namespace opengold;

namespace
{
// A Godot check runs this binary only for the save fixtures its environment
// names; once each is written, the rest of the expedition need not run.
constexpr std::array<const char *, 8> fixture_variables{
    "OPENGOLD_SHOP_FIXTURE",        "OPENGOLD_HALL_FIXTURE",   "OPENGOLD_REST_CHOICE_FIXTURE",
    "OPENGOLD_OHLO_FIXTURE",        "OPENGOLD_SLUMS_FIXTURES", "OPENGOLD_KUTO_WELL_FIXTURE",
    "OPENGOLD_KUTO_NORRIS_FIXTURE", "OPENGOLD_GATE_FIXTURE"};
std::set<std::string, std::less<>> written_fixtures;

bool requested_fixtures_written()
{
    bool requested = false;
    for (const auto *variable : fixture_variables)
    {
        if (std::getenv(variable))
        {
            requested = true;
            if (!written_fixtures.contains(variable))
            {
                return false;
            }
        }
    }
    return requested;
}

using opengold::test::check;
using opengold::test::throws;

std::unique_ptr<rules::RulesModule> module()
{
    return srd5::load(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "data/rules/srd-5.2.1/combat.rules");
}

// A player's character (a fighter unless asked): best rolls to Strength,
// Constitution, then Dexterity.
Character character(std::string name, std::uint64_t seed = 42,
                    const std::string &character_class = "fighter")
{
    CharacterCreator creator(srd5::character_rules(), seed);
    creator.select(rules::CreationField::race, "human");
    creator.select(rules::CreationField::character_class, character_class);
    creator.roll();
    creator.name(std::move(name));
    std::array<unsigned, 6> best{0, 1, 2, 3, 4, 5};
    const auto &rolls = creator.draft().rolls;
    std::stable_sort(best.begin(), best.end(),
                     [&](unsigned a, unsigned b)
                     {
                         return rolls[a].total() > rolls[b].total();
                     });
    using enum rules::Ability;
    constexpr std::array priority{strength, constitution, dexterity,
                                  wisdom,   charisma,     intelligence};
    for (unsigned i = 0; i < 6; ++i)
    {
        creator.assign_roll(best[i], priority[i]);
    }
    return Character(creator.rules(), creator.draft(), creator.appearance());
}

// Original armor records convert to the SRD armor of the same name; SRD 5.2.1
// has no Banded Mail, which becomes Splint. Magic armor stays unsupported.
void armor_conversion_tests()
{
    const std::map<unsigned, std::string> armor{
        {50, "leather"},    {51, "padded"},     {52, "studded_leather"}, {53, "ring_mail"},
        {54, "scale_mail"}, {55, "chain_mail"}, {56, "splint"},          {57, "splint"},
        {58, "plate"},      {59, "shield"}};
    for (const auto &[type, key] : armor)
    {
        por::Equipment item;
        item.stored.type = std::uint8_t(type);
        check(equipment_conversion(item) == key, "Original armor converts by name");
        item.stored.magic_bonus = 1;
        check(equipment_conversion(item) == key + "+1", "Magic armor converts with its bonus");
    }

    // The audited case: a new Cleric buys Scale Mail, pays once and equips it.
    CampaignParty party(module());
    const auto id = party.add_pc(character("Cleric", 1, "cleric"));
    party.set_wealth({0, 0, 0, 250, 0, 0, 0});
    por::Equipment scale;
    scale.stored.type = 54;
    scale.stored.value = 45;
    party.purchase(id, scale);
    const auto item = party.member(id).character.inventory().items().back().id;
    party.equip(id, item);
    check(party.state().wealth[3] == 205 && party.member(id).equipped.size() == 1,
          "Scale Mail is bought for 45 gp and equipped");
}

void coin_purse_tests()
{
    const Purse gold_and_silver{0, 96, 0, 145, 0, 3, 1};
    const auto view = script_coins(gold_and_silver);
    check(view == Purse{15460, 1546, 309, 154, 15, 3, 1},
          "Scripts see what the whole coin purse affords in each denomination");
    const auto unchanged = settle_script_coins(gold_and_silver, view);
    check(unchanged.purse == gold_and_silver && !unchanged.exchange,
          "A script that only reads coins leaves the purse alone");

    auto inn = view;
    --inn[4];
    const auto paid = settle_script_coins(gold_and_silver, inn);
    check(paid.purse == Purse{0, 96, 0, 135, 0, 3, 1} && paid.exchange &&
              paid.exchange->owed == Coins{0, 0, 0, 0, 1} &&
              paid.exchange->paid == Coins{0, 0, 0, 10, 0} && paid.exchange->change == Coins{},
          "One platinum is paid with ten gold");

    const Purse platinum{0, 0, 0, 5, 1, 0, 0};
    auto exact = script_coins(platinum);
    --exact[4];
    const auto from_platinum = settle_script_coins(platinum, exact);
    check(from_platinum.purse == Purse{0, 0, 0, 5, 0, 0, 0} && !from_platinum.exchange,
          "A purse holding the asked-for coin pays it directly");

    const Purse one_platinum{0, 0, 0, 0, 1, 0, 0};
    auto five_gold = script_coins(one_platinum);
    five_gold[3] -= 5;
    const auto broken = settle_script_coins(one_platinum, five_gold);
    check(broken.purse == Purse{0, 0, 0, 5, 0, 0, 0} && broken.exchange &&
              broken.exchange->paid == Coins{0, 0, 0, 0, 1} &&
              broken.exchange->change == Coins{0, 0, 0, 5, 0},
          "A larger coin is broken and the change returned in gold");

    const Purse one_gold{0, 0, 0, 1, 0, 0, 0};
    auto three_silver = script_coins(one_gold);
    three_silver[1] -= 3;
    check(settle_script_coins(one_gold, three_silver).purse == Purse{0, 7, 0, 0, 0, 0, 0},
          "Change below a gold piece comes back in silver");

    auto treasure = view;
    treasure[1] += 50;
    check(settle_script_coins(gold_and_silver, treasure).purse == Purse{0, 146, 0, 145, 0, 3, 1},
          "Coins a script adds are added as given");

    // The party purse holds more of a coin than a script's 16-bit cell shows.
    const Purse hoard{0, 0, 0, 70000, 0, 70000, 0};
    auto hoard_view = script_coins(hoard);
    check(hoard_view[3] == script_coin_limit && hoard_view[5] == script_coin_limit,
          "A script cell shows at most 65,535 coins or gems");
    hoard_view[3] -= 10;
    --hoard_view[5];
    const auto spent = settle_script_coins(hoard, hoard_view);
    check(spent.purse == Purse{0, 0, 0, 69990, 0, 69999, 0} && !spent.exchange,
          "What a script takes comes out of the coins it could not see");

    auto gem = view;
    --gem[5];
    check(settle_script_coins(gold_and_silver, gem).purse == Purse{0, 96, 0, 145, 0, 2, 1},
          "Gems are taken as gems, never converted to coins");

    auto everything = view;
    std::fill(everything.begin(), everything.begin() + 5, 0);
    check(throws(
              [&]
              {
                  (void)settle_script_coins(gold_and_silver, everything);
              }),
          "A script cannot take more than the purse is worth");
}

std::shared_ptr<const por::EclProgram> program(std::vector<std::uint8_t> body)
{
    std::vector<std::uint8_t> bytes{0, 0};
    for (int n = 0; n < 5; ++n)
    {
        bytes.insert(bytes.end(), {1, 1, 0x15, 0x99});
    }
    bytes.push_back(0);
    bytes.insert(bytes.end(), body.begin(), body.end());
    return std::make_shared<const por::EclProgram>(
        por::EclProgram::decode(bytes, "expedition payment"));
}

void settle_synthetic(por::RolfTourSession &town)
{
    for (unsigned n = 0; n < 100 && town.snapshot().phase == por::TourPhase::running; ++n)
    {
        town.advance(.5);
    }
    check(town.snapshot().phase == por::TourPhase::completed, "Synthetic event completes");
}

// The script pays one platinum (SAVE 14 into the shown 15), stores the character
// (one read), then ends the event (a second read). Only the first may charge.
void script_payment_tests()
{
    auto party = std::make_shared<CampaignParty>(module());
    (void)party->add_pc(character("Arden"));
    party->set_wealth({0, 96, 0, 145, 0, 0, 0});
    auto resources = std::make_shared<por::PhlanResources>();
    auto payment = program({9, 0, 14, 1, 0xC3, 0x6B, 10, 0, 128, 0});
    por::RolfTourSession town({}, payment, {}, 0x9914, {}, resources);
    town.campaign_party(party);
    settle_synthetic(town);
    check(town.script_variable(por::EclAddress{0x6BC3}) == 15,
          "The script sees fifteen affordable platinum");
    check(town.explore(por::ExplorationCommand::look), "Payment event starts");
    settle_synthetic(town);
    check(party->state().wealth == Purse{0, 96, 0, 135, 0, 0, 0},
          "The payment is charged exactly once across both script reads");
    check(town.snapshot().payments.size() == 1 && town.snapshot().payments[0].payer == "Arden" &&
              town.snapshot().payments[0].coins.paid == Coins{0, 0, 0, 10, 0},
          "The exchange is reported for the payer");
    check(town.script_variable(por::EclAddress{0x6BC3}) == 14 &&
              town.script_variable(por::EclAddress{0x6BC1}) == 144,
          "The script then sees the settled purse");

    auto reader = program({10, 0, 128, 0});
    por::RolfTourSession reading({}, reader, {}, 0x9914, {}, resources);
    reading.campaign_party(party);
    settle_synthetic(reading);
    reading.explore(por::ExplorationCommand::look);
    settle_synthetic(reading);
    check(party->state().wealth == Purse{0, 96, 0, 135, 0, 0, 0} &&
              reading.snapshot().payments.empty(),
          "Events that only read coins never change the purse");
}

// Runs a synthetic event until it waits or ends, acknowledging a rollback
// notice. Returns true when the event failed and was rolled back.
bool settle_or_rollback(por::RolfTourSession &town, bool until_idle = true)
{
    for (unsigned n = 0; n < 100 && town.snapshot().phase == por::TourPhase::running; ++n)
    {
        town.advance(.5);
    }
    const auto &s = town.snapshot();
    const bool rolled_back = s.phase == por::TourPhase::awaiting_continue &&
                             s.dialogue.starts_with("This event is not supported yet");
    if (rolled_back)
    {
        check(town.choose(s.continue_ticket, 0), "The rollback notice is acknowledged");
    }
    if (until_idle)
    {
        check(town.snapshot().phase == por::TourPhase::completed, "Synthetic event ends");
    }
    return rolled_back;
}

// Runs one synthetic Look. False means the event failed and was rolled back.
bool look(por::RolfTourSession &town)
{
    check(town.explore(por::ExplorationCommand::look), "The event starts");
    return !settle_or_rollback(town);
}

// A training hall's PROGRAM 0 in a synthetic event: leaving without buying
// charges nothing, and a session bought before the script later fails is
// rolled back with the event, gold, experience and the session's record alike.
void hall_event_tests()
{
    for (const bool failing : {false, true})
    {
        for (const bool buying : {false, true})
        {
            auto party = std::make_shared<CampaignParty>(module());
            (void)party->add_pc(character("Arden"));
            party->set_wealth({0, 0, 0, 1000, 0, 0, 0});
            const auto before = party->checkpoint();
            std::vector<std::uint8_t> body{0x38, 0, 0};
            if (failing)
            {
                body.insert(body.end(), {0x38, 0, 8});
            }
            body.push_back(0);
            auto resources = std::make_shared<por::PhlanResources>();
            por::RolfTourSession town({}, program(body), {}, 0x9914, {}, resources);
            town.campaign_party(party);
            settle_synthetic(town);
            test::TourAccess::set_string(town, por::ecl_training_hall_name, "FIGHTERS ");
            check(town.explore(por::ExplorationCommand::look), "The hall's event starts");
            for (unsigned n = 0; n < 100 && town.snapshot().phase == por::TourPhase::running; ++n)
            {
                town.advance(.5);
            }
            const auto &choices = town.snapshot().choices;
            check(!choices.empty() && choices.front().starts_with("Arden: session of experience") &&
                      choices.back() == "Leave",
                  "The hall lists Arden's session first and Leave last");
            if (buying)
            {
                check(town.choose(town.snapshot().continue_ticket, 0) &&
                          party->member(party->state().slots[0]).experience > 0,
                      "Arden buys a session of experience");
            }
            check(town.choose(town.snapshot().continue_ticket, town.snapshot().choices.size() - 1),
                  "The party leaves the hall");
            const bool rolled_back = settle_or_rollback(town);
            check(rolled_back == failing, "Only the failing script rolls back");
            const auto &member = party->member(party->state().slots[0]);
            const bool charged = party->state().wealth != before.wealth;
            check(charged == (buying && !failing) && (member.experience > 0) == charged,
                  buying && !failing ? "A session is paid for once"
                  : failing          ? "A failed event refunds the session and its experience"
                                     : "Leaving without buying charges nothing");
        }
    }
}

// PROGRAM 8, the original's victory after Tyranthraxus, is not supported yet:
// the event fails and is rolled back, its payment included, instead of passing
// as done.
void unsupported_program_tests()
{
    auto party = std::make_shared<CampaignParty>(module());
    (void)party->add_pc(character("Arden"));
    party->set_wealth({0, 96, 0, 145, 0, 0, 0});
    const auto before = party->state().wealth;
    auto resources = std::make_shared<por::PhlanResources>();
    auto victory = program({9, 0, 14, 1, 0xC3, 0x6B, 10, 0, 128, 0x38, 0, 8, 0});
    por::RolfTourSession town({}, victory, {}, 0x9914, {}, resources);
    town.campaign_party(party);
    settle_synthetic(town);
    check(!look(town) && party->state().wealth == before,
          "An unsupported PROGRAM fails its event and refunds what it charged");
}

Purse party_purse(const PartyState &state)
{
    return state.wealth;
}

unsigned claims_starting(const PartyState &state, std::string_view prefix)
{
    return unsigned(std::count_if(state.claimed_rewards.begin(), state.claimed_rewards.end(),
                                  [&](const auto &id)
                                  {
                                      return id.starts_with(prefix);
                                  }));
}

constexpr std::string_view synthetic_treasure = "por:ECL3:0:treasure:";

// The shape of Ohlo's hand-in: TREASURE 150 pp, 1 jewelry and one random item,
// COMBAT with no monsters, then the completion flag 0x4A04. Optionally guarded
// by that flag, and optionally failing afterwards while the second member is selected.
std::shared_ptr<const por::EclProgram> treasure_program(bool guarded, bool failing)
{
    std::vector<std::uint8_t> body;
    if (guarded)
    {
        body.insert(body.end(), {3, 1, 0x04, 0x4A, 0, 255, 22, 0});
    }
    body.insert(body.end(), {39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 150, 0, 0, 0, 1, 0, 129, 36});
    body.insert(body.end(), {9, 0, 255, 1, 0x04, 0x4A});
    if (failing)
    {
        // ROB form 2, which no script uses, fails after the award.
        body.insert(body.end(), {3, 1, 0xB4, 0x6D, 0, 1, 22, 40, 0, 2, 0, 0, 0, 0});
    }
    body.push_back(0);
    return program(body);
}

void script_treasure_tests()
{
    auto party = std::make_shared<CampaignParty>(module());
    party->add_pc(character("Arden"));
    party->add_pc(character("Bryn", 2));
    auto resources = std::make_shared<por::PhlanResources>();
    por::RolfTourSession town({}, treasure_program(true, true), {}, 0x9914, {}, resources);
    town.campaign_party(party);
    settle_synthetic(town);

    party->select(PartySlot{1});
    const auto before = party->checkpoint();
    check(!look(town) && town.script_diagnostics().size() == 1,
          "A failure after the award reports and rolls back the event");
    check(party_purse(party->checkpoint()) == party_purse(before) &&
              party->state().claimed_rewards == before.claimed_rewards &&
              town.script_variable(por::EclAddress{0x4A04}) == 0,
          "The rollback restores purses, claims and the quest flag together");

    party->select(PartySlot{0});
    check(look(town), "The retried event completes");
    const auto paid = party->checkpoint();
    check(paid.wealth == Purse{0, 0, 0, 0, 150, 0, 1} &&
              claims_starting(paid, synthetic_treasure) == 1 &&
              town.script_variable(por::EclAddress{0x4A04}) == 255 &&
              paid.roster[0].experience == 0,
          "The party receives 150 pp and the jewelry once, with no XP");
    const auto &arden = paid.roster[0];
    check(arden.character.inventory().items().size() == 1 && arden.item_sources.size() == 1 &&
              paid.roster[1].character.inventory().items().empty(),
          "The one random item goes to the first living member");
    const auto &item = arden.character.inventory().items().front();
    const auto &source = arden.item_sources.at(item.id);
    check(item.original_type == source.stored.type && source.stored.stored_name.empty() &&
              item.definition_id == equipment_conversion(source),
          "The generated item keeps its original record as provenance");

    // The same script stream generates the same item.
    auto twin = std::make_shared<CampaignParty>(module());
    twin->add_pc(character("Arden"));
    por::RolfTourSession again({}, treasure_program(true, false), {}, 0x9914, {}, resources);
    again.campaign_party(twin);
    settle_synthetic(again);
    check(look(again), "The same treasure runs in a second campaign");
    const auto &twin_member = twin->state().roster[0];
    check(twin_member.item_sources.size() == 1 &&
              twin_member.item_sources.begin()->second.stored.raw == source.stored.raw,
          "Random treasure is deterministic from the saved script random state");

    check(look(town) && party_purse(party->checkpoint()) == party_purse(paid) &&
              party->state().claimed_rewards == paid.claimed_rewards,
          "The completed flag keeps a revisit from paying again");

    // Were a script to reach the same TREASURE again, it is refused, not paid twice.
    por::RolfTourSession repeat({}, treasure_program(false, false), {}, 0x9914, {}, resources);
    auto other = std::make_shared<CampaignParty>(module());
    other->add_pc(character("Cora", 3));
    repeat.campaign_party(other);
    settle_synthetic(repeat);
    check(look(repeat), "The unguarded treasure pays once");
    const auto once = other->checkpoint();
    check(!look(repeat) && party_purse(other->checkpoint()) == party_purse(once) &&
              other->state().claimed_rewards == once.claimed_rewards &&
              repeat.script_diagnostics().back().find("already awarded") != std::string::npos,
          "A repeated award is an explicit, rolled-back failure");
}

// The generator follows the original tables for scripted dice.
void random_treasure_tables()
{
    const auto scripted = [](std::vector<unsigned> rolls)
    {
        return [rolls, next = std::size_t{0}](unsigned sides) mutable
        {
            check(next < rolls.size() && rolls[next] <= sides, "Scripted roll fits its die");
            return rolls[next++];
        };
    };
    // Weapon table: d100 10 then d100 36 is a long sword; d20 15 makes it +2.
    const auto sword = por::random_treasure_items(1, scripted({10, 36, 15})).at(0);
    check(sword.type == 36 && sword.magic_bonus == 2 && sword.name_components[1] == 0xA3 &&
              sword.name_components[2] == 36 && sword.weight == 60 && sword.value == 4000 &&
              sword.hidden_name_words == 6,
          "A generated +2 long sword has the original name, weight and value");
    // d100 95, d15 5 is a potion; d8 6 selects the first preset record.
    const auto potion = por::random_treasure_items(1, scripted({95, 5, 6})).at(0);
    check(potion.type == 71 && potion.magic_bonus == 1 && potion.value == 800 &&
              potion.weight == 1 && potion.effect_codes == std::array<std::uint8_t, 3>{3, 0x63, 0},
          "A generated potion uses its original preset record");
    // d100 70 is a magic-user scroll of d3 = 2 spells: level 1 (d13 13) and level 5 (d4 4).
    const auto scroll = por::random_treasure_items(1, scripted({70, 2, 1, 13, 5, 4})).at(0);
    check(scroll.type == 61 && scroll.value == 1800 && scroll.name_components[1] == 0xD3 &&
              scroll.effect_codes == std::array<std::uint8_t, 3>{21, 94, 0},
          "A generated scroll records its original spell codes");
}

// A full party purse defers the treasure without loss; a reload keeps it pending.
void deferred_treasure_tests()
{
    auto party = std::make_shared<CampaignParty>(module());
    const auto arden = party->add_pc(character("Arden"));
    party->set_wealth({0, 0, 0, 0, purse_limit - 100, 0, 0});
    const auto script = treasure_program(true, false);
    auto resources = std::make_shared<por::PhlanResources>();
    resources->programs.emplace(0, script);
    por::RolfTourSession town({}, script, {}, 0x9914, {}, resources);
    town.campaign_party(party);
    settle_synthetic(town);

    check(look(town) && town.script_variable(por::EclAddress{0x4A04}) == 255 &&
              party->state().wealth == Purse{0, 0, 0, 0, purse_limit - 100, 0, 0} &&
              claims_starting(party->checkpoint(), synthetic_treasure) == 0 &&
              town.snapshot().dialogue.find("Loot is retained") != std::string::npos,
          "A party purse without room defers the whole award and the quest still completes");
    check(look(town) && party->state().wealth == Purse{0, 0, 0, 0, purse_limit - 100, 0, 0},
          "The award stays pending while there is no room");

    const auto rules = module();
    const auto bytes = encode_campaign(*party, &town, "treasure");
    auto loaded = decode_campaign(bytes, *srd5::character_rules(), *rules, "treasure", &town);
    auto reloaded = std::make_shared<CampaignParty>(module());
    reloaded->restore(std::move(loaded.party));
    auto restored = std::move(*loaded.town);
    restored.attach_restored_party(reloaded);
    check(encode_campaign(*reloaded, &restored, "treasure") == bytes,
          "The pending treasure survives a reload");

    check(party->member(arden).character.inventory().items().empty(),
          "Deferred treasure holds back its item too");
    reloaded->set_wealth({0, 0, 0, 0, 100, 0, 0});
    const auto items = [&]
    {
        return reloaded->member(arden).character.inventory().items().size();
    };
    check(look(restored) && reloaded->state().wealth == Purse{0, 0, 0, 0, 250, 0, 1} &&
              items() == 1 && reloaded->member(arden).item_sources.size() == 1 &&
              claims_starting(reloaded->checkpoint(), synthetic_treasure) == 1,
          "Once there is room the deferred treasure and its item are collected exactly once");
    check(look(restored) && reloaded->state().wealth == Purse{0, 0, 0, 0, 250, 0, 1} &&
              items() == 1,
          "Collected treasure is not collected again");
}

// One synthetic door: (0,0) east is locked (code 2); (1,0) west is an ordinary door.
por::GeoMap locked_door_map()
{
    por::GeoMap map;
    map.cells[0].walls[1] = 1;
    map.cells[0].doors[1] = 2;
    map.cells[1].walls[3] = 1;
    map.cells[1].doors[3] = 1;
    return map;
}

// Steps east and answers the Locked menu. True when the party got through or the
// destination's event failed (it is then rolled back).
bool try_door(por::RolfTourSession &town, const std::string &choice,
              const std::vector<std::string> &offered = {"Bash", "Exit"})
{
    check(town.explore(por::ExplorationCommand::forward), "The step starts");
    settle_or_rollback(town, false);
    const auto &s = town.snapshot();
    check(s.dialogue == "Locked." && s.choices == offered,
          "A locked door offers the original choices the party can use");
    const auto chosen = std::find(s.choices.begin(), s.choices.end(), choice);
    check(town.choose(s.continue_ticket, std::size_t(chosen - s.choices.begin())),
          "The door choice is accepted");
    const bool rolled_back = settle_or_rollback(town);
    return rolled_back || town.snapshot().pose.x == 1;
}

// The d20 faces from 1 to 20 that reach `difficulty` with `modifier`.
int winning_faces(int difficulty, int modifier)
{
    int faces = 0;
    for (int die = 1; die <= 20; ++die)
    {
        faces += die + modifier >= difficulty;
    }
    return faces;
}

// Thieves' Tools double a Rogue's chance to pick a lock (LOCK-1): the check's
// difficulty drops until twice as many d20 faces win.
void thieves_tools_tests(const std::shared_ptr<CampaignParty> &party, MemberId rogue,
                         const std::shared_ptr<const por::EclProgram> &passable,
                         const std::shared_ptr<por::PhlanResources> &resources)
{
    por::Equipment tools;
    tools.stored.type = authored_item::thieves_tools;
    check(equipment_conversion(tools) == "thieves_tools" && tools.label() == "Thieves' Tools" &&
              module()->equipment_info("thieves_tools").slot == rules::EquipmentSlot::carried,
          "Thieves' Tools are carried gear");
    party->purchase(rogue, tools);
    for (unsigned tries = 0;; ++tries)
    {
        check(tries < 40, "A Rogue with Thieves' Tools eventually picks the lock");
        por::RolfTourSession town(locked_door_map(), passable, {}, 0x9914, {}, resources);
        town.campaign_party(party);
        settle_synthetic(town);
        town.explore(por::ExplorationCommand::turn_right);
        const bool picked = try_door(town, "Pick", {"Bash", "Pick", "Exit"});
        const auto &check_made = town.snapshot().door_checks.at(0);
        const int modifier = check_made.total - check_made.die;
        check(check_made.thieves_tools && check_made.difficulty <= 15 &&
                  winning_faces(check_made.difficulty, modifier) ==
                      std::min(20, 2 * winning_faces(15, modifier)),
              "Thieves' Tools double the d20 faces that pick the lock");
        check(picked == (check_made.total >= check_made.difficulty),
              "The lowered difficulty decides the pick");
        if (picked)
        {
            break;
        }
        check(!try_door(town, "Exit"), "After a failed Pick the lock offers only Bash and Exit");
    }
}

// A conscious Rogue adds Pick: Dexterity (Sleight of Hand), once per lock until
// the party moves, as the original allowed one pick attempt per step.
void pick_lock_tests()
{
    auto party = std::make_shared<CampaignParty>(module());
    party->add_pc(character("Arden"));
    const auto rogue = party->add_pc(character("Sly", 7, "rogue"));
    auto resources = std::make_shared<por::PhlanResources>();
    const auto passable = program({0});
    for (unsigned tries = 0;; ++tries)
    {
        check(tries < 40, "A Rogue eventually picks the lock");
        por::RolfTourSession town(locked_door_map(), passable, {}, 0x9914, {}, resources);
        town.campaign_party(party);
        settle_synthetic(town);
        town.explore(por::ExplorationCommand::turn_right);
        const bool picked = try_door(town, "Pick", {"Bash", "Pick", "Exit"});
        const auto &checks = town.snapshot().door_checks;
        check(checks.size() == 1 && checks[0].member == "Sly" &&
                  checks[0].method == DoorMethod::pick && checks[0].difficulty == 15,
              "Only the Rogue tries, with one Dexterity (Sleight of Hand) check");
        if (picked)
        {
            check(town.snapshot().door_opened && town.map().at(0, 0).doors[1] == 1,
                  "A picked lock opens the door");
            break;
        }
        check(!try_door(town, "Exit"), "After a failed Pick the lock offers only Bash and Exit");
    }
    thieves_tools_tests(party, rogue, passable, resources);

    auto unconscious = party->checkpoint();
    for (auto &member : unconscious.roster)
    {
        if (member.id == rogue)
        {
            member.vitals.hit_points = 0;
        }
    }
    party->restore(unconscious);
    por::RolfTourSession town(locked_door_map(), passable, {}, 0x9914, {}, resources);
    town.campaign_party(party);
    settle_synthetic(town);
    town.explore(por::ExplorationCommand::turn_right);
    check(!try_door(town, "Exit"), "An unconscious Rogue cannot pick a lock");
}

void locked_door_tests()
{
    auto party = std::make_shared<CampaignParty>(module());
    party->add_pc(character("Arden"));
    party->add_pc(character("Bryn", 2));
    auto resources = std::make_shared<por::PhlanResources>();
    // The destination's search fails, so an opened door must be rolled back too.
    const auto failing = program({3, 1, 0x4B, 0xC0, 0, 1, 22, 40, 0, 2, 0, 0, 0, 0, 0});
    por::RolfTourSession town(locked_door_map(), failing, {}, 0x9914, {}, resources);
    town.campaign_party(party);
    settle_synthetic(town);
    town.explore(por::ExplorationCommand::turn_right);

    const auto random = party->state().random_state;
    check(!try_door(town, "Exit") && town.snapshot().pose.x == 0 &&
              party->state().random_state == random && town.map().at(0, 0).doors[1] == 2 &&
              town.snapshot().door_checks.empty(),
          "Exit leaves the door locked and rolls nothing");

    unsigned tries = 0;
    while (!try_door(town, "Bash"))
    {
        const auto &checks = town.snapshot().door_checks;
        check(checks.size() == 2 && !town.snapshot().door_opened && checks[0].member == "Arden" &&
                  checks[1].member == "Bryn" && checks[0].method == DoorMethod::bash &&
                  checks[0].difficulty == 15 && checks[0].total < 15 &&
                  town.map().at(0, 0).doors[1] == 2,
              "A failed Bash records every member's check and leaves the door locked");
        check(++tries < 40, "The party eventually forces the door");
    }
    check(town.snapshot().pose.x == 0 && town.map().at(0, 0).doors[1] == 2 &&
              town.script_diagnostics().size() == 1,
          "A failure beyond the door rolls back the move and relocks it");

    const auto passable = program({0});
    por::RolfTourSession open(locked_door_map(), passable, {}, 0x9914, {}, resources);
    open.campaign_party(party);
    settle_synthetic(open);
    open.explore(por::ExplorationCommand::turn_right);
    for (tries = 0; !try_door(open, "Bash"); ++tries)
    {
        check(tries < 40, "The party eventually forces the door");
    }
    const auto &opened = open.snapshot();
    check(opened.pose.x == 1 && open.map().at(0, 0).doors[1] == 1 && opened.door_opened &&
              !opened.door_checks.empty() && opened.door_checks.back().total >= 15,
          "A successful Bash records its checks, unlocks the door and moves the party");
    open.explore(por::ExplorationCommand::turn_around);
    open.explore(por::ExplorationCommand::forward);
    settle_synthetic(open);
    check(open.snapshot().pose.x == 0, "The opened door stays open in both directions");
    pick_lock_tests();
}

using Answer = std::function<std::size_t(const por::TourSnapshot &)>;

// Original peaceful answers. Roaming groups are fled from, as in the audited
// trip, so the four-orc paper event is the expedition's only fight.
std::size_t peaceful(const por::TourSnapshot &s)
{
    if (s.dialogue == "Choose a party member.")
    {
        return 0;
    }
    if (s.choices.size() == 4 && s.choices[0] == "Fight")
    {
        return 2;
    }
    for (const auto *safe : {"NO", "LEAVE", "RUN", "GO", "NONE", "EXIT"})
    {
        for (std::size_t n = 0; n < s.choices.size(); ++n)
        {
            if (s.choices[n] == safe)
            {
                return n;
            }
        }
    }
    return s.choices.size() - 1;
}

// Every combat definition fight() has faced, in order.
std::vector<std::string> enemies_fought;

// How each fight fight() played began: where the party stood, the encounter
// distance and each combatant's starting square.
struct FightStart
{
    por::PartyPose pose;
    unsigned distance{};
    std::map<rules::EntityId, rules::Cell> cells;
};

std::vector<FightStart> fights_started;

// The last encounter fight() played, as the town staged it.
std::optional<CampaignEncounter> last_encounter;

// Squares the party sees ahead before a wall or door, at most two.
unsigned open_squares_ahead(const por::GeoMap &map, por::PartyPose pose)
{
    unsigned squares = 0;
    while (squares < 2 && !map.at(pose.x, pose.y).walls[index(pose.facing)])
    {
        ++squares;
        pose.x = (pose.x + 16 + step_x(pose.facing)) % 16;
        pose.y = (pose.y + 16 + step_y(pose.facing)) % 16;
    }
    return squares;
}

void fight(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party)
{
    if (town.monster_picture())
    {
        check(town.start_encounter(), "The monster close-up is dismissed");
    }
    check(town.pending_encounter().has_value(), "The original encounter reaches combat");
    for (const auto &enemy : town.pending_encounter()->enemies)
    {
        enemies_fought.push_back(enemy.definition);
    }
    last_encounter = *town.pending_encounter();
    CombatDemo combat(module());
    combat.campaign_party(party);
    combat.encounter(*town.pending_encounter(), 42);
    FightStart start{town.snapshot().pose, town.pending_encounter()->distance, {}};
    check(town.snapshot().overland || start.distance <= open_squares_ahead(town.map(), start.pose),
          "The monsters are never further away than the party can see");
    for (const auto &unit : combat.combat().snapshot().combatants)
    {
        start.cells[unit.id] = unit.cell;
    }
    fights_started.push_back(std::move(start));
    if (town.pending_encounter()->party_resting)
    {
        const auto log = combat.combat().snapshot().log();
        for (const auto id : party->state().slots)
        {
            if (id && party->member(id).vitals.hit_points > 0)
            {
                const auto line = party->member(id).character.sheet().name + " wakes up prone.";
                check(std::count(log.begin(), log.end(), line) == 1,
                      "Each resting member wakes up Prone");
            }
        }
    }
    for (unsigned n = 0; n < 4000 && combat.combat().snapshot().outcome == rules::Outcome::ongoing;
         ++n)
    {
        check(combat.submit(choose_demo_command(combat.combat(), Searching::party)),
              "Combat accepts the command");
    }
    const auto result = combat.combat().snapshot();
    check(result.outcome == rules::Outcome::victory, "The party wins its fight");
    check(town.resolve_combat(result), "Exploration accepts the victory");
}

// Plays the current event until exploration resumes or a shop opens.
void settle(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party,
            const Answer &answer = peaceful, std::string_view typed = "0")
{
    for (unsigned n = 0; n < 5000; ++n)
    {
        const auto &s = town.snapshot();
        switch (s.phase)
        {
        case por::TourPhase::completed:
        case por::TourPhase::shopping:
            check(town.script_diagnostics().empty(),
                  "The route uses only supported events" +
                      (town.script_diagnostics().empty()
                           ? std::string{}
                           : ": " + town.script_diagnostics().back() + " (script " +
                                 std::to_string(s.script_id) + ", area " +
                                 std::to_string(s.area_id) + " at " + std::to_string(s.pose.x) +
                                 "," + std::to_string(s.pose.y) +
                                 (s.overland ? ", wilderness " + std::to_string(s.overland_x) +
                                                   "," + std::to_string(s.overland_y)
                                             : std::string{}) +
                                 ")"));
            return;
        case por::TourPhase::running:
            town.advance(.5);
            break;
        case por::TourPhase::awaiting_continue:
            check(town.choose(s.continue_ticket, answer(s)), "The script accepts the answer");
            break;
        case por::TourPhase::awaiting_input:
            check(town.input(s.continue_ticket, typed), "The script accepts the input");
            break;
        case por::TourPhase::combat:
            fight(town, party);
            break;
        default:
            throw std::runtime_error("Event failed: " + s.diagnostic);
        }
    }
    std::string choices;
    for (const auto &c : town.snapshot().choices)
    {
        choices += " [" + c + "]";
    }
    throw std::runtime_error("Event did not finish: " + town.snapshot().dialogue + choices);
}

void face(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party, unsigned facing)
{
    while (index(town.snapshot().pose.facing) != facing)
    {
        check(town.explore(por::ExplorationCommand::turn_right), "The party turns");
        settle(town, party);
    }
}

void step(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party, unsigned facing,
          const Answer &answer = peaceful, std::string_view typed = "0")
{
    face(town, party, facing);
    town.explore(por::ExplorationCommand::forward);
    settle(town, party, answer, typed);
}

bool party_wounded(const CampaignParty &party)
{
    for (const auto id : party.state().slots)
    {
        if (id)
        {
            // The current maximum: Life Drain or Harm may have lowered it.
            const auto &member = party.member(id);
            if (!member.vitals.dead && member.vitals.hit_points < party.hit_point_maximum(id))
            {
                return true;
            }
        }
    }
    return false;
}

// Keeps the spell and training choices a finished rest offers as they are, one
// member at a time, as each choice renews the rest's ticket.
void keep_rest_choices(CampaignParty &party)
{
    while (party.state().spell_rest && !party.state().spell_rest->members.empty())
    {
        party.keep_rest_spells(party.state().spell_rest->members.front());
    }
    while (party.state().training_rest && !party.state().training_rest->members.empty())
    {
        const auto &rest = *party.state().training_rest;
        party.keep_rest_training(rest.ticket, rest.members.front());
    }
}

// Camps where the party stands until `rested` holds: a Long Rest when anyone is
// eligible, otherwise a Short Rest spending Hit Dice. Interruptions are played
// like any encounter, and the party rests again.
void rest_until(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party,
                const std::function<bool()> &rested, const std::string &goal)
{
    for (unsigned tries = 0; !rested(); ++tries)
    {
        check(tries < 40, goal);
        const auto info = party->rest_info(RestKind::long_rest);
        const bool long_rest = std::any_of(info.begin(), info.end(),
                                           [](const auto &member)
                                           {
                                               return member.denial == RestDenial::none;
                                           });
        check(town.camp(long_rest ? RestKind::long_rest : RestKind::short_rest), "Camp starts");
        settle(town, party);
        keep_rest_choices(*party);
        if (!party->state().short_rest)
        {
            continue;
        }
        // Each spend renews the ticket, so it is read again every time.
        const auto members = party->state().short_rest->members;
        for (const auto id : members)
        {
            const auto &member = party->member(id);
            const auto recovery =
                party->rule_module().recovery_info(member.character.sheet(), member.vitals);
            if (recovery.hit_dice && member.vitals.hit_points < party->hit_point_maximum(id))
            {
                (void)party->heal_with_hit_dice(party->state().short_rest->ticket, id);
            }
        }
        party->finish_short_rest(party->state().short_rest->ticket);
    }
}

// Camps until nobody living is wounded.
void recover(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party)
{
    rest_until(town, party, [&] { return !party_wounded(*party); }, "The party recovers by camping");
}

// Breadth-first walk over open edges, through the original movement events.
// With bash_doors, locked doors are part of the route and are retried until forced.
// With camp_when_hurt, the party recovers by camping whenever a step leaves it wounded.
// The walk also ends early once `arrived` holds.
void walk_to(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party, unsigned tx,
             unsigned ty, const Answer &answer = peaceful, bool bash_doors = false,
             bool camp_when_hurt = false, std::string_view typed = "0",
             const std::function<bool()> &arrived = {})
{
    const auto locked = [&](unsigned wall, unsigned door)
    {
        return bash_doors && wall && door == 2;
    };
    constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
    std::set<std::pair<int, int>> refused;
    for (unsigned moves = 0; moves < 400; ++moves)
    {
        const auto p = town.snapshot().pose;
        if ((p.x == tx && p.y == ty) || (arrived && arrived()))
        {
            return;
        }
        std::array<int, 256> previous;
        previous.fill(-1);
        std::queue<int> cells;
        const int origin = p.y * 16 + p.x;
        cells.push(origin);
        previous[origin] = origin;
        while (!cells.empty())
        {
            const int cell = cells.front();
            cells.pop();
            for (unsigned d = 0; d < 4; ++d)
            {
                const int x = cell % 16 + dx[d], y = cell / 16 + dy[d];
                if (x < 0 || y < 0 || x >= 16 || y >= 16)
                {
                    continue;
                }
                // As the engine steps: only the wall on the party's side stops it.
                const auto &a = town.map().at(cell % 16, cell / 16);
                const auto blocked = [&](unsigned wall, unsigned door)
                {
                    return !locked(wall, door) && (door > 1 || (wall && !door));
                };
                if (blocked(a.walls[d], a.doors[d]))
                {
                    continue;
                }
                const int next = y * 16 + x;
                if (previous[next] >= 0 || refused.contains({cell, next}))
                {
                    continue;
                }
                previous[next] = cell;
                cells.push(next);
            }
        }
        int next = int(ty * 16 + tx);
        // A refused step may only have met a random encounter; try those edges again.
        if (previous[next] < 0 && !refused.empty())
        {
            refused.clear();
            continue;
        }
        check(previous[next] >= 0, "The destination is reachable");
        while (previous[next] != origin)
        {
            next = previous[next];
        }
        const unsigned facing = next % 16 > int(p.x)   ? 1
                                : next % 16 < int(p.x) ? 3
                                : next / 16 > int(p.y) ? 2
                                                       : 0;
        step(town, party, facing, answer, typed);
        if (camp_when_hurt)
        {
            recover(town, party);
        }
        const auto now = town.snapshot().pose;
        const int reached = int(now.y * 16 + now.x);
        const auto &edge = town.map().at(p.x, p.y);
        const auto &far = town.map().at(next % 16, next / 16);
        const bool door_held = locked(edge.walls[facing], edge.doors[facing]) ||
                               locked(far.walls[(facing + 2) % 4], far.doors[(facing + 2) % 4]);
        // A held door is bashed again. Fleeing a roaming group relocates the
        // party; quest walks then replan rather than giving up on the edge.
        const bool fled = bash_doors && reached != origin;
        if (reached != next && !door_held && !fled)
        {
            refused.insert({origin, next});
        }
    }
    throw std::runtime_error("Walk did not arrive");
}

struct Expedition
{
    std::shared_ptr<CampaignParty> party;
    por::RolfTourSession town;
};

// Six created fighters with the new-character purse, after Rolf's tour, in a
// town that uses data/shops as the game does, and `monsters` if given.
Expedition create_party(const std::filesystem::path &directory,
                        std::shared_ptr<const por::MonsterBundle> monsters = {})
{
    auto party = std::make_shared<CampaignParty>(module());
    std::uint64_t seed = 1;
    for (const auto *name : {"Arden", "Bryn", "Cora", "Darin", "Elin", "Fenn"})
    {
        (void)party->add_pc(character(name, seed++));
        party->add_coins(Coin::gold, 250);
    }
    auto town = por::RolfTourSession::load(directory, {}, std::move(monsters));
    auto shop_data = load_shop_data(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/shops");
    town.temples(std::move(shop_data.temples));
    town.shops(std::move(shop_data.shops));
    town.companion_rules(srd5::character_rules());
    town.campaign_party(party);
    settle(town, party);
    return {party, std::move(town)};
}

// The general store at (15,8) sells the original's oil and the gear its
// data/shops record adds (Torch, Acid, Alchemist's Fire, Magic Ink), all converting to SRD items.
void general_store_gear(const std::filesystem::path &directory)
{
    auto trip = create_party(directory);
    auto &[party, town] = trip;
    party->make_leader(party->state().slots[2]);
    walk_to(town, party, 15, 8);
    face(town, party, 0);
    town.explore(por::ExplorationCommand::look);
    settle(town, party,
           [](const por::TourSnapshot &s)
           {
               return s.dialogue.find("CAN I SHOW") != std::string::npos ? std::size_t{0}
                                                                         : peaceful(s);
           });
    check(town.snapshot().phase == por::TourPhase::shopping, "The general store opens");
    check(party->selected() == party->leader(), "The leader deals with the shopkeeper");
    const auto &stock = town.shop_stock();
    std::set<std::string> sold;
    for (const auto &item : stock)
    {
        sold.insert(equipment_conversion(item));
    }
    check(sold.contains("oil") && sold.contains("torch") && sold.contains("acid") &&
              sold.contains("alchemists_fire"),
          "The general store sells Oil, Torches, Acid and Alchemist's Fire");
    for (unsigned level = 1; level <= 8; ++level)
    {
        const auto ink = std::find_if(stock.begin(), stock.end(),
                                      [&](const auto &item)
                                      {
                                          return equipment_conversion(item) ==
                                                 rules::magic_ink_key(level);
                                      });
        check(ink != stock.end() && ink->stored.value == 1000 * level,
              "Magic Ink of each spell level costs 1000 gp a level");
    }
    check(std::none_of(stock.begin(), stock.end(),
                       [](const auto &item)
                       {
                           return equipment_conversion(item) == rules::magic_ink_key(9);
                       }),
          "No Magic Ink above the highest spell level, 8");
    const auto acid = std::find_if(stock.begin(), stock.end(),
                                   [](const auto &item)
                                   {
                                       return equipment_conversion(item) == "acid";
                                   });
    check(acid->stored.value == 25 &&
              town.buy(town.snapshot().continue_ticket, std::size_t(acid - stock.begin())),
          "A vial of Acid costs 25 gp");
    const auto &items = party->member(party->selected()).character.inventory().items();
    check(std::any_of(items.begin(), items.end(),
                      [](const auto &item)
                      {
                          return item.definition_id == "acid";
                      }),
          "The buyer carries the Acid");
}

void buy_and_equip(Expedition &trip)
{
    auto &[party, town] = trip;
    walk_to(town, party, 13, 8);
    town.explore(por::ExplorationCommand::look);
    settle(town, party,
           [](const por::TourSnapshot &s)
           {
               return s.dialogue.find("SHOP") != std::string::npos ? std::size_t{0} : peaceful(s);
           });
    check(town.snapshot().phase == por::TourPhase::shopping, "The arms shop opens");
    const auto &stock = town.shop_stock();
    check(std::none_of(stock.begin(), stock.end(),
                       [](const auto &item)
                       {
                           return equipment_conversion(item).starts_with("por:unsupported:");
                       }),
          "Every item the arms shop sells converts to SRD equipment");
    for (unsigned slot = 0; slot < 6; ++slot)
    {
        party->select(PartySlot{slot});
        // Original long sword, chain mail and shield records.
        for (const unsigned type : {36u, 55u, 59u})
        {
            const auto offered = std::find_if(stock.begin(), stock.end(),
                                              [&](const auto &item)
                                              {
                                                  return item.stored.type == type;
                                              });
            check(offered != stock.end(), "The arms shop stocks fighter gear");
            check(town.buy(town.snapshot().continue_ticket, std::size_t(offered - stock.begin())),
                  "The party buys its gear");
        }
    }
    party->select(PartySlot{0});
    check(town.leave_shop(town.snapshot().continue_ticket), "The party leaves the shop");
    settle(town, party);
    for (const auto id : party->state().slots)
    {
        if (id)
        {
            for (const auto &item : party->member(id).character.inventory().items())
            {
                party->equip(id, item.id);
            }
            check(party->member(id).equipped.size() == 3 &&
                      party->state().wealth == Purse{0, 0, 0, 6 * (250 - 15 - 75 - 15), 0, 0, 0},
                  "The party pays in gold and each fighter equips sword, mail and shield");
        }
    }
}

unsigned orc_rewards(const CampaignParty &party)
{
    const auto &claimed = party.state().claimed_rewards;
    return unsigned(std::count(claimed.begin(), claimed.end(), "por:ECL2:20:search1:orcs:v1") +
                    std::count(claimed.begin(), claimed.end(), "por:ECL2:20:search1:orcs:v1:loot"));
}

// What a crossing between areas swaps: the map, its wall art, the script and
// its archive bank (ECL3 for the town), and the area's own map memory.
struct AreaContext
{
    unsigned area{}, script{};
    std::uint16_t bank{};
    std::vector<std::uint8_t> map;
    std::vector<std::vector<std::uint8_t>> art;
    std::bitset<256> visited;
    bool operator==(const AreaContext &) const = default;
};

AreaContext area_context(const por::RolfTourSession &town)
{
    const auto &s = town.snapshot();
    AreaContext context{s.area_id, s.script_id, town.script_variable(por::ecl_script_bank),
                        town.map().raw};
    for (const auto &appearance : town.wall_art().appearances)
    {
        for (const auto &image : appearance)
        {
            context.art.push_back(image.rgba);
        }
    }
    context.visited = s.visited;
    return context;
}

bool posed(const por::RolfTourSession &town, unsigned x, unsigned y, por::MapDirection facing)
{
    return town.snapshot().pose == por::PartyPose{x, y, facing};
}

void defeat_four_orcs(Expedition &trip)
{
    auto &[party, town] = trip;
    walk_to(town, party, 0, 4);
    const auto in_town = area_context(town);
    step(town, party, 3);
    const auto in_slums = area_context(town);
    check(in_slums.area == 20 && in_slums.script == 20 && in_slums.bank == 2 &&
              posed(town, 15, 4, por::MapDirection::west),
          "The west gate leads into the Slums, on its east edge and still facing west");
    check(in_slums.map != in_town.map && in_slums.art != in_town.art &&
              in_slums.visited.count() == 1,
          "The Slums bring their own map, wall art and map memory");
    // The route passes (13,1), where the orcs argue over their papers.
    walk_to(town, party, 12, 1);
    check(town.script_variable(por::ecl_slums_orc_victory) == 255 && orc_rewards(*party) == 2,
          "The four-orc victory sets its original flag and claims XP and loot once");
    for (const auto &member : party->state().roster)
    {
        check(member.experience == 300, "Every fighter earns the victory XP");
    }
    check(party->state().wealth[1] == 96, "The orcs' original silver is collected");
    walk_to(town, party, 15, 4);
    step(town, party, 1);
    check(town.snapshot().area_id == 0 && posed(town, 0, 4, por::MapDirection::east),
          "The east edge of the Slums returns to New Phlan's west gate, facing east");
    check(area_context(town) == in_town,
          "Back in town: its map, wall art, script, bank and map memory as they were");
}

// Accepts the inn's quote ("YES") or declines it, and names the payer.
Answer inn_answer(bool stay, std::string payer)
{
    return [stay, payer = std::move(payer)](const por::TourSnapshot &s)
    {
        for (std::size_t n = 0; n < s.choices.size(); ++n)
        {
            if (s.choices[n] == (stay ? "YES" : "NO") || s.choices[n] == payer)
            {
                return n;
            }
        }
        return peaceful(s);
    };
}

Purse party_purse(const CampaignParty &party)
{
    return party.state().wealth;
}

// The original inn (ECL3:0, south of (4,11)) asks the chosen character for one platinum.
void rest_at_inn(Expedition &trip)
{
    auto &[party, town] = trip;
    // The orcs start their fight in contact (PLACE-1) and may drop a fighter.
    // Stable, it comes round within four hours (1d4 hours), so the whole party
    // can take the inn's Long Rest.
    party->advance_time(std::chrono::hours{4});
    walk_to(town, party, 4, 11);
    const auto leave_inn = [&]
    {
        if (town.snapshot().pose.y == 12)
        {
            step(town, party, 0);
        }
    };

    auto before = party->checkpoint();
    const auto start = party_purse(*party);
    step(town, party, 2, inn_answer(false, ""));
    check(town.snapshot().dialogue == "THEN YOU MUST LEAVE." && party_purse(*party) == start &&
              party->state().roster[0].vitals == before.roster[0].vitals &&
              party->state().time_minutes - before.time_minutes < 1,
          "Declining the quote costs nothing and grants no rest");
    leave_inn();

    // A party worth less than one platinum (9 gp 9 sp) is refused by the script.
    const auto savings = party_purse(*party);
    party->set_wealth({0, 9, 0, 9, 0, 0, 0});
    before = party->checkpoint();
    const auto unpaid = party_purse(*party);
    step(town, party, 2, inn_answer(true, "Cora"));
    check(town.snapshot().dialogue == "YOU DON'T HAVE ENOUGH PLATINUM." &&
              party_purse(*party) == unpaid && town.snapshot().payments.empty() &&
              party->state().time_minutes - before.time_minutes < 1,
          "An unaffordable stay is refused with the purse unchanged");
    leave_inn();

    party->set_wealth(savings);
    before = party->checkpoint();
    step(town, party, 2, inn_answer(true, "Arden"));
    auto expected = savings;
    expected[3] -= 10;
    check(party_purse(*party) == expected,
          "The party pays exactly ten gold for the platinum, once");
    const auto &payments = town.snapshot().payments;
    check(payments.size() == 1 && payments[0].payer == "Arden" &&
              payments[0].coins.owed == Coins{0, 0, 0, 0, 1} &&
              payments[0].coins.paid == Coins{0, 0, 0, 10, 0} &&
              payments[0].coins.change == Coins{},
          "The exchange is reported to the player");
    check(party->state().time_minutes >= before.time_minutes + 480,
          "The long rest advances the clock eight hours");
    for (const auto &member : party->state().roster)
    {
        check(member.vitals.hit_points == member.character.sheet().hit_points &&
                  member.last_rest_minutes,
              "The long rest restores every fighter");
    }
    leave_inn();
}

// Walks back to the completed four-orc event and looks: nothing may repeat.
void revisit_orcs(Expedition &trip)
{
    auto &[party, town] = trip;
    walk_to(town, party, 0, 4);
    step(town, party, 3);
    walk_to(town, party, 12, 1);
    // Roaming groups met on the way may be fought; the revisit itself pays nothing.
    const auto before = party->checkpoint();
    town.explore(por::ExplorationCommand::look);
    settle(town, party);
    check(town.script_variable(por::ecl_slums_orc_victory) == 255 && orc_rewards(*party) == 2 &&
              party->state().claimed_rewards == before.claimed_rewards,
          "The completed event does not fight or reward again");
    check(party->state().wealth == before.wealth, "Revisiting pays nothing");
    for (std::size_t n = 0; n < before.roster.size(); ++n)
    {
        check(party->state().roster[n].experience == before.roster[n].experience,
              "Revisiting keeps XP");
    }
}

constexpr std::string_view ohlo_reward = "por:ECL2:20:treasure:a390:v1";

// Ohlo's commission: bash locked doors, TALK and parley NICE, accept, SPEAK at
// the booth, GIVE the potion. Roaming groups are still fled from.
std::size_t quest_answer(const por::TourSnapshot &s)
{
    for (const auto *wanted : {"Bash", "TALK", "NICE", "ACCEPT THE COMMISSION", "SPEAK", "GIVE"})
    {
        for (std::size_t n = 0; n < s.choices.size(); ++n)
        {
            if (s.choices[n] == wanted)
            {
                return n;
            }
        }
    }
    return peaceful(s);
}

// Steps west from (14,10) into Ohlo's room until the forced door lets the party
// in. Returns the party as it was just before the step that reached Ohlo.
PartyState enter_ohlo_room(Expedition &trip, std::uint16_t until_commission)
{
    auto &[party, town] = trip;
    walk_to(town, party, 14, 10, quest_answer, true);
    auto before = party->checkpoint();
    for (unsigned tries = 0; town.script_variable(por::EclAddress{0x4A04}) != until_commission;
         ++tries)
    {
        check(tries < 40, "The party forces Ohlo's door");
        before = party->checkpoint();
        step(town, party, 3, quest_answer);
    }
    const auto pose = town.snapshot().pose;
    check(pose.x == 14 && pose.y == 10 && pose.facing == por::MapDirection::east,
          "Ohlo's script returns the party outside his door");
    return before;
}

unsigned ohlo_rewards(const CampaignParty &party)
{
    const auto &claimed = party.state().claimed_rewards;
    return unsigned(std::count(claimed.begin(), claimed.end(), ohlo_reward));
}

// From New Phlan: spend the four orcs' XP on level two, as in the audit, enter
// the Slums and accept Ohlo's commission. The party waits outside his door.
void accept_commission(Expedition &trip)
{
    auto &[party, town] = trip;
    for (const auto id : party->state().slots)
    {
        if (id)
        {
            check(party->can_advance(id), "The orcs' experience earns level two");
            party->advance(id, party->default_advancement(id));
        }
    }
    walk_to(town, party, 0, 4);
    step(town, party, 3);
    check(town.snapshot().area_id == 20, "The west gate leads into the Slums");

    const auto accepting = enter_ohlo_room(trip, 250);
    check(town.script_variable(por::EclAddress{0x4A81}) == 0 &&
              party_purse(*party) == party_purse(accepting) &&
              party->state().claimed_rewards == accepting.claimed_rewards,
          "Accepting the commission sets its flag and pays nothing");
}

// Fetches the potion from the Old Rope Guild booth. Saving is refused while
// the booth's dialogue waits for an answer.
void fetch_potion(Expedition &trip, const std::string &assets)
{
    auto &[party, town] = trip;
    // The fights on the way to the booth wear down a party that never rests.
    walk_to(town, party, 14, 12, quest_answer, true, true);
    face(town, party, 1);
    town.explore(por::ExplorationCommand::forward);
    while (town.snapshot().phase == por::TourPhase::running)
    {
        town.advance(.5);
    }
    check(town.snapshot().phase == por::TourPhase::awaiting_continue,
          "The booth's dialogue waits for an answer");
    check(throws(
              [&]
              {
                  (void)encode_campaign(*party, &town, assets);
              }),
          "A save during the booth's dialogue is refused");
    settle(town, party, quest_answer, "ohlo");
    check(town.script_variable(por::EclAddress{0x4A81}) == 250 &&
              town.snapshot().dialogue.find("RETURNS WITH A PACKAGE") != std::string::npos,
          "Speaking Ohlo's name at the booth obtains the potion");
    check(speaking_npc_portrait(town.snapshot()) == "NPCs/ohlos-potion-keeper.png",
          "The real rope-guild booth shows its own portrait");
}

// Walks from the booth back to Ohlo's door.
void return_to_ohlo(Expedition &trip)
{
    walk_to(trip.town, trip.party, 14, 10, quest_answer, true);
}

// Ohlo's reward, from just before the hand-in to just after it.
void check_ohlo_reward(const PartyState &before, const PartyState &after)
{
    auto claims = before.claimed_rewards;
    claims.emplace_back(ohlo_reward);
    check(after.claimed_rewards == claims, "The hand-in claims only Ohlo's reward");
    check(after.wealth[4] == before.wealth[4] + 150 && after.wealth[6] == before.wealth[6] + 1,
          "The party receives Ohlo's 150 pp and jewelry");
    const auto original_items = [](const PartyState &state)
    {
        std::size_t count = 0;
        for (const auto &member : state.roster)
        {
            count += member.item_sources.size();
        }
        return count;
    };
    check(original_items(after) == original_items(before) + 1,
          "Ohlo's random item is awarded once with its original record");
    for (std::size_t n = 0; n < after.roster.size(); ++n)
    {
        check(after.roster[n].experience == before.roster[n].experience,
              "The original hand-in grants no experience");
    }
}

// Hands the potion to Ohlo for his reward, then returns to New Phlan.
void hand_in_potion(Expedition &trip)
{
    auto &[party, town] = trip;
    const auto before = enter_ohlo_room(trip, 255);
    check(town.script_variable(por::EclAddress{0x4A81}) == 255 && ohlo_rewards(*party) == 1,
          "Handing in the potion completes the quest and claims the reward once");
    check_ohlo_reward(before, party->checkpoint());
    walk_to(town, party, 15, 4);
    step(town, party, 1);
    check(town.snapshot().area_id == 0, "The party returns to New Phlan");
}

// After completion, Ohlo's room and the booth are quiet and pay nothing again.
void revisit_ohlo(Expedition &trip)
{
    auto &[party, town] = trip;
    if (town.snapshot().area_id == 0)
    {
        walk_to(town, party, 0, 4);
        step(town, party, 3);
    }
    // Each quiet visit is checked on its own, apart from roaming fights on the way.
    const auto visit = [&](unsigned x, unsigned y, unsigned facing)
    {
        walk_to(town, party, x, y, quest_answer, true);
        const auto before = party->checkpoint();
        step(town, party, facing, quest_answer, "OHLO");
        const bool arrived = town.snapshot().pose.x == x + (facing == 1 ? 1 : -1);
        check(!arrived || (party_purse(*party) == party_purse(before) &&
                           party->state().claimed_rewards == before.claimed_rewards),
              "A completed quest location pays nothing again");
    };
    for (unsigned tries = 0; town.snapshot().pose.x != 13; ++tries)
    {
        check(tries < 40, "The party forces Ohlo's door again");
        visit(14, 10, 3);
    }
    step(town, party, 1);
    visit(14, 12, 1);
    // 0x4A04 is area-local (cleared on entering the Slums); 0x4A81 records completion.
    check(town.snapshot().pose.x == 15 && town.script_variable(por::EclAddress{0x4A81}) == 255 &&
              ohlo_rewards(*party) == 1,
          "Revisiting Ohlo and the booth keeps the quest complete without a second reward");
}

// Fights the monsters that interrupt a camp instead of fleeing them.
std::size_t stand_and_fight(const por::TourSnapshot &s)
{
    return s.choices.size() == 4 && s.choices[0] == "Fight" ? 0 : peaceful(s);
}

// Camps on a Slums street, where the original script rolls for interruptions,
// until monsters attack. The party fights, wakes Prone, wins and rests again.
void slums_camp_ambush(Expedition &trip)
{
    auto &[party, town] = trip;
    if (town.snapshot().area_id == 0)
    {
        walk_to(town, party, 0, 4);
        step(town, party, 3);
    }
    walk_to(town, party, 14, 4);
    check(town.script_variable(por::EclAddress{0x6E82}) == 0 &&
              town.script_variable(por::EclAddress{0x4ABB}) < 254,
          "The party stands on an uncleared Slums street without a special event");
    bool attacked = false;
    for (unsigned tries = 0; tries < 60; ++tries)
    {
        const auto before = party->state().time_minutes;
        check(town.camp(RestKind::short_rest), "Camp starts");
        while (town.snapshot().phase == por::TourPhase::running ||
               town.snapshot().phase == por::TourPhase::awaiting_continue)
        {
            const auto &s = town.snapshot();
            if (s.phase == por::TourPhase::running)
            {
                town.advance(.5);
            }
            else
            {
                check(town.choose(s.continue_ticket, stand_and_fight(s)), "Answer accepted");
            }
        }
        check(town.script_variable(por::ecl_rest_check_interval) == 24 &&
                  town.script_variable(por::ecl_rest_interruption_chance) == 24,
              "The original pre-camp script sets the street's 24/24 profile");
        if (town.snapshot().phase == por::TourPhase::combat)
        {
            check(town.snapshot().dialogue.starts_with("Your camp is attacked!"),
                  "The interruption is announced");
            if (town.monster_picture())
            {
                check(town.start_encounter(), "The monster close-up is dismissed");
            }
            check(town.pending_encounter()->party_resting,
                  "The camp's monsters find the party resting");
            check(party->state().time_minutes == before + 60,
                  "A Short Rest's check falls at its last five-minute step");
            settle(town, party, stand_and_fight);
            const auto &text = town.snapshot().dialogue;
            check(text.ends_with("The rest was interrupted. Rest again to recover.") &&
                      !party->state().short_rest,
                  "Victory returns to exploration with no rest benefits");
            attacked = true;
            continue;
        }
        check(town.snapshot().phase == por::TourPhase::completed, "The camp finishes");
        if (const auto &spending = party->state().short_rest)
        {
            party->finish_short_rest(spending->ticket);
            if (attacked)
            {
                break;
            }
        }
    }
    check(attacked, "Camping on a Slums street is eventually interrupted");
    const auto &text = town.snapshot().dialogue;
    check(text.ends_with(
              "Short rest complete: one hour passed; eligible members can spend Hit Dice."),
          "After the fight the party rests again");
}

std::filesystem::path revisit_result(const std::filesystem::path &save)
{
    return std::filesystem::path(save).concat(".revisit");
}

// Loads a saved campaign as the game does, which must re-encode to the same bytes.
Expedition load_expedition(const std::filesystem::path &save,
                           const std::filesystem::path &directory)
{
    const auto assets = campaign_asset_identity(directory);
    const auto bytes = read_campaign_file(save);
    const auto rules = module();
    const auto base = por::RolfTourSession::load(directory);
    auto loaded = decode_campaign(bytes, *srd5::character_rules(), *rules, assets, &base);
    auto party = std::make_shared<CampaignParty>(module());
    party->restore(std::move(loaded.party));
    Expedition trip{party, std::move(*loaded.town)};
    trip.town.attach_restored_party(party);
    trip.town.companion_rules(srd5::character_rules());
    check(encode_campaign(*party, &trip.town, assets) == bytes,
          "The reloaded campaign is the saved campaign");
    return trip;
}

// Runs in a fresh process: load the save, revisit the orcs and Ohlo, camp in the
// Slums and record the result.
void reload_and_revisit(const std::filesystem::path &save, const std::filesystem::path &directory)
{
    const auto assets = campaign_asset_identity(directory);
    auto trip = load_expedition(save, directory);
    auto &party = trip.party;
    check(!party->rest(), "The inn's rest timer survives the reload");
    revisit_orcs(trip);
    revisit_ohlo(trip);
    slums_camp_ambush(trip);
    write_campaign_file(revisit_result(save), encode_campaign(*party, &trip.town, assets));
}

std::filesystem::path checkpoint_save(const std::filesystem::path &folder,
                                      std::string_view checkpoint)
{
    return folder / (std::string(checkpoint) + ".ogs");
}

std::filesystem::path resume_result(const std::filesystem::path &save)
{
    return std::filesystem::path(save).concat(".resumed");
}

// Runs in a fresh process: load a save made in the Slums during Ohlo's quest and
// play on to the next save point, recording the result. Loading relocks forced
// doors, so the run that reaches Ohlo's door with the potion records the result
// there and then forces his door again to finish the quest.
void resume_quest(std::string_view checkpoint, const std::filesystem::path &save,
                  const std::filesystem::path &directory)
{
    const auto assets = campaign_asset_identity(directory);
    auto trip = load_expedition(save, directory);
    const auto &town = trip.town;
    check(town.snapshot().area_id == 20 && town.script_variable(por::EclAddress{0x4A04}) == 250 &&
              ohlo_rewards(*trip.party) == 0,
          "The Slums save keeps the accepted commission and no reward");
    if (checkpoint == "accepted")
    {
        check(town.script_variable(por::EclAddress{0x4A81}) == 0, "The potion is not yet fetched");
        fetch_potion(trip, assets);
        write_campaign_file(resume_result(save), encode_campaign(*trip.party, &trip.town, assets));
        return;
    }
    check(town.script_variable(por::EclAddress{0x4A81}) == 250,
          "The fetched potion is kept for the hand-in");
    return_to_ohlo(trip);
    write_campaign_file(resume_result(save), encode_campaign(*trip.party, &trip.town, assets));
    hand_in_potion(trip);
}

// Checks a save the game wrote after the player handed in the potion, starting
// from the fixture saved at Ohlo's door (OPENGOLD_OHLO_FIXTURE).
void verify_hand_in(const std::filesystem::path &at_door, const std::filesystem::path &handed_in,
                    const std::filesystem::path &directory)
{
    const auto before = load_expedition(at_door, directory);
    const auto after = load_expedition(handed_in, directory);
    check(after.town.snapshot().area_id == 20 &&
              after.town.script_variable(por::EclAddress{0x4A81}) == 255 &&
              ohlo_rewards(*after.party) == 1,
          "The game's save holds the completed quest and one reward");
    check_ohlo_reward(before.party->checkpoint(), after.party->checkpoint());
}

// The save tests/area_crossing_tests.gd wrote after walking from the west gate
// into the Slums and back: the town as the fixture left it, with nothing of the
// Slums in its map memory, and the four-orc victory kept without a second reward.
void verify_gate_crossing(const std::filesystem::path &gate, const std::filesystem::path &crossed,
                          const std::filesystem::path &directory)
{
    const auto before = load_expedition(gate, directory);
    const auto after = load_expedition(crossed, directory);
    check(posed(after.town, 0, 4, por::MapDirection::east) &&
              area_context(after.town) == area_context(before.town),
          "The game's save is back at the west gate with the town as it was");
    check(after.town.script_variable(por::ecl_slums_orc_victory) == 255 &&
              orc_rewards(*after.party) == 2,
          "The crossing keeps the orc victory and claims nothing twice");
}

int run(const std::string &command)
{
#ifdef _WIN32
    return std::system(("\"" + command + "\"").c_str()); // cmd.exe strips one outer pair of quotes.
#else
    return std::system(command.c_str());
#endif
}

// Leaving the Slums by any edge but the east one enters Kuto's Well
// (ECL2:20's NEW ECL 29): its plaza map 29, under script 29.
void enter_kutos_well(Expedition &trip, const std::filesystem::path &folder,
                      const std::filesystem::path &directory)
{
    auto &[party, town] = trip;
    if (town.snapshot().area_id == 0)
    {
        walk_to(town, party, 0, 4);
        step(town, party, 3);
    }
    check(town.snapshot().area_id == 20, "The party is in the Slums");
    // A doorway in the Slums' west edge.
    unsigned edge = 16;
    for (unsigned y = 0; y < 16 && edge == 16; ++y)
    {
        if (town.map().at(0, y).doors[3] == 1)
        {
            edge = y;
        }
    }
    check(edge < 16, "The Slums have a west doorway");
    walk_to(town, party, 0, edge, quest_answer, true, true);
    step(town, party, 3, quest_answer);
    const auto s = town.snapshot();
    check(s.area_id == 29 && s.script_id == 29, "Leaving the Slums westward enters Kuto's Well");
    const auto save = folder / "kutos-well.ogs";
    write_campaign_file(save, encode_campaign(*party, &town, campaign_asset_identity(directory)));
    const auto reloaded = load_expedition(save, directory);
    check(reloaded.town.snapshot().area_id == 29 && reloaded.town.snapshot().script_id == 29,
          "A save in Kuto's Well loads back there");
    // North of the well, kobolds wait in ambush and more climb out behind them.
    const auto lead = party->state().slots[0];
    const auto experience = party->member(lead).experience;
    walk_to(town, party, 7, 4, peaceful, false, true);
    check(party->member(lead).experience > experience,
          "The party wins Kuto's Well fights and earns their XP");
}

// Answers "YES" to climbing down the well and to taking found treasure, picks
// `stand` (FIGHT or SURRENDER) before Norris, and otherwise answers peacefully.
Answer catacomb_answer(std::string stand)
{
    return [stand = std::move(stand)](const por::TourSnapshot &s) -> std::size_t
    {
        if (s.dialogue.find("CLIMB DOWN") != std::string::npos ||
            s.dialogue.find("DO YOU TAKE IT?") != std::string::npos)
        {
            return 0;
        }
        for (std::size_t n = 0; n < s.choices.size(); ++n)
        {
            if (s.choices[n] == stand)
            {
                return n;
            }
        }
        return peaceful(s);
    };
}

unsigned party_coins(const CampaignParty &party)
{
    unsigned coins = 0;
    for (const auto id : party.state().slots)
    {
        if (id)
        {
            for (const auto amount : party.state().wealth)
            {
                coins += amount;
            }
        }
    }
    return coins;
}

std::size_t party_items(const CampaignParty &party)
{
    std::size_t items = 0;
    for (const auto &member : party.state().roster)
    {
        items += member.equipped.size() + member.item_sources.size();
    }
    return items;
}

// For the game play-test (tests/playtest_kutos_well.gd): the campaign saved at
// `from`, walked to a square beside (x, y) and facing it, written to `fixture`.
// With `bash_doors`, the party may stand at a locked door into the target and
// bashes any locked door on the way, answering prompts with `answer`.
void write_approach_fixture(const std::filesystem::path &from, unsigned x, unsigned y,
                            const std::filesystem::path &fixture,
                            const std::filesystem::path &directory, bool bash_doors = false,
                            const Answer &answer = peaceful)
{
    auto trip = load_expedition(from, directory);
    constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
    for (unsigned facing = 0; facing < 4; ++facing)
    {
        const int nx = int(x) - dx[facing], ny = int(y) - dy[facing];
        if (nx < 0 || ny < 0 || nx > 15 || ny > 15)
        {
            continue;
        }
        const auto &cell = trip.town.map().at(unsigned(nx), unsigned(ny));
        if (cell.walls[facing] && cell.doors[facing] != 1 &&
            !(bash_doors && cell.doors[facing] == 2))
        {
            continue;
        }
        walk_to(trip.town, trip.party, unsigned(nx), unsigned(ny), answer, bash_doors, bash_doors);
        face(trip.town, trip.party, facing);
        write_campaign_file(
            fixture, encode_campaign(*trip.party, &trip.town, campaign_asset_identity(directory)));
        return;
    }
    throw std::runtime_error("No open square beside the play-test target");
}

// Below Kuto's Well: an arrow volley, Norris the Gray's band, his treasure and
// the hideout where the party then rests undisturbed. From a save made before
// Norris, surrendering instead costs the party its money and frees it above.
void kutos_well_catacombs(Expedition &trip, const std::filesystem::path &folder,
                          const std::filesystem::path &directory)
{
    auto &[party, town] = trip;
    if (const auto *fixture = std::getenv("OPENGOLD_KUTO_WELL_FIXTURE"))
    {
        const auto plaza = folder / "kutos-plaza.ogs";
        write_campaign_file(plaza,
                            encode_campaign(*party, &town, campaign_asset_identity(directory)));
        write_approach_fixture(plaza, 7, 7, fixture, directory);
        written_fixtures.insert("OPENGOLD_KUTO_WELL_FIXTURE");
    }
    walk_to(town, party, 7, 7, catacomb_answer("FIGHT"), false, true);
    check(town.snapshot().area_id == 32, "The well's rungs lead down to the catacombs");
    face(town, party, 2);
    town.explore(por::ExplorationCommand::forward);
    // Past the catacombs' description, the volley waits on its report.
    for (unsigned n = 0;
         n < 200 && town.snapshot().dialogue.find("\nAn arrow ") == std::string::npos; ++n)
    {
        if (town.snapshot().phase == por::TourPhase::awaiting_continue)
        {
            check(town.choose(town.snapshot().continue_ticket, 0), "The description continues");
        }
        else
        {
            town.advance(.5);
        }
    }
    const auto &volley = town.snapshot();
    check(volley.phase == por::TourPhase::awaiting_continue &&
              volley.dialogue.find("VOLLEY OF ARROWS") != std::string::npos &&
              volley.dialogue.find("\nAn arrow ") != std::string::npos,
          "An arrow volley reports each arrow's attack roll before the story continues");
    settle(town, party, catacomb_answer("FIGHT"));
    // No camping here: until Norris falls, his band interrupts every rest.
    const auto save = folder / "before-norris.ogs";
    write_campaign_file(save, encode_campaign(*party, &town, campaign_asset_identity(directory)));
    if (const auto *fixture = std::getenv("OPENGOLD_KUTO_NORRIS_FIXTURE"))
    {
        write_approach_fixture(save, 10, 3, fixture, directory);
        written_fixtures.insert("OPENGOLD_KUTO_NORRIS_FIXTURE");
    }

    const auto lead = party->state().slots[0];
    const auto experience = party->member(lead).experience;
    walk_to(town, party, 10, 3, catacomb_answer("FIGHT"));
    // The script sets the meeting at distance 2 and walks the band up twice
    // (ECL8:29 0x9D23-0x9D34): Norris stands right in front of the leader.
    const auto &norris = fights_started.back();
    check(norris.pose.facing == por::MapDirection::north && norris.distance == 0 &&
              norris.cells.at(1000) == rules::Cell{27, 12} &&
              norris.cells.at(lead) == rules::Cell{27, 13},
          "Norris's band starts the fight directly in front of the party");
    check(town.script_variable(por::EclAddress{0x4A24}) == 255 &&
              party->member(lead).experience > experience,
          "The party defeats Norris the Gray's band and earns his XP");
    const auto coins = party_coins(*party);
    walk_to(town, party, 13, 1, catacomb_answer("FIGHT"), false, true);
    check(party_coins(*party) > coins, "The bandits' treasure is taken");
    check(town.camp(RestKind::short_rest), "Camp starts in the hideout");
    settle(town, party);
    check(town.snapshot().dialogue.find("Short rest complete") != std::string::npos,
          "With Norris gone, the party rests undisturbed in his hideout");
    party->finish_short_rest(party->state().short_rest->ticket);

    auto captive = load_expedition(save, directory);
    const auto items = party_items(*captive.party);
    check(party_coins(*captive.party) > 0, "The party carries money before surrendering");
    walk_to(captive.town, captive.party, 10, 3, catacomb_answer("SURRENDER"));
    check(party_coins(*captive.party) == 0 && party_items(*captive.party) == items &&
              captive.town.snapshot().area_id == 29,
          "Surrendering to Norris costs all money, keeps items and frees the party above");
}

// The Slums' set encounters with converted creatures (docs/audits/cluebook-check.md):
// the hobgoblins arguing over gold and the gathering of monster leaders are
// fought, and the trolls and ogres reach combat with the treasure they guard.
// Walks toward (x, y) for the event there. Some events move the party off the
// square afterwards, so not arriving is accepted; any other failure is not.
void visit(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party, unsigned x,
           unsigned y, const Answer &answer)
{
    try
    {
        walk_to(town, party, x, y, answer, true, true);
    }
    catch (const std::runtime_error &e)
    {
        if (std::string_view(e.what()) != "Walk did not arrive")
        {
            throw;
        }
    }
}

// Steps off the map toward an area the game does not have yet: the step is
// refused with a notice naming it, and the party stays where it was.
void refused_exit(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party,
                  unsigned facing, std::string_view area)
{
    face(town, party, facing);
    const auto before = town.snapshot().pose;
    const auto context = area_context(town);
    town.explore(por::ExplorationCommand::forward);
    bool refused = false;
    for (unsigned n = 0; n < 200 && town.snapshot().phase != por::TourPhase::completed; ++n)
    {
        const auto &s = town.snapshot();
        if (s.phase == por::TourPhase::awaiting_continue)
        {
            refused |= s.dialogue.find("The way to " + std::string(area) +
                                       " is not in the game yet") != std::string::npos;
            check(town.choose(s.continue_ticket, peaceful(s)), "Acknowledge the notice");
        }
        else
        {
            town.advance(.5);
        }
    }
    check(refused && town.snapshot().pose == before && area_context(town) == context,
          "The way to " + std::string(area) +
              " is refused and the party stays, its area untouched");
}

// Restores every living member's Hit Points, so one check's fight does not
// decide the next.
void heal(CampaignParty &party)
{
    auto state = party.checkpoint();
    for (auto &member : state.roster)
    {
        // Through the rules, so a downed member's stable recovery clock ends too.
        if (!member.vitals.dead)
        {
            party.rule_module().set_hit_points(
                member.vitals, member.character.sheet(),
                std::min(member.character.sheet().hit_points, party.hit_point_maximum(member.id)));
        }
    }
    party.restore(std::move(state));
}

// A level-3 Wizard with Knock prepared, recruited to open Podol's wizard-locked
// doors.
Character knock_wizard()
{
    rules::CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "wizard";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Gwen";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    d.cantrips = std::vector<std::string>{"fire_bolt", "ray_of_frost", "chill_touch"};
    const std::vector<std::string> spells{"magic_missile"};
    d.spells = rules::SpellChoices{{{"spellbook:1", spells}}, spells, {}, {}};
    d.training = {{"class:wizard", {"medicine", "nature"}}};
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(900, "wizard-xp");
    for (unsigned level = 2; level <= 3; ++level)
    {
        auto choice = party.default_advancement(id);
        if (level == 3)
        {
            (*choice.spell_learning)["spellbook:3"] = {"knock", "shatter"};
            choice.spells = party.member(id).character.sheet().prepared_spells;
            choice.spells.insert(choice.spells.end(), {"knock", "shatter"});
        }
        party.advance(id, choice);
    }
    return party.member(id).character;
}

// Podol's hidden temple of Ilmater, behind wizard-locked doors (PODOL-1): Bash
// fails against DC 35 and Knock opens the door; the healer's YES opens the
// temple's Heal list at its own prices, with its priest's portrait; camping there is never
// interrupted.
void podol_temple(Expedition &trip)
{
    auto &[party, town] = trip;
    party->recruit("test:knock-wizard", knock_wizard());
    auto offerings = original_temple_offerings();
    offerings.at(2).price_gp = 90; // Ilmater's Cure Light Wounds, in its own record
    town.temples({{"GEO1.DAX", 18, {{1, 15}}, offerings, "shops/temple-ilmater.png"}});
    walk_to(town, party, 2, 13);
    face(town, party, 3);
    const auto knock_door = [&](std::string_view method)
    {
        town.explore(por::ExplorationCommand::forward);
        for (unsigned n = 0; n < 50 && town.snapshot().phase == por::TourPhase::running; ++n)
        {
            town.advance(.5);
        }
        const auto &s = town.snapshot();
        check(s.dialogue == "Wizard locked." && s.choices.back() == "Exit",
              "The temple's door is wizard locked");
        const auto chosen = std::find(s.choices.begin(), s.choices.end(), method);
        check(chosen != s.choices.end(), std::string(method) + " is offered");
        check(town.choose(s.continue_ticket, static_cast<std::size_t>(chosen - s.choices.begin())),
              "Try the door");
    };
    knock_door("Bash");
    const auto bashes = town.snapshot().door_checks;
    check(!bashes.empty() && bashes.back().difficulty == 35 && !town.snapshot().door_opened,
          "Bashing a wizard-locked door is a DC 35 check, and it holds");
    settle(town, party);
    check(town.snapshot().pose.x == 2, "The party is still outside");
    knock_door("Knock");
    check(town.snapshot().door_opened, "Knock opens the wizard-locked door");
    settle(town, party);
    check(town.snapshot().pose.x == 1 && town.snapshot().pose.y == 13, "The party is inside");
    // The healer at (1,15).
    auto state = party->checkpoint();
    auto &hurt = state.roster.front();
    hurt.vitals.hit_points = 1;
    party->restore(std::move(state));
    const auto hurt_id = party->state().roster.front().id;
    const auto gold = coins(party->state().wealth, Coin::gold);
    bool healed = false;
    walk_to(town, party, 1, 15,
            [&](const por::TourSnapshot &s) -> std::size_t
            {
                const auto pick = [&](std::string_view label)
                {
                    for (std::size_t n = 0; n < s.choices.size(); ++n)
                    {
                        if (s.choices[n].starts_with(label))
                        {
                            return n;
                        }
                    }
                    throw std::runtime_error("No choice " + std::string(label));
                };
                if (s.dialogue.find("IN NEED OF HEALING") != std::string::npos)
                {
                    return pick("YES");
                }
                if (s.dialogue.starts_with("The clerics cast "))
                {
                    healed = true;
                    return pick("Leave");
                }
                if (s.dialogue == "The clerics will cast these spells for the party.")
                {
                    check(std::find(s.choices.begin(), s.choices.end(),
                                    "Cure Light Wounds (90 gp)") != s.choices.end(),
                          "The temple of Ilmater charges its own record's prices");
                    check(speaking_npc_portrait(s, town.shops(), town.temples()) ==
                              "shops/temple-ilmater.png",
                          "Ilmater's priest presides over the Heal list");
                    return pick("Cure Light Wounds");
                }
                if (s.dialogue == "Choose who receives Cure Light Wounds.")
                {
                    return pick(party->member(hurt_id).character.sheet().name);
                }
                if (s.dialogue.starts_with("The clerics will cast Cure Light Wounds on "))
                {
                    return pick("Pay");
                }
                return peaceful(s);
            });
    check(healed && party->member(hurt_id).vitals.hit_points > 1 &&
              coins(party->state().wealth, Coin::gold) == gold - 90,
          "The temple of Ilmater heals a member for 90 gp");
    // The healer says the party may rest here as long as it likes.
    for (unsigned rest = 0; rest < 3; ++rest)
    {
        check(town.camp(RestKind::short_rest), "Camp in the temple");
        settle(town, party);
        check(town.snapshot().dialogue.find("Short rest complete") != std::string::npos &&
                  party->state().short_rest,
              "Nothing interrupts a rest in the temple");
        party->finish_short_rest(party->state().short_rest->ticket);
    }
}

// Answers with the first of `labels` a menu offers, else peacefully.
Answer prefer(std::vector<std::string> labels)
{
    return [labels = std::move(labels)](const por::TourSnapshot &s) -> std::size_t
    {
        for (const auto &label : labels)
        {
            for (std::size_t n = 0; n < s.choices.size(); ++n)
            {
                if (s.choices[n] == label)
                {
                    return n;
                }
            }
        }
        return peaceful(s);
    };
}

// Steps off a city block's edge onto the wilderness map at the city's western
// edge (11,28), leaves its menu, and comes back in by `entrance`.
void out_to_city_edge(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party,
                      unsigned facing, const std::string &entrance)
{
    step(town, party, facing, prefer({"LEAVE"}));
    const auto &s = town.snapshot();
    check(s.overland && s.script_id == 26 && s.overland_x == 11 && s.overland_y == 28,
          "The city's west edge leads onto the wilderness map at (11,28)");
    check(town.explore(por::ExplorationCommand::north), "Step away from the city");
    settle(town, party);
    check(town.explore(por::ExplorationCommand::south), "Step back to the city");
    settle(town, party, prefer({entrance}));
}

// Steps once into the Pitt (4,8) from its door, answering with `answer`.
void enter_pitt(Expedition &trip, const Answer &answer)
{
    auto &[party, town] = trip;
    walk_to(town, party, 5, 8, answer);
    step(town, party, 3, answer);
}

// The Pitt (docs/audits/podol-plaza.md, PODOL-1): the drunk buccaneer's
// bribe, his duel with one member, the patrons' fight, heavy drinking and the
// free-for-all, each played from the save made on entering the plaza.
void podol_pitt(const std::filesystem::path &save, const std::filesystem::path &directory)
{
    const auto pitt_state = [](const Expedition &trip)
    {
        return trip.town.script_variable(por::EclAddress{0x4A37});
    };
    {
        auto trip = load_expedition(save, directory);
        const auto coins = party_coins(*trip.party);
        const auto items = party_items(*trip.party);
        enter_pitt(trip, prefer({"BRIBE", "YES"}));
        check(party_coins(*trip.party) < coins && party_coins(*trip.party) >= coins * 8 / 10 &&
                  party_items(*trip.party) == items && pitt_state(trip) == 0,
              "The bribe costs a tenth of the money and no items, and he comes back");
    }
    // The duel: only the chosen member fights the buccaneer, the SRD Pirate.
    const auto duel = [&](const Answer &later)
    {
        auto trip = load_expedition(save, directory);
        const auto lead = trip.party->state().slots[0];
        const auto fought = fights_started.size();
        enter_pitt(trip, prefer({"CHALLENGE"}));
        check(fights_started.size() == fought + 1 && fights_started.back().cells.size() == 2 &&
                  fights_started.back().cells.contains(lead) && pitt_state(trip) == 1,
              "The challenger alone fights the buccaneer");
        check(std::find(enemies_fought.end() - 1, enemies_fought.end(), "buccaneer") !=
                  enemies_fought.end(),
              "The buccaneer is the SRD Pirate");
        // Back out, then in again: the patrons ignore the party now.
        enter_pitt(trip, later);
        return trip;
    };
    {
        auto trip = duel(prefer({"DRINK HEAVILY"}));
        const auto items = party_items(*trip.party);
        check(party_coins(*trip.party) == 0 && party_items(*trip.party) == items &&
                  trip.town.snapshot().pose.x == 2 && trip.town.snapshot().pose.y == 10,
              "Drinking heavily, the party wakes on (2,10) robbed of its money but not its items");
    }
    {
        auto trip = duel(prefer({"ATTACK"}));
        check(pitt_state(trip) >= 2, "Attacking the patrons wrecks the bar");
    }
    {
        const auto fought = fights_started.size();
        auto trip = duel(prefer({"MIX WITH MONSTERS", "INSULT BACK"}));
        check(fights_started.size() > fought + 1 && trip.town.script_diagnostics().empty(),
              "Insulting back starts the free-for-all, which the party wins");
    }
    // A champion on 1 Hit Point who loses the duel: not the party's defeat; he
    // is left on 1 Hit Point, as in the original. Seeds are tried until the
    // buccaneer wins one.
    for (std::uint64_t seed = 1; seed < 40; ++seed)
    {
        auto trip = load_expedition(save, directory);
        auto state = trip.party->checkpoint();
        state.roster.front().vitals.hit_points = 1;
        trip.party->restore(std::move(state));
        const auto lead = trip.party->state().roster.front().id;
        auto &[party, town] = trip;
        walk_to(town, party, 5, 8);
        face(town, party, 3);
        town.explore(por::ExplorationCommand::forward);
        for (unsigned n = 0; n < 2000 && town.snapshot().phase != por::TourPhase::combat; ++n)
        {
            const auto &s = town.snapshot();
            if (s.phase == por::TourPhase::awaiting_continue)
            {
                check(town.choose(s.continue_ticket, prefer({"CHALLENGE"})(s)), "Answer");
            }
            else
            {
                town.advance(.5);
            }
        }
        if (town.monster_picture())
        {
            check(town.start_encounter(), "The close-up is dismissed");
        }
        CombatDemo combat(module());
        combat.campaign_party(party);
        combat.encounter(*town.pending_encounter(), seed);
        for (unsigned n = 0;
             n < 4000 && combat.combat().snapshot().outcome == rules::Outcome::ongoing; ++n)
        {
            check(combat.submit(choose_demo_command(combat.combat(), Searching::party)),
                  "Combat accepts the command");
        }
        const auto result = combat.combat().snapshot();
        if (result.outcome != rules::Outcome::defeat)
        {
            continue;
        }
        check(town.resolve_combat(result) && town.snapshot().phase != por::TourPhase::defeated &&
                  town.script_variable(por::ecl_combat_result) == 128,
              "A lost duel is not the party's defeat; the script hears 128");
        check(!party->member(lead).vitals.dead && party->member(lead).vitals.hit_points == 1,
              "The champion is left on 1 Hit Point");
        settle(town, party);
        return;
    }
    throw std::runtime_error("No seed's buccaneer wins the duel");
}

// Enters Podol Plaza by Kuto's Well's west edge, answering Podol's events
// with `answer`.
void enter_podol(Expedition &trip, const Answer &answer = peaceful)
{
    auto &[party, town] = trip;
    unsigned edge = 16;
    for (unsigned y = 0; y < 16 && edge == 16; ++y)
    {
        const auto &cell = town.map().at(0, y);
        if (!cell.walls[3] || cell.doors[3] == 1)
        {
            edge = y;
        }
    }
    check(edge < 16, "Kuto's Well has a west way out");
    walk_to(town, party, 0, edge);
    step(town, party, 3, answer);
    check(town.snapshot().area_id == 18 && town.snapshot().script_id == 18,
          "Leaving Kuto's Well westward enters Podol Plaza");
}

// Podol Plaza on the council's mission to learn what is auctioned there,
// from a save in Kuto's Well. City Hall offers that commission only after one
// of its first three is done, which this route does not do, so the test sets
// the commission's flag (0x4AB0) itself.
void podol_mission(const std::filesystem::path &kuto, const std::filesystem::path &directory)
{
    // Each try reseeds the script's dice: whether a group or a rumour comes
    // first is a roll, and a fight on the plaza blows the party's cover.
    const auto commissioned = [&](std::uint32_t seed)
    {
        auto trip = load_expedition(kuto, directory);
        opengold::test::TourAccess::set_variable(trip.town, por::EclAddress{0x4AB0}, 1);
        opengold::test::TourAccess::seed_dice(trip.town, seed);
        return trip;
    };
    const auto approach = [](const Expedition &trip)
    {
        return trip.town.script_variable(por::EclAddress{0x4A35});
    };
    // Disguised as monsters: the party overhears rumours on the plaza, waits
    // out the auction, and greets the robed orc. False when its cover was
    // blown first.
    const auto disguised_visit = [&](std::uint32_t seed)
    {
        auto trip = commissioned(seed);
        auto &[party, town] = trip;
        // A haughty answer passes most questionings by a party this strong.
        const auto disguised = prefer({"DISGUISE PARTY AS MONSTERS.", "HAUGHTY", "Fight",
                                       "STAND AND LISTEN", "WAIT FOR WINNER", "GREET", "YES"});
        enter_podol(trip, disguised);
        step(town, party, 3, disguised);
        check(town.script_variable(por::EclAddress{0x4A34}) == 64,
              "On the mission, the party chooses how to cross the plaza");
        // Back and forth across the plaza, a step at a time: a rumour is
        // printed with no menu after it.
        walk_to(town, party, 8, 9, disguised);
        bool rumour = false;
        for (unsigned steps = 0; steps < 120 && !rumour && approach(trip) == 1; ++steps)
        {
            heal(*party);
            step(town, party, steps % 2 ? 1 : 3, disguised);
            rumour =
                town.snapshot().dialogue.find("AS YOU MOVE YOU OVERHEAR SOME") != std::string::npos;
        }
        if (!rumour || approach(trip) != 1)
        {
            return false;
        }
        heal(*party);
        visit(town, party, 7, 7, disguised);
        check(town.script_variable(por::EclAddress{0x4AB0}) == 254 && approach(trip) == 254,
              "Waiting out the auction learns what is sold: the mission is done");
        // The robed orc greets a disguised party and names a meeting.
        heal(*party);
        walk_to(town, party, 13, 8, disguised);
        step(town, party, 1, disguised);
        check(town.script_variable(por::EclAddress{0x4A38}) == 1,
              "The robed orc recruits the party and names a meeting in two hours");
        // Out of the shrine, three hours, then in again for the meeting.
        walk_to(town, party, 13, 8, disguised);
        party->advance_time(std::chrono::hours(3));
        heal(*party);
        const auto fought = fights_started.size();
        step(town, party, 1, disguised);
        check(fights_started.size() > fought && town.script_variable(por::EclAddress{0x4A38}) == 2,
              "At the meeting the creatures gathered there test the party's loyalty on it");
        return true;
    };
    bool disguised = false;
    for (std::uint32_t seed = 1; seed <= 10 && !disguised; ++seed)
    {
        disguised = disguised_visit(seed);
    }
    check(disguised, "On some seed a disguised party overhears a rumour before a fight");
    // Sneaking: the crowd always notices a sneaking party at the auction,
    // which is cancelled; with no roaming group met this visit, a fresh plaza
    // group attacks (PODOL-1).
    const auto sneaking_visit = [&](std::uint32_t seed)
    {
        auto trip = commissioned(seed);
        auto &[party, town] = trip;
        const auto sneaking =
            prefer({"SNEAK, REMAINING UNSEEN", "HAUGHTY", "Fight", "MOVE IN CLOSER"});
        enter_podol(trip, sneaking);
        step(town, party, 3, sneaking);
        const auto fought = fights_started.size();
        heal(*party);
        visit(town, party, 7, 7, sneaking);
        // Only the auction's own fight counts: a roaming group met on the way
        // blows the party's cover before it gets there.
        if (fights_started.size() != fought + 1 || fights_started.back().pose.x != 7)
        {
            return false;
        }
        check(town.script_diagnostics().empty() &&
                  town.script_variable(por::EclAddress{0x4AB0}) == 254,
              "The cancelled auction's crowd, a fresh plaza group, attacks the party");
        return true;
    };
    bool cancelled = false;
    for (std::uint32_t seed = 1; seed <= 10 && !cancelled; ++seed)
    {
        cancelled = sneaking_visit(seed);
    }
    check(cancelled, "On some seed a sneaking party reaches the auction unseen");
}

// City Hall's clerk (ECL3:8, (5,5), past the guards at (4,5)) pays for Podol
// Plaza from reward slot 10, 250 gp and 200 pp: once for the auction mission
// (0x4AB0 = 254) and once for clearing the plaza (0x4AE7 = 254), each flag
// then 255. A new party in New Phlan stands in for one back from the plaza,
// with the flag Podol's script sets.
void city_hall_rewards(const std::filesystem::path &directory)
{
    for (const auto flag : {por::EclAddress{0x4AB0}, por::EclAddress{0x4AE7}})
    {
        auto trip = create_party(directory);
        auto &[party, town] = trip;
        opengold::test::TourAccess::set_variable(town, flag, 254);
        bool praised = false;
        const Answer clerk = [&](const por::TourSnapshot &s) -> std::size_t
        {
            praised = praised || s.dialogue.find("PODAL PLAZA") != std::string::npos;
            return peaceful(s);
        };
        walk_to(town, party, 4, 5);
        const auto before = party->state().wealth;
        walk_to(town, party, 5, 5, clerk);
        auto paid = before;
        coins(paid, Coin::gold) += 250;
        coins(paid, Coin::platinum) += 200;
        check(praised && party->state().wealth == paid && town.script_variable(flag) == 255,
              "City Hall's clerk pays 250 gp and 200 pp for Podol Plaza");
        walk_to(town, party, 4, 5);
        walk_to(town, party, 5, 5, clerk);
        check(party->state().wealth == paid, "City Hall pays for Podol Plaza only once");
    }
}

// City Hall's clerk pays Cadorna's 20 gems (reward slot 18) for the family
// treasure brought back sealed (0x4AC8 = 1), which no original script sets
// (CADORNA-1); once paid, 0x4AB8 is 255. An opened box (128) earns Cadorna's
// threat on entering City Hall, which settles the payment unpaid (0x4AB8 =
// 255). A new party in New Phlan stands in for one back
// from the textile house, with the flag that script sets.
void cadorna_payment(const std::filesystem::path &directory)
{
    for (const std::uint16_t box : {1, 128})
    {
        auto trip = create_party(directory);
        auto &[party, town] = trip;
        opengold::test::TourAccess::set_variable(town, por::EclAddress{0x4AC8}, box);
        bool thanked = false;
        const Answer clerk = [&](const por::TourSnapshot &s) -> std::size_t
        {
            thanked = thanked || s.dialogue.find("CADORNA LEFT THIS PAYMENT") != std::string::npos;
            return peaceful(s);
        };
        walk_to(town, party, 4, 5, clerk);
        const auto before = party->state().wealth;
        walk_to(town, party, 5, 5, clerk);
        auto paid = before;
        if (box == 1)
        {
            coins(paid, Coin::gems) += 20;
        }
        check(thanked == (box == 1) && party->state().wealth == paid &&
                  town.script_variable(por::EclAddress{0x4AB8}) == 255,
              box == 1 ? "City Hall's clerk pays 20 gems for Cadorna's sealed treasure"
                       : "Cadorna pays nothing to a party that opened his box");
    }
}

// Whether (x, y) can be walked to from `from` over open edges and unlocked doors.
bool reachable(const por::GeoMap &map, por::PartyPose from, unsigned x, unsigned y)
{
    constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
    std::set<unsigned> seen{from.y * 16 + from.x};
    std::queue<unsigned> cells;
    cells.push(from.y * 16 + from.x);
    while (!cells.empty())
    {
        const auto cell = cells.front();
        cells.pop();
        if (cell == y * 16 + x)
        {
            return true;
        }
        for (unsigned d = 0; d < 4; ++d)
        {
            const int nx = int(cell % 16) + dx[d], ny = int(cell / 16) + dy[d];
            if (nx < 0 || ny < 0 || nx >= 16 || ny >= 16)
            {
                continue;
            }
            const auto &a = map.at(cell % 16, cell / 16);
            const auto &b = map.at(nx, ny);
            const unsigned reverse = (d + 2) % 4;
            const auto blocked = [](unsigned wall, unsigned door)
            {
                return door > 1 || (wall && !door);
            };
            if (!blocked(a.walls[d], a.doors[d]) && !blocked(b.walls[reverse], b.doors[reverse]) &&
                seen.insert(ny * 16 + nx).second)
            {
                cells.push(ny * 16 + nx);
            }
        }
    }
    return false;
}

// A square on the map's edge facing `facing` (0 north to 3 west) that opens
// off the map and that the party can walk to.
por::PartyPose edge_opening(const por::GeoMap &map, por::PartyPose party, unsigned facing)
{
    for (unsigned n = 0; n < 16; ++n)
    {
        const unsigned x = facing == 1 ? 15 : facing == 3 ? 0 : n;
        const unsigned y = facing == 0 ? 0 : facing == 2 ? 15 : n;
        const auto &cell = map.at(x, y);
        if ((!cell.walls[facing] || cell.doors[facing] == 1) && reachable(map, party, x, y))
        {
            return {x, y, static_cast<por::MapDirection>(facing)};
        }
    }
    check(false, "The map has a way out on that edge");
    return {};
}

// Plays every step until `done` holds, walking back and forth over the
// squares between (x1,y) and (x2,y), at most `steps` times.
bool pace_until(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party,
                unsigned x1, unsigned x2, unsigned y, const std::function<bool()> &done,
                const Answer &answer, std::string_view typed, unsigned steps = 200)
{
    for (unsigned n = 0; n < steps && !done(); ++n)
    {
        const auto at = town.snapshot().pose;
        const unsigned target = at.x == x1 ? x2 : x1;
        if (at.y != y)
        {
            walk_to(town, party, x1, y, answer, false, false, typed);
            continue;
        }
        step(town, party, target > at.x ? 1 : 3, answer, typed);
        recover(town, party);
    }
    return done();
}

// Whether any member carries an item of the definition.
bool carried(const CampaignParty &party, std::string_view definition)
{
    for (const auto &member : party.state().roster)
    {
        for (const auto &item : member.character.inventory().items())
        {
            if (item.definition_id == definition)
            {
                return true;
            }
        }
    }
    return false;
}

// Whether any member holds an item of the original item type.
bool holds_original(const CampaignParty &party, unsigned type)
{
    for (const auto &member : party.state().roster)
    {
        for (const auto &[id, source] : member.item_sources)
        {
            if (source.stored.type == type)
            {
                return true;
            }
        }
    }
    return false;
}

// Looks where the party stands, up to `tries` times, until `found` holds.
bool look_until(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party,
                const std::function<bool()> &found, const Answer &answer, unsigned tries = 60)
{
    for (unsigned n = 0; n < tries && !found(); ++n)
    {
        town.explore(por::ExplorationCommand::look);
        settle(town, party, answer);
    }
    return found();
}

// Sokal Keep (docs/audits/sokal-keep.md), by boat from New Phlan's docks:
// the harbor master's free passage while the keep is haunted, the dead elf's
// scroll read translated, the undead patrol sent off with SHESTNI, the
// haunts' diary and gems for LUX, the frogs and the smithy's hammer, the
// insects, the orc and hobgoblin raid, the illusory wall's arms and armor,
// Ferran Martinez told the truth, the patrol passed with SAMOSUD after, and
// the boat home.
void sokal_keep(const std::filesystem::path &directory)
{
    auto trip = create_party(directory);
    buy_and_equip(trip);
    auto &[party, town] = trip;
    party->award_experience(6 * 900, "test:sokal-keep-level");
    for (const auto id : party->state().slots)
    {
        while (id && party->member(id).character.sheet().level < 3)
        {
            party->advance(id, party->default_advancement(id));
        }
    }
    const auto variable = [&](std::uint16_t address)
    {
        return town.script_variable(por::EclAddress{address});
    };
    // The harbor master, met stepping north onto the dock at (11,1), books
    // the only passage; the boat leaves from the pier's end at (15,1).
    walk_to(town, party, 11, 2);
    step(town, party, 0);
    check(town.script_variable(por::EclAddress{0x4A01}) == 1,
          "The harbor master books the only passage, to Sokal Keep");
    walk_to(town, party, 15, 1);
    check(town.snapshot().area_id == 21 && town.snapshot().script_id == 21,
          "The harbor master's boat takes the party to Sokal Keep");
    const auto folder = std::filesystem::temp_directory_path() / "opengold-sokal";
    std::filesystem::create_directories(folder);
    const auto keep = folder / "sokal-keep.ogs";
    write_campaign_file(keep, encode_campaign(*party, &town, campaign_asset_identity(directory)));
    check(load_expedition(keep, directory).town.snapshot().area_id == 21,
          "A save in Sokal Keep loads back there");
    const auto fight_them = prefer({"COMBAT", "Fight", "YES", "TELL THE TRUTH?"});
    const auto talk = prefer({"Advance", "Parley", "NICE", "YES", "TELL THE TRUTH?", "SEARCH"});

    // The dead elf's scroll, read by looking, holds three words.
    visit(town, party, 6, 13, talk);
    std::string scroll;
    town.explore(por::ExplorationCommand::look);
    settle(town, party,
           [&](const por::TourSnapshot &s) -> std::size_t
           {
               scroll += s.dialogue;
               return talk(s);
           });
    check(scroll.find("LUX") != std::string::npos && scroll.find("SAMOSUD") != std::string::npos &&
              scroll.find("SHESTNI") != std::string::npos,
          "The elf's scroll shows its three words translated");

    // The undead patrol leaves for the password.
    heal(*party);
    check(pace_until(
              town, party, 4, 6, 6,
              [&]
              {
                  return variable(0x4A19) == 1;
              },
              talk, "SHESTNI"),
          "The undead patrol leaves when told SHESTNI");

    // The barracks' haunts, answered with LUX, point to the diary and gems.
    check(pace_until(
              town, party, 4, 6, 3,
              [&]
              {
                  return variable(0x4A29) == 255;
              },
              talk, "LUX", 12),
          "The haunts tell of the diary, and the gems are taken");

    // The frogs of the smithy, then its old hammer, found by looking.
    heal(*party);
    check(pace_until(
              town, party, 13, 14, 7,
              [&]
              {
                  return variable(0x4A0E) == 255;
              },
              fight_them, "", 12),
          "The smithy's frogs are beaten");
    visit(town, party, 15, 6, fight_them);
    check(look_until(
              town, party,
              [&]
              {
                  return variable(0x4A28) == 255;
              },
              fight_them) &&
              holds_original(*party, 20),
          "Looking in the smithy finds the Hammer +1");

    // The insects, the raid, and the arms behind the illusory wall.
    heal(*party);
    visit(town, party, 2, 11, fight_them);
    check(variable(0x4A0B) == 255, "The insects are beaten");
    heal(*party);
    visit(town, party, 7, 4, fight_them);
    check(variable(0x4AD7) == 255, "The orc and hobgoblin raid is beaten");
    visit(town, party, 12, 0, fight_them);
    check(variable(0x4A21) == 255, "The stands' arms and armor are taken");

    // Ferran Martinez, given LUX and the truth, frees the keep.
    walk_to(town, party, 7, 10, talk, true, true, "LUX");
    check(variable(0x4A26) == 255 && variable(0x4AA7) == 254,
          "Ferran Martinez, told the truth, frees the keep's spirits");
    // Ferran's word, SAMOSUD, now passes the patrols.
    check(variable(0x4A19) == 0 && pace_until(
                                       town, party, 4, 6, 6,
                                       [&]
                                       {
                                           return variable(0x4A19) == 1;
                                       },
                                       talk, "SAMOSUD"),
          "After Ferran, the undead patrol leaves when told SAMOSUD");

    // The boat home.
    const auto south = edge_opening(town.map(), town.snapshot().pose, 2);
    walk_to(town, party, south.x, south.y, talk);
    step(town, party, 2, prefer({"YES"}));
    check(town.snapshot().area_id == 0, "The boat takes the party back to New Phlan");
    std::filesystem::remove_all(folder);
}

// One step on the wilderness map in `direction` (0 north and on clockwise to
// 7), played to its end.
void travel(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party,
            unsigned direction, const Answer &answer = peaceful)
{
    check(town.explore(static_cast<por::ExplorationCommand>(
              static_cast<unsigned>(por::ExplorationCommand::north) + direction)),
          "The party sets off across the wilderness");
    settle(town, party, answer);
}

// Walks out of the central wilderness's grove or ruined huts: the squares at
// its edge (event 10) lead back out to the wilderness map.
void leave_grove(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party)
{
    std::optional<std::pair<unsigned, unsigned>> border;
    for (unsigned y = 0; y < 16 && !border; ++y)
    {
        for (unsigned x = 0; x < 16 && !border; ++x)
        {
            if (town.map().at(x, y).event_number() == 10 &&
                reachable(town.map(), town.snapshot().pose, x, y))
            {
                border = {x, y};
            }
        }
    }
    check(border.has_value(), "The grove or the huts have a way out");
    // The first edge square the walk reaches leads out.
    try
    {
        walk_to(town, party, border->first, border->second, peaceful, true, true);
    }
    catch (const std::runtime_error &)
    {
        check(town.snapshot().overland, "The walk ends only on leaving the grove or the huts");
    }
}

// Wilderness squares whose sites lead to areas not in the game yet, by
// script * 100 + x and y: a journey goes around them, as stepping on one
// only refuses and steps back.
std::set<std::pair<unsigned, unsigned>> closed_sites;

// The first of the eight directions on a shortest way across the party's
// wilderness window from (from_x,from_y) to (x,y), over squares the party may
// enter: rivers only once the Pyramid's valley is healed.
std::optional<unsigned> overland_heading(const por::RolfTourSession &town, unsigned from_x,
                                         unsigned from_y, unsigned x, unsigned y)
{
    const auto &world = opengold::test::TourAccess::overland_world(town);
    const bool rivers = town.script_variable(por::EclAddress{0x4AA2}) != 0;
    constexpr unsigned columns = por::OverlandWorld::window_columns,
                       rows = por::OverlandWorld::rows;
    // Scripts 25-27 are the western, central and eastern windows; the central
    // window's first two columns and the eastern's first three lead west.
    const unsigned window = town.snapshot().script_id - 25;
    const int first_column = window == 1 ? 2 : window == 2 ? 3 : 0;
    const auto start_column = world.window_start[window];
    const auto open = [&](unsigned cx, unsigned cy)
    {
        const auto passage = por::overland_passage(world.at(start_column + cx, cy));
        return passage == por::OverlandPassage::open ||
               (rivers && passage == por::OverlandPassage::river);
    };
    // Breadth-first from the goal, so each square knows its step toward it.
    std::vector<int> distance(columns * rows, -1);
    std::queue<std::pair<unsigned, unsigned>> squares;
    distance[y * columns + x] = 0;
    squares.push({x, y});
    while (!squares.empty())
    {
        const auto [cx, cy] = squares.front();
        squares.pop();
        for (unsigned direction = 0; direction < 8; ++direction)
        {
            const int nx = int(cx) + por::overland_dx[direction],
                      ny = int(cy) + por::overland_dy[direction];
            if (nx < first_column || ny < 0 || nx >= int(columns) || ny >= int(rows) ||
                distance[ny * columns + nx] >= 0 || !open(unsigned(nx), unsigned(ny)) ||
                closed_sites.contains({(window + 25) * 100 + unsigned(nx), unsigned(ny)}))
            {
                continue;
            }
            distance[ny * columns + nx] = distance[cy * columns + cx] + 1;
            squares.push({unsigned(nx), unsigned(ny)});
        }
    }
    for (unsigned direction = 0; direction < 8; ++direction)
    {
        const int nx = int(from_x) + por::overland_dx[direction],
                  ny = int(from_y) + por::overland_dy[direction];
        if (nx >= 0 && ny >= 0 && nx < int(columns) && ny < int(rows) &&
            distance[ny * columns + nx] >= 0 &&
            distance[ny * columns + nx] < distance[from_y * columns + from_x])
        {
            return direction;
        }
    }
    return std::nullopt;
}

// Steps across the wilderness map toward (x,y) by a shortest way, until it
// arrives or `arrived` holds.
void travel_to(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party, unsigned x,
               unsigned y, const Answer &answer = peaceful,
               const std::function<bool()> &arrived = {})
{
    for (unsigned steps = 0; steps < 80; ++steps)
    {
        if (arrived && arrived())
        {
            return;
        }
        const auto &s = town.snapshot();
        // A roaming roll may lead into the grove or the huts on the way.
        if (!s.overland && s.area_id == 26 && s.script_id == 26)
        {
            leave_grove(town, party);
            continue;
        }
        if (!s.overland || (s.overland_x == x && s.overland_y == y))
        {
            return;
        }
        const auto heading = overland_heading(town, s.overland_x, s.overland_y, x, y);
        check(heading.has_value(), "The wilderness has a way to the destination");
        travel(town, party, *heading, answer);
    }
    throw std::runtime_error("The wilderness journey did not arrive");
}

// The Central Wilderness (docs/audits/central-wilderness.md): the harbor's
// western boat to the wilderness map once Sokal Keep is freed, its view and a
// save there, the sea that stops a step, half a day a step, and the city's
// western edge, whose south entrance leads into the Cadorna Textile House.
void central_wilderness(const std::filesystem::path &directory)
{
    auto trip = create_party(directory);
    buy_and_equip(trip);
    auto &[party, town] = trip;
    party->award_experience(6 * 6500, "test:wilderness-level");
    for (const auto id : party->state().slots)
    {
        while (id && party->member(id).character.sheet().level < 5)
        {
            party->advance(id, party->default_advancement(id));
        }
    }
    opengold::test::TourAccess::set_variable(town, por::EclAddress{0x4AA7}, 255);
    walk_to(town, party, 11, 2);
    step(town, party, 0, prefer({"WEST"}));
    check(town.script_variable(por::EclAddress{0x4A01}) == 1,
          "Once Sokal Keep is freed, the harbor master sells passage west");
    walk_to(town, party, 15, 1, prefer({"STAY"}));
    const auto &s = town.snapshot();
    check(s.overland && s.script_id == 26 && s.overland_x == 7 && s.overland_y == 29,
          "The western boat lands the party on the wilderness map at (7,29)");
    const auto view = town.observe_view();
    check(view.width == 120 && view.height == 120,
          "The wilderness view shows the 5 x 5 squares around the party");
    const auto folder = std::filesystem::temp_directory_path() / "opengold-wilderness";
    std::filesystem::create_directories(folder);
    const auto save = folder / "central-wilderness.ogs";
    write_campaign_file(save, encode_campaign(*party, &town, campaign_asset_identity(directory)));
    const auto loaded = load_expedition(save, directory);
    check(loaded.town.snapshot().overland && loaded.town.snapshot().overland_x == 7 &&
              loaded.town.snapshot().overland_y == 29,
          "A save on the wilderness map loads back there");
    std::filesystem::remove_all(folder);
    const auto minutes = party->state().time_minutes;
    travel(town, party, 4);
    check(town.snapshot().overland_y == 29 && party->state().time_minutes == minutes,
          "The sea stops a step south, and no time passes");
    travel(town, party, 2);
    check(town.snapshot().overland_x == 8 && party->state().time_minutes == minutes + 720,
          "A step east takes half a day");
    // A roaming group met on the wilderness map is fought on a field grown
    // from the terrain.
    const auto fights = fights_started.size();
    for (unsigned n = 0; n < 200 && fights_started.size() == fights; ++n)
    {
        travel(town, party, n % 2 ? 4 : 0, prefer({"Fight"}));
        recover(town, party);
    }
    check(fights_started.size() == fights + 1 && last_encounter &&
              last_encounter->field.tiles.size() == 1250,
          "A wilderness group is fought on a wilderness battlefield");
    travel_to(town, party, 8, 28);
    // The grove and the ruined huts, about one wilderness step in 420: a
    // step's arrival rolls RANDOM 19 for a meeting (0 meets one) and then
    // RANDOM 20 for its kind (20 is the grove or the huts). The dice that
    // bring them are found on a copy of the script machine, then rolled.
    std::optional<std::uint32_t> grove;
    for (std::uint32_t seed = 1; seed < 100000 && !grove; ++seed)
    {
        auto dice = opengold::test::TourAccess::machine(town);
        dice.seed_random(seed);
        if (dice.engine_random(20) == 0 && dice.engine_random(21) == 20)
        {
            grove = seed;
        }
    }
    check(grove.has_value(), "Some dice lead to the grove or the huts");
    opengold::test::TourAccess::seed_dice(town, *grove);
    travel(town, party, 0, prefer({"ENTER", "INVESTIGATE"}));
    check(!town.snapshot().overland && town.snapshot().area_id == 26,
          "The grove or the huts are a 3D area of their own");
    leave_grove(town, party);
    check(town.snapshot().overland && town.snapshot().overland_x == 8 &&
              town.snapshot().overland_y == 27,
          "Leaving the grove or the huts returns the party to the wilderness map");
    travel_to(town, party, 11, 28, prefer({"SOUTH"}));
    check(town.snapshot().area_id == 2 && !town.snapshot().overland,
          "The city's western edge leads south into the Cadorna Textile House");
}

// Kovel Mansion (docs/audits/kovel-mansion.md), the thieves' guild on the
// city's northeast block: the harbor's boat to the bay and the city's eastern
// edge, a save in the mansion, a trap the Rogue disarms, the cabinets opened
// with Knock and by force, the jail's thugs, the chase after the guild master,
// the ambush and the weapons' gas, the guild beaten so camps are safe, and the
// way out north onto the wilderness map.
void kovel_mansion(const std::filesystem::path &directory)
{
    auto trip = create_party(directory);
    buy_and_equip(trip);
    auto &[party, town] = trip;
    party->recruit("test:kovel-rogue", character("Sly", 7, "rogue"));
    party->recruit("test:knock-wizard", knock_wizard());
    party->award_experience(8 * 6500, "test:kovel-level");
    for (const auto id : party->state().slots)
    {
        while (id && party->member(id).character.sheet().level < 5)
        {
            auto choice = party->default_advancement(id);
            party->advance(id, choice);
            keep_rest_choices(*party);
        }
    }
    opengold::test::TourAccess::set_variable(town, por::EclAddress{0x4AA7}, 255);
    walk_to(town, party, 11, 2);
    step(town, party, 0, prefer({"BAY"}));
    walk_to(town, party, 14, 1);
    step(town, party, 1, prefer({"STAY", "NORTH"}));
    check(town.snapshot().area_id == 14 && !town.snapshot().overland &&
              town.snapshot().pose.x == 15 && town.snapshot().pose.y == 4,
          "The bay's boat and the city's eastern edge lead north into Kovel Mansion");
    const auto folder = std::filesystem::temp_directory_path() / "opengold-kovel";
    std::filesystem::create_directories(folder);
    const auto save = folder / "kovel-mansion.ogs";
    write_campaign_file(save, encode_campaign(*party, &town, campaign_asset_identity(directory)));
    check(load_expedition(save, directory).town.snapshot().area_id == 14,
          "A save in Kovel Mansion loads back there");
    std::filesystem::remove_all(folder);
    // While the guild stands its thieves break up every camp with slings, so
    // the party is healed between walks instead.
    check(town.camp(RestKind::long_rest), "Camp in the mansion");
    settle(town, party);
    check(town.snapshot().dialogue.find("THIEVES WITH SLINGS") != std::string::npos,
          "The guild's slingers break up a camp in the mansion");
    const auto fight_them = prefer({"Bash", "ATTACK HIM", "ATTACK", "DISARM TRAP", "YES"});
    const auto guild = [&](std::uint16_t address)
    {
        return town.script_variable(por::EclAddress{address});
    };
    // Every prompt the party answers, to check what the events said.
    std::string heard;
    const auto prowl = [&](unsigned x, unsigned y, const Answer &answer)
    {
        heal(*party);
        const Answer noting = [&](const por::TourSnapshot &s)
        {
            heard += s.dialogue + "\n";
            return answer(s);
        };
        walk_to(town, party, x, y, noting, true);
    };

    // The trap room at (4,3): with the Rogue the party may see the trap and
    // disarm it; either way it is spent.
    prowl(4, 3, fight_them);
    check(guild(0x4A05) == 1, "The trap at (4,3) is disarmed or sprung once");

    // The cabinets at (14,10): A is locked and opened with Knock, B is
    // smashed open, C may be trapped.
    std::set<std::string> examined;
    bool knocked = false;
    const auto cabinets = [&](const por::TourSnapshot &s) -> std::size_t
    {
        const auto has = [&](std::string_view label)
        {
            return std::find(s.choices.begin(), s.choices.end(), label) != s.choices.end();
        };
        if (has("CAST KNOCK"))
        {
            const auto label = knocked ? "BREAK" : "CAST KNOCK";
            knocked = true;
            return prefer({label})(s);
        }
        if (has("A") && has("LEAVE"))
        {
            for (const auto *label : {"A", "B", "C"})
            {
                if (examined.insert(label).second)
                {
                    return prefer({label})(s);
                }
            }
            return prefer({"LEAVE"})(s);
        }
        return fight_them(s);
    };
    const auto wealth = party->state().wealth;
    prowl(14, 10, cabinets);
    check(guild(0x4AEA) == 1 && guild(0x4AEB) == 1 && guild(0x4AEC) == 1,
          "The three cabinets are opened and emptied");
    check(knocked && heard.find("NO ONE IN THE PARTY HAS THE KNOCK SPELL") == std::string::npos,
          "The wizard casts Knock on cabinet A");
    check(party->state().wealth[std::size_t(Coin::silver)] >=
                  wealth[std::size_t(Coin::silver)] + 1500 &&
              party->state().wealth[std::size_t(Coin::gold)] >= wealth[std::size_t(Coin::gold)] + 1800,
          "Cabinets B and C hold 1,500 silver and 1,800 gold pieces");

    // The jail at (10,12): ten thugs beating an old thief.
    prowl(10, 12, fight_them);
    check((guild(0x4A48) & 2) != 0, "The jail's thugs are beaten and the old thief's papers taken");

    // The guild master's chase from (6,5).
    const auto killed = guild(0x4A45);
    prowl(6, 5, prefer({"Bash", "YES", "EAST", "ATTACK", "ATTACK HIM"}));
    check(guild(0x4A45) == killed + 1 &&
              heard.find("YOU HAVE CAUGHT ONE OF THE THIEVES") != std::string::npos,
          "The party chases the thieves through the mansion and fights the guild master");

    // The ambush at (8,13): fifteen thieves and three masters.
    prowl(8, 13, fight_them);
    check(guild(0x4A19) == 1, "The ambush is beaten");

    // The weapons at (3,4): taking some sets off the gas and more thieves.
    prowl(3, 4, prefer({"Bash", "TAKE SOME", "ATTACK", "ATTACK HIM", "YES"}));
    check(guild(0x4A45) >= 21 && guild(0x4AB2) == 254,
          "With twenty-one thieves beaten the guild no longer troubles the party's camps");

    // North off the map: the wilderness at the city's northern edge.
    const auto north = edge_opening(town.map(), town.snapshot().pose, 0);
    walk_to(town, party, north.x, north.y, fight_them);
    step(town, party, 0, prefer({"LEAVE"}));
    check(town.snapshot().overland && town.snapshot().overland_x == 13 &&
              town.snapshot().overland_y == 26,
          "Kovel Mansion's north edge leads onto the wilderness map");
}

// The Temple of Bane (docs/audits/temple-of-bane.md): City Hall's summons to
// Tyr's bishop, Dirten joining, the bay's boat and the city's eastern edge
// south into the street before the orcs' mansion, the mansion's orcs, the
// goblin slaves let go, the temple's guards, its three trapdoors found by
// searching, the horde and its half-orc cleric, and Dirten staying behind.
void temple_of_bane(const std::filesystem::path &directory)
{
    auto trip = create_party(directory);
    buy_and_equip(trip);
    auto &[party, town] = trip;
    party->award_experience(6 * 6500, "test:temple-level");
    for (const auto id : party->state().slots)
    {
        while (id && party->member(id).character.sheet().level < 5)
        {
            party->advance(id, party->default_advancement(id));
        }
    }
    // City Hall's clerk names three commissions at most, the bishop's sixth:
    // the party stands in for one that has finished the five before it (the
    // Slums, Sokal Keep, the books, the auction and Cadorna's commission).
    for (const auto done : {0x4ABB, 0x4AA7, 0x4AC2, 0x4AB0, 0x4A97})
    {
        opengold::test::TourAccess::set_variable(town, por::EclAddress{std::uint16_t(done)}, 255);
    }
    walk_to(town, party, 4, 5);
    walk_to(town, party, 5, 5);
    check(town.script_variable(por::EclAddress{0x4A9B}) == 254,
          "City Hall summons the party to Tyr's bishop");
    walk_to(town, party, 10, 5, prefer({"YES"}));
    const auto &slots = party->state().slots;
    const auto joined =
        std::find_if(slots.begin(), slots.end(),
                     [&](MemberId id)
                     {
                         return id && party->member(id).npc_source == "por:MON3CHA:107";
                     });
    check(joined != slots.end(), "Dirten joins the party at the bishop's request");
    const auto dirten = *joined;
    const auto &sheet = party->member(dirten).character.sheet();
    check(sheet.name == "Dirten" && sheet.level == 5 && sheet.character_class == "Cleric" &&
              sheet.scores[rules::Ability::strength] == 10 &&
              sheet.scores[rules::Ability::dexterity] == 10 &&
              sheet.scores[rules::Ability::constitution] == 9 &&
              sheet.scores[rules::Ability::intelligence] == 15 &&
              sheet.scores[rules::Ability::wisdom] == 16 &&
              sheet.scores[rules::Ability::charisma] == 15 &&
              party->member(dirten).equipped.size() == 2,
          "Dirten is a level-5 Cleric with his record's scores, his mace and armor on");

    walk_to(town, party, 11, 2);
    step(town, party, 0, prefer({"BAY"}));
    walk_to(town, party, 14, 1);
    step(town, party, 1, prefer({"STAY", "SOUTH"}));
    check(town.snapshot().area_id == 31 && town.snapshot().pose.x == 15 &&
              town.snapshot().pose.y == 4,
          "The city's eastern edge leads south into the street before the Temple of Bane");
    const auto folder = std::filesystem::temp_directory_path() / "opengold-temple";
    std::filesystem::create_directories(folder);
    const auto save = folder / "temple-of-bane.ogs";
    write_campaign_file(save, encode_campaign(*party, &town, campaign_asset_identity(directory)));
    check(load_expedition(save, directory).town.snapshot().area_id == 31,
          "A save before the Temple of Bane loads back there");
    std::filesystem::remove_all(folder);

    const auto temple = [&](std::uint16_t address)
    {
        return town.script_variable(por::EclAddress{address});
    };
    const auto fight_them = prefer({"Bash", "ATTACK", "COMBAT", "Fight", "SEARCH THE BEDS",
                                    "LET THEM GO", "OPEN TRAPDOOR", "DESTROY ALTAR", "YES"});
    const auto prowl = [&](unsigned x, unsigned y)
    {
        heal(*party);
        walk_to(town, party, x, y, fight_them, true);
    };
    // The orcs' mansion, entered from the south.
    prowl(10, 9);
    check(temple(0x4AA8) == 254, "The mansion's orcs are beaten");
    prowl(6, 8);
    check((temple(0x4A2E) & 16) != 0, "The six orcs and their ogre are beaten");
    prowl(14, 8);
    check((temple(0x4A2E) & 32) != 0, "The goblin slaves are let go");

    // West into the temple's block, to its doors.
    prowl(0, 4);
    step(town, party, 3, fight_them);
    check(town.snapshot().area_id == 24, "The street's west edge leads to the temple's block");
    prowl(2, 7);
    check(temple(0x4A0F) == 1, "The temple's guards are beaten");

    // The three trapdoors inside, found by searching.
    for (const auto &[x, y, bit] : std::array<std::tuple<unsigned, unsigned, unsigned>, 3>{
             {{5, 5, 1}, {9, 12, 2}, {12, 9, 4}}})
    {
        prowl(x, y);
        check(look_until(
                  town, party,
                  [&]
                  {
                      return (temple(0x4A2D) & bit) != 0;
                  },
                  fight_them),
              "Searching finds a trapdoor's treasure");
    }
    check(temple(0x4ABD) == 254, "The horde and its half-orc cleric are beaten");
    check(std::find(slots.begin(), slots.end(), dirten) == slots.end(),
          "Dirten stays behind at the cleansed temple");
}

// The nomad camp (docs/audits/nomad-camp.md), north of the city: entered
// from the wilderness map, the nomads met in peace, the headman's feast and
// a night's lodging, the party staying to fight the kobolds, and the
// headman's reward.
void nomad_camp(const std::filesystem::path &directory)
{
    auto trip = create_party(directory);
    buy_and_equip(trip);
    auto &[party, town] = trip;
    party->award_experience(6 * 6500, "test:nomad-level");
    for (const auto id : party->state().slots)
    {
        while (id && party->member(id).character.sheet().level < 5)
        {
            party->advance(id, party->default_advancement(id));
        }
    }
    opengold::test::TourAccess::set_variable(town, por::EclAddress{0x4AA7}, 255);
    walk_to(town, party, 11, 2);
    step(town, party, 0, prefer({"WEST"}));
    walk_to(town, party, 14, 1);
    step(town, party, 1, prefer({"STAY"}));
    travel_to(town, party, 12, 11, prefer({"ENTER"}));
    check(town.snapshot().area_id == 17 && !town.snapshot().overland,
          "The nomads' camp is a 3D area of its own");
    // The nomads are met in peace; the kobolds are fought.
    const Answer guest = [](const por::TourSnapshot &s)
    {
        if (s.dialogue.find("THE NOMADS WAIT") != std::string::npos)
        {
            return prefer({"Parley"})(s);
        }
        return prefer({"YES", "STAY AND FIGHT", "DESTROY THE KOBOLDS", "Fight"})(s);
    };
    walk_to(town, party, 8, 8, guest, false, false, "0",
            [&] { return town.snapshot().camp_offered; });
    const auto nomads = [&](std::uint16_t address)
    {
        return town.script_variable(por::EclAddress{address});
    };
    check(town.snapshot().camp_offered && (nomads(0x4A7C) & 1) != 0 &&
              town.script_variable(por::EclAddress{0x49C9}) == 18,
          "After the headman's feast, which lasts until evening, the nomads lodge the party "
          "and offer a night's rest");
    check(town.camp(RestKind::long_rest), "Camp in the nomads' lodging");
    settle(town, party, guest);
    check(nomads(0x4A7D) == 3 && town.script_variable(por::EclAddress{0x49C9}) == 0,
          "At midnight the headman asks the party to stay against the kobolds, and it stays");
    rest_until(town, party, [&] { return nomads(0x4A7E) > 0; },
               "The kobolds attack the next evening");
    const auto gold = coins(party->state().wealth, Coin::gold);
    for (unsigned n = 0; n < 10 && !(nomads(0x4A7C) & 8); ++n)
    {
        heal(*party);
        walk_to(town, party, 9, n % 2 ? 12 : 10, guest);
    }
    check((nomads(0x4A7C) & 8) != 0 && coins(party->state().wealth, Coin::gold) >= gold + 8000,
          "With the kobolds broken the shaman pays the promised reward");
    check(town.snapshot().overland && town.snapshot().overland_x == 12 &&
              town.snapshot().overland_y == 11,
          "The nomads send the party back out onto the wilderness map");
}

// The Pyramid (docs/audits/pyramid.md), in the poisoned valley west of the
// camp: the rowboat in the reeds and the secret door, the souls Yarash keeps,
// the teleporters into his laboratories, the password, the pumping room's
// slaves, the fountain's equipment destroyed, Yarash beaten, and the valley
// healing.
void pyramid(const std::filesystem::path &directory)
{
    auto trip = create_party(directory);
    buy_and_equip(trip);
    auto &[party, town] = trip;
    party->award_experience(6 * 14000, "test:pyramid-level");
    for (const auto id : party->state().slots)
    {
        while (id && party->member(id).character.sheet().level < 6)
        {
            party->advance(id, party->default_advancement(id));
        }
    }
    opengold::test::TourAccess::set_variable(town, por::EclAddress{0x4AA7}, 255);
    walk_to(town, party, 11, 2);
    step(town, party, 0, prefer({"WEST"}));
    walk_to(town, party, 14, 1);
    step(town, party, 1, prefer({"STAY"}));
    travel_to(town, party, 6, 14, prefer({"YES", "ENTER"}));
    check(town.snapshot().area_id == 22 && !town.snapshot().overland,
          "The rowboat in the reeds and the secret door lead into the Pyramid");
    // Every prompt the party answers, to check what the events said; the
    // souls Yarash keeps are met in peace where they allow it.
    std::string heard;
    const Answer noting = [&](const por::TourSnapshot &s)
    {
        heard += s.dialogue + "\n";
        return prefer({"Parley", "YES", "Fight"})(s);
    };
    const auto at = [&](unsigned area, unsigned x, unsigned y)
    {
        const auto &s = town.snapshot();
        return s.area_id == area && s.pose.x == x && s.pose.y == y;
    };
    const auto walk = [&](unsigned x, unsigned y, const Answer &answer,
                          const std::function<bool()> &arrived, std::string_view typed = "0")
    {
        heal(*party);
        walk_to(town, party, x, y, answer, true, false, typed, arrived);
    };
    // The prison of souls: the teleporters at (14,8) and (14,9), then the
    // passage at (6,1) into the laboratories.
    walk(14, 8, noting, [&] { return at(22, 5, 11); });
    check(at(22, 5, 11), "The teleporter at (14,8) sends the party to (5,11)");
    walk(14, 9, noting, [&] { return at(22, 0, 0); });
    check(at(22, 0, 0), "The teleporter at (14,9) sends the party to (0,0)");
    walk(6, 1, noting, [&] { return town.snapshot().area_id == 23; });
    check(at(23, 0, 12), "The passage at (6,1) leads into Yarash's laboratories");
    // The rocks send the party on by a flag's parity (0x4A4D bit 1); in
    // Search mode a rock is noticed first (a strange tingle), and one may be
    // thrown to flip the flag.
    bool thrown = true;
    const Answer rock = [&](const por::TourSnapshot &s)
    {
        if (std::find(s.choices.begin(), s.choices.end(), "THROW A ROCK") != s.choices.end())
        {
            heard += s.dialogue + "\n";
            const auto choice = prefer({thrown ? "MOVE ON" : "THROW A ROCK"})(s);
            thrown = true;
            return choice;
        }
        return noting(s);
    };
    town.set_searching(true);
    walk(15, 12, rock, [&] { return town.snapshot().area_id == 23 && town.snapshot().pose.y < 8; });
    check(heard.find("STRANGE TINGLE") != std::string::npos && at(23, 11, 3),
          "In Search mode the rocks at (15,12) are noticed; moving on sends the party to (11,3)");
    thrown = false;
    walk(12, 3, rock, [&] { return at(23, 0, 4); });
    check(at(23, 0, 4), "A rock thrown at (12,3) flips the rocks: they send the party to (0,4)");
    thrown = false;
    walk(0, 2, rock, [&] { return at(23, 0, 0); });
    check(at(23, 0, 0), "A rock thrown at (0,2) sends the party to (0,0)");
    town.set_searching(false);
    // The password door, read from dwarvish runes, the fountain, and Yarash.
    walk(1, 1, noting, [] { return false; }, "NOKNOK");
    check(heard.find("SIX DWARVISH RUNES") != std::string::npos &&
              heard.find("NOKNOK") != std::string::npos,
          "The door's six dwarvish runes read NOKNOK, the password");
    walk(2, 1, prefer({"DESTROY THE EQUIPMENT"}), [] { return false; });
    check(town.script_variable(por::EclAddress{0x4AB3}) == 254,
          "The fountain's equipment is destroyed");
    walk(4, 1, prefer({"Bash", "Fight"}), [] { return false; });
    check((town.script_variable(por::EclAddress{0x4A52}) & 4) != 0, "Yarash is beaten");
    walk(6, 1, noting, [] { return false; });
    check(look_until(town, party,
                     [&] { return (town.script_variable(por::EclAddress{0x4A52}) & 128) != 0; },
                     noting),
          "Searching Yarash's desk finds his papers");
    // The dial's teleporter back to the prison, and its north door out.
    walk(6, 0, noting, [&] { return town.snapshot().area_id == 22; });
    check(at(22, 15, 8), "The dial's teleporter returns the party to the prison of souls");
    walk(15, 0, noting, [] { return false; });
    face(town, party, 0);
    check(look_until(town, party,
                     [&] { return (town.script_variable(por::EclAddress{0x4A4B}) & 1) != 0; },
                     noting),
          "Searching finds the prison's north door");
    town.explore(por::ExplorationCommand::forward);
    settle(town, party, noting);
    check(town.snapshot().overland && town.script_variable(por::EclAddress{0x4AA2}) == 255 &&
              heard.find("BEGINNING TO RECOVER") != std::string::npos,
          "Leaving the Pyramid, the valley begins to heal and its rivers open");
}

// Explores a wilderness window's cave from the square it entered on, its way
// out (`stay` declines it, `leave` takes it): the farthest room reachable
// from the entrance, then back out onto the wilderness map.
void explore_cave(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party,
                  const char *stay, const char *leave)
{
    const auto entrance = town.snapshot().pose;
    std::optional<std::pair<unsigned, unsigned>> room;
    for (unsigned y = 0; y < 16; ++y)
    {
        for (unsigned x = 0; x < 16; ++x)
        {
            const auto &cell = town.map().at(x, y);
            if (cell.event_number() != 0 && (x != entrance.x || y != entrance.y) &&
                reachable(town.map(), entrance, x, y))
            {
                room = {x, y};
            }
        }
    }
    check(room.has_value(), "The cave has a room to explore");
    heal(*party);
    walk_to(town, party, room->first, room->second, prefer({"Fight", "ATTACK", "APPROACH", stay}),
            true);
    heal(*party);
    walk_to(town, party, entrance.x, entrance.y,
            prefer({"Fight", "ATTACK", "APPROACH", leave}), true, false, "0",
            [&] { return town.snapshot().overland; });
}

// The western wilderness (docs/audits/western-wilderness.md): stepping west
// off the central window, the dragon seen over the mountains, its cave, the
// anhkhegs, the slavers and the Buccaneers' base, the Zhentil outpost's riders,
// the dark cave a roaming roll finds, and the way back east.
void western_wilderness(const std::filesystem::path &directory)
{
    auto trip = create_party(directory);
    buy_and_equip(trip);
    auto &[party, town] = trip;
    party->award_experience(6 * 14000, "test:west-level");
    for (const auto id : party->state().slots)
    {
        while (id && party->member(id).character.sheet().level < 6)
        {
            party->advance(id, party->default_advancement(id));
        }
    }
    // The party stands in for one City Hall sent to rescue the Bivant heir
    // from the Buccaneers (0x4A8C) and with a message for the Zhentil keep
    // (0x4A98).
    for (const auto commission : {0x4AA7, 0x4A8C, 0x4A98})
    {
        opengold::test::TourAccess::set_variable(town, por::EclAddress{std::uint16_t(commission)},
                                                 255);
    }
    walk_to(town, party, 11, 2);
    step(town, party, 0, prefer({"WEST"}));
    walk_to(town, party, 14, 1);
    step(town, party, 1, prefer({"STAY"}));
    std::string heard;
    const Answer noting = [&](const por::TourSnapshot &s)
    {
        heard += s.dialogue + "\n";
        return prefer({"Parley", "YES", "Fight", "ENTER", "ENTER CAVE", "STAY", "LEAVE CAVE"})(s);
    };
    // A square of the central window's second column with open ground west.
    const auto &world = opengold::test::TourAccess::overland_world(town);
    std::optional<unsigned> crossing;
    for (unsigned y = 35; y > 0 && !crossing; --y)
    {
        if (por::overland_passage(world.at(14, y)) == por::OverlandPassage::open &&
            por::overland_passage(world.at(15, y)) == por::OverlandPassage::open &&
            overland_heading(town, 7, 29, 2, y))
        {
            crossing = y;
        }
    }
    check(crossing.has_value(), "The central window has a way west");
    travel_to(town, party, 2, *crossing, noting);
    travel(town, party, 6, noting);
    check(town.snapshot().script_id == 25 && town.snapshot().overland &&
              town.snapshot().overland_x == 14 && town.snapshot().overland_y == *crossing,
          "Stepping west off the central window enters the western wilderness");
    // The sites that lead on to areas not in the game yet.
    for (const auto &[x, y, place] : std::array<std::tuple<unsigned, unsigned, const char *>, 3>{
             {{12, 31, "the Buccaneers' base"}, {3, 32, "the Zhentil outpost"},
              {10, 9, "the dragon's cave"}}})
    {
        heal(*party);
        const auto refusal = std::string("The way to ") + place + " is not in the game yet.";
        travel_to(town, party, x, y, noting,
                  [&] { return heard.find(refusal) != std::string::npos; });
        check(heard.find(refusal) != std::string::npos,
              std::string("The western wilderness's way to ") + place + " is refused for now");
        closed_sites.insert({town.snapshot().script_id * 100 + x, y});
    }
    check(heard.find("ARE YOU THE DIPLOMATIC ENVOYS") != std::string::npos &&
              heard.find("A LARGE DRAGON FLYS ABOVE YOU") != std::string::npos,
          "The Zhentil riders meet the envoys and the dragon flies to its cave");
    // The anhkhegs at (8,24).
    heal(*party);
    const auto fights = fights_started.size();
    travel_to(town, party, 8, 24, noting);
    check(town.script_variable(por::EclAddress{0x4A0F}) == 1 && fights_started.size() > fights,
          "Two anhkhegs erupt at (8,24) and are beaten");
    // The dark cave, about one step in 420, as the central window's grove.
    std::optional<std::uint32_t> cave;
    for (std::uint32_t seed = 1; seed < 100000 && !cave; ++seed)
    {
        auto dice = opengold::test::TourAccess::machine(town);
        dice.seed_random(seed);
        if (dice.engine_random(20) == 0 && dice.engine_random(21) == 20)
        {
            cave = seed;
        }
    }
    check(cave.has_value(), "Some dice lead to the dark cave");
    opengold::test::TourAccess::seed_dice(town, *cave);
    travel(town, party, 0, prefer({"ENTER", "STAY IN"}));
    check(!town.snapshot().overland && town.snapshot().area_id == 25,
          "A roaming roll finds the dark cave, a 3D area of its own");
    explore_cave(town, party, "STAY IN", "LEAVE CAVE");
    check(town.snapshot().overland && town.snapshot().script_id == 25,
          "The dark cave's passage leads back outside");
    // Back east into the central window.
    std::optional<unsigned> east;
    for (unsigned y = 35; y > 0 && !east; --y)
    {
        if (por::overland_passage(world.at(15, y)) == por::OverlandPassage::open &&
            por::overland_passage(world.at(16, y)) == por::OverlandPassage::open &&
            overland_heading(town, town.snapshot().overland_x, town.snapshot().overland_y, 15, y))
        {
            east = y;
        }
    }
    check(east.has_value(), "The western window has a way east");
    travel_to(town, party, 15, *east, noting);
    travel(town, party, 2, noting);
    check(town.snapshot().script_id == 26 && town.snapshot().overland &&
              town.snapshot().overland_x == 3,
          "Stepping east off the western window returns to the central wilderness");
}

// The eastern wilderness (docs/audits/eastern-wilderness.md): the harbor's
// eastern boat, a kobold horde, the wyverns' cave a roaming
// roll finds, and the boat home to New Phlan.
void eastern_wilderness(const std::filesystem::path &directory)
{
    auto trip = create_party(directory);
    buy_and_equip(trip);
    auto &[party, town] = trip;
    party->award_experience(6 * 14000, "test:east-level");
    for (const auto id : party->state().slots)
    {
        while (id && party->member(id).character.sheet().level < 6)
        {
            party->advance(id, party->default_advancement(id));
        }
    }
    opengold::test::TourAccess::set_variable(town, por::EclAddress{0x4AA7}, 255);
    walk_to(town, party, 11, 2);
    step(town, party, 0, prefer({"EAST"}));
    walk_to(town, party, 14, 1);
    step(town, party, 1, prefer({"STAY"}));
    check(town.snapshot().script_id == 27 && town.snapshot().overland &&
              town.snapshot().overland_x == 9 && town.snapshot().overland_y == 29,
          "The harbor's eastern boat lands the party on the eastern wilderness at (9,29)");
    std::string heard;
    const Answer noting = [&](const por::TourSnapshot &s)
    {
        heard += s.dialogue + "\n";
        return prefer({"Parley", "YES", "Fight", "ENTER", "SMALL", "LEAVE CAVE"})(s);
    };
    // The kobold horde near the caves, met on the way there or now.
    heal(*party);
    const auto horde_beaten = [&] { return town.script_variable(por::EclAddress{0x4A11}) == 255; };
    travel_to(town, party, 7, 14, noting, horde_beaten);
    check(horde_beaten(), "A screaming horde of kobolds attacks near their caves and is beaten");
    // The wyverns' cave, about one step in 420.
    std::optional<std::uint32_t> cave;
    for (std::uint32_t seed = 1; seed < 100000 && !cave; ++seed)
    {
        auto dice = opengold::test::TourAccess::machine(town);
        dice.seed_random(seed);
        if (dice.engine_random(20) == 0 && dice.engine_random(21) == 20)
        {
            cave = seed;
        }
    }
    check(cave.has_value(), "Some dice lead to the wyverns' cave");
    heal(*party);
    // A step onto open ground next to the party, so the step rolls.
    const auto &world = opengold::test::TourAccess::overland_world(town);
    std::optional<unsigned> open_step;
    for (unsigned direction = 0; direction < 8 && !open_step; ++direction)
    {
        const int nx = int(town.snapshot().overland_x) + por::overland_dx[direction],
                  ny = int(town.snapshot().overland_y) + por::overland_dy[direction];
        if (nx >= 3 && nx < 16 && ny >= 0 && ny < 36 &&
            !closed_sites.contains({2700 + unsigned(nx), unsigned(ny)}) &&
            por::overland_passage(world.at(world.window_start[2] + nx, ny)) ==
                por::OverlandPassage::open)
        {
            open_step = direction;
        }
    }
    check(open_step.has_value(), "The party can step onto open ground");
    opengold::test::TourAccess::seed_dice(town, *cave);
    travel(town, party, *open_step, prefer({"ENTER", "STAY"}));
    check(!town.snapshot().overland && town.snapshot().area_id == 27,
          "A roaming roll finds the wyverns' cave, a 3D area of its own");
    explore_cave(town, party, "STAY", "LEAVE");
    check(town.snapshot().overland && town.snapshot().script_id == 27,
          "The wyverns' cave leads back outside");
    // The boat home.
    heal(*party);
    travel_to(town, party, 9, 29, prefer({"TAKE BOAT"}),
              [&] { return !town.snapshot().overland; });
    check(town.snapshot().area_id == 0 && town.snapshot().script_id == 0,
          "The eastern wilderness's boat returns the party to New Phlan");
}

// The kobold caves (docs/audits/kobold-caves.md), by the eastern
// wilderness: the small cave, the king's hall and his envoys, Princess
// Fatima, the efreeti in the brass bottle, the wyvern, the king's end and
// the community's wealth, and the way out with Fatima taking her leave.
void kobold_caves(const std::filesystem::path &directory)
{
    auto trip = create_party(directory);
    buy_and_equip(trip);
    auto &[party, town] = trip;
    party->award_experience(6 * 23000, "test:kobold-level");
    for (const auto id : party->state().slots)
    {
        while (id && party->member(id).character.sheet().level < 7)
        {
            party->advance(id, party->default_advancement(id));
        }
    }
    opengold::test::TourAccess::set_variable(town, por::EclAddress{0x4AA7}, 255);
    walk_to(town, party, 11, 2);
    step(town, party, 0, prefer({"EAST"}));
    walk_to(town, party, 14, 1);
    step(town, party, 1, prefer({"STAY"}));
    std::string heard;
    // The party answers to stay, to parley where offered, to fight where met,
    // and declines to be the vampires the efreeti seeks.
    const Answer noting = [&](const por::TourSnapshot &s)
    {
        heard += s.dialogue + "\n";
        if (s.dialogue.find("WANT TO LEAVE") != std::string::npos ||
            s.dialogue.find("VAMPIRES") != std::string::npos)
        {
            return prefer({"NO"})(s);
        }
        return prefer({"Bash", "SMALL", "Fight", "FIGHT", "YES", "AGREE", "FOLLOW", "PULL IT OUT",
                       "GIVE IT WATER", "DISARM IT"})(s);
    };
    travel_to(town, party, 6, 15, noting, [&] { return !town.snapshot().overland; });
    check(town.snapshot().area_id == 13 && town.snapshot().pose.x == 6 &&
              town.snapshot().pose.y == 15,
          "The small cave by the eastern wilderness leads into the kobold caves");
    const auto kobolds = [&](std::uint16_t address)
    {
        return town.script_variable(por::EclAddress{address});
    };
    const auto wealth = party->state().wealth;
    // The king's hall, by way of the pool, the truce and the wyvern.
    heal(*party);
    walk_to(town, party, 5, 3, noting, true);
    check(heard.find("FALLS INTO A POOL OF WATER") != std::string::npos &&
              party->state().wealth != wealth,
          "A member falls into the pool and the party loses that member's share of its money");
    check(heard.find("I AM TO ESCORT YOU TO OUR TREASURE") != std::string::npos &&
              kobolds(0x4A8E) == 1,
          "The truce kobold leads the party past its traps to the wyvern, which is beaten");
    check(kobolds(0x4A8D) == 4, "The king's horde and envoys are beaten, and the king escapes");
    for (const auto &[x, y] : std::array<std::pair<unsigned, unsigned>, 5>{
             {{1, 3}, {7, 0}, {10, 1}, {13, 1}, {14, 6}}})
    {
        heal(*party);
        walk_to(town, party, x, y, noting, true);
    }
    const auto &slots = party->state().slots;
    const auto joined = std::find_if(slots.begin(), slots.end(),
                                     [&](MemberId id)
                                     {
                                         return id &&
                                                party->member(id).npc_source == "por:MON8CHA:104";
                                     });
    check(joined != slots.end(), "Princess Fatima joins the party");
    const auto fatima_id = *joined;
    const auto &fatima = party->member(fatima_id);
    check(fatima.character.sheet().level == 4 &&
              fatima.character.sheet().scores[rules::Ability::strength] == 18 &&
              fatima.character.sheet().scores[rules::Ability::dexterity] == 18 &&
              fatima.character.sheet().scores[rules::Ability::constitution] == 16 &&
              fatima.equipped.size() == 3,
          "Fatima is a level-4 Fighter with her record's scores, her sword, armor and shield on");
    check(kobolds(0x4AB6) == 254 && kobolds(0x4A8D) == 7 && kobolds(0x4A8E) == 2,
          "The king dies in his own pit, and the community's wealth and the wyvern's are taken");
    // The brass bottle, found by searching.
    heal(*party);
    walk_to(town, party, 12, 0, noting, true);
    check(look_until(town, party, [&] { return kobolds(0x4AD3) != 0; }, noting, 5) &&
              heard.find("EFREETI SAMIR AHWAHL") != std::string::npos,
          "Searching finds the brass bottle, whose efreeti returns to it");
    // The way out, where Fatima takes her leave.
    heal(*party);
    walk_to(town, party, 6, 15, prefer({"Bash", "Fight", "YES"}), true, false, "0",
            [&] { return town.snapshot().overland; });
    check(town.snapshot().overland && town.snapshot().script_id == 27 &&
              std::find(slots.begin(), slots.end(), fatima_id) == slots.end(),
          "Leaving the caves, Fatima goes her own way");
}

// The lizard men's keep (docs/audits/lizard-keep.md), the ruined castle in
// the eastern wilderness's quicksand: the old lizard man and his champion's
// duel, the flooded lower level and a pool's lizard men, and the way out.
void lizard_keep(const std::filesystem::path &directory)
{
    auto trip = create_party(directory);
    buy_and_equip(trip);
    auto &[party, town] = trip;
    party->award_experience(6 * 23000, "test:lizard-level");
    for (const auto id : party->state().slots)
    {
        while (id && party->member(id).character.sheet().level < 7)
        {
            party->advance(id, party->default_advancement(id));
        }
    }
    // The party stands in for one that freed the Pyramid's chained lizard
    // men (0x4A52 bit 1), who told it their tribe's friend-word.
    opengold::test::TourAccess::set_variable(town, por::EclAddress{0x4AA7}, 255);
    opengold::test::TourAccess::set_variable(town, por::EclAddress{0x4A52}, 1);
    walk_to(town, party, 11, 2);
    step(town, party, 0, prefer({"EAST"}));
    walk_to(town, party, 14, 1);
    step(town, party, 1, prefer({"STAY"}));
    std::string heard;
    // The party parleys with the old lizard man, fights what attacks it, and
    // dives into the pools and climbs the stairs it finds.
    const Answer noting = [&](const por::TourSnapshot &s)
    {
        heard += s.dialogue + "\n";
        if (s.dialogue.find("PATIENTLY") != std::string::npos)
        {
            return prefer({"Parley"})(s);
        }
        return prefer({"ENTER", "Bash", "Fight", "YES"})(s);
    };
    travel_to(town, party, 11, 8, noting,
              [&]
              {
                  return !town.snapshot().overland;
              });
    check(town.snapshot().area_id == 16 && town.snapshot().pose.x == 8 &&
              town.snapshot().pose.y == 14,
          "The drawbridge over the quicksand leads into the lizard men's keep");
    // The old lizard man waits at one of three places on the first level.
    const auto story_told = [&]
    {
        return heard.find("WILL NOT JOIN TYRANTHRAXUS") != std::string::npos;
    };
    for (const auto &[x, y] : std::array<std::pair<unsigned, unsigned>, 8>{
             {{4, 3}, {3, 7}, {10, 4}, {5, 12}, {6, 6}, {9, 6}, {7, 9}, {15, 4}}})
    {
        if (story_told())
        {
            break;
        }
        heal(*party);
        walk_to(town, party, x, y, noting, true, false, "SAVIOR", story_told);
    }
    check(story_told() && heard.find("DO YOU REALLY MEAN SAVIOR") != std::string::npos &&
              town.script_variable(por::EclAddress{0x4A60}) == 255,
          "The old lizard man knows the friend-word SAVIOR, and his champion beats Drythh");
    // The stairs down to the flooded lower level, and one of its pools.
    heal(*party);
    walk_to(town, party, 9, 8, noting, true, false, "0",
            [&]
            {
                return town.snapshot().area_id == 30;
            });
    check(town.snapshot().area_id == 30, "Stairs lead down to the keep's flooded lower level");
    heal(*party);
    walk_to(town, party, 9, 6, noting, true);
    check(heard.find("YOU DIVE INTO THE POOL") != std::string::npos &&
              heard.find("THROW JAVELINS") != std::string::npos &&
              heard.find("\nA javelin ") != std::string::npos,
          "The lower level's pools hide lizard men, who throw javelins at divers");
    // Back up, and out by the drawbridge.
    heal(*party);
    walk_to(town, party, 10, 8, noting, true);
    face(town, party, 0);
    town.explore(por::ExplorationCommand::forward);
    // Up the stairs, then not straight back down them.
    settle(town, party,
           [&](const por::TourSnapshot &s)
           {
               return s.dialogue.find("LEADING DOWN") != std::string::npos ? prefer({"NO"})(s)
                                                                           : noting(s);
           });
    check(town.snapshot().area_id == 16, "The tunnel slopes back up to the first level");
    heal(*party);
    walk_to(town, party, 8, 15, noting, true);
    face(town, party, 2);
    town.explore(por::ExplorationCommand::forward);
    settle(town, party, prefer({"LEAVE"}));
    check(town.snapshot().overland && town.snapshot().script_id == 27,
          "The drawbridge leads back out to the eastern wilderness");
}

// A new party of `level` by way of Podol Plaza's north edge to Stojanow
// Gate, where the trader's wagon is bought and the toll paid: the gate opens
// onto its north side. Every prompt the party meets is added to `heard`.
Expedition through_stojanow_gate(const std::filesystem::path &directory, int level,
                                 std::string &heard)
{
    auto trip = create_party(directory);
    buy_and_equip(trip);
    auto &[party, town] = trip;
    party->award_experience(6 * (level == 7 ? 23000 : 34000), "test:gate-level");
    for (const auto id : party->state().slots)
    {
        while (id && party->member(id).character.sheet().level < level)
        {
            party->advance(id, party->default_advancement(id));
        }
    }
    opengold::test::TourAccess::set_variable(town, por::EclAddress{0x4AA7}, 255);
    walk_to(town, party, 11, 2);
    step(town, party, 0, prefer({"WEST"}));
    walk_to(town, party, 14, 1);
    step(town, party, 1, prefer({"STAY"}));
    travel_to(town, party, 11, 28, prefer({"NORTH"}), [&] { return !town.snapshot().overland; });
    check(town.snapshot().area_id == 18, "The city's west edge leads into Podol Plaza");
    walk_to(town, party, 4, 0, peaceful);
    step(town, party, 0, peaceful);
    check(town.snapshot().area_id == 9, "Podol Plaza's north edge leads to Stojanow Gate");
    // The wagon driver sells his wagon and warns of the bugbears; the toll
    // paid, the gate opens and the party drives through to the north side.
    const Answer paying = [&](const por::TourSnapshot &s)
    {
        heard += s.dialogue + "\n";
        return prefer({"TAKE THE PUNCH", "TAKE IT", "YES", "Parley", "Bash"})(s);
    };
    walk_to(town, party, 7, 12, paying, true);
    step(town, party, 0, paying);
    check(heard.find("HERE'S YOUR WAGON") != std::string::npos &&
              heard.find("LETS HAVE THE 15 GOLD") != std::string::npos &&
              town.snapshot().pose.x == 8 && town.snapshot().pose.y == 5,
          "The wagon bought and the toll paid, the gate opens onto its north side");
    return trip;
}

// Stojanow Gate (docs/audits/stojanow-gate.md): Podol Plaza's north edge, the
// wagon bought and the toll paid, both towers' garrisons, the old town north
// and back, the bugbears beyond the passage and the gate taken, and the way
// back south to Podol Plaza.
void stojanow_gate(const std::filesystem::path &directory)
{
    std::string heard;
    auto trip = through_stojanow_gate(directory, 7, heard);
    auto &[party, town] = trip;
    const Answer noting = [&](const por::TourSnapshot &s)
    {
        heard += s.dialogue + "\n";
        return prefer({"ATTACK", "Fight", "Bash", "YES"})(s);
    };
    const auto gate = [&](std::uint16_t address)
    {
        return town.script_variable(por::EclAddress{address});
    };
    // Each tower's stairs lead up to its garrison: a mage, an aide and three
    // ettins.
    const auto climb = [&](unsigned x)
    {
        heal(*party);
        walk_to(town, party, x, 5, noting);
        step(town, party, 2, noting);
        check(town.snapshot().pose.x == x && town.snapshot().pose.y == 7,
              "The tower's stairs, climbed facing south, lead up to its garrison");
        heal(*party);
        step(town, party, 0, noting);
        check(town.snapshot().pose.x == x && town.snapshot().pose.y == 5,
              "The tower's stairs lead back down");
    };
    climb(4);
    climb(11);
    check((gate(0x4A77) & 12) == 12, "Both towers' garrisons are beaten");
    // North of the gate lies Valjevo's old town.
    walk_to(town, party, 4, 0, noting);
    step(town, party, 0, peaceful);
    check(town.snapshot().area_id == 6, "The gate's north edge leads into Valjevo's old town");
    step(town, party, 2, peaceful);
    check(town.snapshot().area_id == 9, "The old town's south edge leads back to the gate");
    // South through the gate's passage, which opens from its north side, onto
    // the bugbears waiting beyond: with them beaten the gate is taken.
    heal(*party);
    walk_to(town, party, 7, 6, noting);
    step(town, party, 2, noting);
    check(heard.find("BUGBEARS WERE WAITING") != std::string::npos &&
              town.snapshot().dialogue.find("YOU HAVE TAKEN THE GATE") != std::string::npos &&
              (gate(0x4A77) & 16) != 0 && gate(0x4AB9) == 254,
          "The bugbears beyond the passage beaten, the gate is taken");
    for (unsigned n = 0; n < 3; ++n)
    {
        step(town, party, 2, noting);
    }
    check(town.snapshot().pose.x == 7 && town.snapshot().pose.y == 10,
          "The passage leads back out to the gate's south side");
    // Back south to Podol Plaza.
    walk_to(town, party, 4, 15, noting, true);
    step(town, party, 2, noting);
    check(town.snapshot().area_id == 18, "The gate's south edge leads back to Podol Plaza");
}

// Valjevo's old town (docs/audits/valjevo-old-town.md): through Stojanow
// Gate, the washerwomen's disguise, the Temple of Bane's blessing and altar,
// the smithy's fire giants, the castle refused, and the way back to the gate.
void valjevo_old_town(const std::filesystem::path &directory)
{
    std::string heard;
    auto trip = through_stojanow_gate(directory, 8, heard);
    auto &[party, town] = trip;
    walk_to(town, party, 4, 0, peaceful);
    step(town, party, 0, peaceful);
    check(town.snapshot().area_id == 6, "The gate's north edge leads into Valjevo's old town");
    const auto old_town = [&](std::uint16_t address)
    {
        return town.script_variable(por::EclAddress{address});
    };
    const auto answering = [&](std::vector<std::string> labels) -> Answer
    {
        return [&heard, labels](const por::TourSnapshot &s)
        {
            heard += s.dialogue + "\n";
            return prefer(labels)(s);
        };
    };
    // The washerwomen, spoken to nicely, give the party their clothes.
    walk_to(town, party, 12, 13, answering({"NICE", "YES", "LEAVE"}), true);
    check((old_town(0x4A65) & 1) != 0 &&
              town.snapshot().dialogue.find("WE WILL KEEP SILENCE") != std::string::npos,
          "The washerwomen disguise the party as townsfolk");
    // The Temple of Bane's blessing hurts every member who is not evil, and
    // the priest sees through the party.
    heal(*party);
    walk_to(town, party, 1, 2, answering({"ACCEPT BLESSING", "YES", "Fight", "Bash"}), true);
    check(heard.find("DOUBLES UP IN AGONY") != std::string::npos &&
              heard.find("A FIREBALL EXPLODES") != std::string::npos,
          "Bane's blessing hurts the party's good members and the priest attacks");
    // Behind the altar, the offering box and the crossed swords.
    heal(*party);
    walk_to(town, party, 1, 0, answering({"YES", "Fight", "Bash"}), true);
    check(heard.find("CROSSED SWORDS") != std::string::npos,
          "The altar keeps an offering box and two crossed swords");
    // The smith and his fire giants, fought.
    heal(*party);
    walk_to(town, party, 1, 14, answering({"Fight", "YES", "Bash"}), true);
    check((old_town(0x4A69) & 8) != 0, "The smith and his fire giants are beaten");
    // North lies Valjevo Castle, not in the game yet; south, the gate.
    heal(*party);
    walk_to(town, party, 3, 0, peaceful, true);
    refused_exit(town, party, 0, "Valjevo Castle");
    walk_to(town, party, 4, 15, peaceful, true);
    step(town, party, 2, peaceful);
    check(town.snapshot().area_id == 9, "The old town's south edge leads back to Stojanow Gate");
}

// Valhingen Graveyard (docs/audits/valhingen-graveyard.md): the way in from
// the city's north edge, the mystic who joins, the black marble crypt's coffin
// sanctified, the vampire in its pit and, fled to its coffin, destroyed there,
// and the way back out to the wilderness.
void valhingen_graveyard(const std::filesystem::path &directory)
{
    auto trip = create_party(directory);
    buy_and_equip(trip);
    auto &[party, town] = trip;
    party->award_experience(6 * 34000, "test:graveyard-level");
    for (const auto id : party->state().slots)
    {
        while (id && party->member(id).character.sheet().level < 8)
        {
            party->advance(id, party->default_advancement(id));
        }
    }
    opengold::test::TourAccess::set_variable(town, por::EclAddress{0x4AA7}, 255);
    walk_to(town, party, 11, 2);
    step(town, party, 0, prefer({"WEST"}));
    walk_to(town, party, 14, 1);
    step(town, party, 1, prefer({"STAY"}));
    std::string heard;
    const Answer noting = [&](const por::TourSnapshot &s)
    {
        heard += s.dialogue + "\n";
        return prefer({"GRAVEYARD", "LET HIM JOIN", "YES", "SANCTIFY", "Fight", "Bash"})(s);
    };
    travel_to(town, party, 13, 26, noting, [&] { return !town.snapshot().overland; });
    check(town.snapshot().area_id == 10 && town.snapshot().pose.x == 0 &&
              town.snapshot().pose.y == 4,
          "The city's north edge leads into Valhingen Graveyard");
    const auto graveyard = [&](std::uint16_t address)
    {
        return town.script_variable(por::EclAddress{address});
    };
    // The spectres behind the brass doors, met facing east, and in the tower
    // at (9,7); with them beaten the mystic comes.
    heal(*party);
    walk_to(town, party, 8, 1, noting, true);
    step(town, party, 1, noting);
    check(graveyard(0x4A42) == 2, "Spectres flow from between the brass doors");
    heal(*party);
    walk_to(town, party, 9, 7, noting, true);
    check(graveyard(0x4A42) == 3, "A spectre haunts the tower's lower chamber");
    // The mystic, who joins and names the vampire behind the undead.
    heal(*party);
    walk_to(town, party, 8, 9, noting, true);
    const auto &slots = party->state().slots;
    const auto mystic = std::find_if(slots.begin(), slots.end(),
                                     [&](const auto id)
                                     {
                                         return id && party->member(id).character.sheet().name ==
                                                          "Mystic";
                                     });
    check(graveyard(0x4A42) == 250 && mystic != slots.end() &&
              town.snapshot().dialogue.find("NORTHEAST CORNER") != std::string::npos,
          "The mystic joins the party and tells it of the vampire");
    const auto &sheet = party->member(*mystic).character.sheet();
    check(sheet.level == 6 && sheet.scores[rules::Ability::intelligence] == 15 &&
              sheet.scores[rules::Ability::constitution] == 10 &&
              sheet.scores[rules::Ability::strength] == 10 &&
              party->member(*mystic).equipped.size() == 1,
          "The mystic is a level-6 Wizard with his record's scores, his ring worn");
    // The black marble crypt: the scroll read and the coffin sanctified.
    heal(*party);
    walk_to(town, party, 12, 4, noting, true);
    check(graveyard(0x4A43) == 251 && heard.find("ENTRY 43") != std::string::npos,
          "The coffin in the black marble crypt is sanctified after its scroll is read");
    // The vampire's pit, down the wooden stairs.
    heal(*party);
    walk_to(town, party, 15, 0, noting, true);
    check(heard.find("TALL, PALE MAN") != std::string::npos && graveyard(0x4A41) == 250,
          "Beaten in its pit, the vampire flees as a vapor cloud");
    // Back at its sanctified coffin, the vampire is destroyed.
    heal(*party);
    walk_to(town, party, 12, 4, noting, true);
    check(heard.find("THERE IS A VAMPIRE HERE") != std::string::npos &&
              graveyard(0x4A41) == 255,
          "The vampire, weakened by its sanctified coffin, is destroyed there");
    // Out by the west edge.
    heal(*party);
    walk_to(town, party, 0, 4, noting, true);
    step(town, party, 3, prefer({"LEAVE"}));
    check(town.snapshot().overland && town.snapshot().overland_x == 12 &&
              town.snapshot().overland_y == 26,
          "The graveyard's west edge leads back to the city's north edge");
}

// Mendor's Library (docs/audits/mendors-library.md), east of the Cadorna
// Textile House: entered by the house's east edge and saved there, the
// wizard-locked doors opened with Knock, a book of history found by looking in
// the stacks, the basilisk fought for its treasure, the jar and the buried
// Manual of Bodily Health found by looking, the kobolds spared for their map,
// the mad fighter invited along, the Specter that guards the books met on the
// way out, and the way back west.
void mendors_library(Expedition &trip, const std::filesystem::path &folder,
                     const std::filesystem::path &directory)
{
    auto &[party, town] = trip;
    check(town.snapshot().area_id == 15 && town.snapshot().script_id == 15,
          "Leaving the Cadorna Textile House eastward enters Mendor's Library");
    const auto save = folder / "mendors-library.ogs";
    write_campaign_file(save, encode_campaign(*party, &town, campaign_asset_identity(directory)));
    check(load_expedition(save, directory).town.snapshot().area_id == 15,
          "A save in Mendor's Library loads back there");
    const auto books = [&]
    {
        return town.script_variable(por::EclAddress{0x4A2F});
    };
    const auto cleared = [&]
    {
        return town.script_variable(por::EclAddress{0x4A31});
    };
    const auto fight_them = prefer({"Bash", "COMBAT", "Fight", "YES"});
    // The library's doors are wizard locked; Knock opens the north one. Camp
    // until the wizard can cast it again, whatever fights came before.
    walk_to(town, party, 8, 1);
    rest_until(town, party, [&] { return party->can_try_door(DoorMethod::knock); },
               "The wizard regains a slot for Knock by camping");
    face(town, party, 2);
    town.explore(por::ExplorationCommand::forward);
    settle(town, party, prefer({"Knock"}));
    check(town.snapshot().pose.x == 8 && town.snapshot().pose.y == 2,
          "Knock opens the library's wizard-locked north door");

    // The history stacks: looking finds a book worth copying into the journal.
    heal(*party);
    visit(town, party, 11, 4, fight_them);
    check(look_until(town, party, [&] { return (books() & (4 | 8 | 16)) != 0; }, fight_them),
          "Looking through the history stacks finds a book for the journal");

    // The rhetoric stacks hide the basilisk.
    heal(*party);
    visit(town, party, 5, 4, fight_them);
    check((cleared() & 64) && carried(*party, "cloak_of_displacement"),
          "The basilisk falls and its treasure holds the Cloak of Displacement");

    // The mad fighter, asked nicely and urged twice, joins a party with room:
    // Skullcrusher leaves, as the party has room for two companions.
    for (const auto id : party->state().slots)
    {
        if (id && party->member(id).npc_source == "por:MON4CHA:27")
        {
            party->remove(id);
            break;
        }
    }
    visit(town, party, 11, 12, prefer({"Bash", "PARLAY", "NICE", "YES"}));
    const auto &slots = party->state().slots;
    const auto joined = std::find_if(slots.begin(), slots.end(),
                                     [&](MemberId id)
                                     {
                                         return id &&
                                                party->member(id).npc_source == "por:MON2CHA:25";
                                     });
    check((cleared() & 2) && joined != slots.end(), "The mad fighter joins the party");
    const auto &sheet = party->member(*joined).character.sheet();
    check(sheet.level == 1 && sheet.character_class == "Fighter" &&
              sheet.scores[rules::Ability::strength] == 17 &&
              sheet.scores[rules::Ability::constitution] == 10,
          "The mad fighter is a level-1 Fighter with his record's Strength 17");

    // A jar under the floorboards beside the bones, and a book in the study's
    // trash, are found by looking.
    heal(*party);
    visit(town, party, 8, 11, fight_them);
    check(look_until(town, party, [&] { return (books() & 32) != 0; }, fight_them),
          "Looking beside the bones finds the jar of potions");
    visit(town, party, 13, 13, fight_them);
    check(look_until(town, party, [&] { return (books() & 64) != 0; }, fight_them) &&
              carried(*party, "manual_of_bodily_health"),
          "Looking in the study's trash finds the Manual of Bodily Health");

    // The kobolds in the storeroom are spared for what they know.
    // Met across the room, the party advances and parleys.
    visit(town, party, 12, 10, prefer({"Bash", "Advance", "Parley", "YES"}));
    check(cleared() & 128, "The kobolds are spared and sketch a map");

    // Leaving with books, the party meets the Specter at the north door.
    heal(*party);
    walk_to(town, party, 8, 2, fight_them, true);
    step(town, party, 0, fight_them);
    check(cleared() & 32, "The Specter that guards the books is destroyed");
    const auto west = edge_opening(town.map(), town.snapshot().pose, 3);
    walk_to(town, party, west.x, west.y, fight_them);
    step(town, party, 3);
    check(town.snapshot().area_id == 2, "The library's west edge leads back to the textile house");
}

// The Cadorna Textile House's quest chain after a save and reload (issue #4):
// the silenced drum, Grishnak, the ogre chief, the sealed box and Skullcrusher
// are all still done, and walking back through their rooms fights nobody,
// finds no second treasure and recruits no second Skullcrusher.
void reload_cadorna_chain(const std::filesystem::path &save, const std::filesystem::path &directory)
{
    auto trip = load_expedition(save, directory);
    auto &[party, town] = trip;
    const auto chain = [&]
    {
        std::vector<std::uint16_t> flags;
        for (const std::uint16_t address : {0x4A73, 0x4A74, 0x4A75, 0x4AC8})
        {
            flags.push_back(town.script_variable(por::EclAddress{address}));
        }
        return flags;
    };
    const auto skullcrushers = [&]
    {
        const auto &slots = party->state().slots;
        return std::count_if(slots.begin(), slots.end(),
                             [&](MemberId id)
                             {
                                 return id && party->member(id).npc_source == "por:MON4CHA:27";
                             });
    };
    check(chain() == std::vector<std::uint16_t>{255, 2, 255, 1} && skullcrushers() == 1,
          "The reloaded save keeps every step of the textile house's chain");
    heal(*party);
    const auto fights = enemies_fought.size();
    const auto wealth = party->state().wealth;
    for (const auto &[x, y] :
         std::array<std::pair<unsigned, unsigned>, 4>{{{4, 10}, {3, 11}, {12, 14}, {2, 15}}})
    {
        visit(town, party, x, y, peaceful);
    }
    check(enemies_fought.size() == fights && party->state().wealth == wealth &&
              chain() == std::vector<std::uint16_t>{255, 2, 255, 1} && skullcrushers() == 1,
          "Revisiting the chain's rooms after the reload repeats none of it");
}

// The Cadorna Textile House (docs/audits/cadorna-textile-house.md), south of
// Podol Plaza: a save there, its exits to areas not in the game yet refused,
// a fall down the dry well, the courtyard's drum, Grishnak and her treasure,
// the ogre chief and Cadorna's box taken sealed, Skullcrusher unchained with
// the Brass Key and joining the party, and the way back north. Nine roaming fights are counted done
// (0x4AE4 = 9), so none meets the party in the streets while the chief lives.
void cadorna_textile_house(Expedition &trip, const std::filesystem::path &folder,
                           const std::filesystem::path &directory)
{
    auto &[party, town] = trip;
    walk_to(town, party, 4, 15, peaceful);
    step(town, party, 2);
    check(town.snapshot().area_id == 2 && town.snapshot().script_id == 2,
          "Leaving Podol Plaza southward enters the Cadorna Textile House");
    const auto save = folder / "cadorna-textile-house.ogs";
    write_campaign_file(save, encode_campaign(*party, &town, campaign_asset_identity(directory)));
    check(load_expedition(save, directory).town.snapshot().area_id == 2,
          "A save in the Cadorna Textile House loads back there");
    opengold::test::TourAccess::set_variable(town, por::EclAddress{0x4AE4}, 9);
    const auto west = edge_opening(town.map(), town.snapshot().pose, 3);
    walk_to(town, party, west.x, west.y);
    out_to_city_edge(town, party, 3, "SOUTH");
    check(town.snapshot().area_id == 2 && !town.snapshot().overland,
          "The textile house's west edge leads onto the wilderness map and back by the south entrance");

    // A fighter who climbs down the dry well falls 15d6; the party is put back
    // as it was after.
    const auto before_fall = party->checkpoint();
    const auto climber = party->state().slots[0];
    const auto hit_points = party->member(climber).vitals.hit_points;
    bool fell = false;
    walk_to(town, party, 1, 3,
            [&](const por::TourSnapshot &s) -> std::size_t
            {
                fell = fell || s.dialogue.find("YOU BEGIN A LONG FALL") != std::string::npos;
                for (std::size_t n = 0; n < s.choices.size(); ++n)
                {
                    if (s.choices[n] == "CLIMB DOWN")
                    {
                        return n;
                    }
                }
                return peaceful(s);
            });
    const auto &fallen = party->member(climber);
    check(fell && (fallen.vitals.dead || fallen.vitals.hit_points < hit_points),
          "A fighter climbing down the dry well falls and is hurt");
    party->restore(before_fall);

    // Garrisons are fought and locked doors bashed.
    const Answer fight_them = [](const por::TourSnapshot &s) -> std::size_t
    {
        if (!s.choices.empty() && s.choices[0] == "Bash")
        {
            return 0;
        }
        return s.choices.size() == 4 && s.choices[0] == "Fight" ? 0 : peaceful(s);
    };
    const auto fought = [&](std::string_view definition)
    {
        return std::find(enemies_fought.begin(), enemies_fought.end(), definition) !=
               enemies_fought.end();
    };
    heal(*party);
    visit(town, party, 4, 10, fight_them);
    check(town.script_variable(por::EclAddress{0x4A74}) == 1,
          "The courtyard's hobgoblins are beaten and the drum falls silent");
    heal(*party);
    visit(town, party, 3, 11, fight_them);
    const auto brass_key = [&]
    {
        for (const auto &member : party->state().roster)
        {
            for (const auto &[id, source] : member.item_sources)
            {
                if (source.stored.type == 75)
                {
                    return true;
                }
            }
        }
        return false;
    };
    check(fought("grishnak") && town.script_variable(por::EclAddress{0x4A74}) == 2 && brass_key(),
          "Grishnak falls and her treasure holds the Brass Key");
    heal(*party);
    visit(town, party, 12, 14, fight_them);
    check(fought("ogre") && town.script_variable(por::EclAddress{0x4A75}) == 255 &&
              town.script_variable(por::EclAddress{0x4AC8}) == 1,
          "The ogre chief falls and the party takes Cadorna's box sealed");
    // Unlocked, a polite word and a yes: Skullcrusher joins the party.
    heal(*party);
    visit(town, party, 2, 15,
          [&](const por::TourSnapshot &s) -> std::size_t
          {
              for (const auto *wanted : {"FREE", "UNLOCK", "PARLAY", "NICE", "YES"})
              {
                  for (std::size_t n = 0; n < s.choices.size(); ++n)
                  {
                      if (s.choices[n] == wanted)
                      {
                          return n;
                      }
                  }
              }
              return fight_them(s);
          });
    const auto &slots = party->state().slots;
    const auto joined = std::find_if(slots.begin(), slots.end(),
                                     [&](MemberId id)
                                     {
                                         return id &&
                                                party->member(id).npc_source == "por:MON4CHA:27";
                                     });
    const auto companion = joined == slots.end() ? MemberId{} : *joined;
    check(town.script_variable(por::EclAddress{0x4A73}) == 255 && companion,
          "The Brass Key unlocks Skullcrusher's padlocks and he joins the party");
    const auto &sheet = party->member(companion).character.sheet();
    check(sheet.name == "Skullcrusher" && sheet.level == 4 &&
              sheet.character_class == "Fighter" &&
              sheet.scores[rules::Ability::strength] == 18 &&
              sheet.scores[rules::Ability::dexterity] == 16 &&
              sheet.scores[rules::Ability::constitution] == 18 &&
              sheet.scores[rules::Ability::intelligence] == 10 &&
              sheet.scores[rules::Ability::wisdom] == 8 &&
              sheet.scores[rules::Ability::charisma] == 12 &&
              party->member(companion).character.inventory().items().empty(),
          "Skullcrusher is a level-4 Fighter with his record's scores and nothing carried");
    const auto joined_save = folder / "cadorna-skullcrusher.ogs";
    write_campaign_file(joined_save,
                        encode_campaign(*party, &town, campaign_asset_identity(directory)));
    reload_cadorna_chain(joined_save, directory);
    const auto east = edge_opening(town.map(), town.snapshot().pose, 1);
    walk_to(town, party, east.x, east.y, fight_them);
    write_campaign_file(folder / "cadorna-east-edge.ogs",
                        encode_campaign(*party, &town, campaign_asset_identity(directory)));
    step(town, party, 1);
    mendors_library(trip, folder, directory);
    const auto north = edge_opening(town.map(), town.snapshot().pose, 0);
    walk_to(town, party, north.x, north.y, fight_them);
    step(town, party, 0);
    check(town.snapshot().area_id == 18, "The textile house's north edge leads back to Podol Plaza");
}

// Podol Plaza's roaming group table, roll by roll (docs/audits/podol-plaza.md):
// from the save made on entering, each of the nine group rolls (0x9F86) is
// forced by reseeding the script's dice, and the group met on the next plaza
// step must be the one the script's tables give for that roll.
void podol_group_rolls(const std::filesystem::path &save, const std::filesystem::path &directory)
{
    struct Group
    {
        std::string_view name, soldier, leader;
        unsigned divisor, morale, threshold, leaders;
    };

    // The six group types (0xB269 records, 0xB275 divisors, 0xB281 morale,
    // 0xB287 leader thresholds, 0xB28D leaders) and which type each roll
    // picks (0xB260).
    const std::array<Group, 6> types{
        {{"GOBLINS", "slums-goblin", "slums-goblin-leader", 1, 30, 40, 5},
         {"GOBLINS", "slums-goblin", "slums-goblin-leader", 1, 35, 40, 5},
         {"ORCS", "slums-orc", "slums-orc-leader", 1, 40, 30, 4},
         {"ORCS", "slums-orc", "slums-orc-leader", 1, 45, 30, 4},
         {"HOBGOBLINS", "slums-hobgoblin", "slums-hobgoblin", 2, 45, 20, 3},
         {"OGRES", "ogre", "ogre", 4, 50, 11, 0}}};
    constexpr std::array<unsigned, 9> type_of_roll{0, 0, 1, 1, 2, 2, 3, 4, 5};
    for (unsigned roll = 0; roll < type_of_roll.size(); ++roll)
    {
        auto trip = load_expedition(save, directory);
        auto &[party, town] = trip;
        // Row 9 is open plaza (event 0) from (5,9) to (11,9).
        walk_to(town, party, 11, 9);
        face(town, party, 3);
        // The step's first die is the plaza's 0-25 (a group on 0), its second
        // the group's 0-8.
        std::uint32_t seed = 0;
        for (;; ++seed)
        {
            opengold::test::TourAccess::seed_dice(town, seed);
            auto dice = opengold::test::TourAccess::machine(town);
            if (dice.engine_random(26) == 0 && dice.engine_random(9) == roll)
            {
                break;
            }
        }
        const auto &group = types[type_of_roll[roll]];
        const auto fight_them = [](const por::TourSnapshot &s) -> std::size_t
        {
            return s.choices.size() == 4 && s.choices[0] == "Fight" ? 0 : peaceful(s);
        };
        const auto fights = fights_started.size();
        const auto strength = por::party_strength(*party);
        step(town, party, 3, fight_them);
        // The name the script prints: "<n> GOBLINS RUSH TO ATTACK."
        const auto name = opengold::test::TourAccess::machine(town).string(por::EclAddress{0x9800});
        check(name == " " + std::string(group.name) && fights_started.size() == fights + 1 &&
                  last_encounter,
              "Group roll " + std::to_string(roll) + " meets " + std::string(group.name));
        // The script's group: n to 2n for n = strength / divisor / 3, from 2
        // to 50, with leaders once it reaches the type's threshold.
        const unsigned size = town.script_variable(por::EclAddress{0x4A01});
        const unsigned leaders = town.script_variable(por::EclAddress{0x4A03});
        const auto n = strength / group.divisor / 3;
        check(size >= std::max(n, 2U) && size <= std::min(std::max(2 * n, 2U), 50U) &&
                  leaders == (size >= group.threshold ? group.leaders : 0U),
              "Group roll " + std::to_string(roll) + " sizes the group by its type");
        // The fight is that group, fitted to the party's XP budget.
        const auto &enemies = last_encounter->enemies;
        check(!enemies.empty() && enemies.size() <= size + leaders &&
                  std::all_of(enemies.begin(), enemies.end(),
                              [&](const rules::Participant &enemy)
                              {
                                  return enemy.definition == group.soldier ||
                                         enemy.definition == group.leader;
                              }),
              "Group roll " + std::to_string(roll) + " brings only its own creatures");
        check(last_encounter->morale == group.morale + (leaders ? 10 : 0),
              "Group roll " + std::to_string(roll) + " sets its type's morale");
    }
}

// Podol Plaza (docs/audits/podol-plaza.md), off the council's mission:
// entered by Kuto's Well's west edge, Stojanow Gate north and back, a
// roaming group met on the plaza, the robed orc fought for his holy symbols,
// a camp made, and the way back east to Kuto's Well.
void podol_plaza(Expedition &trip, const std::filesystem::path &folder,
                 const std::filesystem::path &directory)
{
    auto &[party, town] = trip;
    check(town.snapshot().area_id == 29, "The party starts in Kuto's Well");
    // The audit expects a party of about level 4 here; the expedition's is 2.
    party->award_experience(6 * 2700, "test:podol-plaza-level");
    for (const auto id : party->state().slots)
    {
        while (id && party->can_advance(id))
        {
            party->advance(id, party->default_advancement(id));
        }
    }
    heal(*party);
    const auto kuto = folder / "kutos-well-level-4.ogs";
    write_campaign_file(kuto, encode_campaign(*party, &town, campaign_asset_identity(directory)));
    podol_mission(kuto, directory);
    enter_podol(trip);
    const auto save = folder / "podol-plaza.ogs";
    write_campaign_file(save, encode_campaign(*party, &town, campaign_asset_identity(directory)));
    check(load_expedition(save, directory).town.snapshot().area_id == 18,
          "A save in Podol Plaza loads back there");
    podol_pitt(save, directory);
    podol_group_rolls(save, directory);
    // Every edge has openings at 4 and 11: north to Stojanow Gate and back.
    walk_to(town, party, 4, 0, peaceful);
    step(town, party, 0);
    check(town.snapshot().area_id == 9, "Podol Plaza's north edge leads to Stojanow Gate");
    step(town, party, 2);
    check(town.snapshot().area_id == 18, "Stojanow Gate's south edge leads back to Podol Plaza");
    walk_to(town, party, 0, 4, peaceful);
    out_to_city_edge(town, party, 3, "NORTH");
    check(town.snapshot().area_id == 18 && !town.snapshot().overland,
          "Podol Plaza's west edge leads onto the wilderness map and back by the north entrance");
    // Off the mission, a roaming group comes on one plaza step in 26; the
    // party flees it or fights. Nine wins in one visit clear the plaza; the
    // count (0x4A34) starts at eight so one fight stands for the first eight.
    const auto fight_them = [](const por::TourSnapshot &s) -> std::size_t
    {
        return s.choices.size() == 4 && s.choices[0] == "Fight" ? 0 : peaceful(s);
    };
    opengold::test::TourAccess::set_variable(town, por::EclAddress{0x4A34}, 8);
    const auto met = fights_started.size();
    const auto lead = party->state().slots[0];
    const auto experience = party->member(lead).experience;
    for (unsigned crossing = 0; crossing < 60 && fights_started.size() == met; ++crossing)
    {
        walk_to(town, party, crossing % 2 ? 5 : 11, 9, fight_them);
    }
    check(fights_started.size() > met && party->member(lead).experience > experience,
          "A roaming group meets the party on the plaza, and the party wins its XP");
    // The next plaza step marks the plaza cleared (0xB241).
    walk_to(town, party, town.snapshot().pose.x == 5 ? 11 : 5, 9, fight_them);
    check(town.script_variable(por::EclAddress{0x4AE7}) == 254,
          "The ninth win in one visit clears the plaza");
    // Camping on a cleared plaza: no roaming group comes to interrupt it.
    heal(*party);
    const auto cleared = fights_started.size();
    check(town.camp(RestKind::short_rest), "Camp starts on the plaza");
    settle(town, party, fight_them);
    check(fights_started.size() == cleared,
          "No roaming group interrupts a camp on a cleared plaza");
    if (party->state().short_rest)
    {
        party->finish_short_rest(party->state().short_rest->ticket);
    }
    // The robed orc in the Bane shrine fights a party that is not disguised;
    // he guards six leather holy symbols, the Temple of Bane's password.
    const auto holy_symbols = [&]
    {
        unsigned count = 0;
        for (const auto &member : party->state().roster)
        {
            for (const auto &[id, source] : member.item_sources)
            {
                if (source.stored.type == 74)
                {
                    count += member.character.inventory().find(id)->get().quantity;
                }
            }
        }
        return count;
    };
    heal(*party);
    const auto before = fights_started.size();
    visit(town, party, 14, 8,
          [](const por::TourSnapshot &s) -> std::size_t
          {
              if (s.dialogue.find("DO YOU TAKE IT?") != std::string::npos)
              {
                  return 0;
              }
              return peaceful(s);
          });
    check(fights_started.size() == before + 1 && holy_symbols() == 6 &&
              town.script_variable(por::EclAddress{0x4A38}) == 2,
          "The party defeats the robed orc and takes his six leather holy symbols");
    heal(*party);
    podol_temple(trip);
    cadorna_textile_house(trip, folder, directory);
    walk_to(town, party, 15, 4, peaceful);
    step(town, party, 1);
    check(town.snapshot().area_id == 29, "Podol Plaza's east edge leads back to Kuto's Well");
}

void slums_set_encounters(const std::filesystem::path &save, const std::filesystem::path &directory)
{
    const Answer fight_them = [](const por::TourSnapshot &s) -> std::size_t
    {
        for (std::size_t n = 0; n < s.choices.size(); ++n)
        {
            if (s.choices[n] == "Fight" || s.choices[n] == "Bash")
            {
                return n;
            }
        }
        return peaceful(s);
    };
    const auto fought = [](std::string_view definition)
    {
        return std::find(enemies_fought.begin(), enemies_fought.end(), definition) !=
               enemies_fought.end();
    };
    auto trip = load_expedition(save, directory);
    auto &[party, town] = trip;
    if (town.snapshot().area_id == 0)
    {
        walk_to(town, party, 0, 4);
        step(town, party, 3);
    }
    enemies_fought.clear();
    visit(town, party, 0, 2, fight_them);
    check(fought("slums-hobgoblin"), "The party fights the hobgoblins arguing over gold");
    visit(town, party, 1, 5, fight_them);
    check(fought("ogre") && fought("gnoll-warrior"),
          "The party fights the gathering of monster leaders");

    auto troll_trip = load_expedition(save, directory);
    auto &[troll_party, trolls] = troll_trip;
    if (trolls.snapshot().area_id == 0)
    {
        walk_to(trolls, troll_party, 0, 4);
        step(trolls, troll_party, 3);
    }
    // The trolls' room at (0, 14) opens only to the south.
    walk_to(trolls, troll_party, 0, 15, fight_them, true, true);
    face(trolls, troll_party, 0);
    trolls.explore(por::ExplorationCommand::forward);
    for (unsigned n = 0; n < 200 && trolls.snapshot().phase != por::TourPhase::combat; ++n)
    {
        if (trolls.snapshot().phase == por::TourPhase::awaiting_continue)
        {
            check(trolls.choose(trolls.snapshot().continue_ticket, 0), "The story continues");
        }
        else
        {
            trolls.advance(.5);
        }
    }
    if (trolls.monster_picture())
    {
        check(trolls.start_encounter(), "The monster close-up is dismissed");
    }
    const auto &enemies = trolls.pending_encounter().value().enemies;
    const auto has = [&](std::string_view definition)
    {
        return std::any_of(enemies.begin(), enemies.end(),
                           [&](const auto &e)
                           {
                               return e.definition == definition;
                           });
    };
    check(has("troll") && has("ogre") && trolls.script_diagnostics().empty(),
          "The trolls and ogres and their treasure reach combat");
}

// Saves for tests/playtest_slums.gd: the party in the Slums beside the
// arguing hobgoblins, beside the monster leaders and south of the trolls' room.
// The same save with every member advanced to level four, as the campaign
// simulator's arena does, so a play-test can win the fight and see what
// follows: monsters breaking, victory, treasure. Written beside it as
// NAME-strong.ogs.
void write_strong_fixture(const std::filesystem::path &save, const std::filesystem::path &directory)
{
    auto trip = load_expedition(save, directory);
    auto &party = *trip.party;
    party.award_experience(party.rule_module().experience_for_level(4), "play-test:strong");
    for (const auto id : party.state().slots)
    {
        if (id)
        {
            while (party.can_advance(id))
            {
                party.advance(id, party.default_advancement(id));
            }
        }
    }
    auto strong = save;
    strong.replace_filename(save.stem().string() + "-strong.ogs");
    write_campaign_file(strong,
                        encode_campaign(party, &trip.town, campaign_asset_identity(directory)));
}

void write_slums_fixtures(const std::filesystem::path &save, const std::filesystem::path &folder,
                          const std::filesystem::path &directory)
{
    std::filesystem::create_directories(folder);
    const Answer bash = [](const por::TourSnapshot &s) -> std::size_t
    {
        for (std::size_t n = 0; n < s.choices.size(); ++n)
        {
            if (s.choices[n] == "Bash")
            {
                return n;
            }
        }
        return peaceful(s);
    };
    auto trip = load_expedition(save, directory);
    auto &[party, town] = trip;
    if (town.snapshot().area_id == 0)
    {
        walk_to(town, party, 0, 4);
        step(town, party, 3);
    }
    const auto entry = folder / "slums-entry.ogs";
    write_campaign_file(entry, encode_campaign(*party, &town, campaign_asset_identity(directory)));
    write_approach_fixture(entry, 1, 5, folder / "leaders.ogs", directory, true, bash);
    // The hobgoblins argue at (0, 2) and are met coming south from (0, 1).
    {
        auto hobgoblins = load_expedition(entry, directory);
        walk_to(hobgoblins.town, hobgoblins.party, 0, 1, bash, true, true);
        face(hobgoblins.town, hobgoblins.party, 2);
        write_campaign_file(folder / "hobgoblins.ogs",
                            encode_campaign(*hobgoblins.party, &hobgoblins.town,
                                            campaign_asset_identity(directory)));
    }
    // The trolls' room at (0, 14) opens only to the south.
    walk_to(town, party, 0, 15, bash, true, true);
    face(town, party, 0);
    write_campaign_file(folder / "trolls.ogs",
                        encode_campaign(*party, &town, campaign_asset_identity(directory)));
    for (const auto *name : {"hobgoblins.ogs", "leaders.ogs", "trolls.ogs"})
    {
        write_strong_fixture(folder / name, directory);
    }
}

// Monsters meet the party at a distance only where it can see that far: turns
// the party the way with the longest view.
void face_longest_view(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party)
{
    const auto pose = town.snapshot().pose;
    unsigned longest = 0;
    for (unsigned facing = 1; facing < 4; ++facing)
    {
        if (open_squares_ahead(town.map(), {pose.x, pose.y, por::MapDirection{facing}}) >
            open_squares_ahead(town.map(), {pose.x, pose.y, por::MapDirection{longest}}))
        {
            longest = facing;
        }
    }
    face(town, party, longest);
}

// Camps on a Slums street until monsters interrupt with the encounter menu,
// then answers it with `answer` until the event ends. Returns the menus shown.
std::vector<por::TourSnapshot> meet_camp_monsters(Expedition &trip, const Answer &answer)
{
    auto &[party, town] = trip;
    if (town.snapshot().area_id == 0)
    {
        walk_to(town, party, 0, 4);
        step(town, party, 3);
    }
    walk_to(town, party, 14, 4);
    face_longest_view(town, party);
    std::vector<por::TourSnapshot> shown;
    for (unsigned tries = 0; tries < 80 && shown.empty(); ++tries)
    {
        check(town.camp(RestKind::short_rest), "Camp starts");
        while (town.snapshot().phase == por::TourPhase::running ||
               town.snapshot().phase == por::TourPhase::awaiting_continue)
        {
            const auto &s = town.snapshot();
            if (s.phase == por::TourPhase::running)
            {
                town.advance(.5);
                continue;
            }
            const bool menu =
                !s.choices.empty() && (s.choices[0] == "Fight" || s.choices[0] == "HAUGHTY");
            if (menu)
            {
                shown.push_back(s);
            }
            check(town.choose(s.continue_ticket, menu ? answer(s) : peaceful(s)),
                  "Answer accepted");
        }
        if (shown.empty())
        {
            if (const auto &spending = party->state().short_rest)
            {
                party->finish_short_rest(spending->ticket);
            }
        }
    }
    check(!shown.empty(), "Monsters interrupt a camp on a Slums street");
    return shown;
}

// The pre-combat menu offers Advance while the monsters are at a distance and
// Parley once they are adjacent; the leader parleys with its Charisma, and a
// meeting that ends without a fight says how.
void slums_encounter_menu(const std::filesystem::path &save, const std::filesystem::path &directory)
{
    auto talkers = load_expedition(save, directory);
    MemberId charming{};
    for (const auto id : talkers.party->state().slots)
    {
        if (id && (!charming ||
                   talkers.party->member(id).character.sheet().scores[rules::Ability::charisma] >
                       talkers.party->member(charming)
                           .character.sheet()
                           .scores[rules::Ability::charisma]))
        {
            charming = id;
        }
    }
    talkers.party->make_leader(charming);
    // A copy: the camp and fight below may rebuild the roster.
    const auto name = talkers.party->member(charming).character.sheet().name;
    unsigned reaction_score = 0;
    const auto menus = meet_camp_monsters(talkers,
                                          [&](const por::TourSnapshot &s) -> std::size_t
                                          {
                                              if (s.choices[0] != "HAUGHTY")
                                              {
                                                  return 3; // Advance until Parley, then Abusive.
                                              }
                                              reaction_score = talkers.town.script_variable(
                                                  por::EclAddress{0x6DCF});
                                              return 4;
                                          });
    check(menus.front().choices == std::vector<std::string>{"Fight", "Wait", "Flee", "Advance"},
          "Distant monsters can be advanced on, not parleyed with");
    const auto parley = std::find_if(menus.begin(), menus.end(),
                                     [](const auto &s)
                                     {
                                         return s.choices[0] == "HAUGHTY";
                                     });
    check(parley != menus.end() && parley[-1].choices.back() == "Parley",
          "Adjacent monsters can be parleyed with");
    check(parley->dialogue.ends_with("\n" + name + " speaks for the party.") &&
              reaction_score == 2u * talkers.party->member(charming)
                                         .character.sheet()
                                         .scores[rules::Ability::charisma],
          "The leader speaks for the party with twice its Charisma");

    auto waiting = load_expedition(save, directory);
    (void)meet_camp_monsters(waiting,
                             [](const por::TourSnapshot &) -> std::size_t
                             {
                                 return 1;
                             });
    check(waiting.town.snapshot().dialogue.find("The monsters go on their way.") !=
              std::string::npos,
          "Waiting monsters that leave say so");
    auto fleeing = load_expedition(save, directory);
    (void)meet_camp_monsters(fleeing,
                             [](const por::TourSnapshot &) -> std::size_t
                             {
                                 return 2;
                             });
    check(fleeing.town.snapshot().dialogue.starts_with("You get away."),
          "A party that escapes says so");
}

// A parley with street monsters that reaches `outcome` (its first option, such
// as "PAY" or "STAY") and answers it with `option` (and `amount` when gold is
// asked for). The leader is the most charming member and parleys Abusive. Each
// try first paces the street a different number of times, since steps roll
// for wandering monsters and so change the dice the parley meets.
struct ParleyResult
{
    Expedition trip;
    std::uint64_t gold_before{};
    std::vector<std::string> said;
};

std::uint64_t party_gold(const CampaignParty &party)
{
    std::uint64_t gold = 0;
    for (const auto id : party.state().slots)
    {
        if (id)
        {
            gold += party.state().wealth[3];
        }
    }
    return gold;
}

std::optional<ParleyResult> parley_once(const std::filesystem::path &save,
                                        const std::filesystem::path &directory, unsigned paces,
                                        std::string_view outcome, std::string_view option,
                                        std::string_view amount)
{
    ParleyResult result{load_expedition(save, directory)};
    auto &[party, town] = result.trip;
    MemberId charming{};
    for (const auto id : party->state().slots)
    {
        if (id && (!charming ||
                   party->member(id).character.sheet().scores[rules::Ability::charisma] >
                       party->member(charming).character.sheet().scores[rules::Ability::charisma]))
        {
            charming = id;
        }
    }
    party->make_leader(charming);
    if (town.snapshot().area_id == 0)
    {
        walk_to(town, party, 0, 4);
        step(town, party, 3);
    }
    walk_to(town, party, 14, 4);
    for (unsigned pace = 0; pace < paces; ++pace)
    {
        walk_to(town, party, 14, 3);
        walk_to(town, party, 14, 4);
    }
    result.gold_before = party_gold(*party);
    bool parleyed = false, answered = false;
    for (unsigned camps = 0; camps < 80; ++camps)
    {
        check(town.camp(RestKind::short_rest), "Camp starts");
        for (unsigned n = 0;
             n < 2000 && (town.snapshot().phase == por::TourPhase::running ||
                          town.snapshot().phase == por::TourPhase::awaiting_continue ||
                          town.snapshot().phase == por::TourPhase::awaiting_input);
             ++n)
        {
            const auto &s = town.snapshot();
            if (s.phase == por::TourPhase::running)
            {
                town.advance(.5);
                continue;
            }
            if (s.phase == por::TourPhase::awaiting_input)
            {
                check(town.input(s.continue_ticket, amount), "The offer is typed");
                continue;
            }
            if (parleyed)
            {
                result.said.push_back(s.dialogue);
            }
            std::size_t pick = peaceful(s);
            if (s.choices.size() == 4 && s.choices[0] == "Fight")
            {
                pick = 3; // Advance until Parley.
            }
            else if (s.choices.size() == 5 && s.choices[0] == "HAUGHTY")
            {
                pick = 4;
                parleyed = true;
            }
            else if (parleyed && !answered && s.choices.size() > 1)
            {
                if (std::find(s.choices.begin(), s.choices.end(), outcome) == s.choices.end())
                {
                    return std::nullopt;
                }
                pick = std::size_t(std::find(s.choices.begin(), s.choices.end(), option) -
                                   s.choices.begin());
                answered = true;
            }
            check(town.choose(s.continue_ticket, pick), "Answer accepted");
        }
        if (parleyed)
        {
            if (!answered)
            {
                return std::nullopt;
            }
            result.said.push_back(town.snapshot().dialogue);
            return result;
        }
        if (town.snapshot().phase == por::TourPhase::combat)
        {
            return std::nullopt;
        }
        if (const auto &spending = party->state().short_rest)
        {
            party->finish_short_rest(spending->ticket);
        }
    }
    return std::nullopt;
}

ParleyResult parley(const std::filesystem::path &save, const std::filesystem::path &directory,
                    std::string_view outcome, std::string_view option,
                    std::string_view amount = "0")
{
    for (unsigned paces = 0; paces < 40; ++paces)
    {
        if (auto result = parley_once(save, directory, paces, outcome, option, amount))
        {
            return std::move(*result);
        }
    }
    throw std::runtime_error("No parley reached the " + std::string(outcome) + " outcome");
}

bool said(const ParleyResult &result, std::string_view text)
{
    return std::any_of(result.said.begin(), result.said.end(),
                       [&](const auto &line)
                       {
                           return line.find(text) != std::string::npos;
                       });
}

// A parley that goes well enough asks a toll or orders the party out; each
// answer reaches the original script's outcome with real coins and moves.
void slums_parley_outcomes(const std::filesystem::path &save,
                           const std::filesystem::path &directory)
{
    const auto fought = [](const ParleyResult &r)
    {
        return r.trip.town.snapshot().phase == por::TourPhase::combat;
    };
    const auto moved = [](const ParleyResult &r)
    {
        const auto pose = r.trip.town.snapshot().pose;
        return pose.x != 14 || pose.y != 4;
    };
    auto cheap = parley(save, directory, "PAY", "PAY", "1");
    check(said(cheap, "CHEAPSKATES") && fought(cheap) &&
              party_gold(*cheap.trip.party) == cheap.gold_before,
          "Too small a toll starts a fight and costs nothing");
    auto paid = parley(save, directory, "PAY", "PAY", "500");
    check(said(paid, "PAW THROUGH YOUR COINS") && !fought(paid) && !moved(paid) &&
              party_gold(*paid.trip.party) <= paid.gold_before / 2 + 6,
          "A large enough toll costs half the party's coins");
    auto surrendered = parley(save, directory, "PAY", "SURRENDER");
    check(said(surrendered, "ROB AND BLINDFOLD") && moved(surrendered) &&
              party_gold(*surrendered.trip.party) == 0 && !said(surrendered, "You get away."),
          "Surrender costs every coin and sends the party elsewhere");
    auto ran = parley(save, directory, "PAY", "RUN");
    check(!fought(ran) && moved(ran) && said(ran, "You get away."),
          "Running from a toll as fast as the monsters gets away");
    auto left = parley(save, directory, "STAY", "LEAVE");
    check(!fought(left) && !moved(left), "Leaving when ordered out ends the meeting");
    auto stayed = parley(save, directory, "STAY", "STAY");
    check(fought(stayed), "Staying when ordered out starts a fight");
}

// The party meets street monsters on a camp and runs for the field's edge:
// those who get away rejoin it, those left on the field are lost, and the
// original script hears that the party fled.
void slums_flight(const std::filesystem::path &save, const std::filesystem::path &directory)
{
    auto trip = load_expedition(save, directory);
    auto &[party, town] = trip;
    if (town.snapshot().area_id == 0)
    {
        walk_to(town, party, 0, 4);
        step(town, party, 3);
    }
    walk_to(town, party, 14, 4);
    face_longest_view(town, party);
    for (unsigned camps = 0; camps < 80 && town.snapshot().phase != por::TourPhase::combat; ++camps)
    {
        check(town.camp(RestKind::short_rest), "Camp starts");
        while (town.snapshot().phase == por::TourPhase::running ||
               town.snapshot().phase == por::TourPhase::awaiting_continue)
        {
            const auto &s = town.snapshot();
            if (s.phase == por::TourPhase::running)
            {
                town.advance(.5);
            }
            else
            {
                check(town.choose(s.continue_ticket, stand_and_fight(s)), "Answer accepted");
            }
        }
        if (town.snapshot().phase != por::TourPhase::combat)
        {
            if (const auto &spending = party->state().short_rest)
            {
                party->finish_short_rest(spending->ticket);
            }
        }
    }
    check(town.snapshot().phase == por::TourPhase::combat, "Monsters attack the camp");
    if (town.monster_picture())
    {
        check(town.start_encounter(), "The monster close-up is dismissed");
    }
    const auto experience = party->member(party->state().slots[0]).experience;
    // A member that fails its flight stays and fights; the monsters wait for
    // what they see (AI-1). A seed whose fight ends with the party away, and
    // no monster killed, is used.
    std::unique_ptr<CombatDemo> fight;
    for (std::uint64_t seed = 42; seed < 52; ++seed)
    {
        fight = std::make_unique<CombatDemo>(module());
        auto &combat = *fight;
        combat.campaign_party(party);
        combat.encounter(*town.pending_encounter(), seed);
        const auto board = combat.combat().snapshot().battlefield;
        std::vector<rules::Cell> edges;
        for (int y = 0; y < board.height; ++y)
        {
            for (int x = 0; x < board.width; ++x)
            {
                if ((x == 0 || y == 0 || x == board.width - 1 || y == board.height - 1) &&
                    board.at({x, y}) != rules::Terrain::obstacle)
                {
                    edges.push_back({x, y});
                }
            }
        }
        const auto to_edge = [&](rules::Cell from)
        {
            int best = 1000;
            for (const auto edge : edges)
            {
                best =
                    std::min(best, std::max(std::abs(edge.x - from.x), std::abs(edge.y - from.y)));
            }
            return best;
        };
        for (unsigned n = 0;
             n < 2000 && combat.combat().snapshot().outcome == rules::Outcome::ongoing; ++n)
        {
            const auto s = combat.combat().snapshot();
            const auto offered = combat.combat().legal_commands();
            const auto mover = std::find_if(s.combatants.begin(), s.combatants.end(),
                                            [&](const auto &u)
                                            {
                                                return u.id == s.actor;
                                            });
            std::optional<rules::Command> pick;
            // A member that failed its flight fights on, as in the Flee mode.
            if (mover != s.combatants.end() && mover->side == rules::Side::party &&
                mover->can_flee && !s.reaction_pending)
            {
                for (const auto &c : offered)
                {
                    if (c.verb == "flee")
                    {
                        pick = c;
                    }
                }
                int closest = to_edge(mover->cell);
                if (!pick)
                {
                    for (const auto &c : offered)
                    {
                        if (c.verb == "move" && to_edge(c.destination) < closest)
                        {
                            pick = c;
                            closest = to_edge(c.destination);
                        }
                    }
                }
                if (!pick)
                {
                    for (const auto &c : offered)
                    {
                        if (c.verb == "end")
                        {
                            pick = c;
                        }
                    }
                }
            }
            check(combat.submit(pick ? *pick
                                     : choose_demo_command(combat.combat(), Searching::party)),
                  "Combat accepts the command");
        }
        const auto end = combat.combat().snapshot();
        if (end.outcome == rules::Outcome::fled &&
            std::none_of(end.combatants.begin(), end.combatants.end(),
                         [](const auto &unit)
                         {
                             return unit.side == rules::Side::opposition && unit.hit_points <= 0;
                         }))
        {
            break;
        }
    }
    auto &combat = *fight;
    const auto result = combat.combat().snapshot();
    check(result.outcome == rules::Outcome::fled, "The party flees the fight");
    check(town.resolve_combat(result), "Exploration accepts the flight");
    check(town.snapshot().phase != por::TourPhase::faulted &&
              town.script_variable(por::ecl_combat_result) == 129,
          "The original script hears that the party fled");
    bool someone_got_away = false;
    for (const auto &unit : result.combatants)
    {
        if (unit.side == rules::Side::party)
        {
            const auto &member = party->member(unit.id);
            const bool in_party =
                std::find(party->state().slots.begin(), party->state().slots.end(), unit.id) !=
                party->state().slots.end();
            if (unit.fled)
            {
                someone_got_away = true;
                check(in_party && !member.vitals.dead &&
                          member.vitals.hit_points == unit.hit_points &&
                          member.experience == experience,
                      "One who got away rejoins with its wounds and no experience for no kills");
            }
            else
            {
                check(!in_party && member.vitals.dead, "One left on the field is lost for good");
            }
        }
    }
    check(someone_got_away, "Someone got away");
}

// The Combat Sprite Catalog's export (#274) in the four-orc fight. A renamed
// creature fights under its new name, in its new art for the weapon set it
// readies, standing as the catalog previews it; one without exported art
// keeps its original icon, now with its action pose. An original item the
// export leaves out is not carried, so it is not left behind.
void exported_monsters(const std::filesystem::path &directory)
{
    const auto orc_fight = [&](std::shared_ptr<const por::MonsterBundle> monsters)
    {
        auto trip = create_party(directory, std::move(monsters));
        walk_to(trip.town, trip.party, 0, 4);
        step(trip.town, trip.party, 3);
        walk_to(trip.town, trip.party, 12, 1);
        check(last_encounter && orc_rewards(*trip.party) == 2, "The four orcs are fought");
        return std::pair{*last_encounter, party_items(*trip.party)};
    };
    const auto [original, original_items] = orc_fight({});
    auto bundle = por::load_monster_bundle(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/monsters",
                                           test::png_size_only);
    auto &tusker = bundle.monsters.at("MON2CHA.DAX:13");
    tusker.name = "TUSKER";
    std::erase_if(tusker.equipment,
                  [](const auto &item)
                  {
                      return item.source_index >= 0;
                  });
    check(std::ranges::find(tusker.equipment, "scimitar", &por::MonsterItem::name)->source_type == 30,
          "The catalog added the orc's scimitar as the original's item type 30");
    bundle.monsters.erase("MON2CHA.DAX:4");
    const auto tusker_art = tusker;
    const auto [exported, exported_items] =
        orc_fight(std::make_shared<const por::MonsterBundle>(std::move(bundle)));
    check(original.enemies.size() == exported.enemies.size(), "The same orcs are fought");
    bool renamed = false;
    for (std::size_t n = 0; n < exported.enemies.size(); ++n)
    {
        const auto &enemy = exported.enemies[n];
        const auto &art = exported.art[n];
        check(art.action.has_value(), "Every orc has an action pose");
        if (enemy.name.starts_with("TUSKER "))
        {
            renamed = true;
            check(art.planted && art.planted->scale == por::art_scale(tusker_art) && art.image.width == 128 &&
                      art.action->width == 128,
                  "The renamed orc stands in its new art, scaled as the catalog previews it");
            check(enemy.melee_weapon == "scimitar" && enemy.ranged_weapon.empty(),
                  "The renamed orc fights with the scimitar the catalog gave it");
        }
        else
        {
            check(!art.planted, enemy.name + " has no exported art and keeps its original icon");
            check(enemy.melee_weapon.empty(), enemy.name + " keeps its stat block's attacks");
        }
    }
    check(renamed, "The renamed orc fights under its new name");
    check(exported_items == original_items,
          "The renamed orc leaves its added scimitar behind, but not its scroll");
}

void installed_first_expedition(const std::filesystem::path &executable,
                                const std::filesystem::path &directory)
{
    const auto assets = campaign_asset_identity(directory);
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto folder =
        std::filesystem::temp_directory_path() / ("opengold-expedition-" + std::to_string(stamp));
    std::filesystem::create_directories(folder);
    const auto save = folder / "expedition.ogs";

    // The game's shop list is checked by tests/shop_disclosure_tests.gd from a
    // separate party standing outside the jeweler at (8,10).
    if (const auto *fixture = std::getenv("OPENGOLD_SHOP_FIXTURE"))
    {
        auto shopper = create_party(directory);
        walk_to(shopper.town, shopper.party, 8, 10);
        write_campaign_file(fixture, encode_campaign(*shopper.party, &shopper.town, assets));
        written_fixtures.insert("OPENGOLD_SHOP_FIXTURE");
    }
    // tests/training_hall_view_tests.gd steps north into the Fighters' hall at
    // (8,0) of the training area, with a party in three states: Arden can buy
    // a session, Bryn must advance first and Cora bought this level's session.
    if (const auto *fixture = std::getenv("OPENGOLD_HALL_FIXTURE"))
    {
        auto trainees = create_party(directory);
        walk_to(trainees.town, trainees.party, 8, 1);
        settle(trainees.town, trainees.party);
        while (trainees.town.snapshot().pose.facing != por::MapDirection::north)
        {
            trainees.town.explore(por::ExplorationCommand::turn_right);
            settle(trainees.town, trainees.party);
        }
        auto state = trainees.party->checkpoint();
        state.roster.at(1).experience = 300;
        state.roster.at(2).hall_experience_level = 1;
        trainees.party->restore(std::move(state));
        write_campaign_file(fixture, encode_campaign(*trainees.party, &trainees.town, assets));
        written_fixtures.insert("OPENGOLD_HALL_FIXTURE");
    }
    // A level-3 Hunter Ranger and a level-3 Circle of the Land Druid saved
    // right after a Long Rest ("-long.ogs") and a Short Rest ("-short.ogs"),
    // with the choices those rests can change still open, for screenshots.
    if (const auto *fixture = std::getenv("OPENGOLD_REST_CHOICE_FIXTURE"))
    {
        auto party = std::make_shared<CampaignParty>(module());
        auto ranger = character("Bryn", 3, "ranger").creation_data();
        ranger.training["class:ranger:weapon_mastery"] = {"longsword", "shortbow"};
        const auto bryn = party->add_pc(Character(*srd5::character_rules(), ranger, {}));
        const auto elin = party->add_pc(character("Elin", 4, "druid"));
        party->award_experience(1800, "rest-choices");
        for (unsigned level = 2; level <= 3; ++level)
        {
            auto ranger = party->default_advancement(bryn);
            auto druid = party->default_advancement(elin);
            if (level == 3)
            {
                ranger.training["subclass:ranger:hunter"] = {"colossus_slayer"};
                druid.training["subclass:druid:land"] = {"arid"};
            }
            party->advance(bryn, ranger);
            party->advance(elin, druid);
        }
        auto town = por::RolfTourSession::load(directory);
        town.campaign_party(party);
        settle(town, party);
        const auto base = std::filesystem::path(fixture);
        const auto after_rest = party->checkpoint();
        check(bool(party->rest(RestKind::long_rest)), "The fixture party takes a Long Rest");
        for (const auto id : {bryn, elin})
        {
            if (party->state().spell_rest)
            {
                party->keep_rest_spells(id);
            }
        }
        write_campaign_file(std::filesystem::path(base).concat("-long.ogs"),
                            encode_campaign(*party, &town, assets));
        party->restore(after_rest);
        check(bool(party->rest(RestKind::short_rest)), "The fixture party takes a Short Rest");
        party->finish_short_rest(party->state().short_rest->ticket);
        write_campaign_file(std::filesystem::path(base).concat("-short.ogs"),
                            encode_campaign(*party, &town, assets));
        written_fixtures.insert("OPENGOLD_REST_CHOICE_FIXTURE");
    }
    if (requested_fixtures_written())
    {
        return;
    }
    general_store_gear(directory);
    city_hall_rewards(directory);
    cadorna_payment(directory);
    exported_monsters(directory);
    auto trip = create_party(directory);
    buy_and_equip(trip);
    defeat_four_orcs(trip);
    const auto after_return = encode_campaign(*trip.party, &trip.town, assets);
    write_campaign_file(save, after_return);
    // Back at New Phlan's west gate: tests/area_crossing_tests.gd walks from
    // here into the Slums and back with the game's own controls.
    if (const auto *fixture = std::getenv("OPENGOLD_GATE_FIXTURE"))
    {
        write_campaign_file(fixture, after_return);
        written_fixtures.insert("OPENGOLD_GATE_FIXTURE");
        if (requested_fixtures_written())
        {
            return;
        }
    }
    rest_at_inn(trip);
    write_campaign_file(save, encode_campaign(*trip.party, &trip.town, assets));
    check(read_campaign_file(std::filesystem::path(save).concat(".bak")) == after_return,
          "Replacing a save keeps the previous one as a backup");
    accept_commission(trip);
    const auto accepted = encode_campaign(*trip.party, &trip.town, assets);
    write_campaign_file(checkpoint_save(folder, "accepted"), accepted);
    fetch_potion(trip, assets);
    const auto potion = encode_campaign(*trip.party, &trip.town, assets);
    write_campaign_file(checkpoint_save(folder, "potion"), potion);
    return_to_ohlo(trip);
    const auto at_ohlo_door = encode_campaign(*trip.party, &trip.town, assets);
    // The game's own save controls load this fixture in tests/ohlo_save_route_tests.gd.
    if (const auto *fixture = std::getenv("OPENGOLD_OHLO_FIXTURE"))
    {
        write_campaign_file(fixture, at_ohlo_door);
        written_fixtures.insert("OPENGOLD_OHLO_FIXTURE");
        if (requested_fixtures_written())
        {
            return;
        }
    }
    hand_in_potion(trip);
    write_campaign_file(save, encode_campaign(*trip.party, &trip.town, assets));
    {
        if (const auto *fixtures = std::getenv("OPENGOLD_SLUMS_FIXTURES"))
        {
            write_slums_fixtures(save, fixtures, directory);
            written_fixtures.insert("OPENGOLD_SLUMS_FIXTURES");
            if (requested_fixtures_written())
            {
                return;
            }
        }
        slums_set_encounters(save, directory);
        slums_encounter_menu(save, directory);
        slums_parley_outcomes(save, directory);
        slums_flight(save, directory);
        sokal_keep(directory);
        central_wilderness(directory);
        kovel_mansion(directory);
        temple_of_bane(directory);
        nomad_camp(directory);
        pyramid(directory);
        western_wilderness(directory);
        eastern_wilderness(directory);
        kobold_caves(directory);
        lizard_keep(directory);
        valhingen_graveyard(directory);
        stojanow_gate(directory);
        valjevo_old_town(directory);
        auto explorer = load_expedition(save, directory);
        enter_kutos_well(explorer, folder, directory);
        kutos_well_catacombs(explorer, folder, directory);
        auto podol = load_expedition(folder / "kutos-well.ogs", directory);
        podol_plaza(podol, folder, directory);
    }

    const auto program = "\"" + executable.string() + "\"";
    const auto resume = [&](std::string_view checkpoint)
    {
        const auto resumed = checkpoint_save(folder, checkpoint);
        check(run(program + " --resume " + std::string(checkpoint) + " \"" + resumed.string() +
                  "\"") == 0,
              "A fresh process loads the Slums save and plays on");
        return read_campaign_file(resume_result(resumed));
    };
    check(resume("accepted") == potion,
          "Fetching the potion after a reload matches fetching it without one");
    check(resume("potion") == at_ohlo_door,
          "Returning to Ohlo after a reload matches returning without one");
    check(run(program + " --revisit \"" + save.string() + "\"") == 0,
          "A fresh process reloads and revisits the event");
    revisit_orcs(trip);
    revisit_ohlo(trip);
    slums_camp_ambush(trip);
    check(read_campaign_file(revisit_result(save)) ==
              encode_campaign(*trip.party, &trip.town, assets),
          "Continuing after a reload, camp interruptions included, matches continuing without one");
    std::filesystem::remove_all(folder);
    std::cout
        << "Installed first expedition: created, equipped, defeated the four orcs, "
           "returned, paid the inn with change, rested, delivered Ohlo's potion, finished "
           "it again from each Slums save, reloaded, revisited and camped in the Slums, fought its "
           "hobgoblins and monster leaders, advanced on, parleyed with, paid, surrendered to, "
           "waited for and fled from its street monsters, met its trolls, won fights in "
           "Kuto's Well, "
           "defeated Norris and surrendered to him from a save, and went on west into Podol "
           "Plaza, its roaming groups and the robed orc.\n";
}

} // namespace

int main(int argc, char **argv)
{
    try
    {
        const auto *directory = std::getenv("OPENGOLD_GAME_DIR");
        if (argc == 3 && std::string_view(argv[1]) == "--revisit")
        {
            reload_and_revisit(argv[2], directory);
            return 0;
        }
        if (argc == 4 && std::string_view(argv[1]) == "--verify-hand-in")
        {
            verify_hand_in(argv[2], argv[3], directory);
            std::cout << "Ohlo hand-in save verified.\n";
            return 0;
        }
        if (argc == 4 && std::string_view(argv[1]) == "--verify-gate-crossing")
        {
            verify_gate_crossing(argv[2], argv[3], directory);
            std::cout << "Gate crossing save verified.\n";
            return 0;
        }
        // Plays Podol Plaza alone, from a save in Kuto's Well.
        if (argc == 3 && std::string_view(argv[1]) == "--podol")
        {
            const auto folder = std::filesystem::temp_directory_path() / "opengold-podol";
            std::filesystem::create_directories(folder);
            auto trip = load_expedition(argv[2], directory);
            podol_plaza(trip, folder, directory);
            std::filesystem::remove_all(folder);
            return 0;
        }
        // Plays the Cadorna Textile House alone, from a save in Podol Plaza.
        if (argc == 2 && std::string_view(argv[1]) == "--cadorna-payment")
        {
            cadorna_payment(directory);
            return 0;
        }
        if (argc == 3 && std::string_view(argv[1]) == "--cadorna")
        {
            const auto folder = std::filesystem::temp_directory_path() / "opengold-cadorna";
            std::filesystem::create_directories(folder);
            cadorna_payment(directory);
            auto trip = load_expedition(argv[2], directory);
            cadorna_textile_house(trip, folder, directory);
            std::filesystem::remove_all(folder);
            return 0;
        }
        // Plays Mendor's Library alone, from a save at the Cadorna Textile
        // House's east edge after Skullcrusher joined (cadorna-east-edge.ogs).
        if (argc == 3 && std::string_view(argv[1]) == "--library")
        {
            const auto folder = std::filesystem::temp_directory_path() / "opengold-library";
            std::filesystem::create_directories(folder);
            auto trip = load_expedition(argv[2], directory);
            step(trip.town, trip.party, 1);
            mendors_library(trip, folder, directory);
            std::filesystem::remove_all(folder);
            return 0;
        }
        // Plays the Central Wilderness alone with a new party.
        if (argc == 2 && std::string_view(argv[1]) == "--wilderness")
        {
            central_wilderness(directory);
            return 0;
        }
        // Plays Valjevo's old town alone with a new party.
        if (argc == 2 && std::string_view(argv[1]) == "--old-town")
        {
            valjevo_old_town(directory);
            return 0;
        }
        // Plays Stojanow Gate alone with a new party.
        if (argc == 2 && std::string_view(argv[1]) == "--gate")
        {
            stojanow_gate(directory);
            return 0;
        }
        // Plays Valhingen Graveyard alone with a new party.
        if (argc == 2 && std::string_view(argv[1]) == "--graveyard")
        {
            valhingen_graveyard(directory);
            return 0;
        }
        // Plays the lizard men's keep alone with a new party.
        if (argc == 2 && std::string_view(argv[1]) == "--lizards")
        {
            lizard_keep(directory);
            return 0;
        }
        // Plays the kobold caves alone with a new party.
        if (argc == 2 && std::string_view(argv[1]) == "--kobolds")
        {
            kobold_caves(directory);
            return 0;
        }
        // Plays the eastern wilderness alone with a new party.
        if (argc == 2 && std::string_view(argv[1]) == "--east")
        {
            eastern_wilderness(directory);
            return 0;
        }
        // Plays the western wilderness alone with a new party.
        if (argc == 2 && std::string_view(argv[1]) == "--west")
        {
            western_wilderness(directory);
            return 0;
        }
        // Plays the Pyramid alone with a new party.
        if (argc == 2 && std::string_view(argv[1]) == "--pyramid")
        {
            pyramid(directory);
            return 0;
        }
        // Plays the nomad camp alone with a new party.
        if (argc == 2 && std::string_view(argv[1]) == "--nomads")
        {
            nomad_camp(directory);
            return 0;
        }
        // Plays the Temple of Bane alone with a new party.
        if (argc == 2 && std::string_view(argv[1]) == "--temple")
        {
            temple_of_bane(directory);
            return 0;
        }
        // Plays Kovel Mansion alone with a new party.
        if (argc == 2 && std::string_view(argv[1]) == "--kovel")
        {
            kovel_mansion(directory);
            return 0;
        }
        // Plays Sokal Keep alone with a new party.
        if (argc == 2 && std::string_view(argv[1]) == "--sokal")
        {
            sokal_keep(directory);
            return 0;
        }
        if (argc == 4 && std::string_view(argv[1]) == "--resume")
        {
            resume_quest(argv[2], argv[3], directory);
            return 0;
        }
        armor_conversion_tests();
        coin_purse_tests();
        script_payment_tests();
        unsupported_program_tests();
        hall_event_tests();
        random_treasure_tables();
        script_treasure_tests();
        deferred_treasure_tests();
        locked_door_tests();
        if (directory && *directory)
        {
            installed_first_expedition(std::filesystem::absolute(argv[0]), directory);
        }
        std::cout << (requested_fixtures_written() ? "Expedition fixtures written.\n"
                                                   : "Expedition tests passed.\n");
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
