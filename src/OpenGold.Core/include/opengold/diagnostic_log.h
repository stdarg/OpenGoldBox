#pragma once

#include <filesystem>
#include <string_view>

// A record of the game's key decisions, for working out afterwards why something
// happened. It is off unless the environment variable OPENGOLD_LOG names a file.
// Each line reads:
//
//     2026-11-01T18:10:03.123Z opengold[4242] info event: script 26 begins
//
// Where the system has syslog(3), every line also goes there as facility LOG_USER,
// ident "opengold". On macOS: log show --last 1h --info --debug --predicate
// 'senderImagePath CONTAINS "libsystem_c"' | grep opengold
namespace opengold
{
// The syslog priorities the game uses.
enum class LogPriority
{
    error,
    warning,
    notice,
    info,
    debug
};

// Starts logging to this file, appending, whatever OPENGOLD_LOG says.
void open_diagnostic_log(const std::filesystem::path &file);

// Callers test this first, so that a log that is off costs no string building.
[[nodiscard]] bool diagnostic_log_enabled();

// Writes one line. Does nothing when the log is off.
void log_diagnostic(LogPriority priority, std::string_view topic, std::string_view message);
} // namespace opengold
