#include "modify_character_dialog.h"
#include "character_text.h"
#include "godot_nodes.h"
#include "godot_text.h"
#include "guarded_handlers.h"
#include "localization.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

using namespace godot;
using namespace opengold;
using presentation::gs;
using presentation::required_node;

namespace
{
constexpr int row_count = 7;
constexpr int hit_point_row = 6;
constexpr const char *spell_list = "Margin/Rows/Spells/List";

String row_path(const char *part, int row)
{
    return gs(std::string("Margin/Rows/Grid/") + part + std::to_string(row));
}

String row_name(int row)
{
    return row == hit_point_row ? i18n::text(N_("Hit Points"))
                                : i18n::text(i18n::ability_names.at(static_cast<std::size_t>(row)));
}

String row_value(const rules::CharacterSheet &sheet, int row)
{
    return row == hit_point_row ? String::num_int64(sheet.hit_points)
                                : i18n::original_score(sheet, rules::all_abilities.at(row));
}

// The spell list's nodes, by the group's and option's places: their ids
// hold characters a node's name may not.
String heading_name(std::size_t group)
{
    return gs("Heading" + std::to_string(group));
}

String option_name(std::size_t group, std::size_t option)
{
    return gs("Option" + std::to_string(group) + "_" + std::to_string(option));
}

// How many of a group's options the draft has chosen.
std::size_t chosen_count(const rules::CharacterDraft &draft, const std::string &group)
{
    const auto found = draft.training.find(group);
    return found == draft.training.end() ? 0 : found->second.size();
}
} // namespace

void ModifyCharacterDialog::_ready()
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  connect_controls();
                              });
}

void ModifyCharacterDialog::connect_controls()
{
    for (int row = 0; row < row_count; ++row)
    {
        required_node<Button>(*this, row_path("Row", row))
            .connect("pressed",
                     presentation::guarded(this, &ModifyCharacterDialog::select_row).bind(row));
        required_node<Button>(*this, row_path("Lower", row))
            .connect("pressed",
                     presentation::guarded(this, &ModifyCharacterDialog::step).bind(row, false));
        required_node<Button>(*this, row_path("Raise", row))
            .connect("pressed",
                     presentation::guarded(this, &ModifyCharacterDialog::step).bind(row, true));
    }
    required_node<LineEdit>(*this, "Margin/Rows/NameRow/Name")
        .connect("text_changed", presentation::guarded(this, &ModifyCharacterDialog::name_changed));
    required_node<Button>(*this, "Margin/Rows/Buttons/Keep")
        .connect("pressed", presentation::guarded(this, &ModifyCharacterDialog::keep));
    required_node<Button>(*this, "Margin/Rows/Buttons/Exit")
        .connect("pressed", presentation::guarded(this, &ModifyCharacterDialog::close));
    connect("close_requested", presentation::guarded(this, &ModifyCharacterDialog::close));
    connect("window_input", presentation::guarded(this, &ModifyCharacterDialog::window_input));
}

void ModifyCharacterDialog::report_failure(const std::exception &failure)
{
    required_node<Label>(*this, "Margin/Rows/Status").set_text(i18n::text(failure.what()));
}

void ModifyCharacterDialog::open(Request request)
{
    if (!request.rules || !request.keep)
    {
        throw std::logic_error("Modify Character needs its rules and what follows Keep");
    }
    request_ = std::move(request);
    row_ = 0;
    auto &name = required_node<LineEdit>(*this, "Margin/Rows/NameRow/Name");
    name.set_text(gs(request_.draft.name));
    required_node<Control>(*this, "Margin/Rows/NameRow").set_visible(request_.renames);
    required_node<Label>(*this, "Margin/Rows/Status").set_text(String());
    fill_spells();
    refresh();
    const bool spells = required_node<Control>(*this, "Margin/Rows/Spells").is_visible();
    // The spell book's lists take the room left; without them the status line
    // sits above the buttons.
    required_node<Label>(*this, "Margin/Rows/Status")
        .set_v_size_flags(spells ? Control::SIZE_FILL
                                 : Control::SIZE_EXPAND | Control::SIZE_SHRINK_END);
    // The spell book's lists scroll; the window keeps within the screen's
    // smallest window (1120 x 800).
    const auto *host = Object::cast_to<Control>(get_parent());
    // Room for the title bar too.
    const int room = host ? static_cast<int>(host->get_size().y) - 80 : 720;
    set_size(Vector2i(760, std::min(spells ? 900 : 610, room)));
    popup_centered();
    required_node<Button>(*this, row_path("Raise", row_)).grab_focus();
}

void ModifyCharacterDialog::fill_spells()
{
    auto &list = required_node<VBoxContainer>(*this, spell_list);
    while (list.get_child_count() > 0)
    {
        auto *child = list.get_child(0);
        list.remove_child(child);
        child->queue_free();
    }
    const auto groups = request_.rules->training_options(request_.draft);
    required_node<Control>(*this, "Margin/Rows/Spells").set_visible(!groups.empty());
    for (std::size_t g = 0; g < groups.size(); ++g)
    {
        const auto &group = groups[g];
        auto heading = presentation::make_node<Label>();
        heading->set_name(heading_name(g));
        heading->set_theme_type_variation("LabelText16");
        presentation::attach_child(list, std::move(heading));
        for (std::size_t o = 0; o < group.options.size(); ++o)
        {
            const auto &option = group.options[o];
            auto box = presentation::make_node<CheckBox>();
            box->set_name(option_name(g, o));
            box->set_text(i18n::text(option.label));
            box->connect("toggled",
                         presentation::guarded(this, &ModifyCharacterDialog::spell_toggled)
                             .bind(gs(group.id), gs(option.id)));
            presentation::attach_child(list, std::move(box));
        }
    }
}

