#include "../../../src/OpenGold.Core/src/archive_io.h"
#include "wall_art_catalog.h"
#include "map_art_profiles.h"
#include "opengold/exploration_view.h"
#include "opengold/map_catalog.h"
#include "opengold/wall_art.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <tuple>

using namespace godot;
using namespace opengold;
using namespace opengold::por;

namespace
{
String godot_string(const std::string &value)
{
    return String::utf8(value.c_str());
}

std::filesystem::path archive_path(const std::filesystem::path &directory, const std::string &name)
{
    return detail::find_archive<std::runtime_error>(directory, name, "Ambiguous archive: " + name,
                                                    "Missing archive: " + name);
}

DaxDecodeResult read_archive(const std::filesystem::path &directory, const std::string &name)
{
    const auto path = archive_path(directory, name);
    auto bytes = detail::read_archive_bytes<std::runtime_error>(
        path, "Archive exceeds size limit: " + name, "Cannot open archive: " + name,
        "Incomplete archive read: " + name);
    auto decoded = decode_dax_archive(bytes);
    if (!decoded)
        throw std::runtime_error("Invalid DAX archive: " + name);
    return decoded;
}

const std::vector<std::uint8_t> &record(const DaxDecodeResult &archive, unsigned id,
                                        const std::string &name)
{
    for (const auto &item : archive.records)
        if (item.id == id)
            return item.bytes;
    throw std::runtime_error("Missing record " + name + ":" + std::to_string(id));
}

bool has_record(const DaxDecodeResult &archive, unsigned id)
{
    return std::any_of(archive.records.begin(), archive.records.end(),
                       [id](const DaxRecord &item)
                       {
                           return item.id == id;
                       });
}

void append_tiles(WallTiles &destination, const DaxDecodeResult &archive, unsigned id,
                  const std::string &name, unsigned expected)
{
    auto decoded = decode_wall_tiles(record(archive, id, name));
    if (!decoded || decoded->size() != expected)
        throw std::runtime_error("Invalid wall tiles " + name + ":" + std::to_string(id));
    destination.insert(destination.end(), decoded->begin(), decoded->end());
}

Ref<godot::Image> godot_image(const opengold::Image &source)
{
    if (!consistent(source))
        return {};
    PackedByteArray pixels;
    pixels.resize(source.rgba.size());
    std::copy(source.rgba.begin(), source.rgba.end(), pixels.ptrw());
    return godot::Image::create_from_data(source.width, source.height, false,
                                          godot::Image::FORMAT_RGBA8, pixels);
}
} // namespace

void WallArtCatalog::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("load_directory", "directory"), &WallArtCatalog::load_directory);
    ClassDB::bind_method(D_METHOD("entries"), &WallArtCatalog::entries);
    ClassDB::bind_method(D_METHOD("image_at", "index"), &WallArtCatalog::image_at);
    ClassDB::bind_method(D_METHOD("maps"), &WallArtCatalog::maps);
    ClassDB::bind_method(D_METHOD("map_cell", "map_index", "profile_index", "x", "y", "view"),
                         &WallArtCatalog::map_cell);
    ClassDB::bind_method(D_METHOD("map_view", "map_index", "profile_index", "x", "y", "facing"),
                         &WallArtCatalog::map_view);
    ClassDB::bind_method(D_METHOD("map_pick", "map_index", "profile_index", "x", "y", "facing",
                                  "pixel_x", "pixel_y"),
                         &WallArtCatalog::map_pick);
    ClassDB::bind_method(
        D_METHOD("map_visible_pieces", "map_index", "profile_index", "x", "y", "facing"),
        &WallArtCatalog::map_visible_pieces);
}

