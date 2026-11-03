// The Creature Stats Editor's importer (tools/creature_edits.cpp,
// dev_tools/docs/creature-stats-editor-interface.md), on a copy of the
// repository's files.
#include "../tools/creature_edits.h"
#include "test_support.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <string>

using namespace opengold::edits;
using opengold::test::check;
using opengold::test::rejects;
namespace fs = std::filesystem;

namespace
{
const fs::path root = OPENGOLD_SOURCE_DIR;
const std::vector<fs::path> imported_files{"data/rules/srd-5.2.1/combat.rules",
                                           "data/encounters/conversions.json",
                                           "data/companions/companions.json",
                                           "src/OpenGold.Rules.Srd5/src/srd5.cpp",
                                           "docs/SAVES.md",
                                           "docs/DEVIATIONS.md"};

std::string read(const fs::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

// A Markdown list as one line per item, without its wrapping.
std::string unwrapped(std::string text)
{
    for (auto at = text.find("\n  "); at != std::string::npos; at = text.find("\n  ", at))
    {
        text.replace(at, 3, " ");
    }
    return text;
}

// The rules version the repository has: N of "0.6.N".
unsigned shipped_version()
{
    std::smatch found;
    const auto code = read(root / "src/OpenGold.Rules.Srd5/src/srd5.cpp");
    check(std::regex_search(code, found, std::regex(R"re("opengold\.srd5", "0\.6\.(\d+)")re")),
          "The rules version is in srd5.cpp");
    return static_cast<unsigned>(std::stoul(found[1].str()));
}

// The files the importer reads and writes, copied, with an edits file.
struct Repository
{
    fs::path directory =
        fs::temp_directory_path() /
        ("opengold-creature-edits-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    Repository()
    {
        for (const auto &file : imported_files)
        {
            fs::create_directories((directory / file).parent_path());
            fs::copy_file(root / file, directory / file);
        }
    }

    Repository(const Repository &) = delete;
    Repository &operator=(const Repository &) = delete;

    ~Repository()
    {
        std::error_code ignored;
        fs::remove_all(directory, ignored);
    }

    [[nodiscard]] std::string text(const fs::path &file) const
    {
        return read(directory / file);
    }

    ImportReport import(const std::string &edits, bool write = true) const
    {
        std::ofstream(directory / "edits.json")
            << R"({"format": "opengold-creature-edits-v1", "edits": [)" << edits << "]}";
        return import_edits(
            {directory, directory / "edits.json", std::nullopt, write, "2026-11-04"});
    }

    // Whether every file is as the repository has it.
    [[nodiscard]] bool unchanged() const
    {
        return std::all_of(imported_files.begin(), imported_files.end(),
                           [&](const fs::path &file)
                           {
                               return text(file) == read(root / file);
                           });
    }
};

const std::string sturdier_knights =
    R"({"change": "hit_points", "definition": "castle-knight", "from": 52, "to": 60,
        "reason": "Sturdier guards"})";

void nothing_to_import()
{
    const Repository repository;
    const auto report = import_edits(
        {repository.directory, repository.directory / "none.json", {}, true, "2026-11-04"});
    check(report.edits.empty() && report.files_changed.empty() && repository.unchanged(),
          "Without an edits file nothing changes");
    check(repository.import("").files_changed.empty() && repository.unchanged(),
          "An empty edits list changes nothing");
}

void hit_points_edit()
{
    const Repository repository;
    const auto checked = repository.import(sturdier_knights, false);
    check(checked.files_changed.size() == 4 && repository.unchanged(),
          "A check reports the files it would change and writes none");
    const auto report = repository.import(sturdier_knights);
    check(report.edits.size() == 1 && report.edits[0].status == EditStatus::applied &&
              report.rules_version == shipped_version() + 1,
          "The edit is applied and the rules version raised");
    const auto rules = repository.text("data/rules/srd-5.2.1/combat.rules");
    check(rules.find("# Creature Stats Editor (2026-11-04): Sturdier guards\n"
                     "creature castle-knight 18 60 0 30") != std::string::npos,
          "The stat line has the new Hit Points and its reason above it");
    check(repository.text("src/OpenGold.Rules.Srd5/src/srd5.cpp")
                      .find("\"0.6." + std::to_string(shipped_version() + 1) + "\"") !=
                  std::string::npos &&
              repository.text("docs/SAVES.md")
                      .find("`opengold.srd5` 0.6." + std::to_string(shipped_version() + 1) + ",") !=
                  std::string::npos,
          "The rules version is raised in the code and in SAVES.md");
    const auto deviations = unwrapped(repository.text("docs/DEVIATIONS.md"));
    const auto entry =
        deviations.find("- **castle-knight** (2026-11-04). Sturdier guards. Creature");
    check(entry != std::string::npos && entry < deviations.find("## Party and parley") &&
              deviations.find("<!-- creature-edit hit_points castle-knight -->") !=
                  std::string::npos,
          "DEVIATIONS gets an entry under Rules and combat");
    const auto again = repository.import(sturdier_knights);
    check(again.edits[0].status == EditStatus::already_applied && again.files_changed.empty() &&
              !again.rules_version,
          "Importing the same edits again changes nothing");
    const auto higher = repository.import(
        R"({"change": "hit_points", "definition": "castle-knight", "from": 60, "to": 65,
            "reason": "Sturdier still"})");
    const auto replaced = unwrapped(repository.text("docs/DEVIATIONS.md"));
    check(higher.rules_version == shipped_version() + 2 &&
              replaced.find("Sturdier guards") == std::string::npos &&
              replaced.find("Sturdier still. Creature Stats Editor: Hit Points 60 to 65.") !=
                  std::string::npos,
          "A later edit of the same value replaces its DEVIATIONS entry");
}

