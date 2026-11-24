// Matrix's actual single-disk campaign. Ending protocol inspection is a
// separate check and never supplies an earned-route completion certificate.
#include "opengold/buck_rogers_characters.h"
#include "opengold/buck_rogers_creation.h"
#include "opengold/buck_rogers_combat.h"
#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/campaign_save.h"
#include "opengold/save_file.h"
#include "opengold/campaign_session.h"
#include "opengold/character.h"
#include "opengold/combat_demo.h"
#include "opengold/buck_rogers_solar_map.h"
#include "opengold/buck_rogers_ship_fight.h"
#include "opengold/formats.h"
#include "opengold/original.h"
#include "opengold/original_content.h"
#include "buck_rogers_training_player.h"
#include "installed_games.h"
#include "test_support.h"
#include "tour_access.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <deque>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
namespace og = opengold::original;
namespace br = opengold::buck_rogers;
using opengold::GoldBoxGame;
using opengold::por::TourPhase;
using opengold::test::check;
constexpr auto matrix = GoldBoxGame::matrix_cubed;

std::vector<std::uint8_t> bytes_of(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    check(static_cast<bool>(in), "Read Matrix's original game file");
    return {std::istreambuf_iterator<char>(in), {}};
}

struct Campaign
{
    std::shared_ptr<const opengold::rules::CharacterRules> creation;
    std::shared_ptr<opengold::CampaignParty> party;
    opengold::por::RolfTourSession session;
    opengold::RandomState battle_random{20261120};
    unsigned battle_draws{}, ground_victories{}, ground_escapes{};
    std::string terminal_party_save;
    std::map<opengold::MemberId, std::string> terminal_records;
    bool ready_for_ceres_demolition{}; // The test player's plan, never a game flag.
    bool leaving_warren{};             // A normal player's destination choice.
    bool leaving_museum{};
    bool prison_rearming{};   // The player's optional barracks/reload plan, never a quest flag.
    unsigned tower_floor{85}; // The player's actual elevator destination.
    bool malcoln_visit{};
    bool leaving_catalina{}; // The player's planned return trip.

    bool university_bomb_code{}; // The player remembers the actual soldier's clue.
    bool ship_passage{};         // Accept a hatch only while deliberately operating that control.
    bool fortuna_visit{};
    unsigned genetics_floor{1};
    std::optional<std::pair<unsigned, unsigned>> genetics_ladder_destination;
    unsigned fortuna_floor{4}; // Public elevator button selected by the player.
    bool hielo_supplies{};     // An ordinary planned shopping visit, never a game flag.
    std::string lunar_phone_target{"LT. JENNER"}; // The player's intended public phone call.
};

Campaign fresh_campaign(const std::filesystem::path &folder)
{
    const auto archive = opengold::decode_dax_archive(bytes_of(folder / "ITEM0.DAX"));
    const auto first = std::ranges::find(archive.records, 1U, &opengold::DaxRecord::id);
    check(first != archive.records.end(), "Matrix has its starting item templates");
    const auto templates = br::decode_item_templates(first->bytes);
    const auto types = br::decode_item_types(bytes_of(folder / "ITEMS"));
    check(templates && types, "Matrix's item types and templates decode");
    const auto items = og::buck_rogers_starting_items(*templates);
    check(items.size() == 2, "Matrix's creation supplies its two starting items");
    const auto weapons = og::specializable_weapon_types(*templates);
    check(std::ranges::find(weapons, items.back().type) != weapons.end(),
          "The starting weapon is offered by the actual specialization menu");
    std::shared_ptr<const opengold::rules::CharacterRules> creation =
        og::character_rules(og::default_aging, matrix);
    auto party = std::make_shared<opengold::CampaignParty>(
        og::rules_module(opengold::load_original_content(folder, matrix)), creation);
    const std::array<std::tuple<og::Race, og::Profession, std::string>, 6> team{{
        {og::Race::terran, og::Profession::rocket_jock, "Jock"},
        {og::Race::desert_runner, og::Profession::warrior, "Runner"},
        {og::Race::martian, og::Profession::warrior, "Shade"},
        {og::Race::tinker, og::Profession::engineer, "Gears"},
        {og::Race::venusian, og::Profession::medic, "Doc"},
        {og::Race::mercurian, og::Profession::warrior, "Merc"},
    }};
    opengold::RandomState random{2026};
    for (const auto &[race, career, name] : team)
    {
        // A fixed player plan using the existing Reroll control. Accept
        // only the displayed ability criteria; never select or edit HP.
        auto creature =
            og::new_buck_rogers_character(race, og::Gender::male, career, matrix, random);
        creature.name = name;
        creature.icon = og::default_icon(race, og::Gender::male, career, random);
        unsigned rolls = 1;
        while (
            (creature.abilities[static_cast<unsigned>(og::BuckRogersAbility::dexterity)] < 17 ||
             creature.abilities[static_cast<unsigned>(og::BuckRogersAbility::constitution)] < 16 ||
             (name == "Runner" &&
              creature.abilities[static_cast<unsigned>(og::BuckRogersAbility::strength)] < 19)) &&
            rolls < 10000)
        {
            auto draft = og::buck_rogers_draft(og::GroundRecord{creature, {}, {}});
            og::roll_buck_rogers_draft(draft, matrix, random);
            const auto rolled = og::buck_rogers_record_of(draft.rules_data);
            check(rolled.has_value(), "The actual creator's Reroll returns its native character");
            creature = rolled->creature;
            ++rolls;
        }
        check(
            rolls < 10000,
            "Actual Reroll reaches the player's fixed displayed ability criteria, including the climber's Strength");
        std::cout << "Actual creation accepted " << name << " after " << rolls
                  << " native rolls; abilities";
        for (const auto score : creature.abilities)
        {
            std::cout << ' ' << unsigned(score);
        }
        std::cout << ".\n";
        auto skills = og::SkillSpending::at_creation(creature, matrix);
        check(skills.career_points() == 0 && skills.general_points() == 0,
              "Matrix creation supplies presets without unspent skill points");
        if (career == og::Profession::rocket_jock)
        {
            for (const auto skill : {og::Skill::drive_ground_car, og::Skill::notice})
            {
                for (unsigned point = 0; point < 30; ++point)
                {
                    check(skills.subtract(skill),
                          "Refund only the pilot's permitted career points");
                }
            }
            for (unsigned point = 0; point < 60; ++point)
            {
                check(skills.add(og::Skill::pilot_fixed_wing) == og::SkillRefusal::none,
                      "Spend the pilot's actual career pool on offered Fixed Wing piloting");
            }
            check(skills.career_points() == 0 && skills.general_points() == 0,
                  "The pilot conserves both creation point pools");
        }
        if (career == og::Profession::engineer)
        {
            // The original Matrix skill screen permits moving preset points
            // within the same pool. Keep a medic and train the ship's technician.
            for (unsigned point = 0; point < 30; ++point)
            {
                check(skills.subtract(og::Skill::first_aid),
                      "The original creation screen refunds an allowed general preset point");
            }
            for (const auto skill : {og::Skill::commo_operation, og::Skill::sensor_operation})
            {
                for (unsigned point = 0; point < 15; ++point)
                {
                    check(skills.add(skill) == og::SkillRefusal::none,
                          "The original creation screen accepts the technician's general skill");
                }
            }
            check(skills.career_points() == 0 && skills.general_points() == 0,
                  "The technician spends exactly the refunded general points");
            for (unsigned point = 0; point < 40; ++point)
            {
                check(skills.subtract(og::Skill::first_aid),
                      "Refund the technician's remaining general First Aid points");
            }
            for (unsigned point = 0; point < 35; ++point)
            {
                check(skills.subtract(og::Skill::repair_weapon),
                      "Refund permitted general Repair Weapon points");
            }
            for (const auto [skill, amount] : std::array<std::pair<og::Skill, unsigned>, 2>{
                     {{og::Skill::mathematics, 10}, {og::Skill::programming, 65}}})
            {
                for (unsigned point = 0; point < amount; ++point)
                {
                    check(skills.add(skill) == og::SkillRefusal::none,
                          "Spend only actual general points, with Mathematics before Programming");
                }
            }
            check(skills.career_points() == 0 && skills.general_points() == 0,
                  "The programmer's legal point-conserving creation spends both pools completely");
        }
        if (name == "Shade")
        {
            for (unsigned point = 0; point < 20; ++point)
            {
                check(skills.subtract(og::Skill::first_aid),
                      "Refund a permitted general preset point for the lock specialist");
                check(skills.add(og::Skill::open_lock) == og::SkillRefusal::none,
                      "Spend that actual general point on the offered Open Lock skill");
            }
            check(skills.career_points() == 0 && skills.general_points() == 0,
                  "The lock specialist spends exactly the refunded general points");
        }
        if (name == "Merc")
        {
            for (unsigned point = 0; point < 20; ++point)
            {
                check(skills.subtract(og::Skill::first_aid),
                      "Refund a permitted general preset point for the security specialist");
                check(skills.add(og::Skill::bypass_security) == og::SkillRefusal::none,
                      "Spend that actual general point on the offered Bypass Security skill");
            }
            check(skills.career_points() == 0 && skills.general_points() == 0,
                  "The security specialist spends exactly the refunded general points");
        }
        skills.apply_to(creature);
        for (int n = 0; n < og::creation_specializations(career, matrix); ++n)
        {
            check(og::specialize(creature, items.back().type),
                  "A new Matrix Warrior chooses its legal starting specializations");
        }
        const og::GroundRecord record{std::move(creature), items, *types};
        party->add_pc(opengold::Character(*creation, og::buck_rogers_draft(record),
                                          opengold::por::CharacterAppearance{}));
    }
    const auto jock = party->state().slots[0];
    const auto merc = party->state().slots[5];
    party->move_member(merc, 0);
    party->move_member(jock, 5);
    auto session = opengold::load_campaign(folder, matrix);
    session.campaign_party(party);
    return {std::move(creation), std::move(party), std::move(session)};
}

void starts_with_matrix_records(const Campaign &campaign)
{
    check(campaign.party->state().roster.size() == 6, "Six Matrix members join the new team");
    for (const auto &member : campaign.party->state().roster)
    {
        const auto record = og::buck_rogers_record_of(campaign.party->game_record(member.id));
        check(record && record->creature.level == 7 && record->items.size() == 2 &&
                  member.experience == record->creature.experience,
              "Matrix's actual level-7 creation and free equipment start the campaign");
        const auto skills = og::SkillSpending::at_creation(record->creature, matrix);
        auto expected = skills.levels();
        if (og::career_from_record(record->creature.career) == og::Profession::rocket_jock)
        {
            og::level_of(expected, og::Skill::drive_ground_car) = 10;
            og::level_of(expected, og::Skill::notice) = 10;
            og::level_of(expected, og::Skill::pilot_fixed_wing) = 70;
        }
        if (og::career_from_record(record->creature.career) == og::Profession::engineer)
        {
            og::level_of(expected, og::Skill::first_aid) = 0;
            og::level_of(expected, og::Skill::repair_weapon) = 35;
            og::level_of(expected, og::Skill::mathematics) = 10;
            og::level_of(expected, og::Skill::programming) = 65;
            og::level_of(expected, og::Skill::commo_operation) = 15;
            og::level_of(expected, og::Skill::sensor_operation) = 15;
        }
        if (record->creature.name == "Shade")
        {
            og::level_of(expected, og::Skill::first_aid) = 50;
            og::level_of(expected, og::Skill::open_lock) = 20;
        }
        if (record->creature.name == "Merc")
        {
            og::level_of(expected, og::Skill::first_aid) = 50;
            og::level_of(expected, og::Skill::bypass_security) = 20;
        }
        check(
            record->creature.skills == expected,
            "Fresh Matrix records retain their presets and accepted point-conserving customization");
    }
}

void saved_campaign_round_trips(Campaign &campaign, const std::filesystem::path &folder,
                                bool winning)
{
    const auto assets = opengold::campaign_asset_identity(folder);
    const auto before = campaign.party->checkpoint();
    const auto saved =
        opengold::encode_campaign(*campaign.party, &campaign.session, assets, matrix);
    check(saved.starts_with("OPENGOLD-CAMPAIGN 55\n"),
          "Only the newly supported Matrix campaign writes format 55");
    const auto header = opengold::saved_campaign_rules(saved);
    check(header.game == matrix, "The save names Matrix's stable game ID 11");
    auto loaded = opengold::decode_campaign(
        saved, *campaign.creation, campaign.party->rule_module(), assets, &campaign.session);
    check(loaded.town.has_value(), "Matrix's save keeps its campaign session");
    auto restored = std::make_shared<opengold::CampaignParty>(
        og::rules_module(opengold::load_original_content(folder, matrix)), campaign.creation);
    restored->restore(std::move(loaded.party));
    loaded.town->attach_restored_party(restored);
    check(loaded.town->snapshot().game_won == winning && loaded.town->can_leave() &&
              loaded.town->snapshot().tour_finished &&
              loaded.town->snapshot().pose == campaign.session.snapshot().pose,
          "A saved Matrix victory or ordinary game retains its actual state");
    check(restored->state().slots == before.slots &&
              restored->state().roster.size() == before.roster.size(),
          "Every active and reserve character survives Matrix's campaign save");
    for (const auto &member : before.roster)
    {
        check(restored->game_record(member.id) == campaign.party->game_record(member.id) &&
                  restored->member(member.id).npc_source == member.npc_source,
              "Saved Matrix characters keep their complete current records and identities");
    }
    check(opengold::encode_campaign(*restored, &*loaded.town, assets, matrix) == saved,
          "Matrix's ordinary campaign save resaves byte-identically");
    check(loaded.town->snapshot().solar_map == campaign.session.snapshot().solar_map &&
              loaded.town->solar_map_state() == campaign.session.solar_map_state(),
          "A whole Matrix save retains the actual Solar state and presentation");
    const auto pose_before_settings = loaded.town->snapshot().pose;
    loaded.town->difficulty(opengold::DifficultySetting::novice);
    check(loaded.town->script_variable(opengold::por::EclAddress{0x4BB6}) == 0 &&
              loaded.town->snapshot().pose == pose_before_settings &&
              loaded.town->snapshot().game_won == winning,
          "Applying settings after load updates the live cell without restarting the campaign");
    if (!winning)
    {
        const auto legacy = opengold::encode_campaign_in_format(*campaign.party, &campaign.session,
                                                                assets, 53, matrix);
        auto old = opengold::decode_campaign(
            legacy, *campaign.creation, campaign.party->rule_module(), assets, &campaign.session);
        check(old.town && !old.town->snapshot().game_won && old.party.slots == before.slots &&
                  old.party.roster.size() == before.roster.size(),
              "An ordinary format-53 Matrix town retains its party without inventing victory");
    }
    if (winning)
    {
        check(!loaded.town->explore(opengold::por::ExplorationCommand::forward) &&
                  !loaded.town->camp(opengold::RestKind::long_rest),
              "A loaded Matrix ending cannot start another turn or camp");
        auto corrupt = saved;
        const std::string receipt = std::to_string(0x9E0B);
        const auto marker = corrupt.rfind(' ' + receipt + ' ');
        check(marker != std::string::npos, "The terminal receipt carries the completed PROGRAM PC");
        corrupt.replace(marker + 1, receipt.size(), std::to_string(0x9E08));
        const auto body_start = corrupt.find('\n', corrupt.find('\n') + 1) + 1;
        const auto body = corrupt.substr(body_start);
        std::uint64_t hash = 14695981039346656037ULL;
        for (unsigned char c : body)
        {
            hash ^= c;
            hash *= 1099511628211ULL;
        }
        corrupt = corrupt.substr(0, corrupt.find('\n') + 1) + std::to_string(hash) + '\n' + body;
        bool refused = false;
        try
        {
            (void)opengold::decode_campaign(corrupt, *campaign.creation,
                                            campaign.party->rule_module(), assets,
                                            &campaign.session);
        }
        catch (const std::runtime_error &error)
        {
            check(std::string_view(error.what()) == "Invalid Matrix terminal victory receipt",
                  "The receipt validator rejects the changed PC: " + std::string(error.what()));
            refused = true;
        }
        check(refused, "A checksum-valid Matrix receipt at the unexecuted PROGRAM is rejected");
    }
}

void native_difficulty_keeps_all_six_settings()
{
    for (const auto game : {GoldBoxGame::countdown_to_doomsday, matrix})
    {
        const auto folder = opengold::test::installed_game_folder(game);
        check(!folder.empty(), "Both installed Buck games are available for their option contract");
        auto session = opengold::load_campaign(folder, game);
        check(opengold::test::TourAccess::machine(session).variable(
                  opengold::por::EclAddress{0x4BB6}) == 2,
              "The existing default remains native Trooper");
        for (unsigned number = 0; number < 6; ++number)
        {
            session.difficulty(static_cast<opengold::DifficultySetting>(number));
            check(opengold::test::TourAccess::machine(session).variable(
                      opengold::por::EclAddress{0x4BB6}) == number,
                  "A live settings update changes the script cell without restarting");
            session.restart();
            check(opengold::test::TourAccess::machine(session).variable(
                      opengold::por::EclAddress{0x4BB6}) == number,
                  "The full existing setting reaches the actual game's difficulty cell");
        }
        session.difficulty(opengold::Difficulty::easy);
        session.restart();
        check(opengold::test::TourAccess::machine(session).variable(
                  opengold::por::EclAddress{0x4BB6}) == 1,
              "The supported Difficulty API still means native Easy");
        session.difficulty(opengold::DifficultySetting::novice);
        auto detached = session.detached_copy();
        detached.restart();
        check(opengold::test::TourAccess::machine(detached).variable(
                  opengold::por::EclAddress{0x4BB6}) == 0,
              "A detached town template retains the selected Novice runtime option");
    }
}

void matrix_format_preserves_released_history(const std::filesystem::path &folder)
{
    auto campaign = fresh_campaign(folder);
    const auto assets = opengold::campaign_asset_identity(folder);
    const auto legacy =
        opengold::encode_campaign_in_format(*campaign.party, nullptr, assets, 53, matrix);
    auto loaded = opengold::decode_campaign(legacy, *campaign.creation,
                                            campaign.party->rule_module(), assets, nullptr);
    check(loaded.game == matrix && !loaded.town &&
              loaded.party.slots == campaign.party->state().slots,
          "A released format-53 Matrix party-only save still loads");
    for (const auto &[game, version] : {std::pair{GoldBoxGame::countdown_to_doomsday, 53U},
                                        std::pair{GoldBoxGame::dark_queen_of_krynn, 54U}})
    {
        const auto bytes = opengold::encode_campaign(*campaign.party, nullptr, assets, game);
        check(bytes.starts_with("OPENGOLD-CAMPAIGN " + std::to_string(version) + '\n'),
              "Countdown and Native keep their existing format-53 and format-54 writers");
        bool refused = false;
        try
        {
            (void)opengold::encode_campaign_in_format(*campaign.party, nullptr, assets, 55, game);
        }
        catch (const std::runtime_error &)
        {
            refused = true;
        }
        check(refused, "A non-Matrix game cannot write Matrix's format-55 layout");
    }
    bool refused = false;
    try
    {
        (void)opengold::encode_campaign_in_format(*campaign.party, nullptr, assets, 54, matrix);
    }
    catch (const std::runtime_error &)
    {
        refused = true;
    }
    check(refused, "Matrix cannot masquerade as Native's format-54 layout");
}

// Choose a carried gun using the native rules' attack counts, damage and
// hit calculation. The copied item list is a preview; only offered combat
// ready/unready commands change the actual inventory.
std::optional<unsigned> best_route_gun(const og::GroundRecord &record,
                                       const opengold::rules::Snapshot &state,
                                       const opengold::rules::CombatantView &actor,
                                       int party_to_hit, bool assassins_first = false)
{
    std::optional<unsigned> best;
    long best_score = -1;
    const opengold::rules::CombatantView *focus = nullptr;
    int focus_priority = 1000000;
    const bool rapid_guns =
        std::ranges::any_of(state.combatants,
                            [](const auto &foe)
                            {
                                return foe.side != opengold::rules::Side::party &&
                                       foe.name.starts_with("ASSAULT ROBOT");
                            });
    for (const auto &enemy : state.combatants)
    {
        if (enemy.side == actor.side || enemy.dead || enemy.fled || enemy.hit_points <= 0)
        {
            continue;
        }
        const int priority =
            enemy.hit_points -
            (assassins_first && enemy.name.starts_with("RAM ASSASSIN") ? 4500 : 0) -
            (enemy.name.starts_with("RAM CBT. GENNIE") ? (rapid_guns ? 3500 : 2000) : 0) -
            (enemy.name.starts_with("ASSAULT ROBOT") ? 3000 : 0) -
            (enemy.name.starts_with("SID REFUGE") ? 4000 : 0);
        if (priority < focus_priority)
        {
            focus_priority = priority;
            focus = &enemy;
        }
    }
    for (unsigned n = 0; n < record.items.size(); ++n)
    {
        const auto &item = record.items[n];
        const auto *type = og::type_of(item, record.types);
        if (!type || type->slot != og::gear_slot::weapon || !item.quantity ||
            type->weapon_class < 1 || type->weapon_class > 5)
        {
            continue;
        }
        auto preview = record.items;
        for (auto &held : preview)
        {
            const auto *held_type = og::type_of(held, record.types);
            if (held_type && held_type->slot == og::gear_slot::weapon)
            {
                held.flags &= static_cast<std::uint8_t>(~br::Item::readied);
            }
        }
        preview[n].flags |= br::Item::readied;
        const auto numbers = og::ground_numbers(record.creature, preview, record.types, false);
        const auto damage = numbers.damage[0];
        const int shots = og::ground_attacks(record.creature, preview, record.types, 0, false)[0] +
                          og::ground_attacks(record.creature, preview, record.types, 1, false)[0];
        const long volley =
            static_cast<long>(shots) * (damage.count * (damage.sides + 1) + 2 * damage.bonus);
        // Retain a usable gun while closing on foes outside its range.
        long score = volley;
        for (const auto &enemy : state.combatants)
        {
            if (&enemy != focus)
            {
                continue;
            }
            const int distance = std::max(std::abs(actor.cell.x - enemy.cell.x),
                                          std::abs(actor.cell.y - enemy.cell.y));
            if (distance > og::readied_weapon_range(preview, record.types) ||
                !opengold::rules::has_line_of_sight(state.battlefield, actor.cell, enemy.cell))
            {
                continue;
            }
            int hits = 0;
            for (int die = 1; die <= 20; ++die)
            {
                hits += og::ground_attack_hits(die, numbers.attack_bonus, party_to_hit,
                                               enemy.armor_class +
                                                   og::range_penalty(distance, type->range));
            }
            auto effectiveness = static_cast<long>(hits) * shots *
                                 (damage.count * (damage.sides + 1) + 2 * damage.bonus);
            const auto defense = og::buck_rogers_record_of(enemy.persistent.resources);
            if (type->weapon_class == static_cast<unsigned>(og::WeaponClass::rocket) && defense &&
                (defense->creature.armor_flags & (og::armor_bit::ecm | og::armor_bit::fields)))
            {
                // Prefer lasers over rockets against visible countermeasures.
                effectiveness /= 4;
            }
            score = std::max(score, effectiveness);
        }
        // The actual combat log exposes the robots throwing their chaff.
        // After observing that, use a carried laser/slug gun instead of
        // repeatedly firing rockets through the same defensive cloud.
        if (rapid_guns && type->weapon_class == static_cast<unsigned>(og::WeaponClass::rocket) &&
            std::ranges::any_of(state.log(),
                                [](const auto &line)
                                {
                                    return line.starts_with("ASSAULT ROBOT") &&
                                           line.find("throws a grenade") != std::string::npos;
                                }))
        {
            score /= 100;
        }
        if (score > best_score)
        {
            best_score = score;
            best = n;
        }
    }
    return best;
}

std::optional<opengold::rules::Command>
ready_route_equipment(const opengold::rules::Snapshot &state,
                      const std::vector<opengold::rules::Command> &offered, bool grenades,
                      unsigned grenade_reserve, bool area_weapons, bool crowd_control,
                      int party_to_hit, bool assassins_first = false)
{
    const auto actor =
        std::ranges::find(state.combatants, state.actor, &opengold::rules::CombatantView::id);
    check(actor != state.combatants.end(), "The actual player actor exists");
    const auto record = og::buck_rogers_record_of(actor->persistent.resources);
    check(record.has_value(), "The actual actor exposes its current native inventory");
    const auto find = [&](unsigned type, unsigned more_than = 0,
                          bool rounds = false) -> std::optional<unsigned>
    {
        unsigned carried_rounds = 0;
        for (const auto &item : record->items)
        {
            if (item.type == type)
            {
                carried_rounds += item.quantity;
            }
        }
        if (rounds && carried_rounds <= more_than)
        {
            return std::nullopt;
        }
        for (unsigned n = 0; n < record->items.size(); ++n)
        {
            if (record->items[n].type == type && (!rounds || record->items[n].quantity > 0))
            {
                return n;
            }
        }
        return std::nullopt;
    };
    auto armor = find(24);
    if (!armor)
    {
        armor = find(23);
    }
    if (!armor)
    {
        armor = find(22);
    }
    if (!armor)
    {
        armor = find(21);
    }
    if (!armor)
    {
        armor = find(20);
    }
    auto weapon = best_route_gun(*record, state, *actor, party_to_hit, assassins_first);
    if (grenades)
    {
        if (const auto grenade = find(33, grenade_reserve, true))
        {
            weapon = grenade;
        }
        if (crowd_control)
        {
            if (const auto stun = find(35, 0, true))
            {
                weapon = stun;
            }
            if (const auto dazzle = find(36, 0, true))
            {
                weapon = dazzle;
            }
        }
        if (area_weapons)
        {
            const auto rocket = find(18, 0, true);
            const auto plasma = find(17, 0, true);
            if (rocket)
            {
                weapon = rocket;
            }
            if (plasma &&
                (!rocket || std::ranges::any_of(state.combatants,
                                                [](const auto &unit)
                                                {
                                                    return unit.side !=
                                                               opengold::rules::Side::party &&
                                                           !unit.dead && !unit.fled &&
                                                           unit.hit_points > 0 &&
                                                           unit.name.starts_with("ASSAULT ROBOT");
                                                })))
            {
                weapon = plasma;
            }
        }
    }
    for (const auto wanted : {armor, weapon})
    {
        if (!wanted)
        {
            continue;
        }
        const auto &desired = record->items[*wanted];
        if (desired.flags & br::Item::readied)
        {
            continue;
        }
        check(desired.type < record->types.size(), "Owned loot retains its actual item type");
        const auto slot = record->types[desired.type].slot;
        std::string verb = "ready";
        unsigned at = *wanted;
        for (unsigned n = 0; n < record->items.size(); ++n)
        {
            if ((record->items[n].flags & br::Item::readied) &&
                record->types.at(record->items[n].type).slot == slot)
            {
                verb = "unready";
                at = n;
                break;
            }
        }
        const auto order =
            std::ranges::find_if(offered,
                                 [&](const auto &candidate)
                                 {
                                     return candidate.verb == verb && candidate.item == at;
                                 });
        check(order != offered.end(), "Loot is equipped using an actual offered command");
        return *order;
    }
    return std::nullopt;
}

std::optional<opengold::rules::Command>
grenade_route_order(const opengold::rules::Snapshot &state,
                    const std::vector<opengold::rules::Command> &offered, int projectile = 6,
                    int maximum_damage = 20)
{
    std::optional<opengold::rules::Command> best;
    int score = 0;
    const auto radius = og::projectile(projectile).radius;
    for (const auto &order : offered)
    {
        if (order.verb != "launch")
        {
            continue;
        }
        int hits = 0;
        bool safe = true;
        for (const auto &unit : state.combatants)
        {
            if (unit.dead || unit.fled || !unit.conscious)
            {
                continue; // Native ground_launch reaches only able combatants.
            }
            const int dx = std::abs(unit.cell.x - order.destination.x),
                      dy = std::abs(unit.cell.y - order.destination.y);
            if (!og::within_blast(2 * std::max(dx, dy) + std::min(dx, dy), radius))
            {
                continue;
            }
            if (unit.side == opengold::rules::Side::party)
            {
                if (unit.hit_points <= 0 || projectile == 5 || projectile == 12 ||
                    (!opengold::is_ally(unit.id) && unit.hit_points <= maximum_damage))
                {
                    safe = false;
                    break;
                }
                hits -= 2 * std::min(unit.hit_points, 22);
            }
            else if (unit.hit_points > 0)
            {
                if (projectile == 12 && unit.status == "helpless")
                {
                    continue;
                }
                const auto record = og::buck_rogers_record_of(unit.persistent.resources);
                if (projectile == 5 && record &&
                    (record->creature.armor_flags & og::armor_bit::goggles))
                {
                    continue;
                }
                hits += std::min(unit.hit_points, 22);
            }
        }
        if (safe && hits > score)
        {
            score = hits;
            best = order;
        }
    }
    return best;
}

// A healthy front-line warrior can move into throwing range before
// choosing a real offered launch. This preview does not move the game actor.
std::optional<opengold::rules::Command>
grenade_firing_move(const opengold::rules::Snapshot &state,
                    const std::vector<opengold::rules::Command> &offered, int projectile)
{
    const auto actor =
        std::ranges::find(state.combatants, state.actor, &opengold::rules::CombatantView::id);
    check(actor != state.combatants.end(), "The grenade thrower is present");
    std::optional<opengold::rules::Command> best;
    int best_hits = 0, least_penalty = 1000000, least_travel = 1000000;
    for (const auto &move : offered)
    {
        if (move.verb != "move")
        {
            continue;
        }
        std::vector<opengold::rules::Command> targets;
        for (const auto &enemy : state.combatants)
        {
            if (enemy.side == actor->side || enemy.hit_points <= 0 || enemy.dead || enemy.fled)
            {
                continue;
            }
            const int dx = std::abs(move.destination.x - enemy.cell.x),
                      dy = std::abs(move.destination.y - enemy.cell.y);
            if ((2 * std::max(dx, dy) + std::min(dx, dy)) / 2 > 3 ||
                !opengold::rules::has_line_of_sight(state.battlefield, move.destination,
                                                    enemy.cell))
            {
                continue;
            }
            auto target = move;
            target.verb = "launch";
            target.destination = enemy.cell;
            targets.push_back(target);
        }
        auto predicted = state;
        const auto mover = std::ranges::find(predicted.combatants, state.actor,
                                             &opengold::rules::CombatantView::id);
        mover->cell = move.destination;
        std::erase_if(targets,
                      [&](const auto &target)
                      {
                          return std::ranges::any_of(
                              predicted.combatants,
                              [&](const auto &unit)
                              {
                                  const int dx = std::abs(target.destination.x - unit.cell.x),
                                            dy = std::abs(target.destination.y - unit.cell.y);
                                  return unit.side == actor->side && !unit.dead && !unit.fled &&
                                         og::within_blast(2 * std::max(dx, dy) + std::min(dx, dy),
                                                          og::projectile(projectile).radius);
                              });
                      });
        const auto shot = grenade_route_order(predicted, targets, projectile);
        if (!shot)
        {
            continue;
        }
        int hits = 0;
        for (const auto &enemy : state.combatants)
        {
            const int dx = std::abs(shot->destination.x - enemy.cell.x),
                      dy = std::abs(shot->destination.y - enemy.cell.y);
            if (enemy.side != actor->side && enemy.hit_points > 0 && !enemy.dead && !enemy.fled &&
                og::within_blast(2 * std::max(dx, dy) + std::min(dx, dy),
                                 og::projectile(projectile).radius))
            {
                ++hits;
            }
        }
        const int travel = std::abs(move.destination.x - actor->cell.x) +
                           std::abs(move.destination.y - actor->cell.y);
        const int dx = std::abs(shot->destination.x - move.destination.x),
                  dy = std::abs(shot->destination.y - move.destination.y);
        const int penalty = og::range_penalty((2 * std::max(dx, dy) + std::min(dx, dy)) / 2, 3);
        if (hits > best_hits ||
            (hits == best_hits &&
             (penalty < least_penalty || (penalty == least_penalty && travel < least_travel))))
        {
            best_hits = hits;
            least_penalty = penalty;
            least_travel = travel;
            best = move;
        }
    }
    return best;
}

struct AmmunitionNeed
{
    unsigned slot{}, item{};
    bool fill{};
};

std::optional<AmmunitionNeed> ammunition_need(const Campaign &campaign)
{
    for (unsigned slot = 0; slot < campaign.party->state().slots.size(); ++slot)
    {
        const auto id = campaign.party->state().slots[slot];
        if (!id)
        {
            continue;
        }
        const auto record = og::buck_rogers_record_of(campaign.party->game_record(id));
        if (!record)
        {
            continue;
        }
        const auto preference =
            og::career_from_record(record->creature.career) == og::Profession::warrior
                ? std::array{18U, 7U, 11U, 13U, 9U}
                : std::array{18U, 11U, 9U, 13U, 7U};
        for (const unsigned type : preference)
        {
            const auto item = std::ranges::find(record->items, type, &br::Item::type);
            if (item == record->items.end())
            {
                continue;
            }
            if (item->quantity < (type == 18 ? 10 : 100))
            {
                const auto fill = og::ammunition_offer(*item, record->types, true);
                const auto clip = og::ammunition_offer(*item, record->types, false);
                if (fill.cost <= record->creature.credits || clip.cost <= record->creature.credits)
                {
                    return AmmunitionNeed{slot, static_cast<unsigned>(item - record->items.begin()),
                                          fill.cost <= record->creature.credits};
                }
            }
            if (type != 18)
            {
                break; // Fill the real launcher, then the preferred regular gun.
            }
        }
    }
    return std::nullopt;
}

