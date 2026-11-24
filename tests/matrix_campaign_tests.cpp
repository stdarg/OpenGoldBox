// Matrix's actual single-disk campaign. Ending protocol inspection is a
// separate check and never supplies an earned-route completion certificate.
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
#include "test_support.h"
#include "tour_access.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <deque>
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
        {og::Race::martian, og::Profession::rogue, "Shade"},
        {og::Race::tinker, og::Profession::engineer, "Gears"},
        {og::Race::venusian, og::Profession::medic, "Doc"},
        {og::Race::mercurian, og::Profession::warrior, "Merc"},
    }};
    opengold::RandomState random{2026};
    for (const auto &[race, career, name] : team)
    {
        auto creature =
            og::new_buck_rogers_character(race, og::Gender::male, career, matrix, random);
        creature.name = name;
        creature.icon = og::default_icon(race, og::Gender::male, career, random);
        const auto skills = og::SkillSpending::at_creation(creature, matrix);
        check(skills.career_points() == 0 && skills.general_points() == 0,
              "Matrix creation supplies presets without unspent skill points");
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
        check(record->creature.skills == skills.levels(),
              "Each fresh Matrix character actually receives its required career presets");
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

bool settle_route(Campaign &campaign, const std::filesystem::path &folder, unsigned &fights)
{
    std::map<std::string, unsigned> menus;
    bool divided = false;
    bool clinic = false;
    std::string patient;
    std::set<std::string> healed;
    unsigned purchase_type = 0;
    opengold::MemberId purchase_buyer{};
    unsigned purchase_credits = 0;
    bool purchase_sent = false;
    const auto wounded = [&]() -> std::optional<std::string>
    {
        for (const auto &member : campaign.party->state().roster)
        {
            const auto record = og::buck_rogers_record_of(campaign.party->game_record(member.id));
            if (record && !healed.contains(record->creature.name) && record->creature.status != 2 &&
                record->creature.status != 3 &&
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
        check(s.phase != TourPhase::faulted && campaign.session.script_diagnostics().empty(),
              "Actual Matrix route event failed: " + s.diagnostic);
        if (s.phase == TourPhase::completed)
        {
            return true;
        }
        if (s.phase == TourPhase::running)
        {
            continue;
        }
        if (s.phase == TourPhase::awaiting_continue)
        {
            std::size_t pick = 0;
            std::string key = s.dialogue;
            for (const auto &choice : s.choices)
            {
                key += '\n' + choice;
            }
            const auto repeat = ++menus[key];
            if (s.dialogue.find("DO YOU NEED HEALING") != std::string::npos)
            {
                clinic = true;
            }
            if (clinic && !patient.empty() && s.dialogue.find(" IS HEALED...") != std::string::npos)
            {
                healed.insert(patient);
                patient.clear();
            }
            std::string wanted;
            if (std::ranges::find(s.choices, "Buy") != s.choices.end() &&
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
                    const auto type = !owns(21) ? 21U
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
                    purchase_type = type;
                    purchase_buyer = id;
                    purchase_credits = record->creature.credits;
                    purchase_sent = false;
                    wanted = "Buy";
                    break;
                }
            }
            else if (std::ranges::find(s.choices, "Back") != s.choices.end() && purchase_type)
            {
                if (purchase_sent)
                {
                    const auto record =
                        og::buck_rogers_record_of(campaign.party->game_record(purchase_buyer));
                    const auto cost = purchase_type == 21 ? 1200U : 335U;
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
                    const std::string prefix = purchase_type == 21 ? "Smartsuit:" : "Laser Pistol:";
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
            else if (clinic && s.choosing_member && wounded() &&
                     std::ranges::find(s.choices, *wounded()) != s.choices.end())
            {
                wanted = *wounded();
                patient = wanted;
            }
            else if (s.dialogue.find("DO YOU NEED HEALING") != std::string::npos ||
                     s.dialogue.find("SOMEONE ELSE?") != std::string::npos)
            {
                wanted = wounded() ? "YES" : "NO";
            }
            else if (s.dialogue.find("The booty:") != std::string::npos)
            {
                wanted = divided ? "Exit" : "Divvy";
                divided = true;
            }
            else
            {
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
            check(campaign.session.choose(s.continue_ticket, pick), "Choose Matrix's actual menu");
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
            std::map<opengold::rules::EntityId, std::deque<std::pair<std::string, unsigned>>>
                equipment;
            for (const auto &member : campaign.party->state().roster)
            {
                const auto record =
                    og::buck_rogers_record_of(campaign.party->game_record(member.id));
                if (!record)
                {
                    continue;
                }
                for (const auto type : {21U, 9U})
                {
                    const auto desired = std::ranges::find(record->items, type, &br::Item::type);
                    if (desired == record->items.end() || (desired->flags & br::Item::readied))
                    {
                        continue;
                    }
                    const auto slot = record->types.at(desired->type).slot;
                    for (unsigned index = 0; index < record->items.size(); ++index)
                    {
                        const auto &held = record->items[index];
                        if ((held.flags & br::Item::readied) &&
                            record->types.at(held.type).slot == slot)
                        {
                            equipment[member.id].emplace_back("unready", index);
                        }
                    }
                    equipment[member.id].emplace_back(
                        "ready", static_cast<unsigned>(desired - record->items.begin()));
                }
            }
            combat.encounter(*campaign.session.pending_encounter(), 7);
            for (unsigned turn = 0; turn < 8000 && combat.combat().snapshot().outcome ==
                                                       opengold::rules::Outcome::ongoing;
                 ++turn)
            {
                const auto state = combat.combat().snapshot();
                const auto actor = std::ranges::find(state.combatants, state.actor,
                                                     &opengold::rules::CombatantView::id);
                const bool member = actor != state.combatants.end() &&
                                    actor->side == opengold::rules::Side::party &&
                                    !opengold::is_ally(state.actor);
                opengold::rules::Command command;
                if (member && !equipment[state.actor].empty())
                {
                    const auto &[verb, item] = equipment[state.actor].front();
                    const auto legal = combat.combat().legal_commands();
                    const auto found = std::ranges::find_if(legal,
                                                            [&](const auto &candidate)
                                                            {
                                                                return candidate.verb == verb &&
                                                                       candidate.item == item;
                                                            });
                    check(found != legal.end(),
                          "Purchased gear is readied through an actual legal combat command");
                    command = *found;
                    equipment[state.actor].pop_front();
                }
                else
                {
                    command = member ? opengold::choose_quick_command(combat.combat(), true,
                                                                      opengold::Searching::everyone)
                                     : opengold::choose_demo_command(combat.combat(),
                                                                     opengold::Searching::everyone,
                                                                     opengold::default_difficulty);
                }
                check(combat.submit(command), "Matrix's actual fight accepts " + command.verb);
            }
            const auto outcome = combat.combat().snapshot();
            if (outcome.outcome != opengold::rules::Outcome::victory)
            {
                const auto lines = outcome.log();
                for (std::size_t line = lines.size() > 50 ? lines.size() - 50 : 0;
                     line < lines.size(); ++line)
                {
                    std::cout << "Actual fight log: " << lines[line] << '\n';
                }
            }
            check(outcome.outcome != opengold::rules::Outcome::ongoing,
                  "Matrix's real fight terminates");
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
        std::cout << "Concrete Matrix transaction boundary: phase "
                  << static_cast<unsigned>(s.phase) << ", script " << s.script_id << ", map "
                  << s.area_id << ", solar " << s.solar_map << ".\n";
        return false;
    }
    throw std::runtime_error("Actual Matrix route event did not settle");
}

bool walk_route(Campaign &campaign, const std::filesystem::path &folder, unsigned goal_x,
                unsigned goal_y, unsigned &fights, bool service = false)
{
    using Command = opengold::por::ExplorationCommand;
    using Direction = opengold::por::MapDirection;
    const auto area = campaign.session.snapshot().area_id;
    for (unsigned step = 0; step < 512; ++step)
    {
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
                if (cell.doors[n] > 1 || (cell.walls[n] && !cell.doors[n]))
                {
                    continue;
                }
                const auto side = static_cast<Direction>(n);
                const auto nx = map.wrapped_x(static_cast<int>(x) + opengold::por::step_x(side));
                const auto ny = map.wrapped_y(static_cast<int>(y) + opengold::por::step_y(side));
                const auto next = ny * map.width + nx;
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
        for (unsigned turns = 0; campaign.session.snapshot().pose.facing != wanted && turns < 4;
             ++turns)
        {
            check(campaign.session.explore(Command::turn_right),
                  "Turn through Matrix's player controls");
            if (!settle_route(campaign, folder, fights))
            {
                return false;
            }
        }
        check(campaign.session.explore(Command::forward), "Walk through Matrix's actual map");
        if (!settle_route(campaign, folder, fights))
        {
            return false;
        }
        const auto &s = campaign.session.snapshot();
        std::cout << "Route step map " << s.area_id << " at " << s.pose.x << ',' << s.pose.y
                  << ".\n";
        if (s.area_id != area)
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

bool play_earned_route(Campaign &campaign, const std::filesystem::path &folder,
                       bool preparation_only)
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
    if (!walk_route(campaign, folder, 2, 12, fights, true))
    {
        return false;
    }
    std::cout << "Genuine Matrix Caloris route reached the coronation hall after " << fights
              << " real fights.\n";
    saved_campaign_round_trips(campaign, folder, campaign.session.snapshot().game_won);
    return campaign.session.snapshot().game_won;
}

bool play_genuine_opening(const std::filesystem::path &folder, bool require_full_ending,
                          bool preparation_only = false, bool easy = false)
{
    auto campaign = fresh_campaign(folder);
    if (easy)
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
                return play_earned_route(campaign, folder, preparation_only);
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

// The town-presence field is the first body difference from the same party's
// save without a town. Change only its following script field and recompute the
// real checksum, so negative cases exercise context validation, not corruption.
std::string saved_with_other_script(const std::string &saved, const std::string &party_only,
                                    unsigned script)
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
    check(body.substr(start, end - start) == "32", "The actual wilderness save names script32");
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
            loaded.town && loaded.town->snapshot().script_id == 32 &&
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
            saved_with_other_script(saved, party_only, 35),
            "An unrelated program cannot use a wilderness kind to bypass district validation",
            {"Saved district and script disagree"});
        rejects_context(
            saved_with_other_script(saved, party_only, 48),
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
        opengold::test::TourAccess::set_variable(changed, opengold::por::EclAddress{0x4BE6}, 0xA2);
        rejects_context(
            opengold::encode_campaign_in_format(*campaign.party, &changed, assets, format, matrix),
            "Another wilderness kind does not authenticate this controller",
            {"Invalid saved Matrix wilderness context"});
        opengold::test::TourAccess::set_variable(changed, opengold::por::EclAddress{0x4BA6}, 0);
        opengold::test::TourAccess::set_variable(changed, opengold::por::EclAddress{0x4BE6}, 0x81);
        const auto ordinary =
            opengold::encode_campaign_in_format(*campaign.party, &changed, assets, format, matrix);
        rejects_context(saved_with_other_script(ordinary, party_only, 35),
                        "Ordinary mismatched districts remain rejected",
                        {"Saved district and script disagree"});
        changed = campaign.session.detached_copy();
        opengold::test::TourAccess::set_variable(changed, opengold::por::EclAddress{0x4BF2}, 48);
        rejects_context(
            opengold::encode_campaign_in_format(*campaign.party, &changed, assets, format, matrix),
            "The restored VM identity must match the saved controller",
            {"Invalid saved Matrix wilderness context"});
    }
}

// An isolated installed-program regression. The full earned route reaches
// this same controller through the real assignment and Solar arrival.
void installed_wilderness_save_protocol(const std::filesystem::path &folder)
{
    auto campaign = fresh_campaign(folder);
    const auto read_pages = [&]
    {
        for (unsigned page = 0; page < 1000; ++page)
        {
            campaign.session.advance(100);
            const auto state = campaign.session.snapshot();
            std::string diagnostic = state.diagnostic;
            for (const auto &message : campaign.session.script_diagnostics())
            {
                diagnostic += '\n' + message;
            }
            check(state.phase != TourPhase::faulted &&
                      campaign.session.script_diagnostics().empty(),
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
                check(campaign.session.choose(state.continue_ticket, pick),
                      "Read the actual installed wilderness setup");
            }
            else
            {
                check(state.phase == TourPhase::running,
                      "The isolated wilderness setup needs no assumed fight outcome");
            }
        }
        throw std::runtime_error("The installed wilderness fixture did not settle");
    };
    // Reading the actual opening establishes its portrait/host state too.
    // Rebuilding a bare inspection VM would skip that original setup.
    read_pages();
    const auto world = opengold::test::TourAccess::program(campaign.session, 32);
    opengold::test::TourAccess::run_from(campaign.session, 32, world->entries().at(4));
    read_pages();
    retained_wilderness_save_checks(campaign, folder);
    const auto y = campaign.session.script_variable(opengold::por::EclAddress{0x4BC4});
    check(campaign.session.explore(opengold::por::ExplorationCommand::north),
          "The installed wilderness accepts an ordinary movement command");
    read_pages();
    check(campaign.session.script_variable(opengold::por::EclAddress{0x4BC4}) + 1 == y,
          "The ordinary step advances the actual wilderness square");
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
    read_pages();
    check(campaign.session.snapshot().overland &&
              campaign.session.snapshot().overland_direction == 2 &&
              campaign.session.snapshot().overland_x ==
                  campaign.session.script_variable(opengold::por::EclAddress{0x4BC3}),
          "The native wilderness heading and coordinates publish before the 3D bounds guard");
    retained_wilderness_save_checks(campaign, folder, false);
    const auto district = opengold::test::TourAccess::program(campaign.session, 17);
    opengold::test::TourAccess::run_from(campaign.session, 17, district->entries().at(4));
    read_pages();
    check(!campaign.session.snapshot().overland && !campaign.session.snapshot().solar_map,
          "Leaving the wilderness clears its movement publication");
    const auto solar = opengold::test::TourAccess::program(campaign.session, 19);
    opengold::test::TourAccess::run_from(campaign.session, 19, solar->entries().at(4));
    read_pages();
    check(campaign.session.snapshot().solar_map && !campaign.session.snapshot().overland,
          "The existing Solar controls remain separate from wilderness controls");
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
        const bool easy = argc > 1 && std::string_view(argv[1]) == "--earned-ending-easy";
        const bool full = easy || (argc > 1 && std::string_view(argv[1]) == "--earned-ending");
        const bool preparation =
            argc > 1 && std::string_view(argv[1]) == "--earned-caloris-preparation";
        if (argc == 1)
        {
            native_difficulty_keeps_all_six_settings();
            dock_scratch_resumes_an_older_save(folder);
            installed_wilderness_save_protocol(folder);
            matrix_format_preserves_released_history(folder);
            actual_program_two_is_a_saved_terminal_win(folder);
            std::cout << "Authored Matrix ending protocol roundtrip passed (diagnostic only).\n";
        }
        if (!play_genuine_opening(folder, full || preparation, preparation, easy))
        {
            return 1;
        }
        std::cout
            << "Matrix campaign protocol checks passed; full earned ending remains separate.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
