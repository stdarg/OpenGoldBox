#include "opengold/diagnostic_log.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <mutex>
#include <string>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#if __has_include(<syslog.h>)
#include <syslog.h>
#define OPENGOLD_HAS_SYSLOG 1
#endif

namespace opengold
{
namespace
{
struct DiagnosticLog
{
    std::mutex mutex;
    std::ofstream file;
    bool enabled{};
};

void open_file(DiagnosticLog &log, const std::filesystem::path &file)
{
    log.file.close();
    log.file.clear();
    log.file.open(file, std::ios::app);
    log.enabled = log.file.is_open();
#ifdef OPENGOLD_HAS_SYSLOG
    if (log.enabled)
    {
        openlog("opengold", LOG_PID, LOG_USER);
    }
#endif
}

DiagnosticLog &diagnostic_log()
{
    static DiagnosticLog log;
    static std::once_flag read_environment;
    std::call_once(read_environment,
                   []
                   {
                       if (const char *file = std::getenv("OPENGOLD_LOG"); file && *file)
                       {
                           open_file(log, file);
                       }
                   });
    return log;
}

int process_id()
{
#ifdef _WIN32
    return _getpid();
#else
    return static_cast<int>(getpid());
#endif
}

// The current time as ISO 8601 in UTC, to the millisecond.
std::string timestamp()
{
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto seconds = system_clock::to_time_t(now);
    const auto milliseconds =
        duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &seconds);
#else
    gmtime_r(&seconds, &utc);
#endif
    std::array<char, 32> date{};
    std::strftime(date.data(), date.size(), "%Y-%m-%dT%H:%M:%S", &utc);
    std::array<char, 40> text{};
    std::snprintf(text.data(), text.size(), "%s.%03dZ", date.data(),
                  static_cast<int>(milliseconds.count()));
    return text.data();
}

std::string_view priority_name(LogPriority priority)
{
    switch (priority)
    {
    case LogPriority::error:
        return "err";
    case LogPriority::warning:
        return "warning";
    case LogPriority::notice:
        return "notice";
    case LogPriority::info:
        return "info";
    case LogPriority::debug:
        return "debug";
    }
    return "info";
}

#ifdef OPENGOLD_HAS_SYSLOG
int syslog_priority(LogPriority priority)
{
    switch (priority)
    {
    case LogPriority::error:
        return LOG_ERR;
    case LogPriority::warning:
        return LOG_WARNING;
    case LogPriority::notice:
        return LOG_NOTICE;
    case LogPriority::info:
        return LOG_INFO;
    case LogPriority::debug:
        return LOG_DEBUG;
    }
    return LOG_INFO;
}
#endif
} // namespace

void open_diagnostic_log(const std::filesystem::path &file)
{
    auto &log = diagnostic_log();
    const std::lock_guard lock(log.mutex);
    open_file(log, file);
}

bool diagnostic_log_enabled()
{
    return diagnostic_log().enabled;
}

void log_diagnostic(LogPriority priority, std::string_view topic, std::string_view message)
{
    auto &log = diagnostic_log();
    if (!log.enabled)
    {
        return;
    }
    const std::lock_guard lock(log.mutex);
    // Flushed line by line: the lines that matter most come just before a crash.
    log.file << timestamp() << " opengold[" << process_id() << "] " << priority_name(priority)
             << ' ' << topic << ": " << message << std::endl;
#ifdef OPENGOLD_HAS_SYSLOG
    const std::string line = std::string(topic) + ": " + std::string(message);
    syslog(syslog_priority(priority), "%s", line.c_str());
#endif
}
} // namespace opengold
