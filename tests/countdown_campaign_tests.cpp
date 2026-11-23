// Buck Rogers: Countdown to Doomsday, plan step B3.1 (docs/BUCK-ROGERS.md): the
// engine plays the game. A team made by Countdown's creation rules starts the
// campaign, which runs the game's own scripts through the script host, shows
// its 3D views and pictures, plays its ground fights through the combat
// session and saves and loads. With --walk, every map is visited and every
// script entry run (opengold_countdown_walk). The checks read the game's own
// files and pass with nothing to check when its folder is not found
// (installed_games.h).
#include "opengold/buck_rogers_battlefield.h"
#include "opengold/buck_rogers_characters.h"
#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/buck_rogers_solar_map.h"
#include "opengold/campaign_save.h"
#include "opengold/campaign_session.h"
#include "opengold/character.h"
#include "opengold/combat_demo.h"
#include "opengold/formats.h"
#include "opengold/music_render.h"
#include "opengold/original.h"
#include "opengold/original_content.h"
#include "installed_games.h"
#include "tour_access.h"
#include "test_support.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <memory>
#include <string>
#include <string_view>
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
    auto creature =
        og::new_buck_rogers_character(race, og::Gender::male, career, countdown, random);
    creature.name = std::move(name);
    auto spending = og::SkillSpending::at_creation(creature, countdown);
    for (const auto skill : og::career_skills(career))
    {
        while (spending.career_points() > 0 && spending.add(skill) == og::SkillRefusal::none)
        {
        }
    }
    for (const auto skill : {Skill::notice, Skill::first_aid, Skill::climb, Skill::acrobatics,
                             Skill::fast_talk, Skill::library_search, Skill::etiquette})
    {
        while (spending.general_points() > 0 && spending.add(skill) == og::SkillRefusal::none)
        {
        }
    }
    spending.apply_to(creature);
    return {std::move(creature), og::buck_rogers_starting_items(*templates), *types};
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

void the_team_is_its_records(const std::filesystem::path &folder)
{
    auto campaign = start_campaign(folder);
    const auto &party = *campaign.party;
    check(party.state().roster.size() == 6, "Six members join the team");
    for (const auto &member : party.state().roster)
    {
        const auto record = og::buck_rogers_record_of(party.game_record(member.id));
        check(record && record->creature.level == 2 && record->items.size() == 2,
              "A member keeps its level-2 record and its two free items");
        check(member.character.sheet().level == 2 &&
                  member.vitals.hit_points == record->creature.hit_points,
              "The sheet shows the record's level and Hit Points");
    }
    const auto participants = party.participants();
    check(participants.size() == 6 &&
              og::decode_ground_record(participants.front().character_profile).has_value(),
          "A member fights as its record");
}

void the_campaign_opens(const std::filesystem::path &folder)
{
    auto campaign = start_campaign(folder);
    auto &session = campaign.session;
    for (unsigned screen = 0; screen < 200; ++screen)
    {
        session.advance(1);
        const auto &snapshot = session.snapshot();
        check(snapshot.phase != opengold::por::TourPhase::faulted,
              "Countdown's opening runs: " + snapshot.diagnostic);
        if (snapshot.phase == opengold::por::TourPhase::completed)
        {
            break;
        }
        if (snapshot.phase == opengold::por::TourPhase::awaiting_continue)
        {
            check(session.choose(snapshot.continue_ticket, 0), "Continue the opening");
        }
    }
    const auto &snapshot = session.snapshot();
    check(snapshot.phase == opengold::por::TourPhase::completed && snapshot.area_id == 16 &&
              snapshot.pose.x == 5 && snapshot.pose.y == 6,
          "The opening's march leaves the team in Chicagorg's spaceport (script 16, map 16) at "
          "(5,6): script " +
              std::to_string(snapshot.script_id) + " map " + std::to_string(snapshot.area_id) +
              " at " + std::to_string(snapshot.pose.x) + "," + std::to_string(snapshot.pose.y));
    check(session.script_diagnostics().empty(), "The opening needs no unsupported script command");
    const auto view = session.observe_view();
    check(view.width > 0 && view.height > 0, "The 3D view draws Countdown's walls");
    const auto assets = opengold::campaign_asset_identity(folder);
    const auto saved = opengold::encode_campaign(*campaign.party, &session, assets, countdown);
    check(opengold::saved_campaign_rules(saved).game == countdown, "The save names Countdown");
    const auto restored = opengold::decode_campaign(
        saved, *campaign.creation, campaign.party->rule_module(), assets, &session);
    auto restored_party = std::make_shared<opengold::CampaignParty>(
        og::rules_module(opengold::load_original_content(folder, countdown)), campaign.creation);
    restored_party->restore(restored.party);
    check(restored.town && restored.town->snapshot().script_id == 16 &&
              opengold::encode_campaign(*restored_party, &*restored.town, assets, countdown) ==
                  saved,
          "The campaign's save loads and saves again byte for byte");
    for (const auto &member : restored_party->state().roster)
    {
        check(restored_party->game_record(member.id) == campaign.party->game_record(member.id),
              "Each member's record comes back whole");
    }
}

