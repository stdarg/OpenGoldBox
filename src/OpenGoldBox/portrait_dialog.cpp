#include "portrait_dialog.h"
#include "godot_nodes.h"
#include "godot_text.h"
#include "guarded_handlers.h"
#include "localization.h"
#include "original_portrait_parts.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>
using namespace godot;
using presentation::required_node;

void PortraitDialog::_ready()
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  connect_controls();
                              });
}

void PortraitDialog::connect_controls()
{
    const auto step =
        [&](const char *path, void (PortraitDialog::*method)(std::int64_t), int direction)
    {
        required_node<Button>(*this, path)
            .connect("pressed", presentation::guarded(this, method).bind(direction));
    };
    step("%Previous", &PortraitDialog::step_new, -1);
    step("%Next", &PortraitDialog::step_new, 1);
    step("%Original/HeadPrevious", &PortraitDialog::step_head, -1);
    step("%Original/HeadNext", &PortraitDialog::step_head, 1);
    step("%Original/BodyPrevious", &PortraitDialog::step_body, -1);
    step("%Original/BodyNext", &PortraitDialog::step_body, 1);
    required_node<OptionButton>(*this, "%Choice")
        .connect("item_selected", presentation::guarded(this, &PortraitDialog::new_selected));
    required_node<Button>(*this, "%Close")
        .connect("pressed", presentation::guarded(this, &PortraitDialog::close));
    connect("close_requested", presentation::guarded(this, &PortraitDialog::close));
}

void PortraitDialog::open(CampaignAccess campaign, opengold::MemberId member,
                          PortraitCatalog &portraits, std::function<void()> changed)
{
    if (!campaign || !changed)
    {
        throw std::logic_error("The portrait dialog needs its campaign and a follow-up");
    }
    campaign_ = std::move(campaign);
    member_ = member;
    portraits_ = &portraits;
    changed_ = std::move(changed);
    const auto &draft = this->member().character.creation_data();
    offered_.clear();
    const auto &entries = portraits_->entries();
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        if (PortraitCatalog::same(entries[i].race, draft.race) &&
            PortraitCatalog::same(entries[i].gender, draft.gender))
        {
            offered_.push_back(i);
        }
    }
    if (offered_.empty())
    {
        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            offered_.push_back(i);
        }
    }
    auto &choice = required_node<OptionButton>(*this, "%Choice");
    choice.clear();
    for (const auto index : offered_)
    {
        choice.add_item(presentation::gs(entries[index].filename));
    }
    set_title(
        i18n::format(N_("{name}'s portrait"),
                     {{"name", String::utf8(this->member().character.sheet().name.c_str())}}));
    show_portrait();
    popup_centered();
}

void PortraitDialog::report_failure(const std::exception &failure)
{
    required_node<Label>(*this, "%Problem").set_text(String::utf8(failure.what()));
}

const opengold::PartyMember &PortraitDialog::member() const
{
    return campaign_().member(member_);
}

// The member's new portrait to pick from while it shows one, else its
// original head and body to build.
void PortraitDialog::show_portrait()
{
    const auto &m = member();
    const auto &appearance = m.character.appearance();
    const auto &draft = m.character.creation_data();
    const bool new_art = portraits_->shows_new(appearance, draft);
    required_node<TextureRect>(*this, "%Preview")
        .set_texture(portraits_->texture(appearance, draft));
    required_node<Control>(*this, "%NewArt").set_visible(new_art);
    required_node<Control>(*this, "%Original").set_visible(!new_art);
    if (new_art)
    {
        const auto &entries = portraits_->entries();
        const auto shown = portraits_->shown_file(appearance, draft);
        const auto found = std::ranges::find_if(offered_,
                                                [&](std::size_t index)
                                                {
                                                    return entries[index].filename == shown;
                                                });
        required_node<OptionButton>(*this, "%Choice")
            .select(found == offered_.end() ? -1 : static_cast<int>(found - offered_.begin()));
        return;
    }
    presentation::show_original_parts(required_node<Node>(*this, "%Original"),
                                      portraits_->original_art(), draft, appearance);
}

void PortraitDialog::step_new(std::int64_t direction)
{
    if (offered_.empty())
    {
        return;
    }
    const auto &choice = required_node<OptionButton>(*this, "%Choice");
    const auto count = static_cast<std::int64_t>(offered_.size());
    const std::int64_t at = choice.get_selected();
    new_selected(at < 0 ? (direction > 0 ? 0 : count - 1)
                        : (at + direction % count + count) % count);
}

void PortraitDialog::new_selected(std::int64_t index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= offered_.size())
    {
        throw std::runtime_error("Invalid portrait selection");
    }
    auto appearance = member().character.appearance();
    appearance.portrait =
        portraits_->entries().at(offered_[static_cast<std::size_t>(index)]).filename;
    change(appearance);
}

void PortraitDialog::step_head(std::int64_t direction)
{
    const auto &m = member();
    change(presentation::stepped_original_part(m.character.appearance(), portraits_->original_art(),
                                               m.character.creation_data(),
                                               presentation::OriginalPart::head, direction));
}

void PortraitDialog::step_body(std::int64_t direction)
{
    const auto &m = member();
    change(presentation::stepped_original_part(m.character.appearance(), portraits_->original_art(),
                                               m.character.creation_data(),
                                               presentation::OriginalPart::body, direction));
}

void PortraitDialog::change(const opengold::por::CharacterAppearance &appearance)
{
    campaign_().set_appearance(member_, appearance);
    show_portrait();
    changed_();
}

void PortraitDialog::close()
{
    hide();
}
