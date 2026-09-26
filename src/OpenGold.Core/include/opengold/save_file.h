#ifndef OPENGOLD_SAVE_FILE_H
#define OPENGOLD_SAVE_FILE_H

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace opengold
{
// Storage only: callers own checkpoint encoding and compatibility validation.
[[nodiscard]] std::string read_save_file(const std::filesystem::path &path, std::size_t limit);
// Flush and verify before replacement; retain the old file at <path>.bak.
// Each write owns a separate temporary file and removes it on failure.
void write_save_file(const std::filesystem::path &path, std::string_view bytes, std::size_t limit);
} // namespace opengold
#endif
