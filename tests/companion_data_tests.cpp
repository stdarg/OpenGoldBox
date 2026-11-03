// data/companions, which the Creature Stats Editor writes and the game builds
// the original's NPCs who join the party from
// (dev_tools/docs/creature-stats-editor-interface.md).
#include "opengold/companions.h"
#include "opengold/srd5.h"
#include "test_support.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace opengold;
using opengold::test::check;
using opengold::test::rejects;

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

// What a built companion is: who, its level, Hit Points, scores, spells and
// items.
std::string sheet_of(const Character &companion)
{
    const auto &sheet = companion.sheet();
    std::ostringstream out;
    out << sheet.name << ' ' << sheet.race << ' ' << sheet.gender << ' ' << sheet.character_class
        << ' ' << sheet.alignment << ' ' << sheet.background << " L" << sheet.level << " HP"
        << sheet.hit_points << " scores";
    for (const auto ability : rules::all_abilities)
    {
        out << ' ' << sheet.scores[ability];
    }
    out << " grants " << sheet.grants.size() << " spells";
    for (const auto &spell : sheet.prepared_spells)
    {
        out << ' ' << spell;
    }
    out << " items";
    for (const auto &item : companion.inventory().items())
    {
        out << " [" << item.definition_id << '|' << item.name << '|' << item.quantity << '|'
            << item.original_type << ']';
    }
    return out.str();
}

// Each companion as the code built them before their definitions moved to
// data/companions (2026-11-03).
void shipped_companions()
{
    const auto rules = srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
    const auto creation = srd5::character_rules();
    const auto companions = load_companions(root / "data/companions");
    const std::vector<std::tuple<unsigned, unsigned, std::string>> expected{
        {4, 27,
         "Skullcrusher Human Male Fighter Neutral Soldier L4 HP44 scores 18 16 18 10 8 12 grants "
         "21 spells items"},
        {2, 25,
         "Mad Man Human Male Fighter Neutral Soldier L1 HP10 scores 17 10 10 10 10 10 grants 14 "
         "spells items"},
        {3, 107,
         "Dirten Human Male Cleric Lawful Good Acolyte L5 HP23 scores 10 10 9 15 16 15 grants 17 "
         "spells inflict_wounds healing_word shield_of_faith blindness "
         "protection_from_evil_and_good command guiding_bolt bane hold_person items "
         "[mace|Mace|1|23] [chain_shirt|Chain Shirt|1|-1]"},
        {8, 104,
         "Princess Fatima Human Female Fighter Lawful Good Soldier L4 HP40 scores 18 18 16 14 10 "
         "15 grants 21 spells items [shortsword|Shortsword|1|37] [leather|Leather Armor|1|50] "
         "[shield|Shield|1|59]"},
        {4, 24,
         "Mystic Human Male Wizard Neutral Good Sage L6 HP26 scores 10 10 10 15 10 10 grants 29 "
         "spells magic_missile protection_from_evil_and_good longstrider scorching_ray blindness "
         "fog_cloud hold_person burning_hands thunderwave shatter items "
         "[ring_of_protection+2|Ring of Protection +2|1|93]"},
        {5, 88,
         "Genheeris Human Male Wizard Neutral Evil Sage L6 HP26 scores 10 10 10 18 10 10 grants 29 "
         "spells magic_missile protection_from_evil_and_good longstrider scorching_ray blindness "
         "fog_cloud hold_person burning_hands thunderwave shatter items"}};
    check(companions.size() == expected.size(), "Every companion moved from the code to the file");
    for (const auto &[bank, record, sheet] : expected)
    {
        const auto companion = original_companion(bank, record, companions, *creation, *rules);
        check(companion && sheet_of(*companion) == sheet,
              "MON" + std::to_string(bank) + "CHA " + std::to_string(record) +
                  " is built as before: " + (companion ? sheet_of(*companion) : "none"));
    }
    check(!original_companion(3, 24, companions, *creation, *rules),
          "A record with no definition has no companion");
}

