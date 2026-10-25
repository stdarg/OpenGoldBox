#include "machine.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace opengold::probe
{
namespace
{
constexpr std::uint16_t bios_segment = 0xF000;
constexpr std::uint16_t stub_base = 0x0100;  // `0F FF n CF` for vector n, four bytes each
constexpr std::uint16_t timer_stub = 0x0600; // `0F FF 08 CD 1C CF`
constexpr std::uint16_t memory_top = 0xA000;
constexpr std::uint16_t environment_segment = 0x0700;
constexpr std::uint16_t program_psp = 0x0800;
constexpr std::uint32_t ega_window = 0xA0000;

std::string upper(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c)
                   {
                       return static_cast<char>(std::toupper(c));
                   });
    return text;
}

// DOS wildcard match of one 8.3 name part: '*' matches the rest, '?' one character.
bool part_matches(const std::string &pattern, const std::string &name)
{
    std::size_t n = 0;
    for (std::size_t p = 0; p < pattern.size(); ++p)
    {
        if (pattern[p] == '*')
        {
            return true;
        }
        if (pattern[p] == '?')
        {
            if (n < name.size())
            {
                ++n;
            }
            continue;
        }
        if (n >= name.size() || pattern[p] != name[n])
        {
            return false;
        }
        ++n;
    }
    return n == name.size();
}

bool wildcard_matches(const std::string &pattern, const std::string &name)
{
    const auto split = [](const std::string &text)
    {
        const auto dot = text.find('.');
        return dot == std::string::npos ? std::pair{text, std::string{}}
                                        : std::pair{text.substr(0, dot), text.substr(dot + 1)};
    };
    const auto [pattern_name, pattern_ext] = split(pattern);
    const auto [file_name, file_ext] = split(name);
    return part_matches(pattern_name, file_name) &&
           (pattern_ext.empty() && pattern.find('.') == std::string::npos
                ? file_ext.empty() || pattern_name == "*"
                : part_matches(pattern_ext, file_ext));
}

// The directory entry whose name matches `name` regardless of case.
std::optional<std::filesystem::path> find_case_insensitive(const std::filesystem::path &directory,
                                                           const std::string &name)
{
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error))
    {
        return std::nullopt;
    }
    for (const auto &entry : std::filesystem::directory_iterator(directory, error))
    {
        if (upper(entry.path().filename().string()) == upper(name))
        {
            return entry.path();
        }
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> resolve(const std::filesystem::path &root,
                                             const std::string &relative)
{
    auto path = root;
    std::stringstream parts(relative);
    std::string part;
    while (std::getline(parts, part, '/'))
    {
        const auto found = find_case_insensitive(path, part);
        if (!found)
        {
            return std::nullopt;
        }
        path = *found;
    }
    return path;
}
} // namespace

Machine::Machine(Options options) : options_(std::move(options)), ram_(0x100000, 0)
{
    std::filesystem::create_directories(options_.scratch_directory);
    install_bios();
    next_tick_ = options_.instructions_per_tick;
}

void Machine::install_bios()
{
    for (unsigned n = 0; n < 256; ++n)
    {
        const std::uint32_t stub =
            Cpu::linear(bios_segment, static_cast<std::uint16_t>(stub_base + n * 4));
        ram_[stub] = 0x0F;
        ram_[stub + 1] = 0xFF;
        ram_[stub + 2] = static_cast<std::uint8_t>(n);
        ram_[stub + 3] = 0xCF;
        const auto offset = n == 8 ? timer_stub : static_cast<std::uint16_t>(stub_base + n * 4);
        ram_[n * 4] = static_cast<std::uint8_t>(offset);
        ram_[n * 4 + 1] = static_cast<std::uint8_t>(offset >> 8U);
        ram_[n * 4 + 2] = static_cast<std::uint8_t>(bios_segment);
        ram_[n * 4 + 3] = static_cast<std::uint8_t>(bios_segment >> 8U);
    }
    const std::uint32_t timer = Cpu::linear(bios_segment, timer_stub);
    const std::array<std::uint8_t, 6> timer_code{0x0F, 0xFF, 0x08, 0xCD, 0x1C, 0xCF};
    std::copy(timer_code.begin(), timer_code.end(), ram_.begin() + timer);
    // An IBM PC (model byte FF), not a Tandy.
    ram_[0xFFFFE] = 0xFF;
    const std::string date = "01/01/88";
    std::copy(date.begin(), date.end(), ram_.begin() + 0xFFFF5);

    const auto put16 = [&](std::uint32_t at, std::uint16_t value)
    {
        ram_[at] = static_cast<std::uint8_t>(value);
        ram_[at + 1] = static_cast<std::uint8_t>(value >> 8U);
    };
    put16(0x410, 0x0021); // equipment: a floppy drive, 80-column colour video
    put16(0x413, 640);    // conventional memory in KB
    put16(0x41A, 0x001E); // keyboard buffer head
    put16(0x41C, 0x001E); // and tail
    put16(0x480, 0x001E); // buffer start
    put16(0x482, 0x003E); // and end
    ram_[0x449] = video_mode_;
    put16(0x44A, 80);
    put16(0x463, 0x3D4);
    ram_[0x484] = 24;
    ram_[0x487] = 0x60;
    ram_[0x488] = 0x09;
}