bool plasma_supplies_ready(const Campaign &campaign)
{
    unsigned equipped = 0;
    for (const auto id : campaign.party->state().slots)
    {
        if (!id)
        {
            continue;
        }
        const auto record = og::buck_rogers_record_of(campaign.party->game_record(id));
        if (record && (record->creature.name == "Shade" || record->creature.name == "Gears") &&
            std::ranges::any_of(record->items,
                                [](const auto &item)
                                {
                                    return item.type == 17;
                                }))
        {
            ++equipped;
        }
    }
    return equipped == 2;
}

bool training_available(const Campaign &campaign)
{
    return std::ranges::any_of(
        campaign.party->state().slots,
        [&](opengold::MemberId id)
        {
            if (!id)
            {
                return false;
            }
            const auto record = og::buck_rogers_record_of(campaign.party->game_record(id));
            return record &&
                   og::training_refusal(record->creature, matrix) == og::TrainingRefusal::none;
        });
}

bool settle_route(Campaign &campaign, const std::filesystem::path &folder, unsigned &fights,
                  bool stop_solar_menu = false, bool stop_world_port = false)
{
    std::map<std::string, unsigned> menus;
    bool divided = false;
    bool clinic = false;
    std::string patient;
    std::set<std::string> healed;
    opengold::MemberId ammo_buyer{};
    unsigned ammo_item{}, ammo_type{}, ammo_before{};
    std::uint32_t ammo_credits{};
    og::AmmunitionOffer ammo_offer;
    bool ammo_fill{}, ammo_sent{};
    unsigned purchase_type = 0;
    opengold::MemberId purchase_buyer{};
    unsigned purchase_credits = 0, purchase_cost = 0;
    bool purchase_sent = false;
    opengold::MemberId sale_buyer{};
    unsigned sale_type{}, sale_credits{}, sale_price{};
    bool sale_sent{};
    unsigned loot_type = 0, loot_before = 0;
    opengold::MemberId loot_buyer{};
    std::set<std::pair<opengold::MemberId, unsigned>> blocked_loot;
    const auto wounded = [&]() -> std::optional<std::string>
    {
        for (const auto id : campaign.party->state().slots)
        {
            if (!id)
            {
                continue;
            }
            const auto record = og::buck_rogers_record_of(campaign.party->game_record(id));
            if (record && !healed.contains(record->creature.name) && record->creature.status != 2 &&
                record->creature.status != 3 &&
                (campaign.session.snapshot().script_id != 19 || record->creature.status == 8) &&
                record->creature.hit_points < record->creature.hit_point_maximum)
            {
                return record->creature.name;
            }
        }
        return std::nullopt;
    };
    for (unsigned tick = 0; tick < 4000; ++tick)
    {
        campaign.session.advance(100);
        const auto &s = campaign.session.snapshot();
        std::string diagnostic = s.diagnostic;
        for (const auto &message : campaign.session.script_diagnostics())
        {
            diagnostic += "\n" + message;
        }
        check(s.phase != TourPhase::faulted && campaign.session.script_diagnostics().empty(),
              "Actual Matrix route event failed in script " + std::to_string(s.script_id) + " at " +
                  std::to_string(s.pose.x) + ',' + std::to_string(s.pose.y) + ": " + diagnostic);
        if (s.script_id == 112 && s.dialogue.find("CODE IS: A76T2") != std::string::npos)
        {
            campaign.university_bomb_code = true;
        }
        if (s.phase == TourPhase::completed)
        {
            return true;
        }
        if (s.phase == TourPhase::running)
        {
            continue;
        }
        if (s.phase == TourPhase::awaiting_input && s.script_id == 38 && s.number_input)
        {
            std::cout << "Actual Tower elevator floor request " << campaign.tower_floor << ".\n";
            check(campaign.session.input(s.continue_ticket, std::to_string(campaign.tower_floor)),
                  "Choose the actual Tower elevator's offered destination");
            continue;
        }
        if (s.phase == TourPhase::awaiting_input && s.script_id == 112 && !s.number_input)
        {
            check(campaign.university_bomb_code,
                  "The real bomb squad supplied the disarm code first");
            check(campaign.session.input(s.continue_ticket, "A76T2"),
                  "Enter the actual soldier's disarm code");
            continue;
        }
        if (s.phase == TourPhase::awaiting_input && s.script_id == 82 && !s.number_input)
        {
            const bool countdown_password =
                campaign.session.script_variable(opengold::por::EclAddress{0x4CA7}) & 0x10;
            const auto answer = s.dialogue.find("TELL US THE WORD") != std::string::npos ? "ELROY"
                                : countdown_password                                     ? "DANATAT"
                                                                                         : "WILBUR";
            std::cout << "Actual Fortuna typed answer " << answer << ".\n";
            check(campaign.session.input(s.continue_ticket, answer),
                  "Type the password learned through the authored Fortuna investigation");
            continue;
        }
        if (s.phase == TourPhase::awaiting_continue)
        {
            if (s.script_id == 114 && campaign.terminal_party_save.empty() &&
                campaign.session.script_variable(opengold::por::EclAddress{0x4C0F}) == 8 &&
                opengold::test::TourAccess::machine(campaign.session).address() == 0x9E08)
            {
                for (const auto &member : campaign.party->state().roster)
                {
                    campaign.terminal_records.emplace(member.id,
                                                      campaign.party->game_record(member.id));
                }
                campaign.terminal_party_save = opengold::encode_campaign(
                    *campaign.party, nullptr, opengold::campaign_asset_identity(folder), matrix);
                if (const auto *directory = std::getenv("OPENGOLD_MATRIX_PROOF_DIR"))
                {
                    opengold::write_save_file(std::filesystem::path(directory) /
                                                  "matrix-at-win-party.ogs",
                                              campaign.terminal_party_save, 16U * 1024U * 1024U);
                }
                std::cout << "Actual terminal Continue captured "
                          << campaign.terminal_records.size()
                          << " complete character records before PROGRAM2.\n";
            }
            if (stop_world_port && s.script_id == 32 &&
                s.dialogue.find("DO YOU LEAVE EARTH?") != std::string::npos &&
                std::ranges::find(s.choices, "YES") != s.choices.end())
            {
                return true;
            }
            if (stop_solar_menu && s.choices.size() > 1)
            {
                return true;
            }
            std::size_t pick = 0;
            std::string key = s.dialogue;
            for (const auto &choice : s.choices)
            {
                key += '\n' + choice;
            }
            const auto repeat = ++menus[key];
            if (s.dialogue.find("DO YOU NEED HEALING") != std::string::npos ||
                s.dialogue.find("USE THE CLINIC?") != std::string::npos)
            {
                clinic = true;
            }
            if (clinic && !patient.empty() &&
                (s.dialogue.find(" IS HEALED...") != std::string::npos ||
                 s.dialogue.find(" IS PATCHED UP...") != std::string::npos))
            {
                healed.insert(patient);
                patient.clear();
            }
            std::string wanted;
            const auto train = std::ranges::find_if(s.choices,
                                                    [](const auto &choice)
                                                    {
                                                        return choice.starts_with("Train ");
                                                    });
            if (train != s.choices.end())
            {
                wanted = *train;
            }
            else if (std::ranges::find(s.choices, "Buy") != s.choices.end() &&
                     std::ranges::find(s.choices, "Ammo") != s.choices.end())
            {
                wanted = "Exit";
                for (unsigned slot = 0; slot < campaign.party->state().slots.size(); ++slot)
                {
                    const auto id = campaign.party->state().slots[slot];
                    if (!id)
                    {
                        continue;
                    }
                    const auto record = og::buck_rogers_record_of(campaign.party->game_record(id));
                    check(record.has_value(), "An actual shop buyer has its native record");
                    const auto owns = [&](unsigned type)
                    {
                        return std::ranges::any_of(record->items,
                                                   [type](const auto &item)
                                                   {
                                                       return item.type == type;
                                                   });
                    };
                    if (s.script_id == 18 || (s.script_id == 20 && campaign.hielo_supplies))
                    {
                        const auto obsolete = std::ranges::find_if(
                            record->items,
                            [&](const auto &item)
                            {
                                return !(item.flags & br::Item::readied) &&
                                       ((item.type == 20 && (owns(21) || owns(22))) ||
                                        (item.type == 21 &&
                                         (owns(22) ||
                                          (s.script_id == 20 && (owns(23) || owns(24))))) ||
                                        (s.script_id == 20 && item.type == 22 &&
                                         (owns(23) || owns(24))));
                            });
                        if (obsolete != record->items.end())
                        {
                            check(campaign.session.can_select_member(),
                                  "The actual seller can be selected");
                            campaign.party->select(opengold::PartySlot{slot});
                            sale_buyer = id;
                            sale_type = obsolete->type;
                            sale_price = og::sale_price(*obsolete);
                            sale_credits = record->creature.credits;
                            sale_sent = false;
                            wanted = "Sell";
                            break;
                        }
                    }
                    const auto type =
                        s.script_id == 20 && campaign.hielo_supplies &&
                                (record->creature.name == "Shade" ||
                                 record->creature.name == "Gears") &&
                                !owns(17)
                            ? 17U
                        : s.script_id == 64 && record->creature.name == "Doc" && !owns(16) ? 16U
                        : !owns(21) && !owns(22) && !owns(23) && !owns(24)                 ? 21U
                        : og::career_from_record(record->creature.career) !=
                                    og::Profession::warrior &&
                                !owns(9)
                            ? 9U
                            : 0U;
                    if (!type)
                    {
                        continue;
                    }
                    check(campaign.session.can_select_member(),
                          "The actual shop permits buyer selection");
                    campaign.party->select(opengold::PartySlot{slot});
                    const auto archive =
                        opengold::decode_dax_archive(bytes_of(folder / "ITEM0.DAX"));
                    const auto first =
                        std::ranges::find(archive.records, 1U, &opengold::DaxRecord::id);
                    check(first != archive.records.end(),
                          "The actual shop retains ITEM0 templates");
                    const auto templates = br::decode_item_templates(first->bytes);
                    check(templates.has_value(), "The actual shop templates decode");
                    const auto stock =
                        og::shop_stock(s.script_id == 20 && campaign.hielo_supplies ? 16
                                       : s.script_id == 64                          ? 2
                                                                                    : 1,
                                       *templates);
                    const auto item = std::ranges::find(stock, type, &br::Item::type);
                    check(item != stock.end(), "The actual Depot stocks the selected weapon");
                    purchase_cost = og::shop_price(
                        *item, campaign.session.script_variable(opengold::por::EclAddress{0x7F6D}));
                    if (s.script_id == 20 && campaign.hielo_supplies &&
                        record->creature.credits < purchase_cost)
                    {
                        wanted = std::ranges::find(s.choices, "Divvy") != s.choices.end() ? "Divvy"
                                                                                          : "Pool";
                        break; // Share only the money earned by the actual team.
                    }
                    check(record->creature.credits >= purchase_cost,
                          "The real buyer can afford this purchase without imaginary funds");
                    purchase_type = type;
                    purchase_buyer = id;
                    purchase_credits = record->creature.credits;
                    purchase_sent = false;
                    wanted = "Buy";
                    break;
                }
                if (wanted == "Exit")
                {
                    if (const auto need = ammunition_need(campaign))
                    {
                        check(campaign.session.can_select_member(),
                              "The ordinary Depot permits its actual ammo buyer");
                        campaign.party->select(opengold::PartySlot{need->slot});
                        ammo_buyer = campaign.party->state().slots[need->slot];
                        const auto record =
                            og::buck_rogers_record_of(campaign.party->game_record(ammo_buyer));
                        check(record.has_value(),
                              "The actual ammunition buyer keeps its native gear");
                        ammo_item = need->item;
                        ammo_type = record->items[ammo_item].type;
                        ammo_before = record->items[ammo_item].quantity;
                        ammo_credits = record->creature.credits;
                        ammo_fill = need->fill;
                        ammo_offer = og::ammunition_offer(record->items[ammo_item], record->types,
                                                          ammo_fill);
                        ammo_sent = false;
                        wanted = "Ammo";
                    }
                }
            }
            else if (std::ranges::find(s.choices, "Back") != s.choices.end() && sale_buyer)
            {
                if (sale_sent)
                {
                    const auto record =
                        og::buck_rogers_record_of(campaign.party->game_record(sale_buyer));
                    check(record && record->creature.credits == sale_credits + sale_price &&
                              std::ranges::none_of(record->items,
                                                   [&](const auto &item)
                                                   {
                                                       return item.type == sale_type;
                                                   }),
                          "The actual Depot removes obsolete armor and pays its native sale price");
                    std::cout << "Actual Depot sale " << record->creature.name << " armor "
                              << sale_type << " for " << sale_price << " credits.\n";
                    wanted = "Back";
                    sale_buyer = 0;
                }
                else
                {
                    const std::string label = sale_type == 20   ? "Spacesuit"
                                              : sale_type == 22 ? "Heavy Body Armor"
                                                                : "Smartsuit";
                    const auto choice =
                        std::ranges::find_if(s.choices,
                                             [&](const auto &text)
                                             {
                                                 return text.starts_with(label + ':');
                                             });
                    check(choice != s.choices.end(),
                          "The actual Sell menu offers this obsolete suit");
                    wanted = *choice;
                    sale_sent = true;
                }
            }
            else if (std::ranges::find(s.choices, "Back") != s.choices.end() && ammo_buyer)
            {
                if (ammo_sent)
                {
                    const auto record =
                        og::buck_rogers_record_of(campaign.party->game_record(ammo_buyer));
                    check(
                        record &&
                            record->items[ammo_item].quantity == ammo_before + ammo_offer.rounds &&
                            record->creature.credits + ammo_offer.cost == ammo_credits,
                        "The offered ammo transaction charges its actual buyer and supplies the actual rounds");
                    std::cout << "Actual Depot ammo " << record->creature.name << " type "
                              << ammo_type << ": " << ammo_before << " -> "
                              << unsigned(record->items[ammo_item].quantity) << ", cost "
                              << ammo_offer.cost << ".\n";
                    wanted = "Back";
                    ammo_buyer = 0;
                }
                else
                {
                    const std::string label = ammo_type == 11   ? "Rocket Rifle"
                                              : ammo_type == 7  ? "Bolt Gun"
                                              : ammo_type == 13 ? "Laser Rifle"
                                              : ammo_type == 10 ? "Rocket Pistol"
                                              : ammo_type == 18 ? "Rocket Lnchr."
                                                                : "Laser Pistol";
                    std::vector<std::string> offers;
                    for (const auto &choice : s.choices)
                    {
                        if (choice.find(label) != std::string::npos)
                        {
                            offers.push_back(choice);
                        }
                    }
                    if (offers.size() != 2)
                    {
                        std::cout << "Actual requested ammo type " << ammo_type << ", item "
                                  << ammo_item << ".\n";
                        for (const auto &choice : s.choices)
                        {
                            std::cout << "Actual ammo offer: " << choice << "\n";
                        }
                    }
                    check(offers.size() == 2,
                          "The actual Depot offers both a clip and a fill for this carried gun");
                    wanted = offers[ammo_fill ? 1 : 0];
                    ammo_sent = true;
                }
            }
            else if (std::ranges::find(s.choices, "Back") != s.choices.end() && purchase_type)
            {
                if (purchase_sent)
                {
                    const auto record =
                        og::buck_rogers_record_of(campaign.party->game_record(purchase_buyer));
                    const auto cost = purchase_cost;
                    check(record &&
                              std::ranges::any_of(record->items,
                                                  [&](const auto &item)
                                                  {
                                                      return item.type == purchase_type;
                                                  }) &&
                              record->creature.credits + cost == purchase_credits,
                          "The actual Caloris shop delivers the item and charges its real buyer");
                    const auto known = br::decode_item_types(bytes_of(folder / "ITEMS"));
                    check(known && record->types.size() > purchase_type &&
                              known->size() > purchase_type,
                          "A real purchase retains the newly owned item's type row");
                    const auto fields = [](const auto &type)
                    {
                        return std::tie(type.slot, type.attacks_doubled, type.weapon_class,
                                        type.heavy, type.dice_count, type.dice_sides,
                                        type.damage_bonus, type.range, type.consumable);
                    };
                    check(fields(record->types.at(purchase_type)) ==
                              fields(known->at(purchase_type)),
                          "A purchased item keeps every field of the actual game's type table");
                    std::cout << "Actual shop purchase " << record->creature.name << " type "
                              << purchase_type << " for " << cost << " credits.\n";
                    wanted = "Back";
                    purchase_type = 0;
                }
                else
                {
                    const std::string prefix = purchase_type == 21   ? "Smartsuit:"
                                               : purchase_type == 16 ? "Grenade Lnchr.:"
                                               : purchase_type == 17 ? "Plasma Thrwr:"
                                                                     : "Laser Pistol:";
                    const auto choice = std::ranges::find_if(s.choices,
                                                             [&](const auto &text)
                                                             {
                                                                 return text.starts_with(prefix);
                                                             });
                    check(choice != s.choices.end(),
                          "The actual Caloris stock offers the chosen gear");
                    wanted = *choice;
                    purchase_sent = true;
                }
            }
            else if (s.script_id == 65 && s.choosing_member &&
                     s.dialogue.find("WHO WILL TRY?") != std::string::npos)
            {
                int best = -1;
                for (const auto id : campaign.party->state().slots)
                {
                    if (!id)
                    {
                        continue;
                    }
                    const auto record = og::buck_rogers_record_of(campaign.party->game_record(id));
                    if (!record ||
                        std::ranges::find(s.choices, record->creature.name) == s.choices.end())
                    {
                        continue;
                    }
                    const auto value = og::skill_value(record->creature, og::Skill::demolitions);
                    if (value > best)
                    {
                        best = value;
                        wanted = record->creature.name;
                    }
                }
            }
            else if (s.script_id == 80 && s.choosing_member &&
                     s.dialogue.find("WHO WILL DO IT?") != std::string::npos &&
                     std::ranges::find(s.choices, "Runner") != s.choices.end())
            {
                wanted = campaign.session.script_variable(opengold::por::EclAddress{0x4C1B}) == 0xFF
                             ? "Gears"
                             : "Runner";
            }
            else if (s.script_id == 36 && s.choosing_member &&
                     s.dialogue.find("WHO WILL TREAT HER?") != std::string::npos &&
                     std::ranges::find(s.choices, "Doc") != s.choices.end())
            {
                wanted = "Doc";
            }
            else if (clinic && s.choosing_member && wounded() &&
                     std::ranges::find(s.choices, *wounded()) != s.choices.end())
            {
                wanted = *wounded();
                patient = wanted;
            }
            else if (s.dialogue.find("DO YOU NEED HEALING") != std::string::npos ||
                     s.dialogue.find("SOMEONE ELSE?") != std::string::npos ||
                     s.dialogue.find("USE THE CLINIC?") != std::string::npos ||
                     s.dialogue.find("FIX ANOTHER TEAM MEMBER?") != std::string::npos)
            {
                wanted = wounded() ? "YES" : "NO";
            }
            else if (s.dialogue.find("The booty:") != std::string::npos &&
                     std::ranges::find(s.choices, "Exit") != s.choices.end())
            {
                if (loot_type)
                {
                    const auto record =
                        og::buck_rogers_record_of(campaign.party->game_record(loot_buyer));
                    check(record.has_value(), "The actual recipient retains its native inventory");
                    unsigned quantity = 0;
                    for (const auto &item : record->items)
                    {
                        if (item.type == loot_type)
                        {
                            quantity += (loot_type == 33 || loot_type == 25) ? item.quantity : 1;
                        }
                    }
                    if (quantity > loot_before)
                    {
                        check(record->types.size() > loot_type,
                              "Newly taken booty keeps its type row");
                        std::cout << "Actual booty pickup " << record->creature.name << " type "
                                  << loot_type << ".\n";
                    }
                    else
                    {
                        blocked_loot.emplace(loot_buyer, loot_type);
                    }
                    loot_type = 0;
                }
                for (const auto &[type, amount, label] :
                     {std::tuple{25U, 3U, "Demo Charge"}, std::tuple{35U, 1U, "Stun Grenade"},
                      std::tuple{36U, 1U, "Dazzle Grenade"},
                      std::tuple{24U, 1U, "Battle armor w/fields"},
                      std::tuple{23U, 1U, "Battle armor"}, std::tuple{18U, 1U, "Rocket Lnchr."},
                      std::tuple{17U, 1U, "Plasma Thrwr"}, std::tuple{16U, 1U, "Grenade Lnchr."},
                      std::tuple{15U, 1U, "Sonic"}, std::tuple{10U, 1U, "Rocket Pistol"},
                      std::tuple{11U, 1U, "Rocket Rifle"}, std::tuple{13U, 1U, "Laser Rifle"},
                      std::tuple{33U, 1U, "Explosive Grenade"},
                      std::tuple{22U, 1U, "Heavy Body Armor"},
                      std::tuple{33U, 2U, "Explosive Grenade"},
                      std::tuple{33U, 3U, "Explosive Grenade"},
                      std::tuple{33U, 4U, "Explosive Grenade"}})
                {
                    if (type == 15 && (s.script_id < 96 || s.script_id > 98))
                    {
                        continue;
                    }
                    const auto take = std::ranges::find_if(
                        s.choices,
                        [&](const auto &choice)
                        {
                            return choice.starts_with("Take ") &&
                                   choice.find(label) != std::string::npos &&
                                   (type != 23 || choice.find("w/fields") == std::string::npos);
                        });
                    if (take == s.choices.end())
                    {
                        continue;
                    }
                    const std::array<unsigned, 6> armor_order =
                        type == 22 ? std::array{4U, 0U, 2U, 3U, 1U, 5U}
                                   : std::array{4U, 3U, 5U, 0U, 2U, 1U};
                    for (unsigned n = 0; n < 6; ++n)
                    {
                        const std::array<unsigned, 6> dazzle_order{2, 0, 1, 4, 3, 5};
                        const auto slot = type >= 22 && type <= 24 ? armor_order[n]
                                          : type == 36             ? dazzle_order[n]
                                                                   : n;
                        const auto id = campaign.party->state().slots[slot];
                        if (!id || blocked_loot.contains({id, type}))
                        {
                            continue;
                        }
                        const auto record =
                            og::buck_rogers_record_of(campaign.party->game_record(id));
                        check(record.has_value(), "The actual booty recipient exists");
                        if (type >= 16 && type <= 18 && record->creature.name == "Doc")
                        {
                            continue;
                        }
                        if ((type == 22 || type == 23) &&
                            std::ranges::any_of(record->items,
                                                [&](const auto &item)
                                                {
                                                    return item.type > type && item.type <= 24;
                                                }))
                        {
                            continue;
                        }
                        unsigned quantity = 0;
                        for (const auto &item : record->items)
                        {
                            if (item.type == type)
                            {
                                quantity += (type == 33 || type == 25) ? item.quantity : 1;
                            }
                        }
                        if (quantity >= amount)
                        {
                            continue;
                        }
                        check(campaign.session.can_select_member(),
                              "The actual booty menu permits recipient selection");
                        campaign.party->select(opengold::PartySlot{slot});
                        loot_type = type;
                        loot_buyer = id;
                        loot_before = quantity;
                        wanted = *take;
                        break;
                    }
                    if (!wanted.empty())
                    {
                        break;
                    }
                }
                if (wanted.empty())
                {
                    wanted = divided || std::ranges::find(s.choices, "Divvy") == s.choices.end()
                                 ? "Exit"
                                 : "Divvy";
                    divided = true;
                }
            }
            else
            {
                if (campaign.malcoln_visit)
                {
                    if (s.script_id == 38 &&
                        s.dialogue.find("DO YOU WISH TO USE IT?") != std::string::npos)
                    {
                        wanted = campaign.session.script_variable(
                                     opengold::por::EclAddress{0x4C02}) == campaign.tower_floor
                                     ? "NO"
                                     : "YES";
                    }
                    if (s.script_id == 32 &&
                        s.dialogue.find("RETURN TO YOUR BOAT") != std::string::npos)
                    {
                        wanted = "NO";
                    }
                    if (s.script_id == 32 &&
                        std::ranges::find(s.choices, "TUNNEL") != s.choices.end())
                    {
                        wanted = "TUNNEL";
                    }
                    if (s.script_id == 32 && campaign.leaving_catalina &&
                        s.dialogue.find("YOU CAN GET RED CARRIN'S BOAT") != std::string::npos)
                    {
                        wanted = "NO";
                    }
                    if (s.script_id == 81 &&
                        s.dialogue.find("AN ELEVATOR GOING UP") != std::string::npos)
                    {
                        wanted =
                            campaign.session.script_variable(opengold::por::EclAddress{0x4C8F}) == 1
                                ? "NO"
                                : "YES";
                    }
                    if (s.script_id == 81 && s.choosing_member)
                    {
                        wanted = "Merc";
                    }
                    if (s.script_id == 84 && s.choosing_member)
                    {
                        wanted = "Gears";
                    }
                    if (s.script_id == 84)
                    {
                        if (s.dialogue.find("AN ELEVATOR GOING DOWN") != std::string::npos)
                        {
                            wanted = campaign.session.script_variable(
                                         opengold::por::EclAddress{0x4C8F}) == 1
                                         ? "YES"
                                         : "NO";
                        }
                        if (std::ranges::find(s.choices, "UP LVL") != s.choices.end() ||
                            std::ranges::find(s.choices, "DOWN LVL") != s.choices.end())
                        {
                            const bool sooth_dead = campaign.session.script_variable(
                                                        opengold::por::EclAddress{0x4C8C}) == 1;
                            const auto access =
                                campaign.session.script_variable(opengold::por::EclAddress{0x4C05});
                            bool recover = false;
                            for (const auto id : campaign.party->state().slots)
                            {
                                if (!id)
                                {
                                    continue;
                                }
                                const auto gear =
                                    og::buck_rogers_record_of(campaign.party->game_record(id));
                                if (gear && gear->creature.name == "Gears")
                                {
                                    recover = gear->creature.hit_points <= 20;
                                }
                            }
                            wanted = !sooth_dead && recover      ? "OUT"
                                     : !sooth_dead && access < 3 ? "UP LVL"
                                     : sooth_dead && access > 1  ? "DOWN LVL"
                                                                 : "SEE FILES";
                        }
                        if (s.dialogue.find("LEAVES THE COMPUTER. DOES SOMEONE ELSE TRY?") !=
                            std::string::npos)
                        {
                            wanted = "NO";
                        }
                        for (const auto file : {"BUGNINE_VIR", "EFANITE_NOT"})
                        {
                            if (std::ranges::find(s.choices, file) != s.choices.end())
                            {
                                wanted = file;
                            }
                        }
                    }
                }
                if ((s.script_id == 96 || s.script_id == 97 || s.script_id == 98) &&
                    s.dialogue.starts_with("THE PASSAGE LEADS") &&
                    std::ranges::find(s.choices, "YES") != s.choices.end())
                {
                    wanted = campaign.ship_passage ? "YES" : "NO";
                }
                if (s.script_id == 98)
                {
                    for (const auto intent : {"DESTROY", "USE DEVICE", "FIGHT WITH STORMRIDERS"})
                    {
                        if (std::ranges::find(s.choices, intent) != s.choices.end())
                        {
                            wanted = intent;
                        }
                    }
                }
                if (s.script_id == 112 &&
                    std::ranges::find(s.choices, "REPAIR CONTROLS") != s.choices.end())
                {
                    wanted = "REPAIR CONTROLS";
                }
                if (s.script_id == 114)
                {
                    if (s.dialogue.find("WILL SOMEONE USE THE JETPACK") != std::string::npos)
                    {
                        wanted = "NO";
                    }
                    if (s.choosing_member &&
                        campaign.session.script_variable(opengold::por::EclAddress{0x4C0F}) == 7)
                    {
                        wanted = s.area_id == 115 ? "Jock" : s.pose.x == 5 ? "Runner" : "Merc";
                    }
                }
                if (s.script_id == 113)
                {
                    const std::array<std::pair<unsigned, unsigned>, 3> arrivals{
                        {{3, 4}, {3, 12}, {11, 4}}};
                    if (s.dialogue.find("AN ELEVATOR. DO YOU USE IT?") != std::string::npos)
                    {
                        wanted = std::pair{unsigned(s.pose.x), unsigned(s.pose.y)} ==
                                         arrivals.at(campaign.genetics_floor - 1)
                                     ? "NO"
                                     : "YES";
                    }
                    if (std::ranges::find(s.choices, "1") != s.choices.end() &&
                        std::ranges::find(s.choices, "3") != s.choices.end())
                    {
                        wanted = std::to_string(campaign.genetics_floor);
                    }
                    if (s.dialogue.starts_with("THE LADDER HERE LEADS"))
                    {
                        wanted = campaign.genetics_ladder_destination &&
                                         std::pair{unsigned(s.pose.x), unsigned(s.pose.y)} !=
                                             *campaign.genetics_ladder_destination
                                     ? "YES"
                                     : "NO";
                    }
                    if (s.choosing_member)
                    {
                        wanted = "Doc";
                    }
                }
                if (campaign.fortuna_visit && s.script_id == 82)
                {
                    const std::array<std::pair<unsigned, unsigned>, 5> arrivals{
                        {{13, 0}, {15, 11}, {15, 14}, {1, 1}, {2, 14}}};
                    if (s.dialogue.find("AN ELEVATOR. DO YOU USE IT?") != std::string::npos)
                    {
                        wanted = std::pair{unsigned(s.pose.x), unsigned(s.pose.y)} ==
                                         arrivals.at(campaign.fortuna_floor - 1)
                                     ? "NO"
                                     : "YES";
                    }
                    if (std::ranges::find(s.choices, "1") != s.choices.end() &&
                        std::ranges::find(s.choices, "5") != s.choices.end())
                    {
                        wanted = std::to_string(campaign.fortuna_floor);
                    }
                    if (s.choosing_member)
                    {
                        wanted = "Gears";
                    }
                    if (std::ranges::find(s.choices, "FAST TALK") != s.choices.end())
                    {
                        wanted = "FAST TALK";
                    }
                    if (std::ranges::find(s.choices, "ELROY") != s.choices.end())
                    {
                        wanted = "ELROY";
                    }
                    if (std::ranges::find(s.choices, "CHASE") != s.choices.end())
                    {
                        wanted = "CHASE";
                    }
                }
                if (campaign.hielo_supplies && s.script_id == 20)
                {
                    const bool ready = plasma_supplies_ready(campaign);
                    if (std::ranges::find(s.choices, "DOWNTOWN") != s.choices.end())
                    {
                        wanted = ready ? "PORT" : "DOWNTOWN";
                    }
                    else if (std::ranges::find(s.choices, "QUIPMENTLAND") != s.choices.end())
                    {
                        wanted = ready ? "EXIT" : "QUIPMENTLAND";
                    }
                    else if (std::ranges::find(s.choices, "SHOP") != s.choices.end())
                    {
                        wanted = ready ? "EXIT" : "SHOP";
                    }
                }
                if (campaign.hielo_supplies && s.script_id == 95 &&
                    std::ranges::find(s.choices, "LAUNCH") != s.choices.end())
                {
                    wanted = "LAUNCH";
                }
                if (s.script_id == 18 && std::ranges::find(s.choices, "PORT") != s.choices.end())
                {
                    wanted = wounded() && std::ranges::find(s.choices, "CLINIC") != s.choices.end()
                                 ? "CLINIC"
                             : training_available(campaign) ? "TRAINING"
                             : ammunition_need(campaign)    ? "DEPOT"
                                                            : "PORT";
                }
                if (s.script_id == 35 && std::ranges::find(s.choices, "ATTACK") != s.choices.end())
                {
                    wanted = "ATTACK";
                }
                if (s.script_id == 35 && std::ranges::find(s.choices, "FORCE") != s.choices.end())
                {
                    wanted = "FORCE";
                }
                if (s.script_id == 35 &&
                    std::ranges::find(s.choices, "ENTER CODE") != s.choices.end())
                {
                    wanted = "ENTER CODE";
                }
                if (s.script_id == 35 &&
                    s.dialogue.find("DO YOU CONTINUE TO WATCH?") != std::string::npos)
                {
                    const auto story =
                        campaign.session.script_variable(opengold::por::EclAddress{0x4CD8});
                    const auto clues =
                        campaign.session.script_variable(opengold::por::EclAddress{0x4CDB});
                    wanted = (story & 0x80) && (clues & 8) ? "NO" : "YES";
                }
                if (s.script_id == 35 &&
                    s.dialogue.find("SET THE EXPLOSIVES TO DESTROY THE LASER NOW?") !=
                        std::string::npos)
                {
                    wanted = campaign.ready_for_ceres_demolition ? "YES" : "NO";
                }
                if (s.script_id == 35 &&
                    std::ranges::find(s.choices, "OPERATE SWITCHES") != s.choices.end())
                {
                    wanted = "OPERATE SWITCHES";
                }
                if (s.script_id == 35 &&
                    std::ranges::find(s.choices, "SET COORDINATES") != s.choices.end())
                {
                    const auto targeted =
                        campaign.session.script_variable(opengold::por::EclAddress{0x4C0D});
                    const auto charged =
                        campaign.session.script_variable(opengold::por::EclAddress{0x4C09});
                    wanted = !targeted ? "SET COORDINATES" : !charged ? "CHARGE" : "DISCHARGE";
                }
                if (s.script_id == 35 && s.dialogue.find("TRY ANOTHER?") != std::string::npos)
                {
                    const auto story =
                        campaign.session.script_variable(opengold::por::EclAddress{0x4CD8});
                    wanted = story & 0x20 ? "NO" : "YES";
                }
                if (s.script_id == 32 && campaign.leaving_museum &&
                    s.dialogue.find("DO YOU ENTER THE HISTORICAL MUSEUM?") != std::string::npos)
                {
                    wanted = "NO";
                }
                if (s.script_id == 32 &&
                    s.dialogue.find("A SECTION OF THE SPRAWL HAS BEEN WALLED OFF") !=
                        std::string::npos)
                {
                    wanted = campaign.leaving_warren ? "NO" : "YES";
                }
                if (s.script_id == 80 && std::ranges::find(s.choices, "STAND") != s.choices.end())
                {
                    wanted = "STAND";
                }
                if (s.script_id == 66 &&
                    std::ranges::find(s.choices, "TAKE CARD") != s.choices.end())
                {
                    wanted = "TAKE CARD";
                }
                if (s.script_id == 66 &&
                    std::ranges::find(s.choices, "PRIMARY REC") != s.choices.end() &&
                    std::ranges::find(s.choices, "REC II") != s.choices.end())
                {
                    wanted = "PRIMARY REC";
                }
                if (s.script_id == 65 &&
                    std::ranges::find(s.choices, "DEMO CHARGE") != s.choices.end())
                {
                    wanted = "DEMO CHARGE";
                }
                if (s.script_id == 65 &&
                    s.dialogue.find("DO YOU ACTIVATE THE HUMMER?") != std::string::npos)
                {
                    wanted = "NO"; // Fight through a blocking room rather than retreating into its
                                   // doorway.
                }
                if (s.script_id == 65 && s.dialogue.find("DISTRACT THEM") != std::string::npos &&
                    std::ranges::find(s.choices, "TALK") != s.choices.end())
                {
                    wanted = "TALK";
                }
                if (s.script_id == 64)
                {
                    if (std::ranges::find(s.choices, "DEMO CHARGES") != s.choices.end())
                    {
                        wanted = "DEMO CHARGES";
                    }
                    if (s.dialogue.find("WHO DO YOU CALL?") != std::string::npos &&
                        std::ranges::find(s.choices, campaign.lunar_phone_target) !=
                            s.choices.end())
                    {
                        wanted = campaign.lunar_phone_target;
                    }
                    if (s.dialogue.find("WHICH FILE?") != std::string::npos)
                    {
                        wanted =
                            std::ranges::find(s.choices, "NEO") != s.choices.end() ? "NEO" : "NEXT";
                    }
                    if (s.dialogue.find("MAKE ANOTHER CALL?") != std::string::npos ||
                        s.dialogue.find("READ ANOTHER FILE?") != std::string::npos)
                    {
                        wanted = "NO";
                    }
                    if (std::ranges::find(s.choices, "A CUT") != s.choices.end() &&
                        std::ranges::find(s.choices, "DR. COLDOR") != s.choices.end())
                    {
                        wanted = "DR. COLDOR";
                    }
                }
                if (s.script_id == 48 &&
                    campaign.session.script_variable(opengold::por::EclAddress{0x4C7E}) == 1 &&
                    s.dialogue.find("CLIMB DOWN THROUGH THE DESTROYED ROOF") != std::string::npos)
                {
                    wanted = "NO"; // The scientist is rescued; continue the evacuation.
                }
                if (s.script_id == 49 && s.dialogue.find("DO YOU LEAVE?") != std::string::npos)
                {
                    wanted = "NO"; // Step inward from the gate before attempting to leave.
                }
                if (s.script_id == 33 || s.script_id == 37)
                {
                    if (std::ranges::find(s.choices, "TALK") != s.choices.end())
                    {
                        wanted = repeat < 3 ? "TALK" : "LEAVE";
                    }
                    if (std::ranges::find(s.choices, "GIVE WEAPONS") != s.choices.end())
                    {
                        wanted = "GIVE WEAPONS";
                    }
                    for (const auto choice :
                         {"GO ALONG", "RESCUE LOA-LOA", "FIGHT MONKS", "GRAB HIM"})
                    {
                        if (std::ranges::find(s.choices, choice) != s.choices.end())
                        {
                            wanted = choice;
                        }
                    }
                    if (std::ranges::find(s.choices, "JOIN") != s.choices.end() &&
                        std::ranges::find(s.choices, "STAY NEUTRAL") != s.choices.end())
                    {
                        wanted = "STAY NEUTRAL";
                    }
                    if (std::ranges::find(s.choices, "HELP LEECHES") != s.choices.end())
                    {
                        wanted = "HELP LEECHES";
                    }
                }
                if (s.script_id == 36)
                {
                    if (std::ranges::find(s.choices, "KNOCK") != s.choices.end())
                    {
                        wanted = "KNOCK";
                    }
                    if (std::ranges::find(s.choices, "CALL SCOT.DOS") != s.choices.end())
                    {
                        wanted = "CALL SCOT.DOS";
                    }
                }
                if (s.script_id == 36 && s.dialogue.find(" RISER?") != std::string::npos &&
                    std::ranges::find(s.choices, "YES") != s.choices.end())
                {
                    // The installed computer's explicit clue includes1.
                    const auto riser =
                        campaign.session.script_variable(opengold::por::EclAddress{0x4C09});
                    const std::array<unsigned, 8> safe{1, 2, 3, 5, 7, 11, 13, 17};
                    wanted = std::ranges::find(safe, riser) != safe.end() ? "YES" : "NO";
                }
                if (s.script_id == 34)
                {
                    if (s.dialogue.find("AN ANTIQUE PROJECTOR IS ON A PEDESTAL") !=
                        std::string::npos)
                    {
                        wanted = "NO"; // The team has a time-sensitive rescue, not a movie night.
                    }
                    for (const auto choice : {"FOLLOW CHADE", "FOLLOW STEFI"})
                    {
                        if (std::ranges::find(s.choices, choice) != s.choices.end())
                        {
                            wanted = choice;
                        }
                    }
                }
                for (const auto &preferred :
                     {"KNOCK DOWN THE SUN KING", "HELP ROMNEY", "QUESTION", "IGNORE HIM"})
                {
                    if (std::ranges::find(s.choices, preferred) != s.choices.end())
                    {
                        wanted = preferred;
                        break;
                    }
                }
                if (s.dialogue.find("LEAVE THEM WITH DE SADE") != std::string::npos)
                {
                    wanted = "NO";
                }
                if (repeat > 2 && wanted.empty())
                {
                    pick = s.choices.size() - 1;
                }
            }
            if (!wanted.empty())
            {
                const auto found = std::ranges::find(s.choices, wanted);
                check(found != s.choices.end(),
                      "Matrix's actual menu offers the route's choice: " + wanted);
                pick = static_cast<std::size_t>(found - s.choices.begin());
            }
            std::cout << "Route page " << s.script_id << ':' << s.pose.x << ',' << s.pose.y << " "
                      << s.dialogue.substr(0, 180) << " [" << s.choices.at(pick) << "]\n";
            std::optional<std::tuple<opengold::MemberId, int, std::uint32_t>> trained;
            if (s.choices.at(pick).starts_with("Train "))
            {
                const auto name = s.choices.at(pick).substr(6);
                for (const auto &member : campaign.party->state().roster)
                {
                    const auto record =
                        og::buck_rogers_record_of(campaign.party->game_record(member.id));
                    if (record && record->creature.name == name)
                    {
                        check(og::training_refusal(record->creature, matrix) ==
                                  og::TrainingRefusal::none,
                              "The real trainee has already earned the required experience");
                        trained = std::tuple{member.id, record->creature.level,
                                             record->creature.experience};
                        break;
                    }
                }
            }
            check(campaign.session.choose(s.continue_ticket, pick), "Choose Matrix's actual menu");
            if (trained)
            {
                check(campaign.session.snapshot().phase == TourPhase::training,
                      "Training opens its choices");
                const auto archive = opengold::decode_dax_archive(bytes_of(folder / "ITEM0.DAX"));
                const auto first = std::ranges::find(archive.records, 1U, &opengold::DaxRecord::id);
                check(first != archive.records.end(), "Matrix has its item templates");
                const auto templates = br::decode_item_templates(first->bytes);
                check(templates.has_value(), "Matrix's item templates decode");
                opengold::test::choose_buck_rogers_training(campaign.session, *campaign.party,
                                                            *templates);
                const auto [id, level, experience] = *trained;
                const auto record = og::buck_rogers_record_of(campaign.party->game_record(id));
                check(record.has_value(), "The trainee keeps its record");
                // Training cuts experience past the next level to one short
                // of it (chargen.pas TrainingMenu).
                const auto next = og::career_experience_for_level(
                    og::career_from_record(record->creature.career).value(), level + 2, matrix);
                const auto kept = next ? std::min(experience, *next - 1) : experience;
                check(record->creature.level == level + 1 && record->creature.experience == kept,
                      "Only actual offered training advances a level, keeping the earned "
                      "experience up to the next level's");
                std::cout << "Actual TRAINING " << record->creature.name << " level "
                          << unsigned(record->creature.level) << ", earned XP " << experience
                          << ", maximum HP " << record->creature.hit_point_maximum << ".\n";
            }
            continue;
        }
        if (s.phase == TourPhase::combat)
        {
            if (campaign.session.monster_picture())
            {
                check(campaign.session.start_encounter(), "Dismiss the actual monster close-up");
                continue;
            }
            opengold::CombatDemo combat(
                og::rules_module(opengold::load_original_content(folder, matrix)));
            combat.campaign_party(campaign.party);
            check(campaign.session.pending_encounter().has_value(),
                  "Matrix asks for a real ground fight");
            std::map<opengold::rules::EntityId, unsigned> avoid_grenades;
            std::map<std::pair<opengold::rules::EntityId, unsigned>, unsigned> player_actions;
            const auto seed =
                static_cast<std::uint64_t>(opengold::roll_die(campaign.battle_random, 2147483647));
            std::cout << "Frozen battle stream 20261120, draw " << ++campaign.battle_draws
                      << ", seed " << seed << ".\n";
            const auto native_level = opengold::test::TourAccess::machine(campaign.session)
                                          .variable(opengold::por::EclAddress{0x4BB6});
            check(native_level < 6, "The actual native difficulty is a supported setting");
            const auto level = static_cast<og::BuckRogersDifficulty>(native_level);
            for (const auto &enemy : campaign.session.pending_encounter()->enemies)
            {
                check(enemy.original_strength.hit_points_percent ==
                          og::monster_hit_points_percent(level),
                      "The actual encounter uses the full selected native HP scaling");
            }
            check(campaign.session.pending_encounter()->party_to_hit == og::party_to_hit(level),
                  "The actual encounter retains the selected native party modifier");
            std::cout << "Actual native difficulty " << native_level << ", enemy HP percent "
                      << og::monster_hit_points_percent(level) << ".\n";
            combat.encounter(*campaign.session.pending_encounter(), seed);
            // Their chaff intercepts thrown explosives near the party.
            const bool assault_robots =
                std::ranges::any_of(combat.combat().snapshot().combatants,
                                    [](const auto &unit)
                                    {
                                        return unit.name.starts_with("ASSAULT ROBOT");
                                    });
            for (const auto &fighter : combat.combat().snapshot().combatants)
            {
                std::cout << "Actual initial fighter " << fighter.name << " HP "
                          << fighter.hit_points << ", side " << static_cast<unsigned>(fighter.side)
                          << ".\n";
                if (s.script_id == 34 && fighter.side != opengold::rules::Side::party)
                {
                    const auto defense = og::buck_rogers_record_of(fighter.persistent.resources);
                    if (defense)
                    {
                        std::cout << "Actual enemy armor flags " << fighter.name << ": "
                                  << unsigned(defense->creature.armor_flags) << ".\n";
                    }
                }
                if (fighter.side == opengold::rules::Side::party && s.script_id == 80 &&
                    !opengold::is_ally(fighter.id))
                {
                    const auto record = og::buck_rogers_record_of(fighter.persistent.resources);
                    if (record)
                    {
                        for (const auto &item : record->items)
                        {
                            if (item.type == 7 || item.type == 9 || item.type == 10 ||
                                item.type == 11 || item.type == 13)
                            {
                                std::cout << "Actual prison gun " << fighter.name << " type "
                                          << unsigned(item.type) << " rounds "
                                          << unsigned(item.quantity) << ".\n";
                            }
                        }
                    }
                }
                if (fighter.side == opengold::rules::Side::party && s.script_id == 34)
                {
                    const auto record = og::buck_rogers_record_of(fighter.persistent.resources);
                    const auto weapon =
                        record ? og::readied_weapon(record->items, record->types) : std::nullopt;
                    if (record)
                    {
                        const auto numbers = og::ground_numbers(record->creature, record->items,
                                                                record->types, false);
                        std::cout << "Actual mobility " << fighter.name << ": movement "
                                  << numbers.movement << ", weight " << numbers.encumbrance
                                  << ", half allowance "
                                  << og::buck_rogers_weight_allowance(
                                         record->creature.current_abilities[0]) /
                                         2
                                  << ".\n";
                        unsigned grenades = 0;
                        for (const auto &item : record->items)
                        {
                            if (item.type == 33)
                            {
                                grenades += item.quantity;
                            }
                        }
                        std::cout << "Actual carried grenades " << fighter.name << ": " << grenades
                                  << ".\n";
                    }
                    if (weapon)
                    {
                        std::cout << "Actual carried gun " << fighter.name << " type "
                                  << unsigned(record->items[*weapon].type) << ", rounds "
                                  << unsigned(record->items[*weapon].quantity) << ".\n";
                    }
                }
            }
            for (unsigned turn = 0; turn < 8000 && combat.combat().snapshot().outcome ==
                                                       opengold::rules::Outcome::ongoing;
                 ++turn)
            {
                const auto state = combat.combat().snapshot();
                const auto actor = std::ranges::find(state.combatants, state.actor,
                                                     &opengold::rules::CombatantView::id);
                const bool member = actor != state.combatants.end() &&
                                    actor->side == opengold::rules::Side::party &&
                                    !opengold::is_ally(state.actor) && !actor->computer_plays;
                const auto offered = combat.combat().legal_commands();
                opengold::rules::Command command;
                // A player who keeps reconsidering free inventory choices
                // can finish the turn normally. Never synthesize an outcome
                // or let an equipment-choice loop replace native combat.
                if (member && s.script_id == 114 &&
                    ++player_actions[{state.actor, state.round}] > 16)
                {
                    const auto done = std::ranges::find(offered, std::string("end"),
                                                        &opengold::rules::Command::verb);
                    check(done != offered.end(),
                          "The actual PC can finish its equipment decisions");
                    std::cout << "Actual player ends a long equipment turn: " << actor->name
                              << ", round " << state.round << ", ready types";
                    const auto gear = og::buck_rogers_record_of(actor->persistent.resources);
                    if (gear)
                    {
                        for (const auto &item : gear->items)
                        {
                            if (item.flags & br::Item::readied)
                            {
                                std::cout << ' ' << unsigned(item.type);
                            }
                        }
                    }
                    std::cout << ".\n";
                    check(combat.submit(*done),
                          "Finish the native turn with its offered End command");
                    continue;
                }
                if (member)
                {
                    const auto healer = og::buck_rogers_record_of(actor->persistent.resources);
                    const bool can_bandage =
                        healer &&
                        (og::level_of(healer->creature.skills, og::Skill::first_aid) > 0 ||
                         og::level_of(healer->creature.skills, og::Skill::treat_light_wounds) > 0);
                    if (can_bandage)
                    {
                        for (const auto &patient : state.combatants)
                        {
                            if (patient.side != actor->side || opengold::is_ally(patient.id) ||
                                patient.fled || patient.dead)
                            {
                                continue;
                            }
                            const auto injured =
                                og::buck_rogers_record_of(patient.persistent.resources);
                            if (!injured || injured->creature.status != 4)
                            {
                                continue;
                            }
                            const auto distance = [&](auto cell)
                            {
                                return std::max(std::abs(cell.x - patient.cell.x),
                                                std::abs(cell.y - patient.cell.y));
                            };
                            auto order = offered.end();
                            if (distance(actor->cell) <= 1)
                            {
                                order = std::ranges::find(offered, std::string("aid"),
                                                          &opengold::rules::Command::verb);
                            }
                            else if (actor->name == "Doc" &&
                                     (s.script_id == 65 || s.script_id == 80))
                            {
                                order = std::ranges::find_if(
                                    offered,
                                    [&](const auto &candidate)
                                    {
                                        return candidate.verb == "move" &&
                                               distance(candidate.destination) <= 1;
                                    });
                            }
                            if (order != offered.end())
                            {
                                std::cout << "Actual rescue order " << actor->name << ' '
                                          << order->verb << " near bleeding " << patient.name
                                          << ".\n";
                                check(
                                    combat.submit(*order),
                                    "Use the offered native action to reach and bandage a bleeding crew member");
                                command.verb = "already submitted";
                                break;
                            }
                        }
                    }
                }
                if (command.verb == "already submitted")
                {
                    continue;
                }
                if (member && campaign.prison_rearming && actor->name != "Doc" &&
                    actor->name != "Jock")
                {
                    const auto gear = og::buck_rogers_record_of(actor->persistent.resources);
                    auto rocket = std::ranges::find(gear->items, 17U, &br::Item::type);
                    if (rocket == gear->items.end() || !rocket->quantity)
                    {
                        rocket = std::ranges::find(gear->items, 18U, &br::Item::type);
                    }
                    check(rocket != gear->items.end() && rocket->quantity,
                          "The real rearming member owns a loaded rocket launcher");
                    auto order = std::ranges::find(offered, std::string("end"),
                                                   &opengold::rules::Command::verb);
                    if (!(rocket->flags & br::Item::readied))
                    {
                        auto item = static_cast<unsigned>(rocket - gear->items.begin());
                        std::string verb = "ready";
                        if (const auto armed = og::readied_weapon(gear->items, gear->types))
                        {
                            item = static_cast<unsigned>(*armed);
                            verb = "unready";
                        }
                        order = std::ranges::find_if(offered,
                                                     [&](const auto &candidate)
                                                     {
                                                         return candidate.verb == verb &&
                                                                candidate.item == item;
                                                     });
                    }
                    check(order != offered.end(),
                          "The barracks still offers the actual rearming action");
                    check(combat.submit(*order),
                          "The barracks rearming action keeps the full native cost");
                    continue;
                }
                // The native fight can still offer player turns after its
                // last active opponent falls. Use those real turns to load
                // the carried heavy weapon; the engine decides when combat ends.
                if (member &&
                    (s.script_id == 66 || s.script_id == 80 || s.script_id == 81 ||
                     s.script_id == 84 || s.script_id == 114) &&
                    std::ranges::none_of(state.combatants,
                                         [](const auto &unit)
                                         {
                                             return unit.side != opengold::rules::Side::party &&
                                                    unit.hit_points > 0 && unit.conscious &&
                                                    !unit.dead && !unit.fled;
                                         }))
                {
                    auto order = std::ranges::find(offered, std::string("end"),
                                                   &opengold::rules::Command::verb);
                    const auto gear = og::buck_rogers_record_of(actor->persistent.resources);
                    auto heavy = std::ranges::find(gear->items, 18U, &br::Item::type);
                    if (campaign.prison_rearming || s.script_id == 114 ||
                        heavy == gear->items.end() || !heavy->quantity)
                    {
                        const auto plasma = std::ranges::find(gear->items, 17U, &br::Item::type);
                        if (plasma != gear->items.end() && plasma->quantity)
                        {
                            heavy = plasma;
                        }
                    }
                    if (heavy == gear->items.end() || !heavy->quantity)
                    {
                        heavy = std::ranges::find(gear->items, 16U, &br::Item::type);
                    }
                    if (heavy != gear->items.end() && !(heavy->flags & br::Item::readied))
                    {
                        auto item = static_cast<unsigned>(heavy - gear->items.begin());
                        std::string verb = "ready";
                        if (heavy->type == 18 || heavy->type == 17)
                        {
                            if (const auto armed = og::readied_weapon(gear->items, gear->types))
                            {
                                item = static_cast<unsigned>(*armed);
                                verb = "unready";
                            }
                        }
                        order = std::ranges::find_if(offered,
                                                     [&](const auto &candidate)
                                                     {
                                                         return candidate.verb == verb &&
                                                                candidate.item == item;
                                                     });
                    }
                    check(order != offered.end(),
                          "Native combat still offers this real cleanup action");
                    std::cout << "Actual heavy cleanup " << actor->name << ' ' << order->verb
                              << ".\n";
                    check(combat.submit(*order),
                          "The native Ready action retains its full turn cost");
                    continue;
                }
                // Chaff blocks shots along its actual path. Still make ordinary
                // offered shots at enemy groups; the native engine resolves cover.
                const bool robots_deployed_chaff =
                    assault_robots &&
                    std::ranges::any_of(state.log(),
                                        [](const auto &line)
                                        {
                                            return line.starts_with("ASSAULT ROBOT") &&
                                                   line.find("throws a grenade") !=
                                                       std::string::npos;
                                        });
                const bool explosives_clear =
                    !assault_robots ||
                    ((s.script_id == 80 || s.script_id == 114) && !robots_deployed_chaff);
                // Save two actual carried grenades per member after Loa's
                // rescue for the museum's much tougher escaped specimens.
                const bool specimens =
                    std::ranges::any_of(state.combatants,
                                        [](const auto &unit)
                                        {
                                            return unit.name.starts_with("STAGE 5 ECG");
                                        });
                const bool museum_rescue =
                    s.script_id == 34 &&
                    s.dialogue.find("CHADE SPRAWLS AT HER FEET") != std::string::npos;
                const bool reserve_grenade =
                    !specimens && !museum_rescue &&
                    (s.script_id == 34 ||
                     ((s.script_id == 32 || s.script_id == 33 || s.script_id == 37) &&
                      (campaign.session.script_variable(opengold::por::EclAddress{0x4C58}) &
                       0x40)));
                const auto actor_gear = member
                                            ? og::buck_rogers_record_of(actor->persistent.resources)
                                            : std::optional<og::GroundRecord>{};
                const bool reserve_doc_launcher =
                    actor_gear && actor->name == "Doc" &&
                    og::readied_in_slot(actor_gear->items, actor_gear->types,
                                        og::gear_slot::grenade_launcher)
                        .has_value();
                const auto equipment =
                    member ? ready_route_equipment(
                                 state, offered,
                                 explosives_clear && !reserve_doc_launcher &&
                                     (!avoid_grenades.contains(state.actor) ||
                                      avoid_grenades.at(state.actor) != state.round),
                                 reserve_grenade ? 2U : (specimens && s.script_id == 34 ? 1U : 0U),
                                 (s.script_id == 80 || s.script_id == 81 || s.script_id == 84 ||
                                  s.script_id == 114) &&
                                     actor->name != "Doc" &&
                                     (campaign.session.pending_encounter()->enemies.size() >= 10 ||
                                      (campaign.prison_rearming && actor->name == "Jock")),
                                 false, og::party_to_hit(level), s.script_id == 114)
                           : std::nullopt;
                if (equipment)
                {
                    command = *equipment;
                }
                else
                {
                    command = opengold::choose_demo_command(combat.combat(),
                                                            opengold::Searching::everyone,
                                                            opengold::default_difficulty);
                    if (member ||
                        (s.script_id == 80 && assault_robots && actor != state.combatants.end() &&
                         actor->side == opengold::rules::Side::party && !actor->computer_plays))
                    {
                        // Reuse Countdown's earned player's public orders:
                        // focus on a wounded foe, closing before long shots.
                        const auto offered = combat.combat().legal_commands();
                        int least_hit_points = 1000000;
                        for (const auto &attack : offered)
                        {
                            if (attack.verb != "ranged" && attack.verb != "melee")
                            {
                                continue;
                            }
                            const auto target =
                                std::ranges::find(state.combatants, attack.target,
                                                  &opengold::rules::CombatantView::id);
                            if (target != state.combatants.end() &&
                                target->side != opengold::rules::Side::party &&
                                target->hit_points > 0)
                            {
                                // In mixed sprawl fights, stop the armed RAM
                                // warriors and gang veterans before weaker support.
                                const int priority =
                                    target->hit_points -
                                    (s.script_id == 114 && target->name.starts_with("RAM ASSASSIN")
                                         ? 4500
                                         : 0) -
                                    (target->name.starts_with("RAM CBT. GENNIE")
                                         ? (s.script_id == 80 && assault_robots ? 3500 : 2000)
                                         : 0) -
                                    (target->name.starts_with("ASSAULT ROBOT") ? 3000 : 0) -
                                    (target->name.starts_with("SID REFUGE") ? 4000 : 0) -
                                    ((s.script_id == 33 || s.script_id == 37) &&
                                             (target->name.starts_with("RAM WARRIOR") ||
                                              target->name.starts_with("GANG VETERAN"))
                                         ? 1000
                                         : 0);
                                if (priority < least_hit_points)
                                {
                                    least_hit_points = priority;
                                    command = attack;
                                }
                            }
                        }
                        const auto distance = [](auto from, auto to)
                        {
                            return std::max(std::abs(from.x - to.x), std::abs(from.y - to.y));
                        };
                        auto nearest = state.combatants.end();
                        int nearest_distance = 1000000;
                        for (auto enemy = state.combatants.begin(); enemy != state.combatants.end();
                             ++enemy)
                        {
                            const auto away = distance(actor->cell, enemy->cell);
                            if (enemy->side != opengold::rules::Side::party &&
                                enemy->hit_points > 0 && !enemy->fled && !enemy->dead &&
                                away < nearest_distance)
                            {
                                nearest = enemy;
                                nearest_distance = away;
                            }
                        }
                        if (command.verb != "ranged" && command.verb != "melee" &&
                            nearest != state.combatants.end() && nearest_distance > 1)
                        {
                            for (const auto &move : offered)
                            {
                                const auto away = distance(move.destination, nearest->cell);
                                if (move.verb == "move" && away < nearest_distance)
                                {
                                    nearest_distance = away;
                                    command = move;
                                }
                            }
                        }
                    }

                    if (member)
                    {
                        const auto native = og::buck_rogers_record_of(actor->persistent.resources);
                        const auto weapon = native
                                                ? og::readied_weapon(native->items, native->types)
                                                : std::nullopt;
                        if (weapon && og::projectile_kind(native->items[*weapon]) != 0)
                        {
                            if (const auto grenade = grenade_route_order(
                                    state, offered, og::projectile_kind(native->items[*weapon]),
                                    native->types[native->items[*weapon].type].dice_count *
                                            native->types[native->items[*weapon].type].dice_sides +
                                        native->types[native->items[*weapon].type].damage_bonus +
                                        native->items[*weapon].plus))
                            {
                                command = *grenade;
                            }
                            else
                            {
                                const bool front_thrower =
                                    (s.script_id == 34 || s.script_id == 36) &&
                                    og::career_from_record(native->creature.career) ==
                                        og::Profession::warrior &&
                                    actor->hit_points * 2 >= actor->max_hit_points;
                                const auto approach =
                                    front_thrower ? grenade_firing_move(
                                                        state, offered,
                                                        og::projectile_kind(native->items[*weapon]))
                                                  : std::nullopt;
                                if (approach)
                                {
                                    command = *approach;
                                }
                                else
                                {
                                    avoid_grenades[state.actor] = state.round;
                                    continue;
                                }
                            }
                        }
                    }
                }
                check(combat.submit(command), "Matrix's actual fight accepts " + command.verb);
            }
            const auto outcome = combat.combat().snapshot();
            if (outcome.outcome != opengold::rules::Outcome::victory ||
                (std::ranges::any_of(outcome.combatants,
                                     [](const auto &member)
                                     {
                                         return member.side == opengold::rules::Side::party &&
                                                member.hit_points <= 0;
                                     })))
            {
                const auto lines = outcome.log();
                for (std::size_t line = 0; line < lines.size(); ++line)
                {
                    std::cout << "Actual fight log: " << lines[line] << '\n';
                }
            }
            check(outcome.outcome != opengold::rules::Outcome::ongoing,
                  "Matrix's real fight terminates");
            campaign.ground_victories += outcome.outcome == opengold::rules::Outcome::victory;
            campaign.ground_escapes += outcome.outcome == opengold::rules::Outcome::fled;
            std::cout << "Actual ground totals: " << campaign.ground_victories << " victories, "
                      << campaign.ground_escapes << " escapes.\n";
            std::cout << "Ground fight " << ++fights << " outcome "
                      << static_cast<unsigned>(outcome.outcome) << ".\n";
            check(campaign.session.resolve_combat(outcome),
                  "Matrix resumes with its actual ground outcome");
            for (const auto &member : campaign.party->state().roster)
            {
                const auto record =
                    og::buck_rogers_record_of(campaign.party->game_record(member.id));
                if (record)
                {
                    std::cout << "Crew " << record->creature.name << " HP "
                              << record->creature.hit_points << '/'
                              << record->creature.hit_point_maximum << " status "
                              << unsigned(record->creature.status) << ".\n";
                }
            }
            continue;
        }
        if (s.phase == TourPhase::ship_combat)
        {
            auto &ship = opengold::por::ship_fight(campaign.session, {});
            for (unsigned order = 0; !ship.over() && order < 5000; ++order)
            {
                if (ship.awaits_dodge())
                {
                    ship.answer_dodge(true);
                    continue;
                }
                if (ship.awaits_round())
                {
                    ship.play_round();
                    continue;
                }
                const auto offered = ship.legal_orders();
                auto chosen =
                    std::ranges::find(offered, og::ShipAction::quit, &og::ShipOrder::action);
                const auto actor = ship.acting_member();
                if (!ship.pilot() && actor &&
                    og::career_from_record(ship.crew()[*actor].character.career) ==
                        og::Profession::rocket_jock)
                {
                    chosen =
                        std::ranges::find(offered, og::ShipAction::command, &og::ShipOrder::action);
                }
                if (chosen == offered.end() || chosen->action != og::ShipAction::command)
                {
                    // The shared ship screen's tested public order pattern.
                    for (const auto wanted :
                         {og::ShipAction::close, og::ShipAction::fire, og::ShipAction::load,
                          og::ShipAction::jury_rig, og::ShipAction::aid})
                    {
                        const auto found =
                            std::ranges::find(offered, wanted, &og::ShipOrder::action);
                        if (found != offered.end())
                        {
                            chosen = found;
                            break;
                        }
                    }
                }
                check(chosen != offered.end(), "The real ship fight offers a player order");
                ship.submit(*chosen);
            }
            check(ship.over(), "The real shared Matrix ship fight terminates");
            const auto end = ship.end();
            std::cout << "Actual Matrix ship fight ended: outcome " << end.outcome << ", boarded "
                      << end.boarded << ", party hull " << end.party_ship.system_status[0]
                      << ", enemy hull " << end.enemy_ship.system_status[0] << ".\n";
            check(campaign.session.finish_ship_combat(s.continue_ticket),
                  "Commit only the actual shared ship fight's completed outcome");
            continue;
        }
        std::cout << "Concrete Matrix transaction boundary: phase "
                  << static_cast<unsigned>(s.phase) << ", script " << s.script_id << ", map "
                  << s.area_id << ", solar " << s.solar_map << ".\n";
        return false;
    }
    throw std::runtime_error("Actual Matrix route event did not settle");
}

