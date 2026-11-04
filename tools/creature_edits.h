#ifndef OPENGOLD_TOOLS_CREATURE_EDITS_H
#define OPENGOLD_TOOLS_CREATURE_EDITS_H

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
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

// A file the import writes, with its text before and after; `before` is empty
// for a file the import creates.
struct FileChange
{
    std::string path; // relative to the repository
    std::string before, after;
};

struct ImportReport
{
    std::vector<EditResult> edits;
    std::vector<FileChange> files;
    std::optional<unsigned> rules_version; // the new "0.6.N" number, when raised
};

struct ImportOptions
{
    std::filesystem::path repository;
    std::filesystem::path edits_file; // for import_edits(options) only
    // The player's install, to check fight overrides and records against the
    // original's files; without one those checks are left out.
    std::optional<std::filesystem::path> game_directory;
    bool write{true}; // false: check and report only
    std::string date; // for docs/DEVIATIONS.md, such as "2026-11-04"
};

// Applies every edit in `edits`, an edits file's text named `source` in
// errors, to the repository's data, or throws ImportError naming the edit and
// writes nothing. The Creature Stats Editor calls this with the edits it holds.
[[nodiscard]] ImportReport import_edits(const ImportOptions &options, std::string_view edits,
                                        const std::string &source);

// The same for the edits file `options.edits_file`. A missing file imports
// nothing.
[[nodiscard]] ImportReport import_edits(const ImportOptions &options);

// The keys of the stat lines (`creature` rows) in combat.rules text, sorted.
[[nodiscard]] std::vector<std::string> stat_line_keys(const std::string &rules_text);

// The lines `after` changes, as a unified diff with two lines of context, for
// showing the user an import before it is written (requirement 19).
[[nodiscard]] std::string file_diff(const std::string &before, const std::string &after);

// What the report says of each changed file: its path, or also its diff.
enum class ReportFiles
{
    paths,
    diffs
};

// The report as JSON, for the editor.
[[nodiscard]] std::string report_json(const ImportReport &report,
                                      ReportFiles files = ReportFiles::paths);
} // namespace opengold::edits
#endif
