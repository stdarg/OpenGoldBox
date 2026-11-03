#pragma once

#include <filesystem>
#include <string>
#include <string_view>

// A record of the game's key decisions, every die rolled and the errors met, for
// working out afterwards why something happened. The game writes it to logs/ in its
// user data folder; elsewhere it is off unless the environment variable
// OPENGOLD_LOG names a file, which the game uses instead. Each line reads:
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

// While it lives, the dice this thread rolls are logged as being for `purpose`,
// such as "TROLL 2: melee at Bryn". Each die is a debug line under "dice".
class RollPurpose
{
  public:
    explicit RollPurpose(std::string purpose);
    ~RollPurpose();
    RollPurpose(const RollPurpose &) = delete;
    RollPurpose &operator=(const RollPurpose &) = delete;

  private:
    std::string previous_;
};
} // namespace opengold
