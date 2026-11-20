// Countdown's earned campaign player. Unlike the diagnostic map walk, this
// starts once and uses ordinary movement, choices and actual rules combat.
#include "opengold/buck_rogers_characters.h"
#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/buck_rogers_upkeep.h"
#include "opengold/campaign_save.h"
#include "opengold/campaign_session.h"
#include "opengold/combat_demo.h"
#include "opengold/formats.h"
#include "opengold/original.h"
#include "opengold/original_content.h"
#include "installed_games.h"
#include "tour_access.h"
#include "test_support.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
using opengold::GoldBoxGame;
using opengold::test::check;
namespace og = opengold::original;
namespace br = opengold::buck_rogers;
using og::Profession;
using og::Race;
using og::Skill;

constexpr auto countdown = GoldBoxGame::countdown_to_doomsday;

std::vector<std::uint8_t> file_bytes(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

std::vector<std::uint8_t> record_of(const std::filesystem::path &path, unsigned id)
{
    const auto archive = opengold::decode_dax_archive(file_bytes(path));
    for (const auto &record : archive.records)
    {
        if (record.id == id)
        {
            return record.bytes;
        }
    }
    return {};
}

// A member of the team as Countdown's creation makes it (chargen.pas
// CreateCharacterMenu): rolled, trained free to level 2, its creation points
// spent on its career's skills in their order and its general points on the
// general skills the team's travels ask for, with its two free items readied.
og::GroundRecord new_member(const std::filesystem::path &folder, Race race, Profession career,
                            std::string name, opengold::RandomState &random)
{
    const auto templates = br::decode_item_templates(record_of(folder / "ITEM0.DAX", 1));
    const auto types = br::decode_item_types(file_bytes(folder / "ITEMS"));
    check(templates && types, "Countdown's item templates and types read");
    auto starting_items = og::buck_rogers_starting_items(*templates);
    check(starting_items.size() == 2 && og::readied(starting_items.front()) &&
              og::readied(starting_items.back()),
          "The authored starting gun and spacesuit are both readied");
    const auto weapon = og::readied_weapon(starting_items, *types);
    const auto armor = og::readied_in_slot(starting_items, *types, og::gear_slot::armor);
    check(weapon && armor, "The actual item types identify the readied gun and armor");
    const auto weapon_type = starting_items[*weapon].type;
    auto creature =
        og::new_buck_rogers_character(race, og::Gender::male, career, countdown, random);
    // Reroll through the same creation command a player uses. Every accepted
    // ability and Hit Point is rolled by the rules; no profile is edited.
    const auto durable = [&]
    {
        return creature.current_abilities[1] >= 12 &&
               creature.hit_point_maximum >= (career == Profession::warrior ? 18 : 10);
    };
    unsigned rerolls = 0;
    while (!durable() && rerolls < 4096)
    {
        og::reroll_buck_rogers_character(creature, countdown, random);
        ++rerolls;
    }
    check(durable(), "Ordinary creation rerolls produce a durable recruit");
    creature.name = std::move(name);
    std::cout << "Created " << creature.name << " with " << creature.hit_point_maximum
              << " Hit Points, Dexterity " << unsigned(creature.current_abilities[1]) << " after "
              << rerolls << " ordinary rerolls.\n";
    if (career == Profession::warrior)
    {
        check(og::specialize(creature, weapon_type),
              "The Warrior spends its starting Bolt Gun specialization");
    }
    std::cout << "Starting gun type " << unsigned(weapon_type) << ", quantity "
              << unsigned(starting_items[*weapon].quantity) << ", specialization "
              << og::specialization_level(creature, weapon_type) << "; armor type "
              << unsigned(starting_items[*armor].type) << ".\n";
    auto spending = og::SkillSpending::at_creation(creature, countdown);
    std::vector<std::pair<Skill, unsigned>> career_choices;
    switch (career)
    {
    case Profession::rocket_jock:
        career_choices = {
            {Skill::pilot_rocket, 30}, {Skill::maneuver_in_zero_g, 20}, {Skill::notice, 30}};
        break;
    case Profession::medic:
        career_choices = {{Skill::treat_light_wounds, 30},
                          {Skill::treat_serious_wounds, 30},
                          {Skill::diagnose, 20}};
        break;
    case Profession::engineer:
        career_choices = {
            {Skill::repair_electrical, 30}, {Skill::repair_mechanical, 25}, {Skill::jury_rig, 25}};
        break;
    case Profession::rogue:
        career_choices = {
            {Skill::bypass_security, 30}, {Skill::open_lock, 25}, {Skill::hide_in_shadows, 25}};
        break;
    case Profession::warrior:
        career_choices = {{Skill::notice, 30},
                          {Skill::leadership, 20},
                          {Skill::battle_tactics, 20},
                          {Skill::maneuver_in_zero_g, 10}};
        break;
    default:
        throw std::runtime_error("The earned team's career has no creation skill plan");
    }
    const auto spend = [&](const auto &choices)
    {
        for (const auto &[skill, points] : choices)
        {
            for (unsigned point = 0; point < points; ++point)
            {
                check(spending.add(skill) == og::SkillRefusal::none,
                      "The actual creation budget permits " +
                          std::string(og::skill_definition(skill).name) + " point " +
                          std::to_string(point + 1));
            }
        }
    };
    spend(career_choices);
    const std::array<std::pair<Skill, unsigned>, 3> general_choices{
        {{Skill::first_aid, 20},
         {career == Profession::rogue ? Skill::acrobatics : Skill::climb, 10},
         {career == Profession::rogue ? Skill::library_search : Skill::fast_talk, 10}}};
    spend(general_choices);
    check(spending.career_points() == 0 && spending.general_points() == 0,
          "The creation plan spends exactly the ordinary available points");
    spending.apply_to(creature);
    return {std::move(creature), std::move(starting_items), *types};
}

struct Campaign
{
    std::shared_ptr<const opengold::rules::CharacterRules> creation;
    std::shared_ptr<opengold::CampaignParty> party;
    opengold::por::RolfTourSession session;
};

// The team of six the walk plays: one of each career and a second Warrior.
Campaign start_campaign(const std::filesystem::path &folder)
{
    std::shared_ptr<const opengold::rules::CharacterRules> creation =
        og::character_rules(og::default_aging, countdown);
    auto party = std::make_shared<opengold::CampaignParty>(
        og::rules_module(opengold::load_original_content(folder, countdown)), creation);
    opengold::RandomState random{2026};
    const std::vector<std::tuple<Race, Profession, std::string>> team{
        {Race::terran, Profession::rocket_jock, "Jock"},
        {Race::desert_runner, Profession::warrior, "Runner"},
        {Race::martian, Profession::rogue, "Shade"},
        {Race::tinker, Profession::engineer, "Gears"},
        {Race::venusian, Profession::medic, "Doc"},
        {Race::mercurian, Profession::warrior, "Merc"}};
    for (const auto &[race, career, name] : team)
    {
        const auto record = new_member(folder, race, career, name, random);
        party->add_pc(opengold::Character(*creation, og::buck_rogers_draft(record),
                                          opengold::por::CharacterAppearance{}));
    }
    auto session = opengold::load_campaign(folder, countdown);
    session.campaign_party(party);
    return {std::move(creation), std::move(party), std::move(session)};
}

class EarnedPlayer
{
  public:
    explicit EarnedPlayer(std::filesystem::path folder)
        : folder_(std::move(folder)), campaign_(start_campaign(folder_)),
          assets_(opengold::campaign_asset_identity(folder_))
    {
    }

    void play(std::vector<std::string> answers = {})
    {
        for (auto &answer : answers)
        {
            answers_.push_back(std::move(answer));
        }
        using opengold::por::TourPhase;
        auto &session = campaign_.session;
        for (unsigned screen = 0; screen < 1000; ++screen)
        {
            const auto snapshot = session.snapshot();
            switch (snapshot.phase)
            {
            case TourPhase::running:
                session.advance(1);
                break;
            case TourPhase::awaiting_continue:
            {
                while (!answers_.empty() && answers_.front().starts_with("select:"))
                {
                    check(session.can_select_member(),
                          "The ordinary inventory menu permits selecting its buyer");
                    campaign_.party->select(opengold::PartySlot{
                        static_cast<unsigned>(std::stoul(answers_.front().substr(7)))});
                    answers_.pop_front();
                }
                std::cout << "script " << snapshot.script_id << " at " << snapshot.pose.x << ','
                          << snapshot.pose.y << ": " << snapshot.dialogue.substr(0, 180) << " [";
                for (const auto &choice : snapshot.choices)
                {
                    std::cout << choice << '|';
                }
                std::cout << "]\n";
                if (snapshot.game_won && !winning_party_)
                {
                    winning_party_ =
                        opengold::encode_campaign(*campaign_.party, nullptr, assets_, countdown);
                }
                std::optional<std::size_t> chosen;
                if (!answers_.empty() && answers_.front() == "fill-ammo")
                {
                    const auto id = campaign_.party->selected();
                    const auto before = og::buck_rogers_record_of(campaign_.party->game_record(id));
                    check(before.has_value(), "The selected buyer has its actual native record");
                    const auto weapon = og::readied_weapon(before->items, before->types);
                    check(weapon.has_value(), "The buyer carries its actual readied gun");
                    const auto offer =
                        og::ammunition_offer(before->items[*weapon], before->types, true);
                    const auto found =
                        std::ranges::find_if(snapshot.choices.rbegin(), snapshot.choices.rend(),
                                             [](const std::string &choice)
                                             {
                                                 return choice.starts_with("Bolt Gun:");
                                             });
                    check(found != snapshot.choices.rend(),
                          "The shop offers the actual gun refill");
                    const auto index = snapshot.choices.size() - 1 -
                                       static_cast<std::size_t>(found - snapshot.choices.rbegin());
                    check(session.choose(snapshot.continue_ticket, index),
                          "Buy the offered ammunition through the ordinary shop");
                    const auto after = og::buck_rogers_record_of(campaign_.party->game_record(id));
                    check(after &&
                              after->items[*weapon].quantity ==
                                  before->items[*weapon].quantity + offer.rounds &&
                              after->creature.credits + offer.cost == before->creature.credits,
                          "The actual selected buyer receives and pays for its ammunition");
                    answers_.pop_front();
                    break;
                }
                if (snapshot.dialogue.starts_with("The booty:"))
                {
                    const auto label =
                        std::ranges::find(snapshot.choices, "Divvy") != snapshot.choices.end()
                            ? "Divvy"
                            : "Exit";
                    const auto found = std::ranges::find(snapshot.choices, label);
                    check(found != snapshot.choices.end(),
                          "The ordinary booty action is available");
                    chosen = static_cast<std::size_t>(found - snapshot.choices.begin());
                }
                else if (snapshot.choices.size() == 1)
                {
                    chosen = 0;
                }
                else if (!answers_.empty())
                {
                    const auto found = std::ranges::find_if(
                        snapshot.choices,
                        [&](const std::string &choice)
                        {
                            return choice.find(answers_.front()) != std::string::npos;
                        });
                    if (found != snapshot.choices.end())
                    {
                        chosen = static_cast<std::size_t>(found - snapshot.choices.begin());
                        answers_.pop_front();
                    }
                }
                if (!chosen && snapshot.dialogue.find("AIRSHAFT") != std::string::npos)
                {
                    const auto no = std::ranges::find(snapshot.choices, "NO");
                    if (no != snapshot.choices.end())
                    {
                        chosen = static_cast<std::size_t>(no - snapshot.choices.begin());
                    }
                }
                if (!chosen)
                {
                    for (const std::string_view choice : {"ATTACK", "FIGHT", "BASH", "YES"})
                    {
                        const auto found = std::ranges::find(snapshot.choices, choice);
                        if (found != snapshot.choices.end())
                        {
                            chosen = static_cast<std::size_t>(found - snapshot.choices.begin());
                            break;
                        }
                    }
                }
                check(chosen.has_value(), "Earned route needs an answer to: " + snapshot.dialogue);
                check(session.choose(snapshot.continue_ticket, *chosen), "The actual choice runs");
                break;
            }
            case TourPhase::combat:
                if (session.monster_picture())
                {
                    check(session.start_encounter(), "Start the actual ground encounter");
                }
                else
                {
                    ground_fight();
                }
                break;
            case TourPhase::treasure:
                check(session.leave_treasure(snapshot.continue_ticket), "Leave ordinary booty");
                break;
            case TourPhase::shopping:
                check(session.leave_shop(snapshot.continue_ticket), "Leave the ordinary shop");
                break;
            case TourPhase::awaiting_input:
                check(!answers_.empty(), "The route supplies the actual text input");
                check(session.input(snapshot.continue_ticket, answers_.front()),
                      "Submit the ordinary story input");
                answers_.pop_front();
                break;
            case TourPhase::ship_combat:
                throw std::runtime_error(
                    "Earned route requires the real ship-combat transaction; no assumed outcome");
            case TourPhase::defeated:
                throw std::runtime_error("The fresh earned team was defeated");
            case TourPhase::faulted:
                throw std::runtime_error("Earned campaign fault: " + snapshot.diagnostic);
            case TourPhase::completed:
                check(session.can_leave(), "The route stops in ordinary idle exploration");
                return;
            default:
                throw std::runtime_error("Unhandled earned input phase " +
                                         std::to_string(static_cast<unsigned>(snapshot.phase)));
            }
        }
        throw std::runtime_error("Earned event exceeded its command bound");
    }

    void opening()
    {
        play();
        const auto &s = campaign_.session.snapshot();
        check(s.area_id == 16 && s.script_id == 16 && s.pose.x == 5 && s.pose.y == 6,
              "The real opening reaches Chicagorg at5,6");
        check(campaign_.party->state().roster.size() == 6,
              "All six new recruits reach the opening");
        save_checkpoint("opening");
    }

    void chicagorg()
    {
        // Take the direct physical route to the eastern missile room. The
        // first officer on the way identifies its controls. After the rescue,
        // accept the normal first assignment and enter the derelict through
        // Salvation's port; its picture-menu loop is not an idle town save.
        // The cluebook's west entrance (event13) is an optional ambush which
        // fuses its door. Use the other physically connected entrance.
        walk(16, 14, 9, {"CHARGE",    "GRENADE",    "Runner",    "DEPOT", "select:0",  "Ammo",
                         "fill-ammo", "Back",       "select:1",  "Ammo",  "fill-ammo", "Back",
                         "select:2",  "Ammo",       "fill-ammo", "Back",  "select:3",  "Ammo",
                         "fill-ammo", "Back",       "select:4",  "Ammo",  "fill-ammo", "Back",
                         "select:5",  "Ammo",       "fill-ammo", "Back",  "Exit",      "PORT",
                         "LAUNCH",    "SEARCH SHIP"},
             {13});
        const auto &snapshot = campaign_.session.snapshot();
        check(snapshot.script_id == 32 && snapshot.area_id == 32 && fights_ > 0,
              "Saving Chicagorg and accepting the first job reaches the real derelict");
        check(answers_.empty(), "The technician and grenade choices were actually played");
        check(std::ranges::all_of(campaign_.party->state().roster,
                                  [](const auto &member)
                                  {
                                      return !member.npc_source.empty() ||
                                             (!member.vitals.dead && member.vitals.hit_points > 0);
                                  }),
              "Every fresh PC survives the first assignment");
        save_checkpoint("salvage-entry");
    }

    void camp_service()
    {
        auto &session = campaign_.session;
        const auto before = campaign_.party->checkpoint();
        const bool accepted = session.camp(opengold::RestKind::long_rest);
        std::cout << "Ordinary Countdown camp accepted: " << accepted << '\n';
        check(accepted, "The opening's authored camp entry allows the ordinary camp");
        play();
        check(campaign_.party->state().time_minutes == before.time_minutes + 24 * 60,
              "An ordinary native camp passes one real day");
        for (const auto &member : campaign_.party->state().roster)
        {
            check(og::holds_buck_rogers_record(member.vitals.resources),
                  "An ordinary camp retains each actual native Buck Rogers record");
            const auto old =
                std::ranges::find(before.roster, member.id, &opengold::PartyMember::id);
            check(old != before.roster.end() && member.vitals.resources == old->vitals.resources,
                  "Resting a healthy recruit preserves its actual complete native record");
        }
        save_checkpoint("camp-service");
    }

    void derelict()
    {
        // The installed ladders loop their ordinary Up/Down/Stay menu
        // after each floor. Leave the lower ladder on deck four, then
        // climb the adjoining ladder and leave it on deck five.
        walk(32, 0, 15, {"UP", "UP", "STAY"});
        check(answers_.empty(), "The lower ship's ladder choices were consumed");
        walk(32, 14, 14, {"UP", "STAY"});
        check(campaign_.session.snapshot().script_id == 33,
              "The ordinary deck-four ladder enters the upper ship");
        save_checkpoint("derelict-upper");
    }

    void scot()
    {
        auto &session = campaign_.session;
        check(session.explore(opengold::por::ExplorationCommand::look),
              "Look at the ordinary deck-five ladder");
        play({"UP", "STAY"});
        // The installed surgeon accepts Dr. Williams' sigma
        // number; its own operation removes parasites and makes the salve.
        walk(32, 14, 3, {"TAKE"});
        walk(32, 15, 2, {"YES", "Runner", "SPEAK", "A95151", "YES"});
        while (session.snapshot().pose.facing != opengold::por::MapDirection::east)
        {
            check(session.explore(opengold::por::ExplorationCommand::turn_right),
                  "Face the ordinary surgery console");
            play();
        }
        check(answers_.empty(), "The ordinary medical lab and surgery choices ran");
        walk(32, 15, 4, {"UP", "STAY"});
        walk(32, 9, 1, {"UP", "UP", "STAY"});
        walk(32, 1, 8, {}, {9});
        walk(32, 0, 8);
        check((session.script_variable(opengold::por::EclAddress{0x4C4E}) & 0x80) != 0,
              "Connecting the actual console lets Scot communicate");
        check((session.script_variable(opengold::por::EclAddress{0x4C52}) & 0x20) != 0,
              "The actual surgeon removes parasites and produces the protective salve");
        save_checkpoint("scot-connected");
    }

    void argon()
    {
        // Return through the same connected ladders and search the lower
        // storage shaft after Scot has identified the ECGs' weakness.
        walk(32, 3, 6, {"DOWN", "DOWN", "STAY"});
        walk(32, 10, 4, {"DOWN", "DOWN", "DOWN", "STAY"});
        check(campaign_.session.snapshot().script_id == 32,
              "Descending the actual ladder returns to the lower ship");
        walk(32, 10, 15, {"DOWN", "DOWN", "DOWN", "STAY"});
        walk(32, 7, 10, {"DOWN"});
        walk(32, 6, 11);
        check((campaign_.session.script_variable(opengold::por::EclAddress{0x4C4B}) & 0x80) != 0,
              "Searching the actual storage shaft finds the argon tanks");
        walk(32, 9, 11, {"UP"});
        walk(32, 5, 10, {"UP", "UP", "STAY"});
        walk(32, 3, 0);
        check((campaign_.session.script_variable(opengold::por::EclAddress{0x4C4C}) & 8) != 0,
              "The actual life-support controls receive the argon tanks");
        save_checkpoint("argon-connected");
    }

    void earn_ship()
    {
        auto &session = campaign_.session;
        walk(32, 0, 5, {"DOWN", "DOWN", "STAY"});
        walk(32, 5, 7, {"Gears", "STAND"});
        check((session.script_variable(opengold::por::EclAddress{0x4C52}) & 4) != 0,
              "The engineer really overrides airflow while the team stands its ground");
        // The cluebook's airshaft reaches the control room before the
        // self-destruct clock expires. Discover and enter it by Look,
        // then take its eight actual Up choices from deck one to nine.
        walk(32, 6, 5, {"YES", "UP", "UP", "UP", "UP", "UP", "UP", "UP", "UP", "STAY"});
        for (unsigned search = 0; !answers_.empty() && search < 8; ++search)
        {
            check(session.explore(opengold::por::ExplorationCommand::look),
                  "Look normally for the airshaft entrance");
            play();
        }
        check(answers_.empty() && session.snapshot().script_id == 33,
              "The actual airshaft carries the team back to deck nine");
        walk(32, 2, 8, {"UP"});
        walk(32, 9, 6,
             {"SHOOT IT", "PORT", "MED SUP", "LAUNCH", "YES", "Jock", "YES", "Runner", "YES",
              "Shade", "YES", "Gears", "YES", "Doc", "YES", "Merc", "NO", "EXIT"});
        check(session.snapshot().script_id == 34 && session.snapshot().solar_map,
              "Defeating the last ECG and reporting to NEO earns the real ship and launches it");
        check(!session.snapshot().game_won && !winning_party_,
              "Earning the ship is partial progress and never certifies the campaign ending");
        check(answers_.empty(), "The last ECG and port choices were actually played");
        for (const auto &member : campaign_.party->state().roster)
        {
            check(!member.vitals.dead && member.vitals.hit_points > 0,
                  "Every actual crew member survives and receives the normal ship clinic");
        }
        std::cout << "Solar handoff orbit cell4BA5="
                  << session.script_variable(opengold::por::EclAddress{0x4BA5})
                  << "; no test movement or orbit placement has been supplied.\n";
        save_checkpoint("ship-earned");
    }

    void walk(unsigned area, unsigned x, unsigned y, std::vector<std::string> answers = {},
              std::vector<unsigned> avoided_events = {})
    {
        check(answers_.empty(), "The previous route answers were consumed");
        for (auto &answer : answers)
        {
            answers_.push_back(std::move(answer));
        }
        auto &session = campaign_.session;
        check(session.snapshot().area_id == area, "A route enters each area through its scripts");
        const unsigned script = session.snapshot().script_id;
        constexpr unsigned width = 16;
        constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
        for (unsigned step = 0; step < 400; ++step)
        {
            const auto start = session.snapshot();
            if (start.area_id != area || start.script_id != script)
            {
                return;
            }
            if (start.pose.x == x && start.pose.y == y)
            {
                return;
            }
            const unsigned origin = start.pose.y * width + start.pose.x;
            const unsigned target = y * width + x;
            std::array<int, width * width> previous;
            previous.fill(-1);
            std::array<unsigned, width * width> direction{};
            std::deque<unsigned> queue{origin};
            previous[origin] = static_cast<int>(origin);
            while (!queue.empty())
            {
                const unsigned at = queue.front();
                queue.pop_front();
                for (unsigned d = 0; d < 4; ++d)
                {
                    const int nx = static_cast<int>(at % width) + dx[d];
                    const int ny = static_cast<int>(at / width) + dy[d];
                    if (nx < 0 || nx >= 16 || ny < 0 || ny >= 16)
                    {
                        continue;
                    }
                    if (std::ranges::find(avoided_events, session.map()
                                                              .at(static_cast<unsigned>(nx),
                                                                  static_cast<unsigned>(ny))
                                                              .event_number()) !=
                        avoided_events.end())
                    {
                        continue;
                    }
                    const auto &edge = session.map().at(at % width, at / width);
                    if (edge.doors[d] > 3 || (edge.walls[d] && !edge.doors[d]))
                    {
                        continue;
                    }
                    const unsigned next = static_cast<unsigned>(ny * 16 + nx);
                    if (previous[next] == -1)
                    {
                        previous[next] = static_cast<int>(at);
                        direction[next] = d;
                        queue.push_back(next);
                    }
                }
            }
            if (previous[target] < 0)
            {
                std::ostringstream message;
                message << "No physical path from " << start.pose.x << ',' << start.pose.y << " to "
                        << x << ',' << y << ". Reachable events:";
                for (unsigned at = 0; at < previous.size(); ++at)
                {
                    const auto event = session.map().at(at % width, at / width).event_number();
                    if (previous[at] >= 0 && event)
                    {
                        message << ' ' << at % width << ',' << at / width << '=' << event;
                    }
                }
                throw std::runtime_error(message.str());
            }
            unsigned next = target;
            while (previous[next] != static_cast<int>(origin))
            {
                next = static_cast<unsigned>(previous[next]);
            }
            for (unsigned turn = 0;
                 static_cast<unsigned>(session.snapshot().pose.facing) != direction[next] &&
                 turn < 4;
                 ++turn)
            {
                check(session.explore(opengold::por::ExplorationCommand::turn_right),
                      "Turn through the ordinary exploration command");
                play();
            }
            check(session.explore(opengold::por::ExplorationCommand::forward),
                  "Step through the ordinary exploration command");
            play();
            if (next == target)
            {
                return;
            }
        }
        throw std::runtime_error("The earned path did not reach its destination");
    }

    void save_checkpoint(std::string_view name)
    {
        auto &session = campaign_.session;
        check(session.can_leave(), "A checkpoint is an ordinary idle campaign save");
        const auto bytes =
            opengold::encode_campaign(*campaign_.party, &session, assets_, countdown);
        // Reload into independently loaded assets, not a copy whose current
        // snapshot could accidentally supply state missing from the save.
        auto fresh_session = opengold::load_campaign(folder_, countdown);
        auto loaded = opengold::decode_campaign(
            bytes, *campaign_.creation, campaign_.party->rule_module(), assets_, &fresh_session);
        auto party = std::make_shared<opengold::CampaignParty>(
            og::rules_module(opengold::load_original_content(folder_, countdown)),
            campaign_.creation);
        party->restore(std::move(loaded.party));
        check(loaded.town.has_value(), "The checkpoint includes the actual world");
        loaded.town->attach_restored_party(party);
        check(loaded.town->snapshot().solar_map == session.snapshot().solar_map,
              "A restored checkpoint keeps its actual solar-map handoff");
        check(opengold::encode_campaign(*party, &*loaded.town, assets_, countdown) == bytes,
              "The earned checkpoint reloads and saves byte for byte");
        if (const auto *prefix = std::getenv("OPENGOLD_COUNTDOWN_EARNED_SAVES"))
        {
            std::ofstream out(std::string(prefix) + std::string(name) + ".ogs", std::ios::binary);
            out << bytes;
            check(static_cast<bool>(out), "Write the ordinary earned checkpoint");
        }
        std::cout << "Earned Countdown " << name << ": " << fights_ << " actual ground victories; "
                  << "ordinary save reloads byte for byte.\n";
    }

    void inspect(unsigned script)
    {
        const auto program = opengold::test::TourAccess::program(campaign_.session, script);
        auto instructions = opengold::por::walk_reachable(*program).instructions;
        std::ranges::sort(instructions, {}, &opengold::por::EclInstruction::address);
        for (const auto &instruction : instructions)
        {
            std::cout << std::hex << instruction.address << ' '
                      << opengold::por::ecl_opcode(instruction.opcode, countdown).name;
            for (const auto &operand : instruction.operands)
            {
                if (operand.tag >= 128)
                {
                    std::cout << " text=" << operand.text;
                }
                else
                {
                    std::cout << " [" << unsigned(operand.tag) << ':' << operand.value << ']';
                }
            }
            std::cout << '\n';
        }
        std::cout << std::dec;
        if (script == 32 || script == 33)
        {
            const auto &raw = program->raw();
            for (unsigned address = 0xA3CD; address < 0xA400 && address - 0x8000 + 2 < raw.size();
                 ++address)
            {
                std::cout << "TABLE " << std::hex << address << ' '
                          << unsigned(raw[address - 0x8000 + 2]) << std::dec << '\n';
            }
        }
        for (const auto &[area, from] : opengold::test::TourAccess::area_scripts(campaign_.session))
        {
            if (from != script)
            {
                continue;
            }
            const auto map = opengold::test::TourAccess::district_map(campaign_.session, area);
            for (unsigned y = 0; y < 16; ++y)
            {
                for (unsigned x = 0; x < 16; ++x)
                {
                    std::cout << "MAP " << area << ' ' << x << ',' << y << " event "
                              << map.at(x, y).event_number() << '\n';
                }
            }
        }
    }

    void ending_protocol()
    {
        // This isolated protocol check starts at Wilma's installed closing
        // speech. It proves recognition timing, not the earned campaign.
        auto bytes = opengold::test::TourAccess::program(campaign_.session, 98)->raw();
        for (unsigned entry = 0; entry < 5; ++entry)
        {
            const unsigned at = 2 + entry * 4;
            bytes.at(at) = 1;
            bytes.at(at + 1) = 1;
            bytes.at(at + 2) = 0x0D;
            bytes.at(at + 3) = 0x9E;
        }
        auto resources =
            std::make_shared<opengold::por::PhlanResources>(*campaign_.session.resources());
        const auto program =
            std::make_shared<const opengold::por::EclProgram>(opengold::por::EclProgram::decode(
                bytes, "Countdown ending protocol fixture", countdown));
        resources->programs[98] = program;
        resources->opening_script = 98;
        opengold::por::RolfTourSession ending(opengold::por::GeoMap{}, program, {}, 0, {},
                                              resources);
        ending.campaign_party(campaign_.party);
        bool earlier_prompt = false;
        for (unsigned screen = 0; screen < 100; ++screen)
        {
            ending.advance(1);
            const auto snapshot = ending.snapshot();
            check(snapshot.phase != opengold::por::TourPhase::faulted,
                  "Run the installed closing speech: " + snapshot.diagnostic);
            if (snapshot.phase != opengold::por::TourPhase::awaiting_continue)
            {
                continue;
            }
            const bool closing =
                snapshot.dialogue.find("YOUR MISSION IS OVER.") != std::string::npos;
            check(
                snapshot.game_won == closing,
                "Victory waits for Wilma's authored final words, not the first success paragraph");
            if (closing)
            {
                check(earlier_prompt, "The ending check crossed an earlier non-winning prompt");
                const auto party_save =
                    opengold::encode_campaign(*campaign_.party, nullptr, assets_, countdown);
                auto saved =
                    opengold::decode_campaign(party_save, *campaign_.creation,
                                              campaign_.party->rule_module(), assets_, nullptr);
                auto party = std::make_shared<opengold::CampaignParty>(
                    og::rules_module(opengold::load_original_content(folder_, countdown)),
                    campaign_.creation);
                party->restore(std::move(saved.party));
                check(opengold::encode_campaign(*party, nullptr, assets_, countdown) == party_save,
                      "All victory-time character records survive an ordinary party save");
                check(ending.choose(snapshot.continue_ticket, 0),
                      "Continue the real ending prompt");
                check(!ending.snapshot().game_won,
                      "Continue clears the transient victory announcement");
                std::cout
                    << "Countdown installed ending protocol passed; this is not an earned win.\n";
                return;
            }
            earlier_prompt = true;
            check(ending.choose(snapshot.continue_ticket, 0), "Continue Wilma's closing speech");
        }
        throw std::runtime_error("The installed closing speech did not reach its winning prompt");
    }

  private:
    void ground_fight()
    {
        auto &session = campaign_.session;
        check(session.pending_encounter().has_value(), "The script supplies its actual enemies");
        opengold::CombatDemo combat(
            og::rules_module(opengold::load_original_content(folder_, countdown)));
        combat.campaign_party(campaign_.party);
        combat.encounter(*session.pending_encounter(),
                         static_cast<std::uint64_t>(opengold::roll_die(random_, 2147483647)));
        const auto &initial = combat.combat().snapshot();
        std::cout << "Ground fight " << fights_ + 1 << " in script " << session.snapshot().script_id
                  << ':';
        for (const auto &actor : initial.combatants)
        {
            std::cout << ' ' << actor.name << '=' << actor.hit_points;
        }
        std::cout << '\n';
        for (const auto &member : campaign_.party->state().roster)
        {
            const auto record = og::buck_rogers_record_of(campaign_.party->game_record(member.id));
            check(record.has_value(), "Each fighter keeps its actual native record");
            const auto weapon = og::readied_weapon(record->items, record->types);
            std::cout << "Gear " << record->creature.name << ": ";
            if (weapon)
            {
                const auto &item = record->items[*weapon];
                std::cout << "weapon type " << unsigned(item.type) << ", rounds "
                          << unsigned(item.quantity);
            }
            else
            {
                std::cout << "unarmed";
            }
            std::cout << '\n';
        }
        for (unsigned command = 0; command < 12000; ++command)
        {
            const auto state = combat.combat().snapshot();
            if (state.outcome != opengold::rules::Outcome::ongoing)
            {
                for (const auto &unit : state.combatants)
                {
                    if (unit.side != opengold::rules::Side::party || unit.id >= 1000)
                    {
                        continue;
                    }
                    const auto record = og::buck_rogers_record_of(unit.persistent.resources);
                    if (record)
                    {
                        const auto weapon = og::readied_weapon(record->items, record->types);
                        if (weapon)
                        {
                            std::cout << "After combat " << unit.name << ": rounds "
                                      << unsigned(record->items[*weapon].quantity) << '\n';
                        }
                    }
                }
                check(state.outcome == opengold::rules::Outcome::victory,
                      "The earned ground battle must actually be won; outcome " +
                          std::to_string(static_cast<int>(state.outcome)));
                check(session.resolve_combat(state), "The campaign commits the real combat result");
                ++fights_;
                return;
            }
            const auto actor = std::ranges::find(state.combatants, state.actor,
                                                 &opengold::rules::CombatantView::id);
            const bool pc = actor != state.combatants.end() &&
                            actor->side == opengold::rules::Side::party &&
                            !opengold::is_ally(state.actor);
            auto order = opengold::choose_demo_command(
                combat.combat(), opengold::Searching::everyone, opengold::default_difficulty);
            if (pc)
            {
                // Concentrate the team's actual shots on a wounded enemy.
                // Quick handles movement when no legal shot reaches a foe.
                const auto offered = combat.combat().legal_commands();
                int least_hit_points = 1000000;
                for (const auto &attack : offered)
                {
                    if (attack.verb != "ranged" && attack.verb != "melee")
                    {
                        continue;
                    }
                    const auto target = std::ranges::find(state.combatants, attack.target,
                                                          &opengold::rules::CombatantView::id);
                    if (target != state.combatants.end() &&
                        target->side != opengold::rules::Side::party && target->hit_points > 0 &&
                        target->hit_points < least_hit_points)
                    {
                        least_hit_points = target->hit_points;
                        order = attack;
                    }
                }
                // A distant low-chance shot needlessly exposes the team to
                // the robots' stronger weapons. Walk closer using the actual
                // offered moves before concentrating fire at normal range.
                const auto distance = [](auto a, auto b)
                {
                    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
                };
                auto nearest = state.combatants.end();
                int nearest_distance = 1000000;
                for (auto enemy = state.combatants.begin(); enemy != state.combatants.end();
                     ++enemy)
                {
                    const auto away = distance(actor->cell, enemy->cell);
                    if (enemy->side != opengold::rules::Side::party && enemy->hit_points > 0 &&
                        !enemy->fled && away < nearest_distance)
                    {
                        nearest = enemy;
                        nearest_distance = away;
                    }
                }
                if (nearest != state.combatants.end() && nearest_distance > 8)
                {
                    for (const auto &move : offered)
                    {
                        const auto away = distance(move.destination, nearest->cell);
                        if (move.verb == "move" && away < nearest_distance)
                        {
                            nearest_distance = away;
                            order = move;
                        }
                    }
                }
            }
            check(combat.submit(order), "The rules accept the actual combat action");
        }
        throw std::runtime_error("The actual ground fight exceeded its command bound");
    }

    std::filesystem::path folder_;
    Campaign campaign_;
    std::string assets_;
    std::deque<std::string> answers_;
    std::optional<std::string> winning_party_;
    opengold::RandomState random_{20261120};
    unsigned fights_{};
};
} // namespace

