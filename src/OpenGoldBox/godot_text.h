#ifndef OPENGOLDBOX_GODOT_TEXT_H
#define OPENGOLDBOX_GODOT_TEXT_H

#include <godot_cpp/variant/string.hpp>
#include <cstdint>
#include <string_view>

namespace presentation
{
inline godot::String gs(std::string_view text)
{
    return godot::String::utf8(text.data(), static_cast<std::int64_t>(text.size()));
}
} // namespace presentation
#endif
