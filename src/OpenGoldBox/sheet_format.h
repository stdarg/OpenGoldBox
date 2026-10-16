#ifndef OPENGOLDBOX_SHEET_FORMAT_H
#define OPENGOLDBOX_SHEET_FORMAT_H

#include <string>
#include <string_view>

namespace presentation
{
inline std::string signed_number(int value)
{
    return (value >= 0 ? "+" : "") + std::to_string(value);
}

inline std::string rich_text_literal(std::string_view value)
{
    std::string text;
    for (char c : value)
        text += c == '[' ? "[lb]" : std::string(1, c);
    return text;
}
} // namespace presentation
#endif