int main(int argc, char **argv)
{
    try
    {
        const auto folder = opengold::test::installed_game_folder(countdown);
        if (folder.empty() &&
            (argc == 1 || (argc == 2 && (std::string_view(argv[1]) == "--ending-protocol" ||
                                         std::string_view(argv[1]) == "--chicagorg" ||
                                         std::string_view(argv[1]) == "--derelict" ||
                                         std::string_view(argv[1]) == "--scot" ||
                                         std::string_view(argv[1]) == "--argon" ||
                                         std::string_view(argv[1]) == "--ship-earned" ||
                                         std::string_view(argv[1]) == "--camp-service"))))
        {
            std::cout
                << "Installed Countdown files are absent; optional installed-data check skipped.\n";
            return 77;
        }
        check(!folder.empty(), "Install Countdown's original files for the earned campaign proof");
        EarnedPlayer player(folder);
        if (argc == 3 && std::string_view(argv[1]) == "--inspect")
        {
            player.inspect(static_cast<unsigned>(std::stoul(argv[2])));
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--ending-protocol")
        {
            player.ending_protocol();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--camp-service")
        {
            player.opening();
            player.camp_service();
            return 0;
        }
        check(argc == 1 || (argc == 2 && (std::string_view(argv[1]) == "--chicagorg" ||
                                          std::string_view(argv[1]) == "--derelict" ||
                                          std::string_view(argv[1]) == "--scot" ||
                                          std::string_view(argv[1]) == "--argon" ||
                                          std::string_view(argv[1]) == "--ship-earned" ||
                                          std::string_view(argv[1]) == "--earned-ending")),
              "Unknown earned-route argument");
        player.opening();
        if (argc > 1 &&
            (std::string_view(argv[1]) == "--chicagorg" ||
             std::string_view(argv[1]) == "--derelict" || std::string_view(argv[1]) == "--scot" ||
             std::string_view(argv[1]) == "--argon" ||
             std::string_view(argv[1]) == "--ship-earned" ||
             std::string_view(argv[1]) == "--earned-ending"))
        {
            player.chicagorg();
        }
        if (argc > 1 &&
            (std::string_view(argv[1]) == "--derelict" || std::string_view(argv[1]) == "--scot" ||
             std::string_view(argv[1]) == "--argon" ||
             std::string_view(argv[1]) == "--ship-earned" ||
             std::string_view(argv[1]) == "--earned-ending"))
        {
            player.derelict();
        }
        if (argc > 1 &&
            (std::string_view(argv[1]) == "--scot" || std::string_view(argv[1]) == "--argon" ||
             std::string_view(argv[1]) == "--ship-earned" ||
             std::string_view(argv[1]) == "--earned-ending"))
        {
            player.scot();
        }
        if (argc > 1 && (std::string_view(argv[1]) == "--argon" ||
                         std::string_view(argv[1]) == "--ship-earned" ||
                         std::string_view(argv[1]) == "--earned-ending"))
        {
            player.argon();
        }
        if (argc > 1 && (std::string_view(argv[1]) == "--ship-earned" ||
                         std::string_view(argv[1]) == "--earned-ending"))
        {
            player.earn_ship();
        }
        if (argc > 1 && std::string_view(argv[1]) == "--earned-ending")
        {
            throw std::runtime_error(
                "The route has not reached the authored campaign ending; partial progress is not a win");
        }
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
