// The Buck Rogers games' upkeep under their original rules, step B2 of
// docs/BUCK-ROGERS.md: the wounded after a fight, the medics, poison and
// antidotes, rest, the credit pool, shops, ammunition and bundles
// (docs/games/countdown.md, "Upkeep and money"). Each test names the routine
// of the original it follows. The checks of the games' own files run where
// the game finder finds them (installed_games.h).
#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/buck_rogers_upkeep.h"
#include "opengold/ecl.h"
#include "opengold/formats.h"
#include "installed_games.h"
#include "test_support.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace
{
using namespace opengold::original;
using opengold::GoldBoxGame;
using opengold::RandomState;
using opengold::test::check;
using Bytes = std::vector<std::uint8_t>;
using Item = opengold::buck_rogers::Item;
using ItemType = opengold::buck_rogers::ItemType;
using Status = BuckRogersStatus;

constexpr std::uint8_t sure = 100;

TeamMember member(Profession career, int hit_points, int maximum, Status status = Status::okay)
{
    TeamMember made;
    auto &character = made.character;
    character.career = record_career(career);
    character.abilities.fill(18);
    character.current_abilities.fill(18);
    character.hit_points = static_cast<std::uint16_t>(hit_points);
    character.hit_point_maximum = static_cast<std::uint16_t>(maximum);
    set_status(character, status);
    return made;
}

void learn(TeamMember &medic, Skill skill, std::uint8_t level = sure)
{
    level_of(medic.character.skills, skill) = level;
}

// A Medic sure of every treatment: its prerequisites met and each skill at
// 100, which no d100 can miss at difficulty 2.
TeamMember sure_medic()
{
    auto medic = member(Profession::medic, 20, 20);
    for (const auto skill :
         {Skill::first_aid, Skill::diagnose, Skill::treat_poisoning, Skill::treat_light_wounds,
          Skill::treat_serious_wounds, Skill::treat_critical_wounds, Skill::treat_stun})
    {
        learn(medic, skill);
    }
    return medic;
}

Item bundle(std::uint8_t type, std::uint8_t quantity, std::uint16_t value = 50)
{
    Item item;
    item.type = type;
    item.name_parts = {0, 16, type};
    item.quantity = quantity;
    item.value = value;
    return item;
}

Item weapon(std::uint8_t type, std::uint8_t rounds, std::uint16_t value = 210)
{
    Item item = bundle(type, rounds, value);
    item.flags = not_joinable;
    return item;
}

// The rows of ITEMS these tests use: a bolt gun (type 7, slug), a club
// (type 1, melee), a rocket launcher (type 18), a grenade (type 35) and an
// antidote (type 31, slot 8).
std::vector<ItemType> item_types()
{
    std::vector<ItemType> types(39);
    types[1] = {0, 2, 0, 0, 1, 3, 0, 0, 0};
    types[7] = {0, 2, 1, 0, 1, 4, 0, 16, 0};
    types[18] = {0, 2, 7, 0, 5, 10, 0, 40, 0};
    types[31] = {8, 0, 0, 0, 0, 0, 0, 0, 0};
    types[35] = {0, 2, 8, 0, 1, 4, 2, 3, 1};
    return types;
}

void a_status_sets_whether_the_character_acts()
{
    auto character = member(Profession::warrior, 10, 10).character;
    check(status_of(character) == Status::okay && character.enabled == 1,
          "types.pas TStatus: an okay character can act");
    set_status(character, Status::comatose);
    check(character.status == 8 && character.enabled == 0,
          "eclmenu.pas SpecialWriteZ1: a comatose character (status 8) cannot act");
}

void the_wounded_recover_after_a_fight()
{
    std::vector<TeamMember> team{member(Profession::warrior, 0, 30, Status::dying),
                                 member(Profession::rogue, 4, 20, Status::unconscious),
                                 member(Profession::medic, 0, 20, Status::unconscious),
                                 member(Profession::engineer, 9, 20, Status::fleeing),
                                 member(Profession::rocket_jock, 0, 20, Status::comatose),
                                 member(Profession::warrior, 12, 30, Status::sprinting)};
    recover_after_fight(team, false);
    check(status_of(team[0].character) == Status::unconscious && team[0].character.enabled == 0,
          "postcbt.pas DetermineCombatResults: the bleeding fall unconscious");
    check(status_of(team[1].character) == Status::okay && team[1].character.enabled == 1,
          "An unconscious member with Hit Points comes round");
    check(status_of(team[2].character) == Status::unconscious, "One with none stays unconscious");
    check(status_of(team[3].character) == Status::okay, "One who fled comes back");
    check(status_of(team[4].character) == Status::comatose, "The comatose stay in their coma");
    check(status_of(team[5].character) == Status::okay, "A sprinter is okay again");

    std::vector<TeamMember> revived{member(Profession::warrior, 0, 30, Status::dying),
                                    member(Profession::medic, 0, 20, Status::unconscious)};
    recover_after_fight(revived, true);
    check(std::all_of(revived.begin(), revived.end(),
                      [](const TeamMember &each)
                      {
                          return status_of(each.character) == Status::okay &&
                                 each.character.hit_points == 1;
                      }),
          "When the script revives the team, the bleeding and unconscious stand with 1 Hit Point");
}

void a_medic_cures_poison_and_then_the_wounds()
{
    std::vector<TeamMember> team{sure_medic(),
                                 member(Profession::warrior, 7, 40, Status::poisoned)};
    RandomState random{11};
    const auto report = treat_injuries(team, nullptr, random);
    check(!report.lines.empty() && report.lines.front().kind == TreatmentLine::Kind::poison_cured &&
              report.lines.front().helper == 0 && report.lines.front().patient == 1,
          "postcbt.pas TreatInjuries: the Medic's Treat Poisoning cures the patient");
    const auto &patient = team[1].character;
    check(status_of(patient) == Status::okay && patient.hit_points > 6,
          "The cured patient, left at 0 Hit Points, is then treated for its wounds");
    const auto wound_lines =
        std::count_if(report.lines.begin(), report.lines.end(),
                      [](const TreatmentLine &line)
                      {
                          return line.kind == TreatmentLine::Kind::wounds_healed;
                      });
    check(wound_lines == 2, "First Aid, then one Treat Wounds skill");
    check(report.summary == TreatmentSummary::healing_completed, "Healing completed");
}

void without_a_medic_poison_takes_an_antidote_or_a_coma()
{
    auto carrier = member(Profession::rogue, 10, 10);
    carrier.items = {bundle(antidote_type, 2, 400)};
    std::vector<TeamMember> team{member(Profession::warrior, 10, 30, Status::poisoned), carrier,
                                 member(Profession::engineer, 10, 30, Status::poisoned)};
    std::vector<std::size_t> asked_carriers;
    int answers = 0;
    RandomState random{3};
    const auto report = treat_injuries(
        team,
        [&](std::size_t, std::size_t carrier_index)
        {
            asked_carriers.push_back(carrier_index);
            return answers++ == 0;
        },
        random);
    check(asked_carriers == std::vector<std::size_t>{1, 1},
          "TreatInjuries UseAntidote: the first member carrying an antidote is asked about");
    check(status_of(team[0].character) == Status::unconscious && team[0].character.hit_points == 0,
          "The antidote cures: unconscious with 0 Hit Points");
    check(status_of(team[2].character) == Status::comatose && team[2].character.enabled == 0,
          "Declining it leaves the patient to slip into a coma");
    check(team[1].items.size() == 1 && team[1].items[0].quantity == 1,
          "One antidote of the bundle is used");
    check(report.summary == TreatmentSummary::no_medics,
          "Without a medic the report says none is available");

    std::vector<TeamMember> alone{member(Profession::warrior, 10, 30, Status::poisoned)};
    const auto unanswered = treat_injuries(alone, nullptr, random);
    check(status_of(alone[0].character) == Status::comatose &&
              unanswered.lines.back().kind == TreatmentLine::Kind::coma,
          "With no antidote the poisoned member slips into a coma");
}

void first_aid_stops_at_six_and_one_wound_skill_follows()
{
    auto first_aider = member(Profession::warrior, 30, 30);
    learn(first_aider, Skill::first_aid);
    std::vector<TeamMember> team{first_aider, first_aider,
                                 member(Profession::rogue, 0, 40, Status::unconscious)};
    RandomState random{5};
    const auto report = treat_injuries(team, nullptr, random);
    check(team[2].character.hit_points <= 6 && team[2].character.hit_points >= 1 &&
              status_of(team[2].character) == Status::okay,
          "TreatInjuries: each member's First Aid, 1d6, never above 6 Hit Points");
    check(report.summary == TreatmentSummary::healing_completed, "First Aid counts as treatment");

    for (std::uint64_t seed = 0; seed < 50; ++seed)
    {
        std::vector<TeamMember> wounded{sure_medic(), member(Profession::warrior, 10, 200)};
        RandomState rolls{seed};
        const auto treated = treat_injuries(wounded, nullptr, rolls);
        const int healed = wounded[1].character.hit_points - 10;
        // First Aid adds nothing above 6; Treat Critical Wounds twice 3d8 + 3.
        check(treated.lines.size() == 1 && treated.lines[0].skill == Skill::treat_critical_wounds &&
                  healed >= 12 && healed <= 54,
              "Treat Critical Wounds heals twice 3d8 + 3, and no other skill follows it");
    }

    std::vector<TeamMember> nearly{sure_medic(), member(Profession::warrior, 38, 40)};
    RandomState rolls{9};
    static_cast<void>(treat_injuries(nearly, nullptr, rolls));
    check(nearly[1].character.hit_points == 40, "No treatment heals past the maximum");
}

void treating_ones_own_critical_wounds_is_harder()
{
    int self_cured = 0;
    constexpr int trials = 400;
    for (std::uint64_t seed = 0; seed < trials; ++seed)
    {
        auto medic = member(Profession::medic, 10, 100);
        learn(medic, Skill::treat_serious_wounds, 40);
        learn(medic, Skill::treat_critical_wounds, sure);
        std::vector<TeamMember> team{medic};
        RandomState random{seed};
        const auto report = treat_injuries(team, nullptr, random);
        self_cured +=
            static_cast<int>(std::count_if(report.lines.begin(), report.lines.end(),
                                           [](const TreatmentLine &line)
                                           {
                                               return line.skill == Skill::treat_critical_wounds;
                                           }));
    }
    // The value 118 is quartered to 29 at difficulty 4; at 2 it never misses.
    check(self_cured > trials / 10 && self_cured < trials / 2,
          "TreatInjuries: a medic's Treat Critical Wounds on itself is at difficulty 4");
}

void the_report_says_why_nothing_was_done()
{
    std::vector<TeamMember> healthy{sure_medic(), member(Profession::warrior, 30, 30)};
    RandomState random{1};
    check(treat_injuries(healthy, nullptr, random).summary == TreatmentSummary::no_injuries,
          "TreatInjuries: no injuries were sustained");
    std::vector<TeamMember> no_skill{member(Profession::warrior, 10, 30),
                                     member(Profession::rogue, 20, 20)};
    check(treat_injuries(no_skill, nullptr, random).summary == TreatmentSummary::no_medics,
          "No one with a treatment skill: no medics are available");
    auto unlucky = member(Profession::rogue, 20, 20);
    learn(unlucky, Skill::first_aid, 1);
    std::vector<TeamMember> failing{member(Profession::warrior, 10, 30), unlucky};
    int nothing = 0;
    for (std::uint64_t seed = 0; seed < 20; ++seed)
    {
        auto team = failing;
        RandomState rolls{seed};
        nothing +=
            treat_injuries(team, nullptr, rolls).summary == TreatmentSummary::nothing_treated;
    }
    check(nothing > 0, "A skill that fails: no injuries were successfully treated");
}

void an_unkillable_member_stands_with_45()
{
    auto tough = member(Profession::warrior, 0, 80, Status::comatose);
    tough.unkillable = true;
    std::vector<TeamMember> team{tough};
    RandomState random{2};
    static_cast<void>(treat_injuries(team, nullptr, random));
    check(status_of(team[0].character) == Status::okay && team[0].character.hit_points == 45,
          "TreatInjuries: the Unkillable effect puts a member below 45 back up with 45");
}

void aid_bandages_the_bleeding_and_ends_a_stun()
{
    auto medic = member(Profession::medic, 20, 20);
    learn(medic, Skill::first_aid, 5);
    auto dazed = member(Profession::warrior, 20, 30);
    dazed.stunned = true;
    std::vector<TeamMember> team{medic, member(Profession::rogue, 0, 20, Status::dying), dazed};
    check(find_patients(0, team) == std::vector<std::size_t>{1},
          "viewchar.pas FindPatients: First Aid finds the bleeding; the stunned need Treat Stun");
    learn(team[0], Skill::treat_stun);
    const auto patients = find_patients(0, team);
    check(patients == std::vector<std::size_t>{1, 2}, "With Treat Stun the stunned are patients");
    RandomState random{4};
    const auto done = treat_patients(0, team, patients, random);
    check(done.size() == 2 && done[0].kind == PatientTreatment::Kind::bandaged &&
              status_of(team[1].character) == Status::unconscious &&
              team[1].character.hit_points == 0,
          "viewchar.pas TreatPatients: the bleeding patient is bandaged, without a roll");
    check(done[1].kind == PatientTreatment::Kind::stun_ended && !team[2].stunned,
          "A Treat Stun check ends the patient's stun");
}

void rest_heals_a_point_a_day()
{
    std::vector<TeamMember> team{member(Profession::warrior, 0, 30, Status::unconscious),
                                 member(Profession::rogue, 18, 20),
                                 member(Profession::medic, 0, 20, Status::dying),
                                 member(Profession::engineer, 5, 20, Status::poisoned),
                                 member(Profession::rocket_jock, 0, 20, Status::comatose)};
    rest_and_heal(team, rest_days);
    check(status_of(team[0].character) == Status::okay && team[0].character.hit_points == 1,
          "gametime.pas RestAndHeal: the unconscious come round and gain the day's point");
    check(team[1].character.hit_points == 19, "A day's rest heals one Hit Point");
    check(status_of(team[2].character) == Status::dying && team[2].character.hit_points == 0 &&
              status_of(team[3].character) == Status::poisoned &&
              team[3].character.hit_points == 5 && status_of(team[4].character) == Status::comatose,
          "The bleeding, the poisoned and the comatose gain nothing");
    rest_and_heal(team, solar_map_step_days);
    check(solar_map_step_days == 3 && team[0].character.hit_points == 4 &&
              team[1].character.hit_points == 20,
          "explore.pas MoveRocket: a step on the solar map is three days, up to the maximum");
}

void the_pool_is_shared_among_those_it_takes_from()
{
    std::vector<TeamMember> team(4, member(Profession::warrior, 10, 10));
    team[0].character.credits = 100;
    team[1].character.credits = 50;
    team[2].character.npc = party_member_npc;
    team[2].character.credits = 51;
    team[3].character.npc = 0x85;
    team[3].character.credits = 1000;
    CreditPool pool;
    pool_credits(team, pool);
    check(pool.credits == 201 && team[3].character.credits == 1000,
          "money.pas PoolMoney: player characters' and party-member NPCs' credits go in");
    share_credits(team, pool);
    check(pool.credits == 0 && team[0].character.credits == 67 && team[1].character.credits == 67 &&
              team[2].character.credits == 67 && team[3].character.credits == 1000,
          "ShareMoney: the pool is shared among those it took from, the party-member NPC too");
    pool.credits = 10;
    share_credits(team, pool);
    check(team[0].character.credits == 71 && team[1].character.credits == 70 &&
              team[2].character.credits == 70,
          "The remainder goes a credit each to the first of them");

    pool.credits = 30;
    take_credits(team[0].character, pool, 50);
    check(pool.credits == 0 && team[0].character.credits == 101,
          "TakeMoney: asking for more than the pool takes all of it");
    team[0].character.credits = 70'000;
    give_credits(team[0].character, team[1].character, 70'000);
    check(team[0].character.credits == 0 && team[1].character.credits == 70'070,
          "viewchar.pas TradeTreasureMenu: the whole amount is handed over, not its last 16 bits");
}

void a_shop_stocks_its_list_last_first()
{
    std::vector<Item> templates(38);
    for (std::size_t number = 1; number <= templates.size(); ++number)
    {
        templates[number - 1] = bundle(static_cast<std::uint8_t>(number), 1);
    }
    templates[29 - 1].type = Item::no_type;
    const auto stock = shop_stock(2, templates);
    check(stock.size() == 5 && stock.front().type == 16 && stock.back().type == 2,
          "eclvm.pas EclShop: list 2 is templates 2, 8, 11, 13, 16, shown last first");
    const auto with_gap = shop_stock(3, templates);
    check(with_gap.size() == 3, "A template that holds no item is left out");
    check(opengold::test::throws<std::out_of_range>(
              [&]
              {
                  static_cast<void>(shop_stock(17, templates));
              }),
          "There are 16 lists");
}

void prices_scale_by_the_multiplier()
{
    const auto gun = weapon(7, 10, 210);
    check(shop_price(gun, 0) == 210 && shop_price(gun, 16) == 210 && shop_price(gun, 32) == 420 &&
              shop_price(gun, 24) == 315,
          "shop.pas ItemPrice: the value times cell 7F6Dh over 16, the value when it is 0");
    check(shop_price(weapon(7, 10, 0), 32) == 2, "ShopBuyItemSelection: a value of 0 counts as 1");
    check(sale_price(gun) == 105 && sale_price(bundle(35, 12, 50)) == 300 &&
              sale_price(bundle(35, 1, 1)) == 0,
          "viewchar.pas SellItem: half the value, for each item of a bundle");
    TeamMember seller = member(Profession::rogue, 10, 10);
    seller.items = {gun, bundle(35, 12, 50)};
    sell_item(seller, 1);
    check(seller.items.size() == 1 && seller.character.credits == 300, "The sold item leaves");
}

void buying_takes_own_credits_then_the_pool()
{
    auto buyer = member(Profession::warrior, 10, 10);
    buyer.character.credits = 150;
    CreditPool pool{100};
    const auto gun = weapon(7, 10, 210);
    check(buy_item(buyer, gun, 16, pool) == PurchaseResult::bought &&
              buyer.character.credits == 0 && pool.credits == 40 && buyer.items.size() == 1,
          "shop.pas ShopBuyMenu: all the buyer's credits, the rest from the pool");
    check(buy_item(buyer, gun, 16, pool) == PurchaseResult::not_enough_money &&
              buyer.items.size() == 1 && pool.credits == 40,
          "Not enough money: nothing changes");

    buyer.character.credits = 1000;
    buyer.items = {bundle(35, 5)};
    check(buy_item(buyer, bundle(35, 1), 16, pool) == PurchaseResult::bought &&
              buyer.items.size() == 1 && buyer.items[0].quantity == 6 &&
              buyer.character.credits == 950,
          "GiveItemCopyToCreature: a bought grenade joins the bundle");

    buyer.items.assign(15, weapon(1, 0, 10));
    buyer.items.push_back(bundle(35, max_bundle));
    const auto credits = buyer.character.credits;
    check(buy_item(buyer, bundle(35, 1), 16, pool) == PurchaseResult::overloaded &&
              buyer.items.size() == 16 && buyer.items.back().quantity == max_bundle &&
              buyer.character.credits == credits,
          "Overloaded: the full bundle keeps its 100 grenades and nothing is paid");
}

void ammunition_comes_in_clips_and_rounds()
{
    const auto types = item_types();
    const auto gun = weapon(7, 100);
    check(takes_ammunition(gun, types) && !takes_ammunition(weapon(1, 0), types) &&
              !takes_ammunition(bundle(35, 3), types),
          "shop.pas ShopAmmoMenu: missile weapons, not melee weapons or grenades");
    const auto clip = ammunition_offer(gun, types, false);
    const auto fill = ammunition_offer(gun, types, true);
    check(clip.rounds == 10 && clip.cost == 10 && fill.rounds == 150 && fill.cost == 150,
          "AmmoValues: a clip of 10 rounds, a fill to 250, each 10 rounds at ammoPrices");
    const auto almost = ammunition_offer(weapon(7, 245), types, false);
    check(almost.rounds == 5 && almost.cost == 10, "Part of a clip costs a whole one");
    const auto launcher = weapon(18, 3);
    const auto round = ammunition_offer(launcher, types, false);
    const auto refill = ammunition_offer(launcher, types, true);
    check(round.rounds == 1 && round.cost == 100 && refill.rounds == 7 && refill.cost == 700,
          "A rocket launcher holds 10 and is sold a round at a time");

    auto buyer = member(Profession::warrior, 10, 10);
    buyer.character.credits = 5;
    buyer.items = {gun, weapon(18, 10)};
    CreditPool pool{200};
    check(buy_ammunition(buyer, 0, types, true, pool) == AmmunitionResult::bought &&
              buyer.items[0].quantity == 250 && buyer.character.credits == 0 && pool.credits == 55,
          "The fill is paid with the buyer's credits and the pool");
    check(buy_ammunition(buyer, 1, types, false, pool) == AmmunitionResult::already_full,
          "A full weapon is already full");
    buyer.items[1].quantity = 0;
    check(buy_ammunition(buyer, 1, types, true, pool) == AmmunitionResult::not_enough_money &&
              pool.credits == 55,
          "Not enough money buys nothing");
}

void bundles_halve_and_join()
{
    std::vector<Item> gear{bundle(35, 25), weapon(7, 10)};
    gear[0].flags = Item::readied;
    check(halve_item(gear, 0) && gear.size() == 3 && gear[0].quantity == 13 &&
              gear[1].quantity == 12 && (gear[1].flags & Item::readied) == 0,
          "viewchar.pas HalveItem: the larger half stays, the smaller follows, not readied");
    std::vector<Item> single{bundle(35, 1)};
    check(!halve_item(gear, 2) && !halve_item(single, 0),
          "A weapon and a bundle of one cannot be halved");
    std::vector<Item> full(max_carried_items, bundle(31, 4));
    check(!halve_item(full, 0), "Nor anything by a character carrying 16 items");

    std::vector<Item> grenades{bundle(35, 60), weapon(7, 10), bundle(35, 70), bundle(36, 5),
                               bundle(35, 0)};
    join_item(grenades, 0);
    // 60 and 70 make 100 and 30; the empty one counts as 1 and joins the 30.
    check(grenades.size() == 4 && grenades[0].quantity == 100 && grenades[1].type == 7 &&
              grenades[2].quantity == 31 && grenades[3].type == 36,
          "viewchar.pas JoinItem: like items join the chosen one up to 100, the rest the next");
    std::vector<Item> overflow{bundle(35, 60), bundle(35, 70), bundle(35, 50)};
    join_item(overflow, 0);
    check(overflow.size() == 2 && overflow[0].quantity == 100 && overflow[1].quantity == 80,
          "Later like items join the next bundle");
    std::vector<Item> weapons{weapon(7, 10), weapon(7, 20)};
    join_item(weapons, 0);
    check(weapons.size() == 2, "Weapons never join");
}

void the_hospitals_fee_check_reads_the_whole_fee()
{
    check(
        script_operand_tag(GoldBoxGame::countdown_to_doomsday, "ECL6.DAX", 99, 0x84F1, 0, 1) == 3 &&
            script_operand_tag(GoldBoxGame::countdown_to_doomsday, "ECL6.DAX", 99, 0x850B, 0, 1) ==
                3 &&
            script_operand_tag(GoldBoxGame::matrix_cubed, "ECL1.DAX", 20, 0x853B, 0, 1) == 3 &&
            script_operand_tag(GoldBoxGame::matrix_cubed, "ECL1.DAX", 20, 0x8555, 0, 1) == 3,
        "The hospital's fee checks (script 99 of ECL6, script 20 of ECL1) read the word");
    check(
        script_operand_tag(GoldBoxGame::countdown_to_doomsday, "ECL6.DAX", 99, 0x84F1, 1, 5) == 5 &&
            script_operand_tag(GoldBoxGame::matrix_cubed, "ECL6.DAX", 99, 0x84F1, 0, 1) == 1 &&
            script_operand_tag(GoldBoxGame::countdown_to_doomsday, "ECL6.DAX", 99, 0x8500, 0, 1) ==
                1,
        "Every other operand keeps its tag");
}

std::optional<std::filesystem::path> files_of(GoldBoxGame game)
{
    const auto folder = opengold::test::installed_game_folder(game);
    if (!std::filesystem::exists(folder / "SHIPS.DAX"))
    {
        std::cout << opengold::game_title(game) << "'s files are not here; they are not read.\n";
        return std::nullopt;
    }
    return folder;
}

Bytes file_bytes(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

void the_games_shops_and_weapons(const std::filesystem::path &folder)
{
    const auto record = opengold::decode_dax_record(file_bytes(folder / "ITEM0.DAX"), 1);
    check(record.has_value(), "ITEM0.DAX holds the templates");
    const auto templates = opengold::buck_rogers::decode_item_templates(*record);
    const auto types = opengold::buck_rogers::decode_item_types(file_bytes(folder / "ITEMS"));
    check(templates && types, "The templates and item types decode");
    check(shop_stock(1, *templates).size() == 16,
          "EclShop: Salvation's list 1 stocks all 16 of its items");
    check(shop_stock(9, *templates).size() == 8,
          "List 9 names templates 92 and 93, which ITEM0.DAX does not hold");
    for (unsigned list = 1; list <= shop_list_count; ++list)
    {
        check(!shop_stock(list, *templates).empty(), "Every list stocks something");
    }

    const auto &antidote = (*templates)[31 - 1];
    check(antidote.type == antidote_type && (antidote.flags & not_joinable) == 0,
          "postcbt.pas ANTIDOTE_TYPE: template 31 is the antidote, which bundles join");
    int armed = 0;
    for (const auto &item : *templates)
    {
        if (item.type == Item::no_type || !takes_ammunition(item, *types))
        {
            continue;
        }
        ++armed;
        const auto clip = ammunition_offer(item, *types, false);
        check(clip.cost > 0 || clip.rounds == 0, "shop.pas AmmoValues: every clip has a price");
    }
    check(armed > 0, "AMMO lists the game's missile weapons");
}

void the_hospital_fee_check_is_the_shipped_one(const std::filesystem::path &folder,
                                               GoldBoxGame game, const std::string &archive,
                                               unsigned script, std::uint16_t first_check)
{
    const auto record = opengold::decode_dax_record(file_bytes(folder / archive),
                                                    static_cast<std::uint8_t>(script));
    check(record.has_value(), archive + " holds the hospital's script");
    const auto program = opengold::por::EclProgram::decode(*record, archive, game);
    const auto fee = program.instruction(first_check - 9);
    check(fee.operands.size() == 3 && fee.operands[2].value == 0x7F79 && fee.operands[2].tag == 3,
          "The hospital writes its fee to cell 7F79h as a word");
    for (const std::uint16_t address :
         {first_check, static_cast<std::uint16_t>(first_check + 0x1A)})
    {
        const auto compare = program.instruction(address);
        check(compare.operands.size() == 2 && compare.operands[0].value == 0x7F79 &&
                  compare.operands[0].tag == 1 &&
                  script_operand_tag(game, archive, script, address, 0, compare.operands[0].tag) ==
                      3,
              "Its checks read one byte of the fee, which the correction widens to the word");
    }
}
} // namespace

int main()
{
    try
    {
        a_status_sets_whether_the_character_acts();
        the_wounded_recover_after_a_fight();
        a_medic_cures_poison_and_then_the_wounds();
        without_a_medic_poison_takes_an_antidote_or_a_coma();
        first_aid_stops_at_six_and_one_wound_skill_follows();
        treating_ones_own_critical_wounds_is_harder();
        the_report_says_why_nothing_was_done();
        an_unkillable_member_stands_with_45();
        aid_bandages_the_bleeding_and_ends_a_stun();
        rest_heals_a_point_a_day();
        the_pool_is_shared_among_those_it_takes_from();
        a_shop_stocks_its_list_last_first();
        prices_scale_by_the_multiplier();
        buying_takes_own_credits_then_the_pool();
        ammunition_comes_in_clips_and_rounds();
        bundles_halve_and_join();
        the_hospitals_fee_check_reads_the_whole_fee();
        if (const auto countdown = files_of(GoldBoxGame::countdown_to_doomsday))
        {
            the_games_shops_and_weapons(*countdown);
            the_hospital_fee_check_is_the_shipped_one(
                *countdown, GoldBoxGame::countdown_to_doomsday, "ECL6.DAX", 99, 0x84F1);
        }
        if (const auto matrix = files_of(GoldBoxGame::matrix_cubed))
        {
            the_games_shops_and_weapons(*matrix);
            the_hospital_fee_check_is_the_shipped_one(*matrix, GoldBoxGame::matrix_cubed,
                                                      "ECL1.DAX", 20, 0x853B);
        }
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Buck Rogers upkeep tests passed\n";
    return 0;
}
