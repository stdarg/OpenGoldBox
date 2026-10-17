#include "opengold/magic_item_catalog.h"

#include <iomanip>
#include <iostream>
#include <sstream>

namespace
{
using opengold::por::MagicItem;
using opengold::por::MagicItemList;

std::string join(const std::vector<std::string> &parts, std::string_view separator)
{
    std::string text;

    for (const auto &part : parts)
    {
        if (!text.empty())
        {
            text += separator;
        }

        text += part;
    }

    return text;
}

// The record facts docs/MAGIC_ITEMS.md explains: type, bonus, curse, charges and spells.
std::string details(const MagicItem &item)
{
    const auto &s = item.stored;
    const bool scroll = s.type == 61 || s.type == 62;
    std::vector<std::string> facts{"type " + std::to_string(s.type)};

    if (s.magic_bonus != 0)
    {
        facts.push_back("bonus " + std::to_string(s.magic_bonus));
    }

    if (s.cursed_raw != 0)
    {
        facts.push_back("cursed");
    }

    if (!scroll && s.effect_codes[0] != 0)
    {
        facts.push_back("charges " + std::to_string(s.effect_codes[0]));
    }

    if (!item.spells.empty())
    {
        facts.push_back(join(item.spells, ", "));
    }
    else if (s.effect_codes[2] != 0)
    {
        std::ostringstream special;
        special << "byte 61 " << static_cast<unsigned>(s.effect_codes[1]) << ", byte 62 0x"
                << std::hex << static_cast<unsigned>(s.effect_codes[2]);
        facts.push_back(special.str());
    }
    else if (s.effect_codes[1] != 0)
    {
        facts.push_back("power " + std::to_string(s.effect_codes[1]));
    }

    return join(facts, "; ");
}

void print(const MagicItemList &list)
{
    std::cout << list.archive << ':' << static_cast<unsigned>(list.record);

    if (!list.carrier.empty())
    {
        std::cout << "  " << list.carrier;
    }

    std::cout << "  ("
              << (list.scripts.empty() ? "no script names it by a constant"
                                       : "named by " + join(list.scripts, ", "))
              << ")\n";

    for (const auto &item : list.items)
    {
        std::cout << "  " << std::left << std::setw(34) << item.name << ' ' << details(item)
                  << '\n';
    }
}
} // namespace

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: opengold_magic_items GAME_DIRECTORY\n"
                     "Lists every magic item in the item lists (ITEMn.DAX) and creature gear "
                     "(MONnITM.DAX).\n";
        return 2;
    }

    try
    {
        const auto catalog = opengold::por::MagicItemCatalog::load(argv[1]);
        std::size_t items = 0;

        for (const auto &list : catalog.lists)
        {
            print(list);
            items += list.items.size();
        }

        std::cout << items << " magic items in " << catalog.lists.size() << " lists.\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