bool ordinary_recovery_camp(Campaign &campaign, const std::filesystem::path &folder,
                            unsigned &fights, bool carry_comatose);

bool walk_route(Campaign &campaign, const std::filesystem::path &folder, unsigned goal_x,
                unsigned goal_y, unsigned &fights, bool service = false)
{
    using Command = opengold::por::ExplorationCommand;
    using Direction = opengold::por::MapDirection;
    const auto area = campaign.session.snapshot().area_id;
    const auto controller = campaign.session.snapshot().script_id;
    for (unsigned step = 0; step < 512; ++step)
    {
        if (campaign.session.snapshot().game_won)
        {
            return true;
        }
        if (campaign.malcoln_visit &&
            (campaign.session.snapshot().script_id == 81 ||
             campaign.session.snapshot().script_id == 84) &&
            campaign.session.snapshot().phase == TourPhase::completed &&
            !ordinary_recovery_camp(campaign, folder, fights, false))
        {
            return false;
        }
        const auto pose = campaign.session.snapshot().pose;
        if (pose.x == goal_x && pose.y == goal_y)
        {
            return true;
        }
        const auto &map = campaign.session.map();
        const auto count = map.width * map.height;
        const unsigned start = pose.y * map.width + pose.x, goal = goal_y * map.width + goal_x;
        std::vector<int> parent(count, -1), via(count, -1);
        std::queue<unsigned> pending;
        pending.push(start);
        parent[start] = static_cast<int>(start);
        while (!pending.empty() && parent[goal] < 0)
        {
            const auto here = pending.front();
            pending.pop();
            const auto x = here % map.width, y = here / map.width;
            const auto &cell = map.at(x, y);
            for (unsigned n = 0; n < 4; ++n)
            {
                // The east edge of the actual village landing is its
                // authored wilderness exit, not a wraparound shortcut.
                if (area == 49 && x == 15 && y == 12 && n == 1)
                {
                    continue;
                }
                if ((area == 114 || area == 115) && cell.walls[n] == 4)
                {
                    continue; // Operate the aircar hatch explicitly instead of taking a route
                              // shortcut.
                }
                if ((area == 96 || area == 97 || area == 113) && cell.walls[n] == 5)
                {
                    continue; // Use the actual hatch explicitly after reaching its control cell.
                }
                if (cell.doors[n] > 1 || (cell.walls[n] && !cell.doors[n]))
                {
                    continue;
                }
                const auto side = static_cast<Direction>(n);
                const auto nx = map.wrapped_x(static_cast<int>(x) + opengold::por::step_x(side));
                const auto ny = map.wrapped_y(static_cast<int>(y) + opengold::por::step_y(side));
                // Every off-map step on this laboratory floor returns
                // to the mines; the gravity field is explicitly dangerous
                // until its actual control has been switched off.
                if (((area == 50 || area == 64 ||
                      (area == 81 && campaign.session.snapshot().script_id == 81)) &&
                     (static_cast<int>(x) + opengold::por::step_x(side) < 0 ||
                      static_cast<int>(y) + opengold::por::step_y(side) < 0 ||
                      static_cast<int>(x) + opengold::por::step_x(side) >=
                          static_cast<int>(map.width) ||
                      static_cast<int>(y) + opengold::por::step_y(side) >=
                          static_cast<int>(map.height))) ||
                    (area == 50 && (map.at(nx, ny).event_number() & 63) == 12 &&
                     campaign.session.script_variable(opengold::por::EclAddress{0x4C80}) != 1))
                {
                    continue;
                }
                const auto next = ny * map.width + nx;
                if (area == 84 && campaign.session.snapshot().script_id == 80 &&
                    campaign.prison_rearming && (map.at(nx, ny).event_number() & 63) == 12)
                {
                    continue; // Finish rearming before entering the power room.
                }
                if (parent[next] >= 0)
                {
                    continue;
                }
                parent[next] = static_cast<int>(here);
                via[next] = static_cast<int>(n);
                pending.push(next);
            }
        }
        check(parent[goal] >= 0, "An actual open path reaches the Matrix route target");
        auto next = goal;
        while (static_cast<unsigned>(parent[next]) != start)
        {
            next = static_cast<unsigned>(parent[next]);
        }
        const auto wanted = static_cast<Direction>(via[next]);
        if (controller == 96 && pose.x == 8 && pose.y == 10 && step < 4)
        {
            std::cout << "Actual hatch navigation facing " << unsigned(pose.facing) << ", wanted "
                      << unsigned(wanted) << ", walls";
            for (const auto wall : map.at(pose.x, pose.y).walls)
            {
                std::cout << ' ' << unsigned(wall);
            }
            std::cout << ", native heading "
                      << campaign.session.script_variable(opengold::por::EclAddress{0xC04D})
                      << ", ahead "
                      << campaign.session.script_variable(opengold::por::EclAddress{0xC04E})
                      << ".\n";
        }
        for (unsigned turns = 0; campaign.session.snapshot().pose.facing != wanted && turns < 4;
             ++turns)
        {
            const auto facing = campaign.session.snapshot().pose.facing;
            const auto turn =
                (static_cast<unsigned>(wanted) + 4 - static_cast<unsigned>(facing)) % 4 == 3
                    ? Command::turn_left
                    : Command::turn_right;
            check(campaign.session.explore(turn), "Turn through Matrix's player controls");
            if (!settle_route(campaign, folder, fights))
            {
                return false;
            }
        }
        if (controller == 96 && pose.x == 8 && pose.y == 10 && step < 4)
        {
            std::cout << "Actual hatch navigation before Forward facing "
                      << unsigned(campaign.session.snapshot().pose.facing) << ", native heading "
                      << campaign.session.script_variable(opengold::por::EclAddress{0xC04D})
                      << ", ahead "
                      << campaign.session.script_variable(opengold::por::EclAddress{0xC04E})
                      << ".\n";
        }
        // OPENGOLD_MATRIX_LED_ALLIES_SAVE=PATH saves the team one step east
        // of the surveillance room, facing it: the fight there has the prison
        // gang beside the team, which Leadership leads, for the combat
        // screen's check (tests/buck_led_allies_tests.gd).
        if (const auto *path = std::getenv("OPENGOLD_MATRIX_LED_ALLIES_SAVE");
            path && campaign.session.snapshot().area_id == 84 &&
            campaign.session.snapshot().pose.x == 14 && campaign.session.snapshot().pose.y == 0 &&
            campaign.session.snapshot().pose.facing == opengold::por::MapDirection::west)
        {
            opengold::write_save_file(
                path,
                opengold::encode_campaign(*campaign.party, &campaign.session,
                                          opengold::campaign_asset_identity(folder), matrix),
                16U * 1024U * 1024U);
            std::cout << "Actual led-allies fixture saved before the surveillance room.\n";
        }
        // OPENGOLD_MATRIX_LAST_STEP_SAVE=PATH saves the team before the step
        // that wins the game, facing it, for the running game's winner-bank
        // check (tests/winner_bank_capture_tests.gd).
        const auto *last_step_path = std::getenv("OPENGOLD_MATRIX_LAST_STEP_SAVE");
        const auto before_step =
            last_step_path && campaign.session.snapshot().area_id == 114 &&
                    campaign.session.can_leave()
                ? opengold::encode_campaign(*campaign.party, &campaign.session,
                                            opengold::campaign_asset_identity(folder), matrix)
                : std::string{};
        check(campaign.session.explore(Command::forward), "Walk through Matrix's actual map");
        if (!settle_route(campaign, folder, fights))
        {
            return false;
        }
        const auto &s = campaign.session.snapshot();
        std::cout << "Route step map " << s.area_id << " at " << s.pose.x << ',' << s.pose.y
                  << ".\n";
        if (s.game_won)
        {
            if (!before_step.empty())
            {
                opengold::write_save_file(last_step_path, before_step, 16U * 1024U * 1024U);
                std::cout << "Actual last step before the win saved.\n";
            }
            return true;
        }
        if (s.area_id != area || s.overland ||
            (campaign.malcoln_visit && s.script_id != controller))
        {
            return false;
        }
        // The real service event returns the party to its doorway. Its
        // receipt is checked by the route; standing inside is not required.
        if (service && next == goal)
        {
            return true;
        }
    }
    throw std::runtime_error("Actual Matrix path exceeded its movement bound");
}

