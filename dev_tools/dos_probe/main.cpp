// Research-only: runs the user's own copy of the original DOS game headless
// and records what it does, to compare with OpenGoldBox. See
// dev_tools/docs/dos-probe.md. Nothing it runs or records ships with the game.
#include "machine.h"
#include "png.h"
#include "script_trace.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using opengold::probe::Machine;

struct Key
{
    std::uint8_t scan, ascii;
};

const std::map<std::string, Key> &named_keys()
{
    static const std::map<std::string, Key> keys{
        {"enter", {0x1C, 0x0D}},
        {"esc", {0x01, 0x1B}},
        {"space", {0x39, 0x20}},
        {"backspace", {0x0E, 0x08}},
        {"tab", {0x0F, 0x09}},
        {"up", {0x48, 0}},
        {"down", {0x50, 0}},
        {"left", {0x4B, 0}},
        {"right", {0x4D, 0}},
        {"home", {0x47, 0}},
        {"end", {0x4F, 0}},
        {"pgup", {0x49, 0}},
        {"pgdn", {0x51, 0}},
        {"ins", {0x52, 0}},
        {"del", {0x53, 0}},
        {"f1", {0x3B, 0}},
        {"f2", {0x3C, 0}},
        {"f3", {0x3D, 0}},
        {"f4", {0x3E, 0}},
        {"f5", {0x3F, 0}},
        {"f10", {0x44, 0}},
        // The keypad with Num Lock on: its scan codes, with digits.
        {"kp1", {0x4F, '1'}},
        {"kp2", {0x50, '2'}},
        {"kp3", {0x51, '3'}},
        {"kp4", {0x4B, '4'}},
        {"kp6", {0x4D, '6'}},
        {"kp7", {0x47, '7'}},
        {"kp8", {0x48, '8'}},
        {"kp9", {0x49, '9'}},
    };
    return keys;
}

Key character_key(char c)
{
    static const std::string rows[] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"};
    static const std::uint8_t first[] = {0x02, 0x10, 0x1E, 0x2C};
    const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (unsigned row = 0; row < 4; ++row)
    {
        const auto at = rows[row].find(lower);
        if (at != std::string::npos)
        {
            return {static_cast<std::uint8_t>(first[row] + at), static_cast<std::uint8_t>(c)};
        }
    }
    if (c == ' ')
    {
        return {0x39, 0x20};
    }
    return {0, static_cast<std::uint8_t>(c)};
}

// Runs until the game waits for a key, at most `limit` instructions.
void wait_for_input(Machine &machine, std::uint64_t limit)
{
    machine.run(limit,
                [&]
                {
                    return machine.waiting_for_key();
                });
}

