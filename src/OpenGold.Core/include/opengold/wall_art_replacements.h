#ifndef OPENGOLD_WALL_ART_REPLACEMENTS_H
#define OPENGOLD_WALL_ART_REPLACEMENTS_H

#include "opengold/png_decoder.h"
#include "opengold/wall_art.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace opengold::por
{
// New perspective pieces for original wall appearances, keyed by
// wall_appearance_key. Every piece is the original's size times `scale`.
struct WallArtReplacements
{
    static constexpr unsigned scale = 4;
    std::map<std::string, std::array<Image, 10>> sets;
};

// "WALLDEF1.DAX:1:4": appearance 4 (one-based) of WALLDEF1.DAX record 1.
[[nodiscard]] std::string wall_appearance_key(std::string_view archive, unsigned record,
                                              unsigned appearance);

// The file each perspective piece is stored in, such as "far_front.png".
[[nodiscard]] std::string_view wall_view_file(WallView view);

// Reads data/art/walls: one folder per appearance, named after its key with
// "." and ":" as "_" (WALLDEF1_DAX_1_4), holding the ten wall_view_file PNGs.
// A missing folder means no new art. Throws on a set the game cannot use.
[[nodiscard]] WallArtReplacements load_wall_art_replacements(const std::filesystem::path &folder,
                                                             const PngDecoder &decode);

// The original art with each appearance that has new art replaced. keys[i]
// names appearance i + 1. With any new art loaded, every remaining original
// piece is enlarged to the new art's scale so one view can show both.
[[nodiscard]] WallArtSet with_replacements(WallArtSet original, std::span<const std::string> keys,
                                           const WallArtReplacements &replacements);

// Keys for the appearances of one decoded WALLDEF record, in order.
[[nodiscard]] std::vector<std::string> wall_appearance_keys(std::string_view archive,
                                                            unsigned record, std::size_t count);
} // namespace opengold::por
#endif
