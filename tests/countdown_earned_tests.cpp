// Countdown's earned campaign player. Unlike the diagnostic map walk, this
// starts once and uses ordinary movement, choices and actual rules combat.
#include "opengold/buck_rogers_characters.h"
#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/buck_rogers_ship_fight.h"
#include "opengold/buck_rogers_solar_map.h"
#include "opengold/buck_rogers_upkeep.h"
#include "opengold/campaign_save.h"
#include "opengold/campaign_session.h"
#include "opengold/combat_demo.h"
#include "opengold/formats.h"
#include "opengold/original.h"
#include "opengold/original_content.h"
#include "buck_rogers_training_player.h"
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
            {Skill::pilot_rocket, 30}, {Skill::maneuver_in_zero_g, 30}, {Skill::notice, 20}};
        break;
    case Profession::medic:
        career_choices = {{Skill::treat_light_wounds, 30},
                          {Skill::treat_serious_wounds, 30},
                          {Skill::diagnose, 20}};
        break;
    case Profession::engineer:
        career_choices = {{Skill::repair_electrical, 30},
                          {Skill::repair_mechanical, 10},
                          {Skill::jury_rig, 10},
                          {Skill::maneuver_in_zero_g, 30}};
        break;
    case Profession::rogue:
        career_choices = {
            {Skill::bypass_security, 30}, {Skill::open_lock, 25}, {Skill::hide_in_shadows, 25}};
        break;
    case Profession::warrior:
        career_choices = {
            {Skill::notice, 30}, {Skill::battle_tactics, 20}, {Skill::maneuver_in_zero_g, 30}};
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
    // The first assignment is in Zero-G. Medics and Rogues buy its offered
    // general training; the other careers spend their normal career points.
    if (career == Profession::medic || career == Profession::rogue)
    {
        spend(std::array<std::pair<Skill, unsigned>, 2>{
            {{Skill::maneuver_in_zero_g, 30}, {Skill::first_aid, 10}}});
    }
    else
    {
        spend(std::array<std::pair<Skill, unsigned>, 3>{
            {{Skill::first_aid, 20}, {Skill::climb, 10}, {Skill::fast_talk, 10}}});
    }
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
          assets_(opengold::campaign_asset_identity(folder_)),
          templates_(br::decode_item_templates(record_of(folder_ / "ITEM0.DAX", 1)).value())
    {
    }

    void novice()
    {
        campaign_.session.difficulty(opengold::DifficultySetting::novice);
        check(campaign_.session.script_variable(opengold::por::EclAddress{0x4BB6}) == 0,
              "The public Novice setting reaches the actual native script difficulty");
        std::cout << "Earned route uses the ordinary Novice difficulty control.\n";
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
                if (stop_solar_menus_ && snapshot.choices.size() > 1)
                {
                    return;
                }
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
                    check(snapshot.script_id == 98 &&
                              snapshot.dialogue.find("YOUR MISSION IS OVER.") != std::string::npos,
                          "Victory is the actual authored final speech");
                    const auto bytes =
                        opengold::encode_campaign(*campaign_.party, nullptr, assets_, countdown);
                    auto loaded =
                        opengold::decode_campaign(bytes, *campaign_.creation,
                                                  campaign_.party->rule_module(), assets_, nullptr);
                    check(
                        loaded.party.slots == campaign_.party->state().slots &&
                            loaded.party.roster.size() == campaign_.party->state().roster.size(),
                        "The ordinary at-win party snapshot preserves every occupied slot and roster member");
                    opengold::CampaignParty restored(
                        og::rules_module(opengold::load_original_content(folder_, countdown)),
                        campaign_.creation);
                    restored.restore(std::move(loaded.party));
                    for (const auto &member : campaign_.party->state().roster)
                    {
                        const auto &saved = restored.member(member.id);
                        check(
                            saved.character.sheet().name == member.character.sheet().name &&
                                saved.npc_source == member.npc_source &&
                                saved.vitals.hit_points == member.vitals.hit_points &&
                                saved.vitals.dead == member.vitals.dead &&
                                restored.game_record(member.id) ==
                                    campaign_.party->game_record(member.id),
                            "Every actual victory-time PC and companion keeps its identity, condition and complete native record");
                    }
                    for (const auto id : campaign_.party->state().slots)
                    {
                        if (!id)
                        {
                            continue;
                        }
                        const auto &member = campaign_.party->member(id);
                        const auto record =
                            og::buck_rogers_record_of(campaign_.party->game_record(id));
                        check(
                            record && !member.vitals.dead &&
                                og::status_of(record->creature) != og::BuckRogersStatus::dead &&
                                og::status_of(record->creature) != og::BuckRogersStatus::gone,
                            "Every actual occupied victory-time member is alive before the epilogue");
                    }
                    check(opengold::encode_campaign(restored, nullptr, assets_, countdown) == bytes,
                          "The full actual winning roster resaves byte for byte before Continue");
                    winning_party_ = bytes;
                    if (const auto *prefix = std::getenv("OPENGOLD_COUNTDOWN_EARNED_SAVES"))
                    {
                        std::ofstream out(std::string(prefix) + "winning-party.ogs",
                                          std::ios::binary);
                        out << *winning_party_;
                        check(static_cast<bool>(out),
                              "Write the ordinary actual victory-time party snapshot");
                    }
                    std::cout << "Actual authored Countdown victory: all winning slots and "
                              << restored.state().roster.size()
                              << " full roster records preserved.\n";
                }
                if (!answers_.empty() && answers_.front() == "train-ready" &&
                    snapshot.script_id == 17)
                {
                    auto offered = std::ranges::find_if(snapshot.choices,
                                                        [](const auto &choice)
                                                        {
                                                            return choice.starts_with("Train ");
                                                        });
                    if (offered == snapshot.choices.end())
                    {
                        // HQ may first report another authored mission.
                        // Re-enter its actual menu after that announcement.
                        offered = std::ranges::find(snapshot.choices, "HQ");
                        if (offered == snapshot.choices.end())
                        {
                            offered = std::ranges::find(snapshot.choices, "Exit");
                            if (offered != snapshot.choices.end())
                            {
                                answers_.pop_front();
                            }
                        }
                    }
                    if (offered != snapshot.choices.end())
                    {
                        check(session.choose(
                                  snapshot.continue_ticket,
                                  static_cast<std::size_t>(offered - snapshot.choices.begin())),
                              "Use the actual HQ training offer until no current member can train");
                        break;
                    }
                }
                if (!answers_.empty() && answers_.front() == "optional-clinic" &&
                    snapshot.script_id == 34 && snapshot.choices.size() > 1)
                {
                    if (std::ranges::find(snapshot.choices, "NO") != snapshot.choices.end())
                    {
                        std::string patient;
                        for (const auto id : campaign_.party->state().slots)
                        {
                            if (!id)
                            {
                                continue;
                            }
                            const auto &member = campaign_.party->member(id);
                            const auto record = og::buck_rogers_record_of(member.vitals.resources);
                            const auto &name = member.character.sheet().name;
                            if (record && !member.vitals.dead &&
                                og::status_of(record->creature) != og::BuckRogersStatus::dead &&
                                record->creature.hit_points < record->creature.hit_point_maximum &&
                                std::ranges::find(clinic_treated_, name) == clinic_treated_.end())
                            {
                                patient = name;
                                break;
                            }
                        }
                        if (patient.empty())
                        {
                            answers_.front() = "NO";
                        }
                        else
                        {
                            clinic_treated_.push_back(patient);
                            answers_.pop_front();
                            answers_.push_front("optional-clinic");
                            answers_.push_front(patient);
                            answers_.push_front("YES");
                        }
                    }
                    else
                    {
                        check(snapshot.script_id == 34 &&
                                  std::ranges::find(snapshot.choices, "EXIT") !=
                                      snapshot.choices.end(),
                              "The optional ship clinic either asks or reaches the arrival menu");
                        answers_.pop_front();
                    }
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
                    const auto type = before->items[*weapon].type;
                    check(type == 7 || type == 10 || type == 11,
                          "The route refills its actually carried bolt or rocket gun");
                    const std::string_view prefix = type == 11   ? "Rocket Rifle:"
                                                    : type == 10 ? "Rocket Pistol:"
                                                                 : "Bolt Gun:";
                    const auto found =
                        std::ranges::find_if(snapshot.choices.rbegin(), snapshot.choices.rend(),
                                             [&](const std::string &choice)
                                             {
                                                 return choice.starts_with(prefix);
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
                    if (snapshot.script_id == 80 || snapshot.script_id == 81 ||
                        snapshot.script_id == 98)
                    {
                        for (const auto &[type, label] :
                             {std::pair{22U, "Take Heavy Body Armor"},
                              std::pair{11U, "Take Rocket Rifle"},
                              std::pair{10U, "Take Rocket Pistol"},
                              std::pair{16U, "Take Grenade Lnchr."},
                              std::pair{37U, "Take Chaff Grenade"},
                              std::pair{13U, "Take Martian Laser Rifle +1"},
                              std::pair{23U, "Take Battle armor"}})
                        {
                            if (snapshot.script_id == 80 &&
                                (type == 37 || type == 13 || type == 23))
                            {
                                continue;
                            }
                            if (snapshot.script_id == 81 && type != 37 && type != 13)
                            {
                                continue;
                            }
                            if (snapshot.script_id == 98 && type != 37 && type != 13 && type != 23)
                            {
                                continue;
                            }
                            const auto loot = std::ranges::find(snapshot.choices, label);
                            if (loot == snapshot.choices.end())
                            {
                                continue;
                            }
                            for (unsigned slot = 0; slot < campaign_.party->state().slots.size();
                                 ++slot)
                            {
                                const auto id = campaign_.party->state().slots[slot];
                                if (!id || !campaign_.party->member(id).npc_source.empty())
                                {
                                    continue;
                                }
                                const auto record =
                                    og::buck_rogers_record_of(campaign_.party->game_record(id));
                                check(record.has_value(),
                                      "The actual recipient has its native inventory");
                                if (std::ranges::none_of(record->items,
                                                         [&](const auto &item)
                                                         {
                                                             return item.type == type;
                                                         }))
                                {
                                    check(session.can_select_member(),
                                          "The real booty permits choosing its recipient");
                                    campaign_.party->select(opengold::PartySlot{slot});
                                    chosen =
                                        static_cast<std::size_t>(loot - snapshot.choices.begin());
                                    break;
                                }
                            }
                            if (chosen)
                            {
                                break;
                            }
                        }
                    }
                    if (!chosen && !answers_.empty() && answers_.front().starts_with("Take "))
                    {
                        const auto found = std::ranges::find(snapshot.choices, answers_.front());
                        check(found != snapshot.choices.end(),
                              "The authored booty really offers " + answers_.front());
                        chosen = static_cast<std::size_t>(found - snapshot.choices.begin());
                        answers_.pop_front();
                    }
                    else if (!chosen)
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
                if (!chosen &&
                    ((snapshot.script_id == 49 &&
                      snapshot.dialogue.find("KILLER GENNIES APPROACH") != std::string::npos) ||
                     snapshot.script_id == 50 || snapshot.script_id == 64))
                {
                    const auto hide = std::ranges::find(snapshot.choices, "HIDE");
                    if (hide != snapshot.choices.end())
                    {
                        chosen = static_cast<std::size_t>(hide - snapshot.choices.begin());
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
                if (!chosen && snapshot.script_id == 96)
                {
                    for (const std::string_view label : {"INNOCENT", "TALK", "EVADE"})
                    {
                        const auto found = std::ranges::find(snapshot.choices, label);
                        if (found != snapshot.choices.end())
                        {
                            chosen = static_cast<std::size_t>(found - snapshot.choices.begin());
                            break;
                        }
                    }
                    if (snapshot.dialogue.find("THERE IS A CLOSED SHOP HERE") != std::string::npos)
                    {
                        const auto no = std::ranges::find(snapshot.choices, "NO");
                        check(no != snapshot.choices.end(),
                              "The actual parade permits passing the closed shop");
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
                ship_fight();
                break;
            case TourPhase::training:
                opengold::test::choose_buck_rogers_training(session, *campaign_.party, templates_);
                break;
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
        clinic_treated_.clear();
        walk(32, 9, 6, {"SHOOT IT", "PORT", "MED SUP", "LAUNCH", "optional-clinic", "EXIT"});
        check(session.snapshot().script_id == 34 && session.snapshot().solar_map,
              "Defeating the last ECG and reporting to NEO earns the real ship and launches it");
        check(!session.snapshot().game_won && !winning_party_,
              "Earning the ship is partial progress and never certifies the campaign ending");
        check(answers_.empty(), "The last ECG and port choices were actually played");
        for (const auto &member : campaign_.party->state().roster)
        {
            check(!member.vitals.dead && member.vitals.hit_points > 0,
                  "Every actual crew member survives the ordinary post-mission services");
        }
        std::cout << "Solar handoff orbit cell4BA5="
                  << session.script_variable(opengold::por::EclAddress{0x4BA5})
                  << "; no test movement or orbit placement has been supplied.\n";
        save_checkpoint("ship-earned");
    }

    void fly_to(std::size_t body)
    {
        using opengold::por::ExplorationCommand;
        using opengold::por::TourPhase;
        static constexpr std::array<std::pair<ExplorationCommand, og::RocketHeading>, 8> flights{{
            {ExplorationCommand::north, og::RocketHeading::north},
            {ExplorationCommand::northeast, og::RocketHeading::northeast},
            {ExplorationCommand::east, og::RocketHeading::east},
            {ExplorationCommand::southeast, og::RocketHeading::southeast},
            {ExplorationCommand::south, og::RocketHeading::south},
            {ExplorationCommand::southwest, og::RocketHeading::southwest},
            {ExplorationCommand::west, og::RocketHeading::west},
            {ExplorationCommand::northwest, og::RocketHeading::northwest},
        }};
        auto &session = campaign_.session;
        check(answers_.empty(), "Solar travel starts after the previous real choices");
        stop_solar_menus_ = true;
        for (unsigned step = 0; step < 80; ++step)
        {
            play();
            const auto snapshot = session.snapshot();
            if (snapshot.phase == TourPhase::awaiting_continue)
            {
                // Attempt the offered escape through the real script. If it
                // opens a ship fight, play() fails closed until that actual
                // transaction is available; no assumed outcome is supplied.
                auto choice = std::ranges::find(snapshot.choices, "FLEE");
                if (choice == snapshot.choices.end())
                {
                    if (session.orbited_body() == body)
                    {
                        std::cout << "Earned Solar arrival at body " << body << " after "
                                  << solar_steps_ << " actual flight steps.\n";
                        return;
                    }
                    choice = std::ranges::find(snapshot.choices, "EXIT");
                }
                check(choice != snapshot.choices.end(),
                      "The flight needs an authored answer: " + snapshot.dialogue);
                check(session.choose(snapshot.continue_ticket,
                                     static_cast<std::size_t>(choice - snapshot.choices.begin())),
                      "Take the actual flight or encountered body's menu choice");
                continue;
            }
            check(snapshot.phase == TourPhase::completed && snapshot.solar_map,
                  "The earned ship is idle on the real Solar map");
            save_checkpoint("solar-last-idle");
            const auto state = session.solar_map_state();
            const auto target = og::place_bodies(state.phase).at(body).cell;
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
                const auto reached = og::step_rocket(state, flight.second).rocket;
                if (reached != state.rocket && distance(reached) < nearest)
                {
                    nearest = distance(reached);
                    best = flight;
                }
            }
            const auto before = campaign_.party->state().time_minutes;
            check(session.explore(best.first), "Fly one ordinary direction toward the body");
            ++solar_steps_;
            play();
            if (session.solar_map_state().rocket != state.rocket)
            {
                check(campaign_.party->state().time_minutes == before + 3 * 24 * 60,
                      "An actual Solar flight step passes three native days");
            }
        }
        throw std::runtime_error("The earned flight did not reach its authored arrival menu");
    }

    void solar_arrival()
    {
        fly_to(4); // Ceres, in the native script's body order.
        const auto &snapshot = campaign_.session.snapshot();
        check(std::ranges::find(snapshot.choices, "ASTEROID BASE") != snapshot.choices.end(),
              "The actual Ceres arrival offers the campaign's Asteroid Base");
        stop_solar_menus_ = false;
        play({"EXIT"});
        check(campaign_.session.solar_map_state().rocket ==
                  og::place_bodies(campaign_.session.solar_map_state().phase).at(4).cell,
              "The earned ship physically reached Ceres");
        save_checkpoint("ceres-arrival");
    }

    void asteroid_entry()
    {
        fly_to(4);
        stop_solar_menus_ = false;
        play({"ASTEROID BASE"});
        check(campaign_.session.snapshot().script_id == 48 &&
                  campaign_.session.snapshot().area_id == 48,
              "Taking the actual Ceres menu docks at the Asteroid Base");
        check(answers_.empty(), "The real Asteroid Base menu choice ran");
        save_checkpoint("asteroid-entry");
    }

    void asteroid_lower()
    {
        walk(48, 11, 1, {"BLUFF"});
        check(answers_.empty(), "The official hears the team's actual bluff");
        save_checkpoint("asteroid-official");
        // ECL48's room tables connect these passages along the corridor
        // the administrator permits. The guarded offices are optional.
        using opengold::por::MapDirection;
        for (const auto &[x, y, direction] :
             std::array<std::tuple<unsigned, unsigned, MapDirection>, 12>{
                 {{10, 3, MapDirection::west},
                  {6, 15, MapDirection::west},
                  {13, 14, MapDirection::south},
                  {2, 5, MapDirection::south},
                  {12, 12, MapDirection::south},
                  {11, 9, MapDirection::south},
                  {14, 2, MapDirection::south},
                  {7, 5, MapDirection::south},
                  {14, 14, MapDirection::south},
                  {7, 11, MapDirection::south},
                  {7, 11, MapDirection::south},
                  {10, 12, MapDirection::east}}})
        {
            walk(48, x, y);
            if (x == 10 && y == 12)
            {
                passage(direction,
                        {"Take Rope", "Take Explosive Grenade", "Take Poison Antidote",
                         "Take Poison Antidote", "Take Demo Charge", "Take Demo Charge"});
            }
            else
            {
                passage(direction);
            }
        }
        walk(48, 10, 11, {"YES"});
        check(campaign_.session.snapshot().script_id == 49,
              "The ordinary elevator descends into the lower Asteroid Base");
        check(answers_.empty(), "The elevator's ordinary descent answer ran");
        save_checkpoint("asteroid-lower");
    }

    void asteroid_card()
    {
        using opengold::por::MapDirection;
        // Installed ECL49's room tables connect the entry to the reflector
        // lab. Each waypoint is reached by normal walking and Forward.
        for (const auto &[x, y, direction] :
             std::array<std::tuple<unsigned, unsigned, MapDirection>, 27>{
                 {{11, 10, MapDirection::east},  {9, 15, MapDirection::east},
                  {12, 9, MapDirection::east},   {9, 11, MapDirection::north},
                  {10, 0, MapDirection::north},  {7, 0, MapDirection::north},
                  {2, 0, MapDirection::north},   {13, 11, MapDirection::east},
                  {15, 11, MapDirection::east},  {15, 6, MapDirection::east},
                  {15, 6, MapDirection::east},   {15, 11, MapDirection::east},
                  {13, 11, MapDirection::east},  {15, 11, MapDirection::east},
                  {15, 6, MapDirection::east},   {5, 13, MapDirection::east},
                  {15, 2, MapDirection::south},  {14, 14, MapDirection::south},
                  {2, 5, MapDirection::south},   {7, 5, MapDirection::south},
                  {12, 12, MapDirection::south}, {9, 9, MapDirection::south},
                  {14, 2, MapDirection::south},  {10, 2, MapDirection::south},
                  {13, 15, MapDirection::west},  {0, 8, MapDirection::west},
                  {0, 13, MapDirection::west}}})
        {
            walk(49, x, y);
            passage(direction);
        }
        walk(49, 8, 14);
        while (campaign_.session.snapshot().pose.facing != MapDirection::west)
        {
            check(campaign_.session.explore(opengold::por::ExplorationCommand::turn_right),
                  "Face the reflector lab's actual search direction");
            play();
        }
        check(campaign_.session.explore(opengold::por::ExplorationCommand::look),
              "Search the actual reflector lab");
        play();
        check((campaign_.session.script_variable(opengold::por::EclAddress{0x4C71}) & 0x80) != 0,
              "The actual reflector lab grants Milo's keycard");
        save_checkpoint("asteroid-card");
    }

    void asteroid_clearance()
    {
        using opengold::por::MapDirection;
        for (const auto &[x, y, direction] :
             std::array<std::tuple<unsigned, unsigned, MapDirection>, 22>{
                 {{9, 14, MapDirection::east},   {5, 13, MapDirection::east},
                  {5, 8, MapDirection::east},    {15, 13, MapDirection::north},
                  {10, 0, MapDirection::north},  {14, 1, MapDirection::north},
                  {9, 6, MapDirection::north},   {12, 10, MapDirection::north},
                  {7, 0, MapDirection::north},   {2, 0, MapDirection::north},
                  {14, 13, MapDirection::north}, {13, 0, MapDirection::west},
                  {0, 13, MapDirection::west},   {10, 6, MapDirection::west},
                  {14, 11, MapDirection::west},  {11, 11, MapDirection::west},
                  {14, 11, MapDirection::west},  {10, 6, MapDirection::west},
                  {10, 6, MapDirection::west},   {14, 11, MapDirection::west},
                  {12, 10, MapDirection::north}, {8, 6, MapDirection::north}}})
        {
            walk(49, x, y);
            passage(direction);
        }
        // The cluebook's DNA is the installed notepad's passcode. The actual
        // computer accepts the acquired card and input, without Reprogram.
        walk(49, 6, 9, {"INSERT CARD", "DNA"});
        check(answers_.empty(), "The actual computer takes the card and passcode");
        check(campaign_.session.script_variable(opengold::por::EclAddress{0x4C80}) == 1,
              "Milo's actual computer message grants Mariposa clearance");
        save_checkpoint("asteroid-clearance");
    }

    void asteroid_return()
    {
        using opengold::por::MapDirection;
        for (const auto &[x, y, direction] :
             std::array<std::tuple<unsigned, unsigned, MapDirection>, 14>{
                 {{8, 11, MapDirection::south},
                  {8, 11, MapDirection::south},
                  {13, 11, MapDirection::east},
                  {15, 11, MapDirection::east},
                  {15, 6, MapDirection::east},
                  {15, 6, MapDirection::east},
                  {15, 11, MapDirection::east},
                  {12, 12, MapDirection::south},
                  {2, 5, MapDirection::south},
                  {7, 5, MapDirection::south},
                  {10, 2, MapDirection::south},
                  {10, 12, MapDirection::east},
                  {12, 9, MapDirection::east},
                  {9, 15, MapDirection::east}}})
        {
            walk(49, x, y);
            passage(direction);
        }
        // The acquired Rope is found by the script's FIND ITEM. Its YES
        // answer climbs the sole shaft still open after the demolition.
        walk(49, 10, 11, {"YES"});
        check(answers_.empty(), "The real Rope and climb answer were accepted");
        check(campaign_.session.snapshot().script_id == 48 &&
                  campaign_.session.snapshot().area_id == 48 &&
                  (campaign_.session.script_variable(opengold::por::EclAddress{0x4C75}) & 0x20) !=
                      0,
              "The native Rope exit returns the actual team to the upper base");
        save_checkpoint("asteroid-return");
    }

    void talon_boarded()
    {
        using opengold::por::MapDirection;
        for (const auto &[x, y, direction] :
             std::array<std::tuple<unsigned, unsigned, MapDirection>, 21>{
                 {{11, 10, MapDirection::east},  {9, 11, MapDirection::north},
                  {7, 6, MapDirection::north},   {7, 6, MapDirection::north},
                  {14, 13, MapDirection::north}, {7, 0, MapDirection::north},
                  {14, 1, MapDirection::north},  {11, 7, MapDirection::north},
                  {12, 10, MapDirection::north}, {2, 0, MapDirection::north},
                  {13, 0, MapDirection::west},   {6, 15, MapDirection::west},
                  {10, 3, MapDirection::west},   {6, 15, MapDirection::west},
                  {13, 8, MapDirection::west},   {10, 3, MapDirection::west},
                  {9, 10, MapDirection::west},   {9, 10, MapDirection::west},
                  {10, 3, MapDirection::west},   {13, 8, MapDirection::west},
                  {6, 15, MapDirection::west}}})
        {
            walk(48, x, y);
            passage(direction);
        }
        walk(48, 11, 0);
        // The native return heals through the offered autodoc and starts
        // Talon's ambush. His boarding party is a real ground encounter.
        passage(MapDirection::north, {"YES", "HAIL", "ATTACK BOARDING PARTY"});
        check(answers_.empty(), "The actual ship return and boarding choices ran");
        check(campaign_.session.snapshot().script_id == 50 &&
                  campaign_.session.snapshot().area_id == 50,
              "The real boarding victory enters Talon's actual ship");
        save_checkpoint("talon-boarded");
    }

    void talon_captured()
    {
        // The connecting hatch's installed trap captures the team. Declining
        // Talon's offered duel is an ordinary story choice, not a combat result.
        passage(opengold::por::MapDirection::east, {"FOLLOW", "DECLINE"});
        check(answers_.empty(), "The crew follows the guards and declines the actual duel");
        check(campaign_.session.snapshot().script_id == 50 &&
                  campaign_.session.snapshot().area_id == 51 &&
                  (campaign_.session.script_variable(opengold::por::EclAddress{0x4C79}) & 1) != 0,
              "The authored capture puts the actual crew in the ship's holding cell");
        save_checkpoint("talon-captured");
    }

    void buck_rescued()
    {
        auto &session = campaign_.session;
        const auto before = campaign_.party->state().roster.size();
        for (unsigned wait = 0;
             wait < 8 && (session.script_variable(opengold::por::EclAddress{0x4C79}) & 2) == 0;
             ++wait)
        {
            // The installed door advances the rescue after actual attempts.
            // A wounded Rogue may be unconscious: choose an actual able crew
            // member through WHO using the current native skill and status.
            std::string actor;
            int best_skill = -1;
            for (const auto &member : campaign_.party->state().roster)
            {
                const auto record = og::buck_rogers_record_of(member.vitals.resources);
                if (!member.npc_source.empty() || !record ||
                    og::status_of(record->creature) != og::BuckRogersStatus::okay ||
                    record->creature.enabled == 0 || record->creature.hit_points == 0)
                {
                    continue;
                }
                const int skill = og::skill_value(record->creature, og::Skill::open_lock);
                if (skill > best_skill)
                {
                    best_skill = skill;
                    actor = member.character.sheet().name;
                }
            }
            check(!actor.empty(), "An actual conscious crew member can attempt the lock");
            passage(opengold::por::MapDirection::north, {"PICKLOCK", actor});
            check(answers_.empty(), "The offered lockpick and conscious member choices ran");
        }
        check((session.script_variable(opengold::por::EclAddress{0x4C79}) & 2) != 0 &&
                  campaign_.party->state().roster.size() > before,
              "Buck's authored rescue actually recruits the companion");
        for (const auto &member : campaign_.party->state().roster)
        {
            if (member.npc_source.empty())
            {
                const auto record = og::buck_rogers_record_of(member.vitals.resources);
                check(record && og::readied_weapon(record->items, record->types),
                      "The real rescue restores each PC's captured equipment");
            }
        }
        save_checkpoint("buck-rescued");
    }

    void talon_clinic()
    {
        // The actual ladder table places deck seven beside the clinic.
        walk(51, 12, 12, {"UP", "UP", "STAY"});
        check(campaign_.session.snapshot().area_id == 50 &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4C12}) == 7,
              "The ordinary ladder reaches the ship's medical deck");
        walk(50, 5, 12);
        for (const auto &member : campaign_.party->state().roster)
        {
            const auto record = og::buck_rogers_record_of(member.vitals.resources);
            check(record && og::status_of(record->creature) == og::BuckRogersStatus::okay &&
                      record->creature.hit_points == record->creature.hit_point_maximum,
                  "The authored ship clinic restores every actual crew member");
        }
        save_checkpoint("talon-clinic");
    }

    void talon_plans()
    {
        // Buck knows the trapped cabin. Reach it on deck twelve through
        // the actual ladder, then read its installed computer scene.
        walk(50, 3, 12, {"UP", "UP", "UP", "UP", "UP", "STAY"});
        check(campaign_.session.snapshot().area_id == 51 &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4C12}) == 12,
              "The real ladder reaches Talon's cabin deck");
        walk(51, 1, 4);
        check((campaign_.session.script_variable(opengold::por::EclAddress{0x4C79}) & 0x40) != 0,
              "Buck and the actual cabin computer reveal the galley wiring");
        save_checkpoint("talon-plans");
    }

    void talon_galley()
    {
        walk(51, 3, 3, {"DOWN", "DOWN", "STAY"});
        check(campaign_.session.script_variable(opengold::por::EclAddress{0x4C12}) == 10,
              "The real ladder reaches the ship's galley deck");
        walk(51, 5, 12, {"YES"});
        check((campaign_.session.script_variable(opengold::por::EclAddress{0x4C7A}) & 4) != 0,
              "The crew wires its actual recovered demo charge through the galley prompt");
        walk(51, 3, 12,
             {"DOWN", "DOWN", "DOWN", "DOWN", "DOWN", "DOWN", "DOWN", "DOWN", "DOWN", "STAY"});
        check(campaign_.session.script_variable(opengold::por::EclAddress{0x4C12}) == 1,
              "The crew leaves the galley and descends normally to engineering");
        check((campaign_.session.script_variable(opengold::por::EclAddress{0x4C7A}) & 8) != 0,
              "The authored demolition actually disrupts the ship's security");
        save_checkpoint("talon-galley");
    }

    void talon_engineering()
    {
        walk(51, 12, 2);
        check((campaign_.session.script_variable(opengold::por::EclAddress{0x4C7B}) & 1) != 0,
              "The actual engineering guards are defeated through ground combat");
        walk(51, 10, 3);
        walk(51, 14, 3);
        walk(51, 12, 1);
        check(campaign_.session.script_variable(opengold::por::EclAddress{0x4C7E}) == 7 &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4C11}) == 1,
              "The crew physically destroys all three real power panels");
        save_checkpoint("talon-engineering");
    }

    void talon_escape()
    {
        walk(51, 12, 3, {"UP", "UP", "UP", "UP", "UP", "UP", "UP", "UP", "UP", "UP", "STAY"});
        check(campaign_.session.snapshot().area_id == 50 &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4C12}) == 11,
              "The real ladder returns to the boarding tube on deck eleven");
        walk(50, 8, 3);
        clinic_treated_.clear();
        passage(opengold::por::MapDirection::east,
                {"CLINIC", "HQ", "train-ready", "PORT", "MED SUP", "LAUNCH", "optional-clinic",
                 "EXIT"});
        check(answers_.empty() && campaign_.session.snapshot().script_id == 34 &&
                  campaign_.session.snapshot().solar_map &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4C81}) == 1,
              "The actual escape, Salvation report and launch complete the pirate chapter");
        save_checkpoint("talon-escape");
    }

    void ship_fight()
    {
        auto &session = campaign_.session;
        auto &fight = opengold::por::ship_fight(session, {});
        for (unsigned step = 0; step < 5000 && !fight.over(); ++step)
        {
            if (fight.awaits_dodge())
            {
                fight.answer_dodge(true);
            }
            else if (fight.awaits_round())
            {
                fight.play_round();
            }
            else
            {
                using Action = og::ShipAction;
                const auto orders = fight.legal_orders();
                std::optional<og::ShipOrder> chosen;
                for (const og::ShipOrder candidate :
                     {og::ShipOrder{.action = Action::command},
                      og::ShipOrder{.action = Action::close},
                      og::ShipOrder{.action = Action::target, .system = og::ShipSystem::hull},
                      og::ShipOrder{.action = Action::fire}, og::ShipOrder{.action = Action::aid}})
                {
                    if (std::ranges::find(orders, candidate) != orders.end())
                    {
                        chosen = candidate;
                        break;
                    }
                }
                if (!chosen)
                {
                    const auto found = std::ranges::find_if(
                        orders,
                        [](const auto &order)
                        {
                            return order.action == Action::load || order.action == Action::jury_rig;
                        });
                    if (found != orders.end())
                    {
                        chosen = *found;
                    }
                }
                if (!chosen)
                {
                    // QUIT ends this member's turn in the native ship menu.
                    // The pilot at close range may have no shot or boarding
                    // opportunity while the gunners still have to act.
                    const auto done =
                        std::ranges::find(orders, og::ShipOrder{.action = Action::quit});
                    check(done != orders.end(), "The real ship menu offers its end-turn order");
                    chosen = *done;
                }
                fight.submit(*chosen);
            }
        }
        check(fight.over(), "The actual ship combat finishes within its ordinary command bound");
        const auto end = fight.end();
        std::cout << "Actual ship combat outcome " << end.outcome << "; boarded " << end.boarded
                  << "; experience " << end.experience << '\n';
        check(end.outcome == 0, "The crew wins the actual ship fight through its rules");
        check(session.finish_ship_combat(session.snapshot().continue_ticket),
              "Commit the finished real ship combat and resume its authored script");
        ++ship_fights_;
    }

    void mars_entry()
    {
        fly_to(3); // Mars, in the installed script's body order.
        stop_solar_menus_ = false;
        // Decline the actual return-to-ship prompt at the landing site.
        play({"MARS HIGH DESERT", "ATTACK", "NO"});
        check(answers_.empty() && campaign_.session.snapshot().script_id == 64,
              "The actual Mars descent and scout cruiser fight reach the high desert");
        save_checkpoint("mars-entry");
    }

    void mars_village()
    {
        auto &session = campaign_.session;
        const auto wounded = [&]
        {
            return std::ranges::any_of(
                campaign_.party->state().slots,
                [&](const auto id)
                {
                    if (!id)
                    {
                        return false;
                    }
                    const auto record =
                        og::buck_rogers_record_of(campaign_.party->member(id).vitals.resources);
                    check(record && og::status_of(record->creature) != og::BuckRogersStatus::dead,
                          "Every actual current crew member survives the Mars arrival");
                    return record->creature.hit_points < record->creature.hit_point_maximum;
                });
        };
        // Recover from the ship battle through the ordinary permitted camp,
        // before setting out and receiving the intercepted attack warning.
        for (unsigned day = 0; day < 32 && wounded(); ++day)
        {
            const auto before = campaign_.party->state().time_minutes;
            check(session.camp(opengold::RestKind::long_rest),
                  "The authored Mars wilderness permits an ordinary recovery camp");
            // The current landing-site search offers to board again after
            // camp. Decline blastoff through that real prompt each time.
            play({"NO"});
            check(answers_.empty() && session.snapshot().script_id == 64,
                  "The recovering crew stays at the actual Mars landing site");
            check(campaign_.party->state().time_minutes == before + 24 * 60,
                  "Each actual native recovery camp passes a whole campaign day");
        }
        check(!wounded(), "The crew recovers through actual native camp rules");
        wilderness_walk(31, 11, {"AWAY", "WARNING", "GLIDERS", "YES"});
        check(answers_.empty() && session.snapshot().script_id == 65 &&
                  session.script_variable(opengold::por::EclAddress{0x4CA3}) == 1,
              "The crew reaches the village and honestly agrees to protect its cubs");
        save_checkpoint("mars-village");
    }

    void mars_defense()
    {
        auto &session = campaign_.session;
        // Join the offered defenders at the real main gate, then patrol
        // the adjacent village streets during the authored evacuation.
        // The source changes that gate scene after the attack begins.
        walk(65, 5, 7);
        for (unsigned watch = 0;
             watch < 128 && session.script_variable(opengold::por::EclAddress{0x4C0B}) != 1;
             ++watch)
        {
            walk(65, 7, 7);
            if (session.script_variable(opengold::por::EclAddress{0x4C0B}) != 1)
            {
                walk(65, 7, 8);
            }
        }
        check(session.script_variable(opengold::por::EclAddress{0x4C0B}) == 1,
              "The authored howling signal confirms that the cubs escaped safely");
        save_checkpoint("mars-defense");
    }

    void wilderness_walk(unsigned target_x, unsigned target_y,
                         std::vector<std::string> answers = {})
    {
        auto &session = campaign_.session;
        check(answers_.empty(), "The previous ordinary route answers were consumed");
        for (auto &answer : answers)
        {
            answers_.push_back(std::move(answer));
        }
        static constexpr std::array commands{
            opengold::por::ExplorationCommand::north, opengold::por::ExplorationCommand::northeast,
            opengold::por::ExplorationCommand::east,  opengold::por::ExplorationCommand::southeast,
            opengold::por::ExplorationCommand::south, opengold::por::ExplorationCommand::southwest,
            opengold::por::ExplorationCommand::west,  opengold::por::ExplorationCommand::northwest};
        const auto wilderness = session.snapshot().script_id;
        check(wilderness == 64 || wilderness == 80,
              "Walk only an actual Countdown wilderness controller");
        for (unsigned step = 0; step < 128 && session.snapshot().script_id == wilderness; ++step)
        {
            const int x = session.script_variable(opengold::por::EclAddress{0x4BC3});
            const int y = session.script_variable(opengold::por::EclAddress{0x4BC4});
            if (x == static_cast<int>(target_x) && y == static_cast<int>(target_y))
            {
                break;
            }
            const auto dx = (static_cast<int>(target_x) > x) - (static_cast<int>(target_x) < x);
            const auto dy = (static_cast<int>(target_y) > y) - (static_cast<int>(target_y) < y);
            const unsigned direction = dy < 0   ? (dx < 0   ? 7
                                                   : dx > 0 ? 1
                                                            : 0)
                                       : dy > 0 ? (dx < 0   ? 5
                                                   : dx > 0 ? 3
                                                            : 4)
                                       : dx < 0 ? 6
                                                : 2;
            check(session.explore(commands[direction]),
                  "Walk one ordinary wilderness direction toward the authored destination");
            play();
        }
    }

    void mars_camp()
    {
        // The source's storeroom trapdoor reaches the real refugee camp
        // after the cubs-safe howl. EARTH answers its actual text prompt.
        // ECL65's event20 is the storeroom; enter its actual doorway
        // to find the root-cellar passage after the evacuation signal.
        walk(65, 7, 11, {"EARTH"});
        check(answers_.empty() && campaign_.session.snapshot().script_id == 64 &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4C8A}) == 1 &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4C94}) == 1,
              "The real refugee camp gives a green card and the blue-rock rendezvous");
        save_checkpoint("mars-camp");
    }

    void mars_base()
    {
        auto &session = campaign_.session;
        const auto wounded = [&]
        {
            return std::ranges::any_of(
                campaign_.party->state().slots,
                [&](const auto id)
                {
                    if (!id)
                    {
                        return false;
                    }
                    const auto record =
                        og::buck_rogers_record_of(campaign_.party->member(id).vitals.resources);
                    check(record && og::status_of(record->creature) != og::BuckRogersStatus::dead,
                          "The actual crew survives the village evacuation");
                    return record->creature.hit_points < record->creature.hit_point_maximum;
                });
        };
        for (unsigned day = 0; day < 64 && wounded(); ++day)
        {
            const auto before = campaign_.party->state().time_minutes;
            check(session.camp(opengold::RestKind::long_rest),
                  "The refugee-camp wilderness permits actual native recovery days");
            play();
            check(campaign_.party->state().time_minutes == before + 24 * 60,
                  "The real recovery day advances the campaign clock");
        }
        check(!wounded(), "Native rest actually restores the wounded crew before the base trip");
        save_checkpoint("mars-recovered");
        std::string actor;
        for (const auto id : campaign_.party->state().slots)
        {
            if (!id)
            {
                continue;
            }
            const auto &member = campaign_.party->member(id);
            const auto record = og::buck_rogers_record_of(member.vitals.resources);
            if (record && og::status_of(record->creature) == og::BuckRogersStatus::okay &&
                record->creature.enabled && record->creature.hit_points > 0)
            {
                actor = member.character.sheet().name;
                break;
            }
        }
        check(!actor.empty(), "An actual conscious crew member can attempt the disguise");
        wilderness_walk(26, 2, {actor});
        check(
            answers_.empty() && session.snapshot().script_id == 66 &&
                session.script_variable(opengold::por::EclAddress{0x4C9F}) == 2 &&
                session.script_variable(opengold::por::EclAddress{0x4C90}) == 1,
            "The actual blue-rock rendezvous disguises the team and recruits Tuskon for the base");
        save_checkpoint("mars-base");
    }

    void mars_blue_card()
    {
        auto &session = campaign_.session;
        walk(66, 12, 3);
        // The green squad's original challenge expects eight members.
        // Answer its actual losses prompt using the current native count.
        const auto present = session.script_variable(opengold::por::EclAddress{0x7F3E});
        check(present < 8, "This actual squad has a loss count for the green-card cover story");
        passage(opengold::por::MapDirection::east,
                {"USE", "PASSCARD", "GREEN", "YES", std::to_string(8 - present), "YES"});
        check(answers_.empty(), "The real checkpoint accepts the card and spoken squad answers");
        // Follow the actual corridor's second green gate, avoiding the
        // optional technicians before taking the level-zero elevator.
        walk(66, 10, 8, {}, {22});
        passage(opengold::por::MapDirection::east, {"USE", "PASSCARD", "GREEN"});
        walk(66, 12, 13, {"2"}, {22});
        check(session.script_variable(opengold::por::EclAddress{0x4C11}) == 2,
              "The actual elevator reaches level two");
        // The restricted passage first asks for the spies report, then
        // accepts the crew's gear in its actual cachebox on the next step.
        // The vault's own cachebox restores those same items afterward.
        walk(66, 4, 2, {"YES", "0", "NO", "NO", "YES", "PASS", "USE", "PASSCARD", "GREEN"});
        check(answers_.empty() && session.script_variable(opengold::por::EclAddress{0x4C5F}) == 1,
              "The crew physically opens the vault and finds its real blue passcard");
        check(session.script_variable(opengold::por::EclAddress{0x4C0B}) == 0,
              "The actual vault cachebox restores the crew's deposited gear");
        for (const auto id : campaign_.party->state().slots)
        {
            if (!id || !campaign_.party->member(id).npc_source.empty())
            {
                continue;
            }
            const auto record =
                og::buck_rogers_record_of(campaign_.party->member(id).vitals.resources);
            check(record && og::readied_weapon(record->items, record->types),
                  "Each actual PC recovers the readied weapon from the real cachebox");
        }
        save_checkpoint("mars-blue-card");
    }

    void mars_prototype()
    {
        auto &session = campaign_.session;
        // The blue card authorizes the hoist branch from level two.
        // Descending through its actual shaft menu boards the rising hoist;
        // the source itself carries the team to the prototype platform.
        walk(66, 4, 3, {"USE", "PASSCARD", "BLUE", "DESCEND"}, {22});
        check(answers_.empty() && session.script_variable(opengold::por::EclAddress{0x4C11}) == 0 &&
                  session.script_variable(opengold::por::EclAddress{0x4C01}) == 4,
              "The real hoist and prototype scene overload the laser and return to the ground");
        save_checkpoint("mars-prototype");
    }

    void mars_departure()
    {
        auto &session = campaign_.session;
        check(session.script_variable(opengold::por::EclAddress{0x4C11}) == 0,
              "The authored falling hoist returns to the base's departure level");
        // Leave through the ground-floor blue gate after the falling hoist.
        walk(66, 14, 8);
        passage(opengold::por::MapDirection::east, {"USE", "PASSCARD", "BLUE"});
        // The installed exit accepts the earned blue passcard, marks the
        // Mars chapter complete and narrates the base's destruction outside.
        walk(66, 12, 5);
        check(session.snapshot().script_id == 64 &&
                  session.script_variable(opengold::por::EclAddress{0x4C5F}) == 1 &&
                  session.script_variable(opengold::por::EclAddress{0x4C87}) == 2,
              "The authored departure completes the Mars mission with its real passcard");
        check(std::ranges::count_if(campaign_.party->state().slots,
                                    [](const auto id)
                                    {
                                        return id != 0;
                                    }) == 6,
              "Tuskon's actual farewell leaves the six PCs in their current party slots");
        save_checkpoint("mars-departure");
    }

    void mars_return()
    {
        auto &session = campaign_.session;
        clinic_treated_.clear();
        wilderness_walk(7, 13, {"optional-clinic", "EXIT"});
        check(answers_.empty() && session.snapshot().script_id == 34 &&
                  session.snapshot().solar_map,
              "The crew walks back to its real ship and departs Mars");
        salvation_refit();
        save_checkpoint("mars-return");
    }

    void salvation_refit()
    {
        auto &session = campaign_.session;
        fly_to(2);
        stop_solar_menus_ = false;
        clinic_treated_.clear();
        play({"SALVATION",
              "CLINIC",
              "HQ",
              "train-ready",
              "DEPOT",
              "select:0",
              "Ammo",
              "fill-ammo",
              "Back",
              "select:1",
              "Ammo",
              "fill-ammo",
              "Back",
              "select:2",
              "Ammo",
              "fill-ammo",
              "Back",
              "select:3",
              "Ammo",
              "fill-ammo",
              "Back",
              "select:4",
              "Ammo",
              "fill-ammo",
              "Back",
              "select:5",
              "Ammo",
              "fill-ammo",
              "Back",
              "Exit",
              "PORT",
              "REPAIR",
              "FUEL",
              "AMMO",
              "MED SUP",
              "LAUNCH",
              "optional-clinic",
              "EXIT"});
        check(answers_.empty() && session.snapshot().script_id == 34 &&
                  session.snapshot().solar_map,
              "The actual Salvation services prepare the crew for its next mission");
    }

    void venus_entry()
    {
        fly_to(1);
        stop_solar_menus_ = false;
        play({"VENUS LOWLANDS", "NO"});
        check(answers_.empty() && campaign_.session.snapshot().script_id == 80,
              "The real flight lands at the authored Venusian lowlands");
        save_checkpoint("venus-entry");
    }

    void venus_ambush()
    {
        wilderness_walk(28, 12);
        check(answers_.empty() && campaign_.session.snapshot().script_id == 80 &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4CBF}) == 1,
              "The real Lowlander ambush is resolved and Leander's offer is answered");
        save_checkpoint("venus-ambush");
    }

    void venus_village()
    {
        // Go around the acid lake south of its shore before turning north.
        wilderness_walk(20, 12);
        wilderness_walk(5, 2);
        check(answers_.empty() && campaign_.session.snapshot().script_id == 81,
              "The crew walks through the actual Venusian wilderness to the Lowlander village");
        save_checkpoint("venus-village");
    }

    void venus_child()
    {
        auto &session = campaign_.session;
        const auto wounded = [&]
        {
            return std::ranges::any_of(
                campaign_.party->state().slots,
                [&](const auto id)
                {
                    if (!id)
                    {
                        return false;
                    }
                    const auto record = og::buck_rogers_record_of(campaign_.party->game_record(id));
                    check(record.has_value(), "The real village campers retain their records");
                    return record->creature.hit_points < record->creature.hit_point_maximum;
                });
        };
        for (unsigned day = 0; day < 64 && wounded(); ++day)
        {
            const auto before = campaign_.party->state().time_minutes;
            check(session.camp(opengold::RestKind::long_rest),
                  "Use the ordinary Camp action in the Lowlander village");
            play();
            check(campaign_.party->state().time_minutes == before + 24 * 60,
                  "A real native village recovery day passes");
        }
        check(!wounded(), "Ordinary native Camp restores the wounded village arrivals");
        walk(81, 13, 3);
        walk(81, 1, 14);
        check(answers_.empty() && session.script_variable(opengold::por::EclAddress{0x4CB1}) == 1,
              "The actual hidden Lowlander child joins the crew");
        save_checkpoint("venus-child");
    }

    void venus_keyring()
    {
        walk(81, 0, 14);
        walk(81, 10, 14, {"KILL THE OFFICER"});
        check(answers_.empty() &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4CB7}) == 1,
              "The real officer encounter yields its actual keyring");
        save_checkpoint("venus-keyring");
    }

    void venus_outskirts()
    {
        walk(81, 8, 15);
        passage(opengold::por::MapDirection::south, {"YES", "NO"});
        check(answers_.empty() && campaign_.session.snapshot().script_id == 80,
              "The actual village gate returns the crew to the Venusian wilderness");
        save_checkpoint("venus-outskirts");
    }

    void venus_base()
    {
        wilderness_walk(9, 14, {"HIDE IN SHADOWS"});
        check(answers_.empty() && campaign_.session.snapshot().script_id == 82 &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4CB8}) == 1,
              "The real wilderness approach enters the Venusian base's top level");
        save_checkpoint("venus-base");
    }

    void venus_landon()
    {
        walk(82, 7, 0, {"3"});
        walk(82, 0, 9);
        check(campaign_.session.script_variable(opengold::por::EclAddress{0x4CB9}) == 1,
              "The actual imprisoned scientists ask the crew to find Landon");
        walk(82, 7, 8, {"4"});
        check(campaign_.session.script_variable(opengold::por::EclAddress{0x4CB8}) == 4,
              "The actual elevator descends to the fourth level");
        walk(82, 15, 12);
        check(answers_.empty() &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4CB9}) == 2,
              "Zane actually rejoins Landon, who reveals the retinal lockpick's hiding place");
        save_checkpoint("venus-landon");
    }

    void venus_retinal()
    {
        walk(82, 15, 8, {"1"});
        check(campaign_.session.script_variable(opengold::por::EclAddress{0x4CB8}) == 1,
              "The actual elevator returns to the top level");
        walk(82, 1, 2);
        check(answers_.empty() &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4CBA}) == 1,
              "The crew finds the real retinal lockpick under Landon's indicated floor panel");
        save_checkpoint("venus-retinal");
    }

    void venus_scientists()
    {
        auto &session = campaign_.session;
        walk(82, 7, 0, {"4"});
        walk(82, 15, 12);
        walk(82, 11, 8);
        passage(opengold::por::MapDirection::west);
        check(session.script_variable(opengold::por::EclAddress{0x4CBB}) == 1,
              "The ordinary passage reveals Landon's secret escape door");
        walk(82, 15, 12);
        check(session.script_variable(opengold::por::EclAddress{0x4CB9}) == 3,
              "Landon asks the crew to fetch the imprisoned scientists");
        walk(82, 15, 8, {"3"});
        walk(82, 0, 9);
        check(session.script_variable(opengold::por::EclAddress{0x4CB9}) == 4,
              "The actual scientists leave their prison to meet Landon");
        walk(82, 7, 8, {"4"});
        walk(82, 15, 12);
        check(answers_.empty() && session.script_variable(opengold::por::EclAddress{0x4CB9}) == 5,
              "The scientists escape through their actual tunnel and Leander says farewell");
        check(std::ranges::count_if(campaign_.party->state().slots,
                                    [](const auto id)
                                    {
                                        return id != 0;
                                    }) == 6,
              "Zane and Leander leave through their actual authored farewells");
        save_checkpoint("venus-scientists");
    }

    void venus_departure()
    {
        walk(82, 15, 8, {"1"});
        walk(82, 0, 4);
        passage(opengold::por::MapDirection::west, {"YES", "NO"});
        check(answers_.empty() && campaign_.session.snapshot().script_id == 80 &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4CBA}) == 1,
              "The real main exit returns the crew with its earned retinal lockpick");
        save_checkpoint("venus-departure");
    }

    void venus_return()
    {
        clinic_treated_.clear();
        wilderness_walk(34, 12, {"optional-clinic", "EXIT"});
        check(answers_.empty() && campaign_.session.snapshot().script_id == 34 &&
                  campaign_.session.snapshot().solar_map,
              "The crew returns to the actual Venus landing site and boards its ship");
        salvation_refit();
        save_checkpoint("venus-return");
    }

    void mercury_entry()
    {
        fly_to(0);
        stop_solar_menus_ = false;
        play({"MARIPOSA THREE"});
        check(answers_.empty() && campaign_.session.snapshot().script_id == 96 &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4C80}) != 0 &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4C5F}) != 0 &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4CBA}) != 0,
              "The three actually earned mission clearances admit the crew to Mariposa Three");
        save_checkpoint("mercury-entry");
    }

    void mercury_parade()
    {
        walk(96, 2, 12);
        walk(96, 1, 9, {"SPEAK", "TWO IF BY SEA"}, {3, 4});
        walk(96, 0, 4, {}, {3, 4});
        walk(96, 8, 3, {"IGNORE"}, {3, 4});
        check(
            answers_.empty() && campaign_.session.snapshot().script_id == 96,
            "The crew follows the actual merchant contact and joins the parade beyond the bazaar");
        save_checkpoint("mercury-parade");
    }

    void mercury_core()
    {
        walk(96, 15, 2);
        passage(opengold::por::MapDirection::east, {"YES", "LEAVE CORE"});
        check(answers_.empty() && campaign_.session.snapshot().script_id == 97,
              "The ordinary core entrance admits the crew after the actual parade passage");
        for (const unsigned address : {0x4C04U, 0x4C05U, 0x4C06U})
        {
            std::cout << "Initial actual core coin field " << std::hex << address << '='
                      << campaign_.session.script_variable(
                             opengold::por::EclAddress{static_cast<std::uint16_t>(address)})
                      << std::dec << '\n';
            check(campaign_.session.script_variable(
                      opengold::por::EclAddress{static_cast<std::uint16_t>(address)}) == 0,
                  "The newly entered core has no unearned coins");
        }
        save_checkpoint("mercury-core");
    }

    void mercury_copper()
    {
        walk(97, 0, 5, {"GRAB COIN", "Gears"});
        check(answers_.empty() &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4C06}) == 1,
              "The real maintenance-robot encounter yields the copper coin");
        save_checkpoint("mercury-copper");
    }

    void mercury_coins()
    {
        walk(97, 3, 3, {"GO UP", "LEAVE CORE"});
        walk(97, 5, 13);
        check(campaign_.session.script_variable(opengold::por::EclAddress{0x4C05}) == 1,
              "The actual closet search uncovers the silver coin");
        walk(97, 3, 10, {"GO UP", "LEAVE CORE"});
        walk(97, 11, 5, {"YES", "Shade"});
        check(answers_.empty() &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4C04}) == 1,
              "The actual plaque and security encounter yield the gold coin");
        save_checkpoint("mercury-coins");
    }

    void sun_king()
    {
        walk(97, 10, 3, {"GO UP", "LEAVE CORE"});
        walk(97, 14, 3);
        passage(opengold::por::MapDirection::north, {"OUI", "OUI", "OUI"});
        check(answers_.empty() &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4CD1}) == 3,
              "The real three-coin audience earns the Sun King's laser deactivation");
        save_checkpoint("sun-king");
    }

    void finale_entry()
    {
        walk(97, 10, 10, {"GO UP", "GO UP", "GO UP", "GO UP", "GO UP", "NO"});
        check(answers_.empty() && campaign_.session.snapshot().script_id == 98,
              "The crew climbs the actual disarmed core and declines descending from its top");
        save_checkpoint("finale-entry");
    }

    void finale_power()
    {
        walk(98, 0, 9, {"YES", "CLIMB STAIRS", "NO"});
        check(campaign_.session.script_variable(opengold::por::EclAddress{0x4C01}) == 3,
              "The actual emergency stairs reach the weapons-control level");
        walk(98, 0, 6, {"YES", "Gears"});
        check(answers_.empty() &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4CCE}) == 1,
              "The real electrical intervention interrupts power to the Doomsday weapon");
        std::cout << "Actual firing count at power interruption: "
                  << campaign_.session.script_variable(opengold::por::EclAddress{0x4CCC}) << '\n';
        save_checkpoint("finale-power");
    }

    void finale_recovery()
    {
        auto &session = campaign_.session;
        check(session.script_variable(opengold::por::EclAddress{0x4CCE}) == 1 &&
                  session.script_variable(opengold::por::EclAddress{0x4CCD}) == 0,
              "Firing is genuinely aborted and no destruction countdown has begun");
        const auto firing_count = session.script_variable(opengold::por::EclAddress{0x4CCC});
        const auto wounded = [&]
        {
            return std::ranges::any_of(
                campaign_.party->state().slots,
                [&](const auto id)
                {
                    if (!id)
                    {
                        return false;
                    }
                    const auto record = og::buck_rogers_record_of(campaign_.party->game_record(id));
                    check(record.has_value(), "The actual power-room campers retain their records");
                    return record->creature.hit_points < record->creature.hit_point_maximum;
                });
        };
        for (unsigned day = 0; day < 64 && wounded(); ++day)
        {
            const auto before = campaign_.party->state().time_minutes;
            check(session.camp(opengold::RestKind::long_rest),
                  "Use the ordinary native Camp action while firing is interrupted");
            play();
            check(campaign_.party->state().time_minutes == before + 24 * 60,
                  "The real recovery day advances the campaign clock");
            check(session.script_variable(opengold::por::EclAddress{0x4CCE}) == 1 &&
                      session.script_variable(opengold::por::EclAddress{0x4CCD}) == 0 &&
                      session.script_variable(opengold::por::EclAddress{0x4CCC}) == firing_count,
                  "Native recovery preserves the authored paused firing state");
        }
        check(!wounded(), "Ordinary native recovery restores the conscious crew before the boss");
        save_checkpoint("finale-recovered");
    }

    void finale_destruct()
    {
        walk(98, 9, 14);
        check(answers_.empty() &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4CCD}) == 1,
              "The real weapons-control battle is won and its destruction mechanism is activated");
        save_checkpoint("finale-destruct");
    }

    void finale_pods()
    {
        // The first attempted step after activation runs the real defense
        // battle. Then use the emergency stairs; the lift has no power.
        walk(98, 0, 2, {"YES", "LEAVE"}, {4, 14, 15});
        check(campaign_.session.script_variable(opengold::por::EclAddress{0x4C01}) == 2,
              "The ordinary emergency stairs descend to pod control");
        walk(98, 13, 5, {}, {15});
        check(answers_.empty() &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4C06}) == 1,
              "Scot actually clears an escape pod through its control terminal");
        save_checkpoint("finale-pods");
    }

    void finale_escape()
    {
        walk(98, 9, 4, {}, {15});
        clinic_treated_.clear();
        passage(opengold::por::MapDirection::south,
                {"EXIT", "PORT", "LAUNCH", "optional-clinic", "EXIT"});
        check(
            answers_.empty() && winning_party_.has_value() &&
                campaign_.session.snapshot().script_id == 34 &&
                campaign_.session.snapshot().solar_map,
            "The actual approved pod escapes, reaches the winning speech and returns to stable flight");
        save_checkpoint("finale-escape");
    }

    void verify_earned_ending()
    {
        check(winning_party_.has_value() &&
                  campaign_.session.script_variable(opengold::por::EclAddress{0x4CCD}) == 1 &&
                  (campaign_.session.script_variable(opengold::por::EclAddress{0x4C66}) & 0x20) !=
                      0,
              "The real mission victory and Salvation celebration have both completed");
        unsigned pcs = 0;
        for (const auto id : campaign_.party->state().slots)
        {
            if (!id)
            {
                continue;
            }
            const auto &member = campaign_.party->member(id);
            if (member.npc_source.empty())
            {
                ++pcs;
            }
            const auto record = og::buck_rogers_record_of(campaign_.party->game_record(id));
            check(record && !member.vitals.dead &&
                      og::status_of(record->creature) != og::BuckRogersStatus::dead &&
                      og::status_of(record->creature) != og::BuckRogersStatus::gone,
                  "Every actual winning party member remains alive");
        }
        check(pcs == 6, "All six fresh PCs complete the actual campaign");
        save_checkpoint("earned-ending");
        std::cout
            << "Countdown earned ending passed: actual final victory, ordinary at-win roster and full-world saves.\n";
    }

    void passage(opengold::por::MapDirection direction, std::vector<std::string> answers = {})
    {
        auto &session = campaign_.session;
        while (session.snapshot().pose.facing != direction)
        {
            const auto turn_command =
                session.snapshot().script_id == 98 &&
                        (static_cast<unsigned>(session.snapshot().pose.facing) + 3) % 4 ==
                            static_cast<unsigned>(direction)
                    ? opengold::por::ExplorationCommand::turn_left
                    : opengold::por::ExplorationCommand::turn_right;
            check(session.explore(turn_command),
                  "Face the authored passage through ordinary turns");
            play();
        }
        check(session.explore(opengold::por::ExplorationCommand::forward),
              "Step through the authored passage");
        play(std::move(answers));
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
        static constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
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
                const auto turn_command =
                    script == 98 &&
                            (static_cast<unsigned>(session.snapshot().pose.facing) + 3) % 4 ==
                                direction[next]
                        ? opengold::por::ExplorationCommand::turn_left
                        : opengold::por::ExplorationCommand::turn_right;
                check(session.explore(turn_command),
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
        for (const auto id : campaign_.party->state().slots)
        {
            if (!id)
            {
                continue;
            }
            const auto &member = campaign_.party->member(id);
            const auto record = og::buck_rogers_record_of(member.vitals.resources);
            check(record && !member.vitals.dead &&
                      og::status_of(record->creature) != og::BuckRogersStatus::dead &&
                      og::status_of(record->creature) != og::BuckRogersStatus::gone,
                  "Every actual current crew member remains alive at the earned checkpoint");
        }
        if (session.snapshot().script_id == 50)
        {
            std::cout << "Talon source position "
                      << session.script_variable(opengold::por::EclAddress{0xC04B}) << ','
                      << session.script_variable(opengold::por::EclAddress{0xC04C}) << " facing "
                      << session.script_variable(opengold::por::EclAddress{0xC04D}) << "; snapshot "
                      << session.snapshot().pose.x << ',' << session.snapshot().pose.y << "; level "
                      << session.script_variable(opengold::por::EclAddress{0x4C12}) << '\n';
        }

        if (session.snapshot().script_id == 64 || session.snapshot().script_id == 80)
        {
            std::cout << "Actual wilderness save context: area " << session.snapshot().area_id
                      << "; bank "
                      << session.resources()->script_banks.at(session.snapshot().script_id)
                      << "; previous " << session.script_variable(opengold::por::EclAddress{0x4BF2})
                      << "; kind " << session.script_variable(opengold::por::EclAddress{0x4BE6})
                      << "; mode " << session.script_variable(opengold::por::EclAddress{0x4BA6})
                      << "; live bank "
                      << session.script_variable(
                             opengold::por::game_address(opengold::por::ecl_script_bank, countdown))
                      << '\n';
        }
        const auto bytes =
            opengold::encode_campaign(*campaign_.party, &session, assets_, countdown);
        const auto verify_saved = [&](const std::string &bytes)
        {
            // Reload into independently loaded assets, not a copy whose current
            // snapshot could accidentally supply state missing from the save.
            auto fresh_session = opengold::load_campaign(folder_, countdown);
            auto loaded =
                opengold::decode_campaign(bytes, *campaign_.creation,
                                          campaign_.party->rule_module(), assets_, &fresh_session);
            auto party = std::make_shared<opengold::CampaignParty>(
                og::rules_module(opengold::load_original_content(folder_, countdown)),
                campaign_.creation);
            party->restore(std::move(loaded.party));
            check(loaded.town.has_value(), "The checkpoint includes the actual world");
            loaded.town->attach_restored_party(party);
            check(loaded.town->snapshot().solar_map == session.snapshot().solar_map,
                  "A restored checkpoint keeps its actual solar-map handoff");
            if (session.snapshot().solar_map)
            {
                check(loaded.town->solar_map_state() == session.solar_map_state(),
                      "A restored earned flight retains its actual phase, days and rocket cell");
            }
            const auto again = opengold::encode_campaign(*party, &*loaded.town, assets_, countdown);
            check(again == bytes, "The earned checkpoint reloads and saves byte for byte");
            if (name == "mars-camp" || name == "mars-departure" || name == "venus-outskirts" ||
                name == "venus-departure")
            {
                check(
                    loaded.town->snapshot().script_id == session.snapshot().script_id &&
                        loaded.town->snapshot().area_id == session.snapshot().area_id &&
                        loaded.town->script_variable(opengold::por::EclAddress{0x4BC3}) ==
                            session.script_variable(opengold::por::EclAddress{0x4BC3}) &&
                        loaded.town->script_variable(opengold::por::EclAddress{0x4BC4}) ==
                            session.script_variable(opengold::por::EclAddress{0x4BC4}),
                    "A retained wilderness save preserves the actual script, district and world square");
            }
            if (name == "venus-outskirts" || name == "venus-departure")
            {
                // Diagnostic copies only: corrupt each authenticated field
                // separately; the earned live campaign is never modified.
                for (const auto &[address, value] :
                     {std::pair{opengold::por::EclAddress{0x4BA6}, 0U},
                      std::pair{opengold::por::EclAddress{0x4BE6}, 0xA8U},
                      std::pair{opengold::por::EclAddress{0x4BF2}, 66U},
                      std::pair{
                          opengold::por::game_address(opengold::por::ecl_script_bank, countdown),
                          4U}})
                {
                    auto changed = session.detached_copy();
                    opengold::test::TourAccess::set_variable(changed, address,
                                                             static_cast<std::uint16_t>(value));
                    const auto bad =
                        opengold::encode_campaign(*campaign_.party, &changed, assets_, countdown);
                    bool rejected = false;
                    try
                    {
                        (void)opengold::decode_campaign(bad, *campaign_.creation,
                                                        campaign_.party->rule_module(), assets_,
                                                        &fresh_session);
                    }
                    catch (const std::runtime_error &error)
                    {
                        check(std::string_view(error.what()) ==
                                  "Saved district and script disagree",
                              "An invalid Venus receipt fails district authentication");
                        rejected = true;
                    }
                    check(rejected,
                          "Corrupted retained Venus contexts cannot bypass save validation");
                }
            }
        };
        verify_saved(bytes);
        if (const auto *prefix = std::getenv("OPENGOLD_COUNTDOWN_EARNED_SAVES"))
        {
            std::ofstream out(std::string(prefix) + std::string(name) + ".ogs", std::ios::binary);
            out << bytes;
            check(static_cast<bool>(out), "Write the ordinary earned checkpoint");
        }
        std::cout << "Earned Countdown " << name << ": " << fights_ << " actual ground victories; "
                  << ship_fights_
                  << " actual ship victories; ordinary save reloads byte for byte.\n";
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
        if (script == 17)
        {
            const auto resources = campaign_.session.resources();
            for (const auto &item : og::shop_stock(1, resources->buck_rogers_templates))
            {
                std::string name;
                for (const auto part : item.name_parts)
                {
                    if (part && part < resources->buck_rogers_item_words.size())
                    {
                        name = resources->buck_rogers_item_words[part] + (name.empty() ? "" : " ") +
                               name;
                    }
                }
                std::cout << "SHOP1 type " << unsigned(item.type) << ' ' << name << " price16 "
                          << og::shop_price(item, 16) << '\n';
            }
            for (const auto &[id, ground] : resources->buck_rogers_creatures)
            {
                const auto useful = [&](const auto &item)
                {
                    return item.type == 24 || og::projectile_kind(item) == 8 ||
                           (item.type < resources->buck_rogers_item_types.size() &&
                            resources->buck_rogers_item_types[item.type].slot == 7);
                };
                if (!std::ranges::any_of(ground.items, useful))
                {
                    continue;
                }
                std::cout << "COUNTERMEASURES creature " << id << ' ' << ground.creature.name;
                for (const auto &item : ground.items)
                {
                    if (useful(item))
                    {
                        std::cout << " type=" << unsigned(item.type)
                                  << " projectile=" << og::projectile_kind(item)
                                  << " quantity=" << unsigned(item.quantity)
                                  << " flags=" << unsigned(item.flags);
                    }
                }
                std::cout << '\n';
            }
        }
        if (script == 32 || script == 33 || script == 48 || script == 49 || script == 50 ||
            script == 52 || script == 64 || script == 66 || script == 80)
        {
            const auto &raw = program->raw();
            const unsigned first = script == 80   ? 0x81E5
                                   : script == 66 ? 0xA3F4
                                   : script == 52 ? 0x8ADB
                                   : script == 64 ? 0xA1BF
                                   : script == 50 ? 0xA38C
                                   : script >= 48 ? 0xA18D
                                                  : 0xA3CD;
            const unsigned last = script == 80 ? 0x841F : script == 52 ? 0x8B70 : 0xA400;
            for (unsigned address = first; address < last && address - 0x8000 + 2 < raw.size();
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
                              << map.at(x, y).event_number() << " raw "
                              << unsigned(map.at(x, y).event_raw) << " edges ";
                    for (unsigned side = 0; side < 4; ++side)
                    {
                        std::cout << unsigned(map.at(x, y).walls[side]) << '/'
                                  << unsigned(map.at(x, y).doors[side]) << ' ';
                    }
                    std::cout << '\n';
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
        const bool actual_enemies = std::ranges::any_of(
            initial.combatants,
            [](const auto &unit)
            {
                return unit.side != opengold::rules::Side::party && unit.hit_points > 0;
            });
        std::cout << (actual_enemies ? "Ground fight " : "Scripted combat processing ")
                  << fights_ + 1 << " in script " << session.snapshot().script_id << ':';
        for (const auto &actor : initial.combatants)
        {
            std::cout << ' ' << actor.name << '=' << actor.hit_points << '@' << actor.cell.x << ','
                      << actor.cell.y;
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
        const auto missile_foe = [](const auto &unit)
        {
            if (unit.side == opengold::rules::Side::party || unit.hit_points <= 0 || unit.fled)
            {
                return false;
            }
            const auto record = og::buck_rogers_record_of(unit.persistent.resources);
            if (!record)
            {
                return false;
            }
            const auto weapon = og::readied_weapon(record->items, record->types);
            return weapon && og::projectile_kind(record->items[*weapon]) >= 10;
        };
        const bool missile_encounter = session.snapshot().script_id == 98 &&
                                       std::ranges::any_of(initial.combatants, missile_foe);
        std::optional<unsigned> chaff_round;
        bool launcher_prepared = false;
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
                if (std::getenv("OPENGOLD_COUNTDOWN_COMBAT_TRACE"))
                {
                    for (const auto &line : state.log())
                    {
                        std::cout << "RULE TRACE " << fights_ + 1 << ": " << line << '\n';
                    }
                }
                check(state.outcome == opengold::rules::Outcome::victory,
                      "The earned ground battle must actually be won; outcome " +
                          std::to_string(static_cast<int>(state.outcome)));
                check(session.resolve_combat(state), "The campaign commits the real combat result");
                if (actual_enemies)
                {
                    ++fights_;
                }
                return;
            }
            const auto actor = std::ranges::find(state.combatants, state.actor,
                                                 &opengold::rules::CombatantView::id);
            const bool pc =
                actor != state.combatants.end() && actor->side == opengold::rules::Side::party &&
                !opengold::is_ally(state.actor) &&
                std::ranges::any_of(campaign_.party->state().roster,
                                    [&](const auto &member)
                                    {
                                        return member.npc_source.empty() &&
                                               member.character.sheet().name == actor->name;
                                    });
            bool throwing_chaff = false;
            bool preparing_launcher = false;
            unsigned chaff_before = 0;
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
                const auto record = og::buck_rogers_record_of(actor->persistent.resources);
                check(record.has_value(), "The real actor retains its current equipment");
                const auto owns = [&](unsigned type)
                {
                    return std::ranges::any_of(record->items,
                                               [&](const auto &item)
                                               {
                                                   return item.type == type;
                                               });
                };
                const bool chaff_available =
                    std::ranges::any_of(record->items,
                                        [](const auto &item)
                                        {
                                            return item.type == 37 && item.quantity > 0;
                                        });
                const bool want_chaff = missile_encounter && chaff_available &&
                                        (!chaff_round || *chaff_round != state.round) &&
                                        std::ranges::any_of(state.combatants, missile_foe);
                const unsigned weapon =
                    want_chaff          ? 37U
                    : missile_encounter ? (owns(13) ? 13U : 7U)
                    : owns(11)          ? 11U
                    : og::career_from_record(record->creature.career) == Profession::warrior ? 7U
                    : owns(10)                                                               ? 10U
                                                                                             : 7U;
                const unsigned armor = owns(23) ? 23U : 22U;
                for (const auto type : {armor, weapon})
                {
                    const auto desired = std::ranges::find(record->items, type, &br::Item::type);
                    if (desired == record->items.end() || (desired->flags & br::Item::readied))
                    {
                        continue;
                    }
                    const auto slot = record->types.at(desired->type).slot;
                    unsigned index = static_cast<unsigned>(desired - record->items.begin());
                    std::string verb = "ready";
                    for (unsigned n = 0; n < record->items.size(); ++n)
                    {
                        const auto &held = record->items[n];
                        if ((held.flags & br::Item::readied) &&
                            record->types.at(held.type).slot == slot)
                        {
                            index = n;
                            verb = "unready";
                            break;
                        }
                    }
                    const auto ready = std::ranges::find_if(offered,
                                                            [&](const auto &candidate)
                                                            {
                                                                return candidate.verb == verb &&
                                                                       candidate.item == index;
                                                            });
                    check(ready != offered.end(),
                          "Ready the won gear through an actual offered combat command: " +
                              actor->name + " " + verb + " item " + std::to_string(index) +
                              " type " + std::to_string(type));
                    order = *ready;
                    break;
                }
                const auto script = session.snapshot().script_id;
                if (!launcher_prepared && (script == 81 || script == 82 || script == 97) &&
                    order.verb != "ready" && order.verb != "unready")
                {
                    const auto launcher = std::ranges::find(record->items, 16, &br::Item::type);
                    if (launcher != record->items.end() && !(launcher->flags & br::Item::readied))
                    {
                        const auto index = static_cast<unsigned>(launcher - record->items.begin());
                        const auto ready = std::ranges::find_if(
                            offered,
                            [&](const auto &candidate)
                            {
                                return candidate.verb == "ready" && candidate.item == index;
                            });
                        if (ready != offered.end())
                        {
                            order = *ready;
                            preparing_launcher = true;
                        }
                    }
                }
                const auto armed = og::readied_weapon(record->items, record->types);
                if (want_chaff && armed && record->items[*armed].type == 37)
                {
                    int best_cover = -1000000;
                    std::optional<opengold::rules::Command> launch;
                    for (const auto &candidate : offered)
                    {
                        if (candidate.verb != "launch")
                        {
                            continue;
                        }
                        const auto cloud = og::lay_ground_cloud(
                            candidate.destination, og::CloudMatter::chaff, true,
                            [&](auto cell)
                            {
                                return state.battlefield.contains(cell) &&
                                       !opengold::rules::blocks_movement(
                                           state.battlefield.at(cell));
                            });
                        int cover = 0;
                        for (const auto &enemy : state.combatants)
                        {
                            if (!missile_foe(enemy))
                            {
                                continue;
                            }
                            if (cloud.covers_square(enemy.cell))
                            {
                                cover += 1000;
                            }
                            for (const auto &member : state.combatants)
                            {
                                if (member.side != opengold::rules::Side::party ||
                                    member.hit_points <= 0)
                                {
                                    continue;
                                }
                                const int steps = distance(enemy.cell, member.cell);
                                for (int at = 0; at <= steps && steps > 0; ++at)
                                {
                                    const opengold::rules::Cell cell{
                                        (enemy.cell.x * (steps - at) + member.cell.x * at +
                                         steps / 2) /
                                            steps,
                                        (enemy.cell.y * (steps - at) + member.cell.y * at +
                                         steps / 2) /
                                            steps};
                                    if (cloud.covers_square(cell))
                                    {
                                        cover += distance(cell, member.cell) > 3 ? 100 : -1000;
                                        break;
                                    }
                                }
                            }
                        }
                        if (cover > best_cover)
                        {
                            best_cover = cover;
                            launch = candidate;
                        }
                    }
                    check(launch.has_value(), "The actual held chaff grenade has an offered throw");
                    order = *launch;
                    throwing_chaff = true;
                    for (const auto &item : record->items)
                    {
                        if (item.type == 37)
                        {
                            chaff_before += item.quantity;
                        }
                    }
                }
            }
            check(combat.submit(order), "The rules accept the actual combat action");
            if (preparing_launcher)
            {
                launcher_prepared = true;
                std::cout << "Actual launcher preparation by " << actor->name << " in script "
                          << session.snapshot().script_id << '\n';
            }
            if (throwing_chaff)
            {
                const auto after = combat.combat().snapshot();
                const auto unit = std::ranges::find(after.combatants, state.actor,
                                                    &opengold::rules::CombatantView::id);
                check(unit != after.combatants.end(), "The actual chaff thrower remains in combat");
                const auto record = og::buck_rogers_record_of(unit->persistent.resources);
                check(record.has_value(), "The actual chaff throw keeps its native inventory");
                unsigned remaining = 0;
                for (const auto &item : record->items)
                {
                    if (item.type == 37)
                    {
                        remaining += item.quantity;
                    }
                }
                check(remaining + 1 == chaff_before,
                      "The real throw consumes one won chaff grenade");
                chaff_round = state.round;
                std::cout << "Actual chaff throw by " << actor->name << " at "
                          << order.destination.x << ',' << order.destination.y << " round "
                          << state.round << '\n';
            }
        }
        throw std::runtime_error("The actual ground fight exceeded its command bound");
    }

    std::filesystem::path folder_;
    Campaign campaign_;
    std::string assets_;
    std::vector<br::Item> templates_;
    std::deque<std::string> answers_;
    std::optional<std::string> winning_party_;
    opengold::RandomState random_{20261120};
    unsigned fights_{};
    unsigned ship_fights_{};
    unsigned solar_steps_{};
    bool stop_solar_menus_{};
    std::vector<std::string> clinic_treated_;
};
} // namespace

