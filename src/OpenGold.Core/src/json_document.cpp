#include "json_document.h"
#include "opengold/path_text.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <locale>
#include <sstream>

namespace opengold::detail
{
namespace
{
class JsonReader
{
  public:
    explicit JsonReader(std::string_view text) : text_(text)
    {
    }

    Json document()
    {
        auto result = value();
        skip_space();
        if (at_ != text_.size())
        {
            fail();
        }
        return result;
    }

  private:
    [[noreturn]] static void fail()
    {
        throw std::runtime_error("Invalid JSON");
    }

    void skip_space()
    {
        while (at_ < text_.size() && std::string_view(" \t\r\n").find(text_[at_]) != npos)
        {
            ++at_;
        }
    }

    bool take(char c)
    {
        skip_space();
        if (at_ < text_.size() && text_[at_] == c)
        {
            ++at_;
            return true;
        }
        return false;
    }

    void expect(char c)
    {
        if (!take(c))
        {
            fail();
        }
    }

    bool take_word(std::string_view word)
    {
        if (text_.substr(at_, word.size()) == word)
        {
            at_ += word.size();
            return true;
        }
        return false;
    }

    Json value()
    {
        skip_space();
        if (at_ == text_.size())
        {
            fail();
        }
        const char c = text_[at_];
        if (c == '{')
        {
            return object();
        }
        if (c == '[')
        {
            return array();
        }
        if (c == '"')
        {
            return {string()};
        }
        if (take_word("true"))
        {
            return {true};
        }
        if (take_word("false"))
        {
            return {false};
        }
        if (take_word("null"))
        {
            return {nullptr};
        }
        return {number()};
    }

    Json object()
    {
        expect('{');
        Json::Object members;
        if (take('}'))
        {
            return {std::move(members)};
        }
        do
        {
            skip_space();
            auto key = string();
            expect(':');
            members.emplace_back(std::move(key), value());
        } while (take(','));
        expect('}');
        return {std::move(members)};
    }

    Json array()
    {
        expect('[');
        Json::Array items;
        if (take(']'))
        {
            return {std::move(items)};
        }
        do
        {
            items.push_back(value());
        } while (take(','));
        expect(']');
        return {std::move(items)};
    }

    unsigned hex4()
    {
        if (at_ + 4 > text_.size())
        {
            fail();
        }
        unsigned code = 0;
        for (int n = 0; n < 4; ++n)
        {
            const char c = text_[at_++];
            const auto digit =
                std::string_view("0123456789abcdef")
                    .find(static_cast<char>(c >= 'A' && c <= 'F' ? c - 'A' + 'a' : c));
            if (digit == npos)
            {
                fail();
            }
            code = code * 16 + static_cast<unsigned>(digit);
        }
        return code;
    }

