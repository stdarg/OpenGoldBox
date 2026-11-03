// Imports the Creature Stats Editor's edits into the game's data
// (dev_tools/docs/creature-stats-editor-interface.md).
#include "creature_edits.h"
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string_view>

namespace
{
std::string today()
{
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
    localtime_r(&now, &local);
    char text[16];
    std::strftime(text, sizeof text, "%Y-%m-%d", &local);
    return text;
}

void usage()
{
    std::cerr
        << "Usage: opengold_import_edits [--check] [--repo DIR] [--edits FILE] [--game GAME_DIR]\n"
           "Applies dev_tools/creature_stats_editor/edits/edits.json to the repository's\n"
           "data. --check reports without writing. GAME_DIR (default $OPENGOLD_GAME_DIR)\n"
           "checks fight overrides and records against the original's files.\n";
}
} // namespace

int main(int argc, char **argv)
{
    opengold::edits::ImportOptions options;
    options.repository = OPENGOLD_SOURCE_DIR;
    options.date = today();
    std::optional<std::filesystem::path> edits;
    if (const auto *game = std::getenv("OPENGOLD_GAME_DIR"); game && *game)
    {
        options.game_directory = game;
    }
    for (int n = 1; n < argc; ++n)
    {
        const std::string_view argument = argv[n];
        const bool has_value = n + 1 < argc;
        if (argument == "--check")
        {
            options.write = false;
        }
        else if (argument == "--repo" && has_value)
        {
            options.repository = argv[++n];
        }
        else if (argument == "--edits" && has_value)
        {
            edits = argv[++n];
        }
        else if (argument == "--game" && has_value)
        {
            options.game_directory = argv[++n];
        }
        else
        {
            usage();
            return 2;
        }
    }
    options.edits_file =
        edits.value_or(options.repository / "dev_tools/creature_stats_editor/edits/edits.json");
    try
    {
        std::cout << opengold::edits::report_json(opengold::edits::import_edits(options));
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cout << "{\"ok\": false, \"error\": ";
        std::string text = error.what();
        std::string escaped;
        for (const char c : text)
        {
            if (c == '"' || c == '\\')
            {
                escaped += '\\';
            }
            escaped += c == '\n' ? ' ' : c;
        }
        std::cout << '"' << escaped << "\"}\n";
        std::cerr << error.what() << '\n';
        return 1;
    }
}
