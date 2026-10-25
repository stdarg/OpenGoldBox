#ifndef OPENGOLD_DOS_PROBE_SCRIPT_TRACE_H
#define OPENGOLD_DOS_PROBE_SCRIPT_TRACE_H

#include "machine.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace opengold::probe
{
// The ECL script the original is running: its archive and record, and the
// data segment whose offsets are the script's own addresses (the script sits
// at DS:9900, its variables at DS:4A00 and so on).
struct ScriptLocation
{
    std::string archive;
    unsigned record{};
    std::uint16_t data_segment{};
};

// Finds the running script by matching every ECL record's opening bytes
// against memory.
[[nodiscard]] std::optional<ScriptLocation>
find_script(const Machine &machine, const std::filesystem::path &game_directory);

// Records, while attached, every script byte the interpreter reads (as runs
// of script addresses) and every write to the script's variable cells, as
// lines of text.
class ScriptTrace
{
  public:
    ScriptTrace(Machine &machine, const ScriptLocation &location,
                const std::filesystem::path &file);
    ~ScriptTrace();
    ScriptTrace(const ScriptTrace &) = delete;
    ScriptTrace &operator=(const ScriptTrace &) = delete;

  private:
    void read(std::uint16_t address);
    void write(std::uint16_t address, std::uint8_t value);
    void flush_reads();

    Machine &machine_;
    std::uint32_t base_{};
    std::ofstream out_;
    std::optional<std::uint16_t> run_start_, run_end_;
};
} // namespace opengold::probe

#endif
