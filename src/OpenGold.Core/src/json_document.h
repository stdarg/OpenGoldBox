#ifndef OPENGOLD_JSON_DOCUMENT_H
#define OPENGOLD_JSON_DOCUMENT_H

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace opengold::detail
{
// A JSON value, enough for the files OpenGoldBox's tools export (data/shops,
// data/monsters). Objects keep their members in order; the files are small, so
// lookup is a scan.
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

// Parses a whole document; throws std::runtime_error on anything else.
[[nodiscard]] Json parse_json(std::string_view text);

// Reads and parses a file; the error names `what` and the path.
[[nodiscard]] Json read_json_file(const std::filesystem::path &path, std::string_view what);

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