bool walk_event_route(Campaign &campaign, const std::filesystem::path &folder, unsigned event,
                      unsigned &fights, bool service = false)
{
    const auto &map = campaign.session.map();
    const auto pose = campaign.session.snapshot().pose;
    std::set<std::pair<unsigned, unsigned>> reachable;
    std::queue<std::pair<unsigned, unsigned>> frontier;
    frontier.emplace(pose.x, pose.y);
    reachable.emplace(pose.x, pose.y);
    while (!frontier.empty())
    {
        const auto [x, y] = frontier.front();
        frontier.pop();
        const auto &cell = map.at(x, y);
        for (unsigned side = 0; side < 4; ++side)
        {
            if (cell.doors[side] > 1 || (cell.walls[side] && !cell.doors[side]))
            {
                continue;
            }
            const auto facing = static_cast<opengold::por::MapDirection>(side);
            const auto nx = map.wrapped_x(static_cast<int>(x) + opengold::por::step_x(facing));
            const auto ny = map.wrapped_y(static_cast<int>(y) + opengold::por::step_y(facing));
            if (reachable.emplace(nx, ny).second)
            {
                frontier.emplace(nx, ny);
            }
        }
    }
    for (unsigned y = 0; y < map.height; ++y)
    {
        for (unsigned x = 0; x < map.width; ++x)
        {
            if ((map.at(x, y).event_number() & 63) == event && reachable.contains({x, y}))
            {
                std::cout << "Actual Matrix map " << campaign.session.snapshot().area_id
                          << " event " << event << " at " << x << ',' << y << ".\n";
                return walk_route(campaign, folder, x, y, fights, service);
            }
        }
    }
    throw std::runtime_error("The actual map has no requested authored event " +
                             std::to_string(event));
}

bool interact_wall_route(Campaign &campaign, const std::filesystem::path &folder, unsigned x,
                         unsigned y, opengold::por::MapDirection facing, unsigned &fights)
{
    using Command = opengold::por::ExplorationCommand;
    if (!walk_route(campaign, folder, x, y, fights))
    {
        return false;
    }
    const auto controller = campaign.session.snapshot().script_id;

    struct RestorePassageIntent
    {
        bool &intent;
        bool prior;

        ~RestorePassageIntent()
        {
            intent = prior;
        }
    };

    const RestorePassageIntent restore{campaign.ship_passage,
                                       std::exchange(campaign.ship_passage, controller == 96 ||
                                                                                controller == 97 ||
                                                                                controller == 98)};
    while (campaign.session.snapshot().pose.facing != facing)
    {
        const auto current = campaign.session.snapshot().pose.facing;
        const auto turn =
            (static_cast<unsigned>(facing) + 4 - static_cast<unsigned>(current)) % 4 == 3
                ? Command::turn_left
                : Command::turn_right;
        const auto prior_pose = campaign.session.snapshot().pose;
        check(campaign.session.explore(turn), "Face the actual console wall");
        if (!settle_route(campaign, folder, fights))
        {
            return false;
        }
        const auto actual_pose = campaign.session.snapshot().pose;
        if (campaign.session.snapshot().script_id != controller || actual_pose.x != prior_pose.x ||
            actual_pose.y != prior_pose.y)
        {
            return true;
        }
    }
    check(campaign.session.explore(Command::forward),
          "Operate the actual panel through its authored pre-move event");
    return settle_route(campaign, folder, fights);
}

// Reuse Countdown's earned player's public navigation pattern. Predictions
// come from the existing shared Solar rules; every actual step runs the script.
bool fly_route(Campaign &campaign, const std::filesystem::path &folder, unsigned &fights,
               std::size_t body)
{
    using Command = opengold::por::ExplorationCommand;
    constexpr std::array<std::pair<Command, og::RocketHeading>, 8> flights{{
        {Command::north, og::RocketHeading::north},
        {Command::northeast, og::RocketHeading::northeast},
        {Command::east, og::RocketHeading::east},
        {Command::southeast, og::RocketHeading::southeast},
        {Command::south, og::RocketHeading::south},
        {Command::southwest, og::RocketHeading::southwest},
        {Command::west, og::RocketHeading::west},
        {Command::northwest, og::RocketHeading::northwest},
    }};
    unsigned moved = 0;
    for (unsigned step = 0; step < 100; ++step)
    {
        if (!settle_route(campaign, folder, fights, true))
        {
            return false;
        }
        const auto state = campaign.session.snapshot();
        if (state.phase == TourPhase::awaiting_continue)
        {
            if (state.script_id == 19 &&
                state.dialogue.find("USE THE CLINIC?") != std::string::npos)
            {
                if (!settle_route(campaign, folder, fights))
                {
                    return false;
                }
                continue;
            }
            auto choice = std::ranges::find(state.choices, "FLEE");
            if (choice == state.choices.end())
            {
                if (state.dialogue.find("MOVE INTO AN APPROACH PATTERN?") != std::string::npos &&
                    std::ranges::find(state.choices, "YES") != state.choices.end() &&
                    std::ranges::find(state.choices, "NO") != state.choices.end())
                {
                    choice = std::ranges::find(
                        state.choices, campaign.session.orbited_body() == body ? "YES" : "NO");
                    check(choice != state.choices.end(),
                          "The actual Earth approach prompt offers the chosen answer");
                    check(campaign.session.choose(
                              state.continue_ticket,
                              static_cast<std::size_t>(choice - state.choices.begin())),
                          "Answer the actual approach prompt before selecting a destination");
                    continue;
                }
                if (campaign.session.orbited_body() == body)
                {
                    std::cout << "Genuine Matrix Solar arrival at body " << body << " after "
                              << moved << " actual flight steps. Choices:";
                    for (const auto &label : state.choices)
                    {
                        std::cout << " [" << label << ']';
                    }
                    std::cout << '\n';
                    return true;
                }
                choice = std::ranges::find(state.choices, "EXIT");
            }
            check(choice != state.choices.end(),
                  "An intermediate Solar event offers an actual escape or Exit: " + state.dialogue);
            check(campaign.session.choose(state.continue_ticket,
                                          static_cast<std::size_t>(choice - state.choices.begin())),
                  "Choose the actual Solar event's escape or Exit");
            continue;
        }
        check(state.phase == TourPhase::completed && state.solar_map,
              "The actual Matrix rocket is idle on the shared Solar map");
        saved_campaign_round_trips(campaign, folder, false);
        const auto orbit = campaign.session.solar_map_state();
        const auto target = og::place_bodies(orbit.phase).at(body).cell;
        const auto distance = [target](og::OrbitCell cell)
        {
            const auto apart = [](unsigned a, unsigned b)
            {
                return a > b ? a - b : b - a;
            };
            return std::max(apart(cell.x, target.x), apart(cell.y, target.y));
        };
        auto best = flights.front();
        unsigned nearest = ~0U;
        for (const auto &flight : flights)
        {
            const auto reached = og::step_rocket(orbit, flight.second).rocket;
            if (reached != orbit.rocket && distance(reached) < nearest)
            {
                nearest = distance(reached);
                best = flight;
            }
        }
        const auto time_before = campaign.party->state().time_minutes;
        check(campaign.session.explore(best.first), "Fly through an actual Solar direction");
        ++moved;
        if (!settle_route(campaign, folder, fights, true))
        {
            return false;
        }
        check(campaign.party->state().time_minutes == time_before + 3 * 24 * 60,
              "An ordinary Solar step passes three actual native days");
        const auto reached = campaign.session.solar_map_state();
        std::cout << "Actual Matrix flight step " << moved << " rocket " << reached.rocket.x << ','
                  << reached.rocket.y << ", phase " << reached.phase << ".\n";
    }
    throw std::runtime_error("Actual Matrix Solar navigation exceeded its movement bound");
}

// The town-presence field is the first body difference from the same party's
// save without a town. Change only its following script field and recompute the
// real checksum, so negative cases exercise context validation, not corruption.
std::string saved_with_other_script(const std::string &saved, const std::string &party_only,
                                    unsigned script, unsigned previous_script = 32)
{
    const auto body_at = [](const std::string &bytes)
    {
        return bytes.find('\n', bytes.find('\n') + 1) + 1;
    };
    auto body = saved.substr(body_at(saved));
    const auto absent = party_only.substr(body_at(party_only));
    const auto different = std::mismatch(body.begin(), body.end(), absent.begin(), absent.end());
    const auto offset = static_cast<std::size_t>(different.first - body.begin());
    check(offset < body.size() && offset < absent.size() && body[offset] == '1' &&
              absent[offset] == '0',
          "The only first body difference is town presence");
    const auto start = body.find_first_not_of(" \r\n\t", offset + 1);
    const auto end = body.find_first_of(" \r\n\t", start);
    check(body.substr(start, end - start) == std::to_string(previous_script),
          "The actual retained-map save names its current script");
    body.replace(start, end - start, std::to_string(script));
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char byte : body)
    {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return saved.substr(0, saved.find('\n') + 1) + std::to_string(hash) + '\n' + body;
}

void retained_wilderness_save_checks(Campaign &campaign, const std::filesystem::path &folder,
                                     bool negative = true)
{
    const auto assets = opengold::campaign_asset_identity(folder);
    const auto x = campaign.session.script_variable(opengold::por::EclAddress{0x4BC3});
    const auto y = campaign.session.script_variable(opengold::por::EclAddress{0x4BC4});
    for (const unsigned format : {53U, 55U})
    {
        const auto saved = opengold::encode_campaign_in_format(*campaign.party, &campaign.session,
                                                               assets, format, matrix);
        auto fresh = opengold::load_campaign(folder, matrix);
        auto loaded = opengold::decode_campaign(saved, *campaign.creation,
                                                campaign.party->rule_module(), assets, &fresh);
        check(
            loaded.town &&
                loaded.town->snapshot().script_id == campaign.session.snapshot().script_id &&
                loaded.town->snapshot().area_id == campaign.session.snapshot().area_id &&
                loaded.town->script_variable(opengold::por::EclAddress{0x4BC3}) == x &&
                loaded.town->script_variable(opengold::por::EclAddress{0x4BC4}) == y,
            "An independently loaded wilderness keeps its controller, retained district and square");
        check(campaign.session.snapshot().overland && loaded.town->snapshot().overland &&
                  !loaded.town->snapshot().solar_map && loaded.town->snapshot().overland_x == x &&
                  loaded.town->snapshot().overland_y == y &&
                  loaded.town->snapshot().overland_direction ==
                      campaign.session.snapshot().overland_direction,
              "Both supported saves publish actual wilderness controls and coordinates");
        auto restored = std::make_shared<opengold::CampaignParty>(
            og::rules_module(opengold::load_original_content(folder, matrix)), campaign.creation);
        restored->restore(std::move(loaded.party));
        loaded.town->attach_restored_party(restored);
        check(opengold::encode_campaign_in_format(*restored, &*loaded.town, assets, format,
                                                  matrix) == saved,
              "A supported wilderness save resaves byte-identically");
        if (!negative)
        {
            continue;
        }
        const auto rejects_context = [&](const std::string &bytes, std::string_view label,
                                         std::initializer_list<std::string_view> reasons)
        {
            bool refused = false;
            try
            {
                (void)opengold::decode_campaign(bytes, *campaign.creation,
                                                campaign.party->rule_module(), assets, &fresh);
            }
            catch (const std::runtime_error &error)
            {
                check(std::ranges::find(reasons, std::string_view(error.what())) != reasons.end(),
                      std::string(label) + ": " + error.what());
                refused = true;
            }
            check(refused, std::string(label));
        };
        const auto party_only =
            opengold::encode_campaign_in_format(*campaign.party, nullptr, assets, format, matrix);
        rejects_context(
            saved_with_other_script(saved, party_only, 35, campaign.session.snapshot().script_id),
            "An unrelated program cannot use a wilderness kind to bypass district validation",
            {"Saved district and script disagree"});
        rejects_context(
            saved_with_other_script(saved, party_only,
                                    campaign.session.snapshot().script_id == 32 ? 48 : 32,
                                    campaign.session.snapshot().script_id),
            "A different known wilderness program cannot accept this controller's image",
            {"Script image size mismatch", "Invalid saved Matrix wilderness context"});
        auto changed = campaign.session.detached_copy();
        opengold::test::TourAccess::set_variable(changed, opengold::por::EclAddress{0x4BA6}, 0);
        const auto bad_mode =
            opengold::encode_campaign_in_format(*campaign.party, &changed, assets, format, matrix);
        rejects_context(bad_mode,
                        "An authenticated controller still requires wilderness picture mode",
                        {"Invalid saved Matrix wilderness context"});
        opengold::test::TourAccess::set_variable(changed, opengold::por::EclAddress{0x4BA6}, 4);
        opengold::test::TourAccess::set_variable(
            changed, opengold::por::EclAddress{0x4BE6},
            campaign.session.snapshot().script_id == 32 ? 0xA2 : 0xA8);
        rejects_context(
            opengold::encode_campaign_in_format(*campaign.party, &changed, assets, format, matrix),
            "Another wilderness kind does not authenticate this controller",
            {"Invalid saved Matrix wilderness context"});
        opengold::test::TourAccess::set_variable(changed, opengold::por::EclAddress{0x4BA6}, 0);
        opengold::test::TourAccess::set_variable(changed, opengold::por::EclAddress{0x4BE6}, 0x81);
        const auto ordinary =
            opengold::encode_campaign_in_format(*campaign.party, &changed, assets, format, matrix);
        rejects_context(saved_with_other_script(ordinary, party_only, 35,
                                                campaign.session.snapshot().script_id),
                        "Ordinary mismatched districts remain rejected",
                        {"Saved district and script disagree"});
        changed = campaign.session.detached_copy();
        opengold::test::TourAccess::set_variable(changed, opengold::por::EclAddress{0x4BF2},
                                                 campaign.session.snapshot().script_id == 32 ? 48
                                                                                             : 32);
        rejects_context(
            opengold::encode_campaign_in_format(*campaign.party, &changed, assets, format, matrix),
            "The restored VM identity must match the saved controller",
            {"Invalid saved Matrix wilderness context"});
    }
}

void retained_interior_save_checks(Campaign &campaign, const std::filesystem::path &folder,
                                   unsigned controller = 37, unsigned district = 33)
{
    const auto assets = opengold::campaign_asset_identity(folder);
    for (const unsigned format : {53U, 55U})
    {
        const auto saved = opengold::encode_campaign_in_format(*campaign.party, &campaign.session,
                                                               assets, format, matrix);
        const auto absent =
            opengold::encode_campaign_in_format(*campaign.party, nullptr, assets, format, matrix);
        auto fresh = opengold::load_campaign(folder, matrix);
        auto loaded = opengold::decode_campaign(saved, *campaign.creation,
                                                campaign.party->rule_module(), assets, &fresh);
        check(loaded.town && loaded.town->snapshot().script_id == controller &&
                  loaded.town->snapshot().area_id == district &&
                  loaded.town->snapshot().pose == campaign.session.snapshot().pose,
              "Both supported saves keep the actual retained-floor controller and shared map");
        auto restored = std::make_shared<opengold::CampaignParty>(
            og::rules_module(opengold::load_original_content(folder, matrix)), campaign.creation);
        restored->restore(std::move(loaded.party));
        loaded.town->attach_restored_party(restored);
        for (const auto &member : campaign.party->state().roster)
        {
            check(restored->game_record(member.id) == campaign.party->game_record(member.id),
                  "The retained floor keeps every actual party record");
        }
        check(opengold::encode_campaign_in_format(*restored, &*loaded.town, assets, format,
                                                  matrix) == saved,
              "The independently loaded retained floor resaves byte-identically");
        bool refused = false;
        try
        {
            const auto changed = saved_with_other_script(saved, absent, 35, controller);
            (void)opengold::decode_campaign(changed, *campaign.creation,
                                            campaign.party->rule_module(), assets, &fresh);
        }
        catch (const std::runtime_error &error)
        {
            check(std::string_view(error.what()) == "Saved district and script disagree",
                  "The narrow shared-map association preserves ordinary mismatch rejection");
            refused = true;
        }
        check(refused, "A checksum-valid unrelated script cannot claim the retained floor's map");
    }
}

bool take_sprawl_stairs(Campaign &campaign, const std::filesystem::path &folder, unsigned &fights,
                        unsigned x, unsigned y, unsigned wall, unsigned controller)
{
    if (!walk_route(campaign, folder, x, y, fights))
    {
        return false;
    }
    const auto stairs = campaign.session.map().at(x, y);
    for (unsigned side = 0; side < 4; ++side)
    {
        if (stairs.walls[side] == wall)
        {
            (void)interact_wall_route(campaign, folder, x, y,
                                      static_cast<opengold::por::MapDirection>(side), fights);
            return campaign.session.snapshot().script_id == controller;
        }
    }
    throw std::runtime_error("The actual sprawl staircase has no authored barrier");
}

bool return_for_salvation_autodoc(Campaign &campaign, const std::filesystem::path &folder,
                                  unsigned &fights, bool return_to_warren = true)
{
    campaign.leaving_warren = true;
    if (campaign.session.snapshot().script_id == 33)
    {
        check(take_sprawl_stairs(campaign, folder, fights, 12, 0, 11, 32),
              "Leave the warren through its actual tunnel before medical recovery");
    }
    else if (campaign.session.snapshot().script_id == 34)
    {
        campaign.leaving_museum = true;
        if (!interact_wall_route(campaign, folder, 7, 15, opengold::por::MapDirection::south,
                                 fights))
        {
            return false;
        }
    }
    check(campaign.session.snapshot().script_id == 32,
          "The actual exit reaches the ordinary wilderness before flying to Salvation");
    using Command = opengold::por::ExplorationCommand;
    for (unsigned step = 0; step < 64; ++step)
    {
        const auto state = campaign.session.snapshot();
        check(state.script_id == 32, "The medical retreat stays on the actual wilderness");
        const auto x = state.overland_x, y = state.overland_y;
        std::cout << "Actual medical wilderness square " << x << ',' << y << ", native "
                  << campaign.session.script_variable(opengold::por::EclAddress{0x4BC3}) << ','
                  << campaign.session.script_variable(opengold::por::EclAddress{0x4BC4}) << ".\n";
        const auto move = x == 20 && y == 5 ? Command::north
                          : x > 20          ? Command::west
                          : x < 20          ? Command::east
                          : y > 5           ? Command::north
                                            : Command::south;
        check(campaign.session.explore(move), "Walk back to the real Duke's Hill spaceport");
        if (!settle_route(campaign, folder, fights, false, true))
        {
            return false;
        }
        if (campaign.session.snapshot().phase == TourPhase::awaiting_continue)
        {
            break;
        }
    }
    auto state = campaign.session.snapshot();
    const auto yes = std::ranges::find(state.choices, "YES");
    check(state.dialogue.find("LEAVE EARTH") != std::string::npos && yes != state.choices.end(),
          "The actual spaceport offers departure for medical care");
    check(campaign.session.choose(state.continue_ticket,
                                  static_cast<std::size_t>(yes - state.choices.begin())),
          "Launch from the real world port");
    if (!settle_route(campaign, folder, fights, true) || !fly_route(campaign, folder, fights, 2))
    {
        return false;
    }
    state = campaign.session.snapshot();
    const auto salvation = std::ranges::find(state.choices, "SALVATION");
    check(salvation != state.choices.end(), "The actual Earth menu offers Salvation's clinic");
    check(campaign.session.choose(state.continue_ticket,
                                  static_cast<std::size_t>(salvation - state.choices.begin())),
          "Enter Salvation for its actual autodoc");
    if (!settle_route(campaign, folder, fights))
    {
        return false;
    }
    for (const auto &member : campaign.party->state().roster)
    {
        const auto record = og::buck_rogers_record_of(campaign.party->game_record(member.id));
        check(record && record->creature.status == 1 &&
                  record->creature.hit_points == record->creature.hit_point_maximum,
              "Salvation's actual autodoc restores every real crew member");
    }
    std::cout << "Actual Salvation medical retreat restores the complete crew.\n";
    saved_campaign_round_trips(campaign, folder, false);
    campaign.leaving_museum = false;
    if (!return_to_warren)
    {
        campaign.leaving_warren = false;
        return true;
    }
    if (!fly_route(campaign, folder, fights, 2))
    {
        return false;
    }
    campaign.leaving_warren = false;
    state = campaign.session.snapshot();
    const auto city = std::ranges::find(state.choices, "LOSANGELORG");
    check(city != state.choices.end(), "Return to the assigned Losangelorg rescue");
    check(campaign.session.choose(state.continue_ticket,
                                  static_cast<std::size_t>(city - state.choices.begin())),
          "Land again at the actual Losangelorg port");
    if (!settle_route(campaign, folder, fights))
    {
        return false;
    }
    for (unsigned step = 0; campaign.session.snapshot().script_id == 32 && step < 64; ++step)
    {
        const auto state = campaign.session.snapshot();
        const auto move = state.overland_y > 0 ? Command::northeast : Command::east;
        check(campaign.session.explore(move), "Walk back to the walled sprawl");
        if (!settle_route(campaign, folder, fights))
        {
            return false;
        }
    }
    check(campaign.session.snapshot().script_id == 33, "Re-enter the real warren after treatment");
    return true;
}

bool look_toward_route(Campaign &campaign, const std::filesystem::path &folder, unsigned &fights,
                       opengold::por::MapDirection facing)
{
    using Command = opengold::por::ExplorationCommand;
    while (campaign.session.snapshot().pose.facing != facing)
    {
        const auto current = campaign.session.snapshot().pose.facing;
        const auto turn =
            (static_cast<unsigned>(facing) + 4 - static_cast<unsigned>(current)) % 4 == 3
                ? Command::turn_left
                : Command::turn_right;
        check(campaign.session.explore(turn), "Face the authored room display");
        if (!settle_route(campaign, folder, fights))
        {
            return false;
        }
    }
    check(campaign.session.explore(Command::look),
          "Read the actual display in the indicated direction");
    return settle_route(campaign, folder, fights);
}

bool ordinary_recovery_camp(Campaign &campaign, const std::filesystem::path &folder,
                            unsigned &fights, bool carry_comatose = false)
{
    std::map<opengold::MemberId, std::string> coma_records;
    for (unsigned day = 0; day < 100; ++day)
    {
        bool wounded = false;
        for (const auto id : campaign.party->state().slots)
        {
            if (!id)
            {
                continue;
            }
            const auto record = og::buck_rogers_record_of(campaign.party->game_record(id));
            check(record && record->creature.status != 2 && record->creature.status != 3 &&
                      (record->creature.status != 8 || carry_comatose),
                  "Ordinary rest cannot cure coma; seek actual medical treatment");
            if (record->creature.status == 8)
            {
                coma_records.try_emplace(id, og::encode_ground_record(*record));
            }
            wounded = wounded || ((record->creature.status == 1 || record->creature.status == 5) &&
                                  record->creature.hit_points < record->creature.hit_point_maximum);
        }
        if (!wounded)
        {
            break;
        }
        const auto before = campaign.party->state().time_minutes;
        check(campaign.session.camp(opengold::RestKind::long_rest),
              "Use the existing ordinary Camp transaction for the wounded crew");
        if (!settle_route(campaign, folder, fights))
        {
            return false;
        }
        check(campaign.party->state().time_minutes == before + 24 * 60,
              "Each accepted native Camp spends one actual day");
        std::cout << "Actual native recovery Camp day " << day + 1 << ".\n";
    }
    for (const auto &[id, before] : coma_records)
    {
        const auto record = og::buck_rogers_record_of(campaign.party->game_record(id));
        check(record && record->creature.status == 8 && og::encode_ground_record(*record) == before,
              "Ordinary Camp preserves the comatose member's complete record without curing it");
        std::cout << "Actual Camp leaves " << record->creature.name
                  << " comatose; authored medical treatment is still required.\n";
    }
    return true;
}

bool walk_venus_world(Campaign &campaign, const std::filesystem::path &folder, unsigned goal_x,
                      unsigned goal_y, unsigned &fights)
{
    using Command = opengold::por::ExplorationCommand;
    constexpr unsigned width = 38, height = 15;
    constexpr std::array<int, 8> dx{0, 1, 1, 1, 0, -1, -1, -1}, dy{-1, -1, 0, 1, 1, 1, 0, -1};
    constexpr std::array commands{Command::north,     Command::northeast, Command::east,
                                  Command::southeast, Command::south,     Command::southwest,
                                  Command::west,      Command::northwest};
    const auto program = opengold::test::TourAccess::program(campaign.session, 48);
    const auto memory = program->memory();
    const auto terrain = [&](unsigned x, unsigned y)
    {
        return program->raw().at(0x83AE + y * width + x - memory.origin +
                                 memory.record_header_size);
    };
    for (unsigned step = 0; step < 256; ++step)
    {
        const auto state = campaign.session.snapshot();
        if (state.script_id != 48)
        {
            return true;
        }
        const auto x = state.overland_x, y = state.overland_y;
        check(x < width && y < height, "The actual Venus party stays inside its installed map");
        if (x == goal_x && y == goal_y)
        {
            return true;
        }
        std::vector<int> parent(width * height, -1), via(width * height, -1);
        std::queue<unsigned> frontier;
        const unsigned start = y * width + x, goal = goal_y * width + goal_x;
        parent[start] = static_cast<int>(start);
        frontier.push(start);
        while (!frontier.empty() && parent[goal] < 0)
        {
            const auto at = frontier.front();
            frontier.pop();
            for (unsigned side = 0; side < 8; ++side)
            {
                const int nx = static_cast<int>(at % width) + dx[side],
                          ny = static_cast<int>(at / width) + dy[side];
                if (nx < 0 || ny < 0 || nx >= static_cast<int>(width) ||
                    ny >= static_cast<int>(height))
                {
                    continue;
                }
                const unsigned next = static_cast<unsigned>(ny) * width + static_cast<unsigned>(nx);
                // The authored terrain4 is a sulfuric-acid lake.
                if (parent[next] >= 0 ||
                    terrain(static_cast<unsigned>(nx), static_cast<unsigned>(ny)) == 4)
                {
                    continue;
                }
                parent[next] = static_cast<int>(at);
                via[next] = static_cast<int>(side);
                frontier.push(next);
            }
        }
        check(parent[goal] >= 0,
              "A real Venus path reaches the destination without entering acid lakes");
        unsigned next = goal;
        while (static_cast<unsigned>(parent[next]) != start)
        {
            next = static_cast<unsigned>(parent[next]);
        }
        check(campaign.session.explore(commands[static_cast<unsigned>(via[next])]),
              "Walk through the actual Venus wilderness controls");
        if (!settle_route(campaign, folder, fights))
        {
            return false;
        }
        if (campaign.session.snapshot().script_id == 48 &&
            !ordinary_recovery_camp(campaign, folder, fights))
        {
            return false;
        }
        std::cout << "Actual Venus step " << campaign.session.snapshot().overland_x << ','
                  << campaign.session.snapshot().overland_y << ", controller "
                  << campaign.session.snapshot().script_id << ".\n";
    }
    throw std::runtime_error("Actual Venus walk exceeded its movement bound");
}

