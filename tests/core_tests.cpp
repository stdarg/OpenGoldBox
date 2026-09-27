#include "opengold/core.h"
#include "opengold/formats.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
void check(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}

// Literal-only DAX encoding keeps malformed sprite fixtures independent of files.
std::vector<std::uint8_t> archive(const std::vector<std::uint8_t> &record)
{
    std::vector<std::uint8_t> result{9,
                                     0,
                                     2,
                                     0,
                                     0,
                                     0,
                                     0,
                                     static_cast<std::uint8_t>(record.size()),
                                     static_cast<std::uint8_t>(record.size() >> 8),
                                     0,
                                     0};
    for (std::size_t offset = 0; offset < record.size();)
    {
        const auto size = std::min<std::size_t>(128, record.size() - offset);
        result.push_back(static_cast<std::uint8_t>(size - 1));
        result.insert(result.end(), record.begin() + offset, record.begin() + offset + size);
        offset += size;
    }
    const auto stored = result.size() - 11;
    result[9] = static_cast<std::uint8_t>(stored);
    result[10] = static_cast<std::uint8_t>(stored >> 8);
    return result;
}

void sprite_boundaries()
{
    std::vector<std::uint8_t> record(22, 0);
    record[0] = 1;
    record[5] = 1;
    record[7] = 1;
    record.insert(record.end(), {0x12, 0x34, 0x56, 0x70});
    check(opengold::decode_ega_sprite(archive(record), 9).status ==
              opengold::FormatResult::not_found,
          "Absent record is distinct from invalid sprite data");
    check(opengold::decode_ega_sprite(archive(record), 2, 1).status ==
              opengold::FormatResult::not_found,
          "Out-of-range sprite frame is absent");
    const auto first = std::vector<std::uint8_t>(record.begin() + 1, record.end());
    record[0] = 2;
    record.insert(record.end(), first.begin(), first.end());
    record.back() = 0x89;
    const auto second = opengold::decode_ega_sprite(archive(record), 2, 1);
    check(second && second.image.width == 8 && second.image.rgba[31] == 255,
          "Later sprite frames use their own header and pixels");
    for (const auto size : {std::size_t(1), std::size_t(21), record.size() - 1})
    {
        auto truncated = record;
        truncated.resize(size);
        check(!opengold::decode_ega_sprite(archive(truncated), 2, 1),
              "Truncated sprite frames reject");
    }
    record.resize(26);
    record[0] = 1;
    record[5] = 0;
    check(!opengold::decode_ega_sprite(archive(record), 2), "Zero-height sprite rejects");
    record[5] = 1;
    record[7] = 0;
    check(!opengold::decode_ega_sprite(archive(record), 2), "Zero-width sprite rejects");
    // A valid DAX record can describe a pixel width too large for Image::width.
    record.resize(22 + 8192 * 4, 0);
    record[7] = 0;
    record[8] = 32;
    check(!opengold::decode_ega_sprite(archive(record), 2), "Sprite width must not wrap to zero");
    record.resize(22 + 8193 * 4, 0);
    record[7] = 1;
    check(!opengold::decode_ega_sprite(archive(record), 2),
          "Sprite width must not wrap to a smaller image");
    record.resize(22 + 8191 * 4, 0);
    record[7] = 255;
    record[8] = 31;
    const auto widest = opengold::decode_ega_sprite(archive(record), 2);
    check(widest && widest.image.width == 65528 && widest.image.rgba.size() == 65528 * 4,
          "Largest representable byte-width decodes");
}

void decode_tests()
{
    const std::array<std::uint8_t, 4> bytes{0x4f, 0x50, 0x47, 0x44};
    const opengold::Core core;

    check(core.checksum(bytes) == 0xcb4b7229u, "Known checksum");

    // One 2x1-byte (16x1 pixel) synthetic EGA sprite frame in a DAX record.
    const std::array<std::uint8_t, 42> dax{
        9, 0, 2, 0, 0, 0, 0, 30, 0, 31, 0, 29, 1, 0,    0,    0,    0,    1,    0,    2,    0,
        0, 0, 0, 0, 1, 0, 0, 0,  0, 0,  0, 0,  0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
    const auto decoded = opengold::decode_ega_sprite(dax, 2);
    check(bool(decoded), "Synthetic sprite decodes");
    check(decoded.image.width == 16 && decoded.image.height == 1 && decoded.image.rgba.size() == 64,
          "Sprite dimensions and buffer agree");
    check(decoded.image.rgba[3] == 255 && decoded.image.rgba[63] == 0, "Sprite transparency");

    // CPIC uses one 17-byte header for every pose, unlike SPRIT records.
    std::vector<std::uint8_t> cpic{9, 0, 4, 0, 0, 0, 0, 25, 0, 26, 0, 24};
    cpic.resize(12 + 25, 0);
    cpic[12] = 1;
    cpic[14] = 2;
    cpic[20] = 1;
    cpic[29] = 0x08;
    cpic[30] = 0xd1;
    const auto icon = opengold::decode_ega_combat_icon(cpic, 4);
    check(icon && icon.image.width == 16 && icon.image.height == 1, "Combat icon dimensions");
    check(icon.image.rgba[3] == 0 && icon.image.rgba[4] == 0 && icon.image.rgba[7] == 255,
          "Combat transparency and opaque black");
    check(icon.image.rgba[8] == 255 && icon.image.rgba[9] == 85 && icon.image.rgba[10] == 255,
          "Combat magenta palette entry");
    check(!opengold::decode_ega_combat_icon(cpic, 4, 1), "Absent combat icon frame rejects");
    cpic.pop_back();
    check(!opengold::decode_ega_combat_icon(cpic, 4), "Truncated combat icon rejects");

    if (const char *game_dir = std::getenv("OPENGOLD_GAME_DIR"))
    {
        std::ifstream input(std::filesystem::path(game_dir) / "SPRIT1.DAX", std::ios::binary);
        const std::vector<std::uint8_t> installed{std::istreambuf_iterator<char>(input),
                                                  std::istreambuf_iterator<char>()};
        const auto installed_image = opengold::decode_ega_sprite(installed, 2);
        check(bool(installed_image), "Installed sprite decodes");
        check(installed_image.image.width == 48 && installed_image.image.height == 80,
              "Installed sprite dimensions");
    }
}
} // namespace

int main()
{
    try
    {
        decode_tests();
        sprite_boundaries();
        std::cout << "Core format tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