// Every arena the game can lay (combatst.pas LoadCombatTileSet): each map's
// every square under every area kind the scripts give, and the wildernesses'
// every ground on many dice, asks only for tiles its set's file holds, but
// LOTEK's indoor walls, whose last tile (53rd) the file lacks
// (docs/games/countdown.md, Bugs).
void arenas_use_the_tiles_the_files_hold(const std::filesystem::path &folder)
{
    auto campaign = start_campaign(folder);
    const auto &session = campaign.session;
    const auto tiles = [&](unsigned kind)
    {
        return static_cast<unsigned>(opengold::test::TourAccess::buck_rogers_tiles(
            session, opengold::por::buck_rogers_tile_set(kind).file));
    };
    // The kinds the scripts write to cell 4BE6h, indoors.
    constexpr std::array<unsigned, 12> kinds{0x84, 0x10, 0x40, 0x50, 0x88, 0x08,
                                             0x82, 0x22, 0x01, 0x11, 0x00, 0x70};
    std::set<unsigned> lacking;
    for (const auto &[map, script] : opengold::test::TourAccess::area_scripts(session))
    {
        const auto geo = opengold::test::TourAccess::district_map(session, map);
        for (const auto kind : kinds)
        {
            for (unsigned y = 0; y < 16; ++y)
            {
                for (unsigned x = 0; x < 16; ++x)
                {
                    if (opengold::por::buck_rogers_battlefield(geo, x, y, kind, script, 0, 1,
                                                               tiles(kind))
                            .missing_tiles > 0)
                    {
                        lacking.insert(kind);
                    }
                }
            }
        }
    }
    check(lacking == std::set<unsigned>{0x82, 0x88},
          "Indoor arenas ask for a tile their file lacks only in LOTEK's areas");
    const opengold::por::GeoMap open{};
    for (const auto kind : {opengold::por::mars_wilderness, opengold::por::venus_wilderness})
    {
        for (unsigned ground = 0; ground <= 11; ++ground)
        {
            for (std::uint32_t seed = 1; seed <= 40; ++seed)
            {
                const auto arena = opengold::por::buck_rogers_battlefield(
                    open, 8, 8, kind, 64, ground, seed, tiles(kind));
                check(arena.missing_tiles == 0,
                      "A wilderness arena asks only for tiles MARSCOM and VENUSCOM hold: kind " +
                          std::to_string(kind) + " ground " + std::to_string(ground));
            }
        }
    }
}

// Countdown's music: MUSIC.DAX record 2, its bank, played over the
// introduction from its first song, at the later games' tempo (the song's
// tempo over 84 a tick); no script plays music (music.pas, musdrv.pas).
void the_title_music_plays(const std::filesystem::path &folder)
{
    const auto piece = opengold::title_music_piece(countdown);
    check(piece == 2u, "Countdown's title plays MUSIC.DAX record 2");
    const auto bank = opengold::load_music_bank(folder, countdown, *piece);
    check(!bank.songs.empty() && bank.songs.front().tick_divisor == bank.songs.front().tempo / 84,
          "The bank's songs keep the later games' tempo");
    const auto samples = opengold::render_music(bank, 0, 22050);
    check(samples.size() > 22050 * 10 && std::ranges::any_of(samples,
                                                             [](std::int16_t sample)
                                                             {
                                                                 return sample != 0;
                                                             }),
          "The first song plays for more than ten seconds: " + std::to_string(samples.size()));
}

// Runs the session to its next menu or input, answering continue prompts.
void run_to_menu(opengold::por::RolfTourSession &session)
{
    for (int step = 0; step < 200; ++step)
    {
        const auto &s = session.snapshot();
        check(s.phase != opengold::por::TourPhase::faulted, "The outpost runs: " + s.diagnostic);
        if (s.phase == opengold::por::TourPhase::running)
        {
            session.advance(1);
            continue;
        }
        if (s.phase == opengold::por::TourPhase::awaiting_continue && s.choices.size() == 1 &&
            s.choices.front().starts_with("PRESS ENTER"))
        {
            check(session.choose(s.continue_ticket, 0), "Go on");
            continue;
        }
        return;
    }
}

// Takes the menu's choice holding `label`, then runs to the next menu.
void answer(opengold::por::RolfTourSession &session, std::string_view label)
{
    run_to_menu(session);
    const auto &s = session.snapshot();
    const auto found = std::ranges::find_if(s.choices,
                                            [&](const std::string &choice)
                                            {
                                                return choice.find(label) != std::string::npos;
                                            });
    std::string offered;
    for (const auto &choice : s.choices)
    {
        offered += choice + "|";
    }
    check(s.phase == opengold::por::TourPhase::awaiting_continue && found != s.choices.end(),
          "The outpost offers " + std::string(label) + ": " + s.dialogue + " [" + offered + "]");
    check(session.choose(s.continue_ticket, static_cast<std::size_t>(found - s.choices.begin())),
          "Choose " + std::string(label));
    run_to_menu(session);
}

// A new game, its opening played to Chicagorg's spaceport.
Campaign opened_game(const std::filesystem::path &folder)
{
    auto campaign = start_campaign(folder);
    for (int step = 0;
         step < 100 && campaign.session.snapshot().phase != opengold::por::TourPhase::completed;
         ++step)
    {
        run_to_menu(campaign.session);
        const auto &s = campaign.session.snapshot();
        if (s.phase == opengold::por::TourPhase::awaiting_continue)
        {
            check(campaign.session.choose(s.continue_ticket, 0), "Continue the opening");
        }
    }
    return campaign;
}

