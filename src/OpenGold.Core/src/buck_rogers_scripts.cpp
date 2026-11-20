#include "opengold/buck_rogers_scripts.h"

#include "archive_io.h"
#include "opengold/ecl.h"
#include "opengold/formats.h"
#include "opengold/path_text.h"

#include <algorithm>
#include <array>
#include <map>
#include <stdexcept>

namespace opengold
{
namespace
{

using por::EclInstruction;
using por::EclOperand;

// Cells the listing watches (types.pas TZone0, TZone1).
constexpr std::uint16_t data_set_cell = 0x7F12;
constexpr std::uint16_t area_kind_cell = 0x4BE6;
constexpr std::uint16_t fuel_cell = 0x4D1E; // The party ship's fuel status, 4D00h + 30.
constexpr unsigned solar_map_area = 0x70;
constexpr unsigned solar_map_picture_mode = 2;

namespace opcode
{
constexpr std::uint8_t exit = 0x00, go_to = 0x01, go_sub = 0x02, save = 0x09, return_ = 0x13;
constexpr std::uint8_t first_if = 0x16, last_if = 0x1B, ship_combat = 0x1F, new_ecl = 0x20;
constexpr std::uint8_t skill_check = 0x22, skill_check_with_text = 0x23, on_goto = 0x25,
                       on_gosub = 0x26, program = 0x38, empty_3b = 0x3B, empty_3c = 0x3C,
                       clear_and_stop = 0x42, skill_check_with_damage = 0x49, shop = 0x4B,
                       picture_mode = 0x4C;
} // namespace opcode

bool is_memory(const EclOperand &operand)
{
    return operand.tag == 1 || operand.tag == 3 || operand.tag == 5;
}

bool is_literal(const EclOperand &operand)
{
    return operand.tag == 0 || operand.tag == 2 || operand.tag == 4;
}

unsigned literal(const EclOperand &operand)
{
    return operand.value | (static_cast<unsigned>(operand.high_word) << 16);
}

std::string hex(unsigned value)
{
    constexpr std::string_view digits = "0123456789ABCDEF";
    std::string text;
    do
    {
        text.insert(text.begin(), digits[value % 16]);
        value /= 16;
    } while (value != 0);
    return text + "h";
}

std::string describe(const EclOperand &operand)
{
    if (is_literal(operand))
    {
        return std::to_string(literal(operand));
    }
    if (is_memory(operand))
    {
        return "[" + hex(operand.value) + "]";
    }
    if (operand.tag == 128)
    {
        return "'" + operand.text + "'";
    }
    return "the text at [" + hex(operand.value) + "]";
}

bool is_if(const EclInstruction &instruction)
{
    return instruction.opcode >= opcode::first_if && instruction.opcode <= opcode::last_if;
}

bool falls_through(const EclInstruction &instruction)
{
    constexpr std::array<std::uint8_t, 7> leaving{
        opcode::exit,     opcode::go_to,    opcode::return_,       opcode::new_ecl,
        opcode::empty_3b, opcode::empty_3c, opcode::clear_and_stop};
    return std::find(leaving.begin(), leaving.end(), instruction.opcode) == leaving.end();
}

// The operands a command writes its result to (eclvm.pas): SAVE, the
// arithmetic, RANDOM, the inputs, the menus, CHECK PARTY, the skill checks'
// member and result, GET TABLE, AND, OR and DISTANCE AHEAD.
std::vector<std::size_t> written_operands(std::uint8_t command)
{
    switch (command)
    {
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x2A:
    case 0x2F:
    case 0x30:
        return {2};
    case 0x08:
    case opcode::save:
    case 0x0F:
    case 0x10:
        return {1};
    case 0x15:
    case 0x2B:
    case 0x31:
    case 0x45:
        return {0};
    case 0x1E:
        return {2, 3, 4, 5};
    case opcode::skill_check:
    case opcode::skill_check_with_text:
        return {1, 2};
    default:
        return {};
    }
}

// How many operands the original's IF steps over for each command
// (eclmenu.pas SkipEclCmd); any other command it steps over as one byte.
std::size_t skipped_operands(std::uint8_t command)
{
    constexpr std::array<std::uint8_t, 17> one{0x01, 0x02, 0x0A, 0x0E, 0x11, 0x12, 0x20, 0x2D, 0x32,
                                               0x34, 0x36, 0x38, 0x39, 0x3C, 0x43, 0x48, 0x4B};
    constexpr std::array<std::uint8_t, 13> two{0x03, 0x08, 0x09, 0x0F, 0x10, 0x1F, 0x41,
                                               0x40, 0x1D, 0x3F, 0x45, 0x46, 0x4C};
    constexpr std::array<std::uint8_t, 16> three{0x04, 0x05, 0x06, 0x07, 0x0B, 0x0C, 0x21, 0x28,
                                                 0x2A, 0x2F, 0x30, 0x35, 0x37, 0x3B, 0x22, 0x23};
    constexpr std::array<std::uint8_t, 2> six{0x1E, 0x49};
    const auto among = [command](const auto &commands)
    {
        return std::find(commands.begin(), commands.end(), command) != commands.end();
    };
    if (among(one))
    {
        return 1;
    }
    if (among(two))
    {
        return 2;
    }
    if (among(three))
    {
        return 3;
    }
    if (among(six))
    {
        return 6;
    }
    return command == 0x14 ? 4 : command == 0x2E ? 5 : 0;
}

bool writes_cell(const EclInstruction &command, std::uint16_t cell)
{
    const auto written = written_operands(command.opcode);
    return std::any_of(written.begin(), written.end(),
                       [&](std::size_t operand)
                       {
                           return operand < command.operands.size() &&
                                  is_memory(command.operands[operand]) &&
                                  command.operands[operand].value == cell;
                       });
}

// One script's reachable commands in address order, with what jumps where.
class ScriptCommands
{
  public:
    ScriptCommands(const por::EclProgram &program, ScriptPlace place) : place_(std::move(place))
    {
        auto walk = por::walk_reachable(program);
        commands_ = std::move(walk.instructions);
        std::sort(commands_.begin(), commands_.end(),
                  [](const EclInstruction &a, const EclInstruction &b)
                  {
                      return a.address < b.address;
                  });
        for (std::size_t index = 0; index < commands_.size(); ++index)
        {
            index_of_.emplace(commands_[index].address, index);
            add_jumps(index);
        }
        entries_.insert(program.entries().begin(), program.entries().end());
    }

