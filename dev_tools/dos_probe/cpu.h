#ifndef OPENGOLD_DOS_PROBE_CPU_H
#define OPENGOLD_DOS_PROBE_CPU_H

#include <array>
#include <cstdint>

namespace opengold::probe
{
// What the CPU reaches outside itself: memory, I/O ports and the host's
// interrupt services.
class Bus
{
  public:
    virtual ~Bus() = default;
    virtual std::uint8_t read(std::uint32_t linear) = 0;
    virtual void write(std::uint32_t linear, std::uint8_t value) = 0;
    virtual std::uint8_t in(std::uint16_t port) = 0;
    virtual void out(std::uint16_t port, std::uint8_t value) = 0;
    // The host trap `0F FF n`, which every unhooked interrupt vector points
    // at: the host performs interrupt n's service on the CPU's registers.
    virtual void host_interrupt(std::uint8_t number) = 0;
};

enum class Reg : std::uint8_t
{
    ax,
    cx,
    dx,
    bx,
    sp,
    bp,
    si,
    di
};

enum class Seg : std::uint8_t
{
    es,
    cs,
    ss,
    ds
};

enum Flag : std::uint16_t
{
    carry = 1U << 0,
    parity = 1U << 2,
    adjust = 1U << 4,
    zero = 1U << 6,
    sign = 1U << 7,
    trap = 1U << 8,
    interrupt = 1U << 9,
    direction = 1U << 10,
    overflow = 1U << 11,
};

// A real-mode 8086 with the 80186 additions, as Borland's compilers of the
// late 1980s could emit. No 8087: escape opcodes do nothing, so a program's
// coprocessor check finds none.
class Cpu
{
  public:
    explicit Cpu(Bus &bus) : bus_(bus)
    {
    }

    // Runs one instruction, with its prefixes.
    void step();
    // Delivers a hardware interrupt if interrupts are enabled; returns
    // whether it was taken.
    bool request_interrupt(std::uint8_t number);
    // Enters interrupt `number` through the vector table, as INT does.
    void interrupt(std::uint8_t number);

    [[nodiscard]] std::uint16_t reg(Reg r) const
    {
        return regs_[static_cast<unsigned>(r)];
    }

    void set_reg(Reg r, std::uint16_t value)
    {
        regs_[static_cast<unsigned>(r)] = value;
    }

    [[nodiscard]] std::uint16_t seg(Seg s) const
    {
        return segs_[static_cast<unsigned>(s)];
    }

    void set_seg(Seg s, std::uint16_t value)
    {
        segs_[static_cast<unsigned>(s)] = value;
    }

    [[nodiscard]] std::uint16_t ip() const
    {
        return ip_;
    }

    void set_ip(std::uint16_t value)
    {
        ip_ = value;
    }

    [[nodiscard]] std::uint16_t flags() const
    {
        return flags_;
    }

    void set_flags(std::uint16_t value)
    {
        flags_ = static_cast<std::uint16_t>((value & 0x0FD5U) | 0xF002U);
    }

    void set_flag(Flag flag, bool on)
    {
        flags_ = static_cast<std::uint16_t>(on ? flags_ | flag : flags_ & ~flag);
    }

    [[nodiscard]] bool flag(Flag flag) const
    {
        return (flags_ & flag) != 0;
    }

    [[nodiscard]] bool halted() const
    {
        return halted_;
    }

    void resume()
    {
        halted_ = false;
    }

    // Linear address of the instruction being executed.
    [[nodiscard]] std::uint32_t instruction_start() const
    {
        return instruction_start_;
    }

    [[nodiscard]] std::uint16_t instruction_segment() const
    {
        return instruction_cs_;
    }

    [[nodiscard]] std::uint16_t instruction_offset() const
    {
        return instruction_ip_;
    }

    static std::uint32_t linear(std::uint16_t segment, std::uint16_t offset)
    {
        return ((std::uint32_t{segment} << 4U) + offset) & 0xFFFFFU;
    }

    std::uint8_t read8(std::uint16_t segment, std::uint16_t offset)
    {
        return bus_.read(linear(segment, offset));
    }

    std::uint16_t read16(std::uint16_t segment, std::uint16_t offset)
    {
        return static_cast<std::uint16_t>(
            read8(segment, offset) |
            (read8(segment, static_cast<std::uint16_t>(offset + 1)) << 8U));
    }

    void write8(std::uint16_t segment, std::uint16_t offset, std::uint8_t value)
    {
        bus_.write(linear(segment, offset), value);
    }

    void write16(std::uint16_t segment, std::uint16_t offset, std::uint16_t value)
    {
        write8(segment, offset, static_cast<std::uint8_t>(value));
        write8(segment, static_cast<std::uint16_t>(offset + 1),
               static_cast<std::uint8_t>(value >> 8U));
    }

    void push(std::uint16_t value);
    std::uint16_t pop();

  private:
    struct Operand
    {
        bool is_register{};
        unsigned index{};    // register number when is_register
        std::uint16_t seg{}; // segment when in memory
        std::uint16_t off{};
    };

    std::uint8_t fetch8();
    std::uint16_t fetch16();
    Operand decode_modrm(std::uint8_t modrm);
    std::uint16_t default_segment(Seg s) const;

    std::uint8_t get8(const Operand &o);
    std::uint16_t get16(const Operand &o);
    void put8(const Operand &o, std::uint8_t value);
    void put16(const Operand &o, std::uint16_t value);
    std::uint8_t reg8(unsigned index) const;
    void set_reg8(unsigned index, std::uint8_t value);

    std::uint32_t alu(unsigned operation, std::uint32_t a, std::uint32_t b, bool wide);
    std::uint32_t shift(unsigned operation, std::uint32_t value, unsigned count, bool wide);
    void set_logic_flags(std::uint32_t result, bool wide);
    void set_szp(std::uint32_t result, bool wide);
    void group3(std::uint8_t modrm, bool wide);
    void string_operation(std::uint8_t opcode);
    bool condition(unsigned code) const;
    void far_return(std::uint16_t release, bool restore_flags);
    void execute(std::uint8_t opcode);

    Bus &bus_;
    std::array<std::uint16_t, 8> regs_{};
    std::array<std::uint16_t, 4> segs_{};
    std::uint16_t ip_{};
    std::uint16_t flags_{0xF002};
    bool halted_{};
    // After STI or a load of SS, the next instruction runs before any
    // hardware interrupt, so a stack switch is never split.
    bool interrupt_shadow_{};

    // Per-instruction prefix state.
    int segment_override_{-1};
    int repeat_{0}; // 0 none, 1 REPE/REP (F3), 2 REPNE (F2)
    std::uint32_t instruction_start_{};
    std::uint16_t instruction_cs_{}, instruction_ip_{};
};
} // namespace opengold::probe

#endif
