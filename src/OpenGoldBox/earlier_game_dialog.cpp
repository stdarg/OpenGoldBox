#include "earlier_game_dialog.h"
#include "application_settings.h"
#include "game_lifecycle.h"
#include "game_encounters.h"
#include "game_folder_questions.h"
#include "game_monsters.h"
#include "game_resources.h"
#include "game_rules.h"
#include "game_wall_art.h"
#include "godot_nodes.h"
#include "godot_path.h"
#include "godot_text.h"
#include "guarded_handlers.h"
#include "localization.h"
#include "save_slots.h"
#include "opengold/campaign_save.h"
#include "opengold/campaign_session.h"
#include "opengold/diagnostic_log.h"
#include "opengold/original.h"
#include "opengold/winner_bank.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>
using namespace godot;
using namespace opengold;
using presentation::required_node;

namespace
{
using presentation::gs;

std::filesystem::path game_folder_of(const settings::Settings &settings, GoldBoxGame game)
{
    return presentation::path_from_godot(settings.game_folder(game));
}

// Whether a save is the earlier game's under the original rules, whose
// characters the campaign can bring in. A file that cannot be read is not
// offered.
bool save_under_original_rules(const std::filesystem::path &file, GoldBoxGame earlier)
{
    try
    {
        const auto saved = saved_campaign_rules(read_campaign_file(file));
        return game_rules_of(saved).set == GameRules::Set::original &&
               original::game_of(saved.identity) == earlier;
    }
    catch (const std::exception &e)
    {
        log_diagnostic(LogPriority::warning, "save", e.what());
        return false;
    }
}

// An earlier game's save's party under the rules it was made with, its world
// built from that game's folder, as loading it does.
PartyState saved_party(const settings::Settings &settings, const std::filesystem::path &file,
                       GoldBoxGame earlier)
{
    const auto folder = game_folder_of(settings, earlier);
    const RulesFiles files{presentation::path_from_godot(game_rules_file()), folder, earlier};
    const auto &wall_art = game_wall_art();
    const auto monsters = game_monsters();
    auto loaded = load_campaign_under_its_rules(
        read_campaign_file(file), files, campaign_asset_identity(folder),
        [&]
        {
            return load_campaign(folder, earlier,
                                 earlier == GoldBoxGame::pool_of_radiance
                                     ? CampaignData{&game_encounters(), &wall_art, monsters}
                                     : CampaignData{});
        });
    return std::move(loaded.saved.party);
}

// "Tarl / Fighter / Level 2", the levels of each class as the original's
// sheet shows them ("2/3").
String member_row(const PartyMember &member)
{
    const auto &sheet = member.character.sheet();
    std::string levels;
    for (const int level : sheet.original->class_levels)
    {
        if (level > 0)
        {
            levels += (levels.empty() ? "" : "/") + std::to_string(level);
        }
    }
    // A Buck Rogers character has one career and its level.
    if (levels.empty())
    {
        levels = std::to_string(sheet.level);
    }
    const auto row = i18n::format("{name} / {class} / Level {levels}",
                                  {{"name", gs(sheet.name)},
                                   {"class", i18n::text(sheet.character_class)},
                                   {"levels", gs(levels)}});
    return member.npc_source.empty() ? row : i18n::format(N_("{member} (NPC)"), {{"member", row}});
}

// What the dialog says of each earlier game: its title, what to choose, that
// none was found, and what a character brings.
struct EarlierGameTexts
{
    const char *title;
    const char *choose;
    const char *none;
    const char *arrival;
};

EarlierGameTexts texts_of(GoldBoxGame earlier)
{
    switch (earlier)
    {
    case GoldBoxGame::curse_of_the_azure_bonds:
        return {
            N_("Add character from Curse of the Azure Bonds"),
            N_("Choose a Curse of the Azure Bonds save, then a character to bring into the party."),
            N_("No Curse of the Azure Bonds game saved under the original rules was found."),
            N_("Brings its levels, experience, spells and items, and its share of all the party's money.")};
    case GoldBoxGame::secret_of_the_silver_blades:
        return {
            N_("Add character from Secret of the Silver Blades"),
            N_("Choose a Secret of the Silver Blades save, then a character to bring into the party."),
            N_("No Secret of the Silver Blades game saved under the original rules was found."),
            N_("Brings its levels, experience, spells and items, and its share of the party's platinum, gems and jewelry.")};
    case GoldBoxGame::gateway_to_the_savage_frontier:
        return {
            N_("Add character from Gateway to the Savage Frontier"),
            N_("Choose a Gateway to the Savage Frontier save, then a character to bring into the party."),
            N_("No Gateway to the Savage Frontier game saved under the original rules was found."),
            settings::gateway_money_kept()
                ? N_("Brings its levels, experience and the items Treasures knows, and its share of the party's platinum, gems and jewelry.")
                : N_("Brings its levels, experience and the items Treasures knows, and 200 platinum; its money stays behind.")};
    case GoldBoxGame::champions_of_krynn:
        return {
            N_("Add character from Champions of Krynn"),
            N_("Choose a Champions of Krynn save, then a character to bring into the party."),
            N_("No Champions of Krynn game saved under the original rules was found."),
            N_("Brings its earned levels, experience, wounds, spells, surviving items and share of all party money.")};
    case GoldBoxGame::death_knights_of_krynn:
        return {
            N_("Add character from Death Knights of Krynn"),
            N_("Choose a Death Knights of Krynn save, then a character to bring into the party."),
            N_("No Death Knights of Krynn game saved under the original rules was found."),
            N_("Brings its earned levels, experience, wounds, spells, surviving items and share of all party money.")};
    case GoldBoxGame::countdown_to_doomsday:
        return {
            N_("Add character from Countdown to Doomsday"),
            N_("Choose a Countdown to Doomsday save, then a character to bring into the party."),
            N_("No Countdown to Doomsday game saved under the original rules was found."),
            N_("Brings its whole record as it was: its level, experience, credits, skills and items.")};
    default:
        return {
            N_("Add character from Pool of Radiance"),
            N_("Choose a Pool of Radiance save, then a character to bring into the party."),
            N_("No Pool of Radiance game saved under the original rules was found."),
            N_("Brings its levels, experience and items, and 300 platinum; its money stays behind.")};
    }
}
} // namespace

