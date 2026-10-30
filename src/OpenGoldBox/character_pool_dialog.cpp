#include "godot_text.h"
#include "character_pool_dialog.h"
#include "character_sheet_text.h"
#include "godot_images.h"
#include "godot_nodes.h"
#include "guarded_handlers.h"
#include "localization.h"
#include "opengold/character_pool.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
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
} // namespace

void CharacterPoolDialog::_ready()
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  connect_controls();
                              });
}

void CharacterPoolDialog::connect_controls()
{
    required_node<ItemList>(*this, "List")
        .connect("item_selected", presentation::guarded(this, &CharacterPoolDialog::select));
    required_node<Button>(*this, "Add")
        .connect("pressed", presentation::guarded(this, &CharacterPoolDialog::add));
    required_node<Button>(*this, "Close")
        .connect("pressed", presentation::guarded(this, &CharacterPoolDialog::close));
    connect("close_requested", presentation::guarded(this, &CharacterPoolDialog::close));
}

void CharacterPoolDialog::connect_host(std::unique_ptr<const rules::CharacterRules> rules,
                                       const por::CharacterArt &art, PortraitCatalog &portraits,
                                       CampaignAccess campaign, std::function<void()> member_added,
                                       FailureReport report)
{
    if (!rules || !campaign || !member_added || !report)
    {
        throw std::logic_error(
            "The character pool needs its rules, campaign, a follow-up and a report");
    }
    rules_ = std::move(rules);
    art_ = &art;
    portraits_ = &portraits;
    campaign_ = std::move(campaign);
    member_added_ = std::move(member_added);
    report_ = std::move(report);
}

void CharacterPoolDialog::report_failure(const std::exception &failure)
{
    report_(failure);
}

void CharacterPoolDialog::fit(Vector2 view_size)
{
    required_node<Node>(*this, "WindowLayout").call("fit", Vector2i(view_size));
}

void CharacterPoolDialog::open()
{
    if (pool_.empty())
    {
        pool_ = character_pool(*rules_, *art_);
        for (auto &c : pool_)
        {
            auto a = c.appearance();
            a.portrait = portraits_->recommended(c.creation_data());
            c.appearance(a);
        }
    }
    auto *list = &required_node<ItemList>(*this, "List");
    list->clear();
    for (const auto &character : pool_)
    {
        list->add_item(i18n::text(character.sheet().character_class) + " / " +
                       gs(character.sheet().name));
    }
    list->select(pool_index_);
    select(pool_index_);
    popup_centered();
}

void CharacterPoolDialog::mark_added(const CampaignParty &campaign)
{
    pool_added_.clear();
    for (unsigned i = 0; i < 48; ++i)
    {
        for (const auto &m : campaign.state().roster)
        {
            if (m.creation_source == "pool:v1:" + std::to_string(i))
            {
                pool_added_.push_back(i);
            }
        }
    }
}

void CharacterPoolDialog::select(std::int64_t index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= pool_.size())
    {
        return;
    }
    pool_index_ = static_cast<unsigned>(index);
    const auto &character = pool_[pool_index_];
    required_node<RichTextLabel>(*this, "Text").call("render", presentation::sheet_text(character));
    required_node<TextureRect>(*this, "Portrait")
        .set_texture(portraits_->texture(character.appearance(), character.creation_data()));
    for (unsigned i = 1; i < 3; ++i)
    {
        const auto source = art_->icon(character.appearance(),
                                       i == 2 ? por::IconPose::action : por::IconPose::ready);
        required_node<TextureRect>(*this, i == 1 ? "Ready" : "Action")
            .set_texture(presentation::image_texture(source));
    }
    const bool added =
        std::find(pool_added_.begin(), pool_added_.end(), pool_index_) != pool_added_.end();
    const auto &slots = campaign_().state().slots;
    const bool full = std::none_of(slots.begin(), slots.begin() + 6,
                                   [](auto id)
                                   {
                                       return !id;
                                   });
    required_node<Button>(*this, "Add").set_disabled(added || full);
    required_node<Label>(*this, "Status")
        .set_text(i18n::text(
            added  ? N_("Already added. Use Rejoin party for a reserved member.")
            : full ? N_("All six PC positions are occupied.")
                   : N_("Starts with 3d6 x 10 gp to buy its gear. Preview portraits and both combat poses before adding.")));
}

void CharacterPoolDialog::add()
{
    try
    {
        if (std::find(pool_added_.begin(), pool_added_.end(), pool_index_) != pool_added_.end())
        {
            return;
        }
        (void)add_pool_member(campaign_(), pool_.at(pool_index_), pool_index_);
        pool_added_.push_back(pool_index_);
        member_added_();
        select(pool_index_);
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "Status").set_text(i18n::text(e.what()));
    }
}

void CharacterPoolDialog::close()
{
    hide();
}
