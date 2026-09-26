#ifndef OPENGOLDBOX_GODOT_IMAGES_H
#define OPENGOLDBOX_GODOT_IMAGES_H

#include "opengold/formats.h"
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <algorithm>

namespace presentation
{
// Keep the engine's RGBA image representation independent of Godot resources.
[[nodiscard]] inline godot::Ref<godot::Image> rgba_image(const opengold::Image &source)
{
    godot::PackedByteArray pixels;
    pixels.resize(source.rgba.size());
    std::copy(source.rgba.begin(), source.rgba.end(), pixels.ptrw());
    return godot::Image::create_from_data(source.width, source.height, false,
                                          godot::Image::FORMAT_RGBA8, pixels);
}

[[nodiscard]] inline godot::Ref<godot::ImageTexture> image_texture(const opengold::Image &source)
{
    return godot::ImageTexture::create_from_image(rgba_image(source));
}
} // namespace presentation
#endif
