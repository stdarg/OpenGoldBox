// Buck Rogers: Countdown to Doomsday, plan step B3.1 (docs/BUCK-ROGERS.md): the
// engine plays the game. A team made by Countdown's creation rules starts the
// campaign, which runs the game's own scripts through the script host, shows
// its 3D views and pictures, plays its ground fights through the combat
// session and saves and loads. With --walk, every map is visited and every
// script entry run (opengold_countdown_walk). The checks read the game's own
// files and pass with nothing to check when its folder is not found
// (installed_games.h).
#include "opengold/buck_rogers_characters.h"
#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/campaign_save.h"
#include "opengold/campaign_session.h"
#include "opengold/character.h"
#include "opengold/combat_demo.h"
#include "opengold/formats.h"
#include "opengold/original.h"
#include "opengold/original_content.h"
#include "installed_games.h"
#include "tour_access.h"
#include "test_support.h"

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
            log.handoffs["the solar map"].push_back(where);
        }
        using opengold::por::TourPhase;
        switch (s.phase)
        {
        case TourPhase::running:
            session.advance(1.0);
            continue;
        case TourPhase::awaiting_continue:
        {
            const auto times = ++shown[s.dialogue];
            const auto pick = times > 2 && !s.choices.empty() ? s.choices.size() - 1 : 0;
            if (!session.choose(s.continue_ticket, pick))
            {
                log.needs["A choice was refused"].push_back(where);
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
    return campaign;
}

// Every map: entered as its script's entry has it, then every square looked
// at, its event played; an event that takes the team away is played from a
// new game. Then every script without a map of its own, entered. Prints what
// the scripts need that the engine lacks and where the solar map and ship
// fights hand off to their screens.
bool walk(const std::filesystem::path &folder)
{
    WalkLog log;
    auto campaign = opened(folder, log);
    const auto maps = opengold::test::TourAccess::area_scripts(campaign.session);
    std::set<unsigned> mapped;
    for (const auto &[map, script] : maps)
    {
        mapped.insert(script);
        const auto enter = [&]
        {
            campaign = opened(folder, log);
            const auto where = "map " + std::to_string(map) + " entry";
            try
            {
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
            ++log.counts["maps not entered"];
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
        const auto where = "script " + std::to_string(script);
        campaign = opened(folder, log);
        try
        {
            opengold::test::TourAccess::enter_script(campaign.session, script, 0,
                                                     {8, 8, opengold::por::MapDirection::north});
            play(campaign, folder, log, where);
            ++log.counts["scripts without a map entered"];
        }
        catch (const std::exception &error)
        {
            log.needs[error.what()].push_back(where);
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
        the_team_is_its_records(folder);
        the_campaign_opens(folder);
        std::cout << "Countdown to Doomsday's campaign passed.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
