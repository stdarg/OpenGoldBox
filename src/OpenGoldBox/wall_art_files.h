#ifndef OPENGOLDBOX_WALL_ART_FILES_H
#define OPENGOLDBOX_WALL_ART_FILES_H

#include "godot_path.h"
#include "opengold/wall_art_replacements.h"
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <algorithm>

// The core reads data/art/walls but has no PNG codec; Godot decodes for it.
// The Tile Art Viewer shares this file so it shows what the game shows.
inline std::optional<opengold::Image> decode_wall_png(std::span<const std::uint8_t> bytes)
{
    godot::PackedByteArray buffer;
    buffer.resize(static_cast<std::int64_t>(bytes.size()));
    std::copy(bytes.begin(), bytes.end(), buffer.ptrw());
    godot::Ref<godot::Image> image;
    image.instantiate();
    if (image->load_png_from_buffer(buffer) != godot::OK)
    {
        return std::nullopt;
    }
    image->convert(godot::Image::FORMAT_RGBA8);
    const auto pixels = image->get_data();
    opengold::Image result;
    result.width = static_cast<std::uint16_t>(image->get_width());
    result.height = static_cast<std::uint16_t>(image->get_height());
    result.rgba.assign(pixels.ptr(), pixels.ptr() + pixels.size());
    return result;
}

inline opengold::por::WallArtReplacements load_wall_art_files(const godot::String &folder)
{
    return opengold::por::load_wall_art_replacements(presentation::path_from_godot(folder),
                                                     decode_wall_png);
}

#endif