void Machine::load(const std::string &dos_path)
{
    const auto relative = game_relative(dos_path);
    const auto path = relative ? resolve(options_.game_directory, *relative) : std::nullopt;
    if (!path)
    {
        throw std::runtime_error("Program not found: " + dos_path);
    }
    std::ifstream file(*path, std::ios::binary);
    const std::vector<std::uint8_t> exe((std::istreambuf_iterator<char>(file)),
                                        std::istreambuf_iterator<char>());
    const auto word = [&](std::size_t at)
    {
        return static_cast<std::uint16_t>(exe.at(at) | (exe.at(at + 1) << 8U));
    };
    if (exe.size() < 28 || exe[0] != 'M' || exe[1] != 'Z')
    {
        throw std::runtime_error("Not an MZ executable: " + dos_path);
    }
    const std::size_t pages = word(4), last = word(2);
    const std::size_t image_end = last ? (pages - 1) * 512 + last : pages * 512;
    const std::size_t header = std::size_t{word(8)} * 16;
    const std::uint16_t load_segment = program_psp + 0x10;
    std::copy(exe.begin() + static_cast<std::ptrdiff_t>(header),
              exe.begin() + static_cast<std::ptrdiff_t>(image_end),
              ram_.begin() + std::uint32_t{load_segment} * 16);
    for (std::size_t n = 0; n < word(6); ++n)
    {
        const std::size_t entry = word(0x18) + n * 4;
        const auto at =
            Cpu::linear(static_cast<std::uint16_t>(load_segment + word(entry + 2)), word(entry));
        const auto value = static_cast<std::uint16_t>(ram_[at] | (ram_[at + 1] << 8U));
        const auto relocated = static_cast<std::uint16_t>(value + load_segment);
        ram_[at] = static_cast<std::uint8_t>(relocated);
        ram_[at + 1] = static_cast<std::uint8_t>(relocated >> 8U);
    }

    // Environment, with the program's own path after it as DOS 3+ gives.
    std::string environment =
        std::string("COMSPEC=C:\\COMMAND.COM") + '\0' + "PATH=C:\\" + '\0' + '\0';
    environment += std::string("\x01\x00", 2) + upper(dos_path) + '\0';
    std::copy(environment.begin(), environment.end(),
              ram_.begin() + std::uint32_t{environment_segment} * 16);

    psp_ = program_psp;
    const std::uint32_t psp = std::uint32_t{psp_} * 16;
    ram_[psp] = 0xCD;
    ram_[psp + 1] = 0x20;
    ram_[psp + 2] = static_cast<std::uint8_t>(memory_top);
    ram_[psp + 3] = static_cast<std::uint8_t>(memory_top >> 8U);
    ram_[psp + 0x2C] = static_cast<std::uint8_t>(environment_segment);
    ram_[psp + 0x2D] = static_cast<std::uint8_t>(environment_segment >> 8U);
    ram_[psp + 0x50] = 0xCD;
    ram_[psp + 0x51] = 0x21;
    ram_[psp + 0x52] = 0xCB;
    std::fill(ram_.begin() + psp + 0x5D, ram_.begin() + psp + 0x68, ' ');
    std::fill(ram_.begin() + psp + 0x6D, ram_.begin() + psp + 0x78, ' ');
    ram_[psp + 0x80] = 0;
    ram_[psp + 0x81] = 0x0D;
    dta_segment_ = psp_;
    dta_offset_ = 0x80;

    blocks_.clear();
    blocks_[environment_segment] = 0x00FF;
    blocks_[psp_] = static_cast<std::uint16_t>(memory_top - psp_);

    cpu_.set_seg(Seg::cs, static_cast<std::uint16_t>(load_segment + word(0x16)));
    cpu_.set_ip(word(0x14));
    cpu_.set_seg(Seg::ss, static_cast<std::uint16_t>(load_segment + word(0x0E)));
    cpu_.set_reg(Reg::sp, word(0x10));
    cpu_.set_seg(Seg::ds, psp_);
    cpu_.set_seg(Seg::es, psp_);
    cpu_.set_flags(Flag::interrupt);
}

namespace
{
std::uint64_t tick_interval(std::uint64_t per_tick, std::uint16_t divisor)
{
    return divisor ? std::max<std::uint64_t>(200, per_tick * divisor / 65536) : per_tick;
}
} // namespace

void Machine::run(std::uint64_t count, const std::function<bool()> &stop)
{
    const auto end = instructions_ + count;
    while (instructions_ < end && !exit_code_)
    {
        if (instructions_ >= next_tick_)
        {
            irq_pending_[0] = true;
            next_tick_ += tick_interval(options_.instructions_per_tick, pit_divisor_);
        }
        const bool buffer_empty = ram_[0x41A] == ram_[0x41C];
        if (!keys_.empty() && !irq_pending_[1] && instructions_ >= key_ready_at_ &&
            (keys_.front().first & 0x80U || buffer_empty))
        {
            port60_ = keys_.front().first;
            pending_ascii_ = keys_.front().second;
            keys_.pop_front();
            irq_pending_[1] = true;
            idle_polls_ = 0;
            // A key is held a few ticks, for programs that watch port 60h.
            key_ready_at_ =
                instructions_ + (port60_ & 0x80U ? 1 : 4) * options_.instructions_per_tick;
        }
        if (irq_pending_[0] && !(pic_mask_ & 1U) && cpu_.request_interrupt(8))
        {
            irq_pending_[0] = false;
        }
        else if (irq_pending_[1] && !(pic_mask_ & 2U) && cpu_.request_interrupt(9))
        {
            irq_pending_[1] = false;
        }
        if (cpu_.halted())
        {
            instructions_ = std::min(end, std::max(instructions_ + 1, next_tick_));
            continue;
        }
        if (before_instruction_)
        {
            before_instruction_(cpu_);
        }
        cpu_.step();
        ++instructions_;
        if (stop && stop())
        {
            return;
        }
    }
}

