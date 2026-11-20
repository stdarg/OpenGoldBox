// Matrix's actual single-disk campaign. Ending protocol inspection is a
// separate check and never supplies an earned-route completion certificate.
#include "opengold/buck_rogers_characters.h"
#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/campaign_save.h"
#include "opengold/campaign_session.h"
#include "opengold/character.h"
#include "opengold/formats.h"
#include "opengold/original.h"
#include "opengold/original_content.h"
#include "installed_games.h"
#include "test_support.h"
#include "tour_access.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
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
          "The actual starting weapon appears in Matrix's specialization menu");
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

bool play_genuine_opening(const std::filesystem::path &folder, bool require_full_ending)
{
    auto campaign = fresh_campaign(folder);
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
                std::cout << "Earned Matrix ending not certified: the current route stops at "
                             "the genuine opening exploration state.\n";
                return false;
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
        const bool full = argc > 1 && std::string_view(argv[1]) == "--earned-ending";
        if (argc == 1)
        {
            matrix_format_preserves_released_history(folder);
            actual_program_two_is_a_saved_terminal_win(folder);
            std::cout << "Authored Matrix ending protocol roundtrip passed (diagnostic only).\n";
        }
        if (!play_genuine_opening(folder, full))
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
