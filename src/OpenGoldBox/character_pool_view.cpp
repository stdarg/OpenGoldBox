#include "character_creation_view.h"
#include "character_pool_dialog.h"
#include "character_sheet_text.h"
#include "rolf_tour_view.h"
#include "opengold/srd5.h"
#include "godot_nodes.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/window.hpp>
#include <algorithm>
using namespace godot;
using namespace opengold;
using presentation::required_node;

void CharacterCreationView::pool_layout()
{
    const double w = std::min(1000.0, double(get_size().x) - 64), h = get_size().y - 120;
    const auto place = [&](const char *path, Rect2 r)
    {
        auto *n = &required_node<Control>(*this, path);
        n->set_position(r.position);
        n->set_size(r.size);
    };
    required_node<CharacterPoolDialog>(*this, "PoolModal").fit(get_size());
    required_node<Window>(*this, "TownSheet").set_size(Vector2i(w, h));
    place("TownSheet/Background", Rect2(0, 0, w, h));
    place("TownSheet/Text", Rect2(24, 24, w - 48, h - 100));
    place("TownSheet/Close", Rect2(w - 154, h - 56, 130, 36));
    place("TownSheet/MakeLeader", Rect2(w - 314, h - 56, 150, 36));
}

void CharacterCreationView::town_member_selected(std::int64_t slot)
{
    if (slot < 0 || slot >= 8 || !campaign_->state().slots[slot])
        return;
    const auto &m = campaign_->member(campaign_->state().slots[slot]);
    town_sheet_member_ = m.id;
    required_node<RichTextLabel>(*this, "TownSheet/Text")
    .set_text(presentation::sheet_text(*campaign_, m));
    required_node<Button>(*this, "TownSheet/MakeLeader").set_disabled(m.id == campaign_->leader());
    required_node<Window>(*this, "TownSheet").popup_centered();
}

void CharacterCreationView::close_town_sheet()
{
    required_node<Window>(*this, "TownSheet").hide();
}
