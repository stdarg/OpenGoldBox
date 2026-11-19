#include "earlier_game_dialog.h"
#include "application_settings.h"
#include "game_encounters.h"
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
#include "opengold/darkness_import.h"
#include "opengold/death_knights_import.h"
#include "opengold/diagnostic_log.h"
#include "opengold/original.h"
#include "opengold/treasures_import.h"
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

std::filesystem::path game_folder_of(GoldBoxGame game)
{
    return presentation::path_from_godot(settings::game_folder(game));
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
PartyState saved_party(const std::filesystem::path &file, GoldBoxGame earlier)
{
    const auto folder = game_folder_of(earlier);
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

// Each player character's even share of the party's platinum, gems and
// jewelry: OpenGoldBox keeps a party's money in one purse, where the original
// gives each character its own.
CampaignParty::BroughtMoney money_share(const PartyState &party, std::size_t characters)
{
    const auto share = [&](Coin coin)
    {
        return static_cast<std::uint32_t>(party.wealth[static_cast<std::size_t>(coin)] /
                                          std::max<std::size_t>(characters, 1));
    };
    return {share(Coin::platinum), share(Coin::gems), share(Coin::jewelry)};
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
    return i18n::format("{name} / {class} / Level {levels}",
                        {{"name", gs(sheet.name)},
                         {"class", i18n::text(sheet.character_class)},
                         {"levels", gs(levels)}});
}
} // namespace

std::optional<GoldBoxGame> earlier_game_of(GoldBoxGame game)
{
    switch (game)
    {
    case GoldBoxGame::curse_of_the_azure_bonds:
        return GoldBoxGame::pool_of_radiance;
    case GoldBoxGame::treasures_of_the_savage_frontier:
        return GoldBoxGame::gateway_to_the_savage_frontier;
    case GoldBoxGame::pools_of_darkness:
        return GoldBoxGame::secret_of_the_silver_blades;
    case GoldBoxGame::death_knights_of_krynn:
        return GoldBoxGame::champions_of_krynn;
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

void EarlierGameDialog::connect_host(CampaignAccess campaign, std::function<void()> member_added)
{
    if (!campaign || !member_added)
    {
        throw std::logic_error("The earlier game's dialog needs its campaign and a follow-up");
    }
    campaign_ = std::move(campaign);
    member_added_ = std::move(member_added);
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
    money_share_.reset();
    const auto earlier = earlier_game();
    const bool from_gateway = earlier == GoldBoxGame::gateway_to_the_savage_frontier;
    const bool from_secret = earlier == GoldBoxGame::secret_of_the_silver_blades;
    required_node<Label>(*this, "Margin/Rows/Title")
        .set_text(i18n::text(from_gateway  ? N_("Add character from Gateway to the Savage Frontier")
                             : from_secret ? N_("Add character from Secret of the Silver Blades")
                                           : N_("Add character from Pool of Radiance")));
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
    auto &saves = required_node<ItemList>(*this, "Margin/Rows/Lists/SaveColumn/Saves");
    saves.clear();
    for (const auto &file : saves_)
    {
        saves.add_item(gs(campaign_save_name(file)));
    }
    required_node<ItemList>(*this, "Margin/Rows/Lists/MemberColumn/Members").clear();
    required_node<Button>(*this, "Margin/Rows/Buttons/Add").set_disabled(true);
    const auto status =
        !saves_.empty()
            ? (from_gateway
                   ? N_("Choose a Gateway to the Savage Frontier save, then a character to bring into the party.")
               : from_secret
                   ? N_("Choose a Secret of the Silver Blades save, then a character to bring into the party.")
                   : N_("Choose a Pool of Radiance save, then a character to bring into the party."))
        : from_gateway
            ? N_("No Gateway to the Savage Frontier game saved under the original rules was found.")
        : from_secret
            ? N_("No Secret of the Silver Blades game saved under the original rules was found.")
            : N_("No Pool of Radiance game saved under the original rules was found.");
    required_node<Label>(*this, "Margin/Rows/Status").set_text(i18n::text(status));
    popup_centered();
}

void EarlierGameDialog::select_save(std::int64_t index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= saves_.size())
    {
        return;
    }
    auto &members = required_node<ItemList>(*this, "Margin/Rows/Lists/MemberColumn/Members");
    members.clear();
    members_.clear();
    brought_.clear();
    required_node<Button>(*this, "Margin/Rows/Buttons/Add").set_disabled(true);
    const auto earlier = earlier_game();
    auto party = saved_party(saves_[static_cast<std::size_t>(index)], earlier);
    source_wealth_ = party.wealth;
    for (auto &member : party.roster)
    {
        if (member.npc_source.empty())
        {
            members.add_item(member_row(member));
            members_.push_back(std::move(member));
        }
    }
    brought_.assign(members_.size(), false);
    money_share_.reset();
    if (earlier != GoldBoxGame::pool_of_radiance)
    {
        money_share_ = money_share(party, members_.size());
    }
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
    const auto &slots = campaign_().state().slots;
    const bool full = std::none_of(slots.begin(), slots.begin() + 6,
                                   [](auto id)
                                   {
                                       return !id;
                                   });
    const bool brought = brought_.at(member_index_);
    required_node<Button>(*this, "Margin/Rows/Buttons/Add").set_disabled(brought || full);
    // Pools of Darkness's LoadChar keeps a Secret character's own platinum,
    // gems and jewelry, and its spells.
    const auto earlier = earlier_game();
    const auto arrival =
        earlier == GoldBoxGame::champions_of_krynn
            ? N_("Brings its earned levels, experience, wounds, spells, surviving items and share of all party money.")
        : earlier == GoldBoxGame::pool_of_radiance
            ? N_("Brings its levels, experience and items, and 300 platinum; its money stays behind.")
        : earlier == GoldBoxGame::secret_of_the_silver_blades
            ? N_("Brings its levels, experience, spells and items, and its share of the party's platinum, gems and jewelry.")
        : settings::gateway_money_kept()
            ? N_("Brings its levels, experience and the items Treasures knows, and its share of the party's platinum, gems and jewelry.")
            : N_("Brings its levels, experience and the items Treasures knows, and 200 platinum; its money stays behind.");
    required_node<Label>(*this, "Margin/Rows/Status")
        .set_text(i18n::text(brought ? N_("Brought into the party.")
                             : full  ? N_("All six PC positions are occupied.")
                                     : arrival));
}

void EarlierGameDialog::add()
{
    if (member_index_ >= members_.size() || brought_.at(member_index_))
    {
        return;
    }
    const auto &member = members_[member_index_];
    const auto earlier = earlier_game();
    if (earlier == GoldBoxGame::champions_of_krynn)
    {
        const auto folder = game_folder_of(GoldBoxGame::death_knights_of_krynn);
        (void)campaign_().bring_across(
            member, champions_items_for_death_knights(folder),
            champions_money_for_death_knights(source_wealth_, member_index_, members_.size()),
            CampaignParty::ConvertedItems::readied_as_they_were);
    }
    else if (earlier == GoldBoxGame::secret_of_the_silver_blades)
    {
        const auto darkness = game_folder_of(GoldBoxGame::pools_of_darkness);
        (void)campaign_().bring_across(member, secret_items_for_darkness(darkness), money_share_,
                                       CampaignParty::ConvertedItems::readied_as_they_were);
    }
    else if (earlier == GoldBoxGame::gateway_to_the_savage_frontier)
    {
        const auto treasures = game_folder_of(GoldBoxGame::treasures_of_the_savage_frontier);
        (void)campaign_().bring_across(member, gateway_items_for_treasures(treasures),
                                       settings::gateway_money_kept() ? money_share_
                                                                      : std::nullopt);
    }
    else
    {
        (void)campaign_().bring_across(member);
    }
    brought_[member_index_] = true;
    member_added_();
    show_member_state();
}

void EarlierGameDialog::close()
{
    hide();
}
