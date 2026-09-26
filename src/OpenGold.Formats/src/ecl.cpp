#include "opengold/ecl.h"
#include <utility>

namespace opengold::por
{
const EclOpcode &ecl_opcode(std::uint8_t op)
{
    // PoR PC 1.3: sources and per-opcode host responsibilities in docs/SCRIPTS.md.
    static const std::array<EclOpcode, 62> specs{{{"EXIT", 0, false, true},
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
                                                  {"ECL CLOCK", 1},
                                                  {"SAVE TABLE", 3, false, true},
                                                  {"ADD NPC", 2, false, true, true},
                                                  {"LOAD PIECES", 3, false, true, true},
                                                  {"PROGRAM", 1, false, true, true},
                                                  {"WHO", 1, false, true, true},
                                                  {"DELAY", 0, false, true, true},
                                                  {"SPELL", 3, false, true},
                                                  {"PROTECTION", 1, false, true, true},
                                                  {"CLEAR BOX", 0, false, true}}};
    if (op >= specs.size())
        throw EclError("Unknown opcode " + std::to_string(op));
    return specs[op];
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
                text.push_back(static_cast<char>(code < 32 ? code + 64 : code));
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
        throw EclError("Invalid execution image size");
    if (address < origin || address >= origin + image.size() - 2)
        throw EclError("Instruction address outside program");
    std::size_t cursor = address - origin + 2;
    const auto byte = [&]()
    {
        if (cursor >= image.size())
            throw EclError("Truncated instruction operand");
        return image[cursor++];
    };
    EclInstruction result;
    result.address = static_cast<std::uint16_t>(address);
    result.opcode = byte();
    const auto &spec = ecl_opcode(result.opcode);
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
                throw EclError("Truncated packed text");
            arg.text = unpack_ecl_text(image.subspan(cursor, arg.value));
            cursor += arg.value;
        }
        else if (arg.tag != 0)
            throw EclError("Unsupported operand tag " + std::to_string(arg.tag));
        return arg;
    };
    for (unsigned i = 0; i < spec.operands; ++i)
        result.operands.push_back(operand());
    if (spec.variable_list)
    {
        const auto &count = result.operands.back();
        if ((count.tag != 0 && count.tag != 2) || count.value > 255)
            throw EclError("Dynamic or invalid operand list length");
        const unsigned n = count.value;
        for (unsigned i = 0; i < n; ++i)
            result.operands.push_back(operand());
    }
    result.next = origin + static_cast<std::uint32_t>(cursor) - 2;
    return result;
}

EclProgram EclProgram::decode(std::span<const std::uint8_t> bytes, std::string source)
{
    try
    {
        if (bytes.size() < 2 || bytes.size() - 2 > limit - origin)
            throw EclError("Invalid ECL record size");
        EclProgram program;
        program.raw_.assign(bytes.begin(), bytes.end());
        program.source_ = source;
        auto cursor = origin;
        for (auto &target : program.entries_)
        {
            const auto entry = program.instruction(cursor);
            if (entry.opcode != 1 || entry.operands[0].tag >= 128)
                throw EclError("Invalid entry jump");
            target = entry.operands[0].value;
            cursor = entry.next;
        }
        program.body_start_ = cursor;
        for (auto target : program.entries_)
            if (target < cursor || target >= origin + bytes.size() - 2)
                throw EclError("Entry target outside program body");
        return program;
    }
    catch (const EclError &e)
    {
        throw EclError(source + ": " + e.what());
    }
}
} // namespace opengold::por
