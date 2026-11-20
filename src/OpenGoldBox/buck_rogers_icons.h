#ifndef OPENGOLDBOX_BUCK_ROGERS_ICONS_H
#define OPENGOLDBOX_BUCK_ROGERS_ICONS_H
#include <godot_cpp/classes/image_texture.hpp>

// The Buck Rogers games' 36 body icons (CHARS.DAX), each in its ready and
// action pose: their characters have no portraits, so the icon is the
// character's picture wherever a portrait would show, as in the original
// (docs/CHARACTER-CREATION.md, "The Buck Rogers games").
namespace presentation
{
inline constexpr unsigned buck_rogers_body_icon_count = 36;

struct BodyIcon
{
    godot::Ref<godot::ImageTexture> ready, action;
};

// The chosen game's icon, read from its folder's CHARS.DAX once; empty
// textures when the file or the icon is missing.
[[nodiscard]] BodyIcon buck_rogers_body_icon(unsigned number);
} // namespace presentation
#endif
