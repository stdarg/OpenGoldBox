#include "character_creation_view.h"
#include "rolf_tour_view.h"
#include "opengold/character_pool.h"
#include "opengold/srd5.h"
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
        auto *n = get_node<Control>(path);
        n->set_position(r.position);
        n->set_size(r.size);
    };
    for (const char *name : {"PoolModal", "TownSheet"})
        get_node<Window>(name)->set_size(Vector2i(w, h));
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
        auto *list = get_node<ItemList>("PoolModal/List");
        list->clear();
        for (const auto &character : pool_)
            list->add_item(gs(character.sheet().character_class + " / " + character.sheet().name));
        list->select(pool_index_);
        pool_selected(pool_index_);
        get_node<Window>("PoolModal")->popup_centered();
    }
    catch (const std::exception &e)
    {
        get_node<Label>("PartyPanel/Status")->set_text(gs(e.what()));
    }
}

void CharacterCreationView::pool_selected(std::int64_t index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= pool_.size())
        return;
    pool_index_ = static_cast<unsigned>(index);
    const auto &character = pool_[pool_index_];
    get_node<RichTextLabel>("PoolModal/Text")->set_text(sheet_text(character));
    get_node<TextureRect>("PoolModal/Portrait")
        ->set_texture(portrait_texture(character.appearance(), character.creation_data()));
    for (unsigned i = 1; i < 3; ++i)
    {
        const auto source = art_->icon(character.appearance(), i == 2);
        PackedByteArray pixels;
        pixels.resize(source.rgba.size());
        std::copy(source.rgba.begin(), source.rgba.end(), pixels.ptrw());
        get_node<TextureRect>(i == 0   ? "PoolModal/Portrait"
                              : i == 1 ? "PoolModal/Ready"
                                       : "PoolModal/Action")
            ->set_texture(ImageTexture::create_from_image(godot::Image::create_from_data(
                source.width, source.height, false, godot::Image::FORMAT_RGBA8, pixels)));
    }
    const bool added =
        std::find(pool_added_.begin(), pool_added_.end(), pool_index_) != pool_added_.end();
    const bool full =
        std::none_of(campaign_->state().slots.begin(), campaign_->state().slots.begin() + 6,
                     [](auto id)
                     {
                         return !id;
                     });
    get_node<Button>("PoolModal/Add")->set_disabled(added || full);
    const auto &c = character.sheet().character_class;
    get_node<Label>("PoolModal/Status")
        ->set_text(
            added  ? "Already added. Use Rejoin party for a reserved member."
            : full ? "All six PC positions are occupied."
            : (c == "Fighter" || c == "Cleric" || c == "Wizard")
                ? "Starts with 250 gp. Preview portraits and both combat poses before adding."
                : "Starts with 250 gp. This class can explore and equip gear; its combat features are not implemented yet.");
}

void CharacterCreationView::pool_add()
{
    try
    {
        if (std::find(pool_added_.begin(), pool_added_.end(), pool_index_) != pool_added_.end())
            return;
        const auto id = campaign_->add_pc(pool_.at(pool_index_));
        campaign_->set_wealth(id, {0, 0, 0, 250, 0, 0, 0});
        auto state = campaign_->checkpoint();
        state.roster.back().creation_source = "pool:v1:" + std::to_string(pool_index_);
        campaign_->restore(std::move(state));
        pool_added_.push_back(pool_index_);
        roster_index_ = campaign_->state().roster.size() - 1;
        refresh_party();
        pool_selected(pool_index_);
    }
    catch (const std::exception &e)
    {
        get_node<Label>("PoolModal/Status")->set_text(gs(e.what()));
    }
}

void CharacterCreationView::close_pool()
{
    get_node<Window>("PoolModal")->hide();
}

void CharacterCreationView::town_member_selected(std::int64_t slot)
{
    if (slot < 0 || slot >= 8 || !campaign_->state().slots[slot])
        return;
    const auto &m = campaign_->member(campaign_->state().slots[slot]);
    get_node<RichTextLabel>("TownSheet/Text")->set_text(sheet_text(m.character, &m));
    get_node<Window>("TownSheet")->popup_centered();
}

void CharacterCreationView::close_town_sheet()
{
    get_node<Window>("TownSheet")->hide();
}