int run_scenario(Machine &machine, std::istream &scenario, const std::filesystem::path &game,
                 const std::filesystem::path &output)
{
    std::unique_ptr<opengold::probe::ScriptTrace> trace;
    // Instructions run, by segment (high 16 bits) and offset.
    std::map<std::uint32_t, std::uint64_t> coverage;
    std::string coverage_name;
    // Where the running script was found; kept, since a poke into the script
    // changes the bytes it was found by.
    std::optional<opengold::probe::ScriptLocation> script;
    const auto locate = [&](bool again)
    {
        if (again || !script)
        {
            script = opengold::probe::find_script(machine, game);
        }
        return script.has_value();
    };
    std::ofstream log(output / "run.log");
    std::string line;
    unsigned number = 0;
    while (std::getline(scenario, line))
    {
        ++number;
        std::istringstream words(line);
        std::string command;
        words >> command;
        if (command.empty() || command[0] == '#')
        {
            continue;
        }
        std::string argument;
        std::getline(words >> std::ws, argument);
        if (command == "run")
        {
            machine.run(std::stoull(argument) * 1000);
        }
        else if (command == "wait")
        {
            wait_for_input(machine,
                           argument.empty() ? 400'000'000ULL : std::stoull(argument) * 1000);
        }
        else if (command == "key")
        {
            const auto named = named_keys().find(argument);
            const auto key =
                named != named_keys().end() ? named->second : character_key(argument.at(0));
            machine.press(key.scan, key.ascii);
            wait_for_input(machine, 400'000'000ULL);
        }
        else if (command == "type")
        {
            for (const char c : argument)
            {
                const auto key = character_key(c);
                machine.press(key.scan, key.ascii);
                wait_for_input(machine, 400'000'000ULL);
            }
        }
        else if (command == "shot")
        {
            opengold::probe::write_png(output / (argument + ".png"), 320, 200, machine.screen());
        }
        else if (command == "trace" || command == "trace-bytes")
        {
            trace.reset();
            if (!locate(false))
            {
                std::cerr << "Line " << number << ": no ECL script is in memory\n";
                return 1;
            }
            trace = std::make_unique<opengold::probe::ScriptTrace>(
                machine, *script, output / (argument + ".trace"), command == "trace-bytes");
        }
        else if (command == "cpu")
        {
            // Logs the next N instructions with the registers before each.
            std::ofstream cpu_log(output / "cpu.log", std::ios::app);
            auto &cpu = machine.cpu();
            using opengold::probe::Reg;
            using opengold::probe::Seg;
            const auto count = std::stoull(argument);
            const auto end = machine.instructions() + count;
            machine.run(count,
                        [&]
                        {
                            cpu_log << std::hex << std::setfill('0') << std::setw(4)
                                    << cpu.seg(Seg::cs) << ':' << std::setw(4) << cpu.ip()
                                    << " ax=" << std::setw(4) << cpu.reg(Reg::ax)
                                    << " bx=" << std::setw(4) << cpu.reg(Reg::bx)
                                    << " cx=" << std::setw(4) << cpu.reg(Reg::cx)
                                    << " dx=" << std::setw(4) << cpu.reg(Reg::dx)
                                    << " si=" << std::setw(4) << cpu.reg(Reg::si)
                                    << " di=" << std::setw(4) << cpu.reg(Reg::di)
                                    << " bp=" << std::setw(4) << cpu.reg(Reg::bp)
                                    << " sp=" << std::setw(4) << cpu.reg(Reg::sp)
                                    << " ds=" << std::setw(4) << cpu.seg(Seg::ds)
                                    << " es=" << std::setw(4) << cpu.seg(Seg::es)
                                    << " fl=" << std::setw(4) << cpu.flags() << std::dec << '\n';
                            return machine.instructions() >= end;
                        });
        }
        else if (command == "services")
        {
            // Logs the BIOS and DOS services called since the last `services`.
            log << "services:";
            for (const auto &[service, count] : machine.take_service_counts())
            {
                log << std::hex << " int " << unsigned{service.first} << '/'
                    << unsigned{service.second} << std::dec << " x" << count;
            }
            log << '\n';
        }
        else if (command == "locate")
        {
            // After the party enters another area, the script to trace changes.
            if (!locate(true))
            {
                std::cerr << "Line " << number << ": no ECL script is in memory\n";
                return 1;
            }
            log << "script " << script->archive << " record " << script->record << '\n';
        }
        else if (command == "watch")
        {
            // watch FIRST LAST NAME: logs every write to linear addresses FIRST
            // to LAST (hex) with the writing instruction, until `unwatch`.
            std::istringstream fields(argument);
            std::string first, last, name;
            fields >> first >> last >> name;
            auto watch_log = std::make_shared<std::ofstream>(output / (name + ".watch"));
            auto &cpu = machine.cpu();
            machine.watch(static_cast<std::uint32_t>(std::stoul(first, nullptr, 16)),
                          static_cast<std::uint32_t>(std::stoul(last, nullptr, 16)),
                          [watch_log, &cpu](std::uint32_t linear, std::uint8_t value, bool written)
                          {
                              if (written)
                              {
                                  *watch_log << std::hex << linear << ' ' << unsigned{value} << " @"
                                             << cpu.instruction_segment() << ':'
                                             << cpu.instruction_offset() << std::dec << '\n';
                              }
                          });
        }
        else if (command == "coverage")
        {
            // coverage SEGMENT NAME: counts each instruction address run in the
            // code segment (hex, or `all`), written to out/NAME.coverage at
            // `uncoverage`.
            std::istringstream fields(argument);
            std::string segment;
            fields >> segment >> coverage_name;
            const bool every = segment == "all";
            const auto code = every ? std::uint16_t{}
                                    : static_cast<std::uint16_t>(std::stoul(segment, nullptr, 16));
            coverage.clear();
            machine.before_instruction(
                [&coverage, code, every](const opengold::probe::Cpu &cpu)
                {
                    const auto cs = cpu.seg(opengold::probe::Seg::cs);
                    if (every || cs == code)
                    {
                        ++coverage[(std::uint32_t{cs} << 16U) | cpu.ip()];
                    }
                });
        }
        else if (command == "uncoverage")
        {
            machine.before_instruction({});
            std::ofstream file(output / (coverage_name + ".coverage"));
            for (const auto &[address, count] : coverage)
            {
                file << std::hex << (address >> 16U) << ':' << (address & 0xFFFFU) << std::dec
                     << ' ' << count << '\n';
            }
        }
        else if (command == "unwatch")
        {
            // Ends every watch, a script trace's too.
            trace.reset();
            machine.clear_watches();
        }
        else if (command == "dump")
        {
            std::ofstream memory(output / (argument + ".bin"), std::ios::binary);
            for (std::uint32_t linear = 0; linear < 0x100000; ++linear)
            {
                memory.put(static_cast<char>(machine.peek(linear)));
            }
        }
        else if (command == "poke" || command == "peek")
        {
            // poke ADDRESS HEX-BYTES / peek ADDRESS COUNT, at script addresses
            // (offsets in the running script's data segment).
            if (!locate(false))
            {
                std::cerr << "Line " << number << ": no ECL script is in memory\n";
                return 1;
            }
            std::istringstream fields(argument);
            std::string address, rest;
            fields >> address >> rest;
            const auto base = std::uint32_t{script->data_segment} * 16 +
                              static_cast<std::uint32_t>(std::stoul(address, nullptr, 16));
            if (command == "poke")
            {
                for (std::size_t n = 0; n + 1 < rest.size(); n += 2)
                {
                    machine.poke(
                        static_cast<std::uint32_t>(base + n / 2),
                        static_cast<std::uint8_t>(std::stoul(rest.substr(n, 2), nullptr, 16)));
                }
            }
            else
            {
                log << "peek " << address << ':';
                for (unsigned n = 0; n < std::stoul(rest); ++n)
                {
                    log << ' ' << std::hex << std::setw(2) << std::setfill('0')
                        << unsigned{machine.peek(base + n)} << std::dec;
                }
                log << '\n';
            }
        }
        else if (command == "untrace")
        {
            trace.reset();
        }
        else
        {
            std::cerr << "Line " << number << ": unknown command " << command << '\n';
            return 2;
        }
        log << "line " << number << " (" << line << "): " << machine.instructions()
            << " instructions, at " << std::hex << machine.cpu().instruction_segment() << ':'
            << machine.cpu().instruction_offset() << std::dec
            << (machine.waiting_for_key() ? ", waiting for a key" : "") << '\n';
        if (machine.exited())
        {
            log << "The program exited with code " << *machine.exit_code() << '\n';
            break;
        }
    }
    log << "\nConsole:\n" << machine.console() << "\n\nFiles opened:\n";
    for (const auto &file : machine.opened_files())
    {
        log << file << '\n';
    }
    return 0;
}
} // namespace

int main(int argc, char **argv)
{
    const bool fresh = argc == 5 && std::string_view(argv[1]) == "--fresh";
    if (argc != 4 && !fresh)
    {
        std::cerr << "Usage: opengold_dos_probe [--fresh] GAME_DIR SCENARIO OUTPUT_DIR\n"
                     "Runs C:\\POOLRAD\\START.EXE from GAME_DIR (never written) through the\n"
                     "scenario's commands; writes screenshots and run.log to OUTPUT_DIR.\n"
                     "--fresh hides GAME_DIR's saved games and characters.\n";
        return 2;
    }
    argv += fresh ? 1 : 0;
    try
    {
        const std::filesystem::path output = argv[3];
        std::filesystem::create_directories(output);
        Machine machine({argv[1], output / "drive", 20000, fresh});
        machine.load("C:\\POOLRAD\\START.EXE");
        std::ifstream scenario(argv[2]);
        if (!scenario)
        {
            std::cerr << "Cannot read " << argv[2] << '\n';
            return 2;
        }
        return run_scenario(machine, scenario, argv[1], output);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