void Machine::press(std::uint8_t scan_code, std::uint8_t ascii)
{
    keys_.emplace_back(scan_code, ascii);
    keys_.emplace_back(static_cast<std::uint8_t>(scan_code | 0x80U), std::uint8_t{0});
}

void Machine::watch(std::uint32_t first, std::uint32_t last, Watch callback)
{
    watches_.push_back({first, last, std::move(callback)});
}

std::uint8_t Machine::read(std::uint32_t linear)
{
    std::uint8_t value = 0;
    if (linear >= ega_window && linear < ega_window + 0x10000)
    {
        value = ega_.read(static_cast<std::uint16_t>(linear - ega_window));
    }
    else
    {
        value = ram_[linear];
    }
    for (const auto &w : watches_)
    {
        if (linear >= w.first && linear <= w.last)
        {
            w.callback(linear, value, false);
        }
    }
    return value;
}

void Machine::write(std::uint32_t linear, std::uint8_t value)
{
    for (const auto &w : watches_)
    {
        if (linear >= w.first && linear <= w.last)
        {
            w.callback(linear, value, true);
        }
    }
    if (linear >= ega_window && linear < ega_window + 0x10000)
    {
        ega_.write(static_cast<std::uint16_t>(linear - ega_window), value);
    }
    else if (linear < 0xF0000)
    {
        ram_[linear] = value;
    }
}

std::uint8_t Machine::in(std::uint16_t port)
{
    std::uint8_t value = 0xFF;
    if (ega_.in(port, value))
    {
        return value;
    }
    switch (port)
    {
    case 0x21:
        return pic_mask_;
    case 0x60:
        return port60_;
    case 0x61:
        return port61_;
    case 0x64:
        return 0x1C;
    case 0x40:
        // A counter that moves, for programs timing short delays.
        return static_cast<std::uint8_t>(~(instructions_ >> 2U));
    default:
        return 0xFF;
    }
}

void Machine::out(std::uint16_t port, std::uint8_t value)
{
    if (ega_.out(port, value))
    {
        return;
    }
    switch (port)
    {
    case 0x21:
        pic_mask_ = value;
        return;
    case 0x43:
        pit_byte_ = 0;
        return;
    case 0x40:
        pit_divisor_ = pit_byte_ == 0
                           ? static_cast<std::uint16_t>((pit_divisor_ & 0xFF00U) | value)
                           : static_cast<std::uint16_t>((pit_divisor_ & 0x00FFU) | (value << 8U));
        pit_byte_ ^= 1U;
        return;
    case 0x61:
        port61_ = value;
        return;
    default:
        return;
    }
}

void Machine::set_stacked_flag(Flag flag, bool on)
{
    const auto ss = cpu_.seg(Seg::ss);
    const auto at = static_cast<std::uint16_t>(cpu_.reg(Reg::sp) + 4);
    auto flags = cpu_.read16(ss, at);
    flags = static_cast<std::uint16_t>(on ? flags | flag : flags & ~flag);
    cpu_.write16(ss, at, flags);
}

void Machine::return_carry(bool failed, std::uint16_t error)
{
    set_stacked_flag(carry, failed);
    if (failed)
    {
        cpu_.set_reg(Reg::ax, error);
    }
}

void Machine::host_interrupt(std::uint8_t number)
{
    const auto ax = cpu_.reg(Reg::ax);
    const auto ah = static_cast<std::uint8_t>(ax >> 8U);
    ++service_counts_[{number, ah}];
    switch (number)
    {
    case 0x08:
        timer_tick();
        return;
    case 0x09:
        keyboard_interrupt();
        return;
    case 0x10:
        video_service();
        return;
    case 0x11:
        cpu_.set_reg(Reg::ax, 0x0021);
        return;
    case 0x12:
        cpu_.set_reg(Reg::ax, 640);
        return;
    case 0x16:
        keyboard_service();
        return;
    case 0x1A:
        if (ah == 0)
        {
            const auto ticks = static_cast<std::uint32_t>(
                ram_[0x46C] | (ram_[0x46D] << 8U) | (ram_[0x46E] << 16U) | (ram_[0x46F] << 24U));
            cpu_.set_reg(Reg::cx, static_cast<std::uint16_t>(ticks >> 16U));
            cpu_.set_reg(Reg::dx, static_cast<std::uint16_t>(ticks));
            cpu_.set_reg(Reg::ax, 0);
        }
        else
        {
            set_stacked_flag(carry, true);
        }
        return;
    case 0x15:
        cpu_.set_reg(Reg::ax, static_cast<std::uint16_t>((ax & 0x00FFU) | 0x8600U));
        set_stacked_flag(carry, true);
        return;
    case 0x20:
        exit_code_ = 0;
        return;
    case 0x21:
        dos_service();
        return;
    case 0x24:
        cpu_.set_reg(Reg::ax, 3); // fail the operation
        return;
    case 0x2F:
        cpu_.set_reg(Reg::ax, static_cast<std::uint16_t>(ax & 0xFF00U));
        return;
    case 0x33:
        cpu_.set_reg(Reg::ax, 0); // no mouse
        return;
    default:
        return;
    }
}

