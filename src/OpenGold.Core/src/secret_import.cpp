#include "opengold/secret_import.h"

#include "area_loading.h"
#include "opengold/equipment.h"
#include "opengold/formats.h"
#include "opengold/por_sound.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace opengold
{
namespace
{

constexpr auto curse = GoldBoxGame::curse_of_the_azure_bonds;
constexpr auto secret = GoldBoxGame::secret_of_the_silver_blades;
using NameWords = std::array<std::uint8_t, 3>;

// What the conversion reads from both games' folders.
struct ImportTables
{
    std::vector<std::string> curse_words, secret_words;
    std::vector<por::ItemTemplate> templates;
    // The type of each of Secret's own items, by its name words.
    std::map<NameWords, std::uint8_t> secret_types;
};

// A word's letters and digits, in capitals: "Of Prot." and "OF PROT" are one.
std::string letters_of(std::string_view word)
{
    std::string kept;
    for (const unsigned char c : word)
    {
        if (std::isalnum(c))
        {
            kept += static_cast<char>(std::toupper(c));
        }
    }
    return kept;
}

std::vector<std::string> words_of(const std::filesystem::path &folder, GoldBoxGame game)
{
    namespace detail = por::detail;
    auto words =
        por::decode_item_words(por::unpack_sound_executable(detail::read_archive(
                                   detail::resolve_archive(folder, item_words_program(game)))),
                               game);
    if (!words)
    {
        throw std::runtime_error(std::string(game_title(game)) + "'s START.EXE cannot be read");
    }
    return std::move(*words);
}

// Every item Secret's treasure and creature item lists hold, by its words.
std::map<NameWords, std::uint8_t> types_by_words(const std::filesystem::path &folder)
{
    std::map<NameWords, std::uint8_t> found;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        const auto name = letters_of(entry.path().stem().string());
        if (!entry.is_regular_file() || letters_of(entry.path().extension().string()) != "DAX" ||
            !(name.starts_with("ITEM") || name.ends_with("ITM")))
        {
            continue;
        }
        const auto archive = decode_dax_archive(por::detail::read_archive(entry.path()));
        for (const auto &record : archive.records)
        {
            for (const auto &item :
                 por::decode_items(record.bytes, secret).value_or(std::vector<por::ItemRecord>{}))
            {
                found.emplace(item.name_components, item.type);
            }
        }
    }
    return found;
}

ImportTables read_tables(const std::filesystem::path &curse_folder,
                         const std::filesystem::path &secret_folder)
{
    ImportTables tables;
    tables.curse_words = words_of(curse_folder, curse);
    tables.secret_words = words_of(secret_folder, secret);
    auto templates =
        por::decode_item_templates(por::detail::read_archive(por::detail::resolve_archive(
                                       secret_folder, item_types_file(secret))),
                                   secret);
    if (!templates)
    {
        throw std::runtime_error("Secret of the Silver Blades' ITEMS cannot be read");
    }
    tables.templates = std::move(*templates);
    tables.secret_types = types_by_words(secret_folder);
    return tables;
}

// Secret's word of a Curse word's letters; 0 stays 0, and none is nothing.
std::optional<std::uint8_t> secret_word(const ImportTables &tables, std::uint8_t word)
{
    if (word == 0)
    {
        return std::uint8_t{0};
    }
    if (word >= tables.curse_words.size())
    {
        return std::nullopt;
    }
    const auto wanted = letters_of(tables.curse_words[word]);
    for (std::size_t own = 1; own < tables.secret_words.size() && own <= 255; ++own)
    {
        if (letters_of(tables.secret_words[own]) == wanted)
        {
            return static_cast<std::uint8_t>(own);
        }
    }
    return std::nullopt;
}

// The Secret type of a Curse item of type `type` named by `curse_words`,
// whose words in Secret are `words`.
std::optional<std::uint8_t> secret_type(const ImportTables &tables, const NameWords &curse_words,
                                        const NameWords &words, std::uint8_t type)
{
    if (const auto known = tables.secret_types.find(words); known != tables.secret_types.end())
    {
        return known->second;
    }
    // Both games name an arm or armor by the word of its type's number.
    if (std::ranges::find(curse_words, type) == curse_words.end())
    {
        return std::nullopt;
    }
    const auto named = secret_word(tables, type);
    if (named && *named != 0 && *named < tables.templates.size())
    {
        return *named;
    }
    return std::nullopt;
}

} // namespace

CampaignParty::ItemConversion curse_items_for_secret(const std::filesystem::path &curse_folder,
                                                     const std::filesystem::path &secret_folder)
{
    auto tables = std::make_shared<const ImportTables>(read_tables(curse_folder, secret_folder));
    return [tables](const por::Equipment &from) -> std::optional<por::Equipment>
    {
        auto item = from.stored;
        const auto curse_words = item.name_components;
        for (auto &word : item.name_components)
        {
            const auto own = secret_word(*tables, word);
            if (!own)
            {
                return std::nullopt;
            }
            word = *own;
        }
        const auto type = secret_type(*tables, curse_words, item.name_components, item.type);
        if (!type || *type >= tables->templates.size())
        {
            return std::nullopt;
        }
        item.type = *type;
        // The record's bytes say so too: an item is kept and known by them
        // (por::original_item_key).
        constexpr std::size_t type_byte = 46, first_word_byte = 47;
        item.raw[type_byte] = item.type;
        std::ranges::copy(item.name_components, item.raw.begin() + first_word_byte);
        por::Equipment taken;
        taken.base = tables->templates[item.type];
        taken.bonuses = por::equipment_bonuses(item, taken.base);
        taken.decoded_name = por::item_name(item, tables->secret_words);
        taken.unidentified_name =
            por::item_name(item, tables->secret_words, item.hidden_name_words);
        taken.stored = std::move(item);
        return taken;
    };
}

} // namespace opengold
