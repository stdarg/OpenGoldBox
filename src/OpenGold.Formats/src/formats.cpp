#include "opengold/formats.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace opengold
{
namespace
{

std::uint16_t read_u16(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(bytes[offset + 1] << 8);
}

std::uint32_t read_u32(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

struct RecordResult
{
    FormatResult status{FormatResult::invalid_data};
    std::vector<std::uint8_t> bytes;
};

RecordResult extract_record(std::span<const std::uint8_t> dax, std::uint8_t wanted_id)
{
    if (dax.size() < 2)
        return {};
    const std::size_t toc_size = read_u16(dax, 0);
    if (toc_size % 9 != 0 || toc_size > dax.size() - 2)
        return {};

    for (std::size_t entry = 0; entry < toc_size / 9; ++entry)
    {
        const std::size_t header = 2 + entry * 9;
        if (dax[header] != wanted_id)
            continue;
        const std::size_t offset = read_u32(dax, header + 1);
        const std::size_t raw_size = read_u16(dax, header + 5);
        const std::size_t compressed_size = read_u16(dax, header + 7);
        if (offset > dax.size() - 2 - toc_size)
            return {};
        const std::size_t data_start = 2 + toc_size + offset;
        if (data_start > dax.size() || compressed_size > dax.size() - data_start)
            return {};

        std::vector<std::uint8_t> output;
        output.reserve(raw_size);
        std::size_t input = data_start;
        const std::size_t input_end = data_start + compressed_size;
        while (input < input_end)
        {
            const auto command = static_cast<std::int8_t>(dax[input++]);
            if (command >= 0)
            {
                const std::size_t count = static_cast<std::size_t>(command) + 1;
                if (count > input_end - input || count > raw_size - output.size())
                    return {};
                output.insert(output.end(), dax.begin() + input, dax.begin() + input + count);
                input += count;
            }
            else
            {
                const std::size_t count = static_cast<std::size_t>(-command);
                if (input >= input_end || count > raw_size - output.size())
                    return {};
                output.insert(output.end(), count, dax[input++]);
            }
        }
        if (output.size() != raw_size)
            return {};
        return {FormatResult::ok, std::move(output)};
    }
    return {FormatResult::not_found, {}};
}

constexpr std::array<std::array<std::uint8_t, 3>, 16> ega_palette{{{0, 0, 0},
        {0, 0, 170},
        {0, 170, 0},
        {0, 170, 170},
        {170, 0, 0},
        {170, 0, 170},
        {170, 85, 0},
        {170, 170, 170},
        {85, 85, 85},
        {85, 85, 255},
        {85, 255, 85},
        {85, 255, 255},
        {255, 85, 85},
        {0, 0, 0},
        {255, 255, 85},
        {255, 255, 255}
    }};

} // namespace

DaxDecodeResult decode_dax_archive(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < 2)
        return {};
    const std::size_t toc_size = read_u16(bytes, 0);
    if (toc_size % 9 != 0 || toc_size > bytes.size() - 2)
        return {};
    std::array<bool, 256> seen{};
    std::vector<DaxRecord> records;
    records.reserve(toc_size / 9);
    for (std::size_t offset = 2; offset < 2 + toc_size; offset += 9)
    {
        const auto id = bytes[offset];
        if (seen[id])
            return {};
        seen[id] = true;
        auto record = extract_record(bytes, id);
        if (record.status != FormatResult::ok)
            return {};
        records.push_back({id, std::move(record.bytes)});
    }
    return {FormatResult::ok, std::move(records)};
}

std::uint32_t formats_checksum(std::span<const std::uint8_t> bytes) noexcept
{
    std::uint32_t checksum = 2166136261u;
    for (const auto byte : bytes)
    {
        checksum ^= byte;
        checksum *= 16777619u;
    }
    return checksum;
}

ImageDecodeResult decode_ega_sprite(std::span<const std::uint8_t> dax, std::uint8_t record_id,
                                    std::uint8_t frame_index)
{
    auto extracted = extract_record(dax, record_id);
    if (extracted.status != FormatResult::ok)
        return {extracted.status, {}};
    const std::span<const std::uint8_t> record{extracted.bytes};
    if (record.empty() || frame_index >= record[0])
        return {FormatResult::not_found, {}};

    std::size_t cursor = 1;
    for (std::uint8_t frame = 0; frame <= frame_index; ++frame)
    {
        if (cursor > record.size() || 21 > record.size() - cursor)
            return {};
        const auto height = read_u16(record, cursor + 4);
        const auto width_bytes = read_u16(record, cursor + 6);
        const std::size_t packed_size = static_cast<std::size_t>(height) * width_bytes * 4;
        const std::size_t pixel_start = cursor + 21;
        if (height == 0 || width_bytes == 0 ||
                width_bytes > std::numeric_limits<std::uint16_t>::max() / 8 ||
                packed_size > record.size() - pixel_start)
            return {};
        if (frame != frame_index)
        {
            cursor = pixel_start + packed_size;
            continue;
        }

        Image image;
        image.width = static_cast<std::uint16_t>(width_bytes * 8);
        image.height = height;
        image.x_offset = static_cast<std::int16_t>(read_u16(record, cursor + 8) * 8);
        image.y_offset = static_cast<std::int16_t>(read_u16(record, cursor + 10) * 8);
        image.rgba.resize(static_cast<std::size_t>(image.width) * image.height * 4);
        for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(image.width) * image.height;
                ++pixel)
        {
            const auto packed = record[pixel_start + pixel / 2];
            const auto index =
                static_cast<std::uint8_t>((pixel & 1) == 0 ? packed >> 4 : packed & 0x0f);
            const std::size_t output = pixel * 4;
            std::copy(ega_palette[index].begin(), ega_palette[index].end(),
                      image.rgba.begin() + output);
            image.rgba[output + 3] = index == 0 ? 0 : 255;
        }
        return {FormatResult::ok, std::move(image)};
    }
    return {FormatResult::not_found, {}};
}