bool play_earned_route(Campaign &campaign, const std::filesystem::path &folder,
                       bool preparation_only, bool losangelorg_only)
{
    unsigned fights = 0;
    if (!walk_route(campaign, folder, 3, 5, fights, true) ||
        !walk_route(campaign, folder, 4, 2, fights) ||
        !walk_route(campaign, folder, 4, 5, fights, true))
    {
        return false;
    }
    for (const auto &member : campaign.party->state().roster)
    {
        const auto record = og::buck_rogers_record_of(campaign.party->game_record(member.id));
        check(record && record->creature.hit_points == record->creature.hit_point_maximum &&
                  record->creature.status == 1,
              "The actual Caloris autodoc restores every wounded living member");
    }
    std::cout << "All six actual Matrix members healed by the Caloris autodoc.\n";
    saved_campaign_round_trips(campaign, folder, false);
    if (preparation_only)
    {
        std::cout << "Genuine Caloris preparation: " << fights
                  << " real fights, six actual living healed members and paid equipment.\n";
        return true;
    }
    if (!walk_route(campaign, folder, 11, 4, fights) ||
        !interact_wall_route(campaign, folder, 11, 5, opengold::por::MapDirection::west, fights))
    {
        return false;
    }
    check((opengold::test::TourAccess::machine(campaign.session)
               .variable(opengold::por::EclAddress{0x4C2E}) &
           1) != 0 &&
              opengold::test::TourAccess::machine(campaign.session)
                      .variable(opengold::por::EclAddress{0x4C07}) == 0,
          "The actual courtesy console summons the team to the coronation");
    if (!walk_route(campaign, folder, 2, 12, fights, true))
    {
        return false;
    }
    std::cout << "Genuine Matrix Caloris route reached the coronation hall after " << fights
              << " real fights.\n";
    saved_campaign_round_trips(campaign, folder, campaign.session.snapshot().game_won);
    if (!walk_route(campaign, folder, 5, 14, fights, true) ||
        !walk_route(campaign, folder, 5, 10, fights, true) ||
        !walk_route(campaign, folder, 4, 5, fights, true) ||
        !interact_wall_route(campaign, folder, 11, 5, opengold::por::MapDirection::west, fights))
    {
        return false;
    }
    std::optional<opengold::por::PartyPose> berth_a;
    for (unsigned y = 0; y < campaign.session.map().height; ++y)
    {
        for (unsigned x = 0; x < campaign.session.map().width; ++x)
        {
            // ECL17's authored floor dispatcher sends event6 to Berth A.
            if ((campaign.session.map().at(x, y).event_number() & 63) == 6)
            {
                berth_a = opengold::por::PartyPose{x, y};
            }
        }
    }
    check(berth_a.has_value(), "Caloris's actual map locates Berth A");
    if (!walk_route(campaign, folder, berth_a->x, berth_a->y, fights, true))
    {
        return false;
    }
    check(campaign.session.script_variable(opengold::por::EclAddress{0x4C2C}) == 3,
          "The actual Berth A confrontation advances Caloris's authored mission");
    std::optional<opengold::por::PartyPose> ship_door;
    const auto &map = campaign.session.map();
    for (unsigned y = 0; y < map.height; ++y)
    {
        for (unsigned x = 0; x < map.width; ++x)
        {
            const auto &cell = map.at(x, y);
            if ((cell.event_number() & 63) == 1)
            {
                for (unsigned direction = 0; direction < 4; ++direction)
                {
                    if (cell.walls[direction] == 6)
                    {
                        ship_door = opengold::por::PartyPose{
                            x, y, static_cast<opengold::por::MapDirection>(direction)};
                    }
                }
            }
        }
    }
    check(ship_door.has_value(), "Caloris's actual map locates the team's airlock");
    if (!interact_wall_route(campaign, folder, ship_door->x, ship_door->y, ship_door->facing,
                             fights))
    {
        return false;
    }
    std::cout << "Genuine Matrix departure: map " << campaign.session.snapshot().area_id
              << ", script " << campaign.session.snapshot().script_id << ", Solar "
              << campaign.session.snapshot().solar_map << ".\n";
    saved_campaign_round_trips(campaign, folder, campaign.session.snapshot().game_won);
    if (!fly_route(campaign, folder, fights, 4))
    {
        return false;
    }
    const auto arrival = campaign.session.snapshot();
    const auto base = std::ranges::find(arrival.choices, "RAM BASE");
    check(base != arrival.choices.end(),
          "Ceres's actual arrival menu offers the assigned RAM base");
    check(campaign.session.choose(arrival.continue_ticket,
                                  static_cast<std::size_t>(base - arrival.choices.begin())),
          "Take the actual RAM BASE destination");
    if (!settle_route(campaign, folder, fights))
    {
        return false;
    }
    const auto entered = campaign.session.snapshot();
    std::cout << "Genuine Matrix RAM base entry: map " << entered.area_id << ", script "
              << entered.script_id << ", pose " << entered.pose.x << ',' << entered.pose.y
              << ", Solar " << entered.solar_map << ".\n";
    saved_campaign_round_trips(campaign, folder, entered.game_won);
    check(entered.area_id == 35 && entered.script_id == 35 && !entered.solar_map,
          "The actual assigned Ceres RAM base is map/script35");
    if (!walk_event_route(campaign, folder, 36, fights))
    {
        return false;
    }
    check((campaign.session.script_variable(opengold::por::EclAddress{0x4CD8}) & 1) != 0,
          "The actual guard quarters confrontation earns disguises and the security pass");
    std::cout << "Genuine Matrix Ceres security pass after " << fights << " real fights.\n";
    saved_campaign_round_trips(campaign, folder, false);
    if (!walk_event_route(campaign, folder, 20, fights) ||
        !walk_event_route(campaign, folder, 35, fights))
    {
        return false;
    }
    check(
        campaign.session.script_variable(opengold::por::EclAddress{0x4C0C}) == 1 &&
            (campaign.session.script_variable(opengold::por::EclAddress{0x4CDB}) & 8) != 0 &&
            (campaign.session.script_variable(opengold::por::EclAddress{0x4CD8}) & 0x80) != 0,
        "The actual armory and security footage provide explosives, the conversation and keycode");
    saved_campaign_round_trips(campaign, folder, false);
    if (!walk_event_route(campaign, folder, 31, fights) ||
        !walk_event_route(campaign, folder, 29, fights))
    {
        return false;
    }
    check(campaign.session.script_variable(opengold::por::EclAddress{0x4CD9}) == 4,
          "The actual laser-room confrontation earns the four remaining crystals");
    std::cout << "Genuine Matrix Ceres crystals after " << fights << " real fights.\n";
    saved_campaign_round_trips(campaign, folder, false);
    for (unsigned look = 0;
         !(campaign.session.script_variable(opengold::por::EclAddress{0x4CD6}) & 0x80) && look < 12;
         ++look)
    {
        check(campaign.session.explore(opengold::por::ExplorationCommand::look),
              "Check the actual sensors for the orbiting cruiser");
        if (!settle_route(campaign, folder, fights))
        {
            return false;
        }
    }
    check((campaign.session.script_variable(opengold::por::EclAddress{0x4CD6}) & 0x80) != 0,
          "The actual communications sensors identify the orbiting RAM cruiser");
    if (!walk_event_route(campaign, folder, 34, fights) ||
        !walk_event_route(campaign, folder, 33, fights))
    {
        return false;
    }
    check((campaign.session.script_variable(opengold::por::EclAddress{0x4CD8}) & 0x20) != 0,
          "The actual crystal, coordinates, charge and discharge destroy the cruiser");
    campaign.ready_for_ceres_demolition = true;
    if (!walk_event_route(campaign, folder, 34, fights))
    {
        return false;
    }
    check(campaign.session.script_variable(opengold::por::EclAddress{0x4C0C}) == 2,
          "The actual won explosives are set on the laser's thirty-command timer");
    if (!interact_wall_route(campaign, folder, 4, 15, opengold::por::MapDirection::south, fights))
    {
        return false;
    }
    std::cout << "Genuine Matrix Ceres departure: script " << campaign.session.snapshot().script_id
              << ", Solar " << campaign.session.snapshot().solar_map << ", remaining crystals "
              << campaign.session.script_variable(opengold::por::EclAddress{0x4CD9}) << ".\n";
    saved_campaign_round_trips(campaign, folder, campaign.session.snapshot().game_won);
    if (!fly_route(campaign, folder, fights, 2))
    {
        return false;
    }
    const auto earth = campaign.session.snapshot();
    const auto salvation = std::ranges::find(earth.choices, "SALVATION");
    check(salvation != earth.choices.end(), "Earth's actual arrival offers Salvation Station");
    check(campaign.session.choose(earth.continue_ticket,
                                  static_cast<std::size_t>(salvation - earth.choices.begin())),
          "Return to the real Salvation briefing");
    if (!settle_route(campaign, folder, fights))
    {
        return false;
    }
    std::cout << "Genuine Matrix post-Ceres report: script "
              << campaign.session.snapshot().script_id << ", Solar "
              << campaign.session.snapshot().solar_map << ".\n";
    saved_campaign_round_trips(campaign, folder, campaign.session.snapshot().game_won);
    if (!fly_route(campaign, folder, fights, 2))
    {
        return false;
    }
    const auto return_to_earth = campaign.session.snapshot();
    const auto city = std::ranges::find(return_to_earth.choices, "LOSANGELORG");
    check(city != return_to_earth.choices.end(), "The real assignment unlocks Losangelorg");
    check(campaign.session.choose(return_to_earth.continue_ticket,
                                  static_cast<std::size_t>(city - return_to_earth.choices.begin())),
          "Take the actual Losangelorg destination");
    if (!settle_route(campaign, folder, fights))
    {
        return false;
    }
    const auto arrived = campaign.session.snapshot();
    std::cout << "Genuine Matrix Losangelorg entry: script " << arrived.script_id << ", map "
              << arrived.area_id << ", pose " << arrived.pose.x << ',' << arrived.pose.y
              << ", Solar " << arrived.solar_map << ".\n";
    retained_wilderness_save_checks(campaign, folder);
    const auto x = campaign.session.script_variable(opengold::por::EclAddress{0x4BC3});
    const auto y = campaign.session.script_variable(opengold::por::EclAddress{0x4BC4});
    check(campaign.session.explore(opengold::por::ExplorationCommand::north),
          "Take one actual wilderness step after the earned Losangelorg arrival");
    if (!settle_route(campaign, folder, fights))
    {
        return false;
    }
    check(campaign.session.snapshot().script_id == 32 &&
              campaign.session.script_variable(opengold::por::EclAddress{0x4BC3}) == x &&
              campaign.session.script_variable(opengold::por::EclAddress{0x4BC4}) + 1 == y,
          "The ordinary wilderness step changes the actual world square");
    retained_wilderness_save_checks(campaign, folder, false);
    std::cout << "Earned Losangelorg entry and ordinary step retain format53/55 saves.\n";
    if (losangelorg_only)
    {
        if (const auto *directory = std::getenv("OPENGOLD_MATRIX_PROOF_DIR"))
        {
            const auto proof = std::filesystem::path(directory);
            std::filesystem::create_directories(proof);
            const auto saved =
                opengold::encode_campaign(*campaign.party, &campaign.session,
                                          opengold::campaign_asset_identity(folder), matrix);
            const auto path = proof / "matrix-losangelorg-earned.ogs";
            opengold::write_save_file(path, saved, 16U * 1024U * 1024U);
            std::cout << "Actual Losangelorg save: " << path << ".\n";
        }
    }
    if (losangelorg_only)
    {
        return true;
    }
    // The actual controller's native map table has the walled-sprawl opening
    // at (33,0). Walk there with the same eight-direction public controls.
    while (campaign.session.snapshot().script_id == 32)
    {
        const auto world_x = campaign.session.snapshot().overland_x;
        const auto world_y = campaign.session.snapshot().overland_y;
        if (world_x == 33 && world_y == 0)
        {
            break;
        }
        using Command = opengold::por::ExplorationCommand;
        const auto direction = world_y > 0 ? Command::northeast : Command::east;
        check(campaign.session.explore(direction), "Walk toward the actual walled sprawl");
        if (!settle_route(campaign, folder, fights))
        {
            return false;
        }
        std::cout << "Actual Losangelorg world step: " << campaign.session.snapshot().overland_x
                  << ',' << campaign.session.snapshot().overland_y << ", controller "
                  << campaign.session.snapshot().script_id << ".\n";
        if (campaign.session.snapshot().script_id == 32)
        {
            retained_wilderness_save_checks(campaign, folder, false);
        }
    }
    std::cout << "Actual sprawl route reached controller " << campaign.session.snapshot().script_id
              << ", map " << campaign.session.snapshot().area_id << ".\n";
    if (campaign.session.snapshot().script_id == 33)
    {
        std::cout << "Actual sprawl interior pose " << campaign.session.snapshot().pose.x << ','
                  << campaign.session.snapshot().pose.y << ".\n";
        for (const unsigned event : {5U, 11U, 12U, 11U})
        {
            if (!walk_event_route(campaign, folder, event, fights))
            {
                return false;
            }
        }
        if (std::ranges::any_of(campaign.party->state().roster,
                                [&](const auto &member)
                                {
                                    const auto record = og::buck_rogers_record_of(
                                        campaign.party->game_record(member.id));
                                    return record && record->creature.status == 8;
                                }) &&
            !return_for_salvation_autodoc(campaign, folder, fights))
        {
            return false;
        }
        std::cout << "Actual sprawl story: stage "
                  << campaign.session.script_variable(opengold::por::EclAddress{0x4C57})
                  << ", weapons "
                  << campaign.session.script_variable(opengold::por::EclAddress{0x4C55}) << ", key "
                  << campaign.session.script_variable(opengold::por::EclAddress{0x4C5C}) << ".\n";
        check(take_sprawl_stairs(campaign, folder, fights, 15, 5, 2, 37),
              "Use the authored stairs to the next sprawl controller");
        retained_interior_save_checks(campaign, folder);
        std::cout << "Actual sprawl stairs reached script " << campaign.session.snapshot().script_id
                  << ", map " << campaign.session.snapshot().area_id << ", pose "
                  << campaign.session.snapshot().pose.x << ',' << campaign.session.snapshot().pose.y
                  << ".\n";
        for (const unsigned event : {5U, 7U, 8U})
        {
            if (!walk_event_route(campaign, folder, event, fights))
            {
                return false;
            }
        }
        check(take_sprawl_stairs(campaign, folder, fights, 0, 10, 1, 33),
              "Return to the basement through its actual stairs");
        if (!walk_event_route(campaign, folder, 17, fights))
        {
            return false;
        }
        std::cout << "Actual Loa-Loa rescue status "
                  << campaign.session.script_variable(opengold::por::EclAddress{0x4C58}) << ".\n";
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C58}) & 0x40) != 0,
              "The real rescue earns Loa-Loa's request to capture Gargoyle");
        if (std::ranges::any_of(campaign.party->state().roster,
                                [&](const auto &member)
                                {
                                    const auto record = og::buck_rogers_record_of(
                                        campaign.party->game_record(member.id));
                                    return record && record->creature.status == 8;
                                }) &&
            !return_for_salvation_autodoc(campaign, folder, fights))
        {
            return false;
        }
        check(take_sprawl_stairs(campaign, folder, fights, 15, 5, 2, 37),
              "Follow Gargoyle upstairs through the original staircase");
        if (!walk_event_route(campaign, folder, 13, fights))
        {
            return false;
        }
        std::cout << "Actual Gargoyle capture flags "
                  << campaign.session.script_variable(opengold::por::EclAddress{0x4C59}) << ".\n";
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C59}) & 0x40) != 0,
              "The offered capture and interrogation send Gargoyle to Loa-Loa");
        check(take_sprawl_stairs(campaign, folder, fights, 0, 10, 1, 33),
              "Bring the actual captive back through the original staircase");
        if (!walk_event_route(campaign, folder, 11, fights))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C5C}) & 0x40) != 0,
              "Loa-Loa's actual interrogation earns the original cell key");
        check(take_sprawl_stairs(campaign, folder, fights, 15, 5, 2, 37),
              "Return upstairs with the earned key");
        if (!walk_event_route(campaign, folder, 17, fights))
        {
            return false;
        }
        for (unsigned look = 0;
             !(campaign.session.script_variable(opengold::por::EclAddress{0x4C5C}) & 2) && look < 8;
             ++look)
        {
            check(campaign.session.explore(opengold::por::ExplorationCommand::look),
                  "Search Chade's actual cell for his hidden belongings");
            if (!settle_route(campaign, folder, fights))
            {
                return false;
            }
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C5C}) & 2) != 0,
              "The actual cell search earns Chade's device and desert clue");
        std::cout << "Actual Chade device recovered after " << campaign.ground_victories
                  << " ground victories.\n";
        saved_campaign_round_trips(campaign, folder, false);
        check(take_sprawl_stairs(campaign, folder, fights, 0, 10, 1, 33),
              "Return from Chade's cell to the actual warren exit floor");
        if (training_available(campaign))
        {
            std::cout << "Actual earned advancement is available before the museum mission.\n";
            if (!return_for_salvation_autodoc(campaign, folder, fights))
            {
                return false;
            }
        }
        campaign.leaving_warren = true;
        check(take_sprawl_stairs(campaign, folder, fights, 12, 0, 11, 32),
              "Leave the warren with its actual device and desert clue");
        for (unsigned step = 0; campaign.session.snapshot().script_id == 32 && step < 80; ++step)
        {
            const auto state = campaign.session.snapshot();
            const auto x = state.overland_x, y = state.overland_y;
            if (x == 24 && y == 12)
            {
                break;
            }
            using Command = opengold::por::ExplorationCommand;
            const auto move = y < 12 && x > 24   ? Command::southwest
                              : y < 12 && x < 24 ? Command::southeast
                              : y > 12 && x > 24 ? Command::northwest
                              : y > 12 && x < 24 ? Command::northeast
                              : y < 12           ? Command::south
                              : y > 12           ? Command::north
                              : x > 24           ? Command::west
                                                 : Command::east;
            check(campaign.session.explore(move), "Walk across the actual desert toward the mesa");
            if (!settle_route(campaign, folder, fights))
            {
                return false;
            }
            std::cout << "Actual desert step " << campaign.session.snapshot().overland_x << ','
                      << campaign.session.snapshot().overland_y << ", controller "
                      << campaign.session.snapshot().script_id << ".\n";
        }
        check(campaign.session.snapshot().script_id == 34 &&
                  (campaign.session.script_variable(opengold::por::EclAddress{0x4C4B}) & 4) != 0,
              "The earned device dispels the mesa hologram and reveals the actual museum");
        std::cout << "Actual hidden museum reached after " << campaign.ground_victories
                  << " ground victories.\n";
        saved_campaign_round_trips(campaign, folder, false);
        // The installed museum has a real explosives exhibit (event37),
        // whose authored treasure supplies three explosive grenades.
        if (!walk_event_route(campaign, folder, 37, fights))
        {
            return false;
        }
        // Explore the actual exhibits before involving the professor.
        // The museum's authored patrols are finite; their genuine loot can
        // supply the field armor needed against the coming armed rescue.
        const auto field_suits = [&]()
        {
            unsigned count = 0;
            for (const auto id : campaign.party->state().slots)
            {
                if (!id)
                {
                    continue;
                }
                const auto record = og::buck_rogers_record_of(campaign.party->game_record(id));
                if (record && std::ranges::any_of(record->items,
                                                  [](const auto &item)
                                                  {
                                                      return item.type == 24;
                                                  }))
                {
                    ++count;
                }
            }
            return count;
        };
        const std::array<std::pair<unsigned, unsigned>, 20> exhibit_floor{
            {{5, 8},   {5, 9},   {4, 8},  {6, 8},  {4, 10},  {6, 10},  {1, 10},
             {0, 8},   {0, 10},  {0, 13}, {1, 12}, {14, 10}, {14, 12}, {14, 9},
             {15, 10}, {15, 12}, {10, 8}, {9, 10}, {9, 9},   {11, 9}}};
        for (unsigned tour = 0;
             tour < 3 && field_suits() < 6 &&
             campaign.session.script_variable(opengold::por::EclAddress{0x4C5E}) < 9;
             ++tour)
        {
            for (const auto [x, y] : exhibit_floor)
            {
                if (!walk_route(campaign, folder, x, y, fights))
                {
                    return false;
                }
                if (field_suits() == 6 ||
                    campaign.session.script_variable(opengold::por::EclAddress{0x4C5E}) >= 9)
                {
                    break;
                }
            }
            std::cout << "Actual museum patrol preparation: " << field_suits()
                      << " occupied members own field armor, counter "
                      << campaign.session.script_variable(opengold::por::EclAddress{0x4C5E})
                      << ".\n";
        }
        // Map34 also stores an upstairs closet with the same event7.
        // This is the actual ground-floor room already encountered in play.
        if (!walk_route(campaign, folder, 8, 5, fights))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C61}) & 4) != 0,
              "The actual professor agrees to come after collecting his notes");
        for (const auto [event, facing] : {std::pair{28U, opengold::por::MapDirection::south},
                                           std::pair{29U, opengold::por::MapDirection::north},
                                           std::pair{30U, opengold::por::MapDirection::east}})
        {
            if (!walk_event_route(campaign, folder, event, fights) ||
                !look_toward_route(campaign, folder, fights, facing))
            {
                return false;
            }
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C5F}) & 0x40) != 0,
              "The real theater, genetics and computer search leads to Chade's daughter");
        std::cout << "Actual professor's daughter search begins after " << campaign.ground_victories
                  << " ground victories at " << campaign.session.snapshot().pose.x << ','
                  << campaign.session.snapshot().pose.y << ".\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (!walk_event_route(campaign, folder, 32, fights))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C60}) & 0x20) != 0,
              "The actual rescue reunites Stefi and Chade and reveals the hidden door");
        // The closet's south edge is the barricade. Walk north once
        // from8,5 onto the actual event33 hidden door at8,4.
        if (!walk_route(campaign, folder, 8, 5, fights) ||
            !look_toward_route(campaign, folder, fights, opengold::por::MapDirection::north))
        {
            return false;
        }
        if (campaign.session.snapshot().script_id != 36)
        {
            check(campaign.session.explore(opengold::por::ExplorationCommand::forward),
                  "Use the professor's authored north-facing hidden doorway");
            if (!settle_route(campaign, folder, fights))
            {
                return false;
            }
        }
        check(campaign.session.snapshot().script_id == 36,
              "The actual hidden doorway enters the upstairs hideaway");
        retained_interior_save_checks(campaign, folder, 36, 34);
        std::cout << "Actual museum hideaway reached after " << campaign.ground_victories
                  << " ground victories at " << campaign.session.snapshot().pose.x << ','
                  << campaign.session.snapshot().pose.y << ".\n";
        if (!interact_wall_route(campaign, folder, 8, 3, opengold::por::MapDirection::west, fights))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C65}) & 0x80) != 0,
              "The actual computer-clue riser sequence reaches the top safely");
        std::cout << "Actual hideaway risers completed at " << campaign.session.snapshot().pose.x
                  << ',' << campaign.session.snapshot().pose.y << " after "
                  << campaign.ground_victories << " ground victories.\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (!interact_wall_route(campaign, folder, 11, 3, opengold::por::MapDirection::east,
                                 fights))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C64}) & 8) != 0,
              "The actual knock and confrontation open Stefi's door");
        if (!walk_route(campaign, folder, 12, 3, fights) ||
            !walk_event_route(campaign, folder, 12, fights))
        {
            return false;
        }
        if ((campaign.session.script_variable(opengold::por::EclAddress{0x4C64}) & 0x80) == 0)
        {
            check(campaign.session.explore(opengold::por::ExplorationCommand::look),
                  "Examine the real fusion simulation computer");
            if (!settle_route(campaign, folder, fights))
            {
                return false;
            }
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C64}) & 0x80) != 0,
              "Actual SCOT.DOS contact retrieves the simulation and sets its self-destruction");
        if (!walk_event_route(campaign, folder, 5, fights))
        {
            return false;
        }
        for (unsigned search = 0;
             !(campaign.session.script_variable(opengold::por::EclAddress{0x4C44}) & 0x40) &&
             search < 12;
             ++search)
        {
            check(campaign.session.explore(opengold::por::ExplorationCommand::look),
                  "Search the actual mattress for the professor's papers");
            if (!settle_route(campaign, folder, fights))
            {
                return false;
            }
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C44}) & 0x40) != 0,
              "The actual hideaway search obtains the fusion reactor diagrams");
        std::cout << "Actual fusion notes recovered after " << campaign.ground_victories
                  << " ground victories.\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (!interact_wall_route(campaign, folder, 8, 3, opengold::por::MapDirection::south,
                                 fights))
        {
            return false;
        }
        check(campaign.session.snapshot().script_id == 34,
              "The actual hideaway door returns to the lower museum");
        if (!return_for_salvation_autodoc(campaign, folder, fights, false))
        {
            return false;
        }
        std::cout << "Actual fusion mission reported at Salvation after "
                  << campaign.ground_victories << " ground victories, current script "
                  << campaign.session.snapshot().script_id << ", Solar "
                  << campaign.session.snapshot().solar_map << ".\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (!fly_route(campaign, folder, fights, 1))
        {
            return false;
        }
        const auto venus = campaign.session.snapshot();
        const auto lowlands = std::ranges::find(venus.choices, "VENUS LOWLANDS");
        check(lowlands != venus.choices.end(),
              "The actual Venus approach offers the assigned lowlands");
        check(campaign.session.choose(venus.continue_ticket,
                                      static_cast<std::size_t>(lowlands - venus.choices.begin())),
              "Land in the real assigned Venus lowlands");
        if (!settle_route(campaign, folder, fights))
        {
            return false;
        }
        const auto entered_venus = campaign.session.snapshot();
        std::cout << "Actual Venus lowlands entry: controller " << entered_venus.script_id
                  << ", retained map " << entered_venus.area_id << ", world "
                  << entered_venus.overland_x << ',' << entered_venus.overland_y << ".\n";
        saved_campaign_round_trips(campaign, folder, false);
        retained_wilderness_save_checks(campaign, folder, false);
        if (const auto *directory = std::getenv("OPENGOLD_MATRIX_PROOF_DIR"))
        {
            const auto proof = std::filesystem::path(directory);
            std::filesystem::create_directories(proof);
            const auto saved =
                opengold::encode_campaign(*campaign.party, &campaign.session,
                                          opengold::campaign_asset_identity(folder), matrix);
            const auto path = proof / "matrix-venus-lowlands-earned.ogs";
            opengold::write_save_file(path, saved, 16U * 1024U * 1024U);
            std::cout << "Actual Venus landing save: " << path << ".\n";
        }
        if (!walk_venus_world(campaign, folder, 34, 12, fights))
        {
            return false;
        }
        check(campaign.session.snapshot().script_id == 49,
              "The actual lowlander village gate enters its installed controller");
        std::cout << "Actual lowlander village reached after " << campaign.ground_victories
                  << " ground victories, occupied crew "
                  << std::ranges::count_if(campaign.party->state().slots,
                                           [](auto id)
                                           {
                                               return id != 0;
                                           })
                  << ".\n";
        saved_campaign_round_trips(campaign, folder, false);
        for (unsigned attempt = 0;
             !(campaign.session.script_variable(opengold::por::EclAddress{0x4C72}) & 4) &&
             attempt < 32;
             ++attempt)
        {
            if (!interact_wall_route(campaign, folder, 1, 10, opengold::por::MapDirection::north,
                                     fights))
            {
                return false;
            }
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C72}) & 4) != 0,
              "An actual offered Open Lock attempt opens the industrial door");
        std::cout << "Actual lowlander industrial entry at " << campaign.session.snapshot().pose.x
                  << ',' << campaign.session.snapshot().pose.y << ".\n";
        const bool elevator_walk = walk_event_route(campaign, folder, 1, fights);
        check((elevator_walk || campaign.session.snapshot().area_id == 52) &&
                  campaign.session.snapshot().script_id == 49 &&
                  campaign.session.snapshot().area_id == 52,
              "Use the actual down elevator to the mine map under controller49");
        if (!walk_event_route(campaign, folder, 33, fights))
        {
            return false;
        }
        std::cout << "Actual lowlander mine encounter after " << campaign.ground_victories
                  << " ground victories, map " << campaign.session.snapshot().area_id << ".\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (!interact_wall_route(campaign, folder, 11, 10, opengold::por::MapDirection::north,
                                 fights))
        {
            return false;
        }
        check(campaign.session.snapshot().pose.y == 8,
              "Use the actual climbing prompt to descend the mine shaft");
        if (!walk_event_route(campaign, folder, 35, fights))
        {
            return false;
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4C79}) == 1,
              "The authored rescue takes the wounded Llorok into the party's care");
        std::cout << "Actual Llorok rescue after " << campaign.ground_victories
                  << " ground victories.\n";
        saved_campaign_round_trips(campaign, folder, false);
        for (unsigned attempt = 0;
             !(campaign.session.script_variable(opengold::por::EclAddress{0x4C74}) & 2) &&
             attempt < 8;
             ++attempt)
        {
            if (!interact_wall_route(campaign, folder, 11, 8, opengold::por::MapDirection::south,
                                     fights))
            {
                return false;
            }
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C74}) & 2) != 0,
              "The actual lowlander medic receives Llorok and reveals the laboratory route");
        for (const auto id : campaign.party->state().slots)
        {
            if (!id)
            {
                continue;
            }
            const auto record = og::buck_rogers_record_of(campaign.party->game_record(id));
            check(record && !record->items.empty() && record->alternate_items.empty(),
                  "The actual miners return each occupied member's confiscated equipment");
        }
        saved_campaign_round_trips(campaign, folder, false);
        if (!ordinary_recovery_camp(campaign, folder, fights))
        {
            return false;
        }
        if (!interact_wall_route(campaign, folder, 15, 3, opengold::por::MapDirection::east,
                                 fights))
        {
            return false;
        }
        check(campaign.session.snapshot().script_id == 50,
              "The actual secret door enters the installed laboratory controller");
        std::cout << "Actual lowlander laboratory reached after " << campaign.ground_victories
                  << " ground victories, map " << campaign.session.snapshot().area_id << ".\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (!walk_route(campaign, folder, 8, 0, fights))
        {
            return false;
        }
        if (!look_toward_route(campaign, folder, fights, opengold::por::MapDirection::east))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C7F}) & 2) != 0,
              "Leander actually identifies the controls east of the gravity field");
        if (!walk_event_route(campaign, folder, 14, fights))
        {
            return false;
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4C80}) == 1,
              "The actual control shuts down the gravity field");
        if (!walk_event_route(campaign, folder, 15, fights))
        {
            return false;
        }
        std::cout << "Actual gravity field disabled and scientist reached after "
                  << campaign.ground_victories << " ground victories.\n";
        saved_campaign_round_trips(campaign, folder, false);
        check(std::ranges::any_of(campaign.party->state().slots,
                                  [&](const auto id)
                                  {
                                      if (!id)
                                      {
                                          return false;
                                      }
                                      const auto record = og::buck_rogers_record_of(
                                          campaign.party->game_record(id));
                                      return record && record->creature.name == "LEANDER";
                                  }),
              "Leander actually joins the occupied crew through ADD NPC");
        const bool lower_elevator = walk_event_route(campaign, folder, 1, fights);
        check(lower_elevator || campaign.session.snapshot().area_id == 51,
              "The actual laboratory elevator remains supported");
        if (campaign.session.snapshot().area_id == 50 &&
            !look_toward_route(campaign, folder, fights, opengold::por::MapDirection::east))
        {
            return false;
        }
        check(campaign.session.snapshot().area_id == 51,
              "Use Leander's actual passcard to descend to the lower laboratory");
        saved_campaign_round_trips(campaign, folder, false);
        if (!walk_event_route(campaign, folder, 40, fights))
        {
            return false;
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4C7E}) == 1,
              "The actual scientists' meeting commits Leander to the NEO mission");
        std::cout << "Actual lower laboratory scientists reached after "
                  << campaign.ground_victories << " ground victories.\n";
        saved_campaign_round_trips(campaign, folder, false);
        const bool upstairs = walk_event_route(campaign, folder, 3, fights);
        check(upstairs || campaign.session.snapshot().area_id == 50,
              "The original elevator returns toward the laboratory exit");
        if (campaign.session.snapshot().area_id == 51 &&
            !look_toward_route(campaign, folder, fights, opengold::por::MapDirection::east))
        {
            return false;
        }
        check(campaign.session.snapshot().area_id == 50,
              "The actual up elevator restores the laboratory ground floor");
        const bool climbed_out = walk_event_route(campaign, folder, 2, fights);
        check(climbed_out || campaign.session.snapshot().script_id == 48,
              "The actual opening in the ruined roof remains usable");
        check(campaign.session.snapshot().script_id == 48,
              "The completed rescue leaves the laboratory with the scientist");
        retained_wilderness_save_checks(campaign, folder, false);
        if (!walk_venus_world(campaign, folder, 5, 2, fights))
        {
            return false;
        }
        if (campaign.session.snapshot().script_id == 48)
        {
            check(campaign.session.explore(opengold::por::ExplorationCommand::look),
                  "Examine the actual ship after its first-arrival script");
            if (!settle_route(campaign, folder, fights))
            {
                return false;
            }
        }
        check(campaign.session.snapshot().script_id == 19 && campaign.session.snapshot().solar_map,
              "Board and launch the actual ship with the rescued scientist");
        saved_campaign_round_trips(campaign, folder, false);
        if (!fly_route(campaign, folder, fights, 2))
        {
            return false;
        }
        const auto report = campaign.session.snapshot();
        const auto station = std::ranges::find(report.choices, "SALVATION");
        check(station != report.choices.end(), "Return to the actual NEO station after Venus");
        check(campaign.session.choose(report.continue_ticket,
                                      static_cast<std::size_t>(station - report.choices.begin())),
              "Present the scientist through the actual Salvation debrief");
        if (!settle_route(campaign, folder, fights))
        {
            return false;
        }
        std::cout << "Actual Venus mission reported after " << campaign.ground_victories
                  << " ground victories, controller " << campaign.session.snapshot().script_id
                  << ", Solar " << campaign.session.snapshot().solar_map << ".\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (const auto *directory = std::getenv("OPENGOLD_MATRIX_PROOF_DIR"))
        {
            const auto path = std::filesystem::path(directory) / "matrix-post-venus-earned.ogs";
            const auto saved =
                opengold::encode_campaign(*campaign.party, &campaign.session,
                                          opengold::campaign_asset_identity(folder), matrix);
            opengold::write_save_file(path, saved, 16U * 1024U * 1024U);
            std::cout << "Actual post-Venus campaign save: " << path << ".\n";
        }
        campaign.hielo_supplies = true;
        if (!fly_route(campaign, folder, fights, 0))
        {
            return false;
        }
        const auto mercury = campaign.session.snapshot();
        const auto hielo = std::ranges::find(mercury.choices, "HIELO");
        check(hielo != mercury.choices.end(), "The actual Mercury orbit offers Hielo");
        check(campaign.session.choose(mercury.continue_ticket,
                                      static_cast<std::size_t>(hielo - mercury.choices.begin())),
              "Land at the actual Hielo station");
        if (!settle_route(campaign, folder, fights))
        {
            return false;
        }
        check(plasma_supplies_ready(campaign) && campaign.session.snapshot().script_id == 19 &&
                  campaign.session.snapshot().solar_map,
              "Real obsolete-armor sales and advanced-shop purchases prepare two plasma throwers");
        campaign.hielo_supplies = false;
        saved_campaign_round_trips(campaign, folder, false);
        if (!fly_route(campaign, folder, fights, 2))
        {
            return false;
        }
        campaign.malcoln_visit = true;
        const auto earth_approach = campaign.session.snapshot();
        const auto earth = std::ranges::find(earth_approach.choices, "DUKE'S HILL");
        check(earth != earth_approach.choices.end(), "The real Earth orbit offers Malcoln's city");
        check(campaign.session.choose(
                  earth_approach.continue_ticket,
                  static_cast<std::size_t>(earth - earth_approach.choices.begin())),
              "Land at the actual Earth port");
        if (!settle_route(campaign, folder, fights))
        {
            return false;
        }
        const auto earth_walk = [&](unsigned x, unsigned y)
        {
            for (unsigned step = 0; campaign.session.snapshot().script_id == 32 && step < 120;
                 ++step)
            {
                const auto state = campaign.session.snapshot();
                if (state.overland_x == x && state.overland_y == y)
                {
                    return true;
                }
                using Move = opengold::por::ExplorationCommand;
                const auto dx = static_cast<int>(x) - static_cast<int>(state.overland_x);
                const auto dy = static_cast<int>(y) - static_cast<int>(state.overland_y);
                const auto move = dx < 0 && dy < 0   ? Move::northwest
                                  : dx < 0 && dy > 0 ? Move::southwest
                                  : dx > 0 && dy < 0 ? Move::northeast
                                  : dx > 0 && dy > 0 ? Move::southeast
                                  : dx < 0           ? Move::west
                                  : dx > 0           ? Move::east
                                  : dy < 0           ? Move::north
                                                     : Move::south;
                check(campaign.session.explore(move),
                      "Travel by the real Earth wilderness controls");
                if (!settle_route(campaign, folder, fights))
                {
                    return false;
                }
            }
            return true; // Authored city entry can replace the wilderness.
        };
        if (!earth_walk(12, 7))
        {
            return false;
        }
        check(campaign.session.snapshot().script_id == 38,
              "Buck's actual Tower Isle passcard opens the authored city");
        saved_campaign_round_trips(campaign, folder, false);
        if (!walk_route(campaign, folder, 13, 8, fights, true))
        {
            return false;
        }
        if (campaign.session.snapshot().phase == TourPhase::awaiting_input &&
            !settle_route(campaign, folder, fights))
        {
            return false;
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4C02}) == 85,
              "The actual Tower elevator reaches Carrin's floor");
        if (!walk_route(campaign, folder, 13, 13, fights))
        {
            return false;
        }
        if (!look_toward_route(campaign, folder, fights, opengold::por::MapDirection::south))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C4B}) & 0x20) != 0,
              "Actually meet Red Carrin and obtain his boat passcard");
        campaign.tower_floor = 1;
        if (!walk_route(campaign, folder, 13, 8, fights, true))
        {
            return false;
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4C02}) == 1,
              "Take the actual elevator back to the ground floor");
        const bool tower_exit =
            interact_wall_route(campaign, folder, 15, 7, opengold::por::MapDirection::east, fights);
        check(tower_exit || campaign.session.snapshot().script_id == 32,
              "Leave Tower Isle through its actual lobby door");
        check(campaign.session.snapshot().script_id == 32,
              "Leave Tower Isle through its actual lobby");
        if (campaign.session.script_variable(opengold::por::EclAddress{0x4C08}) != 1 &&
            !earth_walk(12, 7))
        {
            return false;
        }
        check(campaign.session.snapshot().overland_x == 11 &&
                  campaign.session.snapshot().overland_y == 7,
              "Board Carrin's real boat at the authored dock");
        if (!earth_walk(3, 10) || !earth_walk(2, 10))
        {
            return false;
        }
        if (!earth_walk(6, 12))
        {
            return false;
        }
        check(campaign.session.snapshot().script_id == 81,
              "Actually reach Santa Catalina and enter PURGE headquarters");
        std::cout << "Actual PURGE entry after " << campaign.ground_victories
                  << " ground victories, map " << campaign.session.snapshot().area_id << ".\n";
        saved_campaign_round_trips(campaign, folder, false);
        const bool purge_upstairs = walk_event_route(campaign, folder, 1, fights);
        check(purge_upstairs || campaign.session.snapshot().script_id == 84,
              "The actual PURGE elevator changes its floor controller");
        check(campaign.session.snapshot().script_id == 84, "Use PURGE's actual upstairs elevator");
        std::cout << "Actual PURGE upper floor map " << campaign.session.snapshot().area_id
                  << ".\n";
        retained_interior_save_checks(campaign, folder, 84, 81);
        for (unsigned attempt = 0;
             campaign.session.script_variable(opengold::por::EclAddress{0x4C8F}) != 1 &&
             attempt < 8;
             ++attempt)
        {
            if (!walk_route(campaign, folder, 13, 15, fights))
            {
                return false;
            }
            if (!ordinary_recovery_camp(campaign, folder, fights))
            {
                return false;
            }
            if (!interact_wall_route(campaign, folder, 13, 15, opengold::por::MapDirection::west,
                                     fights))
            {
                return false;
            }
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4C8F}) == 1,
              "The actual programmer retrieves Malcoln's notes from the authored computer");
        saved_campaign_round_trips(campaign, folder, false);
        if (const auto *directory = std::getenv("OPENGOLD_MATRIX_PROOF_DIR"))
        {
            const auto saved =
                opengold::encode_campaign(*campaign.party, &campaign.session,
                                          opengold::campaign_asset_identity(folder), matrix);
            opengold::write_save_file(std::filesystem::path(directory) /
                                          "matrix-malcoln-notes-earned.ogs",
                                      saved, 16U * 1024U * 1024U);
        }
        std::cout << "Actual Malcoln notes retrieved after " << campaign.ground_victories
                  << " ground victories.\n";
        const bool down = walk_route(campaign, folder, 8, 0, fights, true);
        check(down || campaign.session.snapshot().script_id == 81,
              "The actual PURGE elevator returns to the lower controller");
        check(campaign.session.snapshot().script_id == 81,
              "Return to the ground floor with the actual notes");
        const bool hq_exit = interact_wall_route(campaign, folder, 4, 15,
                                                 opengold::por::MapDirection::south, fights);
        check(hq_exit || campaign.session.snapshot().script_id == 32,
              "The real lobby exit leaves PURGE headquarters");
        check(campaign.session.snapshot().script_id == 32,
              "Leave headquarters through its actual front door");
        campaign.leaving_catalina = true;
        if (!earth_walk(2, 10) || !earth_walk(8, 10) || !earth_walk(12, 7))
        {
            return false;
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4C08}) == 0,
              "Return Carrin's actual boat to Tower Isle");
        if (!return_for_salvation_autodoc(campaign, folder, fights, false))
        {
            return false;
        }
        campaign.malcoln_visit = false;
        campaign.leaving_catalina = false;
        std::cout << "Actual Malcoln mission reported and crew recovered after "
                  << campaign.ground_victories << " ground victories.\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (!fly_route(campaign, folder, fights, 2))
        {
            return false;
        }
        const auto lunar_approach = campaign.session.snapshot();

        const auto copernicus = std::ranges::find(lunar_approach.choices, "COPERNICUS");
        check(copernicus != lunar_approach.choices.end(),
              "The actual new assignment opens the Copernicus lunar destination");
        check(campaign.session.choose(
                  lunar_approach.continue_ticket,
                  static_cast<std::size_t>(copernicus - lunar_approach.choices.begin())),
              "Land at the actual Copernicus port to seek Dr. Coldor");
        if (!settle_route(campaign, folder, fights))
        {
            return false;
        }
        check(campaign.session.snapshot().script_id == 64,
              "The actual Copernicus landing reaches its installed controller");
        std::cout << "Actual Copernicus landing after " << campaign.ground_victories
                  << " ground victories, at " << campaign.session.snapshot().pose.x << ','
                  << campaign.session.snapshot().pose.y << ".\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (!walk_event_route(campaign, folder, 7, fights))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C00}) & 2) != 0,
              "Jenner actually supplies the forged badge through his original bargain");
        if (!interact_wall_route(campaign, folder, 8, 11, opengold::por::MapDirection::east,
                                 fights))
        {
            return false;
        }
        saved_campaign_round_trips(campaign, folder, false);
        if (!walk_event_route(campaign, folder, 29, fights))
        {
            return false;
        }
        if ((campaign.session.script_variable(opengold::por::EclAddress{0x4C01}) & 3) != 3 &&
            !look_toward_route(campaign, folder, fights, opengold::por::MapDirection::north))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C01}) & 3) == 3,
              "The actual NEO file and SCOT establish both evidence clues");
        saved_campaign_round_trips(campaign, folder, false);
        for (const auto [event, cell] : {std::pair{18U, 0x4C08U}, std::pair{23U, 0x4C07U}})
        {
            campaign.lunar_phone_target = "LT. JENNER";
            if (!walk_event_route(campaign, folder, 15, fights))
            {
                return false;
            }
            if (!walk_event_route(campaign, folder, event, fights))
            {
                return false;
            }
            if (campaign.session.script_variable(
                    opengold::por::EclAddress{static_cast<std::uint16_t>(cell)}) != 1 &&
                !look_toward_route(campaign, folder, fights, opengold::por::MapDirection::north))
            {
                return false;
            }
            check(campaign.session.script_variable(
                      opengold::por::EclAddress{static_cast<std::uint16_t>(cell)}) == 1,
                  "Search the actual residence or office after Jenner diverts the police");
            saved_campaign_round_trips(campaign, folder, false);
        }
        campaign.lunar_phone_target = "CHIEF MCKAY";
        if (!walk_event_route(campaign, folder, 15, fights))
        {
            return false;
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4C09}) == 1,
              "The real phone conversation arranges the suspects' meeting");
        if (!walk_event_route(campaign, folder, 18, fights) ||
            !look_toward_route(campaign, folder, fights, opengold::por::MapDirection::north))
        {
            return false;
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4C0A}) == 1,
              "Jenner's authored police raid opens the route to Dr. Coldor");
        std::cout << "Actual Copernicus investigation and police raid completed after "
                  << campaign.ground_victories << " ground victories.\n";
        saved_campaign_round_trips(campaign, folder, false);
        const auto before_supplies = campaign.ground_victories;
        if (!walk_event_route(campaign, folder, 34, fights) ||
            !look_toward_route(campaign, folder, fights, opengold::por::MapDirection::east))
        {
            return false;
        }
        unsigned charges = 0;
        for (const auto id : campaign.party->state().slots)
        {
            if (!id)
            {
                continue;
            }
            const auto record = og::buck_rogers_record_of(campaign.party->game_record(id));
            for (const auto &item : record->items)
            {
                if (item.type == 25)
                {
                    charges += item.quantity;
                }
            }
        }
        check(
            charges >= 3 && campaign.ground_victories == before_supplies,
            "The party carries actual demo charges and the supply-only event invents no battle victory");
        saved_campaign_round_trips(campaign, folder, false);
        if (!interact_wall_route(campaign, folder, 2, 0, opengold::por::MapDirection::north,
                                 fights))
        {
            return false;
        }
        check(campaign.session.snapshot().script_id == 65,
              "The actual research-section doorway enters Dr. Coldor's laboratory");
        std::cout << "Actual Coldor laboratory reached with " << charges
                  << " earned demo charges after " << campaign.ground_victories
                  << " ground victories.\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (!walk_event_route(campaign, folder, 13, fights))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C00}) & 1) != 0 &&
                  campaign.session.script_variable(opengold::por::EclAddress{0x4C0D}) == 1,
              "The actual plant encounter frees Dr. Coldor and starts her airshaft escape");
        if (!walk_event_route(campaign, folder, 15, fights) ||
            !look_toward_route(campaign, folder, fights, opengold::por::MapDirection::east))
        {
            return false;
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4C06}) == 1,
              "Find the actual second hummer prototype on its laboratory table");
        if (!walk_event_route(campaign, folder, 14, fights) ||
            !look_toward_route(campaign, folder, fights, opengold::por::MapDirection::south))
        {
            return false;
        }
        std::cout << "Actual Coldor rescue and hummer retrieval after " << campaign.ground_victories
                  << " ground victories.\n";
        saved_campaign_round_trips(campaign, folder, false);
        const bool reached_bay = walk_route(campaign, folder, 12, 7, fights);
        check(reached_bay || campaign.session.snapshot().script_id == 66,
              "The actual laboratory launch-bay confrontation remains supported");
        check(campaign.session.snapshot().script_id == 66,
              "The authored escape and launch reach the next installed campaign controller");
        std::cout << "Actual Luna laboratory escape after " << campaign.ground_victories
                  << " ground victories, next controller " << campaign.session.snapshot().script_id
                  << ".\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (!walk_route(campaign, folder, 13, 14, fights, true))
        {
            return false;
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4C0D}) == 1,
              "Use the actual escape pod after the authored ship breakdown");
        std::cout << "Actual escape-pod rendezvous after " << campaign.ground_victories
                  << " ground victories, controller " << campaign.session.snapshot().script_id
                  << " at " << campaign.session.snapshot().pose.x << ','
                  << campaign.session.snapshot().pose.y << ".\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (!walk_route(campaign, folder, 10, 1, fights, true))
        {
            return false;
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4CE0}) == 1,
              "Accept Kane's actual offer to assist his agents aboard the Deimos");
        std::cout << "Actual Kane meeting and Deimos transfer after " << campaign.ground_victories
                  << " ground victories, at " << campaign.session.snapshot().pose.x << ','
                  << campaign.session.snapshot().pose.y << ".\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (!walk_route(campaign, folder, 1, 0, fights, true))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C09}) & 0x20) != 0,
              "Oiler actually contacts the captured crew with the Deimos plan");
        if (!ordinary_recovery_camp(campaign, folder, fights, true))
        {
            return false;
        }
        saved_campaign_round_trips(campaign, folder, false);
        if (!walk_route(campaign, folder, 0, 5, fights, true))
        {
            return false;
        }
        check(campaign.session.snapshot().pose.x == 2 && campaign.session.snapshot().pose.y == 2,
              "The actual downward airshaft carries the crew out of the prison level");
        std::cout << "Actual Deimos airshaft escape after " << campaign.ground_victories
                  << " ground victories, at " << campaign.session.snapshot().pose.x << ','
                  << campaign.session.snapshot().pose.y << ".\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (!walk_route(campaign, folder, 5, 4, fights, true))
        {
            return false;
        }
        check(campaign.session.snapshot().pose.x == 5 && campaign.session.snapshot().pose.y == 5,
              "The actual civilian elevator carries the crew to PRIMARY REC");
        if (!walk_route(campaign, folder, 9, 0, fights, true))
        {
            return false;
        }
        check(campaign.session.snapshot().pose.x == 15 && campaign.session.snapshot().pose.y == 2,
              "The actual second downward airshaft reaches the weapons-control deck");
        if (!walk_route(campaign, folder, 9, 7, fights, true))
        {
            return false;
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4C16}) == 1,
              "Take the actual officer's RAM card through the authored encounter");
        for (unsigned attempt = 0;
             campaign.session.script_variable(opengold::por::EclAddress{0x4C17}) != 1 &&
             attempt < 32;
             ++attempt)
        {
            if (!interact_wall_route(campaign, folder, 11, 11, opengold::por::MapDirection::east,
                                     fights))
            {
                return false;
            }
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4C17}) == 1,
              "The actual card and a real Bypass Security check unlock the high-level door");
        if (!walk_route(campaign, folder, 13, 8, fights))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C0A}) & 0x20) != 0,
              "The actual backup-weapons-control encounter is completed");
        for (unsigned attempt = 0;
             campaign.session.script_variable(opengold::por::EclAddress{0x4C3B}) != 1 &&
             attempt < 32;
             ++attempt)
        {
            if (!walk_route(campaign, folder, 15, 7, fights))
            {
                return false;
            }
            if (campaign.session.script_variable(opengold::por::EclAddress{0x4C3B}) != 1)
            {
                check(campaign.session.explore(opengold::por::ExplorationCommand::look),
                      "Try the actual offered explosive-device placement");
                if (!settle_route(campaign, folder, fights))
                {
                    return false;
                }
            }
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4C3B}) == 1,
              "An actual Demolitions check installs Oiler's device in backup weapons control");
        std::cout << "Actual Deimos device installed after " << campaign.ground_victories
                  << " ground victories.\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (!interact_wall_route(campaign, folder, 13, 8, opengold::por::MapDirection::west,
                                 fights))
        {
            return false;
        }
        check(campaign.session.snapshot().script_id == 80 &&
                  campaign.session.snapshot().area_id == 84,
              "The authored Deimos capture reaches the actual RAM prison controller and map");
        std::cout << "Actual RAM prison arrival after " << campaign.ground_victories
                  << " ground victories, controller " << campaign.session.snapshot().script_id
                  << ", map " << campaign.session.snapshot().area_id << ".\n";
        saved_campaign_round_trips(campaign, folder, false);
        for (unsigned step = 0;
             campaign.session.script_variable(opengold::por::EclAddress{0x4C1B}) != 0xFF &&
             step < 160;
             ++step)
        {
            check(campaign.session.explore(opengold::por::ExplorationCommand::forward),
                  "Take an actual step through the prison envirocell");
            if (!settle_route(campaign, folder, fights))
            {
                return false;
            }
        }
        check(
            campaign.session.script_variable(opengold::por::EclAddress{0x4C1B}) == 0xFF &&
                campaign.session.snapshot().area_id == 80,
            "Actual desert walking, Buck and Kane's pyramid, and the guard dog escape the envirocell");
        std::cout << "Actual RAM envirocell escape after " << campaign.ground_victories
                  << " ground victories, at " << campaign.session.snapshot().pose.x << ','
                  << campaign.session.snapshot().pose.y << ".\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (const auto *directory = std::getenv("OPENGOLD_MATRIX_PROOF_DIR"))
        {
            const auto saved =
                opengold::encode_campaign(*campaign.party, &campaign.session,
                                          opengold::campaign_asset_identity(folder), matrix);
            opengold::write_save_file(std::filesystem::path(directory) /
                                          "matrix-prison-envirocell-earned.ogs",
                                      saved, 16U * 1024U * 1024U);
        }
        for (unsigned search = 0;
             (campaign.session.script_variable(opengold::por::EclAddress{0x4C0B}) & 3) != 3 &&
             search < 12;
             ++search)
        {
            for (const auto [x, y] :
                 std::array<std::pair<unsigned, unsigned>, 4>{{{1, 3}, {11, 9}, {8, 2}, {2, 11}}})
            {
                if (!walk_route(campaign, folder, x, y, fights))
                {
                    return false;
                }
            }
        }
        const auto earned_allies =
            campaign.session.script_variable(opengold::por::EclAddress{0x4C0B});
        std::cout << "Actual earned prison gang flags " << earned_allies << ".\n";
        // The authored encounter loads each earned gang independently;
        // recruiting both is optional preparation, not a campaign gate.
        check(earned_allies != 0, "Earn actual prison help before confronting the roof robot");
        const auto roof_reached = walk_route(campaign, folder, 15, 0, fights, true);
        check(roof_reached || campaign.session.snapshot().area_id == 84,
              "The authored roof exit changes to the actual facility map");
        check(campaign.session.snapshot().area_id == 84 &&
                  campaign.session.script_variable(opengold::por::EclAddress{0x4C14}) == 0xFF,
              "Defeat the actual roof robot and climb into the prison facility");
        std::cout << "Actual prison facility entry after " << campaign.ground_victories
                  << " ground victories.\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (!walk_route(campaign, folder, 14, 3, fights, true))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C04}) & 0x20) != 0,
              "Find and recover the crew's actual confiscated weapons in the exhibit room");
        std::cout << "Actual prison equipment recovered after " << campaign.ground_victories
                  << " ground victories.\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (!ordinary_recovery_camp(campaign, folder, fights))
        {
            return false;
        }
        if (!walk_route(campaign, folder, 2, 9, fights, true))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C04}) & 0x40) != 0,
              "Actually defeat the interrogators and free the Stormrider captive");
        std::cout << "Actual prison Stormrider rescued after " << campaign.ground_victories
                  << " ground victories.\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (!ordinary_recovery_camp(campaign, folder, fights))
        {
            return false;
        }
        if (!walk_route(campaign, folder, 6, 11, fights))
        {
            return false;
        }
        if (!ordinary_recovery_camp(campaign, folder, fights, true))
        {
            return false;
        }
        for (unsigned attempt = 0;
             !(campaign.session.script_variable(opengold::por::EclAddress{0x4C05}) & 4) &&
             attempt < 8;
             ++attempt)
        {
            if (!interact_wall_route(campaign, folder, 6, 11, opengold::por::MapDirection::north,
                                     fights))
            {
                return false;
            }
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C05}) & 4) != 0,
              "The actual prison computer resolves SCOT's authored crisis");
        std::cout << "Actual prison computer crisis resolved, SCOT loss flag "
                  << campaign.session.script_variable(opengold::por::EclAddress{0x4CCC}) << ".\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (!walk_route(campaign, folder, 7, 11, fights, true))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C05}) & 8) != 0,
              "Defeat the actual comm-room troops and reunite with Buck's escapees");
        if (!ordinary_recovery_camp(campaign, folder, fights, true))
        {
            return false;
        }
        campaign.prison_rearming = true;
        if (!interact_wall_route(campaign, folder, 15, 7, opengold::por::MapDirection::north,
                                 fights))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C05}) & 0x80) != 0,
              "Actually clear the prison barracks while rearming for the final defense");
        campaign.prison_rearming = false;
        if (!ordinary_recovery_camp(campaign, folder, fights, true))
        {
            return false;
        }
        saved_campaign_round_trips(campaign, folder, false);
        if (const auto *directory = std::getenv("OPENGOLD_MATRIX_PROOF_DIR"))
        {
            const auto saved =
                opengold::encode_campaign(*campaign.party, &campaign.session,
                                          opengold::campaign_asset_identity(folder), matrix);
            opengold::write_save_file(std::filesystem::path(directory) /
                                          "matrix-prison-comm-earned.ogs",
                                      saved, 16U * 1024U * 1024U);
        }
        if (!walk_route(campaign, folder, 9, 12, fights, true))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4C05}) & 0x20) != 0,
              "Actually guard Buck and Natbakka while they disable the prison defenses");
        saved_campaign_round_trips(campaign, folder, false);
        const auto landing = walk_route(campaign, folder, 8, 6, fights, true);
        check(landing || campaign.session.snapshot().script_id == 18 ||
                  (campaign.session.snapshot().script_id == 19 &&
                   campaign.session.snapshot().solar_map),
              "The actual NEO escape ship carries the rescued party to Salvation");
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4CCD}) == 0xFF,
              "Complete the authored RAM prison escape rather than placing the crew outside");
        std::cout << "Actual RAM prison escape after " << campaign.ground_victories
                  << " ground victories.\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (const auto *directory = std::getenv("OPENGOLD_MATRIX_PROOF_DIR"))
        {
            const auto saved =
                opengold::encode_campaign(*campaign.party, &campaign.session,
                                          opengold::campaign_asset_identity(folder), matrix);
            opengold::write_save_file(std::filesystem::path(directory) /
                                          "matrix-prison-escaped-earned.ogs",
                                      saved, 16U * 1024U * 1024U);
        }
        if (!fly_route(campaign, folder, fights, 6))
        {
            return false;
        }
        const auto fortuna = campaign.session.snapshot();
        const auto base = std::ranges::find(fortuna.choices, "NEO BASE");
        check(base != fortuna.choices.end(), "The actual Fortuna orbit offers its NEO base");
        campaign.fortuna_visit = true;
        campaign.malcoln_visit = false;
        check(campaign.session.choose(fortuna.continue_ticket,
                                      static_cast<std::size_t>(base - fortuna.choices.begin())),
              "Enter the authored Fortuna base");
        if (!settle_route(campaign, folder, fights))
        {
            return false;
        }
        check(campaign.session.snapshot().script_id == 82,
              "Actual Fortuna entry uses controller82");
        saved_campaign_round_trips(campaign, folder, false);
        const auto elevator = [&](unsigned floor)
        {
            campaign.fortuna_floor = floor;
            if (!walk_event_route(campaign, folder, 2, fights, true))
            {
                return false;
            }
            check(campaign.session.explore(opengold::por::ExplorationCommand::look),
                  "Use the actual Fortuna elevator control");
            return settle_route(campaign, folder, fights);
        };
        if (!elevator(4) || !walk_route(campaign, folder, 2, 3, fights, true))
        {
            return false;
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4CA8}) >= 1,
              "The real Director grants the first access card");
        if (!elevator(1))
        {
            return false;
        }
        for (unsigned attempt = 0;
             attempt < 8 &&
             !(campaign.session.script_variable(opengold::por::EclAddress{0x4CA5}) & 0x40);
             ++attempt)
        {
            if (!walk_route(campaign, folder, 9, 0, fights, true))
            {
                return false;
            }
            check(campaign.session.explore(opengold::por::ExplorationCommand::look),
                  "Search the actual basement junk with native Notice");
            if (!settle_route(campaign, folder, fights))
            {
                return false;
            }
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4CA5}) & 0x40) != 0,
              "Actually discover the suspicious Fortuna device");
        if (!elevator(4) || !walk_route(campaign, folder, 2, 3, fights, true))
        {
            return false;
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4CA8}) == 3,
              "The Director grants actual code3 after seeing the device");
        if (!elevator(2) || !walk_route(campaign, folder, 14, 7, fights, true) ||
            !walk_route(campaign, folder, 12, 11, fights, true))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4CA4}) & 0x10) != 0,
              "Dr Hampshire identifies the real device");
        if (!elevator(4) || !walk_route(campaign, folder, 2, 3, fights, true) ||
            !walk_route(campaign, folder, 1, 3, fights, true))
        {
            return false;
        }
        if (!elevator(3) || !walk_route(campaign, folder, 11, 13, fights, true))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4CA5}) & 0x10) != 0,
              "Actually discover ELROY in Cebert's room");
        if (!walk_route(campaign, folder, 13, 15, fights, true))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4CA5}) & 0x20) != 0,
              "Actually recover the spy infiltration document");
        saved_campaign_round_trips(campaign, folder, false);
        std::cout << "Actual Fortuna infiltration evidence after " << campaign.ground_victories
                  << " ground victories.\n";
        if (!ordinary_recovery_camp(campaign, folder, fights))
        {
            return false;
        }
        if (!elevator(4) || !walk_route(campaign, folder, 2, 3, fights, true))
        {
            return false;
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4CA9}) == 1 &&
                  campaign.session.script_variable(opengold::por::EclAddress{0x4CA8}) == 4,
              "The actual invasion grants the Director's code4 cards");
        if (!ordinary_recovery_camp(campaign, folder, fights))
        {
            return false;
        }
        if (!walk_route(campaign, folder, 2, 6, fights, true))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4CA5}) & 2) != 0,
              "Actually capture the spy Zachary after his native battle");
        if (!ordinary_recovery_camp(campaign, folder, fights))
        {
            return false;
        }
        if (!elevator(5) || !walk_route(campaign, folder, 2, 11, fights, true))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4CA6}) & 0x40) != 0,
              "Actually place the captured spy in the brig");
        if (!elevator(4))
        {
            return false;
        }
        for (unsigned attempt = 0;
             attempt < 8 &&
             !(campaign.session.script_variable(opengold::por::EclAddress{0x4CA6}) & 0x10);
             ++attempt)
        {
            if (!walk_route(campaign, folder, 3, 8, fights, true))
            {
                return false;
            }
            check(campaign.session.explore(opengold::por::ExplorationCommand::look),
                  "Use the actual base computer with the crew's Programming skill");
            if (!settle_route(campaign, folder, fights))
            {
                return false;
            }
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4CA6}) & 0x10) != 0,
              "The real WILBUR command seals Airlock2");
        if (!elevator(3) || !walk_route(campaign, folder, 8, 13, fights, true))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4CA6}) & 2) != 0,
              "Actually gather the dance-hall civilians");
        if (!elevator(1) || !walk_route(campaign, folder, 15, 0, fights, true))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4CA7}) & 1) != 0,
              "Deliver the civilians to the real Airlock1");
        if (!ordinary_recovery_camp(campaign, folder, fights))
        {
            return false;
        }
        if (!elevator(5) || !walk_route(campaign, folder, 4, 15, fights, true))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4CA7}) & 8) != 0,
              "Defeat the actual first Airlock2 wave");
        if (!ordinary_recovery_camp(campaign, folder, fights))
        {
            return false;
        }
        if (!elevator(2) || !walk_route(campaign, folder, 12, 9, fights, true))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4CA7}) & 0x10) != 0,
              "The actual secondary console reveals DANATAT");
        if (!ordinary_recovery_camp(campaign, folder, fights))
        {
            return false;
        }
        if (!elevator(4) || !walk_route(campaign, folder, 3, 8, fights, true))
        {
            return false;
        }
        check((campaign.session.script_variable(opengold::por::EclAddress{0x4CA7}) & 0x20) != 0,
              "The real DANATAT password starts the authored evacuation countdown");
        if (!elevator(3))
        {
            return false;
        }
        const auto fortuna_escape = walk_route(campaign, folder, 10, 15, fights, true);
        check(fortuna_escape || campaign.session.snapshot().script_id == 19,
              "The actual Fortuna hatch returns the party to its ship");
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4CAA}) == 1 &&
                  campaign.session.snapshot().script_id == 19,
              "Actually escape Fortuna through the authored hatch before its timer expires");
        campaign.fortuna_visit = false;
        std::cout << "Actual Fortuna escape after " << campaign.ground_victories
                  << " ground victories.\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (const auto *directory = std::getenv("OPENGOLD_MATRIX_PROOF_DIR"))
        {
            const auto saved =
                opengold::encode_campaign(*campaign.party, &campaign.session,
                                          opengold::campaign_asset_identity(folder), matrix);
            opengold::write_save_file(std::filesystem::path(directory) /
                                          "matrix-fortuna-escaped-earned.ogs",
                                      saved, 16U * 1024U * 1024U);
        }
        if (!fly_route(campaign, folder, fights, 2))
        {
            return false;
        }
        const auto salvation = campaign.session.snapshot();
        const auto salvation_choice = std::ranges::find(salvation.choices, "SALVATION");
        check(salvation_choice != salvation.choices.end(),
              "The actual Earth orbit offers Salvation debrief");
        check(campaign.session.choose(
                  salvation.continue_ticket,
                  static_cast<std::size_t>(salvation_choice - salvation.choices.begin())),
              "Report the real Fortuna evacuation to Buck");
        if (!settle_route(campaign, folder, fights))
        {
            return false;
        }
        check(campaign.session.script_variable(opengold::por::EclAddress{0x4C37}) == 3,
              "The authored briefing recognizes actual notes, prison and Fortuna completion");
        if (!fly_route(campaign, folder, fights, 9))
        {
            return false;
        }
        const auto juno = campaign.session.snapshot();
        const auto fungus = std::ranges::find(juno.choices, "FUNGUS ASTEROID");
        check(fungus != juno.choices.end(), "Actual Juno orbit offers the Fungus asteroid mission");
        check(campaign.session.choose(juno.continue_ticket,
                                      static_cast<std::size_t>(fungus - juno.choices.begin())),
              "Land at the actual Fungus asteroid");
        if (!settle_route(campaign, folder, fights))
        {
            return false;
        }
        check(campaign.session.snapshot().script_id == 97 &&
                  campaign.session.snapshot().area_id == 96,
              "The real Fungus arrival uses controller97 and map96");
        saved_campaign_round_trips(campaign, folder, false);
        std::cout << "Actual Fungus asteroid arrival after " << campaign.ground_victories
                  << " ground victories.\n";
        if (!walk_route(campaign, folder, 1, 13, fights, true) ||
            !look_toward_route(campaign, folder, fights, opengold::por::MapDirection::south))
        {
            return false;
        }
        if (!(campaign.session.script_variable(opengold::por::EclAddress{0x4C20}) & 8))
        {
            if (!walk_route(campaign, folder, 1, 15, fights, true) ||
                !look_toward_route(campaign, folder, fights, opengold::por::MapDirection::south))
            {
                return false;
            }
        }
        std::cout << "Actual living-ship beacon outcome "
                  << campaign.session.script_variable(opengold::por::EclAddress{0x4C20})
                  << ", controller " << campaign.session.snapshot().script_id << ", map "
                  << campaign.session.snapshot().area_id << ", pose "
                  << campaign.session.snapshot().pose.x << ',' << campaign.session.snapshot().pose.y
                  << ".\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (campaign.session.script_variable(opengold::por::EclAddress{0x4C20}) & 0x20)
        {
            if (!walk_route(campaign, folder, 1, 11, fights, true))
            {
                return false;
            }
            const auto boarded =
                look_toward_route(campaign, folder, fights, opengold::por::MapDirection::north);
            check(boarded || campaign.session.snapshot().script_id == 96,
                  "Board the real living ship through its authored airlock");
        }
        std::cout << "Actual living-ship entry controller " << campaign.session.snapshot().script_id
                  << ", map " << campaign.session.snapshot().area_id << ", pose "
                  << campaign.session.snapshot().pose.x << ',' << campaign.session.snapshot().pose.y
                  << ".\n";
        saved_campaign_round_trips(campaign, folder, false);
        if (campaign.session.snapshot().script_id == 97 &&
            campaign.session.snapshot().area_id == 96)
        {
            if (!interact_wall_route(campaign, folder, 9, 10, opengold::por::MapDirection::west,
                                     fights))
            {
                return false;
            }
            check(campaign.session.snapshot().script_id == 96,
                  "The actual ship hatch enters its propulsion controller");
        }
        if (campaign.session.snapshot().script_id == 96)
        {
            if (!walk_route(campaign, folder, 14, 1, fights, true))
            {
                return false;
            }
            check((campaign.session.script_variable(opengold::por::EclAddress{0x4CF3}) & 2) != 0,
                  "Actually obtain the techs' tools before repairing the ship");
            if (!ordinary_recovery_camp(campaign, folder, fights))
            {
                return false;
            }
            if (!interact_wall_route(campaign, folder, 12, 9, opengold::por::MapDirection::south,
                                     fights))
            {
                return false;
            }
            check(campaign.session.snapshot().script_id == 96 &&
                      campaign.session.snapshot().pose.x == 12 &&
                      campaign.session.snapshot().pose.y == 10,
                  "The actual inner passage enters the propulsion instrument room");
            for (const auto [x, y] : {std::pair{11U, 11U}, std::pair{13U, 11U}})
            {
                if (!walk_route(campaign, folder, x, y, fights, true))
                {
                    return false;
                }
                const auto cell = campaign.session.map().at(x, y);
                const auto open = std::ranges::find(cell.walls, 0);
                check(open != cell.walls.end(),
                      "The actual instrument has an open examination direction");
                if (!look_toward_route(
                        campaign, folder, fights,
                        static_cast<opengold::por::MapDirection>(open - cell.walls.begin())))
                {
                    return false;
                }
            }
            std::cout << "Actual propulsion repairs "
                      << campaign.session.script_variable(opengold::por::EclAddress{0x4C22})
                      << ", damaged gear "
                      << campaign.session.script_variable(opengold::por::EclAddress{0x4CF2})
                      << ".\n";
            saved_campaign_round_trips(campaign, folder, false);
            if (!interact_wall_route(campaign, folder, 12, 10, opengold::por::MapDirection::north,
                                     fights))
            {
                return false;
            }
            const auto next_deck = interact_wall_route(campaign, folder, 5, 0,
                                                       opengold::por::MapDirection::north, fights);
            check(next_deck || campaign.session.snapshot().script_id == 97,
                  "Climb through the actual hatch to the living ship's crew deck");
            check(campaign.session.snapshot().script_id == 97 &&
                      campaign.session.snapshot().area_id == 97,
                  "The crew-deck transition uses its explicitly loaded map97");
            saved_campaign_round_trips(campaign, folder, false);
            std::cout << "Actual living-ship crew deck reached after " << campaign.ground_victories
                      << " ground victories.\n";
            if (!walk_route(campaign, folder, 10, 9, fights, true) ||
                !look_toward_route(campaign, folder, fights, opengold::por::MapDirection::south))
            {
                return false;
            }
            check((campaign.session.script_variable(opengold::por::EclAddress{0x4C26}) & 0x40) != 0,
                  "The actual officers' encounter yields its real security pass");
            if (!ordinary_recovery_camp(campaign, folder, fights))
            {
                return false;
            }
            const auto bridge = interact_wall_route(campaign, folder, 2, 8,
                                                    opengold::por::MapDirection::north, fights);
            check(bridge || campaign.session.snapshot().script_id == 98,
                  "Use the actual crew-deck passage to reach the bridge controller");
            check(campaign.session.snapshot().script_id == 98 &&
                      campaign.session.snapshot().area_id == 97,
                  "The actual bridge retains its ship map rather than loading a nonexistent map98");
            saved_campaign_round_trips(campaign, folder, false);
            if (!walk_route(campaign, folder, 7, 2, fights, true) ||
                !look_toward_route(campaign, folder, fights, opengold::por::MapDirection::north))
            {
                return false;
            }
            check(campaign.session.script_variable(opengold::por::EclAddress{0x4CF1}) == 3,
                  "Actually defeat the pirate captain and destroy the violent control device");
            if (!ordinary_recovery_camp(campaign, folder, fights))
            {
                return false;
            }
            std::cout << "Actual living-ship bridge cleared after " << campaign.ground_victories
                      << " ground victories.\n";
            saved_campaign_round_trips(campaign, folder, false);
            if (const auto *directory = std::getenv("OPENGOLD_MATRIX_PROOF_DIR"))
            {
                const auto saved =
                    opengold::encode_campaign(*campaign.party, &campaign.session,
                                              opengold::campaign_asset_identity(folder), matrix);
                opengold::write_save_file(std::filesystem::path(directory) /
                                              "matrix-bridge-earned.ogs",
                                          saved, 16U * 1024U * 1024U);
            }
            if (!interact_wall_route(campaign, folder, 7, 0, opengold::por::MapDirection::west,
                                     fights))
            {
                return false;
            }
            if (!walk_route(campaign, folder, 3, 5, fights, true) ||
                !look_toward_route(campaign, folder, fights, opengold::por::MapDirection::south))
            {
                return false;
            }
            check(((campaign.session.script_variable(opengold::por::EclAddress{0x4C22}) |
                    campaign.session.script_variable(opengold::por::EclAddress{0x4CF2})) &
                   0x10) != 0,
                  "The real left synapse attempt retains its native success or permanent failure");
            if (!interact_wall_route(campaign, folder, 6, 0, opengold::por::MapDirection::east,
                                     fights) ||
                !interact_wall_route(campaign, folder, 9, 0, opengold::por::MapDirection::east,
                                     fights))
            {
                return false;
            }
            if (!walk_route(campaign, folder, 13, 5, fights, true) ||
                !look_toward_route(campaign, folder, fights, opengold::por::MapDirection::south))
            {
                return false;
            }
            check(((campaign.session.script_variable(opengold::por::EclAddress{0x4C22}) |
                    campaign.session.script_variable(opengold::por::EclAddress{0x4CF2})) &
                   0x20) != 0,
                  "The real right synapse attempt retains its native success or permanent failure");
            if (!interact_wall_route(campaign, folder, 10, 0, opengold::por::MapDirection::west,
                                     fights))
            {
                return false;
            }
            if (!walk_route(campaign, folder, 7, 2, fights, true) ||
                !look_toward_route(campaign, folder, fights, opengold::por::MapDirection::north))
            {
                return false;
            }
            std::cout << "Actual ship synapse result "
                      << campaign.session.script_variable(opengold::por::EclAddress{0x4C22})
                      << ", permanent failures "
                      << campaign.session.script_variable(opengold::por::EclAddress{0x4CF2})
                      << ", access device "
                      << campaign.session.script_variable(opengold::por::EclAddress{0x4C25})
                      << ".\n";
            saved_campaign_round_trips(campaign, folder, false);
            if (campaign.session.script_variable(opengold::por::EclAddress{0x4CF2}) != 0xFF)
            {
                check((campaign.session.script_variable(opengold::por::EclAddress{0x4C25}) &
                       0x10) != 0,
                      "The Stormriders actually supply the two-pronged access device");
                for (const auto [x, y, facing, mask] :
                     {std::tuple{10U, 5U, opengold::por::MapDirection::south, 0x80U},
                      std::tuple{7U, 6U, opengold::por::MapDirection::north, 0x40U},
                      std::tuple{7U, 4U, opengold::por::MapDirection::north, 0x04U},
                      std::tuple{9U, 4U, opengold::por::MapDirection::north, 0x08U}})
                {
                    const auto walked = walk_route(campaign, folder, x, y, fights, true);
                    if (!walked && campaign.session.snapshot().script_id != 112)
                    {
                        return false;
                    }
                    if (campaign.session.snapshot().script_id == 112)
                    {
                        break;
                    }
                    if (!look_toward_route(campaign, folder, fights, facing))
                    {
                        return false;
                    }
                    if (campaign.session.snapshot().script_id == 112)
                    {
                        break;
                    }
                    const auto successes =
                        campaign.session.script_variable(opengold::por::EclAddress{0x4C22});
                    const auto failures =
                        campaign.session.script_variable(opengold::por::EclAddress{0x4CF2});
                    check(
                        ((successes | failures) & mask) != 0,
                        "Each actual system keeps its real repair result without resetting failed checks");
                    std::cout << "Actual ship system mask " << mask << ", successes " << successes
                              << ", permanent failures " << failures << ".\n";
                    saved_campaign_round_trips(campaign, folder, false);
                    if (failures == 0xFF)
                    {
                        break;
                    }
                }
            }
            std::cout << "Actual living-ship system attempt boundary controller "
                      << campaign.session.snapshot().script_id << ", successes "
                      << campaign.session.script_variable(opengold::por::EclAddress{0x4C22})
                      << ", permanent failures "
                      << campaign.session.script_variable(opengold::por::EclAddress{0x4CF2})
                      << ".\n";
            if (campaign.session.snapshot().script_id == 98 &&
                campaign.session.script_variable(opengold::por::EclAddress{0x4CF2}) == 0xFF)
            {
                check(std::ranges::any_of(campaign.party->state().slots,
                                          [&](const auto id)
                                          {
                                              if (!id)
                                              {
                                                  return false;
                                              }
                                              const auto gear = og::buck_rogers_record_of(
                                                  campaign.party->game_record(id));
                                              return gear &&
                                                     std::ranges::any_of(gear->items,
                                                                         [](const auto &item)
                                                                         {
                                                                             return item.type == 15;
                                                                         });
                                          }),
                      "An actual active crew member took a source-supplied sonic stunner");
                const auto lower_bridge = interact_wall_route(
                    campaign, folder, 2, 7, opengold::por::MapDirection::south, fights);
                check(lower_bridge || campaign.session.snapshot().script_id == 97,
                      "Return through the actual bridge passage");
                check(campaign.session.snapshot().script_id == 97,
                      "The source returns to the ordinary crew deck");
                const auto propulsion = interact_wall_route(
                    campaign, folder, 5, 15, opengold::por::MapDirection::south, fights);
                check(propulsion || campaign.session.snapshot().script_id == 96,
                      "Descend through the real crew-deck hatch");
                check(campaign.session.snapshot().script_id == 96 &&
                          campaign.session.snapshot().area_id == 96,
                      "The authored hatch reloads the propulsion map96");
                if (!ordinary_recovery_camp(campaign, folder, fights))
                {
                    return false;
                }
                if (!interact_wall_route(campaign, folder, 12, 9,
                                         opengold::por::MapDirection::south, fights))
                {
                    return false;
                }
                const auto nerve = walk_route(campaign, folder, 10, 13, fights, true);
                if (!nerve && campaign.session.snapshot().script_id != 112)
                {
                    return false;
                }
                if (campaign.session.snapshot().script_id != 112)
                {
                    const auto stimulated = look_toward_route(campaign, folder, fights,
                                                              opengold::por::MapDirection::south);
                    check(stimulated || campaign.session.snapshot().script_id == 112,
                          "Use the real carried stunner at the authored propulsion nerve");
                }
            }
            check(campaign.session.snapshot().script_id == 112 &&
                      campaign.session.snapshot().area_id == 112,
                  "Actually reach Jupiter through the living ship's voyage after its real finale");
            saved_campaign_round_trips(campaign, folder, false);
            std::cout << "Actual Jupiter university arrival after " << campaign.ground_victories
                      << " ground victories.\n";
            if (const auto *directory = std::getenv("OPENGOLD_MATRIX_PROOF_DIR"))
            {
                const auto saved =
                    opengold::encode_campaign(*campaign.party, &campaign.session,
                                              opengold::campaign_asset_identity(folder), matrix);
                opengold::write_save_file(std::filesystem::path(directory) /
                                              "matrix-jupiter-arrival-earned.ogs",
                                          saved, 16U * 1024U * 1024U);
            }
            const auto university_visit =
                [&](unsigned x, unsigned y, opengold::por::MapDirection facing)
            {
                if (!walk_route(campaign, folder, x, y, fights, true))
                {
                    return false;
                }
                return look_toward_route(campaign, folder, fights, facing);
            };
            using Facing = opengold::por::MapDirection;
            if (!university_visit(1, 13, Facing::west) || !university_visit(7, 13, Facing::south))
            {
                return false;
            }
            check(campaign.session.script_variable(opengold::por::EclAddress{0x4CB9}) == 1,
                  "Meeting Makali actually triggers the university invasion");
            for (const auto [x, y, facing] :
                 {std::tuple{5U, 2U, Facing::north}, std::tuple{1U, 13U, Facing::west},
                  std::tuple{11U, 2U, Facing::east}, std::tuple{13U, 2U, Facing::north}})
            {
                if (campaign.session.script_variable(opengold::por::EclAddress{0x4CB9}) != 1)
                {
                    break;
                }
                if (!university_visit(x, y, facing))
                {
                    return false;
                }
                if (!ordinary_recovery_camp(campaign, folder, fights))
                {
                    return false;
                }
                std::cout << "Actual university defense count "
                          << campaign.session.script_variable(opengold::por::EclAddress{0x4CBA})
                          << ".\n";
            }
            if (campaign.session.script_variable(opengold::por::EclAddress{0x4CB9}) == 1)
            {
                if (!university_visit(10, 7, Facing::south))
                {
                    return false;
                }
                check(campaign.university_bomb_code,
                      "Actually learn the wounded demolition soldier's clue");
                // Go directly to the real bomb; no resting or timer writes.
                if (!university_visit(10, 10, Facing::north) ||
                    !university_visit(13, 12, Facing::north))
                {
                    return false;
                }
                check(campaign.session.script_variable(opengold::por::EclAddress{0x4C04}) == 2,
                      "Actually disarm the planted bomb with its learned code");
                if (!ordinary_recovery_camp(campaign, folder, fights))
                {
                    return false;
                }
            }
            if (campaign.session.script_variable(opengold::por::EclAddress{0x4CB9}) == 1)
            {
                if (!university_visit(11, 15, Facing::west) ||
                    !university_visit(14, 13, Facing::south))
                {
                    return false;
                }
                if (!ordinary_recovery_camp(campaign, folder, fights))
                {
                    return false;
                }
            }
            check(campaign.session.script_variable(opengold::por::EclAddress{0x4CB9}) == 2 &&
                      campaign.session.script_variable(opengold::por::EclAddress{0x4CBA}) == 5,
                  "Five actual university defense resolutions earn Tsubulu's mission");
            saved_campaign_round_trips(campaign, folder, false);
            std::cout << "Actual university invasion completed after " << campaign.ground_victories
                      << " ground victories.\n";
            const auto genetics_arrival = university_visit(4, 5, Facing::north);
            check(genetics_arrival || campaign.session.snapshot().script_id == 113,
                  "Leave with Tsubulu through the actual university landing pad");
            check(campaign.session.snapshot().script_id == 113 &&
                      campaign.session.snapshot().area_id == 113,
                  "The authored wasphopper mission reaches Genetics Foundation");
            std::cout << "Actual Genetics capture boundary pose "
                      << campaign.session.snapshot().pose.x << ','
                      << campaign.session.snapshot().pose.y << ".\n";
            saved_campaign_round_trips(campaign, folder, false);
            if (!walk_route(campaign, folder, 10, 11, fights, true) ||
                !look_toward_route(campaign, folder, fights, Facing::north) ||
                !walk_route(campaign, folder, 12, 8, fights, true) ||
                !look_toward_route(campaign, folder, fights, Facing::east))
            {
                return false;
            }
            check(campaign.session.snapshot().pose.x == 13 &&
                      campaign.session.snapshot().pose.y == 8,
                  "The cooks' real access grate leads to the maintenance ladder");
            const auto genetics_ladder = [&](unsigned x, unsigned y, unsigned to_x, unsigned to_y)
            {
                campaign.genetics_ladder_destination = std::pair{to_x, to_y};
                if (!walk_route(campaign, folder, x, y, fights, true))
                {
                    return false;
                }
                const auto before = campaign.session.snapshot().pose;
                if (before.x != to_x || before.y != to_y)
                {
                    check(campaign.session.explore(opengold::por::ExplorationCommand::look),
                          "Use the real Genetics ladder");
                    if (!settle_route(campaign, folder, fights))
                    {
                        return false;
                    }
                }
                campaign.genetics_ladder_destination.reset();
                const auto actual = campaign.session.snapshot().pose;
                check(actual.x == to_x && actual.y == to_y,
                      "The authored ladder, not a placement, reaches its other end");
                return true;
            };
            const auto genetics_elevator = [&](unsigned floor)
            {
                campaign.genetics_floor = floor;
                if (!walk_event_route(campaign, folder, 4, fights, true))
                {
                    return false;
                }
                check(campaign.session.explore(opengold::por::ExplorationCommand::look),
                      "Use the offered Genetics elevator button");
                return settle_route(campaign, folder, fights);
            };
            if (!genetics_ladder(15, 8, 15, 0) ||
                !walk_route(campaign, folder, 11, 6, fights, true))
            {
                return false;
            }
            check((campaign.session.script_variable(opengold::por::EclAddress{0x4CC2}) & 8) != 0,
                  "Find the confiscated equipment and win its actual guard battle");
            if (!ordinary_recovery_camp(campaign, folder, fights))
            {
                return false;
            }
            saved_campaign_round_trips(campaign, folder, false);
            if (!genetics_elevator(2) || !walk_route(campaign, folder, 6, 9, fights, true) ||
                !walk_route(campaign, folder, 7, 9, fights, true))
            {
                return false;
            }
            check((campaign.session.script_variable(opengold::por::EclAddress{0x4CC3}) & 4) != 0,
                  "Actually shut down the main security computer");
            if (!ordinary_recovery_camp(campaign, folder, fights))
            {
                return false;
            }
            if (!genetics_elevator(3) || !genetics_ladder(8, 0, 0, 8))
            {
                return false;
            }
            check((campaign.session.script_variable(opengold::por::EclAddress{0x4CC3}) & 8) != 0,
                  "Actually deactivate secondary security and unlock its cells");
            if (!genetics_ladder(0, 8, 8, 0) || !genetics_elevator(1))
            {
                return false;
            }
            if (!walk_route(campaign, folder, 1, 6, fights, true) ||
                !look_toward_route(campaign, folder, fights, Facing::south))
            {
                return false;
            }
            check((campaign.session.script_variable(opengold::por::EclAddress{0x4CC2}) & 0x80) != 0,
                  "Actually recover the captive wasphopper grubs");
            if (!(campaign.session.script_variable(opengold::por::EclAddress{0x4CC3}) & 1))
            {
                if (!walk_route(campaign, folder, 7, 5, fights, true))
                {
                    return false;
                }
            }
            check((campaign.session.script_variable(opengold::por::EclAddress{0x4CC3}) & 1) != 0,
                  "The real native skill or authored medbay revives the grubs");
            if (!genetics_elevator(3) || !walk_route(campaign, folder, 9, 0, fights, true) ||
                !look_toward_route(campaign, folder, fights, Facing::east))
            {
                return false;
            }
            check((campaign.session.script_variable(opengold::por::EclAddress{0x4CC3}) & 0x40) != 0,
                  "Release living grubs into the actual Genetics growth vat");
            saved_campaign_round_trips(campaign, folder, false);
            if (!genetics_elevator(1))
            {
                return false;
            }
            const auto genetics_exit =
                interact_wall_route(campaign, folder, 5, 0, Facing::north, fights);
            check(genetics_exit || campaign.session.snapshot().script_id == 112,
                  "Escape through the actual Genetics airlock to Tsubulu's shuttle");
            check((campaign.session.snapshot().script_id == 112 ||
                   campaign.session.snapshot().script_id == 114) &&
                      campaign.session.script_variable(opengold::por::EclAddress{0x4CB9}) == 3,
                  "The real Genetics mission returns to Makali and her onward shuttle");
            saved_campaign_round_trips(campaign, folder, false);
            std::cout << "Actual Genetics Foundation escaped after " << campaign.ground_victories
                      << " ground victories.\n";
            if (campaign.session.snapshot().script_id == 112)
            {
                const auto onward = university_visit(4, 5, Facing::north);
                check(onward || campaign.session.snapshot().script_id == 114,
                      "Take Makali's real onward shuttle to the mining rig");
            }
            check(campaign.session.snapshot().script_id == 114 &&
                      campaign.session.snapshot().area_id == 114,
                  "Reach the actual final mining rig without a direct placement");
            saved_campaign_round_trips(campaign, folder, false);
            if (const auto *directory = std::getenv("OPENGOLD_MATRIX_PROOF_DIR"))
            {
                const auto saved =
                    opengold::encode_campaign(*campaign.party, &campaign.session,
                                              opengold::campaign_asset_identity(folder), matrix);
                opengold::write_save_file(std::filesystem::path(directory) /
                                              "matrix-finale-arrival-earned.ogs",
                                          saved, 16U * 1024U * 1024U);
            }
            if (!walk_route(campaign, folder, 15, 8, fights, true))
            {
                return false;
            }
            const auto aircar = interact_wall_route(campaign, folder, 9, 8, Facing::north, fights);
            check(aircar || campaign.session.snapshot().area_id == 115,
                  "Enter the real aircar hatch after reporting to the Boss");
            check(campaign.session.snapshot().script_id == 114 &&
                      campaign.session.snapshot().area_id == 115,
                  "The actual rig hatch changes to alternate map115 under controller114");
            saved_campaign_round_trips(campaign, folder, false);
            if (!walk_route(campaign, folder, 8, 7, fights, true) ||
                !look_toward_route(campaign, folder, fights, Facing::west))
            {
                return false;
            }
            check(campaign.session.script_variable(opengold::por::EclAddress{0x4C0F}) == 1,
                  "Actually meet the scientists and begin the final rig defense");
            std::cout << "Actual final rig defense begun after " << campaign.ground_victories
                      << " ground victories.\n";
            const auto rig_aircar = [&](bool entering)
            {
                const auto target = entering ? 115U : 114U;
                if (campaign.session.snapshot().area_id == target)
                {
                    return true;
                }
                const auto passed =
                    interact_wall_route(campaign, folder, 9, entering ? 8U : 7U,
                                        entering ? Facing::north : Facing::south, fights);
                check(passed || campaign.session.snapshot().area_id == target,
                      "Use the authored aircar hatch in its actual direction");
                check(campaign.session.snapshot().area_id == target &&
                          campaign.session.snapshot().script_id == 114,
                      "The hatch keeps the final controller on its actual selected map");
                saved_campaign_round_trips(campaign, folder, false);
                return true;
            };
            const auto rig_clinic = [&]()
            {
                if (!rig_aircar(false) || !walk_route(campaign, folder, 10, 9, fights, true))
                {
                    return false;
                }
                check(campaign.session.script_variable(opengold::por::EclAddress{0x4C0F}) != 7,
                      "Use the real autosurgeons before their authored power failure");
                for (const auto id : campaign.party->state().slots)
                {
                    if (!id)
                    {
                        continue;
                    }
                    const auto record = og::buck_rogers_record_of(campaign.party->game_record(id));
                    check(record && record->creature.status == 1 &&
                              record->creature.hit_points == record->creature.hit_point_maximum,
                          "The actual powered rig clinic restores the occupied crew");
                }
                return true;
            };
            const auto rig_deck = [&]()
            {
                if (campaign.session.snapshot().game_won)
                {
                    return true;
                }
                if (!rig_aircar(false) || !walk_route(campaign, folder, 3, 3, fights, true))
                {
                    return false;
                }
                if (campaign.session.snapshot().game_won)
                {
                    return true;
                }
                return walk_route(campaign, folder, 4, 3, fights, true);
            };
            const auto rig_scientists = [&]()
            {
                if (!rig_aircar(true) || !walk_route(campaign, folder, 8, 7, fights, true))
                {
                    return false;
                }
                return look_toward_route(campaign, folder, fights, Facing::west);
            };
            if (!rig_deck())
            {
                return false;
            }
            check((campaign.session.script_variable(opengold::por::EclAddress{0x4C04}) & 0x10) != 0,
                  "Defeat the actual first shuttle-deck assault");
            if (!rig_clinic() || !walk_route(campaign, folder, 7, 9, fights, true))
            {
                return false;
            }
            check(campaign.session.script_variable(opengold::por::EclAddress{0x4C0F}) == 2,
                  "The real first infiltrator battle earns the quiet regrouping phase");
            if (!rig_clinic() || !walk_route(campaign, folder, 15, 8, fights, true))
            {
                return false;
            }
            check(campaign.session.script_variable(opengold::por::EclAddress{0x4C0F}) == 3,
                  "Boss Nikita actually starts the next defense phase");
            if (!rig_deck())
            {
                return false;
            }
            check((campaign.session.script_variable(opengold::por::EclAddress{0x4C04}) & 0x20) != 0,
                  "Defeat the real second shuttle-deck assault");
            if (!rig_clinic() || !walk_route(campaign, folder, 11, 6, fights, true))
            {
                return false;
            }
            if (campaign.session.script_variable(opengold::por::EclAddress{0x4C0F}) == 3)
            {
                if (!walk_route(campaign, folder, 11, 14, fights, true))
                {
                    return false;
                }
            }
            check(
                campaign.session.script_variable(opengold::por::EclAddress{0x4C0F}) == 4,
                "Two actual infiltrator groups at different rows earn the next scientist briefing");
            if (!rig_clinic() || !rig_scientists())
            {
                return false;
            }
            check(campaign.session.script_variable(opengold::por::EclAddress{0x4C0F}) == 5,
                  "The real scientist briefing begins the ECG defense phase");
            saved_campaign_round_trips(campaign, folder, false);
            std::cout << "Actual final rig phase5 reached after " << campaign.ground_victories
                      << " ground victories.\n";
            if (!rig_deck())
            {
                return false;
            }
            check((campaign.session.script_variable(opengold::por::EclAddress{0x4C04}) & 0x40) != 0,
                  "Defeat the actual third shuttle-deck wave");
            if (!rig_clinic() || !rig_scientists())
            {
                return false;
            }
            check(campaign.session.snapshot().area_id == 115 &&
                      campaign.session.script_variable(opengold::por::EclAddress{0x4C0D}) >= 0x1F,
                  "The actual gas-defense plan closes the aircar for its ECG search");
            for (unsigned visit = 0; visit < 20 && campaign.session.script_variable(
                                                       opengold::por::EclAddress{0x4C0D}) < 0x7F;
                 ++visit)
            {
                const auto x = visit % 2 ? 5U : 6U;
                const auto y = visit % 2 ? 7U : 5U;
                if (!walk_route(campaign, folder, x, y, fights, true))
                {
                    return false;
                }
                check(campaign.session.explore(opengold::por::ExplorationCommand::look),
                      "Search the actual aircar duct column for its ECGs");
                if (!settle_route(campaign, folder, fights))
                {
                    return false;
                }
                std::cout << "Actual aircar ECG search mask "
                          << campaign.session.script_variable(opengold::por::EclAddress{0x4C0D})
                          << ", native column " << x << ".\n";
            }
            check(campaign.session.script_variable(opengold::por::EclAddress{0x4C0D}) >= 0x7F,
                  "Clear two actual ECG encounters in distinct native columns");
            if (!rig_scientists())
            {
                return false;
            }
            check(campaign.session.script_variable(opengold::por::EclAddress{0x4C0F}) == 6 &&
                      campaign.session.script_variable(opengold::por::EclAddress{0x4C14}) == 0,
                  "Win the real commando battle and finish the authored gas defense");
            if (!rig_clinic() || !rig_deck())
            {
                return false;
            }
            check((campaign.session.script_variable(opengold::por::EclAddress{0x4C04}) & 0x80) != 0,
                  "Defeat the actual last shuttle-deck wave");
            if (!rig_clinic())
            {
                return false;
            }
            std::cout << "Actual final rig last wave cleared after " << campaign.ground_victories
                      << " ground victories, robot search "
                      << campaign.session.script_variable(opengold::por::EclAddress{0x4C09})
                      << ".\n";
            saved_campaign_round_trips(campaign, folder, false);
            for (const auto [x, y, facing] :
                 {std::tuple{1U, 7U, Facing::west}, std::tuple{2U, 7U, Facing::east},
                  std::tuple{1U, 8U, Facing::west}, std::tuple{2U, 8U, Facing::east},
                  std::tuple{3U, 7U, Facing::west}, std::tuple{5U, 7U, Facing::south}})
            {
                if (campaign.session.script_variable(opengold::por::EclAddress{0x4C09}) >= 6)
                {
                    break;
                }
                check(campaign.session.map().at(x, y).walls[static_cast<unsigned>(facing)] == 7,
                      "The actual rig search target is a native robot wall");
                if (!interact_wall_route(campaign, folder, x, y, facing, fights))
                {
                    return false;
                }
            }
            check(campaign.session.script_variable(opengold::por::EclAddress{0x4C09}) >= 6,
                  "Ordinary distinct robot-wall interactions complete the actual rig search");
            if (!rig_clinic() || !rig_scientists())
            {
                return false;
            }
            check(campaign.session.script_variable(opengold::por::EclAddress{0x4C0F}) == 7,
                  "The actual search leads to the authored power failure and mine hunt");
            if (!rig_aircar(false))
            {
                return false;
            }
            for (const auto [x, y] : {std::pair{2U, 1U}, std::pair{5U, 14U}})
            {
                if (campaign.session.script_variable(opengold::por::EclAddress{0x4C0C}) > 4)
                {
                    break;
                }
                if (!walk_route(campaign, folder, x, y, fights, true))
                {
                    return false;
                }
                if (!look_toward_route(campaign, folder, fights, Facing::east))
                {
                    return false;
                }
            }
            check(campaign.session.script_variable(opengold::por::EclAddress{0x4C0C}) > 4,
                  "Actual mine attempts resolve through their native successes or damage");
            if (!rig_scientists())
            {
                return false;
            }
            check(campaign.session.script_variable(opengold::por::EclAddress{0x4C0F}) == 8,
                  "The real pilot check and all three Refuge fights earn phase8");
            if (!rig_clinic() || !walk_route(campaign, folder, 15, 8, fights, true))
            {
                return false;
            }
            check((campaign.session.script_variable(opengold::por::EclAddress{0x4C05}) & 0x40) != 0,
                  "Boss Nikita recognizes the real Refuge victory and living ship's return");
            if (!rig_deck())
            {
                return false;
            }
            check(campaign.session.snapshot().game_won,
                  "The actual final celebration executes PROGRAM2 through the ordinary route");
        }
    }
    if (campaign.session.snapshot().game_won)
    {
        check(!campaign.terminal_party_save.empty() &&
                  campaign.terminal_records.size() == campaign.party->state().roster.size(),
              "The earned terminal Continue captured every real winning character");
        for (const auto &[id, record] : campaign.terminal_records)
        {
            check(campaign.party->game_record(id) == record,
                  "PROGRAM2 preserves each exact at-win PC and NPC record");
        }
        saved_campaign_round_trips(campaign, folder, true);
        const auto assets = opengold::campaign_asset_identity(folder);
        const auto won =
            opengold::encode_campaign(*campaign.party, &campaign.session, assets, matrix);
        auto fresh = opengold::load_campaign(folder, matrix);
        auto loaded = opengold::decode_campaign(won, *campaign.creation,
                                                campaign.party->rule_module(), assets, &fresh);
        auto restored = std::make_shared<opengold::CampaignParty>(
            og::rules_module(opengold::load_original_content(folder, matrix)), campaign.creation);
        restored->restore(std::move(loaded.party));
        check(loaded.town.has_value(), "Fresh assets restore the earned complete winning world");
        loaded.town->attach_restored_party(restored);
        check(
            loaded.town->snapshot().game_won && loaded.town->snapshot().tour_finished &&
                restored->state().roster.size() == campaign.terminal_records.size() &&
                opengold::encode_campaign(*restored, &*loaded.town, assets, matrix) == won,
            "Independent fresh-assets victory restore keeps all records and resaves byte-identically");
        for (const auto &[id, record] : campaign.terminal_records)
        {
            check(restored->game_record(id) == record &&
                      restored->member(id).npc_source == campaign.party->member(id).npc_source,
                  "Fresh winning restore retains the actual full at-win character identities");
        }
        auto captured = opengold::decode_campaign(campaign.terminal_party_save, *campaign.creation,
                                                  campaign.party->rule_module(), assets, nullptr);
        check(!captured.town && captured.party.roster.size() == campaign.terminal_records.size(),
              "The exact pre-Continue party artifact independently restores without a world");
        auto captured_party = std::make_shared<opengold::CampaignParty>(
            og::rules_module(opengold::load_original_content(folder, matrix)), campaign.creation);
        captured_party->restore(std::move(captured.party));
        for (const auto &[id, record] : campaign.terminal_records)
        {
            check(captured_party->game_record(id) == record &&
                      captured_party->member(id).npc_source ==
                          campaign.party->member(id).npc_source,
                  "The pre-Continue artifact retains every complete PC and NPC identity");
        }
        check(opengold::encode_campaign(*captured_party, nullptr, assets, matrix) ==
                  campaign.terminal_party_save,
              "The captured whole party resaves byte-identically");
        if (const auto *directory = std::getenv("OPENGOLD_MATRIX_PROOF_DIR"))
        {
            opengold::write_save_file(std::filesystem::path(directory) / "matrix-won.ogs", won,
                                      16U * 1024U * 1024U);
        }
        std::cout << "Genuine Matrix PROGRAM2 victory after " << campaign.ground_victories
                  << " ground victories; " << campaign.terminal_records.size()
                  << " exact at-win records and independent full/party save proofs passed.\n";
    }
    return campaign.session.snapshot().game_won;
}