// A folder holding one companions.json, removed afterwards.
struct Folder
{
    std::filesystem::path directory =
        std::filesystem::temp_directory_path() /
        ("opengold-companion-data-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    explicit Folder(const std::string &text)
    {
        std::filesystem::create_directories(directory);
        std::ofstream(directory / "companions.json") << text;
    }

    Folder(const Folder &) = delete;
    Folder &operator=(const Folder &) = delete;

    ~Folder()
    {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
};

const std::string one_companion = R"({
  "format": "opengold-companions-v1",
  "companions": [
    {"bank": 5, "record": 88, "name": "Genheeris", "race": "human", "gender": "male",
     "class": "wizard", "alignment": "neutral_evil", "background": "sage",
     "rolled": [10, 10, 9, 14, 10, 10],
     "background_increase": {"plus_two": "intelligence", "plus_one": "constitution"},
     "level": 8, "items": [{"key": "dagger", "name": "Dagger", "original_type": 12}]}
  ]
})";

std::vector<CompanionDefinition> load(const std::string &text)
{
    const Folder folder(text);
    return load_companions(folder.directory);
}

void rejected(const std::string &from, const std::string &to, const std::string &message)
{
    auto text = one_companion;
    text.replace(text.find(from), from.size(), to);
    rejects(
        [&]
        {
            (void)load(text);
        },
        message);
}

void edited_companion()
{
    const auto companions = load(one_companion);
    check(companions.size() == 1 && companions[0].level == 8 &&
              companions[0].plus_two == rules::Ability::intelligence &&
              companions[0].items.size() == 1 && companions[0].items[0].quantity == 1 &&
              companions[0].items[0].original_type == 12,
          "An edited level and item are read; an item's quantity defaults to 1");
    const auto rules = srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
    const auto creation = srd5::character_rules();
    const auto built = build_companion(companions[0], *creation, *rules);
    check(built.sheet().level == 8 && built.inventory().items().size() == 1 &&
              built.inventory().items()[0].definition_id == "dagger",
          "A companion is built at the edited level with the edited gear");
    auto unknown_class = companions[0];
    unknown_class.character_class = "jester";
    rejects(
        [&]
        {
            (void)build_companion(unknown_class, *creation, *rules);
        },
        "The rules refuse an unknown class");
}

void broken_companions()
{
    rejected("companions-v1", "companions-v2", "An unknown format is refused");
    rejected("\"level\": 8", "\"level\": 16", "A level beyond the classes' highest is refused");
    rejected("\"level\": 8", "\"level\": 0", "A companion has a level");
    rejected("[10, 10, 9, 14, 10, 10]", "[10, 10, 9, 14, 10]", "Six scores are rolled");
    rejected("[10, 10, 9, 14, 10, 10]", "[10, 10, 2, 14, 10, 10]", "A score is rolled from 3");
    rejected("[10, 10, 9, 14, 10, 10]", "[10, 10, 19, 14, 10, 10]", "A score is rolled up to 18");
    rejected("\"plus_one\": \"constitution\"", "\"plus_one\": \"intelligence\"",
             "The background raises two abilities");
    rejected("\"plus_one\": \"constitution\"", "\"plus_one\": \"luck\"",
             "An unknown ability is refused");
    rejected("\"class\": \"wizard\"", "\"class\": \"\"", "A companion has a class");
    rejected("\"key\": \"dagger\"", "\"key\": \"\"", "An item has a key");
    rejected("\"original_type\": 12", "\"original_type\": 256", "An original item type is a byte");
    rejected("\"items\": [",
             "\"items\": [{\"key\": \"dagger\", \"name\": \"Dagger\", \"quantity\": 0}, ",
             "An item has a quantity");
    auto twice = one_companion;
    const auto entry_start = twice.find("{\"bank\"");
    const auto entry_end = twice.find("]}", entry_start) + 2;
    twice.insert(entry_end, ",\n    " + twice.substr(entry_start, entry_end - entry_start));
    rejects(
        [&]
        {
            (void)load(twice);
        },
        "A record with two companions is refused");
}
} // namespace

int main()
{
    try
    {
        shipped_companions();
        edited_companion();
        broken_companions();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Companion data tests passed\n";
}
