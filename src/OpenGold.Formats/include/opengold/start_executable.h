#ifndef OPENGOLD_START_EXECUTABLE_H
#define OPENGOLD_START_EXECUTABLE_H

#include <filesystem>
#include <optional>

namespace opengold::format
{
// A game folder's START.EXE, whatever the case of its name: START.EXE itself
// first, then start.exe, then any other spelling (as Start.exe, which some
// installers write). Nothing when the folder has none or cannot be read.
[[nodiscard]] std::optional<std::filesystem::path>
find_start_executable(const std::filesystem::path &game_folder);
} // namespace opengold::format
#endif
