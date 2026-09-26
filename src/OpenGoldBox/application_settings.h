#ifndef OPENGOLDBOX_APPLICATION_SETTINGS_H
#define OPENGOLDBOX_APPLICATION_SETTINGS_H
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

namespace settings
{
bool flag(const char *name);
godot::String path();
godot::String saved_game_path();
godot::String saved_language();
godot::String game_path();
godot::String language();
int combat_zoom_percent();
bool valid_language(const godot::String &locale);
bool save_game_path(const godot::String &directory);
bool save_language(const godot::String &locale);
struct Validation
{
    bool usable{};
    godot::PackedStringArray missing;
    godot::PackedStringArray different;
    godot::String error;
};
Validation validate_game_path(const godot::String &directory);
} // namespace settings
#endif
