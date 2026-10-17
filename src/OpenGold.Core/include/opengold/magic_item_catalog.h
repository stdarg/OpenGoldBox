#ifndef OPENGOLD_MAGIC_ITEM_CATALOG_H
#define OPENGOLD_MAGIC_ITEM_CATALOG_H

#include "opengold/pool_radiance.h"
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace opengold::por
{
class MagicItemError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

// An enchantment, a curse, charges or a spell. Mundane gear has none of these; a
// giant's boulder sets only byte 62 and is not magic.
[[nodiscard]] bool is_magic(const ItemRecord &item) noexcept;

struct MagicItem
{
    ItemRecord stored;
    std::string name;
    // A scroll's spells (bytes 60-62), or the spell a charged item casts (byte 61).
    std::vector<std::string> spells;
};

// One item list (ITEMn.DAX) or one creature's gear (MONnITM.DAX) holding magic items.
struct MagicItemList
{
    std::string archive;
    std::uint8_t record{};
    std::string carrier; // The MONnCHA creature's name; empty for an item list.
    // Scripts naming this list in a TREASURE, or this creature in a LOAD MONSTER, by a
    // constant. Lists chosen through registers or tables are not found.
    std::vector<std::string> scripts;
    std::vector<MagicItem> items;
};

// The original's magic items, read from the user's game folder (docs/MAGIC_ITEMS.md).
struct MagicItemCatalog
{
    std::vector<std::string> item_words, spell_names;
    std::vector<MagicItemList> lists;

    [[nodiscard]] static MagicItemCatalog load(const std::filesystem::path &directory);
};
} // namespace opengold::por
#endif