// The menu's choices, as one text to compare and print.
std::string choices_of(const opengold::por::TourSnapshot &s)
{
    std::string offered;
    for (const auto &choice : s.choices)
    {
        offered += choice + "|";
    }
    return offered;
}

// Plays on until the team can fly again or a body's menu waits: prompts go
// on, a ship met en route is fled from and a ship fight is handed off as
// the enemy gone (the ship-combat screen plays it).
void fly_on(opengold::por::RolfTourSession &session)
{
    using opengold::por::TourPhase;
    for (int stop = 0; stop < 40; ++stop)
    {
        run_to_menu(session);
        const auto &s = session.snapshot();
        if (s.phase == TourPhase::ship_combat)
        {
            check(session.finish_ship_combat(
                      s.continue_ticket, opengold::por::ShipCombatOutcome::team_lost_or_enemy_gone),
                  "The ship fight is handed off");
            continue;
        }
        if (s.phase == TourPhase::awaiting_continue && choices_of(s) == "HAIL|ATTACK|FLEE|")
        {
            check(session.choose(s.continue_ticket, 2), "Flee the ship met en route");
            continue;
        }
        return;
    }
    check(false, "The flight goes on");
}

// The step commands, north and on clockwise, with the rocket's heading for each.
constexpr std::array<std::pair<opengold::por::ExplorationCommand, og::RocketHeading>, 8> flights{{
    {opengold::por::ExplorationCommand::north, og::RocketHeading::north},
    {opengold::por::ExplorationCommand::northeast, og::RocketHeading::northeast},
    {opengold::por::ExplorationCommand::east, og::RocketHeading::east},
    {opengold::por::ExplorationCommand::southeast, og::RocketHeading::southeast},
    {opengold::por::ExplorationCommand::south, og::RocketHeading::south},
    {opengold::por::ExplorationCommand::southwest, og::RocketHeading::southwest},
    {opengold::por::ExplorationCommand::west, og::RocketHeading::west},
    {opengold::por::ExplorationCommand::northwest, og::RocketHeading::northwest},
}};

// Flies the rocket on the solar map, a cell a step, toward the cell where
// `body` stands now, as a player steers for it, until the body's menu
// offers its places; a body flown past on the way is left by its Exit.
// Returns the steps flown.
unsigned fly_to(opengold::por::RolfTourSession &session, std::size_t body)
{
    using opengold::por::TourPhase;
    for (unsigned steps = 0; steps < 80; ++steps)
    {
        const auto &s = session.snapshot();
        if (s.phase == TourPhase::awaiting_continue && session.orbited_body())
        {
            if (session.orbited_body() == body)
            {
                return steps;
            }
            check(!s.choices.empty() && s.choices.back() == "EXIT",
                  "A body flown past offers Exit: " + choices_of(s));
            check(session.choose(s.continue_ticket, s.choices.size() - 1), "Fly on past");
            fly_on(session);
            continue;
        }
        check(s.solar_map && s.phase == TourPhase::completed,
              "The rocket can fly: " + s.dialogue + " [" + choices_of(s) + "]");
        const auto state = session.solar_map_state();
        const auto target = og::place_bodies(state.phase)[body].cell;
        const auto distance = [&](og::OrbitCell cell)
        {
            const auto apart = [](unsigned a, unsigned b)
            {
                return a > b ? a - b : b - a;
            };
            return std::max(apart(cell.x, target.x), apart(cell.y, target.y));
        };
        auto best = flights.front();
        auto nearest = ~0U;
        for (const auto &flight : flights)
        {
            const auto reached = og::step_rocket(state, flight.second).rocket;
            if (reached != state.rocket && distance(reached) < nearest)
            {
                nearest = distance(reached);
                best = flight;
            }
        }
        check(session.explore(best.first), "The rocket flies a step");
        fly_on(session);
    }
    check(false, "The rocket reaches body " + std::to_string(body));
    return 0;
}

// Takes the team into the solar map's script (34), as the port's launch
// does, and leaves Earth's menu: the rocket is free to fly.
void launch(opengold::por::RolfTourSession &session)
{
    opengold::test::TourAccess::enter_script(session, 34, 0,
                                             {8, 8, opengold::por::MapDirection::north});
    fly_on(session);
    const auto &s = session.snapshot();
    check(s.solar_map && session.orbited_body() == 2 && choices_of(s) == "SALVATION|TYCHO|EXIT|",
          "The rocket starts in Earth's orbit, its menu offering Salvation and Tycho: " +
              s.dialogue + " " + choices_of(s));
    check(session.choose(s.continue_ticket, 2), "Exit Earth's orbit");
    fly_on(session);
}

// Each body's place that is an outpost of script 99, the outpost's number,
// and its picture, as script 34's arrival menus land there (ECL2:34 84AAh
// to 88B2h, 88BEh to 8BC8h): HIELO, NEW ELYSIUM, TYCHO, PAVONIS, then each
// asteroid's own.
struct Outpost
{
    std::size_t body{}, choice{};
    std::uint16_t picture{};
};