void conflicts()
{
    const Repository repository;
    rejects(
        [&]
        {
            (void)repository.import(R"({"change": "hit_points", "definition": "castle-knight",
                                               "from": 87, "to": 60, "reason": "Stale"})");
        },
        "An edit whose from is not what the data holds is refused");
    rejects(
        [&]
        {
            (void)repository.import(sturdier_knights + "," +
                                    R"({"change": "hit_points", "definition": "castle-knight",
                                             "from": 87, "to": 70, "reason": "Stale"})");
        },
        "One refused edit stops the whole import");
    check(repository.unchanged(), "A refused import writes nothing");
    rejects(
        [&]
        {
            (void)repository.import(R"({"change": "hit_points", "definition": "castle-knight",
                                               "from": 52, "to": 60})");
        },
        "An edit gives its reason");
    rejects(
        [&]
        {
            (void)repository.import(R"({"change": "level", "reason": "No such change"})");
        },
        "An unknown change is refused");
    rejects(
        [&]
        {
            (void)repository.import(R"({"change": "hit_points", "definition": "nobody",
                                               "from": 1, "to": 2, "reason": "No such creature"})");
        },
        "An edit of a creature the rules lack is refused");
    rejects(
        [&]
        {
            (void)repository.import(R"({"change": "spell_profile", "definition": "castle-mage",
                                               "from": {"spell_slots": 4, "casting_bonus": 5, "level": 4},
                                               "to": {"spell_slots": 9, "casting_bonus": 9, "level": 9},
                                               "reason": "An invented profile"})");
        },
        "A spellcasting profile no creature has is refused");
    check(repository.unchanged(), "Refused imports write nothing");
}

const std::string knight_conversion =
    R"({"record": 85, "definition": "castle-knight", "fit_xp": 700, "award_xp": 700})";

void stat_line_and_conversion()
{
    const Repository repository;
    const auto report = repository.import(
        R"({"change": "new_stat_line", "definition": "tower-knight", "copy_of": "castle-knight",
            "reason": "The tower's guards alone"},
           {"change": "hit_points", "definition": "tower-knight", "from": 52, "to": 70,
            "reason": "The tower's guards"},
           {"change": "conversion", "script": 7, "record": 85, "from": )" +
        knight_conversion + R"(, "to": {"record": 85, "definition": "tower-knight", "fit_xp": 900,
            "award_xp": 700}, "reason": "The guards fight as the tower's knights"})");
    check(report.edits.size() == 3 && report.rules_version == shipped_version() + 1,
          "A copied stat line, its Hit Points and a conversion to it import together");
    const auto deviations = repository.text("docs/DEVIATIONS.md");
    check(deviations.find("<!-- creature-edit new_stat_line tower-knight -->") !=
                  std::string::npos &&
              deviations.find("<!-- creature-edit conversion 7:85 -->") != std::string::npos,
          "Each entry's marker is on one line");
    const auto rules = repository.text("data/rules/srd-5.2.1/combat.rules");
    check(rules.find("creature tower-knight 18 70 0 30") != std::string::npos &&
              rules.find("parry tower-knight 2\n") != std::string::npos &&
              rules.find("creature castle-knight 18 52 0 30") != std::string::npos,
          "The copy has the original's rows under its own key; the original is untouched");
    const auto conversions = repository.text("data/encounters/conversions.json");
    check(conversions.find(
              R"({"record": 85, "definition": "tower-knight", "fit_xp": 900, "award_xp": 700})") !=
              std::string::npos,
          "The conversion is written in the file's layout");
    (void)repository.import(R"({"change": "conversion", "script": 3, "record": 85, "from":
        {"record": 85, "definition": "tower-knight", "fit_xp": 900, "award_xp": 700}, "to": )" +
                            knight_conversion + R"(, "reason": "Back"})");
    check(repository.text("data/encounters/conversions.json") ==
              read(root / "data/encounters/conversions.json"),
          "Undoing the conversion gives back the file byte for byte");
    rejects(
        [&]
        {
            (void)repository.import(R"({"change": "conversion", "script": 7, "record": 85,
                                               "from": )" +
                                    knight_conversion +
                                    R"(, "to": {"record": 85, "definition": "nobody",
                                               "fit_xp": 1, "award_xp": 1}, "reason": "x"})");
        },
        "A conversion to a stat line the rules lack is refused");
}