void ModifyCharacterDialog::refresh()
{
    const auto &rules = *request_.rules;
    const auto &draft = request_.draft;
    const auto sheet = rules.evaluate(draft, rules::NameRequirement::optional);
    required_node<Label>(*this, "Margin/Rows/Title")
        .set_text(i18n::format("Modify {name}", {{"name", gs(sheet.name)}}));
    for (int row = 0; row < row_count; ++row)
    {
        const auto modify_row = static_cast<rules::ModifyRow>(row);
        const auto moves = [&](rules::ModifyStep direction)
        {
            return rules.modified(draft, modify_row, direction).rules_data != draft.rules_data;
        };
        required_node<Button>(*this, row_path("Row", row))
            .set_text((row == row_ ? String("> ") : String()) + row_name(row));
        required_node<Label>(*this, row_path("Value", row)).set_text(row_value(sheet, row));
        required_node<Button>(*this, row_path("Lower", row))
            .set_disabled(!moves(rules::ModifyStep::lower));
        required_node<Button>(*this, row_path("Raise", row))
            .set_disabled(!moves(rules::ModifyStep::raise));
    }
    auto &list = required_node<VBoxContainer>(*this, spell_list);
    const auto groups = rules.training_options(draft);
    for (std::size_t g = 0; g < groups.size(); ++g)
    {
        const auto &group = groups[g];
        const auto chosen = chosen_count(draft, group.id);
        auto &heading = required_node<Label>(list, heading_name(g));
        heading.set_text(i18n::text(group.label) + " (" + String::num_int64(chosen) + " / " +
                         String::num_int64(group.count) + ")");
        const auto found = draft.training.find(group.id);
        for (std::size_t o = 0; o < group.options.size(); ++o)
        {
            const auto &option = group.options[o];
            auto &box = required_node<CheckBox>(list, option_name(g, o));
            const bool picked = found != draft.training.end() &&
                                std::ranges::find(found->second, option.id) != found->second.end();
            box.set_pressed_no_signal(picked);
            box.set_disabled(!picked && chosen >= group.count);
        }
    }
    const auto refusal = keep_refusal();
    required_node<Button>(*this, "Margin/Rows/Buttons/Keep").set_disabled(!refusal.is_empty());
    required_node<Label>(*this, "Margin/Rows/Status").set_text(refusal);
}

String ModifyCharacterDialog::keep_refusal() const
{
    const auto &draft = request_.draft;
    if (request_.renames)
    {
        if (draft.name.empty())
        {
            return i18n::text(N_("Enter a name for your character."));
        }
        if (std::ranges::find(request_.taken_names, draft.name) != request_.taken_names.end())
        {
            return i18n::format("There is already a {name} in the party.",
                                {{"name", gs(draft.name)}});
        }
    }
    for (const auto &group : request_.rules->training_options(draft))
    {
        if (chosen_count(draft, group.id) < group.count)
        {
            return i18n::text(N_("Choose the spell book's spells to keep the changes."));
        }
    }
    return {};
}

void ModifyCharacterDialog::select_row(std::int64_t row)
{
    row_ = static_cast<int>(std::clamp<std::int64_t>(row, 0, row_count - 1));
    refresh();
}

void ModifyCharacterDialog::step(std::int64_t row, bool raise)
{
    row_ = static_cast<int>(std::clamp<std::int64_t>(row, 0, row_count - 1));
    request_.draft =
        request_.rules->modified(request_.draft, static_cast<rules::ModifyRow>(row_),
                                 raise ? rules::ModifyStep::raise : rules::ModifyStep::lower);
    refresh();
}

void ModifyCharacterDialog::name_changed(const String &name)
{
    request_.draft.name = name.strip_edges().utf8().get_data();
    refresh();
}

void ModifyCharacterDialog::spell_toggled(bool selected, const String &group, const String &option)
{
    auto &chosen = request_.draft.training[group.utf8().get_data()];
    const std::string id = option.utf8().get_data();
    std::erase(chosen, id);
    if (selected)
    {
        chosen.push_back(id);
    }
    refresh();
}

void ModifyCharacterDialog::keep()
{
    if (!keep_refusal().is_empty())
    {
        return;
    }
    // Keep may refuse (a character that gained experience meanwhile); the
    // dialog stays open with the reason.
    request_.keep(request_.draft);
    close();
}

void ModifyCharacterDialog::close()
{
    hide();
}

void ModifyCharacterDialog::window_input(const Ref<InputEvent> &event)
{
    const Ref<InputEventKey> key = event;
    if (key.is_null() || !key->is_pressed())
    {
        return;
    }
    const bool naming = required_node<LineEdit>(*this, "Margin/Rows/NameRow/Name").has_focus();
    switch (key->get_keycode())
    {
    case Key::KEY_UP:
    case Key::KEY_DOWN:
        select_row((row_ + (key->get_keycode() == Key::KEY_UP ? row_count - 1 : 1)) % row_count);
        // Off the name, so Left and Right change the row again.
        required_node<Button>(*this, row_path("Row", row_)).grab_focus();
        break;
    case Key::KEY_LEFT:
    case Key::KEY_RIGHT:
        if (naming)
        {
            return;
        }
        step(row_, key->get_keycode() == Key::KEY_RIGHT);
        break;
    case Key::KEY_ENTER:
    case Key::KEY_KP_ENTER:
        if (key->is_echo())
        {
            return;
        }
        keep();
        break;
    case Key::KEY_ESCAPE:
        if (key->is_echo())
        {
            return;
        }
        close();
        break;
    default:
        return;
    }
    set_input_as_handled();
}
