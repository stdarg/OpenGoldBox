#include "opengold/magic_item_catalog.h"
#include "test_support.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string_view>

using namespace opengold::por;

namespace
{
using Bytes = std::vector<std::uint8_t>;
using opengold::test::check;

constexpr std::size_t word_slot = 21, spell_slot = 37;

void append_slot(Bytes &image, std::string_view text, std::size_t slot)
{
    image.push_back(static_cast<std::uint8_t>(text.size()));
    image.insert(image.end(), text.begin(), text.end());
    image.insert(image.end(), slot - 1 - text.size(), 0);
}

// Item words as START.EXE lays them out: slot k holds word k + 1. Words 60 and 61
// are empty, as some are in the original.
Bytes word_table(std::size_t slots = 255)
{
    Bytes image(40, 0x90);

    for (std::size_t slot = 0; slot < slots; ++slot)
    {
        const auto word = slot + 1;
        const std::string text = word == 1    ? "Battle Axe"
                                 : word == 2  ? "Hand Axe"
                                 : word == 60 ? ""
                                 : word == 61 ? ""
                                              : "Word" + std::to_string(word);
        append_slot(image, text, word_slot);
    }

    return image;
}

Bytes spell_table()
{
    Bytes image(17, 0);

    for (const auto *spell : {"Bless", "Curse", "Cure Light Wounds", ""})
    {
        append_slot(image, spell, spell_slot);
    }

    return image;
}

ItemRecord item(std::array<std::uint8_t, 3> components)
{
    ItemRecord record;
    record.name_components = components;
    return record;
}

void name_tables()
{
    const auto words = decode_item_words(word_table());
    check(words && words->size() == 256, "One word per name component");
    check((*words)[0].empty() && (*words)[1] == "Battle Axe" && (*words)[2] == "Hand Axe" &&
              (*words)[3] == "Word3" && (*words)[255] == "Word255",
          "Words are indexed by component");
    check((*words)[60].empty() && (*words)[62] == "Word62", "Empty slots stay in the table");

    check(item_name(item({3, 2, 1}), *words) == "Battle Axe Hand Axe Word3",
          "Components read last to first");
    check(item_name(item({0, 60, 2}), *words) == "Hand Axe", "Unused and empty words are skipped");

    check(!decode_item_words(Bytes(400, 0)), "No table without its first entries");
    check(!decode_item_words(word_table(200)), "A table cut short is rejected");
    auto corrupt = word_table();
    corrupt[40 + 99 * word_slot + 1] = 7;
    check(!decode_item_words(corrupt), "A non-printable word is rejected");

    const auto spells = decode_spell_names(spell_table());
    check(spells && *spells == std::vector<std::string>{"", "Bless", "Curse", "Cure Light Wounds"},
          "Spells are numbered from 1 and end at the first empty slot");
}

void magic_items()
{
    ItemRecord sword;
    sword.type = 36;
    check(!is_magic(sword), "A plain sword is mundane");
    sword.magic_bonus = 1;
    check(is_magic(sword), "An enchanted sword is magic");

    ItemRecord cursed;
    cursed.cursed_raw = 1;
    check(is_magic(cursed), "A cursed item is magic");

    ItemRecord potion;
    potion.effect_codes = {1, 85, 0};
    check(is_magic(potion), "A potion with a dose is magic");

    ItemRecord boulder;
    boulder.effect_codes = {0, 0, 135};
    check(!is_magic(boulder), "A giant's boulder is not magic");
}

const MagicItemList *find_list(const MagicItemCatalog &catalog, std::string_view archive,
                               std::uint8_t record)
{
    const auto found = std::find_if(catalog.lists.begin(), catalog.lists.end(),
                                    [&](const auto &list)
                                    {
                                        return list.archive == archive && list.record == record;
                                    });
    return found == catalog.lists.end() ? nullptr : &*found;
}

bool holds(const MagicItemList &list, std::string_view name)
{
    return std::any_of(list.items.begin(), list.items.end(),
                       [&](const auto &item)
                       {
                           return item.name == name;
                       });
}

// The facts docs/MAGIC_ITEMS.md reports, against the user's original files.
void installed()
{
    const char *directory = std::getenv("OPENGOLD_GAME_DIR");

    if (!directory)
    {
        std::cout << "Installed-game checks skipped (set OPENGOLD_GAME_DIR).\n";
        return;
    }

    const auto catalog = MagicItemCatalog::load(directory);
    check(catalog.item_words.size() == 256 && catalog.item_words[1] == "Battle Axe" &&
              catalog.item_words[255] == "Cursed",
          "START.EXE item words");
    check(catalog.spell_names.size() == 57 && catalog.spell_names[56] == "Restoration",
          "START.EXE spells 1 to 56");

    std::size_t items = 0;

    for (const auto &list : catalog.lists)
    {
        items += list.items.size();
    }

    check(items == 209 && catalog.lists.size() == 79, "209 magic items in 79 lists");

    const auto *commission = find_list(catalog, "ITEM3.DAX", 51);
    check(commission && commission->scripts == std::vector<std::string>{"ECL3.DAX:8"} &&
              holds(*commission, "Two-Handed Sword +1 +3 vs Undead"),
          "City Hall's graveyard commission");
    check(std::count_if(commission->items.begin(), commission->items.end(),
                        [](const auto &item)
                        {
                            return item.spells ==
                                   std::vector<std::string>{"Restoration", "Restoration"};
                        }) == 4,
          "Four Restoration scrolls come with the commission");

    const auto *kobold = find_list(catalog, "MON2ITM.DAX", 11);
    check(kobold && kobold->carrier == "KOBOLD LEADER" && holds(*kobold, "Bracers AC 6") &&
              std::find(kobold->scripts.begin(), kobold->scripts.end(), "ECL2.DAX:20") !=
                  kobold->scripts.end(),
          "The Slums kobold leader's bracers");

    const auto *graveyard = find_list(catalog, "ITEM4.DAX", 28);
    check(graveyard != nullptr, "The graveyard's scattered objects");
    const auto wand = std::find_if(graveyard->items.begin(), graveyard->items.end(),
                                   [](const auto &item)
                                   {
                                       return item.name == "Wand of Lightning Bolt";
                                   });
    check(wand != graveyard->items.end() && wand->stored.effect_codes[0] == 3 &&
              wand->spells == std::vector<std::string>{"Lightning Bolt"},
          "A charged wand's spell and charges");

    for (const std::uint8_t shop : {52, 53, 54, 55})
    {
        check(!find_list(catalog, "ITEM3.DAX", shop), "New Phlan's shops sell no magic items");
    }
}
} // namespace

int main()
{
    try
    {
        name_tables();
        magic_items();
        installed();
        std::cout << "Magic item tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