bool play_genuine_opening(const std::filesystem::path &folder, bool require_full_ending,
                          bool preparation_only = false, bool easy = false, bool novice = false,
                          bool losangelorg_only = false)
{
    auto campaign = fresh_campaign(folder);
    if (novice)
    {
        campaign.session.difficulty(opengold::DifficultySetting::novice);
        std::cout << "Player selected the existing Novice campaign difficulty option.\n";
    }
    else if (easy)
    {
        campaign.session.difficulty(opengold::Difficulty::easy);
        std::cout << "Player selected the existing Easy campaign difficulty option.\n";
    }
    starts_with_matrix_records(campaign);
    std::map<std::string, unsigned> seen_menus;
    bool divided_booty = false;
    for (unsigned step = 0; step < 1000; ++step)
    {
        campaign.session.advance(100);
        const auto &s = campaign.session.snapshot();
        check(campaign.session.script_diagnostics().empty() && s.phase != TourPhase::faulted,
              "Matrix's real opening runs without unsupported events: " + s.diagnostic);
        if (s.phase == TourPhase::awaiting_continue)
        {
            check(!s.choices.empty(), "Matrix's pending menu offers an actual player choice");
            std::size_t pick = 0;
            if (s.dialogue.find("The booty:") != std::string::npos)
            {
                const auto wanted = divided_booty ? "Exit" : "Divvy";
                const auto choice = std::ranges::find(s.choices, wanted);
                check(choice != s.choices.end(), "The actual booty menu offers Divvy and Exit");
                pick = static_cast<std::size_t>(choice - s.choices.begin());
                divided_booty = true;
            }
            else
            {
                std::string menu = s.dialogue;
                for (const auto &choice : s.choices)
                {
                    menu += '\n' + choice;
                }
                if (++seen_menus[menu] > 2)
                {
                    pick = s.choices.size() - 1;
                }
            }
            check(campaign.session.choose(s.continue_ticket, pick),
                  "The player reads Matrix's opening pages");
            continue;
        }
        if (s.phase == TourPhase::completed)
        {
            saved_campaign_round_trips(campaign, folder, false);
            std::cout << "Genuine Matrix opening: script " << s.script_id << ", map " << s.area_id
                      << ", solar handoff " << s.solar_map << ".\n";
            if (require_full_ending)
            {
                return play_earned_route(campaign, folder, preparation_only, losangelorg_only);
            }
            return true;
        }
        if (s.phase != TourPhase::running)
        {
            std::cout << "Matrix opening stopped at an unimplemented player transaction, phase "
                      << static_cast<unsigned>(s.phase) << ".\n";
            return false;
        }
    }
    const auto &s = campaign.session.snapshot();
    const auto machine = opengold::test::TourAccess::machine(campaign.session);
    throw std::runtime_error("Matrix opening did not settle: script " +
                             std::to_string(s.script_id) + ", map " + std::to_string(s.area_id) +
                             ", phase " + std::to_string(static_cast<unsigned>(s.phase)) + ", pc " +
                             std::to_string(machine.address()) + ", solar " +
                             std::to_string(s.solar_map) + ", text " + s.dialogue.substr(0, 160));
}

