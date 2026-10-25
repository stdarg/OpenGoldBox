#ifndef OPENGOLD_PATH_TEXT_H
#define OPENGOLD_PATH_TEXT_H

#include <filesystem>
#include <string>

namespace opengold
{
// A path as UTF-8 text, for messages and name comparisons. On Windows
// path::string() converts to the ANSI code page and throws for a character it
// lacks, so a game or user folder named in Japanese would fail to load.
[[nodiscard]] inline std::string path_text(const std::filesystem::path &path)
{
    const auto text = path.u8string();
    return {text.begin(), text.end()};
}
} // namespace opengold

#endif
