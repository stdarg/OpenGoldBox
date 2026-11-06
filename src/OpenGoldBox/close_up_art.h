#ifndef OPENGOLDBOX_CLOSE_UP_ART_H
#define OPENGOLDBOX_CLOSE_UP_ART_H

#include "monster_picture_timing.h"
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

// New art for the original's animated close-ups (PIC1.DAX to PIC8.DAX), drawn
// from a written description. Each record has its own folder, such as
// "pic2_dax_4", holding frame_0.png, frame_1.png and so on: one 352 x 422 PNG
// for each frame the original holds on screen, in order. That is the 3D view's
// 5:6 shape, which shows the original's 88 x 88 pictures 1.2 times taller than
// wide, at four times their width. The new frames keep the original's timing. The game reads
// data/art/closeups; the Combat Sprite Catalog's Close-ups tab shares this file,
// so it plays what the game plays.
namespace presentation
{
inline constexpr std::int64_t close_up_art_width = 352;
inline constexpr std::int64_t close_up_art_height = 422;

struct CloseUpArt
{
    std::vector<godot::Ref<godot::ImageTexture>> frames;
    std::vector<std::uint32_t> delays;
};

// A record's new art, or why its folder cannot be used. A record with no folder
// has neither, and keeps the original.
struct CloseUpArtLoad
{
    std::optional<CloseUpArt> art;
    godot::String problem;
};

[[nodiscard]] inline godot::String close_up_folder_name(unsigned bank, unsigned record)
{
    return godot::String("pic") + godot::String::num_uint64(bank) + "_dax_" +
           godot::String::num_uint64(record);
}

[[nodiscard]] inline godot::String close_up_frame_file(std::size_t frame)
{
    return godot::String("frame_") + godot::String::num_uint64(frame) + ".png";
}

// `delays` are the original record's, every frame included.
[[nodiscard]] inline CloseUpArtLoad load_close_up_art(const godot::String &folder,
                                                      std::span<const std::uint32_t> delays)
{
    if (!godot::DirAccess::dir_exists_absolute(folder))
    {
        return {};
    }
    const auto held = held_frames(delays);
    if (godot::FileAccess::file_exists(folder.path_join(close_up_frame_file(held.size()))))
    {
        return {std::nullopt, godot::String("It has more frames than the original's ") +
                                  godot::String::num_uint64(held.size())};
    }
    CloseUpArt art;
    for (std::size_t n = 0; n < held.size(); ++n)
    {
        const auto file = close_up_frame_file(n);
        const auto path = folder.path_join(file);
        if (!godot::FileAccess::file_exists(path))
        {
            return {std::nullopt, file + godot::String(" is missing")};
        }
        const auto image = godot::Image::load_from_file(path);
        if (image.is_null())
        {
            return {std::nullopt, file + godot::String(" cannot be read")};
        }
        if (image->get_width() != close_up_art_width || image->get_height() != close_up_art_height)
        {
            return {std::nullopt, file + godot::String(" is not 352 x 422")};
        }
        art.frames.push_back(godot::ImageTexture::create_from_image(image));
        art.delays.push_back(delays[held[n]]);
    }
    return {std::move(art), {}};
}
} // namespace presentation

#endif
