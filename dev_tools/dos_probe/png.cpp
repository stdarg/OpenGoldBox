#include "png.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace opengold::probe
{
namespace
{
std::uint32_t crc32(const std::vector<std::uint8_t> &bytes)
{
    static const auto table = []
    {
        std::array<std::uint32_t, 256> entries{};
        for (std::uint32_t n = 0; n < 256; ++n)
        {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k)
            {
                c = (c & 1U) ? 0xEDB88320U ^ (c >> 1U) : c >> 1U;
            }
            entries[n] = c;
        }
        return entries;
    }();
    std::uint32_t c = 0xFFFFFFFFU;
    for (const auto byte : bytes)
    {
        c = table[(c ^ byte) & 0xFFU] ^ (c >> 8U);
    }
    return c ^ 0xFFFFFFFFU;
}

void put32(std::vector<std::uint8_t> &out, std::uint32_t value)
{
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        out.push_back(static_cast<std::uint8_t>(value >> static_cast<unsigned>(shift)));
    }
}

void chunk(std::vector<std::uint8_t> &out, std::string_view type,
           const std::vector<std::uint8_t> &data)
{
    put32(out, static_cast<std::uint32_t>(data.size()));
    std::vector<std::uint8_t> checked(type.begin(), type.end());
    checked.insert(checked.end(), data.begin(), data.end());
    out.insert(out.end(), checked.begin(), checked.end());
    put32(out, crc32(checked));
}
} // namespace

void write_png(const std::filesystem::path &path, unsigned width, unsigned height,
               const std::vector<std::uint8_t> &rgb)
{
    if (rgb.size() != std::size_t{width} * height * 3)
    {
        throw std::invalid_argument("PNG pixel count does not match its size");
    }
    std::vector<std::uint8_t> raw;
    for (unsigned y = 0; y < height; ++y)
    {
        raw.push_back(0);
        const auto row = rgb.begin() + static_cast<std::ptrdiff_t>(std::size_t{y} * width * 3);
        raw.insert(raw.end(), row, row + static_cast<std::ptrdiff_t>(width * 3));
    }
    // zlib with stored (uncompressed) blocks.
    std::vector<std::uint8_t> zlib{0x78, 0x01};
    for (std::size_t at = 0; at < raw.size(); at += 65535)
    {
        const auto length =
            static_cast<std::uint16_t>(std::min<std::size_t>(65535, raw.size() - at));
        zlib.push_back(at + length >= raw.size() ? 1 : 0);
        zlib.push_back(static_cast<std::uint8_t>(length));
        zlib.push_back(static_cast<std::uint8_t>(length >> 8U));
        zlib.push_back(static_cast<std::uint8_t>(~length));
        zlib.push_back(static_cast<std::uint8_t>(~length >> 8U));
        zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(at),
                    raw.begin() + static_cast<std::ptrdiff_t>(at + length));
    }
    std::uint32_t a = 1, b = 0;
    for (const auto byte : raw)
    {
        a = (a + byte) % 65521U;
        b = (b + a) % 65521U;
    }
    put32(zlib, (b << 16U) | a);

    std::vector<std::uint8_t> png{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<std::uint8_t> header;
    put32(header, width);
    put32(header, height);
    header.insert(header.end(), {8, 2, 0, 0, 0});
    chunk(png, "IHDR", header);
    chunk(png, "IDAT", zlib);
    chunk(png, "IEND", {});
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(png.data()),
               static_cast<std::streamsize>(png.size()));
    if (!file)
    {
        throw std::runtime_error("Cannot write " + path.string());
    }
}
} // namespace opengold::probe