std::optional<GoldBoxGame> earlier_game_of(GoldBoxGame game)
{
    switch (game)
    {
    case GoldBoxGame::curse_of_the_azure_bonds:
        return GoldBoxGame::pool_of_radiance;
    case GoldBoxGame::secret_of_the_silver_blades:
        return GoldBoxGame::curse_of_the_azure_bonds;
    case GoldBoxGame::matrix_cubed:
        return GoldBoxGame::countdown_to_doomsday;
    case GoldBoxGame::treasures_of_the_savage_frontier:
        return GoldBoxGame::gateway_to_the_savage_frontier;
    case GoldBoxGame::pools_of_darkness:
        return GoldBoxGame::secret_of_the_silver_blades;
    case GoldBoxGame::death_knights_of_krynn:
        return GoldBoxGame::champions_of_krynn;
    case GoldBoxGame::dark_queen_of_krynn:
        return GoldBoxGame::death_knights_of_krynn;
    default:
        return std::nullopt;
    }
}

void EarlierGameDialog::_ready()
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  connect_controls();
                              });
}

void EarlierGameDialog::connect_controls()
{
    required_node<ItemList>(*this, "Margin/Rows/Lists/SaveColumn/Saves")
        .connect("item_selected", presentation::guarded(this, &EarlierGameDialog::select_save));
    required_node<ItemList>(*this, "Margin/Rows/Lists/MemberColumn/Members")
        .connect("item_selected", presentation::guarded(this, &EarlierGameDialog::select_member));
    required_node<Button>(*this, "Margin/Rows/Buttons/Add")
        .connect("pressed", presentation::guarded(this, &EarlierGameDialog::add));
    required_node<Button>(*this, "Margin/Rows/Buttons/Close")
        .connect("pressed", presentation::guarded(this, &EarlierGameDialog::close));
    connect("close_requested", presentation::guarded(this, &EarlierGameDialog::close));
}