constexpr std::array<Outpost, og::solar_bodies> outposts{{{0, 0, 0x59},
                                                          {1, 0, 0x58},
                                                          {2, 1, 0x5A},
                                                          {3, 0, 0x57},
                                                          {4, 0, 0x4A},
                                                          {5, 0, 0x4A},
                                                          {6, 0, 0x4A},
                                                          {7, 0, 0x4A},
                                                          {8, 0, 0x4A},
                                                          {9, 0, 0x4A},
                                                          {10, 0, 0x4A},
                                                          {11, 0, 0x4A},
                                                          {12, 0, 0x4A}}};

// Flies from Earth to outpost `number` and takes its place in the body's
// menu: the team docks at the spaceport, script 99 with the outpost's number
// in cell 4C31h and its picture in 4C2Fh.
void dock_at_outpost(opengold::por::RolfTourSession &session, unsigned number)
{
    const auto &outpost = outposts.at(number);
    launch(session);
    (void)fly_to(session, outpost.body);
    const auto &s = session.snapshot();
    check(session.choose(s.continue_ticket, outpost.choice), "Take the outpost's place");
    run_to_menu(session);
    const auto machine = opengold::test::TourAccess::machine(session);
    check(session.snapshot().script_id == 99 &&
              machine.variable(opengold::por::EclAddress{0x4C31}) == number &&
              machine.variable(opengold::por::EclAddress{0x4C2F}) == outpost.picture,
          "The team docks at outpost " + std::to_string(number) + ": script " +
              std::to_string(session.snapshot().script_id));
}

// Docks at outpost `number` and launches again from its port, as a team that
// has been there: the rocket is back in its body's orbit, the menu left, and
// the outpost is the one the outposts' script runs next (cells 4C31h and
// 4C2Fh).
void dock_and_launch(opengold::por::RolfTourSession &session, unsigned number)
{
    dock_at_outpost(session, number);
    answer(session, "PORT");
    answer(session, "LAUNCH");
    fly_on(session);
    const auto &s = session.snapshot();
    check(s.solar_map && session.orbited_body() == outposts.at(number).body && !s.choices.empty(),
          "The launch is back in the body's orbit: " + choices_of(s));
    check(session.choose(s.continue_ticket, s.choices.size() - 1), "Exit the orbit");
    fly_on(session);
}

// The solar map (docs/games/countdown.md, "The solar map"): a new game's
// rocket starts in Earth's orbit at phase 0 and its menu is script 34's; a
// step east spends the script's fuel, takes three days and heals; the bodies'
// menus are the script's own, Ceres's lands at its outpost; and the map
// saves and loads with the game.
void the_solar_map_flies(const std::filesystem::path &folder)
{
    auto campaign = opened_game(folder);
    auto &session = campaign.session;
    auto &party = *campaign.party;
    launch(session);
    check(session.solar_map_state() == og::new_voyage(), "A new voyage starts at phase 0");
    const auto view = session.observe_view();
    check(view.width == 176 && view.height == 176, "The map draws 22 cells of 8 pixels a side");
    const auto member = party.state().slots[0];
    auto record = *og::buck_rogers_record_of(party.game_record(member));
    record.creature.hit_points = static_cast<std::uint16_t>(record.creature.hit_points - 5);
    party.keep_game_record(member, og::encode_ground_record(record));
    const auto wounded = record.creature.hit_points;
    using Ship = opengold::por::RolfTourSession;
    const auto ship = session.team_ship();
    const auto fuel = ship.system(Ship::ShipSystemKind::fuel);
    check(ship.name == "Maelstrom Rider" && fuel.whole == 450 && fuel.now > 0 && fuel.now <= 450 &&
              ship.system(Ship::ShipSystemKind::hull).whole == 600,
          "The Maelstrom Rider holds 450 fuel and a hull of 600: " + ship.name + " " +
              std::to_string(fuel.now));
    const auto time = party.state().time_minutes;
    check(session.explore(opengold::por::ExplorationCommand::east), "The rocket flies east");
    fly_on(session);
    const auto flown = session.solar_map_state();
    check(flown.rocket == og::OrbitCell{10, 8} && flown.heading == og::RocketHeading::east &&
              flown.days == 3 && flown.phase == 0,
          "A step east turns the rocket and moves it a cell");
    const auto spent = fuel.now - session.team_ship().system(Ship::ShipSystemKind::fuel).now;
    check(spent == 10 || spent == 20,
          "The script spends 10 fuel a step, 10 more when Astrogation fails: " +
              std::to_string(spent));
    check(party.state().time_minutes == time + 3 * 24 * 60, "A step takes three days");
    const auto healed = og::buck_rogers_record_of(party.game_record(member))->creature.hit_points;
    check(healed == wounded + 3, "Three days heal three Hit Points: " + std::to_string(wounded) +
                                     " to " + std::to_string(healed));
    // The days change only the Hit Points: the record keeps its items,
    // credits and experience (the clock once put back the record as made).
    auto rested = *og::buck_rogers_record_of(party.game_record(member));
    rested.creature.hit_points = record.creature.hit_points;
    check(og::encode_ground_record(rested) == og::encode_ground_record(record),
          "The days keep the rest of the record");
    check(!session.explore(opengold::por::ExplorationCommand::look), "The solar map has no Look");
    const auto assets = opengold::campaign_asset_identity(folder);
    const auto saved = opengold::encode_campaign(party, &session, assets, countdown);
    const auto restored =
        opengold::decode_campaign(saved, *campaign.creation, party.rule_module(), assets, &session);
    check(restored.town && restored.town->solar_map_state() == session.solar_map_state() &&
              restored.town->snapshot().solar_map,
          "The solar map saves and loads with the game");
    (void)fly_to(session, 4);
    const auto &ceres = session.snapshot();
    check(choices_of(ceres) == "CERES|ASTEROID BASE|EXIT|" &&
              ceres.dialogue.find("ORBITING") != std::string::npos,
          "At Ceres the script offers its places: " + ceres.dialogue + " [" + choices_of(ceres) +
              "]");
    check(session.choose(ceres.continue_ticket, 0), "Land at Ceres");
    run_to_menu(session);
    check(session.snapshot().script_id == 99 &&
              opengold::test::TourAccess::machine(session).variable(
                  opengold::por::EclAddress{0x4C31}) == 4,
          "Ceres's place is outpost 4 of script 99");
    check(!session.snapshot().solar_map, "The outpost's picture takes the solar map's place");
    // Salvation's script keeps the solar map's area kind but shows its own
    // picture (PICTURE MODE 3), as the original's last change of screen says.
    auto salvation = opened_game(folder);
    opengold::test::TourAccess::enter_script(salvation.session, 34, 0,
                                             {8, 8, opengold::por::MapDirection::north});
    fly_on(salvation.session);
    check(salvation.session.choose(salvation.session.snapshot().continue_ticket, 0),
          "Dock at Salvation");
    run_to_menu(salvation.session);
    check(salvation.session.snapshot().script_id == 17 && !salvation.session.snapshot().solar_map,
          "At Salvation the station's picture shows, not the solar map");
}

