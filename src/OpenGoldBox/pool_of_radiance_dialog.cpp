#include "pool_of_radiance_dialog.h"
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
#include "opengold/diagnostic_log.h"
#include "opengold/original.h"
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

constexpr auto pool_of_radiance = GoldBoxGame::pool_of_radiance;

std::filesystem::path pool_folder()
{
    return presentation::path_from_godot(settings::game_folder(pool_of_radiance));
}

// Whether a save is Pool of Radiance's under the original rules, whose
// characters Curse can bring in. A file that cannot be read is not offered.
bool pool_save_under_original_rules(const std::filesystem::path &file)
{
    try
    {
        const auto saved = saved_campaign_rules(read_campaign_file(file));
        return game_rules_of(saved).set == GameRules::Set::original &&
               original::game_of(saved.identity) == pool_of_radiance;
    }
    catch (const std::exception &e)
    {
        log_diagnostic(LogPriority::warning, "save", e.what());
        return false;
    }
}

// A Pool of Radiance save's party under the rules it was made with, its town
// built from Pool of Radiance's folder when it has one, as loading it does.
PartyState pool_party(const std::filesystem::path &file)
{
    const auto folder = pool_folder();
    const RulesFiles files{presentation::path_from_godot(game_rules_file()), folder,
                           pool_of_radiance};
    auto loaded = load_campaign_under_its_rules(
        read_campaign_file(file), files, campaign_asset_identity(folder),
        [&]
        {
            return por::RolfTourSession::load(folder, game_encounters(), game_wall_art(),
                                              game_monsters());
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
    return i18n::format("{name} / {class} / Level {levels}",
                        {{"name", gs(sheet.name)},
                         {"class", i18n::text(sheet.character_class)},
                         {"levels", gs(levels)}});
}
} // namespace

void PoolOfRadianceDialog::_ready()
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  connect_controls();
                              });
}

void PoolOfRadianceDialog::connect_controls()
{
    required_node<ItemList>(*this, "Margin/Rows/Lists/SaveColumn/Saves")
        .connect("item_selected", presentation::guarded(this, &PoolOfRadianceDialog::select_save));
    required_node<ItemList>(*this, "Margin/Rows/Lists/MemberColumn/Members")
        .connect("item_selected",
                 presentation::guarded(this, &PoolOfRadianceDialog::select_member));
    required_node<Button>(*this, "Margin/Rows/Buttons/Add")
        .connect("pressed", presentation::guarded(this, &PoolOfRadianceDialog::add));
    required_node<Button>(*this, "Margin/Rows/Buttons/Close")
        .connect("pressed", presentation::guarded(this, &PoolOfRadianceDialog::close));
    connect("close_requested", presentation::guarded(this, &PoolOfRadianceDialog::close));
}

void PoolOfRadianceDialog::connect_host(CampaignAccess campaign, std::function<void()> member_added)
{
    if (!campaign || !member_added)
    {
        throw std::logic_error("The Pool of Radiance dialog needs its campaign and a follow-up");
    }
    campaign_ = std::move(campaign);
    member_added_ = std::move(member_added);
}

void PoolOfRadianceDialog::report_failure(const std::exception &failure)
{
    required_node<Label>(*this, "Margin/Rows/Status").set_text(i18n::text(failure.what()));
}

void PoolOfRadianceDialog::open()
{
    saves_.clear();
    members_.clear();
    brought_.clear();
    const auto directory = campaign_save_directory(pool_of_radiance);
    if (std::filesystem::exists(directory))
    {
        for (const auto &entry : std::filesystem::directory_iterator(directory))
        {
            const auto &file = entry.path();
            if (entry.is_regular_file() && file.extension() == ".ogs" &&
                !campaign_save_name(file).empty() && pool_save_under_original_rules(file))
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
    required_node<Label>(*this, "Margin/Rows/Status")
        .set_text(i18n::text(
            saves_.empty()
                ? N_("No Pool of Radiance game saved under the original rules was found.")
                : N_("Choose a Pool of Radiance save, then a character to bring into the party.")));
    popup_centered();
}

void PoolOfRadianceDialog::select_save(std::int64_t index)
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
    auto party = pool_party(saves_[static_cast<std::size_t>(index)]);
    for (auto &member : party.roster)
    {
        if (member.npc_source.empty())
        {
            members.add_item(member_row(member));
            members_.push_back(std::move(member));
        }
    }
    brought_.assign(members_.size(), false);
    required_node<Label>(*this, "Margin/Rows/Status")
        .set_text(i18n::text(members_.empty() ? N_("This save holds no player characters.")
                                              : N_("Choose a character to bring into the party.")));
}

void PoolOfRadianceDialog::select_member(std::int64_t index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= members_.size())
    {
        return;
    }
    member_index_ = static_cast<std::size_t>(index);
    show_member_state();
}

void PoolOfRadianceDialog::show_member_state()
{
    const auto &slots = campaign_().state().slots;
    const bool full = std::none_of(slots.begin(), slots.begin() + 6,
                                   [](auto id)
                                   {
                                       return !id;
                                   });
    const bool brought = brought_.at(member_index_);
    required_node<Button>(*this, "Margin/Rows/Buttons/Add").set_disabled(brought || full);
    required_node<Label>(*this, "Margin/Rows/Status")
        .set_text(i18n::text(
            brought ? N_("Brought into the party.")
            : full
                ? N_("All six PC positions are occupied.")
                : N_("Brings its levels, experience and items, and 300 platinum; its money stays behind.")));
}

void PoolOfRadianceDialog::add()
{
    if (member_index_ >= members_.size() || brought_.at(member_index_))
    {
        return;
    }
    (void)campaign_().bring_across(members_[member_index_]);
    brought_[member_index_] = true;
    member_added_();
    show_member_state();
}

void PoolOfRadianceDialog::close()
{
    hide();
}