void EarlierGameDialog::connect_host(CampaignAccess campaign, std::function<void()> member_added,
                                     GameFolderQuestions &folder_questions)
{
    if (!campaign || !member_added)
    {
        throw std::logic_error("The earlier game's dialog needs its campaign and a follow-up");
    }
    campaign_ = std::move(campaign);
    member_added_ = std::move(member_added);
    folder_questions_ = &folder_questions;
}

void EarlierGameDialog::report_failure(const std::exception &failure)
{
    required_node<Label>(*this, "Margin/Rows/Status").set_text(i18n::text(failure.what()));
}

GoldBoxGame EarlierGameDialog::earlier_game() const
{
    const auto earlier = earlier_game_of(original::game_of(campaign_().identity()));
    if (!earlier)
    {
        throw std::logic_error("This campaign brings no characters from an earlier game");
    }
    return *earlier;
}

void EarlierGameDialog::open()
{
    saves_.clear();
    members_.clear();
    brought_.clear();
    const auto earlier = earlier_game();
    const auto texts = texts_of(earlier);
    required_node<Label>(*this, "Margin/Rows/Title").set_text(i18n::text(texts.title));
    const auto directory = campaign_save_directory(earlier);
    if (std::filesystem::exists(directory))
    {
        for (const auto &entry : std::filesystem::directory_iterator(directory))
        {
            const auto &file = entry.path();
            if (entry.is_regular_file() && file.extension() == ".ogs" &&
                !campaign_save_name(file).empty() && save_under_original_rules(file, earlier))
            {
                saves_.push_back(file);
            }
        }
    }
    std::ranges::sort(saves_);
    // The party that won the game, banked at its win, comes first.
    const auto winning_party = winning_party_file(earlier);
    if (std::filesystem::exists(winning_party) && save_under_original_rules(winning_party, earlier))
    {
        saves_.insert(saves_.begin(), winning_party);
    }
    auto &saves = required_node<ItemList>(*this, "Margin/Rows/Lists/SaveColumn/Saves");
    saves.clear();
    for (const auto &file : saves_)
    {
        saves.add_item(file == winning_party ? i18n::text(N_("Winning party (banked at the win)"))
                                             : gs(campaign_save_name(file)));
    }
    required_node<ItemList>(*this, "Margin/Rows/Lists/MemberColumn/Members").clear();
    required_node<Button>(*this, "Margin/Rows/Buttons/Add").set_disabled(true);
    required_node<Label>(*this, "Margin/Rows/Status")
        .set_text(i18n::text(saves_.empty() ? texts.none : texts.choose));
    popup_centered();
}

void EarlierGameDialog::select_save(std::int64_t index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= saves_.size())
    {
        return;
    }
    if (const auto game = folder_to_ask())
    {
        ask_folder(*game, index);
        return;
    }
    list_members(index);
}

// The earlier game's folder, whose save is read, then the campaign's own,
// whose item lists the characters' items are matched against.
std::optional<GoldBoxGame> EarlierGameDialog::folder_to_ask() const
{
    for (const auto game : {earlier_game(), original::game_of(campaign_().identity())})
    {
        if (!GameFolderQuestions::folder_chosen(application_settings(*this), game))
        {
            return game;
        }
    }
    return std::nullopt;
}