void Machine::timer_tick()
{
    for (std::uint32_t at = 0x46C; at < 0x470; ++at)
    {
        if (++ram_[at] != 0)
        {
            break;
        }
    }
}

void Machine::keyboard_interrupt()
{
    if (port60_ & 0x80U)
    {
        return;
    }
    const auto head = static_cast<std::uint16_t>(ram_[0x41A] | (ram_[0x41B] << 8U));
    const auto tail = static_cast<std::uint16_t>(ram_[0x41C] | (ram_[0x41D] << 8U));
    auto next = static_cast<std::uint16_t>(tail + 2);
    if (next >= 0x3E)
    {
        next = 0x1E;
    }
    if (next == head)
    {
        return;
    }
    ram_[0x400 + tail] = pending_ascii_;
    ram_[0x400 + tail + 1] = port60_;
    ram_[0x41C] = static_cast<std::uint8_t>(next);
    ram_[0x41D] = 0;
}

void Machine::keyboard_service()
{
    const auto ah = static_cast<std::uint8_t>(cpu_.reg(Reg::ax) >> 8U);
    const auto head = static_cast<std::uint16_t>(ram_[0x41A] | (ram_[0x41B] << 8U));
    const auto tail = static_cast<std::uint16_t>(ram_[0x41C] | (ram_[0x41D] << 8U));
    const bool empty = head == tail;
    const auto key =
        static_cast<std::uint16_t>(ram_[0x400 + head] | (ram_[0x400 + head + 1] << 8U));
    switch (ah & 0xEFU)
    {
    case 0x00:
        if (empty)
        {
            // Wait with interrupts on, as the BIOS does, by running the trap again.
            note_idle_poll();
            cpu_.set_flag(Flag::interrupt, true);
            cpu_.set_ip(static_cast<std::uint16_t>(cpu_.ip() - 3));
            return;
        }
        cpu_.set_reg(Reg::ax, key);
        {
            auto next = static_cast<std::uint16_t>(head + 2);
            ram_[0x41A] = static_cast<std::uint8_t>(next >= 0x3E ? 0x1E : next);
            ram_[0x41B] = 0;
        }
        idle_polls_ = 0;
        return;
    case 0x01:
        set_stacked_flag(zero, empty);
        if (empty)
        {
            note_idle_poll();
        }
        else
        {
            cpu_.set_reg(Reg::ax, key);
        }
        return;
    case 0x02:
        cpu_.set_reg(Reg::ax,
                     static_cast<std::uint16_t>((cpu_.reg(Reg::ax) & 0xFF00U) | ram_[0x417]));
        return;
    default:
        return;
    }
}

void Machine::video_service()
{
    const auto ax = cpu_.reg(Reg::ax);
    const auto ah = static_cast<std::uint8_t>(ax >> 8U), al = static_cast<std::uint8_t>(ax);
    const auto bx = cpu_.reg(Reg::bx);
    switch (ah)
    {
    case 0x00:
        video_mode_ = al & 0x7FU;
        ram_[0x449] = video_mode_;
        ram_[0x44A] = video_mode_ == 0x0D || video_mode_ <= 1 ? 40 : 80;
        for (unsigned n = 0; n < 16; ++n)
        {
            ega_.set_palette(n, static_cast<std::uint8_t>(n < 8 ? n : n + 8));
        }
        return;
    case 0x0E:
        console_ += static_cast<char>(al);
        return;
    case 0x0F:
        cpu_.set_reg(Reg::ax, static_cast<std::uint16_t>((ram_[0x44A] << 8U) | video_mode_));
        cpu_.set_reg(Reg::bx, static_cast<std::uint16_t>(bx & 0x00FFU));
        return;
    case 0x10:
        if (al == 0x00)
        {
            ega_.set_palette(bx & 0x0FU, static_cast<std::uint8_t>(bx >> 8U));
        }
        else if (al == 0x01)
        {
            ega_.set_border(static_cast<std::uint8_t>(bx >> 8U));
        }
        else if (al == 0x02)
        {
            const auto es = cpu_.seg(Seg::es), dx = cpu_.reg(Reg::dx);
            for (unsigned n = 0; n < 16; ++n)
            {
                ega_.set_palette(n, cpu_.read8(es, static_cast<std::uint16_t>(dx + n)));
            }
            ega_.set_border(cpu_.read8(es, static_cast<std::uint16_t>(dx + 16)));
        }
        return;
    case 0x12:
        if ((bx & 0xFFU) == 0x10)
        {
            cpu_.set_reg(Reg::bx, 0x0003); // colour, 256 KB
            cpu_.set_reg(Reg::cx, 0x0009);
        }
        return;
    case 0x1A:
        cpu_.set_reg(Reg::ax, static_cast<std::uint16_t>(ax & 0xFF00U)); // no VGA
        return;
    default:
        return;
    }
}

std::string Machine::read_string(std::uint16_t segment, std::uint16_t offset) const
{
    std::string text;
    for (std::uint16_t n = 0; n < 128; ++n)
    {
        const auto c = ram_[Cpu::linear(segment, static_cast<std::uint16_t>(offset + n))];
        if (c == 0)
        {
            break;
        }
        text += static_cast<char>(c);
    }
    return text;
}