String WallArtCatalog::load_directory(const String &directory)
{
    entries_.clear();
    maps_.clear();
    try
    {
        const auto path = std::filesystem::path(std::string(directory.utf8().get_data()));
        if (!std::filesystem::is_directory(path))
            throw std::runtime_error("Game directory does not exist: " + path.string());
        const auto shared_name = std::string("8X8D1.DAX");
        const auto shared = read_archive(path, shared_name);
        std::vector<Entry> loaded;
        using ArtKey = std::pair<unsigned, unsigned>;
        std::map<ArtKey, std::vector<std::array<int, 10>>> art_indices;
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
                else if (id * 10 + 2 <= 255 && has_record(tiles, id * 10 + 1) &&
                         has_record(tiles, id * 10 + 2))
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
                    throw std::runtime_error("Invalid wall definitions " + definition_name + ":" +
                                             std::to_string(id));
                auto &indices = art_indices[{bank, id}];
                for (unsigned appearance = 0; appearance < art->appearances.size(); ++appearance)
                {
                    std::array<int, 10> views{};
                    for (unsigned view = 0; view < 10; ++view)
                    {
                        views[view] = static_cast<int>(loaded.size());
                        loaded.push_back({definition_name, id, appearance + 1, view,
                                          art->appearances[appearance][view]});
                    }
                    indices.push_back(views);
                }
            }
        }
        const auto catalog = MapCatalog::load(path);
        std::vector<MapEntry> loaded_maps;
        for (const auto &[id, geometry] : catalog.all())
        {
            const auto bank =
                static_cast<unsigned>(std::stoul(id.archive.substr(3, id.archive.size() - 7)));
            MapEntry map;
            map.archive = id.archive;
            map.record = id.record;
            map.geometry = geometry;
            for (const auto &spec : map_art_profiles())
            {
                if (spec.bank != bank || spec.map_record != id.record)
                    continue;
                map.name = spec.name;
                Profile profile;
                profile.label = spec.label;
                profile.source = spec.source;
                for (const int record_id : spec.wall_records)
                {
                    if (record_id < 0)
                        break;
                    const auto found = art_indices.find({bank, static_cast<unsigned>(record_id)});
                    if (found == art_indices.end())
                        throw std::runtime_error("Missing wall set for " + id.archive + ":" +
                                                 std::to_string(id.record));
                    for (const auto &views : found->second)
                    {
                        profile.appearance_entries.push_back(views);
                        std::array<opengold::Image, 10> pieces;
                        for (unsigned view = 0; view < 10; ++view)
                            pieces[view] = loaded[views[view]].image;
                        profile.art.appearances.push_back(std::move(pieces));
                    }
                }
                for (const auto &cell : geometry.cells)
                    for (const auto wall : cell.walls)
                        if (wall > profile.art.appearances.size())
                            throw std::runtime_error("Wall ID exceeds selected artwork in " +
                                                     id.archive + ":" + std::to_string(id.record));
                map.profiles.push_back(std::move(profile));
            }
            if (map.profiles.empty())
                throw std::runtime_error("No wall-art profile for " + id.archive + ":" +
                                         std::to_string(id.record));
            loaded_maps.push_back(std::move(map));
        }
        entries_ = std::move(loaded);
        maps_ = std::move(loaded_maps);
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
    return godot_image(entries_[index].image);
}

Array WallArtCatalog::maps() const
{
    Array result;
    for (const auto &map : maps_)
    {
        Dictionary metadata;
        metadata["archive"] = godot_string(map.archive);
        metadata["record"] = map.record;
        metadata["name"] = godot_string(map.name);
        Array profiles;
        for (const auto &profile : map.profiles)
        {
            Dictionary choice;
            choice["label"] = godot_string(profile.label);
            choice["source"] = godot_string(profile.source);
            choice["appearances"] = static_cast<int>(profile.appearance_entries.size());
            profiles.push_back(choice);
        }
        metadata["profiles"] = profiles;
        result.push_back(metadata);
    }
    return result;
}

Dictionary WallArtCatalog::map_cell(int map_index, int profile_index, int x, int y, int view) const
{
    if (map_index < 0 || static_cast<std::size_t>(map_index) >= maps_.size() || x < 0 || x >= 16 ||
        y < 0 || y >= 16 || view < 0 || view >= 10)
        return {};
    const auto &map = maps_[map_index];
    if (profile_index < 0 || static_cast<std::size_t>(profile_index) >= map.profiles.size())
        return {};
    const auto &profile = map.profiles[profile_index];
    const auto &cell = map.geometry.at(x, y);
    Dictionary result;
    result["x"] = x;
    result["y"] = y;
    result["event_raw"] = cell.event_raw;
    result["event_number"] = cell.event_number();
    result["interior"] = cell.event_high_bit();
    Array faces;
    for (unsigned direction = 0; direction < 4; ++direction)
    {
        Dictionary face;
        face["direction"] = direction;
        face["wall_id"] = cell.walls[direction];
        face["door_code"] = cell.doors[direction];
        const unsigned wall = cell.walls[direction];
        face["entry_index"] = wall == 0 ? -1 : profile.appearance_entries[wall - 1][view];
        faces.push_back(face);
    }
    result["faces"] = faces;
    return result;
}

