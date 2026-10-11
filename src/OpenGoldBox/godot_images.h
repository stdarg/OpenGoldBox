#ifndef OPENGOLDBOX_GODOT_IMAGES_H
#define OPENGOLDBOX_GODOT_IMAGES_H

#include "opengold/character_art.h"
#include "opengold/formats.h"
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace presentation
{
// Keep the engine's RGBA image representation independent of Godot resources.
[[nodiscard]] inline godot::Ref<godot::Image> rgba_image(const opengold::Image &source)
{
    if (!opengold::consistent(source))
        throw std::runtime_error("Image pixels do not match its size");
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

// Newer race heads are packaged PNGs, not DAX records. Art loaded from the game
// files needs them before it can compose those characters' portraits.
inline void load_additional_portrait_heads(opengold::por::CharacterArt &art)
{
    for (const auto &head : opengold::por::additional_portrait_heads())
    {
        const auto path =
            godot::String::utf8(("res://bin/portraits/" + std::string(head.filename)).c_str());
        godot::Ref<godot::Texture2D> texture = godot::ResourceLoader::get_singleton()->load(path);
        if (texture.is_null())
            throw std::runtime_error("Missing portrait: " + std::string(head.filename) +
                                     ". Run build-opengoldbox.cmd and opengoldbox.exe.");
        auto source = texture->get_image();
        if (source.is_null() || (source->is_compressed() && source->decompress() != godot::OK))
            throw std::runtime_error("Cannot decode portrait: " + std::string(head.filename));
        source->convert(godot::Image::FORMAT_RGBA8);
        const auto pixels = source->get_data();
        opengold::Image decoded;
        decoded.width = source->get_width();
        decoded.height = source->get_height();
        decoded.rgba.assign(pixels.ptr(), pixels.ptr() + pixels.size());
        art.add_portrait_head(head.id, opengold::por::prepare_portrait_head(decoded, head.id));
    }
}
} // namespace presentation
#endif