std::optional<std::string> Machine::game_relative(const std::string &dos_path) const
{
    auto path = upper(dos_path);
    std::replace(path.begin(), path.end(), '/', '\\');
    if (path.size() >= 2 && path[1] == ':')
    {
        if (path[0] != 'C')
        {
            return std::nullopt;
        }
        path = path.substr(2);
    }
    if (path.empty() || path[0] != '\\')
    {
        path = "\\" + current_directory_ + (current_directory_.empty() ? "" : "\\") + path;
    }
    std::vector<std::string> parts;
    std::stringstream stream(path);
    std::string part;
    while (std::getline(stream, part, '\\'))
    {
        if (part.empty() || part == ".")
        {
            continue;
        }
        if (part == "..")
        {
            if (!parts.empty())
            {
                parts.pop_back();
            }
            continue;
        }
        parts.push_back(part);
    }
    if (parts.empty() || parts.front() != "POOLRAD")
    {
        return std::nullopt;
    }
    std::string relative;
    for (std::size_t n = 1; n < parts.size(); ++n)
    {
        relative += (n > 1 ? "/" : "") + parts[n];
    }
    return relative;
}

std::filesystem::path Machine::read_path(const std::string &relative) const
{
    if (deleted_.contains(relative))
    {
        return {};
    }
    if (const auto written = resolve(options_.scratch_directory, relative))
    {
        return *written;
    }
    return original_path(relative).value_or(std::filesystem::path{});
}

std::optional<std::filesystem::path> Machine::original_path(const std::string &relative) const
{
    if (options_.fresh_saves && (relative == "SAVE" || relative.starts_with("SAVE/")))
    {
        return std::nullopt;
    }
    return resolve(options_.game_directory, relative);
}

std::filesystem::path Machine::write_path(const std::string &relative) const
{
    if (const auto written = resolve(options_.scratch_directory, relative))
    {
        return *written;
    }
    const auto path = options_.scratch_directory / relative;
    std::filesystem::create_directories(path.parent_path());
    return path;
}

int Machine::open_file(const std::string &dos_path, std::uint8_t mode, bool create)
{
    const auto relative = game_relative(dos_path);
    if (!relative || relative->empty())
    {
        return -3;
    }
    std::filesystem::path path;
    auto flags = std::ios::binary | std::ios::in;
    if (create)
    {
        path = write_path(*relative);
        std::ofstream(path, std::ios::binary | std::ios::trunc);
        deleted_.erase(*relative);
        flags |= std::ios::out;
    }
    else if ((mode & 3U) == 0)
    {
        path = read_path(*relative);
        if (path.empty() || !std::filesystem::is_regular_file(path))
        {
            return -2;
        }
    }
    else
    {
        const auto original = read_path(*relative);
        if (original.empty() || !std::filesystem::is_regular_file(original))
        {
            return -2;
        }
        path = write_path(*relative);
        if (original != path)
        {
            std::filesystem::copy_file(original, path,
                                       std::filesystem::copy_options::overwrite_existing);
        }
        flags |= std::ios::out;
    }
    auto file = std::make_shared<OpenFile>();
    file->stream.open(path, flags);
    file->dos_path = upper(dos_path);
    if (!file->stream)
    {
        return -5;
    }
    int handle = 5;
    while (files_.contains(handle))
    {
        ++handle;
    }
    files_[handle] = std::move(file);
    opened_files_.push_back(upper(dos_path) + (create        ? " (created)"
                                               : (mode & 3U) ? " (written)"
                                                             : ""));
    return handle;
}