Ref<godot::Image> WallArtCatalog::map_view(int map_index, int profile_index, int x, int y,
                                           int facing) const
{
    if (map_index < 0 || static_cast<std::size_t>(map_index) >= maps_.size() || x < 0 || x >= 16 ||
        y < 0 || y >= 16 || facing < 0 || facing >= 4)
        return {};
    const auto &map = maps_[map_index];
    if (profile_index < 0 || static_cast<std::size_t>(profile_index) >= map.profiles.size())
        return {};
    const auto direction = MapDirection{static_cast<std::size_t>(facing)};
    return godot_image(
        compose_exploration_view(map.geometry, map.profiles[profile_index].art, x, y, direction));
}

Dictionary WallArtCatalog::map_pick(int map_index, int profile_index, int x, int y, int facing,
                                    int pixel_x, int pixel_y) const
{
    if (map_index < 0 || static_cast<std::size_t>(map_index) >= maps_.size() || x < 0 || x >= 16 ||
        y < 0 || y >= 16 || facing < 0 || facing >= 4 || pixel_x < 0 || pixel_x >= 88 ||
        pixel_y < 0 || pixel_y >= 88)
        return {};
    const auto &map = maps_[map_index];
    if (profile_index < 0 || static_cast<std::size_t>(profile_index) >= map.profiles.size())
        return {};
    const auto &profile = map.profiles[profile_index];
    const auto rendered = render_exploration_view(map.geometry, profile.art, x, y,
                                                  MapDirection{static_cast<std::size_t>(facing)});
    const auto &hit = rendered.wall_hits[pixel_y * 88 + pixel_x];
    if (hit.cell < 0)
        return {};
    const auto &cell = map.geometry.cells[hit.cell];
    const auto face = index(hit.face);
    const auto wall = cell.walls[face];
    const auto view = static_cast<unsigned>(hit.perspective);
    Dictionary result;
    result["x"] = hit.cell % 16;
    result["y"] = hit.cell / 16;
    result["face"] = static_cast<int>(face);
    result["wall_id"] = wall;
    result["door_code"] = cell.doors[face];
    result["view"] = view;
    result["entry_index"] = profile.appearance_entries[wall - 1][view];
    result["event_raw"] = cell.event_raw;
    return result;
}

Array WallArtCatalog::map_visible_pieces(int map_index, int profile_index, int x, int y,
                                         int facing) const
{
    Array result;
    if (map_index < 0 || static_cast<std::size_t>(map_index) >= maps_.size() || x < 0 || x >= 16 ||
        y < 0 || y >= 16 || facing < 0 || facing >= 4)
        return result;
    const auto &map = maps_[map_index];
    if (profile_index < 0 || static_cast<std::size_t>(profile_index) >= map.profiles.size())
        return result;
    const auto rendered =
        render_exploration_view(map.geometry, map.profiles[profile_index].art, x, y,
                                MapDirection{static_cast<std::size_t>(facing)});
    std::set<std::tuple<int, std::size_t, unsigned>> seen;
    for (int py = 0; py < 88; ++py)
        for (int px = 0; px < 88; ++px)
        {
            const auto &hit = rendered.wall_hits[py * 88 + px];
            if (hit.cell < 0 ||
                !seen.insert({hit.cell, index(hit.face), static_cast<unsigned>(hit.perspective)})
                     .second)
                continue;
            Dictionary piece;
            piece["pixel_x"] = px;
            piece["pixel_y"] = py;
            piece["x"] = hit.cell % 16;
            piece["y"] = hit.cell / 16;
            piece["face"] = static_cast<int>(index(hit.face));
            piece["view"] = static_cast<unsigned>(hit.perspective);
            result.push_back(piece);
        }
    return result;
}