ImageDecodeResult decode_ega_combat_icon(std::span<const std::uint8_t> dax, std::uint8_t record_id,
        std::uint8_t frame_index)
{
    const auto extracted = extract_record(dax, record_id);
    if (extracted.status != FormatResult::ok)
        return {extracted.status, {}};
    const std::span<const std::uint8_t> record{extracted.bytes};
    if (record.size() < 17)
        return {};
    const auto height = read_u16(record, 0), width_bytes = read_u16(record, 2);
    const auto count = record[8];
    const std::size_t frame_size = static_cast<std::size_t>(height) * width_bytes * 4;
    if (!height || height > 200 || !width_bytes || width_bytes > 40 || !count ||
            record.size() != 17 + frame_size * count)
        return {};
    if (frame_index >= count)
        return {FormatResult::not_found, {}};
    Image image;
    image.width = width_bytes * 8;
    image.height = height;
    image.rgba.resize(image.width * image.height * 4);
    const std::size_t start = 17 + frame_index * frame_size;
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(image.width) * height; ++pixel)
    {
        const auto packed = record[start + pixel / 2];
        const unsigned index = pixel % 2 ? packed & 15 : packed >> 4;
        std::copy(ega_palette[index].begin(), ega_palette[index].end(),
                  image.rgba.begin() + pixel * 4);
        if (index == 8)
            image.rgba[pixel * 4] = image.rgba[pixel * 4 + 1] = image.rgba[pixel * 4 + 2] = 0;
        if (index == 13)
        {
            image.rgba[pixel * 4] = 255;
            image.rgba[pixel * 4 + 1] = 85;
            image.rgba[pixel * 4 + 2] = 255;
        }
        image.rgba[pixel * 4 + 3] = index == 0 ? 0 : 255;
    }
    return {FormatResult::ok, std::move(image)};
}

ImageDecodeResult decode_ega_picture(std::span<const std::uint8_t> record)
{
    if (record.size() < 17)
        return {};
    const auto height = read_u16(record, 0), width_bytes = read_u16(record, 2);
    const unsigned count = record[8];
    if (!height || height > 200 || !width_bytes || width_bytes > 40 || !count ||
            record.size() != 17 + static_cast<std::size_t>(height) * width_bytes * 4 * count)
        return {};
    Image image;
    image.width = width_bytes * 8;
    image.height = height;
    image.rgba.resize(static_cast<std::size_t>(image.width) * height * 4);
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(image.width) * height; ++pixel)
    {
        const auto packed = record[17 + pixel / 2];
        const unsigned color = pixel % 2 ? packed & 15 : packed >> 4;
        std::copy(ega_palette[color].begin(), ega_palette[color].end(),
                  image.rgba.begin() + pixel * 4);
        if (color == 13)
        {
            image.rgba[pixel * 4] = 255;
            image.rgba[pixel * 4 + 1] = 85;
            image.rgba[pixel * 4 + 2] = 255;
        }
        image.rgba[pixel * 4 + 3] = 255;
    }
    return {FormatResult::ok, std::move(image)};
}
} // namespace opengold
