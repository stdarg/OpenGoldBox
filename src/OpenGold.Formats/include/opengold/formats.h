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

} // namespace opengold

#endif
