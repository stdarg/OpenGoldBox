#include "opengold/dark_queen_import.h"

#include "opengold/dark_queen_records.h"
#include "opengold/ua_design.h"

#include <algorithm>
#include <memory>
#include <stdexcept>

namespace opengold
{
CampaignParty::ItemConversion
death_knights_items_for_dark_queen(const std::filesystem::path &folder)
{
    constexpr auto game = GoldBoxGame::dark_queen_of_krynn;
    const ua::DesignFiles files(folder, folder);
    const auto program = files.file_bytes(item_words_program(game));
    const auto bytes = files.file_bytes(item_types_file(game));
    auto words = program ? por::decode_item_words(*program, game) : std::nullopt;
    auto types = bytes ? por::decode_item_templates(*bytes, game) : std::nullopt;
    if (!words || !types)
    {
        throw std::runtime_error("Dark Queen's native import needs DQK.EXE and ITEMS.DAT");
    }

    struct Tables
    {
        std::vector<std::string> words;
        std::vector<por::ItemTemplate> types;
    };

    auto tables = std::make_shared<const Tables>(Tables{std::move(*words), std::move(*types)});
    return [tables](const por::Equipment &from) -> std::optional<por::Equipment>
    {
        auto item = from.stored;
        // The native importer drops type 54. Ammunition is omitted under
        // the project's unlimited ranged-ammunition policy.
        if (item.type == 54 || item.type == 30 || item.type == 12 || item.type == 74 ||
            item.type >= tables->types.size())
        {
            return std::nullopt;
        }
        for (auto &word : item.name_components)
        {
            word = static_cast<std::uint8_t>(dark_queen::import_item_word(word));
        }
        const auto spell_byte = [](std::uint8_t byte)
        {
            return static_cast<std::uint8_t>(dark_queen::import_spell_number(byte & 127) |
                                             (byte & 128));
        };
        if (item.type == 39 || item.type == 40 || item.type == 73)
        {
            std::ranges::transform(item.effect_codes, item.effect_codes.begin(), spell_byte);
        }
        else if (item.effect_codes[2] == 128)
        {
            item.effect_codes[1] =
                static_cast<std::uint8_t>(dark_queen::import_effect_number(item.effect_codes[1]));
        }
        else
        {
            item.effect_codes[1] = spell_byte(item.effect_codes[1]);
        }
        if (item.type == 52)
        {
            item.type = 51;
            item.effect_codes[2] = 0;
        }
        item.raw[46] = item.type;
        std::ranges::copy(item.name_components, item.raw.begin() + 47);
        std::ranges::copy(item.effect_codes, item.raw.begin() + 60);
        por::Equipment taken;
        taken.stored = item;
        taken.base = tables->types[item.type];
        taken.bonuses = por::equipment_bonuses(item, taken.base);
        taken.decoded_name = por::item_name(item, tables->words);
        taken.unidentified_name = por::item_name(item, tables->words, item.hidden_name_words);
        return taken;
    };
}
} // namespace opengold
