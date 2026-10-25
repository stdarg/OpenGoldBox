#include "opengold/wall_art_replacements.h"
#include "opengold/path_text.h"
#include <fstream>
#include <stdexcept>

namespace opengold::por
{
namespace
{
constexpr std::array<std::string_view, 10> view_files{
    "far_front.png",    "far_left.png",   "far_right.png", "middle_front.png", "middle_left.png",
    "middle_right.png", "near_front.png", "near_left.png", "near_right.png",   "far_extension.png"};

// WALLDEF1_DAX_1_4 names the key WALLDEF1.DAX:1:4; anything else is not a set.
std::optional<std::string> key_from_folder(const std::string &name)
{
    constexpr std::string_view prefix = "WALLDEF", middle = "_DAX_";
    if (!name.starts_with(prefix) || name.size() <= prefix.size() + middle.size() ||
        name.compare(prefix.size() + 1, middle.size(), middle) != 0)
    {
        return std::nullopt;
    }
    const char bank = name[prefix.size()];
    const auto rest = name.substr(prefix.size() + 1 + middle.size());
    const auto separator = rest.find('_');
    if (bank < '1' || bank > '8' || separator == std::string::npos || separator == 0 ||
        separator + 1 == rest.size())
    {
        return std::nullopt;
    }
    const auto record = rest.substr(0, separator), appearance = rest.substr(separator + 1);
    const auto digits = [](const std::string &text)
    {
        return text.size() <= 3 && text.find_first_not_of("0123456789") == std::string::npos;
    };
    if (!digits(record) || !digits(appearance) || std::stoul(appearance) == 0)
    {
        return std::nullopt;
    }
    return wall_appearance_key(std::string(prefix) + bank + ".DAX",
                               static_cast<unsigned>(std::stoul(record)),
                               static_cast<unsigned>(std::stoul(appearance)));
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
    {
        throw std::runtime_error("Cannot read wall art " + path_text(path));
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(in.tellg()));
    in.seekg(0);
    if (!in.read(reinterpret_cast<char *>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size())))
    {
        throw std::runtime_error("Cannot read wall art " + path_text(path));
    }
    return bytes;
}

// Nearest-neighbor enlargement, so an original piece keeps its hard pixels.
Image enlarged(const Image &piece, unsigned scale)
{
    Image result;
    result.width = static_cast<std::uint16_t>(piece.width * scale);
    result.height = static_cast<std::uint16_t>(piece.height * scale);
    result.rgba.resize(static_cast<std::size_t>(result.width) * result.height * 4);
    const auto row_bytes = static_cast<std::size_t>(result.width) * 4;
    for (unsigned y = 0; y < piece.height; ++y)
    {
        // Widen one source row, then repeat it for the rows below.
        const auto row = result.rgba.begin() + static_cast<std::ptrdiff_t>(y * scale * row_bytes);
        for (unsigned x = 0; x < result.width; ++x)
        {
            std::copy_n(piece.rgba.begin() + (y * piece.width + x / scale) * 4, 4, row + x * 4);
        }
        for (unsigned copy = 1; copy < scale; ++copy)
        {
            std::copy_n(row, row_bytes, row + static_cast<std::ptrdiff_t>(copy * row_bytes));
        }
    }
    return result;
}
} // namespace

std::string wall_appearance_key(std::string_view archive, unsigned record, unsigned appearance)
{
    return std::string(archive) + ":" + std::to_string(record) + ":" + std::to_string(appearance);
}

std::string_view wall_view_file(WallView view)
{
    return view_files[static_cast<unsigned>(view)];
}

std::vector<std::string> wall_appearance_keys(std::string_view archive, unsigned record,
                                              std::size_t count)
{
    std::vector<std::string> keys;
    for (std::size_t appearance = 1; appearance <= count; ++appearance)
    {
        keys.push_back(wall_appearance_key(archive, record, static_cast<unsigned>(appearance)));
    }
    return keys;
}

WallArtReplacements load_wall_art_replacements(const std::filesystem::path &folder,
                                               const PngDecoder &decode)
{
    WallArtReplacements result;
    if (!std::filesystem::is_directory(folder))
    {
        return result;
    }
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        if (!entry.is_directory())
        {
            continue;
        }
        const auto name = path_text(entry.path().filename());
        const auto key = key_from_folder(name);
        if (!key)
        {
            throw std::runtime_error("Unknown wall art folder " + path_text(entry.path()));
        }
        std::array<Image, 10> pieces;
        for (unsigned view = 0; view < pieces.size(); ++view)
        {
            const auto path = entry.path() / view_files[view];
            auto image = decode(read_bytes(path));
            if (!image || !consistent(*image) || image->width == 0 || image->height == 0)
            {
                throw std::runtime_error("Cannot decode wall art " + path_text(path));
            }
            pieces[view] = std::move(*image);
        }
        result.sets.emplace(*key, std::move(pieces));
    }
    return result;
}

WallArtSet with_replacements(WallArtSet original, std::span<const std::string> keys,
                             const WallArtReplacements &replacements)
{
    if (replacements.sets.empty())
    {
        return original;
    }
    if (keys.size() != original.appearances.size() || original.scale != 1)
    {
        throw std::invalid_argument("Wall art keys do not match its appearances");
    }
    constexpr auto scale = WallArtReplacements::scale;
    for (std::size_t appearance = 0; appearance < keys.size(); ++appearance)
    {
        auto &pieces = original.appearances[appearance];
        const auto found = replacements.sets.find(keys[appearance]);
        for (unsigned view = 0; view < pieces.size(); ++view)
        {
            if (found == replacements.sets.end())
            {
                pieces[view] = enlarged(pieces[view], scale);
                continue;
            }
            const auto &piece = found->second[view];
            if (piece.width != pieces[view].width * scale ||
                piece.height != pieces[view].height * scale)
            {
                throw std::runtime_error("Wall art " + keys[appearance] + " " +
                                         std::string(view_files[view]) + " must be " +
                                         std::to_string(pieces[view].width * scale) + " x " +
                                         std::to_string(pieces[view].height * scale));
            }
            pieces[view] = piece;
        }
    }
    original.scale = scale;
    return original;
}
} // namespace opengold::por
