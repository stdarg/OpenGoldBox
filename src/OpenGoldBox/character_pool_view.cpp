#include "godot_images.h"
#include "localization.h"
#include "character_creation_view.h"
#include "rolf_tour_view.h"
#include "opengold/character_pool.h"
#include "opengold/srd5.h"
#include "godot_nodes.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/image.hpp>
#include <algorithm>
using namespace godot;
using namespace opengold;
using presentation::required_node;

namespace
{
String gs(std::string_view s)
{
    return String::utf8(s.data(), s.size());
}
} // namespace

void CharacterCreationView::pool_layout()
{
    const double w = std::min(1000.0, double(get_size().x) - 64), h = get_size().y - 120;
    const auto place = [&](const char *path, Rect2 r)
    {
        auto *n = &required_node<Control>(*this, path);
        n->set_position(r.position);
        n->set_size(r.size);
    };
    for (const char *name :
            {"PoolModal", "TownSheet"
            })
        required_node<Window>(*this, name).set_size(Vector2i(w, h));
    place("PoolModal/Background", Rect2(0, 0, w, h));
    place("TownSheet/Background", Rect2(0, 0, w, h));
    place("PoolModal/Title", Rect2(20, 16, w - 40, 36));
    place("PoolModal/List", Rect2(20, 64, 260, h - 154));
    place("PoolModal/Portrait", Rect2(300, 64, 188, 188));
    place("PoolModal/Ready", Rect2(300, 270, 88, 88));
    place("PoolModal/Action", Rect2(400, 270, 88, 88));
    place("PoolModal/Text", Rect2(508, 64, w - 528, h - 154));
    place("PoolModal/Status", Rect2(20, h - 82, w - 350, 62));
    place("PoolModal/Add", Rect2(w - 310, h - 58, 150, 36));
    place("PoolModal/Close", Rect2(w - 146, h - 58, 126, 36));
    place("TownSheet/Text", Rect2(24, 24, w - 48, h - 100));
    place("TownSheet/Close", Rect2(w - 154, h - 56, 130, 36));
    place("TownSheet/MakeLeader", Rect2(w - 314, h - 56, 150, 36));
}

void CharacterCreationView::show_pool()
{
    try
    {
        if (pool_.empty())
        {
            pool_ = character_pool(creator_->rules(), *art_);
            for (auto &c : pool_)
            {
                auto a = c.appearance();
                a.portrait = recommended_portrait(c.creation_data());
                c.appearance(a);
            }
        }
        auto *list = &required_node<ItemList>(*this, "PoolModal/List");
        list->clear();
        for (const auto &character : pool_)
            list->add_item(i18n::text(character.sheet().character_class) + " / " +
                           gs(character.sheet().name));
        list->select(pool_index_);
        pool_selected(pool_index_);
        required_node<Window>(*this, "PoolModal").popup_centered();
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "PartyPanel/Status").set_text(i18n::text(e.what()));
    }
}

void CharacterCreationView::pool_selected(std::int64_t index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= pool_.size())
        return;
    pool_index_ = static_cast<unsigned>(index);
    const auto &character = pool_[pool_index_];
    required_node<RichTextLabel>(*this, "PoolModal/Text").set_text(sheet_text(character));
    required_node<TextureRect>(*this, "PoolModal/Portrait")
    .set_texture(portrait_texture(character.appearance(), character.creation_data()));
    for (unsigned i = 1; i < 3; ++i)
    {
        const auto source = art_->icon(character.appearance(),
                                         i == 2 ? por::IconPose::action : por::IconPose::ready);
        required_node<TextureRect>(*this, i == 0   ? "PoolModal/Portrait"
                              : i == 1 ? "PoolModal/Ready"
                              : "PoolModal/Action")
        .set_texture(presentation::image_texture(source));
    }
    const bool added =
        std::find(pool_added_.begin(), pool_added_.end(), pool_index_) != pool_added_.end();
    const bool full =
        std::none_of(campaign_->state().slots.begin(), campaign_->state().slots.begin() + 6,
                     [](auto id)
    {
        return !id;
    });
    required_node<Button>(*this, "PoolModal/Add").set_disabled(added || full);
    const auto &c = character.sheet().character_class;
    required_node<Label>(*this, "PoolModal/Status")
    .set_text(i18n::text(
                   added  ? N_("Already added. Use Rejoin party for a reserved member.")
                   : full ? N_("All six PC positions are occupied.")
                   : (c == "Fighter" || c == "Cleric" || c == "Wizard")
                   ? N_("Starts with 250 gp. Preview portraits and both combat poses before adding.")
                   : N_("Starts with 250 gp. This class can explore and equip gear; its combat features are not implemented yet.")));
}

void CharacterCreationView::pool_add()
{
    try
    {
        if (std::find(pool_added_.begin(), pool_added_.end(), pool_index_) != pool_added_.end())
            return;
        (void)opengold::add_pool_member(*campaign_, pool_.at(pool_index_), pool_index_);
        pool_added_.push_back(pool_index_);
        roster_index_ = campaign_->state().roster.size() - 1;
        refresh_party();
        pool_selected(pool_index_);
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "PoolModal/Status").set_text(i18n::text(e.what()));
    }
}

void CharacterCreationView::close_pool()
{
    required_node<Window>(*this, "PoolModal").hide();
}

void CharacterCreationView::town_member_selected(std::int64_t slot)
{
    if (slot < 0 || slot >= 8 || !campaign_->state().slots[slot])
        return;
    const auto &m = campaign_->member(campaign_->state().slots[slot]);
    town_sheet_member_ = m.id;
    required_node<RichTextLabel>(*this, "TownSheet/Text").set_text(sheet_text(m.character, &m));
    required_node<Button>(*this, "TownSheet/MakeLeader").set_disabled(m.id == campaign_->leader());
    required_node<Window>(*this, "TownSheet").popup_centered();
}

void CharacterCreationView::close_town_sheet()
{
    required_node<Window>(*this, "TownSheet").hide();
}
