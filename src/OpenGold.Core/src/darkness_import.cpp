#include "opengold/darkness_import.h"

#include "area_loading.h"
#include "opengold/equipment.h"
#include "opengold/por_sound.h"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>

namespace opengold
{
namespace
{

constexpr auto darkness = GoldBoxGame::pools_of_darkness;

// What the conversion reads from the Pools of Darkness folder: its item
// types and its own name words.
struct ImportTables
{
    std::vector<std::string> words;
    std::vector<por::ItemTemplate> templates;
};

ImportTables read_tables(const std::filesystem::path &folder)
{
    namespace detail = por::detail;
    const auto program = item_words_program(darkness);
    auto words = por::decode_item_words(por::unpack_sound_executable(detail::read_archive(
                                            detail::resolve_archive(folder, program))),
                                        darkness);
    auto templates = por::decode_item_templates(
        detail::read_archive(detail::resolve_archive(folder, item_types_file(darkness))), darkness);
    if (!words || !templates)
    {
        throw std::runtime_error("Pools of Darkness's GAME.EXE or ITEMS cannot be read");
    }
    return {std::move(*words), std::move(*templates)};
}

} // namespace

CampaignParty::ItemConversion secret_items_for_darkness(const std::filesystem::path &folder)
{
    auto tables = std::make_shared<const ImportTables>(read_tables(folder));
    return [tables](const por::Equipment &from) -> std::optional<por::Equipment>
    {
        // Secret's bundle, which holds the items after it (saveload.pas,
        // LoadChar, its legacy items).
        constexpr std::uint8_t bundle = 73, boots = 45, cloak = 58;
        constexpr std::uint8_t displacement = 89;
        auto item = from.stored;
        if (item.type == bundle || item.type >= tables->templates.size())
        {
            return std::nullopt;
        }
        if (item.type == boots)
        {
            item.magic_bonus = 2;
        }
        if (item.type == cloak)
        {
            item.effect_codes[1] = displacement;
        }
        // The record's bytes, which name the item in the inventory
        // (por::original_item_key), say what its fields now do: its type,
        // name words and bonus at 46 to 50, its effect bytes at 60 to 62.
        item.raw[46] = item.type;
        std::ranges::copy(item.name_components, item.raw.begin() + 47);
        item.raw[50] = static_cast<std::uint8_t>(item.magic_bonus);
        std::ranges::copy(item.effect_codes, item.raw.begin() + 60);
        por::Equipment taken;
        taken.base = tables->templates[item.type];
        taken.bonuses = por::equipment_bonuses(item, taken.base);
        taken.decoded_name = por::item_name(item, tables->words);
        taken.unidentified_name = por::item_name(item, tables->words, item.hidden_name_words);
        taken.stored = std::move(item);
        return taken;
    };
}

} // namespace opengold
