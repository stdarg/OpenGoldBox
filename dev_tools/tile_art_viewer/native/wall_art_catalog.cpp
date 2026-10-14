#include "wall_art_catalog.h"
#include "opengold/wall_art.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <stdexcept>

using namespace godot;
using namespace opengold;
using namespace opengold::por;

namespace
{
String godot_string(const std::string &value)
{
    return String::utf8(value.c_str());
}

std::string uppercase(std::string value)
{
    for (char &ch : value)
        if (ch >= 'a' && ch <= 'z')
            ch -= 'a' - 'A';
    return value;
}

// The original installation may use upper- or lowercase DOS filenames.
std::filesystem::path archive_path(const std::filesystem::path &directory,
                                   const std::string &name)
{
    std::optional<std::filesystem::path> match;
    for (const auto &file : std::filesystem::directory_iterator(directory))
    {
        if (!file.is_regular_file() || uppercase(file.path().filename().string()) != name)
            continue;
        if (match)
            throw std::runtime_error("Ambiguous archive: " + name);
        match = file.path();
    }
    if (!match)
        throw std::runtime_error("Missing archive: " + name);
    return *match;
}

DaxDecodeResult read_archive(const std::filesystem::path &directory, const std::string &name)
{
    const auto path = archive_path(directory, name);
    const auto size = std::filesystem::file_size(path);
    if (size > 32 * 1024 * 1024)
        throw std::runtime_error("Archive exceeds size limit: " + name);
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot open archive: " + name);
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
    if (input.bad() || bytes.size() != size)
        throw std::runtime_error("Incomplete archive read: " + name);
    auto decoded = decode_dax_archive(bytes);
    if (!decoded)
        throw std::runtime_error("Invalid DAX archive: " + name);
    return decoded;
}

const std::vector<std::uint8_t> &record(const DaxDecodeResult &archive,
                                        unsigned id, const std::string &name)
{
    for (const auto &item : archive.records)
        if (item.id == id)
            return item.bytes;
    throw std::runtime_error("Missing record " + name + ":" + std::to_string(id));
}

bool has_record(const DaxDecodeResult &archive, unsigned id)
{
    return std::any_of(archive.records.begin(), archive.records.end(),
                       [id](const DaxRecord &item) { return item.id == id; });
}

void append_tiles(WallTiles &destination, const DaxDecodeResult &archive,
                  unsigned id, const std::string &name, unsigned expected)
{
    auto decoded = decode_wall_tiles(record(archive, id, name));
    if (!decoded || decoded->size() != expected)
        throw std::runtime_error("Invalid wall tiles " + name + ":" + std::to_string(id));
    destination.insert(destination.end(), decoded->begin(), decoded->end());
}
} // namespace

void WallArtCatalog::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("load_directory", "directory"), &WallArtCatalog::load_directory);
    ClassDB::bind_method(D_METHOD("entries"), &WallArtCatalog::entries);
    ClassDB::bind_method(D_METHOD("image_at", "index"), &WallArtCatalog::image_at);
}

String WallArtCatalog::load_directory(const String &directory)
{
    entries_.clear();
    try
    {
        const auto path = std::filesystem::path(std::string(directory.utf8().get_data()));
        if (!std::filesystem::is_directory(path))
            throw std::runtime_error("Game directory does not exist: " + path.string());
        const auto shared_name = std::string("8X8D1.DAX");
        const auto shared = read_archive(path, shared_name);
        std::vector<Entry> loaded;
        for (unsigned bank = 1; bank <= 8; ++bank)
        {
            const auto suffix = std::to_string(bank) + ".DAX";
            const auto definition_name = "WALLDEF" + suffix;
            const auto tile_name = "8X8D" + suffix;
            const auto definitions = read_archive(path, definition_name);
            const auto tiles = read_archive(path, tile_name);
            for (const auto &definition : definitions.records)
            {
                WallTiles wall_tiles(1); // Transparent tile slot zero.
                append_tiles(wall_tiles, shared, 203, shared_name, 45);
                const unsigned id = definition.id;
                if (id == 0 && bank == 3)
                {
                    // Phlan's fifteen appearances use three consecutive 70-tile banks.
                    for (unsigned part = 101; part <= 103; ++part)
                        append_tiles(wall_tiles, tiles, part, tile_name, 70);
                }
                else if (has_record(tiles, id))
                    append_tiles(wall_tiles, tiles, id, tile_name, 70);
                else if (id * 10 + 2 <= 255 &&
                         has_record(tiles, id * 10 + 1) && has_record(tiles, id * 10 + 2))
                {
                    // The two ten-appearance records are stored in numbered parts.
                    append_tiles(wall_tiles, tiles, id * 10 + 1, tile_name, 70);
                    append_tiles(wall_tiles, tiles, id * 10 + 2, tile_name, 70);
                }
                else
                    throw std::runtime_error("No tile bank for " + definition_name + ":" +
                                             std::to_string(id));
                const auto art = decode_wall_art(definition.bytes, wall_tiles);
                if (!art)
                    throw std::runtime_error("Invalid wall definitions " + definition_name +
                                             ":" + std::to_string(id));
                for (unsigned appearance = 0; appearance < art->appearances.size(); ++appearance)
                    for (unsigned view = 0; view < 10; ++view)
                        loaded.push_back({definition_name, id, appearance + 1, view,
                                          art->appearances[appearance][view]});
            }
        }
        entries_ = std::move(loaded);
        return {};
    }
    catch (const std::exception &error)
    {
        return godot_string(error.what());
    }
}

Array WallArtCatalog::entries() const
{
    Array result;
    for (const auto &entry : entries_)
    {
        Dictionary metadata;
        metadata["archive"] = godot_string(entry.archive);
        metadata["record"] = entry.record;
        metadata["appearance"] = entry.appearance;
        metadata["view"] = entry.view;
        metadata["width"] = entry.image.width;
        metadata["height"] = entry.image.height;
        result.push_back(metadata);
    }
    return result;
}

Ref<godot::Image> WallArtCatalog::image_at(int index) const
{
    if (index < 0 || static_cast<std::size_t>(index) >= entries_.size())
        return {};
    const auto &source = entries_[index].image;
    if (!consistent(source))
        return {};
    PackedByteArray pixels;
    pixels.resize(source.rgba.size());
    std::copy(source.rgba.begin(), source.rgba.end(), pixels.ptrw());
    return godot::Image::create_from_data(source.width, source.height, false,
                                          godot::Image::FORMAT_RGBA8, pixels);
}