// The thirteen bodies, each flown to from Earth in a new game, and the
// places script 34 offers there (ECL2:34 8499h to 88B2h): the four planets'
// outposts, then each asteroid by its own name, which is how the screen's
// names of the bodies are known. Places a story opens later (Thule's prison,
// Juno's rendezvous) are not offered yet.
void each_body_offers_its_places(const std::filesystem::path &folder)
{
    constexpr std::array<std::string_view, og::solar_bodies> places{
        "HIELO|MARIPOSA THREE|EXIT|",
        "NEW ELYSIUM|VENUS LOWLANDS|EXIT|",
        "SALVATION|TYCHO|EXIT|",
        "PAVONIS|MARS HIGH DESERT|EXIT|",
        "CERES|ASTEROID BASE|EXIT|",
        "VESTA|EXIT|",
        "FORTUNA|EXIT|",
        "PALLAS|EXIT|",
        "PSYCHE|EXIT|",
        "JUNO|EXIT|",
        "HYGEIA|COMM CENTER|EXIT|",
        "AURORA|EXIT|",
        "THULE|EXIT|"};
    for (std::size_t body = 0; body < places.size(); ++body)
    {
        if (body == 2)
        {
            continue; // Earth's, where the rocket starts (launch).
        }
        auto campaign = opened_game(folder);
        launch(campaign.session);
        (void)fly_to(campaign.session, body);
        const auto &s = campaign.session.snapshot();
        check(choices_of(s) == places[body], "Body " + std::to_string(body) + " offers " +
                                                 std::string(places[body]) + ": " + choices_of(s));
    }
}

void write_solar_map_fixture(const std::filesystem::path &folder, std::string_view path)
{
    auto campaign = opened_game(folder);
    launch(campaign.session);
    check(campaign.session.can_leave(), "The team is free to fly");
    opengold::write_campaign_file(
        std::filesystem::path(path),
        opengold::encode_campaign(*campaign.party, &campaign.session,
                                  opengold::campaign_asset_identity(folder), countdown));
}

// The hospital of an outpost of script 99 as its script runs it
// (docs/games/countdown.md, "The outposts"): the wounded member is healed to
// full Hit Points for 160 times the multiplier of 16, from the bank's
// account when it holds the fee, else from the patient's credits, else not
// at all; the shipped script compared one byte of the fee and never refused
// (Bugs). The bank's account and the wound are the test's.
struct HospitalVisit
{
    bool healed{};
    unsigned bank{};
};

HospitalVisit visit_hospital(const std::filesystem::path &folder, std::uint16_t bank)
{
    auto campaign = opened_game(folder);
    auto &session = campaign.session;
    auto &party = *campaign.party;
    // The bank's account (4CE2h), four bytes.
    opengold::test::TourAccess::set_variable(session, opengold::por::EclAddress{0x4CE2}, bank);
    opengold::test::TourAccess::set_variable(session, opengold::por::EclAddress{0x4CE4}, 0);
    // The rocket flies to Mercury and docks at its Hielo station, outpost 0;
    // the flight's days have healed the team, and then one is hurt.
    dock_at_outpost(session, 0);
    const auto member = party.state().slots[0];
    auto record = *og::buck_rogers_record_of(party.game_record(member));
    record.creature.hit_points = static_cast<std::uint16_t>(record.creature.hit_points - 3);
    party.keep_game_record(member, og::encode_ground_record(record));
    answer(session, "HOSPITAL");
    const auto healed = [&]
    {
        const auto now = og::buck_rogers_record_of(party.game_record(member));
        return now->creature.hit_points == now->creature.hit_point_maximum;
    };
    // The hospital asks whom to heal; it may ask what to do first.
    for (int pick = 0; pick < 4 && !healed(); ++pick)
    {
        run_to_menu(session);
        const auto &s = session.snapshot();
        const auto name = std::ranges::find_if(s.choices,
                                               [](const std::string &choice)
                                               {
                                                   return choice == "Jock" ||
                                                          choice.find("HEAL") != std::string::npos;
                                               });
        if (name == s.choices.end())
        {
            break;
        }
        check(session.choose(s.continue_ticket, static_cast<std::size_t>(name - s.choices.begin())),
              "Choose the patient");
    }
    return {healed(), opengold::test::TourAccess::machine(session).variable(
                          opengold::por::EclAddress{0x4CE2})};
}

