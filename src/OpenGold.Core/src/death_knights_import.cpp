#include "opengold/death_knights_import.h"

#include "area_loading.h"
#include "opengold/equipment.h"
#include "opengold/por_sound.h"

#include <algorithm>
#include <memory>
#include <stdexcept>

namespace opengold
{
namespace
{
struct ImportTables
{
    std::vector<std::string> words;
    std::vector<por::ItemTemplate> templates;
};
} // namespace

CampaignParty::ItemConversion champions_items_for_death_knights(const std::filesystem::path &folder)
{
    constexpr auto game = GoldBoxGame::death_knights_of_krynn;
    namespace detail = por::detail;
    auto words =
        por::decode_item_words(por::unpack_sound_executable(detail::read_archive(
                                   detail::resolve_archive(folder, item_words_program(game)))),
                               game);
    auto templates = por::decode_item_templates(
        detail::read_archive(detail::resolve_archive(folder, item_types_file(game))), game);
    if (!words || !templates)
    {
        throw std::runtime_error("Death Knights' START.EXE or ITEMS cannot be read");
    }
    auto tables = std::make_shared<const ImportTables>(
        ImportTables{std::move(*words), std::move(*templates)});
    return [tables](const por::Equipment &from) -> std::optional<por::Equipment>
    {
        constexpr std::array<int, 7> discontinued{42, 47, 58, 60, 63, 69, 72};
        const auto &item = from.stored;
        if (std::ranges::find(discontinued, item.type) != discontinued.end() ||
            item.type >= tables->templates.size())
        {
            return std::nullopt;
        }
        por::Equipment taken;
        taken.base = tables->templates[item.type];
        taken.bonuses = por::equipment_bonuses(item, taken.base);
        taken.decoded_name = por::item_name(item, tables->words);
        taken.unidentified_name = por::item_name(item, tables->words, item.hidden_name_words);
        taken.stored = item;
        return taken;
    };
}

CampaignParty::BroughtMoney
champions_money_for_death_knights(const Purse &purse, std::size_t character, std::size_t count)
{
    if (!count || character >= count)
    {
        throw std::invalid_argument("A money share needs a character in the source party");
    }
    const auto share = [&](Coin coin)
    {
        const auto amount = purse[static_cast<std::size_t>(coin)];
        return static_cast<std::uint32_t>(amount / count + (character < amount % count ? 1 : 0));
    };
    return {share(Coin::platinum), share(Coin::gems),     share(Coin::jewelry), share(Coin::copper),
            share(Coin::silver),   share(Coin::electrum), share(Coin::gold)};
}
} // namespace opengold