    static void append_utf8(std::string &out, unsigned code)
    {
        if (code < 0x80)
        {
            out += static_cast<char>(code);
        }
        else if (code < 0x800)
        {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
        else if (code < 0x10000)
        {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
        else
        {
            out += static_cast<char>(0xF0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }

    std::string string()
    {
        if (at_ == text_.size() || text_[at_] != '"')
        {
            fail();
        }
        ++at_;
        std::string out;
        while (at_ < text_.size() && text_[at_] != '"')
        {
            const char c = text_[at_++];
            if (c != '\\')
            {
                out += c;
                continue;
            }
            if (at_ == text_.size())
            {
                fail();
            }
            const char escaped = text_[at_++];
            const auto simple = std::string_view("\"\\/bfnrt").find(escaped);
            if (simple != npos)
            {
                out += "\"\\/\b\f\n\r\t"[simple];
                continue;
            }
            if (escaped != 'u')
            {
                fail();
            }
            unsigned code = hex4();
            // A character beyond the first plane is written as a surrogate pair.
            if (code >= 0xD800 && code < 0xDC00 && take_word("\\u"))
            {
                code = 0x10000 + ((code - 0xD800) << 10) + (hex4() - 0xDC00);
            }
            append_utf8(out, code);
        }
        if (at_ == text_.size())
        {
            fail();
        }
        ++at_;
        return out;
    }

    double number()
    {
        const auto start = at_;
        while (at_ < text_.size() && std::string_view("+-0123456789.eE").find(text_[at_]) != npos)
        {
            ++at_;
        }
        double result{};
        std::istringstream in{std::string(text_.substr(start, at_ - start))};
        in.imbue(std::locale::classic());
        if (start == at_ || !(in >> result) || !in.eof())
        {
            fail();
        }
        return result;
    }

    static constexpr auto npos = std::string_view::npos;
    std::string_view text_;
    std::size_t at_{};
};

bool is_scalar(const Json &value)
{
    return !std::holds_alternative<Json::Array>(value.value) &&
           !std::holds_alternative<Json::Object>(value.value);
}

void write_string(std::string &out, const std::string &text)
{
    out += '"';
    for (const unsigned char c : text)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += static_cast<char>(c);
        }
        else if (c == '\n')
        {
            out += "\\n";
        }
        else if (c < 0x20)
        {
            char escaped[8];
            std::snprintf(escaped, sizeof escaped, "\\u%04x", c);
            out += escaped;
        }
        else
        {
            out += static_cast<char>(c);
        }
    }
    out += '"';
}

void write_scalar(std::string &out, const Json &value)
{
    if (const auto *number = std::get_if<double>(&value.value))
    {
        if (*number == std::floor(*number) && std::abs(*number) < 1e15)
        {
            out += std::to_string(static_cast<long long>(*number));
            return;
        }
        char text[32];
        std::snprintf(text, sizeof text, "%.17g", *number);
        out += text;
        return;
    }
    if (const auto *text = std::get_if<std::string>(&value.value))
    {
        write_string(out, *text);
        return;
    }
    if (const auto *flag = std::get_if<bool>(&value.value))
    {
        out += *flag ? "true" : "false";
        return;
    }
    out += "null";
}

// One line for a value whose members are all plain values, such as a
// conversion's record or a companion's rolls; otherwise one member per line.
bool fits_one_line(const Json &value)
{
    if (const auto *array = std::get_if<Json::Array>(&value.value))
    {
        return std::all_of(array->begin(), array->end(), is_scalar);
    }
    if (const auto *object = std::get_if<Json::Object>(&value.value))
    {
        return std::all_of(object->begin(), object->end(),
                           [](const auto &member)
                           {
                               return is_scalar(member.second);
                           });
    }
    return true;
}

void write_json(std::string &out, const Json &value, std::size_t indent)
{
    if (is_scalar(value))
    {
        write_scalar(out, value);
        return;
    }
    const bool one_line = fits_one_line(value);
    const auto separator = one_line ? std::string(", ") : ",\n" + std::string(indent + 2, ' ');
    const auto open = one_line ? std::string() : "\n" + std::string(indent + 2, ' ');
    const auto close = one_line ? std::string() : "\n" + std::string(indent, ' ');
    if (const auto *array = std::get_if<Json::Array>(&value.value))
    {
        out += '[';
        out += array->empty() ? "" : open;
        for (std::size_t n = 0; n < array->size(); ++n)
        {
            out += n ? separator : "";
            write_json(out, (*array)[n], indent + 2);
        }
        out += array->empty() ? "" : close;
        out += ']';
        return;
    }
    const auto &object = std::get<Json::Object>(value.value);
    out += '{';
    out += object.empty() ? "" : open;
    for (std::size_t n = 0; n < object.size(); ++n)
    {
        out += n ? separator : "";
        write_string(out, object[n].first);
        out += ": ";
        write_json(out, object[n].second, indent + 2);
    }
    out += object.empty() ? "" : close;
    out += '}';
}
} // namespace

bool json_equal(const Json &a, const Json &b)
{
    if (a.value.index() != b.value.index())
    {
        return false;
    }
    if (const auto *object = std::get_if<Json::Object>(&a.value))
    {
        const auto &other = std::get<Json::Object>(b.value);
        return object->size() == other.size() &&
               std::all_of(object->begin(), object->end(),
                           [&](const auto &member)
                           {
                               const auto *found = b.find(member.first);
                               return found && json_equal(member.second, *found);
                           });
    }
    if (const auto *array = std::get_if<Json::Array>(&a.value))
    {
        const auto &other = std::get<Json::Array>(b.value);
        return array->size() == other.size() &&
               std::equal(array->begin(), array->end(), other.begin(), json_equal);
    }
    if (const auto *number = std::get_if<double>(&a.value))
    {
        return *number == std::get<double>(b.value);
    }
    if (const auto *text = std::get_if<std::string>(&a.value))
    {
        return *text == std::get<std::string>(b.value);
    }
    if (const auto *flag = std::get_if<bool>(&a.value))
    {
        return *flag == std::get<bool>(b.value);
    }
    return true; // both null
}

std::string json_text(const Json &value)
{
    std::string out;
    write_json(out, value, 0);
    return out;
}

std::string json_file_text(const Json &document)
{
    return json_text(document) + "\n";
}

Json parse_json(std::string_view text)
{
    return JsonReader(text).document();
}

Json read_json_file(const std::filesystem::path &path, std::string_view what)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw std::runtime_error("Cannot read " + std::string(what) + " " + path_text(path));
    }
    std::ostringstream text;
    text << in.rdbuf();
    try
    {
        return parse_json(text.str());
    }
    catch (const std::runtime_error &)
    {
        throw std::runtime_error("Cannot read " + std::string(what) + " " + path_text(path));
    }
}
} // namespace opengold::detail