void the_hospital_heals_for_its_fee(const std::filesystem::path &folder)
{
    const auto paid = visit_hospital(folder, 10000);
    check(paid.healed && paid.bank == 10000 - 2560,
          "The hospital heals for 2,560 credits from the bank's account: " +
              std::to_string(paid.bank));
    // Neither the bank's 100 credits nor the patient's 1,000 pay the fee.
    const auto refused = visit_hospital(folder, 100);
    check(!refused.healed && refused.bank == 100,
          "The hospital refuses a patient no one can pay for: " + std::to_string(refused.bank));
}

// What the walk met: each failure with where it happened, each hand-off to a
// screen the engine does not have yet with where it was reached, and counts.
struct WalkLog
{
    std::map<std::string, std::vector<std::string>> needs, handoffs;
    std::map<std::string, unsigned> counts;
};

// A ground fight, every side played by the game's own computer: the members
// as Quick plays them (choose_quick_command), the enemies as the game's
// monsters choose (choose_demo_command).
bool fight(Campaign &campaign, const std::filesystem::path &folder, WalkLog &log,
           const std::string &where)
{
    // Which tile set the arena is drawn with, in zero gravity or not, and
    // whether it asks for a tile the set's file lacks (docs/games/countdown.md).
    {
        const auto &session = campaign.session;
        const auto machine = opengold::test::TourAccess::machine(session);
        const unsigned kind = machine.variable(opengold::por::EclAddress{0x4BE6}) & 0xFF;
        const unsigned ground = machine.variable(opengold::por::EclAddress{0x4BB7}) & 0xFF;
        const auto set = opengold::por::buck_rogers_tile_set(kind);
        const auto tiles = opengold::test::TourAccess::buck_rogers_tiles(session, set.file);
        const auto pose = session.snapshot().pose;
        const bool zero_g = (kind & 0x10) != 0;
        ++log.counts[std::string("fights on ") + std::string(set.file) +
                     (zero_g ? " in zero gravity" : "")];
        check(session.pending_encounter()->zero_gravity == zero_g,
              "A fight is in zero gravity where its area's kind says");
        for (std::uint32_t seed = 1; seed <= 20; ++seed)
        {
            const auto arena = opengold::por::buck_rogers_battlefield(
                session.map(), pose.x, pose.y, kind, session.snapshot().script_id, ground, seed,
                static_cast<unsigned>(tiles));
            if (arena.missing_tiles > 0)
            {
                ++log.counts[std::string("arenas asking for a tile ") + std::string(set.file) +
                             " lacks (kind " + std::to_string(kind) + ")"];
            }
        }
    }
    opengold::CombatDemo combat(
        og::rules_module(opengold::load_original_content(folder, countdown)));
    combat.campaign_party(campaign.party);
    combat.encounter(*campaign.session.pending_encounter(), 7);
    for (int command = 0;
         command < 4000 && combat.combat().snapshot().outcome == opengold::rules::Outcome::ongoing;
         ++command)
    {
        const auto &session = combat.combat();
        const auto state = session.snapshot();
        const auto actor =
            std::ranges::find(state.combatants, state.actor, &opengold::rules::CombatantView::id);
        const bool member = actor != state.combatants.end() &&
                            actor->side == opengold::rules::Side::party &&
                            !opengold::is_ally(state.actor);
        const auto chosen =
            member ? opengold::choose_quick_command(session, true, opengold::Searching::everyone)
                   : opengold::choose_demo_command(session, opengold::Searching::everyone,
                                                   opengold::default_difficulty);
        if (!combat.submit(chosen))
        {
            log.needs["A fight stopped at " + chosen.verb].push_back(where);
            return false;
        }
    }
    const auto result = combat.combat().snapshot();
    ++log.counts["fights ended " + std::to_string(static_cast<int>(result.outcome))];
    return campaign.session.resolve_combat(result);
}

