// data/encounters, which the game reads
// (dev_tools/docs/creature-stats-editor-interface.md).
#include "opengold/encounter_data.h"
#include "opengold/srd5.h"
#include "test_support.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>

using namespace opengold;
using namespace opengold::por;
using opengold::test::check;
using opengold::test::rejects;

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

std::string read(const std::filesystem::path &path)
{
    std::ifstream in(path);
    check(bool(in), "Read " + path.string());
    return {std::istreambuf_iterator<char>(in), {}};
}

// A folder of encounter files, removed afterwards.
struct Folder
{
    std::filesystem::path directory =
        std::filesystem::temp_directory_path() /
        ("opengold-encounter-data-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    Folder()
    {
        std::filesystem::create_directories(directory);
    }

    Folder(const Folder &) = delete;
    Folder &operator=(const Folder &) = delete;

    ~Folder()
    {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }

    void write(const std::string &name, const std::string &text) const
    {
        std::ofstream(directory / name) << text;
    }
};

const std::string two_areas = R"({
  "format": "opengold-conversions-v1",
  "areas": [
    {"scripts": [20], "bank": 2, "name": "The Slums", "creatures": [
      {"record": 0, "definition": "slums-kobold", "fit_xp": 25, "award_xp": 25}]},
    {"scripts": [3, 4], "bank": 5, "name": "Valjevo Castle", "creatures": [
      {"record": 85, "definition": "castle-knight", "fit_xp": 700, "award_xp": 650}]}
  ]
})";

EncounterData load_conversions(const std::string &text)
{
    Folder folder;
    folder.write("conversions.json", text);
    return load_encounter_data(folder.directory);
}

void rejected(std::string text, const std::string &from, const std::string &to,
              const std::string &message)
{
    text.replace(text.find(from), from.size(), to);
    rejects(
        [&]
        {
            (void)load_conversions(text);
        },
        message);
}

void shipped_conversions()
{
    const auto data = load_encounter_data(root / "data/encounters");
    std::size_t records = 0;
    for (const auto &area : data.areas)
    {
        records += area.creatures.size();
    }
    check(data.areas.size() == 23 && records == 195,
          "Every area's conversions moved from the code to the file");
    for (const auto script : {3U, 4U, 5U, 7U})
    {
        const auto *castle = data.area(script);
        check(castle && castle->name == "Valjevo Castle" && castle->bank == 5,
              "Valjevo Castle's four scripts share one entry");
        const auto knight = std::find_if(castle->creatures.begin(), castle->creatures.end(),
                                         [](const auto &creature)
                                         {
                                             return creature.first == 85;
                                         });
        check(knight != castle->creatures.end() && knight->second.definition == "castle-knight" &&
                  knight->second.fit_xp == 700 && knight->second.award_xp == 700,
              "Tyranthraxus's guards are Knights rated 700 XP");
    }
    const auto *slums = data.area(20);
    check(slums && slums->creatures.at(1).first == 1 &&
              slums->creatures.at(1).second.fit_xp == 25 &&
              slums->creatures.at(1).second.award_xp == 50,
          "The kobold leader is sized as a kobold and awards double");
    check(!data.area(19), "The dragon's cave converts no creatures of its own");
}

// Every stat line the file names is one combat.rules defines.
void shipped_definitions_exist()
{
    const auto rules =
        srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                            "\ncreature target 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
    std::set<std::string> definitions{"no-such-creature"};
    for (const auto &area : load_encounter_data(root / "data/encounters").areas)
    {
        for (const auto &creature : area.creatures)
        {
            definitions.insert(creature.second.definition);
        }
    }
    for (const auto &definition : definitions)
    {
        const bool known = !test::throws(
            [&]
            {
                (void)rules->create({{10, 4, std::vector<rules::Terrain>(40)},
                                     {{1, definition, "Monster", rules::Side::opposition, {1, 1}},
                                      {2, "target", "Hero", rules::Side::party, {5, 1}}}},
                                    7);
            });
        check(known == (definition != "no-such-creature"), "combat.rules defines " + definition);
    }
}

void edited_conversions()
{
    const auto data = load_conversions(two_areas);
    check(data.areas.size() == 2 && data.area(4) == data.area(3) && !data.area(5),
          "An entry applies to each script it lists");
    const auto &knight = data.area(3)->creatures.at(0);
    check(knight.first == 85 && knight.second.definition == "castle-knight" &&
              knight.second.fit_xp == 700 && knight.second.award_xp == 650,
          "A fight's rating and its award are read separately");
}

void broken_conversions()
{
    rejects(
        []
        {
            (void)load_encounter_data(root / "no-such-folder");
        },
        "The game needs the conversions");
    rejected(two_areas, "conversions-v1", "conversions-v2", "An unknown format is refused");
    rejected(two_areas, "\"scripts\": [3, 4]", "\"scripts\": [3, 20]",
             "A script in two entries is refused");
    rejected(two_areas, "\"scripts\": [3, 4]", "\"scripts\": []", "An entry needs a script");
    rejected(two_areas, "\"fit_xp\": 700", "\"fit_xp\": 70.5", "A fractional XP is refused");
    rejected(two_areas, "\"award_xp\": 650", "\"award_xp\": 1000001",
             "An XP above a million is refused");
    rejected(two_areas, "\"award_xp\": 650", "\"award_xp\": -1", "A negative XP is refused");
    rejected(two_areas, "\"record\": 85", "\"record\": 256",
             "A record beyond the archive is refused");
    rejected(two_areas, "\"definition\": \"castle-knight\"", "\"definition\": \"\"",
             "A record needs a stat line");
    rejected(two_areas, ", \"fit_xp\": 700", "", "A record needs its fight rating");
    rejected(two_areas, "\"bank\": 5", "\"bank\": 0", "A bank names a MONnCHA archive");
    rejected(two_areas, "\"award_xp\": 650}",
             "\"award_xp\": 650}, {\"record\": 85, \"definition\": \"troll\", \"fit_xp\": 1, "
             "\"award_xp\": 1}",
             "A record listed twice in an area is refused");
}