void Machine::dos_service()
{
    const auto ax = cpu_.reg(Reg::ax);
    const auto ah = static_cast<std::uint8_t>(ax >> 8U), al = static_cast<std::uint8_t>(ax);
    const auto ds = cpu_.seg(Seg::ds), dx = cpu_.reg(Reg::dx);
    switch (ah)
    {
    case 0x00:
        exit_code_ = 0;
        return;
    case 0x01:
    case 0x06:
    case 0x07:
    case 0x08:
    {
        if (ah == 0x06 && (dx & 0xFFU) != 0xFF)
        {
            console_ += static_cast<char>(dx);
            return;
        }
        // DOS hands over an extended key as 0, then its scan code.
        if (pending_scan_)
        {
            cpu_.set_reg(Reg::ax, static_cast<std::uint16_t>((ax & 0xFF00U) | *pending_scan_));
            pending_scan_.reset();
            set_stacked_flag(zero, false);
            idle_polls_ = 0;
            return;
        }
        const auto head = static_cast<std::uint16_t>(ram_[0x41A] | (ram_[0x41B] << 8U));
        const auto tail = static_cast<std::uint16_t>(ram_[0x41C] | (ram_[0x41D] << 8U));
        if (head == tail)
        {
            if (ah == 0x06)
            {
                set_stacked_flag(zero, true);
                note_idle_poll();
                return;
            }
            note_idle_poll();
            cpu_.set_flag(Flag::interrupt, true);
            cpu_.set_ip(static_cast<std::uint16_t>(cpu_.ip() - 3));
            return;
        }
        cpu_.set_reg(Reg::ax, static_cast<std::uint16_t>((ax & 0xFF00U) | ram_[0x400 + head]));
        if (ram_[0x400 + head] == 0)
        {
            pending_scan_ = ram_[0x400 + head + 1];
        }
        const auto next = static_cast<std::uint16_t>(head + 2);
        ram_[0x41A] = static_cast<std::uint8_t>(next >= 0x3E ? 0x1E : next);
        set_stacked_flag(zero, false);
        idle_polls_ = 0;
        return;
    }
    case 0x02:
        console_ += static_cast<char>(dx);
        return;
    case 0x09:
        for (std::uint16_t n = 0; n < 1024; ++n)
        {
            const auto c = cpu_.read8(ds, static_cast<std::uint16_t>(dx + n));
            if (c == '$')
            {
                break;
            }
            console_ += static_cast<char>(c);
        }
        return;
    case 0x0B:
        cpu_.set_reg(Reg::ax, static_cast<std::uint16_t>(
                                  (ax & 0xFF00U) |
                                  (ram_[0x41A] != ram_[0x41C] || pending_scan_ ? 0xFF : 0)));
        if (ram_[0x41A] == ram_[0x41C])
        {
            note_idle_poll();
        }
        return;
    case 0x0D:
        return;
    case 0x0E:
        cpu_.set_reg(Reg::ax, static_cast<std::uint16_t>((ax & 0xFF00U) | 26));
        return;
    case 0x19:
        cpu_.set_reg(Reg::ax, static_cast<std::uint16_t>((ax & 0xFF00U) | 2));
        return;
    case 0x1A:
        dta_segment_ = ds;
        dta_offset_ = dx;
        return;
    case 0x25:
        cpu_.write16(0, static_cast<std::uint16_t>(al * 4), dx);
        cpu_.write16(0, static_cast<std::uint16_t>(al * 4 + 2), ds);
        return;
    case 0x2A:
        cpu_.set_reg(Reg::cx, 1990);
        cpu_.set_reg(Reg::dx, 0x0101);
        cpu_.set_reg(Reg::ax, static_cast<std::uint16_t>((ax & 0xFF00U) | 1));
        return;
    case 0x2C:
    {
        // Time of day from the tick count, starting at 08:00.
        const auto ticks =
            static_cast<std::uint32_t>(ram_[0x46C] | (ram_[0x46D] << 8U) | (ram_[0x46E] << 16U));
        const std::uint64_t hundredths = std::uint64_t{ticks} * 10000 / 182 + 8ULL * 360000;
        cpu_.set_reg(Reg::cx, static_cast<std::uint16_t>(((hundredths / 360000 % 24) << 8U) |
                                                         (hundredths / 6000 % 60)));
        cpu_.set_reg(Reg::dx, static_cast<std::uint16_t>(((hundredths / 100 % 60) << 8U) |
                                                         (hundredths % 100)));
        return;
    }
    case 0x2B:
    case 0x2D:
        cpu_.set_reg(Reg::ax, static_cast<std::uint16_t>(ax & 0xFF00U));
        return;
    case 0x2F:
        cpu_.set_seg(Seg::es, dta_segment_);
        cpu_.set_reg(Reg::bx, dta_offset_);
        return;
    case 0x30:
        cpu_.set_reg(Reg::ax, 0x0005);
        cpu_.set_reg(Reg::bx, 0);
        cpu_.set_reg(Reg::cx, 0);
        return;
    case 0x33:
        cpu_.set_reg(Reg::dx, static_cast<std::uint16_t>(dx & 0xFF00U));
        return;
    case 0x35:
        cpu_.set_reg(Reg::bx, cpu_.read16(0, static_cast<std::uint16_t>(al * 4)));
        cpu_.set_seg(Seg::es, cpu_.read16(0, static_cast<std::uint16_t>(al * 4 + 2)));
        return;
    case 0x36:
        cpu_.set_reg(Reg::ax, 8);
        cpu_.set_reg(Reg::bx, 10000);
        cpu_.set_reg(Reg::cx, 512);
        cpu_.set_reg(Reg::dx, 20000);
        return;
    case 0x3B:
    {
        const auto relative = game_relative(read_string(ds, dx));
        if (!relative)
        {
            return_carry(true, 3);
            return;
        }
        current_directory_ = "POOLRAD" + (relative->empty() ? "" : "\\" + *relative);
        std::replace(current_directory_.begin(), current_directory_.end(), '/', '\\');
        return_carry(false);
        return;
    }
    case 0x47:
    {
        const auto si = cpu_.reg(Reg::si);
        for (std::size_t n = 0; n <= current_directory_.size(); ++n)
        {
            cpu_.write8(ds, static_cast<std::uint16_t>(si + n),
                        n < current_directory_.size()
                            ? static_cast<std::uint8_t>(current_directory_[n])
                            : 0);
        }
        return_carry(false);
        return;
    }
    case 0x48:
    case 0x49:
    case 0x4A:
        memory_service(ah);
        return;
    case 0x4C:
        exit_code_ = al;
        return;
    case 0x4E:
    case 0x4F:
        find_service(ah);
        return;
    case 0x50:
        psp_ = cpu_.reg(Reg::bx);
        return;
    case 0x51:
    case 0x62:
        cpu_.set_reg(Reg::bx, psp_);
        return;
    case 0x58:
        cpu_.set_reg(Reg::ax, 0);
        return_carry(false);
        return;
    default:
        if ((ah >= 0x39 && ah <= 0x46) || ah == 0x56 || ah == 0x57)
        {
            file_service(ah);
            return;
        }
        console_ += "[DOS function " + std::to_string(ah) + " unsupported]\n";
        return_carry(true, 1);
        return;
    }
}

