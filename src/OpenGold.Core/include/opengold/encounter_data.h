#ifndef OPENGOLD_ENCOUNTER_DATA_H
#define OPENGOLD_ENCOUNTER_DATA_H

#include <compare>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace opengold::por
{
// What an original creature record fights as: its combat creature, the XP of
// that stat block (which sizes encounters) and the experience it awards.
struct EncounterConversion
{
    std::string definition;
    unsigned fit_xp{}, award_xp{};
};

// The original creature records that one or more area scripts convert, as
// data/encounters/conversions.json lists them. Valjevo Castle's four scripts
// share one entry.
struct AreaConversions
{
    std::vector<unsigned> scripts;
    unsigned bank{}; // the MONnCHA archive the records are in
    std::string name;
    std::vector<std::pair<unsigned, EncounterConversion>> creatures; // by record, in file order
};

// A place an original script loads monsters: the LOAD MONSTER instruction at
// `address` in ECL<bank>.DAX record `script`.
struct EncounterSite
{
    unsigned bank{}, script{};
    std::uint16_t address{};
    auto operator<=>(const EncounterSite &) const = default;
};

// The site's name in the files, such as "ECL5.DAX:7:0xA60D".
[[nodiscard]] std::string site_name(const EncounterSite &site);

// What a LOAD MONSTER loads instead of the original's, as
// data/encounters/overrides.json lists it. The original's record and count
// are kept as a check against the player's script; a value left out stays the
// original's. The count is still sized to the party.
struct EncounterOverride
{
    EncounterSite site;
    unsigned original_record{}, original_count{};
    std::optional<unsigned> record, count, icon;
    std::string reason;
};

// A number an original script sets a wandering encounter's chance with, as
// data/encounters/chances.json changes it: the literal operand `operand` of the
// instruction at `site`, a RANDOM's range, the number a COMPARE tests a roll
// against, or a SAVE of the rest check's interval or chance. `original` is
// checked against the player's script.
struct ChanceOverride
{
    EncounterSite site;
    unsigned operand{}, original{}, value{};
    std::string reason;
};

// The encounter data the game reads from data/encounters, which the Creature
// Stats Editor's edits reach through opengold_import_edits
// (dev_tools/docs/creature-stats-editor-interface.md).
struct EncounterData
{
    std::vector<AreaConversions> areas;
    std::vector<EncounterOverride> overrides; // none without overrides.json
    std::vector<ChanceOverride> chances;      // none without chances.json

    // The entry that lists `script`, or null.
    [[nodiscard]] const AreaConversions *area(unsigned script) const;
};

// Reads data/encounters: conversions.json, and overrides.json and
// chances.json where there are. Throws std::runtime_error, naming the file, the entry and the
// field, on a file the game cannot use.
[[nodiscard]] EncounterData load_encounter_data(const std::filesystem::path &folder);
} // namespace opengold::por
#endif
