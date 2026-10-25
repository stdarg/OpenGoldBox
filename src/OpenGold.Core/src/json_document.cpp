#include "json_document.h"
#include "opengold/path_text.h"
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

} // namespace

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
