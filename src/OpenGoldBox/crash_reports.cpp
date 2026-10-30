#include "crash_reports.h"
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <signal.h>
#endif

using namespace godot;

namespace
{
constexpr auto sessions_folder = "user://sessions";
constexpr auto reports_folder = "user://crash-reports";
constexpr auto logs_folder = "user://logs";
constexpr int kept_reports = 10;
constexpr int reported_log_lines = 400;

[[nodiscard]] String session_mark(int64_t process_id)
{
    return String(sessions_folder).path_join(String::num_int64(process_id) + ".running");
}

// OS::is_process_running knows only the processes this game launched, and the
// other session here is a separate copy of the game.
[[nodiscard]] bool process_is_alive(int32_t process_id)
{
#ifdef _WIN32
    const auto process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(process_id));
    if (process == nullptr)
    {
        return false;
    }
    const auto alive = WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
    CloseHandle(process);
    return alive;
#else
    return kill(process_id, 0) == 0 || errno == EPERM;
#endif
}

void log_uncaught_exception()
{
    const auto current = std::current_exception();
    if (!current)
    {
        UtilityFunctions::printerr("OpenGoldBox: terminated without an exception");
        return;
    }
    try
    {
        std::rethrow_exception(current);
    }
    catch (const std::exception &failure)
    {
        UtilityFunctions::printerr(String("OpenGoldBox: uncaught exception: ") +
                                   String::utf8(failure.what()));
    }
    catch (...)
    {
        UtilityFunctions::printerr("OpenGoldBox: uncaught exception of an unknown type");
    }
}

[[noreturn]] void terminate_through_crash_handler()
{
    log_uncaught_exception();
    // The engine's crash handler dumps the stack on SIGSEGV, not on the abort
    // that would otherwise end a terminate.
    std::raise(SIGSEGV);
    std::abort();
}

[[noreturn]] void throw_for_check()
{
    throw std::runtime_error("Deliberate failure for the crash report check");
}

// The log of the session before this one. The engine renames godot.log with a
// date when a new session starts, so that is the latest dated file.
[[nodiscard]] String previous_session_log()
{
    auto names = DirAccess::get_files_at(logs_folder);
    names.sort();
    for (int64_t index = names.size() - 1; index >= 0; --index)
    {
        if (names[index] != "godot.log")
        {
            return FileAccess::get_file_as_string(String(logs_folder).path_join(names[index]));
        }
    }
    return {};
}

[[nodiscard]] String log_tail(const String &log)
{
    const auto lines = log.split("\n");
    const auto first_line =
        lines.size() > reported_log_lines ? lines.size() - reported_log_lines : 0;
    return String("\n").join(lines.slice(first_line));
}

[[nodiscard]] String report_text(const String &session_start)
{
    auto *os = OS::get_singleton(); // borrowed engine singleton
    const Dictionary engine_version = Engine::get_singleton()->get_version_info();
    return String("OpenGoldBox crash report\n") +
           "The session below did not shut down normally.\n\n" +
           "Game version: " + OPENGOLDBOX_VERSION + "\n" +
           "Engine: " + String(engine_version["string"]) + "\n" + "System: " + os->get_name() +
           " " + os->get_version() + ", " + os->get_model_name() + ", " + os->get_processor_name() +
           "\n" + "Session started: " + session_start + "\n" +
           "Report written: " + Time::get_singleton()->get_datetime_string_from_system() +
           "\n\nLast lines of the session's log:\n\n" + log_tail(previous_session_log());
}

void remove_oldest_reports()
{
    auto names = DirAccess::get_files_at(reports_folder);
    names.sort();
    for (int64_t index = 0; index < names.size() - kept_reports; ++index)
    {
        DirAccess::remove_absolute(String(reports_folder).path_join(names[index]));
    }
}

// Reports must never get in the way of playing, so a folder that cannot be
// written costs only the report.
void write_report(const String &process_id, const String &session_start)
{
    const auto now = Time::get_singleton()->get_datetime_string_from_system().replace(":", "-");
    const auto path = String(reports_folder).path_join("crash-" + now + "-" + process_id + ".txt");
    DirAccess::make_dir_recursive_absolute(reports_folder);
    const auto file = FileAccess::open(path, FileAccess::WRITE);
    if (file.is_null())
    {
        UtilityFunctions::push_warning("Cannot write the crash report " + path);
        return;
    }
    file->store_string(report_text(session_start));
    file->close();
    remove_oldest_reports();
    UtilityFunctions::print(
        "OpenGoldBox: the last session did not shut down normally; report: " +
        OS::get_singleton()->get_user_data_dir().path_join(path.trim_prefix("user://")));
}

void report_unfinished_sessions()
{
    const auto own_process = OS::get_singleton()->get_process_id();
    const auto marks = DirAccess::get_files_at(sessions_folder);
    for (int64_t index = 0; index < marks.size(); ++index)
    {
        const auto process_id = marks[index].get_basename();
        const auto marked_process = static_cast<int32_t>(process_id.to_int());
        // A second copy of the game may be running. A process id reused by this
        // session can only belong to an earlier, ended one.
        if (marked_process != own_process && process_is_alive(marked_process))
        {
            continue;
        }
        const auto mark = String(sessions_folder).path_join(marks[index]);
        write_report(process_id, FileAccess::get_file_as_string(mark));
        DirAccess::remove_absolute(mark);
    }
}
} // namespace

namespace crash_reports
{
void install_terminate_handler()
{
    std::set_terminate(terminate_through_crash_handler);
}

void begin_session()
{
    DirAccess::make_dir_recursive_absolute(sessions_folder);
    report_unfinished_sessions();
    const auto mark =
        FileAccess::open(session_mark(OS::get_singleton()->get_process_id()), FileAccess::WRITE);
    if (mark.is_null())
    {
        UtilityFunctions::push_warning("Cannot mark this session as running");
        return;
    }
    mark->store_string(Time::get_singleton()->get_datetime_string_from_system());
}

void end_session()
{
    DirAccess::remove_absolute(session_mark(OS::get_singleton()->get_process_id()));
}

void crash_for_check() noexcept
{
    throw_for_check();
}
} // namespace crash_reports
