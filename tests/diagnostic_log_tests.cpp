#include "opengold/diagnostic_log.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

namespace
{
void check(bool condition, const std::string &what)
{
    if (!condition)
    {
        std::cerr << what << '\n';
        std::exit(1);
    }
}

std::vector<std::string> lines_of(const std::filesystem::path &file)
{
    std::vector<std::string> lines;
    std::ifstream in(file);
    for (std::string line; std::getline(in, line);)
    {
        lines.push_back(line);
    }
    return lines;
}
} // namespace

int main()
{
    using namespace opengold;
    const auto folder = std::filesystem::temp_directory_path() / "opengold_diagnostic_log_tests";
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    const auto file = folder / "game.log";

    check(!diagnostic_log_enabled(), "The log is off unless asked for");
    log_diagnostic(LogPriority::info, "event", "nobody reads this");
    check(!std::filesystem::exists(file), "A log that is off writes nothing");

    open_diagnostic_log(file);
    check(diagnostic_log_enabled(), "Opening a log turns it on");
    log_diagnostic(LogPriority::info, "event", "script 26 begins");
    log_diagnostic(LogPriority::error, "script", "rolled back");

    const auto lines = lines_of(file);
    check(lines.size() == 2, "Each message is one line");
    const std::regex timestamped(
        R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z opengold\[\d+\] info event: script 26 begins$)");
    check(std::regex_match(lines[0], timestamped),
          "A line has a UTC timestamp, the process, its priority and its topic: " + lines[0]);
    check(lines[1].find(" err script: rolled back") != std::string::npos,
          "Priorities use syslog's names: " + lines[1]);

    open_diagnostic_log(file);
    log_diagnostic(LogPriority::debug, "event", "third");
    check(lines_of(file).size() == 3, "A log is appended to, so a run keeps its history");

    std::cout << "Diagnostic log tests passed\n";
    return 0;
}
