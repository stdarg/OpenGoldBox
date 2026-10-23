#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character_creator.h"
#include "opengold/coin_purse.h"
#include "opengold/combat_demo.h"
#include "opengold/random_treasure.h"
#include "opengold/rolf_tour.h"
#include "opengold/scroll_key.h"
#include "opengold/shop_data.h"
#include "opengold/npc_portraits.h"
#include "opengold/srd5.h"
#include "test_support.h"
#include "tour_access.h"
#include <algorithm>
#include <array>
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
constexpr std::array<const char *, 7> fixture_variables{
    "OPENGOLD_SHOP_FIXTURE",       "OPENGOLD_HALL_FIXTURE",   "OPENGOLD_REST_CHOICE_FIXTURE",
    "OPENGOLD_OHLO_FIXTURE",       "OPENGOLD_SLUMS_FIXTURES", "OPENGOLD_KUTO_WELL_FIXTURE",
    "OPENGOLD_KUTO_NORRIS_FIXTURE"};
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
        body.insert(body.end(), {3, 1, 0xB4, 0x6D, 0, 1, 22, 40, 0, 0, 0, 0, 0, 0});
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
    const auto failing = program({3, 1, 0x4B, 0xC0, 0, 1, 22, 40, 0, 0, 0, 0, 0, 0, 0});
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
    CombatDemo combat(module());
    combat.campaign_party(party);
    combat.encounter(*town.pending_encounter(), 42);
    FightStart start{town.snapshot().pose, town.pending_encounter()->distance, {}};
    check(start.distance <= open_squares_ahead(town.map(), start.pose),
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
                           : ": " + town.script_diagnostics().back()));
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
        town.explore(por::ExplorationCommand::turn_right);
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
            const auto &member = party.member(id);
            if (!member.vitals.dead &&
                member.vitals.hit_points < member.character.sheet().hit_points)
            {
                return true;
            }
        }
    }
    return false;
}

// Camps where the party stands until nobody living is wounded: a Long Rest when
// anyone is eligible, otherwise a Short Rest spending Hit Dice. Interruptions
// are played like any encounter, and the party rests again.
void recover(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party)
{
    for (unsigned tries = 0; party_wounded(*party); ++tries)
    {
        check(tries < 40, "The party recovers by camping");
        const auto info = party->rest_info(RestKind::long_rest);
        const bool long_rest = std::any_of(info.begin(), info.end(),
                                           [](const auto &member)
                                           {
                                               return member.denial == RestDenial::none;
                                           });
        check(town.camp(long_rest ? RestKind::long_rest : RestKind::short_rest), "Camp starts");
        settle(town, party);
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
            if (recovery.hit_dice && member.vitals.hit_points < member.character.sheet().hit_points)
            {
                (void)party->heal_with_hit_dice(party->state().short_rest->ticket, id);
            }
        }
        party->finish_short_rest(party->state().short_rest->ticket);
    }
}

// Breadth-first walk over open edges, through the original movement events.
// With bash_doors, locked doors are part of the route and are retried until forced.
// With camp_when_hurt, the party recovers by camping whenever a step leaves it wounded.
void walk_to(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party, unsigned tx,
             unsigned ty, const Answer &answer = peaceful, bool bash_doors = false,
             bool camp_when_hurt = false)
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
        if (p.x == tx && p.y == ty)
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
                const auto &a = town.map().at(cell % 16, cell / 16);
                const auto &b = town.map().at(x, y);
                const unsigned reverse = (d + 2) % 4;
                const auto blocked = [&](unsigned wall, unsigned door)
                {
                    return !locked(wall, door) && (door > 1 || (wall && !door));
                };
                if (blocked(a.walls[d], a.doors[d]) || blocked(b.walls[reverse], b.doors[reverse]))
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
        step(town, party, facing, answer);
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
// town that uses data/shops as the game does.
Expedition create_party(const std::filesystem::path &directory)
{
    auto party = std::make_shared<CampaignParty>(module());
    std::uint64_t seed = 1;
    for (const auto *name : {"Arden", "Bryn", "Cora", "Darin", "Elin", "Fenn"})
    {
        (void)party->add_pc(character(name, seed++));
        party->add_coins(Coin::gold, 250);
    }
    auto town = por::RolfTourSession::load(directory);
    auto shop_data = load_shop_data(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/shops");
    town.temples(std::move(shop_data.temples));
    town.shops(std::move(shop_data.shops));
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

void defeat_four_orcs(Expedition &trip)
{
    auto &[party, town] = trip;
    walk_to(town, party, 0, 4);
    step(town, party, 3);
    check(town.snapshot().area_id == 20, "The west gate leads into the Slums");
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
    check(town.snapshot().area_id == 0 && town.snapshot().pose.x == 0 &&
              town.snapshot().pose.y == 4,
          "The east edge of the Slums returns to New Phlan");
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
    check(refused && town.snapshot().area_id == 18 && town.snapshot().pose.x == before.x &&
              town.snapshot().pose.y == before.y,
          "The way to " + std::string(area) + " is refused and the party stays");
}

// Restores every living member's Hit Points, so one check's fight does not
// decide the next.
void heal(CampaignParty &party)
{
    auto state = party.checkpoint();
    for (auto &member : state.roster)
    {
        if (!member.vitals.dead)
        {
            member.vitals.hit_points = member.character.sheet().hit_points;
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
// temple's Heal list at its own prices; camping there is never interrupted.
void podol_temple(Expedition &trip)
{
    auto &[party, town] = trip;
    party->recruit("test:knock-wizard", knock_wizard());
    auto offerings = original_temple_offerings();
    offerings.at(2).price_gp = 90; // Ilmater's Cure Light Wounds, in its own record
    town.temples({{"GEO1.DAX", 18, {{1, 15}}, offerings}});
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

// Podol Plaza (docs/audits/podol-plaza.md), off the council's mission:
// entered by Kuto's Well's west edge, its exits to areas not in the game yet
// refused, a roaming group met on the plaza, the robed orc fought for his holy
// symbols, a camp made, and the way back east to Kuto's Well.
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
    // Every edge has openings at 4 and 11; only the east one leads anywhere
    // the game has.
    walk_to(town, party, 4, 0, peaceful);
    refused_exit(town, party, 0, "Stojanow Gate");
    walk_to(town, party, 0, 4, peaceful);
    refused_exit(town, party, 3, "the wilderness");
    walk_to(town, party, 4, 15, peaceful);
    refused_exit(town, party, 2, "Cadorna Textile House");
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
    auto trip = create_party(directory);
    buy_and_equip(trip);
    defeat_four_orcs(trip);
    const auto after_return = encode_campaign(*trip.party, &trip.town, assets);
    write_campaign_file(save, after_return);
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
        if (argc == 4 && std::string_view(argv[1]) == "--resume")
        {
            resume_quest(argv[2], argv[3], directory);
            return 0;
        }
        armor_conversion_tests();
        coin_purse_tests();
        script_payment_tests();
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