// Isolated compatibility/host regression, separate from the earned route.
void dock_scratch_resumes_an_older_save(const std::filesystem::path &folder)
{
    auto campaign = fresh_campaign(folder);
    bool settled = false;
    for (unsigned step = 0; step < 1000; ++step)
    {
        campaign.session.advance(100);
        const auto &state = campaign.session.snapshot();
        if (state.phase == TourPhase::completed)
        {
            settled = true;
            break;
        }
        if (state.phase == TourPhase::awaiting_continue)
        {
            const auto exit = std::ranges::find(state.choices, "Exit");
            const auto pick = exit == state.choices.end()
                                  ? 0U
                                  : static_cast<unsigned>(exit - state.choices.begin());
            check(campaign.session.choose(state.continue_ticket, pick),
                  "Read the actual opening before the isolated legacy dock regression");
        }
        else
        {
            check(state.phase == TourPhase::running, "The actual opening stays supported");
        }
    }
    check(settled, "The actual opening reaches an ordinary save point");
    opengold::test::rejects<opengold::por::EclError>(
        [&]
        {
            (void)campaign.session.script_variable(opengold::por::EclAddress{0x7A00});
        });
    // A partial existing scratch cell must survive lazy initialization too.
    opengold::test::TourAccess::set_variable(campaign.session, opengold::por::EclAddress{0x7AF0},
                                             0xA5);
    const auto assets = opengold::campaign_asset_identity(folder);
    const auto bytes =
        opengold::encode_campaign_in_format(*campaign.party, &campaign.session, assets, 53, matrix);
    auto loaded = opengold::decode_campaign(
        bytes, *campaign.creation, campaign.party->rule_module(), assets, &campaign.session);
    check(loaded.town.has_value(), "The older Matrix town restores");
    auto restored = std::make_shared<opengold::CampaignParty>(
        og::rules_module(opengold::load_original_content(folder, matrix)), campaign.creation);
    restored->restore(std::move(loaded.party));
    loaded.town->attach_restored_party(restored);
    check(opengold::encode_campaign_in_format(*restored, &*loaded.town, assets, 53, matrix) ==
              bytes,
          "An older town resaves byte-identically before its next action");
    opengold::test::rejects<opengold::por::EclError>(
        [&]
        {
            (void)loaded.town->script_variable(opengold::por::EclAddress{0x7A00});
        });
    // The welcome was already read in this protocol fixture. Its next dock
    // event really SAVEs MAELSTROM RIDER to7A00h and PRINTs the label.
    const auto flags = loaded.town->script_variable(opengold::por::EclAddress{0x4C2F});
    opengold::test::TourAccess::set_variable(*loaded.town, opengold::por::EclAddress{0x4C2F},
                                             flags | 4);
    check(loaded.town->explore(opengold::por::ExplorationCommand::look),
          "Read the actual dock label through a normal player event");
    std::string text;
    for (unsigned step = 0; step < 100; ++step)
    {
        loaded.town->advance(100);
        const auto &state = loaded.town->snapshot();
        text += state.dialogue;
        check(state.phase != TourPhase::faulted && loaded.town->script_diagnostics().empty(),
              "The actual dock SAVE/PRINT runs after loading the older scratch table");
        if (state.phase == TourPhase::completed)
        {
            break;
        }
        if (state.phase == TourPhase::awaiting_continue)
        {
            check(loaded.town->choose(state.continue_ticket, 0), "Read the dock's actual label");
        }
    }
    check(text.find("MAELSTROM RIDER") != std::string::npos &&
              loaded.town->script_variable(opengold::por::EclAddress{0x7A00}) == 'M' &&
              loaded.town->script_variable(opengold::por::EclAddress{0x7AF0}) == 0xA5,
          "The real dock string is written and displayed without clearing existing scratch data");
}

