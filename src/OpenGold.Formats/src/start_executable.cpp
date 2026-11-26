#include "opengold/start_executable.h"

#include <cctype>
#include <string>
#include <system_error>

namespace opengold::format
{
namespace
{
std::string upper_case(std::string text)
{
    for (auto &c : text)
    {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return text;
}
} // namespace

std::optional<std::filesystem::path> find_start_executable(const std::filesystem::path &game_folder)
{
    // The two exact spellings come first so a case-sensitive folder holding
    // several still opens the one the game's sound loader always opened.
    for (const auto *name : {"START.EXE", "start.exe"})
    {
        auto path = game_folder / name;
        if (std::filesystem::exists(path))
        {
            return path;
        }
    }

    std::error_code error;
    for (const auto &entry : std::filesystem::directory_iterator(game_folder, error))
    {
        if (upper_case(entry.path().filename().string()) == "START.EXE")
        {
            return entry.path();
        }
    }
    return std::nullopt;
}
} // namespace opengold::format
