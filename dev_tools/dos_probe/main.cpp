// Research-only: runs the user's own copy of the original DOS game headless
// and records what it does, to compare with OpenGoldBox. See
// dev_tools/docs/dos-probe.md. Nothing it runs or records ships with the game.
#include "machine.h"
#include "png.h"
#include "script_trace.h"

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>

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
        {"enter", {0x1C, 0x0D}},     {"esc", {0x01, 0x1B}}, {"space", {0x39, 0x20}},
        {"backspace", {0x0E, 0x08}}, {"tab", {0x0F, 0x09}}, {"up", {0x48, 0}},
        {"down", {0x50, 0}},         {"left", {0x4B, 0}},   {"right", {0x4D, 0}},
        {"home", {0x47, 0}},         {"end", {0x4F, 0}},    {"pgup", {0x49, 0}},
        {"pgdn", {0x51, 0}},         {"ins", {0x52, 0}},    {"del", {0x53, 0}},
        {"f1", {0x3B, 0}},           {"f2", {0x3C, 0}},     {"f3", {0x3D, 0}},
        {"f4", {0x3E, 0}},           {"f5", {0x3F, 0}},     {"f10", {0x44, 0}},
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
        else if (command == "trace")
        {
            trace.reset();
            const auto script = opengold::probe::find_script(machine, game);
            if (!script)
            {
                std::cerr << "Line " << number << ": no ECL script is in memory\n";
                return 1;
            }
            trace = std::make_unique<opengold::probe::ScriptTrace>(machine, *script,
                                                                   output / (argument + ".trace"));
        }
        else if (command == "dump")
        {
            std::ofstream memory(output / (argument + ".bin"), std::ios::binary);
            for (std::uint32_t linear = 0; linear < 0x100000; ++linear)
            {
                memory.put(static_cast<char>(machine.peek(linear)));
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
    if (argc != 4)
    {
        std::cerr << "Usage: opengold_dos_probe GAME_DIR SCENARIO OUTPUT_DIR\n"
                     "Runs C:\\POOLRAD\\START.EXE from GAME_DIR (never written) through the\n"
                     "scenario's commands; writes screenshots and run.log to OUTPUT_DIR.\n";
        return 2;
    }
    try
    {
        const std::filesystem::path output = argv[3];
        std::filesystem::create_directories(output);
        Machine machine({argv[1], output / "drive", 20000});
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
