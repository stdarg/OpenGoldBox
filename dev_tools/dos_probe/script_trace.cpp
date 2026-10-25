#include "script_trace.h"

#include "opengold/formats.h"

#include <iomanip>
#include <sstream>

namespace opengold::probe
{
namespace
{
constexpr std::uint16_t script_origin = 0x9900;
constexpr std::uint16_t script_limit = 0xB700;
// Cells the scripts use as variables: the campaign's flags, the engine's
// working cells and the text cells (docs/SCRIPTS.md).
constexpr std::pair<std::uint16_t, std::uint16_t> variable_ranges[]{
    {0x4900, 0x4BFF}, {0x6D00, 0x7FFF}, {0x9700, 0x98FF}, {0xC000, 0xC0FF}};

std::string hex(unsigned value, int width = 4)
{
    std::ostringstream text;
    text << std::uppercase << std::hex << std::setw(width) << std::setfill('0') << value;
    return text.str();
}
} // namespace

std::optional<ScriptLocation> find_script(const Machine &machine,
                                          const std::filesystem::path &game_directory)
{
    constexpr std::size_t probe_length = 48;
    for (unsigned disk = 1; disk <= 8; ++disk)
    {
        const auto name = "ECL" + std::to_string(disk) + ".DAX";
        std::ifstream file(game_directory / name, std::ios::binary);
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                              std::istreambuf_iterator<char>());
        const auto archive = decode_dax_archive(bytes);
        if (!archive)
        {
            continue;
        }
        for (const auto &record : archive.records)
        {
            if (record.bytes.size() < probe_length)
            {
                continue;
            }
            // Byte k of the record is script address 9900 + k - 2; the two
            // bytes before the script are not loaded as they are stored.
            for (std::uint32_t linear = 0x10000; linear + probe_length < 0xA0000; ++linear)
            {
                if ((linear - script_origin) % 16 != 0)
                {
                    continue;
                }
                bool same = true;
                for (std::size_t n = 0; n < probe_length && same; ++n)
                {
                    same =
                        machine.peek(static_cast<std::uint32_t>(linear + n)) == record.bytes[n + 2];
                }
                if (same)
                {
                    return ScriptLocation{
                        name, record.id, static_cast<std::uint16_t>((linear - script_origin) / 16)};
                }
            }
        }
    }
    return std::nullopt;
}

ScriptTrace::ScriptTrace(Machine &machine, const ScriptLocation &location,
                         const std::filesystem::path &file)
    : machine_(machine), base_(std::uint32_t{location.data_segment} * 16), out_(file)
{
    out_ << "# " << location.archive << " record " << location.record << ", data segment "
         << hex(location.data_segment) << '\n';
    machine_.watch(base_ + script_origin, base_ + script_limit - 1,
                   [this](std::uint32_t linear, std::uint8_t value, bool written)
                   {
                       const auto address = static_cast<std::uint16_t>(linear - base_);
                       if (written)
                       {
                           write(address, value);
                       }
                       else
                       {
                           read(address);
                       }
                   });
    for (const auto &[first, last] : variable_ranges)
    {
        machine_.watch(base_ + first, base_ + last,
                       [this](std::uint32_t linear, std::uint8_t value, bool written)
                       {
                           if (written)
                           {
                               write(static_cast<std::uint16_t>(linear - base_), value);
                           }
                       });
    }
}

ScriptTrace::~ScriptTrace()
{
    flush_reads();
    machine_.clear_watches();
}

void ScriptTrace::read(std::uint16_t address)
{
    if (run_end_ && (address == *run_end_ || address == *run_end_ + 1))
    {
        run_end_ = address;
        return;
    }
    flush_reads();
    run_start_ = run_end_ = address;
}

void ScriptTrace::write(std::uint16_t address, std::uint8_t value)
{
    flush_reads();
    out_ << "W " << hex(address) << ' ' << hex(value, 2) << '\n';
}

void ScriptTrace::flush_reads()
{
    if (run_start_)
    {
        out_ << "R " << hex(*run_start_) << '-' << hex(*run_end_) << '\n';
    }
    run_start_.reset();
    run_end_.reset();
}
} // namespace opengold::probe