    [[nodiscard]] const std::vector<EclInstruction> &commands() const noexcept
    {
        return commands_;
    }

    [[nodiscard]] ScriptPlace place(std::size_t index) const
    {
        auto place = place_;
        place.address = commands_[index].address;
        return place;
    }

    // The command after this one in the script's bytes, if reachable.
    [[nodiscard]] bool follows(std::size_t earlier, std::size_t later) const
    {
        return later < commands_.size() && later == earlier + 1 &&
               commands_[earlier].next == commands_[later].address;
    }

    // The literals the operand can hold as the command at index reads it.
    [[nodiscard]] TracedValues value_of(std::size_t index, const EclOperand &operand) const
    {
        if (is_literal(operand))
        {
            return {{literal(operand)}, true};
        }
        if (!is_memory(operand))
        {
            return {{}, false};
        }
        return values_before(index, operand.value);
    }

  private:
    void add_jumps(std::size_t index)
    {
        const auto &command = commands_[index];
        if (command.opcode == opcode::go_to || command.opcode == opcode::go_sub)
        {
            jumpers_.emplace(command.operands[0].value, index);
        }
        if (command.opcode == opcode::on_goto || command.opcode == opcode::on_gosub)
        {
            for (std::size_t target = 2; target < command.operands.size(); ++target)
            {
                jumpers_.emplace(command.operands[target].value, index);
            }
        }
    }

    struct Predecessor
    {
        std::size_t index{};
        // The command is a GOSUB or ON GOSUB whose subroutine returns here.
        bool after_call{};
    };

