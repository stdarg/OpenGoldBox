#include "opengold/shop_data.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <variant>

namespace opengold
{
namespace
{
// A JSON value, enough for the Shop & Temple Manager's files. Objects keep
// their members in order; the files are small, so lookup is a scan.
struct Json
{
    using Array = std::vector<Json>;
    using Object = std::vector<std::pair<std::string, Json>>;
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value;

    [[nodiscard]] const Json *find(std::string_view key) const
    {
        if (const auto *object = std::get_if<Object>(&value))
        {
            for (const auto &[name, member] : *object)
            {
                if (name == key)
                {
                    return &member;
                }
            }
        }
        return nullptr;
    }
};

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

Json read_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw std::runtime_error("Cannot read shop data " + path.string());
    }
    std::ostringstream text;
    text << in.rdbuf();
    try
    {
        return JsonReader(text.str()).document();
    }
    catch (const std::runtime_error &)
    {
        throw std::runtime_error("Cannot read shop data " + path.string());
    }
}

// A field the file must have, of the type it must be.
template <typename T> const T &field(const Json &object, std::string_view key)
{
    const auto *member = object.find(key);
    const auto *value = member ? std::get_if<T>(&member->value) : nullptr;
    if (!value)
    {
        throw std::runtime_error("Shop data lacks " + std::string(key));
    }
    return *value;
}

// A whole number in [low, high]; the manager writes some as 0.0.
long whole(const Json &object, std::string_view key, long low, long high)
{
    const double number = field<double>(object, key);
    if (number != std::floor(number) || number < low || number > high)
    {
        throw std::runtime_error("Invalid shop data " + std::string(key));
    }
    return static_cast<long>(number);
}

unsigned whole(const Json &value)
{
    const auto *number = std::get_if<double>(&value.value);
    if (!number || *number != std::floor(*number) || *number < 0 || *number > 255)
    {
        throw std::runtime_error("Invalid shop data marker");
    }
    return static_cast<unsigned>(*number);
}

std::vector<std::pair<unsigned, unsigned>> markers(const Json &location)
{
    std::vector<std::pair<unsigned, unsigned>> result;
    for (const auto &marker : field<Json::Array>(location, "markers"))
    {
        const auto *pair = std::get_if<Json::Array>(&marker.value);
        if (!pair || pair->size() != 2)
        {
            throw std::runtime_error("Invalid shop data marker");
        }
        result.emplace_back(whole((*pair)[0]), whole((*pair)[1]));
    }
    return result;
}

// A portrait as the game finds it, in its portraits folder; the Shop & Temple
// Manager exports its own portraits there, so records never name its folder.
std::string game_portrait(const std::string &path)
{
    const std::filesystem::path portrait(path);
    const bool outside = portrait.is_absolute() ||
                         std::ranges::find(portrait, std::filesystem::path("..")) != portrait.end();
    if (outside)
    {
        throw std::runtime_error("A shop portrait is outside the game's portraits: " + path);
    }
    return path;
}

std::vector<TempleOffering> temple_offerings(const Json &record)
{
    std::vector<TempleOffering> result;
    for (const auto &offering : field<Json::Array>(record, "offerings"))
    {
        const auto service = temple_service_named(field<std::string>(offering, "service_id"));
        if (!service)
        {
            throw std::runtime_error("Invalid temple service in data/shops");
        }
        const auto price = static_cast<std::uint32_t>(whole(offering, "price_gp", 0, 1'000'000));
        if (field<bool>(offering, "offered"))
        {
            result.push_back({*service, price});
        }
    }
    return result;
}

std::vector<ShopOffering> shop_offerings(const Json &record)
{
    std::vector<ShopOffering> result;
    for (const auto &item : field<Json::Array>(record, "offerings"))
    {
        // -1 is unlimited and a positive quantity a limit, which the game does
        // not keep yet: only 0, out of stock, changes what the shop sells.
        if (whole(item, "quantity", -1, 1'000'000) == 0)
        {
            continue;
        }
        ShopOffering offering;
        offering.item_type = static_cast<unsigned>(whole(item, "item_type", 0, 255));
        offering.name = field<std::string>(item, "name");
        // An item's value is 16 bits, as in the original's records.
        offering.price_gp = static_cast<std::uint32_t>(whole(item, "price_gp", 0, 65535));
        offering.bundle_quantity = static_cast<unsigned>(whole(item, "bundle_quantity", 1, 255));
        const auto &archive = field<std::string>(item, "source_archive");
        if (archive.starts_with("ITEM") && archive.ends_with(".DAX"))
        {
            offering.archive = archive;
            offering.record = static_cast<unsigned>(whole(item, "source_record", 0, 255));
            offering.index = static_cast<unsigned>(whole(item, "source_index", 0, 255));
        }
        else if (offering.item_type < 200)
        {
            throw std::runtime_error("A shop item has no original source: " + offering.name);
        }
        result.push_back(std::move(offering));
    }
    return result;
}
} // namespace

ShopData load_shop_data(const std::filesystem::path &directory)
{
    const auto manifest = read_file(directory / "locations.json");
    ShopData data;
    for (const auto &location : field<Json::Array>(manifest, "locations"))
    {
        std::string archive;
        unsigned record{};
        for (const auto &map : field<Json::Array>(manifest, "maps"))
        {
            if (field<std::string>(map, "name") == field<std::string>(location, "map"))
            {
                archive = field<std::string>(map, "archive");
                record = static_cast<unsigned>(whole(map, "record", 0, 255));
            }
        }
        if (archive.empty())
        {
            throw std::runtime_error("A location in data/shops names no known map");
        }
        const auto &id = field<std::string>(location, "id");
        const auto file = read_file(directory / (id + ".json"));
        const auto &kind = field<std::string>(location, "kind");
        const auto portrait = game_portrait(field<std::string>(file, "portrait"));
        if (kind == "temple")
        {
            data.temples.push_back(
                {archive, record, markers(location), temple_offerings(file), portrait});
        }
        else if (kind == "shop")
        {
            data.shops.push_back(
                {archive, record, markers(location), portrait, shop_offerings(file)});
        }
        else
        {
            throw std::runtime_error("Unknown location kind in data/shops: " + kind);
        }
    }
    return data;
}
} // namespace opengold
