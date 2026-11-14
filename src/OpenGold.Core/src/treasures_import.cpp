#include "opengold/treasures_import.h"

#include "area_loading.h"
#include "opengold/por_sound.h"
#include "opengold/treasures_texts.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <memory>
#include <stdexcept>

namespace opengold
{
namespace
{

constexpr auto treasures = GoldBoxGame::treasures_of_the_savage_frontier;

// What the conversion reads from the Treasures folder.
struct ImportTables
{
    std::vector<std::string> gateway_words, treasures_words;
    // Gateway's item types 1 to 127, each Treasures' type or 0 for none.
    std::array<std::uint8_t, 127> types{};
    std::vector<por::ItemTemplate> templates;
};

std::string upper(std::string text)
{
    std::ranges::transform(text, text.begin(),
                           [](unsigned char c)
                           {
                               return static_cast<char>(std::toupper(c));
                           });
    return text;
}

ImportTables read_tables(const std::filesystem::path &folder)
{
    namespace detail = por::detail;
    ImportTables tables;
    const auto image = por::unpack_sound_executable(
        detail::read_archive(detail::resolve_archive(folder, "GAME.EXE")));
    auto words = por::decode_item_words(image, treasures);
    // Its legacy item types (legacyTypeMap) sit at a fixed place in the
    // release this engine reads, where Gateway's battle axe and hand axe keep
    // their numbers and its types 3 to 6 have none.
    constexpr std::size_t type_table = 0xCE48;
    if (!words || image.size() < type_table + tables.types.size() || image[type_table] != 1 ||
        image[type_table + 1] != 2 || image[type_table + 2] != 0)
    {
        throw std::runtime_error("Treasures' GAME.EXE is not the release this engine reads");
    }
    tables.treasures_words = std::move(*words);
    std::copy_n(image.begin() + static_cast<std::ptrdiff_t>(type_table), tables.types.size(),
                tables.types.begin());
    constexpr std::uint8_t gateway_words_record = 10;
    constexpr std::size_t word_letters = 20;
    const auto record = decode_dax_record(
        detail::read_archive(detail::resolve_archive(folder, "BORD.DAX")), gateway_words_record);
    auto gateway_words =
        record ? treasures::decode_name_table(*record, word_letters) : std::nullopt;
    auto templates = por::decode_item_templates(
        detail::read_archive(detail::resolve_archive(folder, item_types_file(treasures))),
        treasures);
    if (!gateway_words || !templates)
    {
        throw std::runtime_error("Treasures' BORD.DAX or ITEMS cannot be read");
    }
    // Numbered from 1, as the item words are.
    gateway_words->insert(gateway_words->begin(), std::string{});
    tables.gateway_words = std::move(*gateway_words);
    tables.templates = std::move(*templates);
    return tables;
}

// A Gateway name word as Treasures numbers it: the word of the same letters,
// 0 for none or for the words 210 to 212, which it drops; nothing when it
// has no such word.
std::optional<std::uint8_t> treasures_word(const ImportTables &tables, std::uint8_t word)
{
    constexpr std::uint8_t first_dropped = 210, last_dropped = 212;
    if (word == 0 || (word >= first_dropped && word <= last_dropped))
    {
        return 0;
    }
    if (word >= tables.gateway_words.size())
    {
        return std::nullopt;
    }
    const auto wanted = upper(tables.gateway_words[word]);
    for (std::size_t own = 1; own < tables.treasures_words.size(); ++own)
    {
        if (upper(tables.treasures_words[own]) == wanted)
        {
            return static_cast<std::uint8_t>(own);
        }
    }
    return std::nullopt;
}

} // namespace

CampaignParty::ItemConversion gateway_items_for_treasures(const std::filesystem::path &folder)
{
    auto tables = std::make_shared<const ImportTables>(read_tables(folder));
    return [tables](const por::Equipment &from) -> std::optional<por::Equipment>
    {
        constexpr std::uint8_t unusable_effect = 98;
        auto item = from.stored;
        for (auto &word : item.name_components)
        {
            const auto own = treasures_word(*tables, word);
            if (!own)
            {
                return std::nullopt;
            }
            word = *own;
        }
        if (item.type < 1 || item.type > tables->types.size())
        {
            return std::nullopt;
        }
        item.type = tables->types[item.type - 1U];
        if (item.type == 0 || item.type >= tables->templates.size() ||
            item.effect_codes[1] == unusable_effect)
        {
            return std::nullopt;
        }
        item.readied_raw = 0;
        por::Equipment taken;
        taken.base = tables->templates[item.type];
        taken.bonuses = por::equipment_bonuses(item, taken.base);
        taken.decoded_name = por::item_name(item, tables->treasures_words);
        taken.unidentified_name =
            por::item_name(item, tables->treasures_words, item.hidden_name_words);
        taken.stored = std::move(item);
        return taken;
    };
}

} // namespace opengold