// Installed encounter protocol only: no campaign prerequisite or outcome
// is supplied by this inspection of the authored Venus ambush.
void venus_distant_encounter_protocol(const std::filesystem::path &folder)
{
    auto campaign = fresh_campaign(folder);
    for (unsigned page = 0; page < 1000 && !campaign.session.can_leave(); ++page)
    {
        campaign.session.advance(100);
        const auto state = campaign.session.snapshot();
        check(state.phase != TourPhase::faulted && campaign.session.script_diagnostics().empty(),
              "Read the actual opening before inspecting the Venus encounter");
        if (state.phase == TourPhase::awaiting_continue)
        {
            const auto exit = std::ranges::find(state.choices, "Exit");
            const auto pick = exit == state.choices.end()
                                  ? 0U
                                  : static_cast<unsigned>(exit - state.choices.begin());
            check(campaign.session.choose(state.continue_ticket, pick),
                  "Read the actual opening pages");
        }
    }
    check(campaign.session.can_leave(), "The actual opening reaches ordinary idle exploration");
    const auto program = opengold::test::TourAccess::program(campaign.session, 48);
    const auto setup = program->instruction(0x9756);
    check(opengold::por::ecl_opcode(setup.opcode, matrix).name == "SETUP MONSTER" &&
              setup.operands.size() == 4 && setup.operands[0].value == 255 &&
              setup.operands[1].value == 3 && setup.operands[3].value == 15,
          "The installed Venus ambush requests distance three from all sides");
    opengold::test::TourAccess::run_from(campaign.session, 48, setup.address);
    for (unsigned page = 0; page < 100; ++page)
    {
        campaign.session.advance(100);
        const auto state = campaign.session.snapshot();
        std::string diagnostic = state.diagnostic;
        for (const auto &message : campaign.session.script_diagnostics())
        {
            diagnostic += '\n' + message;
        }
        check(state.phase != TourPhase::faulted && campaign.session.script_diagnostics().empty(),
              "The authored distance-three encounter remains supported: " + diagnostic);
        if (state.phase == TourPhase::combat)
        {
            check(campaign.session.pending_encounter().has_value(),
                  "The actual COMBAT publishes its pending encounter");
            const auto &encounter = *campaign.session.pending_encounter();
            check(encounter.enemies.size() == 10 && encounter.distance <= 2 &&
                      encounter.enemy_quarters.size() == encounter.enemies.size(),
                  "The authored ten enemies retain the existing combat distance and side limits");
            check(!state.game_won && campaign.ground_victories == 0,
                  "An isolated encounter inspection supplies no victory");
            return;
        }
        if (state.phase == TourPhase::awaiting_continue)
        {
            check(campaign.session.choose(state.continue_ticket, 0),
                  "Read the authored ambush pages without supplying an outcome");
        }
        else
        {
            check(state.phase == TourPhase::running,
                  "The authored ambush reaches its ordinary combat transaction");
        }
    }
    throw std::runtime_error("The authored Venus ambush did not reach combat");
}

// Source-program diagnostic only: an arrival keeps its previous controller
// until the wilderness's pre-step entry writes48. None of this setup is a win.
void venus_arrival_save_protocol(const std::filesystem::path &folder)
{
    const auto settle = [](Campaign &campaign)
    {
        for (unsigned page = 0; page < 1000; ++page)
        {
            campaign.session.advance(100);
            const auto state = campaign.session.snapshot();
            check(state.phase != TourPhase::faulted &&
                      campaign.session.script_diagnostics().empty(),
                  "The real Venus initialization remains supported");
            if (state.phase == TourPhase::completed)
            {
                return;
            }
            if (state.phase == TourPhase::awaiting_continue)
            {
                const auto exit = std::ranges::find(state.choices, "Exit");
                const auto pick = exit == state.choices.end()
                                      ? 0U
                                      : static_cast<unsigned>(exit - state.choices.begin());
                check(campaign.session.choose(state.continue_ticket, pick),
                      "Read actual Venus arrival pages");
            }
            else
            {
                check(state.phase == TourPhase::running,
                      "The Venus diagnostic assumes no combat result");
            }
        }
        throw std::runtime_error("Venus arrival diagnostic did not settle");
    };
    for (const auto [previous, x, y] :
         {std::tuple{19U, 5U, 2U}, std::tuple{49U, 34U, 12U}, std::tuple{50U, 35U, 9U}})
    {
        auto campaign = fresh_campaign(folder);
        settle(campaign);
        const auto venus = opengold::test::TourAccess::program(campaign.session, 48);
        opengold::test::TourAccess::set_variable(campaign.session,
                                                 opengold::por::EclAddress{0x4BF2}, previous);
        opengold::test::TourAccess::run_from(campaign.session, 48, venus->entries().at(4));
        settle(campaign);
        check(
            campaign.session.script_variable(opengold::por::EclAddress{0x4BF2}) == previous &&
                campaign.session.snapshot().script_id == 48 &&
                campaign.session.snapshot().overland_x == x &&
                campaign.session.snapshot().overland_y == y,
            "Actual Venus init preserves its source controller and publishes its authored arrival square");
        retained_wilderness_save_checks(campaign, folder);
        const auto assets = opengold::campaign_asset_identity(folder);
        for (const auto cell :
             {opengold::por::EclAddress{0x4BF2}, opengold::por::EclAddress{0x4BC3},
              opengold::por::game_address(opengold::por::ecl_script_bank, matrix)})
        {
            auto changed = campaign.session.detached_copy();
            const auto old = changed.script_variable(cell);
            opengold::test::TourAccess::set_variable(changed, cell, old + 1);
            for (const unsigned format : {53U, 55U})
            {
                const auto saved = opengold::encode_campaign_in_format(*campaign.party, &changed,
                                                                       assets, format, matrix);
                auto fresh = opengold::load_campaign(folder, matrix);
                bool refused = false;
                try
                {
                    (void)opengold::decode_campaign(saved, *campaign.creation,
                                                    campaign.party->rule_module(), assets, &fresh);
                }
                catch (const std::runtime_error &error)
                {
                    refused =
                        std::string_view(error.what()) == "Invalid saved Matrix wilderness context";
                }
                check(
                    refused,
                    "A checksum-valid Venus arrival rejects wrong prior controller, square or bank");
            }
        }
        if (previous == 19)
        {
            check(campaign.session.explore(opengold::por::ExplorationCommand::east),
                  "The real Venus arrival accepts an ordinary first movement");
            settle(campaign);
            check(campaign.session.script_variable(opengold::por::EclAddress{0x4BF2}) == 48 &&
                      campaign.session.snapshot().overland_x == x + 1,
                  "The actual first step establishes the normal wilderness controller receipt");
            retained_wilderness_save_checks(campaign, folder);
        }
    }
}

// An isolated installed-program regression. The full earned route reaches
// this same controller through the real assignment and Solar arrival.
void installed_wilderness_save_protocol(const std::filesystem::path &folder)
{
    auto campaign = fresh_campaign(folder);
    const auto read_pages = [&](Campaign &reading)
    {
        for (unsigned page = 0; page < 1000; ++page)
        {
            reading.session.advance(100);
            const auto state = reading.session.snapshot();
            std::string diagnostic = state.diagnostic;
            for (const auto &message : reading.session.script_diagnostics())
            {
                diagnostic += '\n' + message;
            }
            check(state.phase != TourPhase::faulted && reading.session.script_diagnostics().empty(),
                  "The installed wilderness fixture stays supported: " + diagnostic);
            if (state.phase == TourPhase::completed)
            {
                return;
            }
            if (state.phase == TourPhase::awaiting_continue)
            {
                const auto exit = std::ranges::find(state.choices, "Exit");
                const auto pick = exit == state.choices.end()
                                      ? 0U
                                      : static_cast<unsigned>(exit - state.choices.begin());
                check(reading.session.choose(state.continue_ticket, pick),
                      "Read the actual installed wilderness setup");
            }
            else
            {
                check(state.phase == TourPhase::running,
                      "The isolated setup needs no assumed fight outcome, script " +
                          std::to_string(state.script_id) + ", phase " +
                          std::to_string(static_cast<unsigned>(state.phase)));
            }
        }
        throw std::runtime_error("The installed wilderness fixture did not settle");
    };
    // Reading the actual opening establishes its portrait/host state too.
    // Rebuilding a bare inspection VM would skip that original setup.
    read_pages(campaign);
    const auto world = opengold::test::TourAccess::program(campaign.session, 32);
    opengold::test::TourAccess::run_from(campaign.session, 32, world->entries().at(4));
    read_pages(campaign);
    retained_wilderness_save_checks(campaign, folder);
    const auto y = campaign.session.script_variable(opengold::por::EclAddress{0x4BC4});
    check(campaign.session.explore(opengold::por::ExplorationCommand::north),
          "The installed wilderness accepts an ordinary movement command");
    read_pages(campaign);
    check(campaign.session.script_variable(opengold::por::EclAddress{0x4BC4}) + 1 == y,
          "The ordinary step advances the actual wilderness square");
    check(campaign.session.script_variable(opengold::por::EclAddress{0xC04D}) == 0,
          "North reaches the actual eight-way heading read by the wilderness controller");
    retained_wilderness_save_checks(campaign, folder, false);
    check(campaign.session.snapshot().overland_x > campaign.session.map().width &&
              campaign.session.snapshot().overland_y + 1 == y,
          "Wilderness publication uses its real square outside the retained 16x16 district");
    // Deliberately stale district coordinates exercise the early bounds guard;
    // this isolated protocol fixture never supplies an earned-route result.
    opengold::test::TourAccess::set_variable(campaign.session, opengold::por::EclAddress{0xC04B},
                                             30);
    opengold::test::TourAccess::set_variable(campaign.session, opengold::por::EclAddress{0xC04C},
                                             30);
    check(campaign.session.explore(opengold::por::ExplorationCommand::east),
          "Actual wilderness movement remains available outside retained district bounds");
    read_pages(campaign);
    check(campaign.session.snapshot().overland &&
              campaign.session.snapshot().overland_direction == 2 &&
              campaign.session.snapshot().overland_x ==
                  campaign.session.script_variable(opengold::por::EclAddress{0x4BC3}),
          "The native wilderness heading and coordinates publish before the 3D bounds guard");
    retained_wilderness_save_checks(campaign, folder, false);
    check(campaign.session.script_variable(opengold::por::EclAddress{0xC04D}) == 2,
          "East updates the controller's actual terrain lookup heading");
    const auto district = opengold::test::TourAccess::program(campaign.session, 17);
    opengold::test::TourAccess::run_from(campaign.session, 17, district->entries().at(4));
    read_pages(campaign);
    check(!campaign.session.snapshot().overland && !campaign.session.snapshot().solar_map,
          "Leaving the wilderness clears its movement publication");
    // The preceding diagnostic intentionally corrupted obsolete3D cells.
    // Actual district initialization above has now restored their real pose.
    opengold::test::TourAccess::run_from(campaign.session, 32, world->entries().at(4));
    read_pages(campaign);
    for (const auto command :
         {opengold::por::ExplorationCommand::north, opengold::por::ExplorationCommand::east})
    {
        check(campaign.session.explore(command),
              "Walk from the real landing square toward its port");
        read_pages(campaign);
    }
    check(campaign.session.explore(opengold::por::ExplorationCommand::southwest),
          "Walk back onto the actual spaceport tile with a native diagonal heading");
    read_pages(campaign);
    check(campaign.session.snapshot().script_id == 19 && campaign.session.snapshot().solar_map &&
              !campaign.session.snapshot().overland &&
              campaign.session.script_variable(opengold::por::EclAddress{0xC04D}) == 5,
          "The authored spaceport departure reaches Solar with its retained eight-way heading");
    saved_campaign_round_trips(campaign, folder, false);
    const auto warren = opengold::test::TourAccess::program(campaign.session, 33);
    const auto upper = opengold::test::TourAccess::program(campaign.session, 37);
    const auto stair = warren->instruction(0x820A);
    check(opengold::por::ecl_opcode(stair.opcode, matrix).name == "NEW ECL" &&
              stair.operands.size() == 1 && stair.operands.front().value == 37,
          "The installed warren staircase enters its other controller");
    opengold::test::TourAccess::run_from(campaign.session, 33, warren->entries().at(4));
    read_pages(campaign);
    const auto retained_map = campaign.session.snapshot().area_id;
    opengold::test::TourAccess::run_from(campaign.session, 37, upper->entries().at(4));
    read_pages(campaign);
    check(retained_map == 33 && campaign.session.snapshot().area_id == retained_map &&
              campaign.session.snapshot().script_id == 37,
          "The installed upper warren retains its actual district");
    retained_interior_save_checks(campaign, folder);
    for (const auto [base_id, next_id, transition] :
         {std::tuple{34U, 36U, 0x836FU}, std::tuple{81U, 84U, 0x87ADU},
          std::tuple{97U, 98U, 0x8302U}})
    {
        auto floor_campaign = fresh_campaign(folder);
        read_pages(floor_campaign);
        std::cout << "Retained-floor source protocol " << base_id << " -> " << next_id << ".\n";
        const auto base = opengold::test::TourAccess::program(floor_campaign.session, base_id);
        const auto next = opengold::test::TourAccess::program(floor_campaign.session, next_id);
        const auto entry = base->instruction(transition);
        check(opengold::por::ecl_opcode(entry.opcode, matrix).name == "NEW ECL" &&
                  entry.operands.size() == 1 && entry.operands.front().value == next_id &&
                  std::ranges::none_of(opengold::por::walk_reachable(*next).instructions,
                                       [](const auto &instruction)
                                       {
                                           return instruction.opcode == 33;
                                       }),
              "Each installed retained-floor transition has no replacement map load");
        if (base_id == 81)
        {
            // Isolated protocol setup, never counted as an earned entry.
            opengold::test::TourAccess::set_variable(floor_campaign.session,
                                                     opengold::por::EclAddress{0x4C86}, 1);
        }
        if (base_id == 97)
        {
            check(floor_campaign.session.explore(opengold::por::ExplorationCommand::look),
                  "Initialize native scratch cells before the isolated ship protocol");
            read_pages(floor_campaign);
            opengold::test::TourAccess::set_variable(floor_campaign.session,
                                                     opengold::por::EclAddress{0x4BF2}, 96);
        }
        opengold::test::TourAccess::run_from(floor_campaign.session, base_id,
                                             base->entries().at(4));
        read_pages(floor_campaign);
        const auto map = floor_campaign.session.snapshot().area_id;
        check(map == base_id, "The installed source controller loads its actual map");
        if (base_id == 97)
        {
            opengold::test::TourAccess::place(floor_campaign.session,
                                              {2, 8, opengold::por::MapDirection::north});
        }
        opengold::test::TourAccess::run_from(floor_campaign.session, next_id,
                                             next->entries().at(4));
        read_pages(floor_campaign);
        retained_interior_save_checks(floor_campaign, folder, next_id, map);
    }
}

// Isolated installed-source protocol; never an earned campaign entry or win.
void retained_ship_wall_refreshes_before_the_next_command(const std::filesystem::path &folder)
{
    auto campaign = fresh_campaign(folder);
    unsigned fights = 0;
    check(settle_route(campaign, folder, fights),
          "Read the installed opening before this isolated protocol");
    check(campaign.session.explore(opengold::por::ExplorationCommand::look) &&
              settle_route(campaign, folder, fights),
          "Initialize native temporary cells through a real command");
    const auto ship = opengold::test::TourAccess::program(campaign.session, 97);
    opengold::test::TourAccess::set_variable(campaign.session, opengold::por::EclAddress{0x4BF2},
                                             19);
    opengold::test::TourAccess::run_from(campaign.session, 97, ship->entries().at(4));
    check(settle_route(campaign, folder, fights), "The actual Fungus setup loads its native map");
    opengold::test::TourAccess::place(campaign.session, {9, 10, opengold::por::MapDirection::west});
    // Invoke the actual accepted passage instruction after the isolated
    // setup; its SAVE/NEW sequence, not this fixture, moves the party.
    opengold::test::TourAccess::run_from(campaign.session, 97, 0x830E);
    check(settle_route(campaign, folder, fights), "Read the actual installed upward passage");
    const auto state = campaign.session.snapshot();
    check(state.script_id == 96 && state.area_id == 96 && state.pose.x == 8 && state.pose.y == 10 &&
              state.pose.facing == opengold::por::MapDirection::west &&
              campaign.session.map().at(8, 10).walls[3] == 0,
          "The installed source moves into the retained map with an open way west");
    check(campaign.session.script_variable(opengold::por::EclAddress{0xC04E}) == 5,
          "The source retained its old wall cell without requesting a redraw");
    const auto assets = opengold::campaign_asset_identity(folder);
    for (const unsigned format : {53U, 55U})
    {
        const auto saved = opengold::encode_campaign_in_format(*campaign.party, &campaign.session,
                                                               assets, format, matrix);
        auto fresh = opengold::load_campaign(folder, matrix);
        auto loaded = opengold::decode_campaign(saved, *campaign.creation,
                                                campaign.party->rule_module(), assets, &fresh);
        check(loaded.town.has_value(), "Both supported ship saves restore an actual world");
        auto restored = std::make_shared<opengold::CampaignParty>(
            og::rules_module(opengold::load_original_content(folder, matrix)), campaign.creation);
        restored->restore(std::move(loaded.party));
        loaded.town->attach_restored_party(restored);
        check(opengold::encode_campaign_in_format(*restored, &*loaded.town, assets, format,
                                                  matrix) == saved,
              "An idle retained-wall save stays byte-identical until the next player action");
        check(loaded.town->explore(opengold::por::ExplorationCommand::forward),
              "The next public Forward command is accepted");
        loaded.town->advance(100);
        check(loaded.town->snapshot().pose.x == 7 && loaded.town->snapshot().pose.y == 10 &&
                  !loaded.town->snapshot().dialogue.starts_with("THE PASSAGE LEADS"),
              "The next Forward refreshes the real wall instead of repeating the obsolete passage");
        check(
            !loaded.town->snapshot().game_won && loaded.town->script_diagnostics().empty(),
            "This isolated movement protocol invents no victory and runs no unsupported instruction");
    }
}

// Installed-source failure protocol only; never an earned campaign win.
void authored_solo_loss_preserves_the_rest_of_the_team(const std::filesystem::path &folder)
{
    for (const bool solo : {true, false})
    {
        auto campaign = fresh_campaign(folder);
        campaign.session.difficulty(opengold::DifficultySetting::novice);
        const auto prison = opengold::test::TourAccess::program(campaign.session, 80);
        const auto read_to_boundary = [&]()
        {
            for (unsigned step = 0; step < 1000; ++step)
            {
                campaign.session.advance(100);
                const auto state = campaign.session.snapshot();
                check(state.phase != TourPhase::faulted &&
                          campaign.session.script_diagnostics().empty(),
                      "The isolated installed loss protocol stays supported");
                if (state.phase == TourPhase::completed || state.phase == TourPhase::combat)
                {
                    return;
                }
                if (state.phase == TourPhase::awaiting_continue)
                {
                    const auto exit = std::ranges::find(state.choices, "Exit");
                    const auto pick = exit == state.choices.end()
                                          ? 0U
                                          : static_cast<unsigned>(exit - state.choices.begin());
                    check(campaign.session.choose(state.continue_ticket, pick),
                          "Read the installed protocol's actual setup pages");
                    continue;
                }
                check(state.phase == TourPhase::running,
                      "The loss protocol reaches its native boundary");
            }
            throw std::runtime_error("The installed loss protocol did not settle");
        };
        read_to_boundary();
        opengold::test::TourAccess::run_from(campaign.session, 80, prison->entries().at(4));
        read_to_boundary();
        check(campaign.session.snapshot().phase == TourPhase::completed,
              "The authored prison initialization loads its actual map before this diagnostic");
        const auto entry = solo ? 0x900EU : 0x9D32U;
        opengold::test::TourAccess::run_from(campaign.session, 80, entry);
        read_to_boundary();
        check(campaign.session.snapshot().phase == TourPhase::combat &&
                  campaign.session.pending_encounter().has_value(),
              "The actual installed script opens the requested native fight");
        const auto champion = campaign.session.pending_encounter()->champion;
        check((champion != 0) == solo, "Only the actual SOLO COMBAT chooses one champion");
        const auto roster = campaign.party->state().roster.size();
        std::map<opengold::MemberId, std::pair<unsigned, unsigned>> waiting;
        if (solo)
        {
            for (const auto id : campaign.party->state().slots)
            {
                if (!id || id == champion)
                {
                    continue;
                }
                const auto record = og::buck_rogers_record_of(campaign.party->game_record(id));
                check(record && record->creature.enabled != 0,
                      "Other real PCs wait outside the solo bout");
                waiting.emplace(
                    id, std::pair{record->creature.hit_points, record->creature.experience});
            }
        }
        if (campaign.session.monster_picture())
        {
            check(campaign.session.start_encounter(),
                  "Dismiss the actual protocol's monster picture");
        }
        opengold::CombatDemo combat(
            og::rules_module(opengold::load_original_content(folder, matrix)));
        combat.campaign_party(campaign.party);
        combat.encounter(*campaign.session.pending_encounter(), 20261124);
        for (unsigned turn = 0;
             turn < 8000 && combat.combat().snapshot().outcome == opengold::rules::Outcome::ongoing;
             ++turn)
        {
            const auto state = combat.combat().snapshot();
            const auto actor = std::ranges::find(state.combatants, state.actor,
                                                 &opengold::rules::CombatantView::id);
            check(actor != state.combatants.end(), "The native loss protocol has its real actor");
            const auto choices = combat.combat().legal_commands();
            const bool player =
                actor->side == opengold::rules::Side::party && !opengold::is_ally(actor->id);
            auto order =
                std::ranges::find(choices, player ? std::string("end") : std::string("quick"),
                                  &opengold::rules::Command::verb);
            if (order == choices.end())
            {
                order = std::ranges::find(choices, std::string("quick"),
                                          &opengold::rules::Command::verb);
            }
            check(order != choices.end() && combat.submit(*order),
                  "The diagnostic loses only through actual offered native combat actions");
        }
        const auto result = combat.combat().snapshot();
        check(result.outcome == opengold::rules::Outcome::defeat,
              "The actual native combat, not an invented snapshot, reports defeat");
        check(campaign.session.resolve_combat(result), "Commit the actual native defeat");
        check(!campaign.session.snapshot().game_won &&
                  campaign.party->state().roster.size() == roster,
              "A failed native fight grants no victory and preserves the full actual roster");
        if (!solo)
        {
            check(campaign.session.snapshot().phase == TourPhase::defeated,
                  "A full-team defeat remains terminal");
            continue;
        }
        check(campaign.session.snapshot().phase != TourPhase::defeated,
              "A lost solo bout does not destroy the healthy PCs waiting outside it");
        check(campaign.session.script_variable(opengold::por::EclAddress{0x7EC7}) == 0x80,
              "The script hears the real solo defeat, never a fabricated victory");
        bool dropped = false;
        for (unsigned page = 0; page < 100; ++page)
        {
            campaign.session.advance(100);
            const auto state = campaign.session.snapshot();
            if (state.dialogue.find("BACK DOWN THE HOLE!") != std::string::npos)
            {
                dropped = true;
                break;
            }
            check(state.phase != TourPhase::defeated && state.phase != TourPhase::faulted,
                  "The actual loss continues through its authored fallback");
            if (state.phase == TourPhase::awaiting_continue)
            {
                check(campaign.session.choose(state.continue_ticket, 0),
                      "Read the authored failure page");
            }
        }
        check(dropped, "The actual installed dog drops the defeated climber back into the hole");
        for (const auto &[id, values] : waiting)
        {
            const auto record = og::buck_rogers_record_of(campaign.party->game_record(id));
            check(record && record->creature.hit_points == values.first &&
                      record->creature.experience == values.second,
                  "Waiting PCs retain their actual health and earned experience");
        }
    }
    std::cout << "Installed solo-loss and full-team-defeat protocols passed (diagnostic only).\n";
}

void actual_program_two_is_a_saved_terminal_win(const std::filesystem::path &folder)
{
    auto campaign = fresh_campaign(folder);
    const auto &program = *opengold::test::TourAccess::programs(campaign.session).at(114);
    const auto ending = program.instruction(0x9E08);
    check(ending.opcode == 0x38 && ending.operands.size() == 1 &&
              ending.operands.front().value == 2 && program.instruction(ending.next).opcode == 0,
          "Matrix's actual PROGRAM 2 immediately precedes EXIT, with no omitted epilogue");
    // This diagnostic invokes only the installed ending instruction. It does
    // not earn the preceding campaign and cannot certify an earned win.
    opengold::test::TourAccess::inspect_instruction(campaign.session, 114, ending.address);
    const auto &s = campaign.session.snapshot();
    std::cout << "Matrix PROGRAM 2 inspection: phase " << static_cast<unsigned>(s.phase) << ", won "
              << s.game_won << ", pc "
              << opengold::test::TourAccess::machine(campaign.session).address() << ", diagnostic "
              << s.diagnostic << ".\n";
    check(s.game_won && s.phase == TourPhase::completed &&
              s.dialogue.find("You have won.") != std::string::npos && campaign.session.can_leave(),
          "The authored PROGRAM 2 ends Matrix and permits an ordinary winning save");
    saved_campaign_round_trips(campaign, folder, true);
}
} // namespace

int main(int argc, char **argv)
{
    try
    {
        const auto folder = opengold::test::installed_game_folder(matrix);
        if (folder.empty())
        {
            std::cout << "Matrix Cubed's files are not here.\n";
            return 0;
        }
        if (argc > 1 && std::string_view(argv[1]) == "--buck-solo-loss-protocol")
        {
            authored_solo_loss_preserves_the_rest_of_the_team(folder);
            return 0;
        }
        const bool easy = argc > 1 && std::string_view(argv[1]) == "--earned-ending-easy";
        const bool losangelorg = argc > 1 && std::string_view(argv[1]) == "--earned-losangelorg";
        const bool novice =
            losangelorg || (argc > 1 && std::string_view(argv[1]) == "--earned-ending-novice");
        if (argc > 1 && std::string_view(argv[1]) == "--buck-retained-wall-protocol")
        {
            retained_ship_wall_refreshes_before_the_next_command(folder);
            std::cout << "Installed retained-wall protocol passed (diagnostic only).\n";
            return 0;
        }
        const bool full =
            novice || easy || (argc > 1 && std::string_view(argv[1]) == "--earned-ending");
        const bool preparation =
            argc > 1 && std::string_view(argv[1]) == "--earned-caloris-preparation";
        if (argc == 1)
        {
            native_difficulty_keeps_all_six_settings();
            dock_scratch_resumes_an_older_save(folder);
            venus_distant_encounter_protocol(folder);
            venus_arrival_save_protocol(folder);
            installed_wilderness_save_protocol(folder);
            matrix_format_preserves_released_history(folder);
            retained_ship_wall_refreshes_before_the_next_command(folder);
            authored_solo_loss_preserves_the_rest_of_the_team(folder);
            actual_program_two_is_a_saved_terminal_win(folder);
            std::cout << "Authored Matrix ending protocol roundtrip passed (diagnostic only).\n";
        }
        if (!play_genuine_opening(folder, full || preparation, preparation, easy, novice,
                                  losangelorg))
        {
            return 1;
        }
        std::cout
            << (full
                    ? "Matrix's full earned campaign ending and save proofs passed.\n"
                    : "Matrix campaign protocol checks passed; full earned ending remains separate.\n");
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
