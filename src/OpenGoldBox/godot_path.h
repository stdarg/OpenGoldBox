#ifndef OPENGOLDBOX_GODOT_PATH_H
#define OPENGOLDBOX_GODOT_PATH_H

#include <filesystem>
#include <godot_cpp/variant/string.hpp>
#include <string>
#include <string_view>

namespace presentation
{
// A path from UTF-8 bytes. Converting each byte to char8_t keeps non-ASCII
// names intact on Windows, where a plain char path is read in the ANSI code
// page. Replaces std::filesystem::u8path, deprecated in C++20.
[[nodiscard]] inline std::filesystem::path path_from_utf8(std::string_view utf8)
{
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

[[nodiscard]] inline std::filesystem::path path_from_godot(const godot::String &text)
{
    const godot::CharString utf8 = text.utf8();
    return path_from_utf8(
               std::string_view(utf8.get_data(), static_cast<std::size_t>(utf8.length())));
}
} // namespace presentation

#endif