void Machine::file_service(std::uint8_t function)
{
    const auto ax = cpu_.reg(Reg::ax);
    const auto al = static_cast<std::uint8_t>(ax);
    const auto ds = cpu_.seg(Seg::ds), dx = cpu_.reg(Reg::dx);
    const auto bx = cpu_.reg(Reg::bx), cx = cpu_.reg(Reg::cx);
    const auto file = files_.find(bx);
    switch (function)
    {
    case 0x39:
    case 0x3A:
        return_carry(false);
        return;
    case 0x3C:
    case 0x3D:
    {
        const auto handle = open_file(read_string(ds, dx), al, function == 0x3C);
        if (handle < 0)
        {
            return_carry(true, static_cast<std::uint16_t>(-handle));
            return;
        }
        cpu_.set_reg(Reg::ax, static_cast<std::uint16_t>(handle));
        return_carry(false);
        return;
    }
    case 0x3E:
        if (bx < 5)
        {
            return_carry(false);
            return;
        }
        if (file == files_.end())
        {
            return_carry(true, 6);
            return;
        }
        files_.erase(file);
        return_carry(false);
        return;
    case 0x3F:
    {
        if (file == files_.end())
        {
            cpu_.set_reg(Reg::ax, 0);
            return_carry(bx >= 5, 6);
            return;
        }
        std::vector<char> buffer(cx);
        auto &stream = file->second->stream;
        stream.clear();
        stream.read(buffer.data(), cx);
        const auto read = static_cast<std::uint16_t>(stream.gcount());
        stream.clear();
        for (std::uint16_t n = 0; n < read; ++n)
        {
            cpu_.write8(ds, static_cast<std::uint16_t>(dx + n),
                        static_cast<std::uint8_t>(buffer[n]));
        }
        cpu_.set_reg(Reg::ax, read);
        return_carry(false);
        return;
    }
    case 0x40:
    {
        if (bx == 1 || bx == 2)
        {
            for (std::uint16_t n = 0; n < cx; ++n)
            {
                console_ += static_cast<char>(cpu_.read8(ds, static_cast<std::uint16_t>(dx + n)));
            }
            cpu_.set_reg(Reg::ax, cx);
            return_carry(false);
            return;
        }
        if (file == files_.end())
        {
            return_carry(true, 6);
            return;
        }
        auto &stream = file->second->stream;
        stream.clear();
        if (cx == 0)
        {
            // Truncates at the current position.
            const auto at = stream.tellp();
            const auto relative = game_relative(file->second->dos_path);
            stream.flush();
            std::filesystem::resize_file(write_path(*relative), static_cast<std::uintmax_t>(at));
            cpu_.set_reg(Reg::ax, 0);
            return_carry(false);
            return;
        }
        std::vector<char> buffer(cx);
        for (std::uint16_t n = 0; n < cx; ++n)
        {
            buffer[n] = static_cast<char>(cpu_.read8(ds, static_cast<std::uint16_t>(dx + n)));
        }
        stream.seekp(stream.tellg());
        stream.write(buffer.data(), cx);
        stream.flush();
        stream.seekg(stream.tellp());
        cpu_.set_reg(Reg::ax, cx);
        return_carry(false);
        return;
    }
    case 0x41:
    {
        const auto relative = game_relative(read_string(ds, dx));
        if (!relative || read_path(*relative).empty())
        {
            return_carry(true, 2);
            return;
        }
        if (const auto written = resolve(options_.scratch_directory, *relative))
        {
            std::filesystem::remove(*written);
        }
        deleted_.insert(*relative);
        return_carry(false);
        return;
    }
    case 0x42:
    {
        if (file == files_.end())
        {
            return_carry(true, 6);
            return;
        }
        auto &stream = file->second->stream;
        stream.clear();
        const auto offset = static_cast<std::int32_t>((std::uint32_t{cx} << 16U) | dx);
        const auto origin = al == 0 ? std::ios::beg : al == 1 ? std::ios::cur : std::ios::end;
        stream.seekg(offset, origin);
        const auto at = static_cast<std::uint32_t>(stream.tellg());
        stream.seekp(at);
        cpu_.set_reg(Reg::ax, static_cast<std::uint16_t>(at));
        cpu_.set_reg(Reg::dx, static_cast<std::uint16_t>(at >> 16U));
        return_carry(false);
        return;
    }
    case 0x43:
    {
        const auto relative = game_relative(read_string(ds, dx));
        if (!relative || read_path(*relative).empty())
        {
            return_carry(true, 2);
            return;
        }
        cpu_.set_reg(Reg::cx, std::filesystem::is_directory(read_path(*relative)) ? 0x10 : 0x20);
        return_carry(false);
        return;
    }
    case 0x44:
        if (al == 0x00)
        {
            cpu_.set_reg(Reg::dx, bx < 3 ? 0x80D3 : 0x0002);
            return_carry(false);
            return;
        }
        if (al == 0x08)
        {
            cpu_.set_reg(Reg::ax, 1);
            return_carry(false);
            return;
        }
        return_carry(true, 1);
        return;
    case 0x45:
    {
        if (file == files_.end())
        {
            return_carry(true, 6);
            return;
        }
        int handle = 5;
        while (files_.contains(handle))
        {
            ++handle;
        }
        files_[handle] = file->second;
        cpu_.set_reg(Reg::ax, static_cast<std::uint16_t>(handle));
        return_carry(false);
        return;
    }
    case 0x46:
        if (file != files_.end())
        {
            files_[cx] = file->second;
        }
        return_carry(false);
        return;
    case 0x56:
    {
        const auto from = game_relative(read_string(ds, dx));
        const auto to = game_relative(read_string(cpu_.seg(Seg::es), cpu_.reg(Reg::di)));
        if (!from || !to || read_path(*from).empty())
        {
            return_carry(true, 2);
            return;
        }
        const auto source = read_path(*from);
        const auto target = write_path(*to);
        std::filesystem::copy_file(source, target,
                                   std::filesystem::copy_options::overwrite_existing);
        if (const auto written = resolve(options_.scratch_directory, *from))
        {
            std::filesystem::remove(*written);
        }
        deleted_.insert(*from);
        deleted_.erase(*to);
        return_carry(false);
        return;
    }
    default: // 0x57: file date and time
        cpu_.set_reg(Reg::cx, 0x6000);
        cpu_.set_reg(Reg::dx, 0x1421);
        return_carry(false);
        return;
    }
}

