#ifndef OPENGOLDBOX_GODOT_KEYS_H
#define OPENGOLDBOX_GODOT_KEYS_H

#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>

namespace presentation
{
// True for the first press of `keycode`, not a held key's repeats and not its
// release: what a dialog's Escape handler answers to.
[[nodiscard]] inline bool key_pressed(const godot::Ref<godot::InputEvent> &event,
                                      godot::Key keycode)
{
    const godot::Ref<godot::InputEventKey> key = event;
    return key.is_valid() && key->is_pressed() && !key->is_echo() && key->get_keycode() == keycode;
}

[[nodiscard]] inline bool escape_pressed(const godot::Ref<godot::InputEvent> &event)
{
    return key_pressed(event, godot::KEY_ESCAPE);
}
} // namespace presentation
#endif
