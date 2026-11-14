#include "opengold/original_content.h"

#include "opengold/creature_catalog.h"

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

} // namespace opengold