int main(int argc, char **argv)
{
    try
    {
        const std::string_view command = argc == 2 ? argv[1] : "";
        static constexpr std::array stages{"",
                                           "--chicagorg",
                                           "--derelict",
                                           "--scot",
                                           "--argon",
                                           "--ship-earned",
                                           "--solar-arrival",
                                           "--asteroid-entry",
                                           "--asteroid-lower",
                                           "--asteroid-card",
                                           "--asteroid-clearance",
                                           "--asteroid-return",
                                           "--talon-boarded",
                                           "--talon-captured",
                                           "--buck-rescued",
                                           "--talon-clinic",
                                           "--talon-plans",
                                           "--talon-galley",
                                           "--talon-engineering",
                                           "--talon-escape",
                                           "--mars-entry",
                                           "--mars-village",
                                           "--mars-defense",
                                           "--mars-camp",
                                           "--mars-base",
                                           "--mars-blue-card",
                                           "--mars-prototype",
                                           "--mars-departure",
                                           "--mars-return",
                                           "--venus-entry",
                                           "--venus-ambush",
                                           "--venus-village",
                                           "--venus-child",
                                           "--venus-keyring",
                                           "--venus-outskirts",
                                           "--venus-base",
                                           "--venus-landon",
                                           "--venus-retinal",
                                           "--venus-scientists",
                                           "--venus-departure",
                                           "--venus-return",
                                           "--mercury-entry",
                                           "--mercury-parade",
                                           "--mercury-core",
                                           "--mercury-copper",
                                           "--mercury-coins",
                                           "--sun-king",
                                           "--finale-entry",
                                           "--finale-power",
                                           "--finale-recovered",
                                           "--finale-destruct",
                                           "--finale-pods",
                                           "--finale-escape",
                                           "--earned-ending"};
        const auto stage = std::ranges::find(stages, command);
        const auto folder = opengold::test::installed_game_folder(countdown);
        if (folder.empty() && argc <= 2 &&
            (stage != stages.end() || command == "--ending-protocol" ||
             command == "--camp-service"))
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
        if (command == "--ending-protocol")
        {
            player.ending_protocol();
            return 0;
        }
        if (command == "--camp-service")
        {
            player.opening();
            player.camp_service();
            return 0;
        }
        check(argc <= 2 && stage != stages.end(), "Unknown earned-route argument");
        const auto progress = stage - stages.begin();
        if (progress >= 9)
        {
            player.novice();
        }
        player.opening();
        if (progress >= 1)
        {
            player.chicagorg();
        }
        if (progress >= 2)
        {
            player.derelict();
        }
        if (progress >= 3)
        {
            player.scot();
        }
        if (progress >= 4)
        {
            player.argon();
        }
        if (progress >= 5)
        {
            player.earn_ship();
        }
        if (progress == 6)
        {
            player.solar_arrival();
        }
        if (progress >= 7)
        {
            player.asteroid_entry();
        }
        if (progress >= 8)
        {
            player.asteroid_lower();
        }
        if (progress >= 9)
        {
            player.asteroid_card();
        }
        if (progress >= 10)
        {
            player.asteroid_clearance();
        }
        if (progress >= 11)
        {
            player.asteroid_return();
        }
        if (progress >= 12)
        {
            player.talon_boarded();
        }
        if (progress >= 13)
        {
            player.talon_captured();
        }
        if (progress >= 14)
        {
            player.buck_rescued();
        }
        if (progress >= 15)
        {
            player.talon_clinic();
        }
        if (progress >= 16)
        {
            player.talon_plans();
        }
        if (progress >= 17)
        {
            player.talon_galley();
        }
        if (progress >= 18)
        {
            player.talon_engineering();
        }
        if (progress >= 19)
        {
            player.talon_escape();
        }
        if (progress >= 20)
        {
            player.mars_entry();
        }
        if (progress >= 21)
        {
            player.mars_village();
        }
        if (progress >= 22)
        {
            player.mars_defense();
        }
        if (progress >= 23)
        {
            player.mars_camp();
        }
        if (progress >= 24)
        {
            player.mars_base();
        }
        if (progress >= 25)
        {
            player.mars_blue_card();
        }
        if (progress >= 26)
        {
            player.mars_prototype();
        }
        if (progress >= 27)
        {
            player.mars_departure();
        }
        if (progress >= 28)
        {
            player.mars_return();
        }
        if (progress >= 29)
        {
            player.venus_entry();
        }
        if (progress >= 30)
        {
            player.venus_ambush();
        }
        if (progress >= 31)
        {
            player.venus_village();
        }
        if (progress >= 32)
        {
            player.venus_child();
        }
        if (progress >= 33)
        {
            player.venus_keyring();
        }
        if (progress >= 34)
        {
            player.venus_outskirts();
        }
        if (progress >= 35)
        {
            player.venus_base();
        }
        if (progress >= 36)
        {
            player.venus_landon();
        }
        if (progress >= 37)
        {
            player.venus_retinal();
        }
        if (progress >= 38)
        {
            player.venus_scientists();
        }
        if (progress >= 39)
        {
            player.venus_departure();
        }
        if (progress >= 40)
        {
            player.venus_return();
        }
        if (progress >= 41)
        {
            player.mercury_entry();
        }
        if (progress >= 42)
        {
            player.mercury_parade();
        }
        if (progress >= 43)
        {
            player.mercury_core();
        }
        if (progress >= 44)
        {
            player.mercury_copper();
        }
        if (progress >= 45)
        {
            player.mercury_coins();
        }
        if (progress >= 46)
        {
            player.sun_king();
        }
        if (progress >= 47)
        {
            player.finale_entry();
        }
        if (progress >= 48)
        {
            player.finale_power();
        }
        if (progress >= 49)
        {
            player.finale_recovery();
        }
        if (progress >= 50)
        {
            player.finale_destruct();
        }
        if (progress >= 51)
        {
            player.finale_pods();
        }
        if (progress >= 52)
        {
            player.finale_escape();
        }
        if (command == "--earned-ending")
        {
            player.verify_earned_ending();
        }
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