// Plays what the session asks until the event ends: the first choice of a
// menu, then the last once it comes back twice; 1 or A typed; treasure left;
// shops left; fights played; a ship fight handed off.
void play(Campaign &campaign, const std::filesystem::path &folder, WalkLog &log,
          const std::string &where)
{
    auto &session = campaign.session;
    std::map<std::string, unsigned> shown;
    bool solar_map_noted = false;
    std::size_t diagnosed = session.script_diagnostics().size();
    const auto attribute = [&]
    {
        const auto &found = session.script_diagnostics();
        for (; diagnosed < found.size(); ++diagnosed)
        {
            log.needs[found[diagnosed]].push_back(where);
        }
    };
    for (int step = 0; step < 800; ++step)
    {
        const auto &s = session.snapshot();
        if (s.solar_map && !solar_map_noted)
        {
            solar_map_noted = true;
            ++log.counts["events reaching the solar map"];
        }
        using opengold::por::TourPhase;
        switch (s.phase)
        {
        case TourPhase::running:
            session.advance(1.0);
            continue;
        case TourPhase::awaiting_continue:
            // OPENGOLD_WALK_TRACE prints what each menu says, to see a loop by.
            if (std::getenv("OPENGOLD_WALK_TRACE"))
            {
                std::cout << where << ": " << s.dialogue.substr(0, 120) << " [";
                for (const auto &choice : s.choices)
                {
                    std::cout << choice << "|";
                }
                std::cout << "]\n";
            }
        {
            // A menu that comes back (an outpost's) is answered with each
            // choice in turn; once each has been taken twice the walk has
            // seen what it offers and goes on elsewhere.
            std::string menu;
            for (const auto &choice : s.choices)
            {
                menu += choice + "|";
            }
            const auto times = ++shown[menu];
            if (s.choices.empty())
            {
                log.needs["A menu without choices"].push_back(where);
                return;
            }
            if (s.choices.size() > 1 && times > 2 * s.choices.size() + 2)
            {
                ++log.counts["menus explored to the end"];
                attribute();
                return;
            }
            const auto pick = (times - 1) % s.choices.size();
            if (!session.choose(s.continue_ticket, pick))
            {
                log.needs["A choice was refused: " + s.choices[pick]].push_back(where);
                return;
            }
            continue;
        }
        case TourPhase::awaiting_input:
            if (!session.input(s.continue_ticket, s.number_input ? "1" : "A"))
            {
                log.needs["An answer was refused"].push_back(where);
                return;
            }
            continue;
        case TourPhase::treasure:
            (void)session.leave_treasure(s.continue_ticket);
            continue;
        case TourPhase::shopping:
            (void)session.leave_shop(s.continue_ticket);
            continue;
        case TourPhase::ship_combat:
            log.handoffs["ship combat with enemy type " + std::to_string(s.ship_combat->enemy_type)]
                .push_back(where);
            // The walk takes the enemy as gone: the ship-combat screen plays
            // the fight itself.
            (void)session.finish_ship_combat(
                s.continue_ticket, opengold::por::ShipCombatOutcome::team_lost_or_enemy_gone);
            continue;
        case TourPhase::combat:
            if (session.monster_picture())
            {
                (void)session.start_encounter();
                continue;
            }
            if (!session.pending_encounter() || !fight(campaign, folder, log, where))
            {
                log.needs["A fight ended without a result: " + s.diagnostic].push_back(where);
                attribute();
                return;
            }
            continue;
        case TourPhase::faulted:
            log.needs[s.diagnostic].push_back(where);
            attribute();
            return;
        default:
            ++log.counts["events ended in phase " + std::to_string(static_cast<int>(s.phase))];
            attribute();
            return;
        }
    }
    log.needs["An event ran too long"].push_back(where);
    attribute();
}

// A new game, its opening played.
Campaign opened(const std::filesystem::path &folder, WalkLog &log)
{
    auto campaign = start_campaign(folder);
    play(campaign, folder, log, "opening");
    // The enemy ship whose decks the boarding script (52) fights over, which
    // the ship fight names: the first type, the Hubble.
    opengold::test::TourAccess::set_variable(campaign.session, opengold::por::EclAddress{0x4C61},
                                             1);
    return campaign;
}

