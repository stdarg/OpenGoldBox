#include "opengold/map_catalog.h"
#include "opengold/dungeon_battlefield.h"
#include "opengold/encounter_formation.h"
#include "opengold/overland.h"
#include "opengold/por_sound.h"
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

namespace
{
void print_cells(const std::vector<std::optional<opengold::rules::Cell>> &cells)
{
    std::cout << '[';
    for (std::size_t n = 0; n < cells.size(); ++n)
    {
        std::cout << (n ? "," : "");
        if (cells[n])
        {
            std::cout << '[' << cells[n]->x << ',' << cells[n]->y << ']';
        }
        else
        {
            std::cout << "null";
        }
    }
    std::cout << ']';
}

unsigned number(std::string_view text)
{
    unsigned n{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), n);
    if (error != std::errc{} || end != text.data() + text.size())
    {
        throw std::runtime_error("Expected unsigned integer");
    }
    return n;
}

std::vector<std::uint8_t> read_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw std::runtime_error("Cannot read " + path.string());
    }
    return {std::istreambuf_iterator<char>(in), {}};
}

// The game directory's START.EXE, whatever the case of its name.
std::filesystem::path start_exe(const std::filesystem::path &directory)
{
    for (const auto &entry : std::filesystem::directory_iterator(directory))
    {
        auto name = entry.path().filename().string();
        for (auto &c : name)
        {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (name == "START.EXE")
        {
            return entry.path();
        }
    }
    throw std::runtime_error("START.EXE not found");
}

// Where a wilderness fight's two sides start on a field of the original's
// combat tiles (50 by 25 bytes, row by row, as read from the running game),
// every square side open, as OpenGoldBox places them.
int wild_formation(const std::filesystem::path &directory, char **argv)
{
    const auto world = opengold::por::read_overland_world(
        opengold::por::unpack_sound_executable(read_file(start_exe(directory))));
    if (!world)
    {
        throw std::runtime_error("START.EXE has no wilderness");
    }
    const auto grid = read_file(argv[3]);
    constexpr int columns = 50, rows = 25;
    if (grid.size() != static_cast<std::size_t>(columns * rows))
    {
        throw std::runtime_error("A field has 50 by 25 tiles");
    }
    opengold::por::DungeonBattlefield field{{columns, rows,
                                             std::vector<opengold::rules::Terrain>(grid.size()),
                                             std::vector<bool>(grid.size(), true)},
                                            std::vector<std::uint8_t>(grid.size())};
    for (std::size_t n = 0; n < grid.size(); ++n)
    {
        const auto &tile = world->combat_tiles.at(grid[n]);
        field.tiles[n] = tile.frame;
        field.geometry.terrain[n] =
            tile.cost == 255 ? opengold::rules::Terrain::obstacle : opengold::rules::Terrain::open;
    }
    const auto facing = number(argv[4]);
    if (facing > 3)
    {
        throw std::runtime_error("Facing exceeds 3");
    }
    const auto starts =
        opengold::por::encounter_formation(field, static_cast<opengold::por::MapDirection>(facing),
                                           number(argv[5]), number(argv[6]), number(argv[7]));
    std::cout << "{\"party\":";
    print_cells(starts.party);
    std::cout << ",\"enemies\":";
    print_cells(starts.enemies);
    std::cout << "}\n";
    return std::cout ? 0 : 1;
}
} // namespace

int main(int argc, char **argv)
{
    const bool formation = argc == 11 && std::string_view(argv[2]) == "--formation";
    const bool wild = argc == 8 && std::string_view(argv[2]) == "--wild-formation";
    if (argc != 2 && argc != 7 && !formation && !wild)
    {
        std::cerr << "Usage: opengold_maps GAME_DIRECTORY [--battlefield GEO_ARCHIVE RECORD X Y]\n"
                     "       opengold_maps GAME_DIRECTORY --formation GEO_ARCHIVE RECORD X Y "
                     "FACING DISTANCE PARTY ENEMIES\n"
                     "       opengold_maps GAME_DIRECTORY --wild-formation FIELD_FILE FACING "
                     "DISTANCE PARTY ENEMIES\n"
                     "FACING is 0 (north) to 3 (west); --formation prints where a fight's "
                     "two sides start (PLACE-1); --wild-formation does it on a wilderness "
                     "field of 50 by 25 combat tiles.\n";
        return 2;
    }
    try
    {
        if (wild)
        {
            return wild_formation(argv[1], argv);
        }
        const auto catalog = opengold::por::MapCatalog::load(argv[1]);
        if (argc == 7 || formation)
        {
            if (!formation && std::string_view(argv[2]) != "--battlefield")
            {
                throw std::runtime_error("Unknown map command");
            }
            const auto record = number(argv[4]);
            if (record > 255)
            {
                throw std::runtime_error("Map record exceeds 255");
            }
            const auto map = catalog.find({argv[3], static_cast<std::uint8_t>(record)});
            if (!map)
            {
                throw std::runtime_error("Map not found");
            }
            const auto field =
                opengold::por::dungeon_battlefield(map->get(), number(argv[5]), number(argv[6]));
            if (formation)
            {
                const auto facing = number(argv[7]);
                if (facing > 3)
                {
                    throw std::runtime_error("Facing exceeds 3");
                }
                const auto starts = opengold::por::encounter_formation(
                    field, static_cast<opengold::por::MapDirection>(facing), number(argv[8]),
                    number(argv[9]), number(argv[10]));
                std::cout << "{\"party\":";
                print_cells(starts.party);
                std::cout << ",\"enemies\":";
                print_cells(starts.enemies);
                std::cout << "}\n";
                return std::cout ? 0 : 1;
            }
            std::cout << "{\"width\":50,\"height\":25,\"tiles\":[";
            for (std::size_t i = 0; i < field.tiles.size(); ++i)
            {
                if (i)
                {
                    std::cout << ',';
                }
                std::cout << static_cast<unsigned>(field.tiles[i]);
            }
            std::cout << "],\"terrain\":[";
            for (std::size_t i = 0; i < field.geometry.terrain.size(); ++i)
            {
                if (i)
                {
                    std::cout << ',';
                }
                std::cout << static_cast<unsigned>(field.geometry.terrain[i]);
            }
            std::cout << "]}\n";
            return std::cout ? 0 : 1;
        }
        std::cout << "{\"version\":1,\"maps\":[";
        bool first = true;
        for (const auto &[id, map] : catalog.all())
        {
            if (!first)
            {
                std::cout << ',';
            }
            first = false;
            // Archive names are restricted to GEO + digits + .DAX by the loader.
            std::cout << "{\"archive\":\"" << id.archive
                      << "\",\"id\":" << static_cast<unsigned>(id.record)
                      << ",\"bytes\":" << map.raw.size() << ",\"cells\":[";
            for (std::size_t i = 0; i < map.cells.size(); ++i)
            {
                if (i)
                {
                    std::cout << ',';
                }
                const auto &c = map.cells[i];
                std::cout << "{\"walls\":[";
                for (unsigned d = 0; d < 4; ++d)
                {
                    if (d)
                    {
                        std::cout << ',';
                    }
                    std::cout << static_cast<unsigned>(c.walls[d]);
                }
                std::cout << "],\"doors\":[";
                for (unsigned d = 0; d < 4; ++d)
                {
                    if (d)
                    {
                        std::cout << ',';
                    }
                    std::cout << static_cast<unsigned>(c.doors[d]);
                }
                std::cout << "],\"event\":" << static_cast<unsigned>(c.event_raw) << '}';
            }
            std::cout << "]}";
        }
        std::cout << "]}\n";
        return std::cout ? 0 : 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