    // The commands that can run just before the one at index: those that jump
    // to it, the one before it when that goes on, and an IF two before it,
    // which skips the one between.
    [[nodiscard]] std::vector<Predecessor> predecessors(std::size_t index) const
    {
        std::vector<Predecessor> found;
        const auto [first, last] = jumpers_.equal_range(commands_[index].address);
        for (auto jumper = first; jumper != last; ++jumper)
        {
            found.push_back({jumper->second, false});
        }
        if (index >= 1 && follows(index - 1, index) && falls_through(commands_[index - 1]))
        {
            const auto call = commands_[index - 1].opcode;
            found.push_back({index - 1, call == opcode::go_sub || call == opcode::on_gosub});
        }
        if (index >= 2 && follows(index - 2, index - 1) && follows(index - 1, index) &&
            is_if(commands_[index - 2]))
        {
            found.push_back({index - 2, false});
        }
        return found;
    }

    // What a subroutine's own commands, up to its first RETURN, leave in a cell.
    struct SubroutineWrites
    {
        TracedValues values;
        // No command writes the cell on every way through.
        bool may_keep{true};
    };

    [[nodiscard]] SubroutineWrites subroutine_writes(std::uint16_t start, std::uint16_t cell) const
    {
        SubroutineWrites written;
        const auto found = index_of_.find(start);
        if (found == index_of_.end())
        {
            return written;
        }
        for (auto index = found->second; index < commands_.size(); ++index)
        {
            const auto &command = commands_[index];
            if (writes_cell(command, cell))
            {
                const bool conditional = index > found->second && is_if(commands_[index - 1]);
                if (!conditional)
                {
                    written.values = {};
                    written.may_keep = false;
                }
                if (command.opcode == opcode::save && is_literal(command.operands[0]))
                {
                    written.values.values.insert(literal(command.operands[0]));
                }
                else
                {
                    written.values.complete = false;
                }
            }
            if (!falls_through(command) || !follows(index, index + 1))
            {
                break;
            }
        }
        return written;
    }

    // Steps back over a GOSUB or ON GOSUB: adds the values its subroutines
    // save in the cell; true when some subroutine may leave the cell as the
    // call found it.
    [[nodiscard]] bool merge_calls(const EclInstruction &call, std::uint16_t cell,
                                   TracedValues &traced) const
    {
        const auto first_target = call.opcode == opcode::go_sub ? 0U : 2U;
        bool some_keep_it = false;
        for (auto target = first_target; target < call.operands.size(); ++target)
        {
            const auto written = subroutine_writes(call.operands[target].value, cell);
            traced.values.insert(written.values.values.begin(), written.values.values.end());
            traced.complete = traced.complete && written.values.complete;
            some_keep_it = some_keep_it || written.may_keep;
        }
        return some_keep_it;
    }

    [[nodiscard]] TracedValues values_before(std::size_t index, std::uint16_t cell) const
    {
        TracedValues traced;
        std::set<std::size_t> seen;
        std::vector<std::size_t> pending{index};
        while (!pending.empty())
        {
            const auto at = pending.back();
            pending.pop_back();
            if (!seen.insert(at).second)
            {
                continue;
            }
            const auto before = predecessors(at);
            if (before.empty() || entries_.contains(commands_[at].address))
            {
                traced.complete = false;
            }
            for (const auto [earlier, after_call] : before)
            {
                const auto &command = commands_[earlier];
                if (after_call)
                {
                    if (merge_calls(command, cell, traced))
                    {
                        pending.push_back(earlier);
                    }
                }
                else if (!writes_cell(command, cell))
                {
                    pending.push_back(earlier);
                }
                else if (command.opcode == opcode::save && is_literal(command.operands[0]))
                {
                    traced.values.insert(literal(command.operands[0]));
                }
                else
                {
                    traced.complete = false;
                }
            }
        }
        return traced;
    }

