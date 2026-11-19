#include "opengold/original_content.h"

#include "area_loading.h"
#include "opengold/creature_catalog.h"
#include "opengold/original.h"
#include "opengold/por_sound.h"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace opengold
{

original::Content load_original_content(const std::filesystem::path &folder, GoldBoxGame game)
{
    original::Content content;
    content.game = game;
    const auto catalog = por::CreatureCatalog::load(folder, game);
    for (const auto &[id, creature] : catalog.all())
    {
        std::vector<original::CarriedItem> carried;
        for (const auto &item : creature.equipment)
        {
            carried.push_back({item.stored, item.base});
        }
        std::vector<por::EffectRecord> effects;
        for (const auto &effect : creature.effects)
        {
            effects.push_back(effect.stored);
        }
        content.bestiary.emplace(
            id.key(), original::combatant_from_record(creature.stored, carried, effects));
        content.gear.emplace(id.key(), original::MonsterGear{creature.stored, std::move(carried)});
    }
    std::ifstream items(folder / item_types_file(game), std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(items), {}};
    auto types = por::decode_item_templates(bytes, game);
    if (!types)
    {
        throw std::runtime_error("The game's ITEMS file cannot be read");
    }
    content.item_types = std::move(*types);
    return content;
}

std::map<unsigned, por::Equipment> load_item_definitions(const std::filesystem::path &folder,
                                                         GoldBoxGame game)
{
    const auto types = por::decode_item_templates(
        por::detail::read_archive(por::detail::resolve_archive(folder, item_types_file(game))),
        game);
    if (!types)
    {
        throw std::runtime_error("The game's ITEMS file cannot be read");
    }
    const auto program = item_words_program(game);
    auto words = por::decode_item_words(por::unpack_sound_executable(por::detail::read_archive(
                                            por::detail::resolve_archive(folder, program))),
                                        game);
    if (!words)
    {
        throw std::runtime_error(std::string(program) + " has no item name table");
    }
    if (game == GoldBoxGame::champions_of_krynn)
    {
        // The knight's three creation items are constructed by CharGen, not ITEM0.
        std::map<unsigned, por::Equipment> definitions;
        constexpr std::array<unsigned, 3> kinds{36, 18, 37};
        constexpr std::array<unsigned, 3> weights{450, 50, 60};
        for (std::size_t at = 0; at < kinds.size(); ++at)
        {
            std::array<std::uint8_t, 63> raw{};
            raw[46] = static_cast<std::uint8_t>(kinds[at]);
            raw[49] = raw[46];
            if (at == 0)
            {
                raw[48] = 47;
            }
            raw[55] = static_cast<std::uint8_t>(weights[at] & 255);
            raw[56] = static_cast<std::uint8_t>(weights[at] >> 8);
            por::Equipment item;
            item.stored = por::decode_items(raw, game)->front();
            item.base = types->at(kinds[at]);
            item.bonuses = por::equipment_bonuses(item.stored, item.base);
            item.decoded_name = por::item_name(item.stored, *words);
            item.unidentified_name = item.decoded_name;
            definitions.emplace(kinds[at], std::move(item));
        }
        return definitions;
    }
    const por::detail::AreaFiles files(folder, *types, std::move(*words), game);
    constexpr auto definitions = "ITEM0.DAX";
    return files.item_definitions(definitions, files.archive(definitions).records.at(0).id);
}

void outfit_original_character(CampaignParty &party, MemberId id,
                               const std::map<unsigned, por::Equipment> &definitions)
{
    const auto &sheet = party.member(id).character.sheet();
    if (!sheet.original)
    {
        return;
    }
    const auto starting =
        original::starting_items(sheet.original->class_levels, original::game_of(sheet.identity));
    for (const auto &wanted : starting)
    {
        const auto found = definitions.find(wanted.definition);
        if (found == definitions.end())
        {
            continue;
        }
        auto item = found->second;
        // The character knows its equipment: no name word stays hidden.
        constexpr std::uint8_t hidden_words = 0x07;
        item.stored.hidden_name_words &= static_cast<std::uint8_t>(~hidden_words);
        if (wanted.count)
        {
            item.stored.stack_size = static_cast<std::uint8_t>(*wanted.count);
        }
        party.take_item(id, item);
        if (!wanted.readied)
        {
            continue;
        }
        const auto taken = party.member(id).character.inventory().items().back().id;
        try
        {
            (void)party.equip(id, taken);
        }
        catch (const std::runtime_error &)
        {
            // The rules refuse it to the character's class, or its hands are
            // full: it stays in the pack.
        }
    }
}

} // namespace opengold