// Every map: entered as its script's entry has it, then every square looked
// at, its event played; an event that takes the team away is played from a
// new game. Then every script without a map of its own, entered. Prints what
// the scripts need that the engine lacks and where the solar map and ship
// fights hand off to their screens.
bool walk(const std::filesystem::path &folder)
{
    constexpr unsigned outposts_map = 99;
    WalkLog log;
    auto campaign = opened(folder, log);
    const auto maps = opengold::test::TourAccess::area_scripts(campaign.session);
    std::set<unsigned> mapped;
    for (const auto &[map, script] : maps)
    {
        mapped.insert(script);
        // OPENGOLD_WALK_MAP walks that map alone (a planning aid).
        if (const char *only = std::getenv("OPENGOLD_WALK_MAP");
            only && std::to_string(map) != only)
        {
            continue;
        }
        const auto enter = [&]
        {
            campaign = opened(folder, log);
            const auto where = "map " + std::to_string(map) + " entry";
            try
            {
                // Map 99, the Mariposa's top, takes the outposts' script
                // (99) of its number, which runs the outpost the team last
                // docked at (Mercury's Hielo here) and leaves the team at its
                // menus, not on the map; script 95 plays the map, entered
                // below.
                if (map == outposts_map)
                {
                    dock_and_launch(campaign.session, 0);
                }
                opengold::test::TourAccess::enter(campaign.session, map,
                                                  {8, 8, opengold::por::MapDirection::north});
                play(campaign, folder, log, where);
            }
            catch (const std::exception &error)
            {
                log.needs[error.what()].push_back(where);
                return false;
            }
            return campaign.session.snapshot().area_id == map && campaign.session.can_leave();
        };
        if (!enter())
        {
            log.handoffs["a map its entry does not stand the team on"].push_back(
                "map " + std::to_string(map));
            continue;
        }
        ++log.counts["maps walked"];
        for (unsigned y = 0; y < 16; ++y)
        {
            for (unsigned x = 0; x < 16; ++x)
            {
                const auto where = "map " + std::to_string(map) + " " + std::to_string(x) + "," +
                                   std::to_string(y);
                auto &session = campaign.session;
                if ((session.snapshot().area_id != map || !session.can_leave()) && !enter())
                {
                    ++log.counts["maps not reentered"];
                    x = y = 16;
                    break;
                }
                try
                {
                    opengold::test::TourAccess::place(campaign.session,
                                                      {x, y, opengold::por::MapDirection::north});
                    if (campaign.session.explore(opengold::por::ExplorationCommand::look))
                    {
                        ++log.counts["squares looked at"];
                        play(campaign, folder, log, where);
                    }
                }
                catch (const std::exception &error)
                {
                    log.needs[error.what()].push_back(where);
                }
            }
        }
    }
    // Each new game loads the campaign again, so the scripts are listed first.
    std::vector<unsigned> scripts;
    for (const auto &[script, program] : opengold::test::TourAccess::programs(campaign.session))
    {
        scripts.push_back(script);
    }
    for (const auto script : scripts)
    {
        if (mapped.contains(script) || script == 18)
        {
            continue;
        }
        // The outposts' script (99) runs the outpost the solar map's script
        // landed at: the walk flies the rocket from Earth to each body and
        // takes its outpost from the body's menu.
        constexpr unsigned outposts_script = 99;
        const unsigned visits =
            script == outposts_script ? static_cast<unsigned>(outposts.size()) : 1U;
        for (unsigned visit = 0; visit < visits; ++visit)
        {
            const auto where = "script " + std::to_string(script) +
                               (visits > 1 ? " outpost " + std::to_string(visit) : std::string{});
            campaign = opened(folder, log);
            try
            {
                if (script == outposts_script)
                {
                    const auto flown = campaign.party->state().time_minutes;
                    dock_at_outpost(campaign.session, visit);
                    log.counts["solar map steps flown"] +=
                        static_cast<unsigned>((campaign.party->state().time_minutes - flown) /
                                              (og::rocket_step_days * 24 * 60));
                    ++log.counts["outposts docked at from the solar map"];
                }
                else
                {
                    opengold::test::TourAccess::enter_script(
                        campaign.session, script, 0, {8, 8, opengold::por::MapDirection::north});
                }
                play(campaign, folder, log, where);
                ++log.counts["scripts without a map entered"];
                // On the wildernesses of Venus and Mars the team steps about
                // the big picture, as its random fights come on it.
                using Command = opengold::por::ExplorationCommand;
                constexpr std::array steps{Command::east,  Command::south,     Command::west,
                                           Command::north, Command::southeast, Command::northwest};
                for (unsigned step = 0; step < 60 && campaign.session.can_leave(); ++step)
                {
                    if (!campaign.session.explore(steps[(step / 5) % steps.size()]))
                    {
                        break;
                    }
                    ++log.counts["wilderness steps"];
                    play(campaign, folder, log, where + " step " + std::to_string(step));
                }
            }
            catch (const std::exception &error)
            {
                log.needs[error.what()].push_back(where);
            }
        }
    }
    for (const auto &[what, count] : log.counts)
    {
        std::cout << what << ": " << count << "\n";
    }
    for (const auto &[what, places] : log.handoffs)
    {
        std::cout << "Hand-off to " << what << ": " << places.size() << " times, first at "
                  << places.front() << "\n";
    }
    for (const auto &[what, places] : log.needs)
    {
        std::cout << "NEEDS " << what << " (" << places.size() << "): " << places.front();
        for (std::size_t n = 1; n < places.size() && n < 6; ++n)
        {
            std::cout << "; " << places[n];
        }
        std::cout << "\n";
    }
    return log.needs.empty();
}
} // namespace

int main(int argc, char **argv)
{
    const auto folder = opengold::test::installed_game_folder(countdown);
    if (folder.empty())
    {
        std::cout << "Countdown to Doomsday's files are not here.\n";
        return 0;
    }
    try
    {
        if (argc > 1 && std::string_view(argv[1]) == "--walk")
        {
            return walk(folder) ? 0 : 1;
        }
        // --solar-map-fixture=PATH writes the save the solar map's screen
        // check loads (solar_map_tests.gd): a new team on the solar map, out
        // of Earth's orbit, free to fly.
        if (constexpr std::string_view flag = "--solar-map-fixture=";
            argc > 1 && std::string_view(argv[1]).starts_with(flag))
        {
            write_solar_map_fixture(folder, std::string_view(argv[1]).substr(flag.size()));
            std::cout << "Countdown's solar map fixture written.\n";
            return 0;
        }
        the_team_is_its_records(folder);
        the_campaign_opens(folder);
        arenas_use_the_tiles_the_files_hold(folder);
        the_solar_map_flies(folder);
        each_body_offers_its_places(folder);
        the_hospital_heals_for_its_fee(folder);
        the_title_music_plays(folder);
        std::cout << "Countdown to Doomsday's campaign passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