    ScriptPlace place_;
    std::vector<EclInstruction> commands_;
    std::map<std::uint16_t, std::size_t> index_of_;
    std::multimap<std::uint16_t, std::size_t> jumpers_;
    std::set<std::uint16_t> entries_;
};

ScriptCommandUse command_use(const ScriptCommands &script, std::size_t index, GoldBoxGame game)
{
    const auto &command = script.commands()[index];
    ScriptCommandUse use{
        script.place(index), std::string(por::ecl_opcode(command.opcode, game).name), {}};
    for (const auto &operand : command.operands)
    {
        use.operands.push_back(describe(operand));
    }
    return use;
}

void list_skill_check(BuckRogersScriptListing &listing, const ScriptCommands &script,
                      std::size_t index)
{
    const auto &command = script.commands()[index];
    listing.skill_checks.push_back(
        {script.place(index), command.opcode, script.value_of(index, command.operands[0]),
         script.value_of(index, command.operands[2]), script.value_of(index, command.operands[1])});
}

bool opens_solar_map(const ScriptCommands &script, std::size_t index)
{
    const auto &command = script.commands()[index];
    if (command.opcode == opcode::picture_mode)
    {
        return script.value_of(index, command.operands[0]).values.contains(solar_map_picture_mode);
    }
    return command.opcode == opcode::save && is_memory(command.operands[1]) &&
           command.operands[1].value == area_kind_cell && is_literal(command.operands[0]) &&
           literal(command.operands[0]) == solar_map_area;
}

void list_command(BuckRogersScriptListing &listing, const ScriptCommands &script, std::size_t index,
                  GoldBoxGame game)
{
    const auto &commands = script.commands();
    const auto &command = commands[index];
    switch (command.opcode)
    {
    case opcode::skill_check:
    case opcode::skill_check_with_text:
    case opcode::skill_check_with_damage:
        list_skill_check(listing, script, index);
        break;
    case opcode::ship_combat:
        listing.ship_combats.push_back(command_use(script, index, game));
        break;
    case opcode::shop:
        listing.shops.push_back(command_use(script, index, game));
        break;
    case opcode::program:
        listing.programs.push_back(command_use(script, index, game));
        break;
    case opcode::new_ecl:
        listing.script_loads.push_back({script.place(index),
                                        script.value_of(index, command.operands[0]),
                                        script.value_of(index, {3, data_set_cell, {}, 0})});
        break;
    case opcode::empty_3b:
    case opcode::empty_3c:
        listing.stalls.push_back(command_use(script, index, game));
        break;
    default:
        break;
    }
    if (opens_solar_map(script, index))
    {
        listing.solar_maps.push_back(command_use(script, index, game));
    }
    if (writes_cell(command, fuel_cell))
    {
        listing.fuel_writes.push_back(command_use(script, index, game));
    }
    if (is_if(command) && script.follows(index, index + 1) &&
        skipped_operands(commands[index + 1].opcode) != commands[index + 1].operands.size())
    {
        listing.uneven_skips.push_back(command_use(script, index + 1, game));
    }
}

std::vector<std::pair<std::string, std::filesystem::path>>
script_archives(const std::filesystem::path &folder)
{
    std::vector<std::pair<std::string, std::filesystem::path>> archives;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        const auto name = por::detail::upper_ascii(path_text(entry.path().filename()));
        if (entry.is_regular_file() && name.starts_with("ECL") && name.ends_with(".DAX"))
        {
            archives.emplace_back(name, entry.path());
        }
    }
    std::sort(archives.begin(), archives.end());
    return archives;
}

} // namespace

BuckRogersScriptListing list_buck_rogers_scripts(const std::filesystem::path &folder,
                                                 GoldBoxGame game)
{
    BuckRogersScriptListing listing;
    for (const auto &[name, path] : script_archives(folder))
    {
        const auto bytes = por::detail::read_archive_bytes<std::runtime_error>(
            path, name + " is too large", "Cannot open " + name, "Cannot read " + name);
        const auto archive = decode_dax_archive(bytes);
        if (!archive)
        {
            throw std::runtime_error(name + " is not a DAX archive");
        }
        for (const auto &record : archive.records)
        {
            const auto program = por::EclProgram::decode(
                record.bytes, name + " record " + std::to_string(record.id), game);
            const ScriptCommands script(program, {name, record.id, 0});
            for (std::size_t index = 0; index < script.commands().size(); ++index)
            {
                list_command(listing, script, index, game);
            }
        }
    }
    return listing;
}

} // namespace opengold
