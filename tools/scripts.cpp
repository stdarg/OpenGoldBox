#include "opengold/ecl_machine.h"
#include <charconv>
#include <deque>
#include <iostream>
#include <set>

using namespace opengold::por;
namespace
{
unsigned number(std::string_view s, unsigned max)
{
    unsigned n{};
    int base = 10;
    if (s.starts_with("0x") || s.starts_with("0X"))
    {
        s.remove_prefix(2);
        base = 16;
    }
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), n, base);
    if (ec != std::errc{} || end != s.data() + s.size() || n > max)
        throw EclError("Invalid numeric argument");
    return n;
}
std::shared_ptr<const EclProgram> demo()
{
    std::vector<std::uint8_t> bytes{0, 0};
    for (int i = 0; i < 5; ++i)
        bytes.insert(bytes.end(), {1, 1, 0x14, 0x99});
    // Compute 12, display it, then prompt for a choice and display the script result.
    std::vector<std::uint8_t> body{9, 0,    7,  1, 0, 0x97, 4,  0, 5, 1,    0, 0x97, 1,
                                   0, 0x97, 17, 1, 0, 0x97, 43, 1, 0, 0x97, 0, 2};
    for (std::string_view label : {"CONTINUE", "LEAVE"})
    {
        std::vector<std::uint8_t> packed;
        unsigned buffer = 0, bits = 0;
        for (const unsigned char ch : label)
        {
            buffer = (buffer << 6) | (ch & 63);
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                packed.push_back(static_cast<std::uint8_t>(buffer >> bits));
            }
        }
        if (bits)
            packed.push_back(static_cast<std::uint8_t>(buffer << (8 - bits)));
        body.push_back(128);
        body.push_back(static_cast<std::uint8_t>(packed.size()));
        body.insert(body.end(), packed.begin(), packed.end());
    }
    body.insert(body.end(), {17, 1, 0, 0x97, 0});
    bytes.insert(bytes.end(), body.begin(), body.end());
    return std::make_shared<const EclProgram>(EclProgram::decode(bytes, "built-in demo"));
}
int inspect(const EclProgram &p)
{
    using Edge = std::pair<std::uint32_t, std::optional<std::uint16_t>>;
    std::deque<Edge> queue;
    for (auto entry : p.entries())
        queue.emplace_back(entry, std::nullopt);
    std::set<std::uint32_t> seen;
    std::map<std::uint32_t, std::uint32_t> spans;
    bool failed = false;
    while (!queue.empty())
    {
        const auto [pc, speculative] = queue.front();
        queue.pop_front();
        if (!seen.insert(pc).second)
            continue;
        try
        {
            if (pc < p.body_start())
                throw EclError("Target in entry table");
            const auto i = p.instruction(pc);
            for (const auto &[start, end] : spans)
                if (pc < end && i.next > start)
                    throw EclError("Overlapping instructions");
            spans[pc] = i.next;
            const auto &spec = ecl_opcode(i.opcode);
            std::cout << "0x" << std::hex << pc << std::dec << " " << spec.name;
            for (const auto &a : i.operands)
            {
                if (a.tag == 128)
                    std::cout << " \"" << a.text << '"';
                else
                    std::cout << " {tag=" << unsigned(a.tag) << ",value=" << a.value << '}';
            }
            if (!spec.executable)
                std::cout << " [execution unsupported]";
            else if (spec.requires_host)
                std::cout << " [host required]";
            std::cout << '\n';
            if (i.opcode == 1 || i.opcode == 2)
                queue.emplace_back(i.operands[0].value, speculative);
            if (i.opcode == 37 || i.opcode == 38)
                for (std::size_t n = 2; n < i.operands.size(); ++n)
                    queue.emplace_back(i.operands[n].value, speculative);
            if (i.opcode >= 22 && i.opcode <= 27)
                queue.emplace_back(p.instruction(i.next).next, speculative);
            if (i.opcode != 0 && i.opcode != 1 && i.opcode != 19 && i.opcode != 32)
                queue.emplace_back(i.next, i.opcode == 37 ? std::optional(i.address) : speculative);
        }
        catch (const EclError &e)
        {
            failed = true;
            std::cerr << p.source() << " @ 0x" << std::hex << pc;
            if (speculative)
                std::cerr << " (possible ON GOTO fallthrough from 0x" << *speculative << ")";
            std::cerr << std::dec << ": " << e.what() << '\n';
        }
    }
    return failed ? 1 : 0;
}
int run(EclMachine &vm)
{
    while (true)
    {
        const auto result = vm.run();
        if (result.state == EclState::completed)
        {
            std::cout << "Script completed.\n";
            return 0;
        }
        if (result.state == EclState::faulted)
        {
            std::cerr << result.diagnostic << "\nRecent addresses:";
            for (const auto pc : vm.trace())
                std::cerr << " 0x" << std::hex << pc;
            std::cerr << std::dec << '\n';
            return 1;
        }
        if (result.state == EclState::waiting)
        {
            const auto &r = *result.request;
            if (r.clear)
                std::cout << "[clear text]\n";
            std::cout << r.text;
            if (!r.instruction || r.instruction->opcode != 51)
                std::cout << '\n';
            if (r.kind == EclRequestKind::text)
            {
                (void)vm.resume(r.id);
                continue;
            }
            if (r.kind == EclRequestKind::host)
            {
                std::cerr << "Console has no service for " << ecl_opcode(r.instruction->opcode).name
                          << '\n';
                return 2;
            }
            for (std::size_t i = 0; i < r.choices.size(); ++i)
                std::cout << i + 1 << ") " << r.choices[i] << '\n';
            while (true)
            {
                if (r.kind == EclRequestKind::menu)
                    std::cout << "Choice (1-" << r.choices.size() << "): ";
                else
                    std::cout << "Input (up to " << r.input_limit
                              << (r.kind == EclRequestKind::input_number ? " digits): "
                                                                         : " characters): ");
                std::cout << std::flush;
                std::string input;
                if (!std::getline(std::cin, input))
                {
                    std::cerr << "Input ended while script was waiting.\n";
                    return 2;
                }
                // PowerShell/.NET redirected UTF-8 input may include a BOM.
                std::string_view selection = input;
                if (selection.starts_with("\xEF\xBB\xBF"))
                    selection.remove_prefix(3);
                if (r.kind == EclRequestKind::input_string ||
                    r.kind == EclRequestKind::input_number)
                {
                    if (vm.resume_input(r.id, selection))
                        break;
                    std::cout << "Invalid input or unbound destination buffer.\n";
                    continue;
                }
                while (!selection.empty() &&
                       (selection.front() == ' ' || selection.front() == '\t'))
                    selection.remove_prefix(1);
                while (!selection.empty() && (selection.back() == ' ' || selection.back() == '\t' ||
                                              selection.back() == '\r'))
                    selection.remove_suffix(1);
                try
                {
                    const auto n = number(selection, static_cast<unsigned>(r.choices.size()));
                    if (n && vm.resume(r.id, n - 1))
                        break;
                }
                catch (const EclError &)
                { /* Reprompt without losing continuation. */
                }
                std::cout << "Invalid choice.\n";
            }
        }
    }
}
} // namespace
int main(int argc, char **argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--demo")
        {
            EclMachine vm(demo());
            vm.bind_variable(0x9700, 0);
            (void)vm.start(0);
            return run(vm);
        }
        if (argc < 3)
            throw EclError(
                "Usage: opengold_scripts --demo | --list GAME_DIR | --inspect GAME_DIR ARCHIVE RECORD | --run GAME_DIR ARCHIVE RECORD ENTRY [ADDRESS=VALUE ...]\nNumbers are decimal or 0x-prefixed hex. Entry slots are 0..4; startup is slot 4 (reference role).");
        const std::string mode = argv[1];
        if (mode != "--list" && mode != "--inspect" && mode != "--run")
            throw EclError("Unknown mode");
        if ((mode == "--list" && argc != 3) || (mode == "--inspect" && argc != 5) ||
            (mode == "--run" && argc < 6))
            throw EclError("Incorrect argument count");
        const auto catalog = EclCatalog::load(argv[2]);
        if (mode == "--list")
        {
            for (const auto &[id, p] : catalog.all())
            {
                std::cout << p->source() << " bytes=" << p->raw().size() << " entries:";
                for (auto entry : p->entries())
                    std::cout << " 0x" << std::hex << entry;
                std::cout << std::dec << '\n';
            }
            std::cout << catalog.all().size()
                      << " programs (entry tables validated; bodies decoded on demand).\n";
            return 0;
        }
        std::string archive = argv[3];
        for (auto &c : archive)
            if (c >= 'a' && c <= 'z')
                c -= 'a' - 'A';
        const auto program =
            catalog.find({archive, static_cast<std::uint8_t>(number(argv[4], 255))});
        if (!program)
            throw EclError("Script not found");
        if (mode == "--inspect")
            return inspect(*program);
        EclMachine vm(program);
        for (int i = 6; i < argc; ++i)
        {
            const std::string_view binding = argv[i];
            const auto equal = binding.find('=');
            if (equal == std::string_view::npos)
                throw EclError("Expected ADDRESS=VALUE");
            vm.bind_variable(static_cast<std::uint16_t>(number(binding.substr(0, equal), 65535)),
                             static_cast<std::uint16_t>(number(binding.substr(equal + 1), 65535)));
        }
        (void)vm.start(number(argv[5], 4));
        return run(vm);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
