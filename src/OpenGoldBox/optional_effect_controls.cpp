#include "optional_effect_controls.h"
#include "localization.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/window.hpp>

namespace presentation
{
void setup_optional_effect(godot::Node &root, const godot::Callable &use,
                           const godot::Callable &skip, const godot::Callable &input,
                           const godot::Callable &select)
{
    using namespace godot;
    auto *w = attach_modal_dialog(root, "OptionalEffect");
    w->connect("close_requested", skip);
    w->connect("window_input", input);
    auto *caption = dialog_control<Label>(*w, "ResolveLabel");
    caption->set_text(i18n::text(N_("Resolve next")));
    caption->hide();
    auto *options = dialog_control<OptionButton>(*w, "Resolve");
    options->connect("item_selected", select);
    options->hide();
    dialog_control<Label>(*w, "Text");
    auto *no = dialog_control<Button>(*w, "Skip");
    no->set_text(i18n::text(N_("Skip")));
    no->connect("pressed", skip);
    auto *yes = dialog_control<Button>(*w, "Use");
    yes->set_text(i18n::text(N_("Use")));
    yes->connect("pressed", use);
}

void refresh_optional_effect(godot::Node &root,
                             const std::optional<opengold::rules::OptionalEffectChoice> &choice,
                             bool player)
{
    using namespace godot;
    auto *w = &required_node<Window>(root, "OptionalEffect");
    if (!player || !choice)
    {
        if (w->is_visible())
        {
            w->hide();
            if (player)
            {
                required_node<Button>(root, "End").grab_focus();
            }
        }
        return;
    }
    auto *options = &required_node<OptionButton>(*w, "Resolve");
    const int prior = w->is_visible() ? options->get_selected_id() : -1;
    SignalsBlocked options_quiet(*options);
    options->clear();
    int selected = 0;
    for (const auto &option : choice->options)
    {
        options->add_item(i18n::render(option.title), option.id);
        if (static_cast<int>(option.id) == prior)
        {
            selected = options->get_item_count() - 1;
        }
    }
    if (!choice->options.empty())
    {
        options->select(selected);
    }
    options_quiet.unblock();
    const bool multiple = choice->options.size() > 1;
    options->set_visible(multiple);
    required_node<Label>(*w, "ResolveLabel").set_visible(multiple);
    auto &layout = required_node<Node>(*w, "Layout");
    layout.call("play", multiple ? "multiple" : "single");
    layout.call("advance", 0);
    auto *label = &required_node<Label>(*w, "Text");
    const auto *option = choice->options.empty() ? nullptr : &choice->options[selected];
    w->set_title(i18n::render(option ? option->title : choice->title));
    label->set_text(i18n::render(option ? option->description : choice->description));
    required_node<Button>(*w, "Use").set_disabled(option && !option->available);
    if (!w->is_visible())
    {
        w->popup_centered();
        required_node<Button>(*w, "Use").grab_focus();
    }
}
} // namespace presentation
