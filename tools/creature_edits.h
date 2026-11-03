#ifndef OPENGOLD_TOOLS_CREATURE_EDITS_H
#define OPENGOLD_TOOLS_CREATURE_EDITS_H

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// Imports the Creature Stats Editor's edits into the game's data
// (dev_tools/docs/creature-stats-editor-interface.md): the editor writes a
// list of changes in dev_tools/, and this applies them to data/ with the
// game's own loaders checking the result.
namespace opengold::edits
{
// An edit that cannot be applied, or a result the game would refuse. Nothing
// is written when one is thrown.
struct ImportError : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

enum class EditStatus
{
    applied,
    already_applied, // the data already holds the edit's `to`
};

struct EditResult
{
    std::string change, subject, summary;
    EditStatus status{};
};

struct ImportReport
{
    std::vector<EditResult> edits;
    std::vector<std::string> files_changed; // relative to the repository
    std::optional<unsigned> rules_version;  // the new "0.6.N" number, when raised
};

struct ImportOptions
{
    std::filesystem::path repository;
    std::filesystem::path edits_file;
    // The player's install, to check fight overrides and records against the
    // original's files; without one those checks are left out.
    std::optional<std::filesystem::path> game_directory;
    bool write{true}; // false: check and report only
    std::string date; // for docs/DEVIATIONS.md, such as "2026-11-04"
};

// Applies every edit in `options.edits_file` to the repository's data, or
// throws ImportError naming the edit and writes nothing. A missing edits file
// imports nothing.
[[nodiscard]] ImportReport import_edits(const ImportOptions &options);

// The report as JSON, for the editor.
[[nodiscard]] std::string report_json(const ImportReport &report);
} // namespace opengold::edits
#endif
