#ifndef OPENGOLDBOX_CRASH_REPORTS_H
#define OPENGOLDBOX_CRASH_REPORTS_H

// Leaves a report a player can send after a session that did not shut down
// normally. A session marks itself running on start and clears the mark on a
// clean exit, so the next launch finds what a crash, a force quit or a power
// loss left behind. Reports stay on the player's machine in user://crash-reports.
namespace crash_reports
{
// Sends an uncaught C++ exception to the log and the engine's crash handler.
void install_terminate_handler();

// Reports earlier sessions that never ended, then marks this one running.
void begin_session();

// Clears this session's mark. Only a clean exit gets here.
void end_session();

// Ends the game with an uncaught exception, for the package smoke check.
[[noreturn]] void crash_for_check() noexcept;
} // namespace crash_reports

#endif
