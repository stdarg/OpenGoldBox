#ifndef OPENGOLDBOX_UI_PALETTE_H
#define OPENGOLDBOX_UI_PALETTE_H

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/color.hpp>
#include <stdexcept>

namespace presentation
{
// For BBCode builders that do not own a Control. ResourceLoader reuses the
// project's cached theme; the resource remains owned by Godot.
inline godot::Color palette_color(const char *name)
{
    const auto theme = godot::ResourceLoader::get_singleton()->load(
                           "res://themes/opengold.tres");
    if (theme.is_null())
        throw std::runtime_error("OpenGoldBox theme is unavailable");
    return theme->call("get_color", name, "OpenGoldPalette");
}

inline int palette_metric(const char *name)
{
    const auto theme = godot::ResourceLoader::get_singleton()->load(
                           "res://themes/opengold.tres");
    if (theme.is_null())
        throw std::runtime_error("OpenGoldBox theme is unavailable");
    return theme->call("get_constant", name, "OpenGoldMetrics");
}
} // namespace presentation

#endif