const std::string one_override = R"({
  "format": "opengold-encounter-overrides-v1",
  "overrides": [
    {"site": "ECL5.DAX:7:0xA60D", "original": {"record": 85, "count": 12},
     "count": 6, "reason": "Half the guards"}
  ]
})";

EncounterData load_overrides(const std::string &text)
{
    Folder folder;
    folder.write("conversions.json", two_areas);
    folder.write("overrides.json", text);
    return load_encounter_data(folder.directory);
}

void rejected_override(std::string text, const std::string &from, const std::string &to,
                       const std::string &message)
{
    text.replace(text.find(from), from.size(), to);
    rejects(
        [&]
        {
            (void)load_overrides(text);
        },
        message);
}

void edited_overrides()
{
    check(load_conversions(two_areas).overrides.empty(), "Without overrides.json nothing changes");
    const auto data = load_overrides(one_override);
    check(data.overrides.size() == 1, "The override is read");
    const auto &change = data.overrides.front();
    check(change.site == EncounterSite{5, 7, 0xA60D} &&
              site_name(change.site) == "ECL5.DAX:7:0xA60D",
          "A site is the script's archive, record and address");
    check(change.original_record == 85 && change.original_count == 12 && !change.record &&
              change.count == 6u && !change.icon && change.reason == "Half the guards",
          "A value left out stays the original's");
    check(site_name({2, 20, 0x9E65}) == "ECL2.DAX:20:0x9E65", "A site is named in upper-case hex");
}

void broken_overrides()
{
    rejected_override(one_override, "overrides-v1", "overrides-v2", "An unknown format is refused");
    rejected_override(one_override, "ECL5.DAX:7:0xA60D", "ECL5:7:0xA60D",
                      "A malformed site is refused");
    rejected_override(one_override, "ECL5.DAX:7:0xA60D", "ECL5.DAX:7:A60D",
                      "A site's address is hex");
    rejected_override(one_override, "ECL5.DAX:7:0xA60D", "ECL5.DAX:256:0xA60D",
                      "A site's script is a record");
    rejected_override(one_override, "\"count\": 6", "\"count\": 0", "An override loads someone");
    rejected_override(one_override, "\"count\": 6", "\"count\": 251",
                      "An override loads at most 250");
    rejected_override(one_override, "\"count\": 6, ", "", "An override changes something");
    rejected_override(one_override, "\"reason\": \"Half the guards\"", "\"reason\": \"\"",
                      "An override gives its reason");
    rejected_override(one_override, "\"original\": {\"record\": 85, \"count\": 12},", "",
                      "An override names what the original loads");
    rejected_override(one_override, "\"reason\": \"Half the guards\"}",
                      "\"reason\": \"Half the guards\"}, {\"site\": \"ECL5.DAX:7:0xA60D\", "
                      "\"original\": {\"record\": 85, \"count\": 12}, \"icon\": 3, "
                      "\"reason\": \"Again\"}",
                      "A site listed twice is refused");
}

const std::string one_chance = R"({
  "format": "opengold-encounter-chances-v1",
  "chances": [
    {"site": "ECL2.DAX:20:0x9B60", "operand": 1, "original": 12, "value": 13,
     "reason": "Fewer street groups"}
  ]
})";

EncounterData load_chances(const std::string &text)
{
    Folder folder;
    folder.write("conversions.json", two_areas);
    folder.write("chances.json", text);
    return load_encounter_data(folder.directory);
}

void rejected_chance(const std::string &from, const std::string &to, const std::string &message)
{
    auto text = one_chance;
    text.replace(text.find(from), from.size(), to);
    rejects(
        [&]
        {
            (void)load_chances(text);
        },
        message);
}

void chances()
{
    check(load_conversions(two_areas).chances.empty(), "Without chances.json no chance changes");
    const auto data = load_chances(one_chance);
    check(data.chances.size() == 1 && data.chances[0].site == EncounterSite{2, 20, 0x9B60} &&
              data.chances[0].operand == 1 && data.chances[0].original == 12 &&
              data.chances[0].value == 13 && data.chances[0].reason == "Fewer street groups",
          "A chance names its instruction, operand, the original's number and the new one");
    rejected_chance("chances-v1", "chances-v2", "An unknown format is refused");
    rejected_chance("\"operand\": 1", "\"operand\": 2", "A chance is operand 0 or 1");
    rejected_chance("\"value\": 13", "\"value\": 256", "A chance number is a byte");
    rejected_chance("\"reason\": \"Fewer street groups\"", "\"reason\": \"\"",
                    "A chance gives its reason");
    rejected_chance("ECL2.DAX:20:0x9B60", "ECL2.DAX:20", "A malformed site is refused");
    rejected_chance("\"reason\": \"Fewer street groups\"}",
                    "\"reason\": \"Fewer street groups\"}, {\"site\": \"ECL2.DAX:20:0x9B60\", "
                    "\"operand\": 1, \"original\": 12, \"value\": 9, \"reason\": \"Again\"}",
                    "An operand listed twice is refused");
}
} // namespace

int main()
{
    try
    {
        shipped_conversions();
        shipped_definitions_exist();
        edited_conversions();
        broken_conversions();
        edited_overrides();
        broken_overrides();
        chances();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Encounter data tests passed\n";
}
