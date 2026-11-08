#ifndef OPENGOLD_JSON_DOCUMENT_H
#define OPENGOLD_JSON_DOCUMENT_H

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace opengold::detail
{
// A JSON value, enough for the files OpenGoldBox's tools export (data/shops,
// data/monsters). Objects keep their members in order; the files are small, so
// lookup is a scan.
struct Json
{
    // Its own type rather than std::pair<std::string, Json>: a pair would hold
    // Json while Json is still incomplete, which Clang rejects with libstdc++.
    struct Member;
    using Array = std::vector<Json>;
    using Object = std::vector<Member>;
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value;

    [[nodiscard]] const Json *find(std::string_view key) const;
};

struct Json::Member
{
    std::string name;
    Json value;
};

inline const Json *Json::find(std::string_view key) const
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

// Parses a whole document; throws std::runtime_error on anything else.
[[nodiscard]] Json parse_json(std::string_view text);

// Reads and parses a file; the error names `what` and the path.
[[nodiscard]] Json read_json_file(const std::filesystem::path &path, std::string_view what);

// Whether the values are the same, objects compared member by member in any
// order.
[[nodiscard]] bool json_equal(const Json &a, const Json &b);

// The value as the data files lay it out: an array or object whose members are
// all plain values on one line, any other one member per line, indented by two.
// Reading a shipped file and writing it gives it back byte for byte.
[[nodiscard]] std::string json_text(const Json &value);

// json_text with the newline a file ends with.
[[nodiscard]] std::string json_file_text(const Json &document);

// A field the object must have, of the type it must be; the error names `what`.
template <typename T>
const T &json_field(const Json &object, std::string_view key, std::string_view what)
{
    const auto *member = object.find(key);
    const auto *value = member ? std::get_if<T>(&member->value) : nullptr;
    if (!value)
    {
        throw std::runtime_error(std::string(what) + " lacks " + std::string(key));
    }
    return *value;
}
} // namespace opengold::detail
#endif
