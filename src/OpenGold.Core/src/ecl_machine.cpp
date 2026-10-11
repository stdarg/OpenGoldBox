#include "opengold/ecl_machine.h"
#include "opengold/formats.h"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace opengold::por
{
EclCatalog EclCatalog::load(const std::filesystem::path &directory)
{
    try
    {
        std::map<std::string, std::filesystem::path> files;
        for (const auto &entry : std::filesystem::directory_iterator(directory))
        {
            if (!entry.is_regular_file())
                continue;
            auto name = entry.path().filename().string();
            for (auto &c : name)
                if (c >= 'a' && c <= 'z')
                    c -= 'a' - 'A';
            if (!name.starts_with("ECL") || !name.ends_with(".DAX"))
                continue;
            const auto digits = name.substr(3, name.size() - 7);
            if (!std::all_of(digits.begin(), digits.end(),
                             [](char c)
        {
            return c >= '0' && c <= '9';
        }))
            continue;
            if (!files.emplace(name, entry.path()).second)
                throw EclError("Ambiguous script archive " + name);
        }
        EclCatalog catalog;
        for (const auto &[name, path] : files)
        {
            const auto size = std::filesystem::file_size(path);
            if (size > 32 * 1024 * 1024)
                throw EclError("Script archive too large: " + name);
            std::ifstream input(path, std::ios::binary);
            if (!input)
                throw EclError("Cannot open " + name);
            std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
            if (input.bad() || size != bytes.size())
                throw EclError("Cannot read " + name);
            auto dax = decode_dax_archive(bytes);
            if (!dax)
                throw EclError("Invalid DAX archive " + name);
            for (const auto &record : dax.records)
            {
                auto program =
                    EclProgram::decode(record.bytes, name + ":" + std::to_string(record.id));
                catalog.programs_.emplace(ScriptId{name, record.id},
                                          std::make_shared<const EclProgram>(std::move(program)));
            }
        }
        if (catalog.programs_.empty())
            throw EclError("No ECL programs found in " + directory.string());
        return catalog;
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        throw EclError(e.what());
    }
}

std::shared_ptr<const EclProgram> EclCatalog::find(const ScriptId &id) const
{
    const auto found = programs_.find(id);
    return found == programs_.end() ? nullptr : found->second;
}

namespace
{
// A machine runs nothing without a program, so a missing one is refused
// before any member is built from it.
std::shared_ptr<const EclProgram> required_program(std::shared_ptr<const EclProgram> program)
{
    if (!program)
        throw EclError("EclMachine requires a program");
    return program;
}

std::uint16_t encoded_address(const EclOperand &arg)
{
    if (arg.tag >= 128)
        throw EclError("Expected encoded address");
    return arg.value;
}

EclAddress indexed_address(EclAddress base, std::size_t index)
{
    if (index > 65535U - base.location)
        throw EclError("Address calculation overflow");
    return EclAddress(static_cast<std::uint16_t>(base.location + index));
}

bool program_address(EclAddress address)
{
    return address.location >= EclProgram::origin && address.location < EclProgram::limit;
}

bool byte_address(EclAddress address)
{
    return program_address(address) || (address >= ecl_party_x && address <= ecl_cell_event);
}

EclConditions relation(int c)
{
    return {c == 0, c != 0, c < 0, c > 0, c <= 0, c >= 0};
}

EclConditions equality(bool equal)
{
    return {equal, !equal, false, false, false, false};
}

void validate_string(EclAddress address, std::string_view value)
{
    if (value.size() > 255 || value.find('\0') != std::string_view::npos)
        throw EclError("ECL string exceeds 255 characters or contains NUL");
    (void)indexed_address(address, value.size());
}
} // namespace

EclMachine::EclMachine(std::shared_ptr<const EclProgram> program)
    : program_(required_program(std::move(program))), image_(program_->raw())
{
}

void EclMachine::require_configurable() const
{
    if (state_ != EclState::idle && state_ != EclState::completed)
        throw EclError("Cannot configure machine during execution");
}

void EclMachine::seed_random(std::uint32_t seed)
{
    require_configurable();
    random_.seed(seed);
}

void EclMachine::enable_host(std::uint8_t opcode)
{
    require_configurable();
    if (!ecl_opcode(opcode).requires_host)
        throw EclError("Opcode is not a host operation");
    host_opcodes_.insert(opcode);
}

void EclMachine::bind_variable(EclAddress address, std::uint16_t v)
{
    require_configurable();
    if (program_address(address))
        throw EclError("Program addresses cannot be bound as variables");
    variables_[address.location] = byte_address(address) ? v & 255 : v;
}

void EclMachine::bind_string(EclAddress address, std::string_view v)
{
    require_configurable();
    validate_string(address, v);
    // Check the whole range before changing any binding.
    for (std::size_t n = 0; n <= v.size(); ++n)
    {
        if (program_address(indexed_address(address, n)))
            throw EclError("Program addresses cannot be bound as strings");
    }
    for (std::size_t n = 0; n <= v.size(); ++n)
        bind_variable(indexed_address(address, n),
                      n == v.size() ? 0 : static_cast<unsigned char>(v[n]));
}

std::uint16_t EclMachine::variable(EclAddress address) const
{
    if (program_address(address))
    {
        const auto offset = address.location - EclProgram::origin + 2;
        if (offset >= image_.size())
            throw EclError("Read beyond loaded script image");
        return image_[offset];
    }
    const auto it = variables_.find(address.location);
    if (it == variables_.end())
        throw EclError("Unbound variable address " + std::to_string(address.location));
    return it->second;
}

std::string EclMachine::string(EclAddress address) const
{
    std::string result;
    for (std::size_t n = 0; n <= 255; ++n)
    {
        const auto ch = variable(indexed_address(address, n)) & 255;
        if (!ch)
            return result;
        if (n == 255)
            break;
        result.push_back(static_cast<char>(ch));
    }
    throw EclError("Unterminated ECL string");
}

void EclMachine::write(EclAddress address, std::uint16_t v)
{
    (void)variable(address);
    if (program_address(address))
    {
        image_[address.location - EclProgram::origin + 2] = static_cast<std::uint8_t>(v);
        // A table may share the script image with code. Invalidate affected decoded
        // spans after a write so subsequent fetches see this machine's new bytes.
        for (auto it = instruction_spans_.begin(); it != instruction_spans_.end();)
        {
            if (address.location >= it->first && address.location < it->second)
                it = instruction_spans_.erase(it);
            else
                ++it;
        }
    }
    else
        variables_.at(address.location) = byte_address(address) ? v & 255 : v;
}

void EclMachine::write_string(EclAddress address, std::string_view v)
{
    validate_string(address, v);
    for (std::size_t n = 0; n <= v.size(); ++n)
        (void)variable(indexed_address(address, n));
    for (std::size_t n = 0; n <= v.size(); ++n)
        write(indexed_address(address, n), n == v.size() ? 0 : static_cast<unsigned char>(v[n]));
}

std::uint16_t EclMachine::value(const EclOperand &a) const
{
    // GetEclVariable returns the encoded length/address for strings in numeric roles.
    if (a.tag == 0 || a.tag == 2 || a.tag == 128 || a.tag == 129)
        return a.value;
    if (a.tag == 1 || a.tag == 3)
        return variable(EclAddress{a.value});
    throw EclError("Numeric operand required");
}

EclAddress EclMachine::destination(const EclOperand &a) const
{
    if (a.tag == 128)
        throw EclError("Inline text is not a destination");
    // String destinations in real scripts use tag 129 as well as numeric address tags.
    const EclAddress address{a.value};
    (void)variable(address); // Validate before any mutation or request.
    return address;
}

std::string EclMachine::text(const EclOperand &a) const
{
    if (a.tag == 128)
        return a.text;
    if (a.tag == 129)
        return string(EclAddress{a.value});
    return std::to_string(value(a));
}

EclInstruction EclMachine::decode(std::uint32_t address)
{
    if (address < program_->body_start())
        throw EclError("Target points into the entry table");
    auto result = program_->instruction(address, image_);
    for (const auto &[start, end] : instruction_spans_)
        if (start != address && address < end && result.next > start)
            throw EclError("Overlapping instruction or target inside operand data");
    instruction_spans_[address] = result.next;
    return result;
}

void EclMachine::jump(std::uint32_t address)
{
    (void)decode(address);
    pc_ = address;
}

bool EclMachine::start(std::size_t slot)
{
    if (slot >= program_->entries().size() ||
            (state_ != EclState::idle && state_ != EclState::completed))
        return false;
    pc_ = program_->entries()[slot];
    state_ = EclState::running;
    stack_.clear();
    trace_.clear();
    conditions_.fill(false);
    pending_.reset();
    destination_.reset();
    menu_values_.clear();
    diagnostic_.clear();
    total_instructions_ = 0;
    try
    {
        auto cursor = EclProgram::origin;
        for (std::size_t n = 0; n <= slot; ++n)
        {
            const auto entry = program_->instruction(cursor, image_);
            if (entry.opcode != 1)
                throw EclError("Modified entry is not GOTO");
            pc_ = encoded_address(entry.operands[0]);
            cursor = entry.next;
        }
    }
    catch (const EclError &e)
    {
        diagnostic_ = program_->source() + ": Invalid runtime entry table: " + e.what();
        state_ = EclState::faulted;
        return false;
    }
    return true;
}

bool EclMachine::start_at_for_inspection(std::uint32_t address)
{
    if (state_ != EclState::idle && state_ != EclState::completed)
        return false;
    try
    {
        (void)decode(address);
    }
    catch (const EclError &)
    {
        return false;
    }
    pc_ = address;
    state_ = EclState::running;
    stack_.clear();
    trace_.clear();
    conditions_.fill(false);
    pending_.reset();
    destination_.reset();
    menu_values_.clear();
    diagnostic_.clear();
    total_instructions_ = 0;
    return true;
}

void EclMachine::execute(const EclInstruction &i)
{
    const auto &spec = ecl_opcode(i.opcode);
    if (!spec.executable)
        throw EclError("Unsupported execution: " + std::string(spec.name));
    const auto &a = i.operands;
    // These are explicit no-ops in PoR PC, unlike later games. Never call a DOS pointer.
    if (i.opcode == 59)
    {
        pc_ = i.next;
        return;
    }
    if (i.opcode == 45)
    {
        const auto address = encoded_address(a[0]);
        if (address != 0x2C90 && address != 0x8000 && address != 0x8001 && address != 0xBA03 &&
                address != 0xC018 && address != 0xC01E)
        {
            pc_ = i.next;
            return;
        }
    }
    if (i.opcode == 56)
    {
        const auto id = value(a[0]) & 255;
        if (id != 0 && id != 8 && id != 9)
        {
            pc_ = i.next;
            return;
        }
    }
    if (spec.requires_host)
    {
        request_host(i);
        pc_ = i.next;
        return;
    }
    switch (i.opcode)
    {
    case 0:
        state_ = EclState::completed;
        stack_.clear();
        break;
    case 1:
        jump(encoded_address(a[0]));
        return;
    case 2:
        if (stack_.size() >= 256)
            throw EclError("Subroutine stack limit exceeded");
        jump(encoded_address(a[0]));
        stack_.push_back(i.next);
        return;
    case 19:
        if (stack_.empty())
            break; // PoR ignores an empty return stack.
        jump(stack_.back());
        stack_.pop_back();
        return;
    case 3:
    {
        if (a[0].tag >= 128 && a[1].tag >= 128)
        {
            const auto lhs = text(a[0]), rhs = text(a[1]);
            conditions_ = relation(lhs < rhs ? -1 : lhs > rhs ? 1 : 0);
        }
        else
        {
            const auto lhs = value(a[0]), rhs = value(a[1]);
            conditions_ = relation(lhs < rhs ? -1 : lhs > rhs ? 1 : 0);
        }
        break;
    }
    case 20:
    {
        const auto equals = [&](std::size_t n)
        {
            if (a[n].tag >= 128 && a[n + 1].tag >= 128)
                return text(a[n]) == text(a[n + 1]);
            return value(a[n]) == value(a[n + 1]);
        };
        const bool first = equals(0), second = equals(2);
        conditions_ = equality(first && second);
        break;
    }
    case 4:
    case 5:
    case 6:
    case 7:
    case 47:
    case 48:
    {
        const std::uint32_t lhs = value(a[0]), rhs = value(a[1]);
        const auto dest = destination(a[2]);
        std::uint32_t v = 0;
        if (i.opcode == 4)
            v = lhs + rhs;
        if (i.opcode == 5)
            v = rhs - lhs; // Reference SUBTRACT operand order.
        if (i.opcode == 6)
        {
            if (!rhs)
                throw EclError("Division by zero");
            v = lhs / rhs; // PoR does not save the remainder.
        }
        if (i.opcode == 7)
            v = lhs * rhs;
        if (i.opcode == 47)
            v = lhs & rhs;
        if (i.opcode == 48)
            v = lhs | rhs;
        write(dest, static_cast<std::uint16_t>(v & 65535));
        if (i.opcode == 47 || i.opcode == 48)
            conditions_ = equality(v == 0);
        break;
    }
    case 8:
    {
        // DOS increments a uint8 upper bound, saturating that increment at 255.
        const auto count = std::min<unsigned>((value(a[0]) & 255) + 1, 255);
        const auto dest = destination(a[1]);
        // Rejection sampling keeps the bounded result unbiased and portable.
        const std::uint32_t threshold = (0U - count) % count;
        std::uint32_t draw;
        do
        {
            draw = static_cast<std::uint32_t>(random_());
        }
        while (draw < threshold);
        write(dest, static_cast<std::uint16_t>(draw % count));
        break;
    }
    case 9:
    {
        const auto dest = destination(a[1]);
        if (a[0].tag == 128 || (a[0].tag == 129 && a[1].tag == 129))
            write_string(dest, text(a[0]));
        else
            write(dest, a[0].tag == 129 ? a[0].value : value(a[0]));
        break;
    }
    case 42:
    {
        const auto source = indexed_address(EclAddress{encoded_address(a[0])}, value(a[1]));
        const auto v = variable(source);
        const auto dest = destination(a[2]);
        write(dest, v);
        break;
    }
    case 53:
    {
        const auto v = value(a[0]);
        const auto dest = indexed_address(EclAddress{encoded_address(a[1])}, value(a[2]));
        write(dest, v);
        break;
    }
    case 15:
    case 16:
    {
        const auto dest = destination(a[1]);
        EclRequest request;
        request.kind = i.opcode == 15 ? EclRequestKind::input_number : EclRequestKind::input_string;
        // The PC 1.3 routines ignore the encoded limit; use 6 digits / 40 chars.
        request.input_limit = i.opcode == 15 ? 6 : 40;
        request.instruction = i;
        request.id = next_request_++;
        destination_ = dest;
        pending_ = std::move(request);
        state_ = EclState::waiting;
        break;
    }
    case 17:
    case 18:
    case 51:
    case 61:
    {
        EclRequest request;
        request.kind = EclRequestKind::text;
        if (i.opcode == 17 || i.opcode == 18)
            request.text = text(a[0]);
        if (i.opcode == 51)
            request.text = "\n";
        request.clear = i.opcode == 18 || i.opcode == 61;
        request.instruction = i;
        request.id = next_request_++;
        pending_ = std::move(request);
        state_ = EclState::waiting;
        break;
    }
    case 21:
    case 43:
    case 44:
    {
        const auto dest = destination(a[i.opcode == 44 ? 5 : 0]);
        const auto count = i.opcode == 44 ? 5 : a[spec.operands - 1].value;
        if (!count)
            throw EclError("Empty menu is not supported");
        EclRequest request;
        request.kind = EclRequestKind::menu;
        request.vertical = i.opcode == 21;
        if (request.vertical)
            request.text = text(a[1]);
        std::vector<std::uint16_t> results;
        if (i.opcode == 44)
        {
            request.choices = {"HAUGHTY", "SLY", "NICE", "MEEK", "ABUSIVE"};
            for (std::size_t n = 0; n < 5; ++n)
                results.push_back(value(a[n]));
        }
        else
        {
            for (std::size_t n = spec.operands; n < a.size(); ++n)
                request.choices.push_back(text(a[n]));
            for (std::uint16_t n = 0; n < count; ++n)
                results.push_back(n);
        }
        request.instruction = i;
        request.id = next_request_++;
        pending_ = std::move(request);
        destination_ = dest;
        menu_values_ = std::move(results);
        state_ = EclState::waiting;
        break;
    }
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    {
        if (!conditions_[i.opcode - 22])
        {
            pc_ = decode(i.next).next;
            return;
        }
        break;
    }
    case 37:
    case 38:
    {
        const auto index = value(a[0]) & 255;
        if (index < a[1].value)
        {
            if (i.opcode == 38 && stack_.size() >= 256)
                throw EclError("Subroutine stack limit exceeded");
            jump(encoded_address(a[2 + index]));
            if (i.opcode == 38)
                stack_.push_back(i.next);
            return;
        }
        break; // Zero-based dispatch; out-of-range falls through.
    }
    default:
        throw EclError("Missing opcode implementation");
    }
    pc_ = i.next;
}

EclHostArgument::EclHostArgument(std::uint16_t number) noexcept
    : kind_(EclArgumentKind::number), value_(number)
{
}

EclHostArgument::EclHostArgument(EclAddress address) noexcept
    : kind_(EclArgumentKind::address), value_(address.location)
{
}

EclHostArgument::EclHostArgument(std::string text) noexcept
    : kind_(EclArgumentKind::text), text_(std::move(text))
{
}

std::uint16_t EclHostArgument::number() const
{
    if (kind_ != EclArgumentKind::number)
        throw EclError("Host argument is not a number");
    return value_;
}

EclAddress EclHostArgument::address() const
{
    if (kind_ != EclArgumentKind::address)
        throw EclError("Host argument is not an address");
    return EclAddress{value_};
}

const std::string &EclHostArgument::text() const
{
    if (kind_ != EclArgumentKind::text)
        throw EclError("Host argument is not text");
    return text_;
}

void EclMachine::request_host(const EclInstruction &i)
{
    if (!host_opcodes_.contains(i.opcode))
        throw EclError("Host capability unavailable: " + std::string(ecl_opcode(i.opcode).name));
    EclRequest request;
    request.kind = EclRequestKind::host;
    request.instruction = i;
    for (std::size_t n = 0; n < i.operands.size(); ++n)
    {
        const auto &operand = i.operands[n];
        const auto op = i.opcode;
        const bool output =
            (op == 29 && n == 0) || (op == 30 && n >= 2) || op == 34 || (op == 41 && n == 3);
        const bool address =
            output || (op == 30 && n == 0) || (op == 35 && n < 2) || op == 45 || op == 60;
        const bool is_text = op == 57 || (op == 41 && n >= 9 && n <= 11);
        if (address)
            request.arguments.emplace_back(
                output ? destination(operand)
                : EclAddress{op == 60 && operand.tag == 129 ? operand.value
                             : encoded_address(operand)});
        else if (is_text)
            request.arguments.emplace_back(text(operand));
        else
        {
            std::uint16_t number = value(operand);
            if (op != 39 || n == 7)
                number &= 255;
            request.arguments.emplace_back(number);
        }
    }
    request.id = next_request_++;
    pending_ = std::move(request);
    state_ = EclState::waiting;
}

EclRunResult EclMachine::run(std::size_t budget)
{
    std::size_t count = 0;
    while (state_ == EclState::running && count < budget)
    {
        try
        {
            if (total_instructions_ >= 1000000)
                throw EclError("Invocation instruction limit exceeded");
            const auto instruction = decode(pc_);
            if (trace_.size() == 64)
                trace_.erase(trace_.begin());
            trace_.push_back(pc_);
            ++count;
            ++total_instructions_;
            execute(instruction);
        }
        catch (const EclError &error)
        {
            std::ostringstream message;
            message << program_->source() << " @ 0x" << std::hex << pc_ << " (record offset 0x"
                    << (pc_ - EclProgram::origin + 2) << ", stack depth " << std::dec
                    << stack_.size() << "): " << error.what();
            diagnostic_ = message.str();
            state_ = EclState::faulted;
        }
    }
    return {state_, count, pending_, diagnostic_};
}

bool EclMachine::resume(std::uint64_t id, std::optional<std::size_t> choice)
{
    if (state_ != EclState::waiting || !pending_ || pending_->id != id)
        return false;
    if (pending_->kind == EclRequestKind::menu)
    {
        if (!choice || *choice >= pending_->choices.size())
            return false;
        write(*destination_, menu_values_[*choice]);
    }
    else if (pending_->kind != EclRequestKind::text || choice)
        return false;
    finish_request();
    return true;
}

void EclMachine::finish_request()
{
    pending_.reset();
    destination_.reset();
    menu_values_.clear();
    state_ = EclState::running;
}

bool EclMachine::resume_input(std::uint64_t id, std::string_view input)
{
    if (state_ != EclState::waiting || !pending_ || pending_->id != id ||
            input.size() > pending_->input_limit)
        return false;
    try
    {
        if (pending_->kind == EclRequestKind::input_number)
        {
            std::uint16_t v{};
            const auto [end, ec] = std::from_chars(input.data(), input.data() + input.size(), v);
            if (input.empty() || ec != std::errc{} || end != input.data() + input.size())
                return false;
            write(*destination_, v);
        }
        else if (pending_->kind == EclRequestKind::input_string)
        {
            // The original input routine returns the typed text in upper case,
            // so answers such as OHLO match however they were typed.
            std::string typed(input.empty() ? std::string_view(" ") : input);
            for (auto &c : typed)
                if (c >= 'a' && c <= 'z')
                    c = static_cast<char>(c - 'a' + 'A');
            write_string(*destination_, typed);
        }
        else
            return false;
    }
    catch (const EclError &)
    {
        return false;
    }
    finish_request();
    return true;
}

unsigned EclMachine::host_random(std::uint64_t id, unsigned count)
{
    if (state_ != EclState::waiting || !pending_ || pending_->id != id ||
            pending_->kind != EclRequestKind::host || !count || count > 65536)
        throw EclError("Invalid host random request");
    return draw(count);
}

unsigned EclMachine::engine_random(unsigned count)
{
    if (state_ == EclState::waiting || !count || count > 65536)
        throw EclError("Invalid engine random request");
    return draw(count);
}

unsigned EclMachine::draw(unsigned count)
{
    const std::uint32_t threshold = (0U - count) % count;
    std::uint32_t value;
    do
    {
        value = static_cast<std::uint32_t>(random_());
    }
    while (value < threshold);
    return value % count;
}

bool EclMachine::resume_host(std::uint64_t id, const EclHostReply &reply)
{
    if (state_ != EclState::waiting || !pending_ || pending_->id != id ||
            pending_->kind != EclRequestKind::host)
        return false;
    const auto opcode = pending_->instruction->opcode;
    if ((opcode == 32) != static_cast<bool>(reply.next_program) ||
            (opcode == 50) != reply.conditions.has_value())
        return false;
    if (reply.conditions && ((*reply.conditions)[0] == (*reply.conditions)[1] ||
                             std::any_of(reply.conditions->begin() + 2, reply.conditions->end(),
                                         [](bool v)
{
    return v;
})))
    return false;
    // Validate the entire reply, including required output arguments, before applying writes.
    std::set<EclAddress> addresses;
    try
    {
        for (const auto &w : reply.writes)
        {
            (void)variable(w.address);
            if (!addresses.insert(w.address).second)
                return false;
        }
        for (std::size_t n = 0; n < pending_->arguments.size(); ++n)
            if (((opcode == 29 && n == 0) || (opcode == 30 && n >= 2) || opcode == 34 ||
                    (opcode == 41 && n == 3)) &&
                    !addresses.contains(pending_->arguments[n].address()))
                return false;
    }
    catch (const EclError &)
    {
        return false;
    }
    auto next_image = reply.next_program ? reply.next_program->raw() : std::vector<std::uint8_t> {};
    for (const auto &w : reply.writes)
        write(w.address, w.value);
    if (reply.conditions)
        conditions_ = *reply.conditions;
    finish_request();
    if (reply.next_program)
    {
        program_ = reply.next_program;
        image_ = std::move(next_image);
        instruction_spans_.clear();
        stack_.clear();
        conditions_.fill(false);
        for (auto &[address, v] : variables_)
            if (address >= 0x4A00 && address <= 0x4A1F)
                v = 0;
        state_ = EclState::completed;
        pc_ = program_->entries()[4];
        // Host installs map/party state, then starts slot 4; old-script fallthrough is forbidden.
    }
    return true;
}
} // namespace opengold::por