void overrides_and_companions()
{
    const Repository repository;
    const std::string half =
        R"({"site": "ECL5.DAX:7:0xA60D", "original": {"record": 85, "count": 12}, "count": 6,
            "reason": "Half the guards"})";
    const auto added = repository.import(R"({"change": "override", "site": "ECL5.DAX:7:0xA60D",
        "from": null, "to": )" + half + R"(, "reason": "Half the guards"})");
    check(added.edits[0].status == EditStatus::applied && !added.rules_version &&
              repository.text("data/encounters/overrides.json")
                      .find(R"("site": "ECL5.DAX:7:0xA60D")") != std::string::npos,
          "An override is added, without raising the rules version");
    rejects(
        [&]
        {
            (void)repository.import(R"({"change": "override", "site": "ECL5.DAX:7:0xA60D",
        "from": null, "to": {"site": "ECL5.DAX:7:0xA60D", "original": {"record": 85, "count": 12},
        "count": 0, "reason": "None"}, "reason": "None"})");
        },
        "An override the game would refuse is refused");
    const std::string rare =
        R"({"site": "ECL2.DAX:20:0x9B60", "operand": 1, "original": 12, "value": 13,
            "reason": "Fewer street groups"})";
    const auto chance = repository.import(R"({"change": "chance", "site": "ECL2.DAX:20:0x9B60",
        "operand": 1, "from": null, "to": )" +
                                          rare + R"(, "reason": "Fewer street groups"})");
    check(chance.edits[0].summary == "chance number the original's to 13" &&
              repository.text("data/encounters/chances.json")
                      .find(R"("site": "ECL2.DAX:20:0x9B60")") != std::string::npos,
          "A wandering encounter's chance is changed");
    rejects(
        [&]
        {
            (void)repository.import(R"({"change": "chance", "site": "ECL2.DAX:20:0x9B60",
        "operand": 1, "from": )" + rare +
                                    R"(, "to": {"site": "ECL2.DAX:20:0x9B60", "operand": 1,
        "original": 12, "value": 300, "reason": "x"}, "reason": "x"})");
        },
        "A chance number the game would refuse is refused");
    const auto companions = read(root / "data/companions/companions.json");
    const auto start = companions.find("    {\n      \"bank\": 5,");
    check(start != std::string::npos, "Genheeris is in the shipped file");
    const std::string genheeris =
        companions.substr(start, companions.find("\n    }", start) + 6 - start);
    auto higher = genheeris;
    higher.replace(higher.find("\"level\": 6"), 10, "\"level\": 8");
    const auto raised = repository.import(R"({"change": "companion", "bank": 5, "record": 88,
        "from": )" + genheeris + R"(, "to": )" +
                                          higher + R"(, "reason": "A stronger turncoat"})");
    check(raised.edits[0].summary == "level changed" &&
              repository.text("data/companions/companions.json").find("\"level\": 8") !=
                  std::string::npos,
          "A companion's level is changed");
    (void)repository.import(R"({"change": "companion", "bank": 5, "record": 88, "from": )" +
                            higher + R"(, "to": )" + genheeris + R"(, "reason": "Back"})");
    check(repository.text("data/companions/companions.json") == companions,
          "Undoing it gives back the file byte for byte");
    auto jester = genheeris;
    jester.replace(jester.find("\"class\": \"wizard\""), 17, "\"class\": \"jester\"");
    rejects(
        [&]
        {
            (void)repository.import(R"({"change": "companion", "bank": 5, "record": 88,
        "from": )" + genheeris + R"(, "to": )" +
                                    jester + R"(, "reason": "x"})");
        },
        "A companion the rules cannot build is refused");
}
} // namespace

int main()
{
    try
    {
        nothing_to_import();
        hit_points_edit();
        conflicts();
        stat_line_and_conversion();
        overrides_and_companions();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Creature edits tests passed\n";
}