// A folder the search found, or none, is asked about before the import reads
// from it, as at startup (the user's decision, 2026-11-20). The dialog steps
// aside while the questions show, both being exclusive, and comes back when
// they close: with the save's characters once the folders are known, without
// them when the player goes back.
void EarlierGameDialog::ask_folder(GoldBoxGame game, std::int64_t save_index)
{
    hide();
    folder_questions_->ask(
        {game},
        [this, save_index]
        {
            popup_centered();
            presentation::run_guarded(*this,
                                      [&]
                                      {
                                          select_save(save_index);
                                      });
        },
        [this]
        {
            required_node<ItemList>(*this, "Margin/Rows/Lists/SaveColumn/Saves").deselect_all();
            popup_centered();
        });
}

void EarlierGameDialog::list_members(std::int64_t index)
{
    auto &members = required_node<ItemList>(*this, "Margin/Rows/Lists/MemberColumn/Members");
    members.clear();
    members_.clear();
    brought_.clear();
    required_node<Button>(*this, "Margin/Rows/Buttons/Add").set_disabled(true);
    const auto earlier = earlier_game();
    const auto &file = saves_[static_cast<std::size_t>(index)];
    auto party = saved_party(application_settings(*this), file, earlier);
    source_wealth_ = party.wealth;
    // A winning party offers its winners, its permanent NPCs after its
    // player characters; another save its player characters.
    std::vector<MemberId> offered;
    if (file == winning_party_file(earlier))
    {
        offered = winners(party);
    }
    else
    {
        for (const auto &member : party.roster)
        {
            if (member.npc_source.empty())
            {
                offered.push_back(member.id);
            }
        }
    }
    player_characters_ = 0;
    for (const auto id : offered)
    {
        auto &member = *std::ranges::find(party.roster, id, &PartyMember::id);
        player_characters_ += member.npc_source.empty() ? 1 : 0;
        members.add_item(member_row(member));
        members_.push_back(std::move(member));
    }
    brought_.assign(members_.size(), false);
    required_node<Label>(*this, "Margin/Rows/Status")
        .set_text(i18n::text(members_.empty() ? N_("This save holds no player characters.")
                                              : N_("Choose a character to bring into the party.")));
}

void EarlierGameDialog::select_member(std::int64_t index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= members_.size())
    {
        return;
    }
    member_index_ = static_cast<std::size_t>(index);
    show_member_state();
}

void EarlierGameDialog::show_member_state()
{
    // A player character takes one of the six PC positions, an NPC one of the
    // two NPC positions.
    const auto &slots = campaign_().state().slots;
    const bool npc = chosen_is_npc();
    const auto first = slots.begin() + (npc ? 6 : 0);
    const bool full = std::none_of(first, first + (npc ? 2 : 6),
                                   [](auto id)
                                   {
                                       return !id;
                                   });
    const bool brought = brought_.at(member_index_);
    required_node<Button>(*this, "Margin/Rows/Buttons/Add").set_disabled(brought || full);
    const auto *const arrival =
        npc ? N_("Joins in an NPC position with its levels, experience and items; it brings no money.")
            : texts_of(earlier_game()).arrival;
    required_node<Label>(*this, "Margin/Rows/Status")
        .set_text(i18n::text(brought ? N_("Brought into the party.")
                             : full  ? (npc ? N_("Both NPC positions are occupied.")
                                            : N_("All six PC positions are occupied."))
                                     : arrival));
}

void EarlierGameDialog::add()
{
    if (member_index_ >= members_.size() || brought_.at(member_index_))
    {
        return;
    }
    bring_in(members_[member_index_]);
    brought_[member_index_] = true;
    member_added_();
    show_member_state();
}

void EarlierGameDialog::bring_in(const PartyMember &member)
{
    const auto earlier = earlier_game();
    (void)bring_into_sequel(
        campaign_(), member,
        {.earlier = earlier,
         .earlier_folder = game_folder_of(application_settings(*this), earlier),
         .sequel_folder =
             game_folder_of(application_settings(*this), original::game_of(campaign_().identity())),
         .source_wealth = source_wealth_,
         .player_character = member_index_,
         .player_characters = player_characters_,
         .gateway_money_kept = settings::gateway_money_kept()});
}

void EarlierGameDialog::close()
{
    hide();
}
