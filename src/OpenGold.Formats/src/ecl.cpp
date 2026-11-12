#include "opengold/ecl.h"
#include <deque>
#include <span>
#include <map>
#include <set>
#include <utility>

namespace opengold::por
{
namespace
{

// PoR PC 1.3: sources and per-opcode host responsibilities in docs/SCRIPTS.md.
constexpr std::array<EclOpcode, 62> pool_of_radiance_opcodes{
    {{"EXIT", 0, false, true},
     {"GOTO", 1, false, true},
     {"GOSUB", 1, false, true},
     {"COMPARE", 2, false, true},
     {"ADD", 3, false, true},
     {"SUBTRACT", 3, false, true},
     {"DIVIDE", 3, false, true},
     {"MULTIPLY", 3, false, true},
     {"RANDOM", 2, false, true},
     {"SAVE", 2, false, true},
     {"LOAD CHARACTER", 1, false, true, true},
     {"LOAD MONSTER", 3, false, true, true},
     {"SETUP MONSTER", 3, false, true, true},
     {"APPROACH", 0, false, true, true},
     {"PICTURE", 1, false, true, true},
     {"INPUT NUMBER", 2, false, true},
     {"INPUT STRING", 2, false, true},
     {"PRINT", 1, false, true},
     {"PRINTCLEAR", 1, false, true},
     {"RETURN", 0, false, true},
     {"COMPARE AND", 4, false, true},
     {"VERTICAL MENU", 3, true, true},
     {"IF =", 0, false, true},
     {"IF <>", 0, false, true},
     {"IF <", 0, false, true},
     {"IF >", 0, false, true},
     {"IF <=", 0, false, true},
     {"IF >=", 0, false, true},
     {"CLEAR MONSTERS", 0, false, true, true},
     {"PARTY STRENGTH", 1, false, true, true},
     {"CHECK PARTY", 6, false, true, true},
     {"UNDEFINED 1F", 2},
     {"NEW ECL", 1, false, true, true},
     {"LOAD FILES", 3, false, true, true},
     {"PARTY SURPRISE", 2, false, true, true},
     {"SURPRISE", 4, false, true, true},
     {"COMBAT", 0, false, true, true},
     {"ON GOTO", 2, true, true},
     {"ON GOSUB", 2, true, true},
     {"TREASURE", 8, false, true, true},
     {"ROB", 3, false, true, true},
     {"ENCOUNTER MENU", 14, false, true, true},
     {"GETTABLE", 3, false, true},
     {"HORIZONTAL MENU", 2, true, true},
     {"PARLAY", 6, false, true},
     {"CALL", 1, false, true, true},
     {"DAMAGE", 5, false, true, true},
     {"AND", 3, false, true},
     {"OR", 3, false, true},
     {"SPRITE OFF", 0, false, true, true},
     {"FIND ITEM", 1, false, true, true},
     {"PRINT RETURN", 0, false, true},
     {"ECL CLOCK", 1, false, true, true},
     {"SAVE TABLE", 3, false, true},
     {"ADD NPC", 2, false, true, true},
     {"LOAD PIECES", 3, false, true, true},
     {"PROGRAM", 1, false, true, true},
     {"WHO", 1, false, true, true},
     {"DELAY", 0, false, true, true},
     {"SPELL", 3, false, true, true},
     {"PROTECTION", 1, false, true, true},
     {"CLEAR BOX", 0, false, true}}};

// Curse of the Azure Bonds keeps Pool of Radiance's commands, gives ECL CLOCK a
// second operand and adds three (docs/games/curse.md).
std::vector<EclOpcode> curse_opcodes()
{
    std::vector<EclOpcode> opcodes(pool_of_radiance_opcodes.begin(),
                                   pool_of_radiance_opcodes.end());
    opcodes.at(0x34).operands = 2;
    opcodes.push_back({"DUMP", 0, false, true, true});
    opcodes.push_back({"FIND SPECIAL", 1, false, true, true});
    opcodes.push_back({"DESTROY ITEMS", 1, false, true, true});
    return opcodes;
}

std::span<const EclOpcode> game_opcodes(GoldBoxGame game)
{
    static const std::vector<EclOpcode> curse = curse_opcodes();
    switch (game)
    {
    case GoldBoxGame::pool_of_radiance:
        return pool_of_radiance_opcodes;
    case GoldBoxGame::curse_of_the_azure_bonds:
        return curse;
    default:
        throw EclError("The scripts of " + std::string(game_title(game)) + " are not decoded yet");
    }
}

} // namespace

const EclOpcode &ecl_opcode(std::uint8_t op, GoldBoxGame game)
{
    const auto opcodes = game_opcodes(game);
    if (op >= opcodes.size())
    {
        throw EclError("Unknown opcode " + std::to_string(op));
    }
    return opcodes[op];
}

EclMemory ecl_memory(GoldBoxGame game)
{
    switch (game)
    {
    case GoldBoxGame::pool_of_radiance:
        return {EclProgram::origin, EclProgram::limit};
    case GoldBoxGame::curse_of_the_azure_bonds:
        return {0x8000, 0x9E00};
    default:
        throw EclError("The scripts of " + std::string(game_title(game)) + " are not decoded yet");
    }
}

std::string unpack_ecl_text(std::span<const std::uint8_t> bytes)
{
    std::string text;
    unsigned buffer = 0, bits = 0;
    for (auto byte : bytes)
    {
        buffer = (buffer << 8) | byte;
        bits += 8;
        while (bits >= 6)
        {
            bits -= 6;
            const auto code = (buffer >> bits) & 63;
            if (code)
            {
                text.push_back(static_cast<char>(code < 32 ? code + 64 : code));
            }
        }
        buffer &= (1U << bits) - 1;
    }
    return text;
}

EclInstruction EclProgram::instruction(std::uint32_t address) const
{
    return instruction(address, raw_);
}

EclInstruction EclProgram::instruction(std::uint32_t address,
                                       std::span<const std::uint8_t> image) const
{
    if (image.size() != raw_.size())
    {
        throw EclError("Invalid execution image size");
    }
    const auto bank_start = memory_.origin;
    if (address < bank_start || address >= bank_start + image.size() - 2)
    {
        throw EclError("Instruction address outside program");
    }
    std::size_t cursor = address - bank_start + 2;
    const auto byte = [&]()
    {
        if (cursor >= image.size())
        {
            throw EclError("Truncated instruction operand");
        }
        return image[cursor++];
    };
    EclInstruction result;
    result.address = static_cast<std::uint16_t>(address);
    result.opcode = byte();
    const auto &spec = ecl_opcode(result.opcode, game_);
    const auto operand = [&]()
    {
        EclOperand arg;
        arg.tag = byte();
        arg.value = byte();
        if (arg.tag == 1 || arg.tag == 2 || arg.tag == 3 || arg.tag == 129)
        {
            arg.value |= static_cast<std::uint16_t>(byte()) << 8;
        }
        else if (arg.tag == 128)
        {
            if (arg.value > image.size() - cursor)
            {
                throw EclError("Truncated packed text");
            }
            arg.text = unpack_ecl_text(image.subspan(cursor, arg.value));
            cursor += arg.value;
        }
        else if (arg.tag != 0)
        {
            throw EclError("Unsupported operand tag " + std::to_string(arg.tag));
        }
        return arg;
    };
    for (unsigned i = 0; i < spec.operands; ++i)
    {
        result.operands.push_back(operand());
    }
    if (spec.variable_list)
    {
        const auto &count = result.operands.back();
        if ((count.tag != 0 && count.tag != 2) || count.value > 255)
        {
            throw EclError("Dynamic or invalid operand list length");
        }
        const unsigned n = count.value;
        for (unsigned i = 0; i < n; ++i)
        {
            result.operands.push_back(operand());
        }
    }
    result.next = bank_start + static_cast<std::uint32_t>(cursor) - 2;
    for (const auto &change : overrides_)
    {
        if (change.address == result.address && change.operand < result.operands.size())
        {
            result.operands[change.operand].value = change.value;
        }
    }
    return result;
}

EclProgram EclProgram::with_operand_overrides(std::vector<EclOperandOverride> overrides) const
{
    auto changed = *this;
    changed.overrides_ = std::move(overrides);
    return changed;
}

EclProgram EclProgram::decode(std::span<const std::uint8_t> bytes, std::string_view source,
                              GoldBoxGame game)
{
    try
    {
        const auto memory = ecl_memory(game);
        if (bytes.size() < 2 || bytes.size() - 2 > memory.limit - memory.origin)
        {
            throw EclError("Invalid ECL record size");
        }
        EclProgram program;
        program.raw_.assign(bytes.begin(), bytes.end());
        program.source_ = source;
        program.game_ = game;
        program.memory_ = memory;
        auto cursor = memory.origin;
        for (auto &target : program.entries_)
        {
            const auto entry = program.instruction(cursor);
            if (entry.opcode != 1 || entry.operands[0].tag >= 128)
            {
                throw EclError("Invalid entry jump");
            }
            target = entry.operands[0].value;
            cursor = entry.next;
        }
        program.body_start_ = cursor;
        for (auto target : program.entries_)
        {
            if (target < cursor || target >= memory.origin + bytes.size() - 2)
            {
                throw EclError("Entry target outside program body");
            }
        }
        return program;
    }
    catch (const EclError &e)
    {
        throw EclError(std::string(source) + ": " + e.what());
    }
}

EclWalk walk_reachable(const EclProgram &program)
{
    using Edge = std::pair<std::uint32_t, std::optional<std::uint16_t>>;
    std::deque<Edge> queue;

    for (auto entry : program.entries())
    {
        queue.emplace_back(entry, std::nullopt);
    }

    std::set<std::uint32_t> seen;
    std::map<std::uint32_t, std::uint32_t> spans;
    EclWalk walk;

    while (!queue.empty())
    {
        const auto [pc, fallthrough_from] = queue.front();
        queue.pop_front();

        if (!seen.insert(pc).second)
        {
            continue;
        }

        try
        {
            if (pc < program.body_start())
            {
                throw EclError("Target in entry table");
            }

            const auto i = program.instruction(pc);

            for (const auto &[start, end] : spans)
            {
                if (pc < end && i.next > start)
                {
                    throw EclError("Overlapping instructions");
                }
            }

            spans[pc] = i.next;
            walk.instructions.push_back(i);

            if (i.opcode == 1 || i.opcode == 2)
            {
                queue.emplace_back(i.operands[0].value, fallthrough_from);
            }

            if (i.opcode == 37 || i.opcode == 38)
            {
                for (std::size_t n = 2; n < i.operands.size(); ++n)
                {
                    queue.emplace_back(i.operands[n].value, fallthrough_from);
                }
            }

            if (i.opcode >= 22 && i.opcode <= 27)
            {
                queue.emplace_back(program.instruction(i.next).next, fallthrough_from);
            }

            if (i.opcode != 0 && i.opcode != 1 && i.opcode != 19 && i.opcode != 32)
            {
                queue.emplace_back(i.next,
                                   i.opcode == 37 ? std::optional(i.address) : fallthrough_from);
            }
        }
        catch (const EclError &e)
        {
            walk.errors.push_back({pc, fallthrough_from, e.what()});
        }
    }

    return walk;
}
} // namespace opengold::por