void Machine::memory_service(std::uint8_t function)
{
    const auto bx = cpu_.reg(Reg::bx);
    // The free paragraphs after `segment`'s block, or from `start`.
    const auto room_after = [&](std::uint16_t start)
    {
        const auto next = blocks_.upper_bound(start);
        const std::uint16_t limit =
            next == blocks_.end() ? memory_top : static_cast<std::uint16_t>(next->first - 1);
        return static_cast<std::uint16_t>(limit > start ? limit - start : 0);
    };
    switch (function)
    {
    case 0x48:
    {
        std::uint16_t largest = 0;
        for (const auto &[segment, size] : blocks_)
        {
            const auto start = static_cast<std::uint16_t>(segment + size + 1);
            const auto room = room_after(start);
            if (room >= bx && start < memory_top)
            {
                blocks_[start] = bx;
                cpu_.set_reg(Reg::ax, start);
                return_carry(false);
                return;
            }
            largest = std::max(largest, room);
        }
        cpu_.set_reg(Reg::bx, largest);
        return_carry(true, 8);
        return;
    }
    case 0x49:
        blocks_.erase(cpu_.seg(Seg::es));
        return_carry(false);
        return;
    default:
    {
        const auto segment = cpu_.seg(Seg::es);
        if (!blocks_.contains(segment))
        {
            return_carry(true, 9);
            return;
        }
        const auto room = room_after(segment);
        if (bx > room)
        {
            cpu_.set_reg(Reg::bx, room);
            return_carry(true, 8);
            return;
        }
        blocks_[segment] = bx;
        return_carry(false);
        return;
    }
    }
}

void Machine::find_service(std::uint8_t function)
{
    const std::uint32_t dta = Cpu::linear(dta_segment_, dta_offset_);
    if (function == 0x4E)
    {
        const auto pattern = upper(read_string(cpu_.seg(Seg::ds), cpu_.reg(Reg::dx)));
        const auto slash = pattern.find_last_of("\\:");
        const auto directory =
            slash == std::string::npos ? std::string{} : pattern.substr(0, slash + 1);
        const auto name = slash == std::string::npos ? pattern : pattern.substr(slash + 1);
        auto relative = game_relative(directory.empty() ? "." : directory + ".");
        Search search;
        if (relative)
        {
            std::set<std::string> names;
            for (const bool original : {true, false})
            {
                const auto &root = original ? options_.game_directory : options_.scratch_directory;
                const auto folder = relative->empty() ? std::optional{root}
                                    : original        ? original_path(*relative)
                                                      : resolve(root, *relative);
                std::error_code error;
                if (!folder || !std::filesystem::is_directory(*folder, error))
                {
                    continue;
                }
                for (const auto &entry : std::filesystem::directory_iterator(*folder, error))
                {
                    const auto file = upper(entry.path().filename().string());
                    const auto full = relative->empty() ? file : *relative + "/" + file;
                    if (entry.is_regular_file() && wildcard_matches(name, file) &&
                        !deleted_.contains(full))
                    {
                        names.insert(full);
                    }
                }
            }
            search.names.assign(names.begin(), names.end());
        }
        searches_[dta] = std::move(search);
    }
    auto &search = searches_[dta];
    if (search.next >= search.names.size())
    {
        return_carry(true, function == 0x4E ? 2 : 18);
        return;
    }
    const auto &full = search.names[search.next++];
    const auto path = read_path(full);
    const auto size = static_cast<std::uint32_t>(std::filesystem::file_size(path));
    const auto name =
        full.substr(full.find_last_of('/') == std::string::npos ? 0 : full.find_last_of('/') + 1);
    ram_[dta + 0x15] = 0x20;
    ram_[dta + 0x16] = 0x00;
    ram_[dta + 0x17] = 0x60;
    ram_[dta + 0x18] = 0x21;
    ram_[dta + 0x19] = 0x14;
    for (unsigned n = 0; n < 4; ++n)
    {
        ram_[dta + 0x1A + n] = static_cast<std::uint8_t>(size >> (8U * n));
    }
    for (std::size_t n = 0; n < 13; ++n)
    {
        ram_[dta + 0x1E + n] = n < name.size() ? static_cast<std::uint8_t>(name[n]) : 0;
    }
    return_carry(false);
}
} // namespace opengold::probe
