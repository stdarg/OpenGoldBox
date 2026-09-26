#include "opengold/map_catalog.h"
#include "opengold/dungeon_battlefield.h"
#include <charconv>
#include <iostream>
int main(int argc, char **argv)
{
    if (argc != 2 && argc != 7)
    {
        std::cerr << "Usage: opengold_maps GAME_DIRECTORY [--battlefield GEO_ARCHIVE RECORD X Y]\n";
        return 2;
    }
    try
    {
        const auto catalog = opengold::por::MapCatalog::load(argv[1]);
        if (argc == 7)
        {
            if (std::string_view(argv[2]) != "--battlefield")
                throw std::runtime_error("Unknown map command");
            const auto number = [](std::string_view text)
            {
                unsigned n{};
                const auto [end, error] =
                    std::from_chars(text.data(), text.data() + text.size(), n);
                if (error != std::errc{} || end != text.data() + text.size())
                    throw std::runtime_error("Expected unsigned integer");
                return n;
            };
            const auto record = number(argv[4]);
            if (record > 255)
                throw std::runtime_error("Map record exceeds 255");
            const auto map = catalog.find({argv[3], static_cast<std::uint8_t>(record)});
            if (!map)
                throw std::runtime_error("Map not found");
            const auto field =
                opengold::por::dungeon_battlefield(map->get(), number(argv[5]), number(argv[6]));
            std::cout << "{\"width\":50,\"height\":25,\"tiles\":[";
            for (std::size_t i = 0; i < field.tiles.size(); ++i)
            {
                if (i)
                    std::cout << ',';
                std::cout << unsigned(field.tiles[i]);
            }
            std::cout << "],\"terrain\":[";
            for (std::size_t i = 0; i < field.geometry.terrain.size(); ++i)
            {
                if (i)
                    std::cout << ',';
                std::cout << unsigned(field.geometry.terrain[i]);
            }
            std::cout << "]}\n";
            return std::cout ? 0 : 1;
        }
        std::cout << "{\"version\":1,\"maps\":[";
        bool first = true;
        for (const auto &[id, map] : catalog.all())
        {
            if (!first)
                std::cout << ',';
            first = false;
            // Archive names are restricted to GEO + digits + .DAX by the loader.
            std::cout << "{\"archive\":\"" << id.archive << "\",\"id\":" << unsigned(id.record)
                      << ",\"bytes\":" << map.raw.size() << ",\"cells\":[";
            for (std::size_t i = 0; i < map.cells.size(); ++i)
            {
                if (i)
                    std::cout << ',';
                const auto &c = map.cells[i];
                std::cout << "{\"walls\":[";
                for (unsigned d = 0; d < 4; ++d)
                {
                    if (d)
                        std::cout << ',';
                    std::cout << unsigned(c.walls[d]);
                }
                std::cout << "],\"doors\":[";
                for (unsigned d = 0; d < 4; ++d)
                {
                    if (d)
                        std::cout << ',';
                    std::cout << unsigned(c.doors[d]);
                }
                std::cout << "],\"event\":" << unsigned(c.event_raw) << '}';
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
