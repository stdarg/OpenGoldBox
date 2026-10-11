#ifndef OPENGOLD_FORMATS_H
#define OPENGOLD_FORMATS_H

#include <cstdint>
#include <span>
#include <vector>

namespace opengold
{

enum class FormatResult
{
    ok = 0,
    invalid_data,
    not_found
};

struct DaxRecord
{
    std::uint8_t id{};
    std::vector<std::uint8_t> bytes;
};

struct DaxDecodeResult
{
    FormatResult status{FormatResult::invalid_data};
    std::vector<DaxRecord> records;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return status == FormatResult::ok;
    }
};

// Validates the entire archive. Duplicate IDs and malformed records fail atomically.
[[nodiscard]] DaxDecodeResult decode_dax_archive(std::span<const std::uint8_t> bytes);

struct Image
{
    std::uint16_t width{};
    std::uint16_t height{};
    std::int16_t x_offset{};
    std::int16_t y_offset{};
    std::vector<std::uint8_t> rgba;
};

// An image's pixels are width x height RGBA quadruplets, row by row. Image is a
// plain record the decoders fill; code that accepts an image from elsewhere
// checks this before indexing it (Effective C++ Item 22).
[[nodiscard]] inline bool consistent(const Image &image) noexcept
{
    return image.rgba.size() == std::size_t{image.width} * image.height * 4;
}

struct ImageDecodeResult
{
    FormatResult status{FormatResult::invalid_data};
    Image image;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return status == FormatResult::ok;
    }
};

[[nodiscard]] std::uint32_t formats_checksum(std::span<const std::uint8_t> bytes) noexcept;
[[nodiscard]] ImageDecodeResult decode_ega_sprite(std::span<const std::uint8_t> dax,
        std::uint8_t record_id,
        std::uint8_t frame_index = 0);
[[nodiscard]] ImageDecodeResult decode_ega_combat_icon(std::span<const std::uint8_t> dax,
        std::uint8_t record_id,
        std::uint8_t frame_index = 0);
// A decompressed 17-byte-header HEAD/BODY/PIC record, using normal EGA colors.
[[nodiscard]] ImageDecodeResult decode_ega_picture(std::span<const std::uint8_t> record);

struct AnimationFrame
{
    std::uint32_t delay{}; // Original timer ticks this frame stays on screen.
    Image image;
};

struct AnimationDecodeResult
{
    FormatResult status{FormatResult::invalid_data};
    std::vector<AnimationFrame> frames;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return status == FormatResult::ok;
    }
};

// A decompressed PIC record: a frame count, then per frame a 32-bit delay and a
// picture. Frames after the first store only the XOR difference from the first.
[[nodiscard]] AnimationDecodeResult decode_ega_animation(std::span<const std::uint8_t> record);

} // namespace opengold

#endif
