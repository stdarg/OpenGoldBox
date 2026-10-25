#include "cpu.h"

#include <utility>

namespace opengold::probe
{
namespace
{
bool even_parity(std::uint32_t value)
{
    unsigned bits = value & 0xFFU;
    bits ^= bits >> 4U;
    bits ^= bits >> 2U;
    bits ^= bits >> 1U;
    return (bits & 1U) == 0;
}

std::uint16_t sign_extend(std::uint8_t value)
{
    return static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<std::int8_t>(value)));
}
} // namespace

std::uint8_t Cpu::fetch8()
{
    const auto value = read8(segs_[1], ip_);
    ++ip_;
    return value;
}

std::uint16_t Cpu::fetch16()
{
    const auto value = read16(segs_[1], ip_);
    ip_ = static_cast<std::uint16_t>(ip_ + 2);
    return value;
}

void Cpu::push(std::uint16_t value)
{
    regs_[4] = static_cast<std::uint16_t>(regs_[4] - 2);
    write16(segs_[2], regs_[4], value);
}

std::uint16_t Cpu::pop()
{
    const auto value = read16(segs_[2], regs_[4]);
    regs_[4] = static_cast<std::uint16_t>(regs_[4] + 2);
    return value;
}

std::uint16_t Cpu::default_segment(Seg s) const
{
    return segment_override_ >= 0 ? segs_[static_cast<unsigned>(segment_override_)]
                                  : segs_[static_cast<unsigned>(s)];
}

Cpu::Operand Cpu::decode_modrm(std::uint8_t modrm)
{
    const unsigned mod = modrm >> 6U, rm = modrm & 7U;
    if (mod == 3)
    {
        return {true, rm, 0, 0};
    }
    std::uint16_t offset = 0;
    Seg segment = Seg::ds;
    switch (rm)
    {
    case 0:
        offset = static_cast<std::uint16_t>(regs_[3] + regs_[6]);
        break;
    case 1:
        offset = static_cast<std::uint16_t>(regs_[3] + regs_[7]);
        break;
    case 2:
        offset = static_cast<std::uint16_t>(regs_[5] + regs_[6]);
        segment = Seg::ss;
        break;
    case 3:
        offset = static_cast<std::uint16_t>(regs_[5] + regs_[7]);
        segment = Seg::ss;
        break;
    case 4:
        offset = regs_[6];
        break;
    case 5:
        offset = regs_[7];
        break;
    case 6:
        if (mod == 0)
        {
            offset = fetch16();
        }
        else
        {
            offset = regs_[5];
            segment = Seg::ss;
        }
        break;
    default:
        offset = regs_[3];
        break;
    }
    if (mod == 1)
    {
        offset = static_cast<std::uint16_t>(offset + sign_extend(fetch8()));
    }
    else if (mod == 2)
    {
        offset = static_cast<std::uint16_t>(offset + fetch16());
    }
    return {false, 0, default_segment(segment), offset};
}

std::uint8_t Cpu::reg8(unsigned index) const
{
    return index < 4 ? static_cast<std::uint8_t>(regs_[index])
                     : static_cast<std::uint8_t>(regs_[index - 4] >> 8U);
}

void Cpu::set_reg8(unsigned index, std::uint8_t value)
{
    if (index < 4)
    {
        regs_[index] = static_cast<std::uint16_t>((regs_[index] & 0xFF00U) | value);
    }
    else
    {
        regs_[index - 4] = static_cast<std::uint16_t>((regs_[index - 4] & 0x00FFU) | (value << 8U));
    }
}

std::uint8_t Cpu::get8(const Operand &o)
{
    return o.is_register ? reg8(o.index) : read8(o.seg, o.off);
}

std::uint16_t Cpu::get16(const Operand &o)
{
    return o.is_register ? regs_[o.index] : read16(o.seg, o.off);
}

void Cpu::put8(const Operand &o, std::uint8_t value)
{
    if (o.is_register)
    {
        set_reg8(o.index, value);
    }
    else
    {
        write8(o.seg, o.off, value);
    }
}

void Cpu::put16(const Operand &o, std::uint16_t value)
{
    if (o.is_register)
    {
        regs_[o.index] = value;
    }
    else
    {
        write16(o.seg, o.off, value);
    }
}

void Cpu::set_szp(std::uint32_t result, bool wide)
{
    const std::uint32_t top = wide ? 0x8000U : 0x80U;
    set_flag(zero, (result & (wide ? 0xFFFFU : 0xFFU)) == 0);
    set_flag(sign, (result & top) != 0);
    set_flag(parity, even_parity(result));
}

void Cpu::set_logic_flags(std::uint32_t result, bool wide)
{
    set_flag(carry, false);
    set_flag(overflow, false);
    set_flag(adjust, false);
    set_szp(result, wide);
}

// The eight ALU operations in opcode order: ADD OR ADC SBB AND SUB XOR CMP.
std::uint32_t Cpu::alu(unsigned operation, std::uint32_t a, std::uint32_t b, bool wide)
{
    const std::uint32_t mask = wide ? 0xFFFFU : 0xFFU;
    const std::uint32_t top = wide ? 0x8000U : 0x80U;
    switch (operation)
    {
    case 0:
    case 2:
    {
        const std::uint32_t carry_in = operation == 2 && flag(carry) ? 1 : 0;
        const std::uint32_t result = a + b + carry_in;
        set_flag(carry, result > mask);
        set_flag(overflow, ((a ^ result) & (b ^ result) & top) != 0);
        set_flag(adjust, ((a ^ b ^ result) & 0x10U) != 0);
        set_szp(result & mask, wide);
        return result & mask;
    }
    case 3:
    case 5:
    case 7:
    {
        const std::uint32_t borrow_in = operation == 3 && flag(carry) ? 1 : 0;
        const std::uint32_t result = (a - b - borrow_in) & mask;
        set_flag(carry, a < b + borrow_in);
        set_flag(overflow, ((a ^ b) & (a ^ result) & top) != 0);
        set_flag(adjust, ((a ^ b ^ result) & 0x10U) != 0);
        set_szp(result, wide);
        return result;
    }
    case 1:
        set_logic_flags(a | b, wide);
        return a | b;
    case 4:
        set_logic_flags(a & b, wide);
        return a & b;
    default:
        set_logic_flags(a ^ b, wide);
        return a ^ b;
    }
}

// The shift group in opcode order: ROL ROR RCL RCR SHL SHR SAL SAR. The count
// is masked to five bits, as from the 80186 on.
std::uint32_t Cpu::shift(unsigned operation, std::uint32_t value, unsigned count, bool wide)
{
    count &= 0x1FU;
    if (count == 0)
    {
        return value;
    }
    const unsigned bits = wide ? 16 : 8;
    const std::uint32_t mask = wide ? 0xFFFFU : 0xFFU;
    const std::uint32_t top = 1U << (bits - 1);
    const std::uint32_t original = value;
    for (unsigned n = 0; n < count; ++n)
    {
        switch (operation)
        {
        case 0:
        {
            const bool out = (value & top) != 0;
            value = ((value << 1U) | (out ? 1U : 0U)) & mask;
            set_flag(carry, out);
            break;
        }
        case 1:
        {
            const bool out = (value & 1U) != 0;
            value = (value >> 1U) | (out ? top : 0U);
            set_flag(carry, out);
            break;
        }
        case 2:
        {
            const bool out = (value & top) != 0;
            value = ((value << 1U) | (flag(carry) ? 1U : 0U)) & mask;
            set_flag(carry, out);
            break;
        }
        case 3:
        {
            const bool out = (value & 1U) != 0;
            value = (value >> 1U) | (flag(carry) ? top : 0U);
            set_flag(carry, out);
            break;
        }
        case 4:
        case 6:
            set_flag(carry, (value & top) != 0);
            value = (value << 1U) & mask;
            break;
        case 5:
            set_flag(carry, (value & 1U) != 0);
            value >>= 1U;
            break;
        default:
            set_flag(carry, (value & 1U) != 0);
            value = (value >> 1U) | (value & top);
            break;
        }
    }
    const bool high = (value & top) != 0;
    const bool next = (value & (top >> 1U)) != 0;
    switch (operation)
    {
    case 0:
    case 2:
    case 4:
    case 6:
        set_flag(overflow, high != flag(carry));
        break;
    case 1:
    case 3:
        set_flag(overflow, high != next);
        break;
    case 5:
        set_flag(overflow, (original & top) != 0);
        break;
    default:
        set_flag(overflow, false);
        break;
    }
    if (operation >= 4)
    {
        set_szp(value, wide);
    }
    return value;
}

void Cpu::group3(std::uint8_t modrm, bool wide)
{
    const auto operand = decode_modrm(modrm);
    const unsigned operation = (modrm >> 3U) & 7U;
    const std::uint32_t value = wide ? get16(operand) : get8(operand);
    switch (operation)
    {
    case 0:
    case 1:
    {
        const std::uint32_t immediate = wide ? fetch16() : fetch8();
        set_logic_flags(value & immediate, wide);
        return;
    }
    case 2:
        if (wide)
        {
            put16(operand, static_cast<std::uint16_t>(~value));
        }
        else
        {
            put8(operand, static_cast<std::uint8_t>(~value));
        }
        return;
    case 3:
    {
        const auto result = alu(5, 0, value, wide);
        set_flag(carry, value != 0);
        if (wide)
        {
            put16(operand, static_cast<std::uint16_t>(result));
        }
        else
        {
            put8(operand, static_cast<std::uint8_t>(result));
        }
        return;
    }
    case 4:
        if (wide)
        {
            const std::uint32_t product = std::uint32_t{regs_[0]} * value;
            regs_[0] = static_cast<std::uint16_t>(product);
            regs_[2] = static_cast<std::uint16_t>(product >> 16U);
            set_flag(carry, regs_[2] != 0);
            set_flag(overflow, regs_[2] != 0);
        }
        else
        {
            regs_[0] = static_cast<std::uint16_t>((regs_[0] & 0xFFU) * value);
            set_flag(carry, (regs_[0] >> 8U) != 0);
            set_flag(overflow, (regs_[0] >> 8U) != 0);
        }
        return;
    case 5:
        if (wide)
        {
            const std::int32_t product = std::int32_t{static_cast<std::int16_t>(regs_[0])} *
                                         static_cast<std::int16_t>(value);
            regs_[0] = static_cast<std::uint16_t>(product);
            regs_[2] = static_cast<std::uint16_t>(static_cast<std::uint32_t>(product) >> 16U);
            const bool fits = product == static_cast<std::int16_t>(product);
            set_flag(carry, !fits);
            set_flag(overflow, !fits);
        }
        else
        {
            const std::int16_t product = static_cast<std::int16_t>(
                static_cast<std::int8_t>(regs_[0]) * static_cast<std::int8_t>(value));
            regs_[0] = static_cast<std::uint16_t>(product);
            const bool fits = product == static_cast<std::int8_t>(product);
            set_flag(carry, !fits);
            set_flag(overflow, !fits);
        }
        return;
    case 6:
        if (value == 0)
        {
            interrupt(0);
            return;
        }
        if (wide)
        {
            const std::uint32_t dividend = (std::uint32_t{regs_[2]} << 16U) | regs_[0];
            const std::uint32_t quotient = dividend / value;
            if (quotient > 0xFFFFU)
            {
                interrupt(0);
                return;
            }
            regs_[0] = static_cast<std::uint16_t>(quotient);
            regs_[2] = static_cast<std::uint16_t>(dividend % value);
        }
        else
        {
            const std::uint32_t quotient = regs_[0] / value;
            if (quotient > 0xFFU)
            {
                interrupt(0);
                return;
            }
            regs_[0] = static_cast<std::uint16_t>(((regs_[0] % value) << 8U) | quotient);
        }
        return;
    default:
        if (value == 0)
        {
            interrupt(0);
            return;
        }
        if (wide)
        {
            const std::int32_t dividend =
                static_cast<std::int32_t>((std::uint32_t{regs_[2]} << 16U) | regs_[0]);
            const std::int32_t divisor = static_cast<std::int16_t>(value);
            const std::int64_t quotient = std::int64_t{dividend} / divisor;
            if (quotient > 32767 || quotient < -32768)
            {
                interrupt(0);
                return;
            }
            regs_[0] = static_cast<std::uint16_t>(quotient);
            regs_[2] = static_cast<std::uint16_t>(dividend % divisor);
        }
        else
        {
            const std::int32_t dividend = static_cast<std::int16_t>(regs_[0]);
            const std::int32_t divisor = static_cast<std::int8_t>(value);
            const std::int32_t quotient = dividend / divisor;
            if (quotient > 127 || quotient < -128)
            {
                interrupt(0);
                return;
            }
            regs_[0] =
                static_cast<std::uint16_t>(((static_cast<std::uint8_t>(dividend % divisor)) << 8U) |
                                           static_cast<std::uint8_t>(quotient));
        }
        return;
    }
}

// MOVS CMPS STOS LODS SCAS INS OUTS, with any REP prefix.
void Cpu::string_operation(std::uint8_t opcode)
{
    const bool wide = (opcode & 1U) != 0;
    const std::uint16_t step =
        static_cast<std::uint16_t>((wide ? 2 : 1) * (flag(direction) ? -1 : 1));
    const auto source_segment = default_segment(Seg::ds);
    const bool compares = opcode == 0xA6 || opcode == 0xA7 || opcode == 0xAE || opcode == 0xAF;
    for (;;)
    {
        if (repeat_ && regs_[1] == 0)
        {
            return;
        }
        switch (opcode & 0xFEU)
        {
        case 0xA4:
            if (wide)
            {
                write16(segs_[0], regs_[7], read16(source_segment, regs_[6]));
            }
            else
            {
                write8(segs_[0], regs_[7], read8(source_segment, regs_[6]));
            }
            regs_[6] = static_cast<std::uint16_t>(regs_[6] + step);
            regs_[7] = static_cast<std::uint16_t>(regs_[7] + step);
            break;
        case 0xA6:
        {
            const std::uint32_t a =
                wide ? read16(source_segment, regs_[6]) : read8(source_segment, regs_[6]);
            const std::uint32_t b = wide ? read16(segs_[0], regs_[7]) : read8(segs_[0], regs_[7]);
            (void)alu(7, a, b, wide);
            regs_[6] = static_cast<std::uint16_t>(regs_[6] + step);
            regs_[7] = static_cast<std::uint16_t>(regs_[7] + step);
            break;
        }
        case 0xAA:
            if (wide)
            {
                write16(segs_[0], regs_[7], regs_[0]);
            }
            else
            {
                write8(segs_[0], regs_[7], static_cast<std::uint8_t>(regs_[0]));
            }
            regs_[7] = static_cast<std::uint16_t>(regs_[7] + step);
            break;
        case 0xAC:
            if (wide)
            {
                regs_[0] = read16(source_segment, regs_[6]);
            }
            else
            {
                set_reg8(0, read8(source_segment, regs_[6]));
            }
            regs_[6] = static_cast<std::uint16_t>(regs_[6] + step);
            break;
        case 0xAE:
        {
            const std::uint32_t a = wide ? regs_[0] : (regs_[0] & 0xFFU);
            const std::uint32_t b = wide ? read16(segs_[0], regs_[7]) : read8(segs_[0], regs_[7]);
            (void)alu(7, a, b, wide);
            regs_[7] = static_cast<std::uint16_t>(regs_[7] + step);
            break;
        }
        case 0x6C:
            write8(segs_[0], regs_[7], bus_.in(regs_[2]));
            if (wide)
            {
                write8(segs_[0], static_cast<std::uint16_t>(regs_[7] + 1),
                       bus_.in(static_cast<std::uint16_t>(regs_[2] + 1)));
            }
            regs_[7] = static_cast<std::uint16_t>(regs_[7] + step);
            break;
        default: // 0x6E
            bus_.out(regs_[2], read8(source_segment, regs_[6]));
            if (wide)
            {
                bus_.out(static_cast<std::uint16_t>(regs_[2] + 1),
                         read8(source_segment, static_cast<std::uint16_t>(regs_[6] + 1)));
            }
            regs_[6] = static_cast<std::uint16_t>(regs_[6] + step);
            break;
        }
        if (!repeat_)
        {
            return;
        }
        regs_[1] = static_cast<std::uint16_t>(regs_[1] - 1);
        if (compares && ((repeat_ == 1 && !flag(zero)) || (repeat_ == 2 && flag(zero))))
        {
            return;
        }
    }
}

bool Cpu::condition(unsigned code) const
{
    bool result = false;
    switch (code >> 1U)
    {
    case 0:
        result = flag(overflow);
        break;
    case 1:
        result = flag(carry);
        break;
    case 2:
        result = flag(zero);
        break;
    case 3:
        result = flag(carry) || flag(zero);
        break;
    case 4:
        result = flag(sign);
        break;
    case 5:
        result = flag(parity);
        break;
    case 6:
        result = flag(sign) != flag(overflow);
        break;
    default:
        result = flag(zero) || flag(sign) != flag(overflow);
        break;
    }
    return (code & 1U) ? !result : result;
}

void Cpu::interrupt(std::uint8_t number)
{
    push(flags_);
    push(segs_[1]);
    push(ip_);
    set_flag(Flag::interrupt, false);
    set_flag(trap, false);
    ip_ = read16(0, static_cast<std::uint16_t>(number * 4));
    segs_[1] = read16(0, static_cast<std::uint16_t>(number * 4 + 2));
}

bool Cpu::request_interrupt(std::uint8_t number)
{
    if (!flag(Flag::interrupt) || interrupt_shadow_)
    {
        return false;
    }
    halted_ = false;
    interrupt(number);
    return true;
}

void Cpu::far_return(std::uint16_t release, bool restore_flags)
{
    ip_ = pop();
    segs_[1] = pop();
    if (restore_flags)
    {
        set_flags(pop());
    }
    regs_[4] = static_cast<std::uint16_t>(regs_[4] + release);
}

void Cpu::step()
{
    if (halted_)
    {
        return;
    }
    interrupt_shadow_ = false;
    instruction_cs_ = segs_[1];
    instruction_ip_ = ip_;
    instruction_start_ = linear(segs_[1], ip_);
    segment_override_ = -1;
    repeat_ = 0;
    for (;;)
    {
        const auto opcode = fetch8();
        switch (opcode)
        {
        case 0x26:
        case 0x2E:
        case 0x36:
        case 0x3E:
            segment_override_ = (opcode >> 3U) & 3U;
            continue;
        case 0xF0:
        case 0xF1:
            continue;
        case 0xF2:
            repeat_ = 2;
            continue;
        case 0xF3:
            repeat_ = 1;
            continue;
        default:
            execute(opcode);
            return;
        }
    }
}

void Cpu::execute(std::uint8_t opcode)
{
    if (opcode < 0x40 && (opcode & 7U) < 6)
    {
        const unsigned operation = opcode >> 3U;
        const bool wide = (opcode & 1U) != 0;
        switch (opcode & 7U)
        {
        case 0:
        case 1:
        {
            const auto modrm = fetch8();
            const auto operand = decode_modrm(modrm);
            const unsigned source = (modrm >> 3U) & 7U;
            if (wide)
            {
                const auto result = alu(operation, get16(operand), regs_[source], true);
                if (operation != 7)
                {
                    put16(operand, static_cast<std::uint16_t>(result));
                }
            }
            else
            {
                const auto result = alu(operation, get8(operand), reg8(source), false);
                if (operation != 7)
                {
                    put8(operand, static_cast<std::uint8_t>(result));
                }
            }
            return;
        }
        case 2:
        case 3:
        {
            const auto modrm = fetch8();
            const auto operand = decode_modrm(modrm);
            const unsigned target = (modrm >> 3U) & 7U;
            if (wide)
            {
                const auto result = alu(operation, regs_[target], get16(operand), true);
                if (operation != 7)
                {
                    regs_[target] = static_cast<std::uint16_t>(result);
                }
            }
            else
            {
                const auto result = alu(operation, reg8(target), get8(operand), false);
                if (operation != 7)
                {
                    set_reg8(target, static_cast<std::uint8_t>(result));
                }
            }
            return;
        }
        case 4:
        {
            const auto result = alu(operation, regs_[0] & 0xFFU, fetch8(), false);
            if (operation != 7)
            {
                set_reg8(0, static_cast<std::uint8_t>(result));
            }
            return;
        }
        default:
        {
            const auto result = alu(operation, regs_[0], fetch16(), true);
            if (operation != 7)
            {
                regs_[0] = static_cast<std::uint16_t>(result);
            }
            return;
        }
        }
    }
    if (opcode >= 0x40 && opcode < 0x50)
    {
        const unsigned index = opcode & 7U;
        const bool carried = flag(carry);
        regs_[index] =
            static_cast<std::uint16_t>(alu(opcode < 0x48 ? 0 : 5, regs_[index], 1, true));
        set_flag(carry, carried);
        return;
    }
    if (opcode >= 0x50 && opcode < 0x58)
    {
        // An 8086 pushes SP after decrementing it.
        if (opcode == 0x54)
        {
            push(static_cast<std::uint16_t>(regs_[4] - 2));
        }
        else
        {
            push(regs_[opcode & 7U]);
        }
        return;
    }
    if (opcode >= 0x58 && opcode < 0x60)
    {
        regs_[opcode & 7U] = pop();
        return;
    }
    if (opcode >= 0x70 && opcode < 0x80)
    {
        const auto displacement = sign_extend(fetch8());
        if (condition(opcode & 0x0FU))
        {
            ip_ = static_cast<std::uint16_t>(ip_ + displacement);
        }
        return;
    }
    if (opcode >= 0x91 && opcode < 0x98)
    {
        std::swap(regs_[0], regs_[opcode & 7U]);
        return;
    }
    if (opcode >= 0xB0 && opcode < 0xB8)
    {
        set_reg8(opcode & 7U, fetch8());
        return;
    }
    if (opcode >= 0xB8 && opcode < 0xC0)
    {
        regs_[opcode & 7U] = fetch16();
        return;
    }
    if (opcode >= 0xD8 && opcode < 0xE0)
    {
        // No coprocessor: consume the operand and do nothing.
        (void)decode_modrm(fetch8());
        return;
    }
    switch (opcode)
    {
    case 0x06:
    case 0x0E:
    case 0x16:
    case 0x1E:
        push(segs_[(opcode >> 3U) & 3U]);
        return;
    case 0x07:
    case 0x17:
    case 0x1F:
        segs_[(opcode >> 3U) & 3U] = pop();
        interrupt_shadow_ = opcode == 0x17;
        return;
    case 0x0F:
        if (fetch8() == 0xFF)
        {
            bus_.host_interrupt(fetch8());
            return;
        }
        interrupt(6);
        return;
    case 0x27:
    case 0x2F:
    {
        const std::uint8_t old_al = static_cast<std::uint8_t>(regs_[0]);
        const bool old_carry = flag(carry);
        const bool subtract = opcode == 0x2F;
        std::uint8_t al = old_al;
        set_flag(carry, false);
        if ((al & 0x0FU) > 9 || flag(adjust))
        {
            set_flag(carry, old_carry || (subtract ? al < 6 : al > 0xF9));
            al = static_cast<std::uint8_t>(subtract ? al - 6 : al + 6);
            set_flag(adjust, true);
        }
        else
        {
            set_flag(adjust, false);
        }
        if (old_al > 0x99 || old_carry)
        {
            al = static_cast<std::uint8_t>(subtract ? al - 0x60 : al + 0x60);
            set_flag(carry, true);
        }
        set_reg8(0, al);
        set_szp(al, false);
        return;
    }
    case 0x37:
    case 0x3F:
    {
        const bool subtract = opcode == 0x3F;
        if ((regs_[0] & 0x0FU) > 9 || flag(adjust))
        {
            const auto al = static_cast<std::uint8_t>(regs_[0] + (subtract ? -6 : 6));
            const auto ah = static_cast<std::uint8_t>((regs_[0] >> 8U) + (subtract ? -1 : 1));
            regs_[0] = static_cast<std::uint16_t>((ah << 8U) | al);
            set_flag(adjust, true);
            set_flag(carry, true);
        }
        else
        {
            set_flag(adjust, false);
            set_flag(carry, false);
        }
        regs_[0] = static_cast<std::uint16_t>(regs_[0] & 0xFF0FU);
        return;
    }
    case 0x60:
    {
        const auto original_sp = regs_[4];
        for (unsigned r = 0; r < 8; ++r)
        {
            push(r == 4 ? original_sp : regs_[r]);
        }
        return;
    }
    case 0x61:
        for (int r = 7; r >= 0; --r)
        {
            const auto value = pop();
            if (r != 4)
            {
                regs_[static_cast<unsigned>(r)] = value;
            }
        }
        return;
    case 0x62:
    {
        const auto modrm = fetch8();
        const auto operand = decode_modrm(modrm);
        const auto index = static_cast<std::int16_t>(regs_[(modrm >> 3U) & 7U]);
        const auto low = static_cast<std::int16_t>(read16(operand.seg, operand.off));
        const auto high = static_cast<std::int16_t>(
            read16(operand.seg, static_cast<std::uint16_t>(operand.off + 2)));
        if (index < low || index > high)
        {
            interrupt(5);
        }
        return;
    }
    case 0x68:
        push(fetch16());
        return;
    case 0x6A:
        push(sign_extend(fetch8()));
        return;
    case 0x69:
    case 0x6B:
    {
        const auto modrm = fetch8();
        const auto operand = decode_modrm(modrm);
        const auto multiplicand = static_cast<std::int16_t>(get16(operand));
        const auto multiplier =
            static_cast<std::int16_t>(opcode == 0x69 ? fetch16() : sign_extend(fetch8()));
        const std::int32_t product = std::int32_t{multiplicand} * multiplier;
        regs_[(modrm >> 3U) & 7U] = static_cast<std::uint16_t>(product);
        const bool fits = product == static_cast<std::int16_t>(product);
        set_flag(carry, !fits);
        set_flag(overflow, !fits);
        return;
    }
    case 0x6C:
    case 0x6D:
    case 0x6E:
    case 0x6F:
    case 0xA4:
    case 0xA5:
    case 0xA6:
    case 0xA7:
    case 0xAA:
    case 0xAB:
    case 0xAC:
    case 0xAD:
    case 0xAE:
    case 0xAF:
        string_operation(opcode);
        return;
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
    {
        const bool wide = opcode & 1U;
        const auto modrm = fetch8();
        const auto operand = decode_modrm(modrm);
        const unsigned operation = (modrm >> 3U) & 7U;
        if (wide)
        {
            const std::uint16_t immediate = opcode == 0x81 ? fetch16() : sign_extend(fetch8());
            const auto result = alu(operation, get16(operand), immediate, true);
            if (operation != 7)
            {
                put16(operand, static_cast<std::uint16_t>(result));
            }
        }
        else
        {
            const auto result = alu(operation, get8(operand), fetch8(), false);
            if (operation != 7)
            {
                put8(operand, static_cast<std::uint8_t>(result));
            }
        }
        return;
    }
    case 0x84:
    case 0x85:
    {
        const auto modrm = fetch8();
        const auto operand = decode_modrm(modrm);
        const unsigned source = (modrm >> 3U) & 7U;
        if (opcode & 1U)
        {
            set_logic_flags(get16(operand) & regs_[source], true);
        }
        else
        {
            set_logic_flags(get8(operand) & reg8(source), false);
        }
        return;
    }
    case 0x86:
    case 0x87:
    {
        const auto modrm = fetch8();
        const auto operand = decode_modrm(modrm);
        const unsigned source = (modrm >> 3U) & 7U;
        if (opcode & 1U)
        {
            const auto value = get16(operand);
            put16(operand, regs_[source]);
            regs_[source] = value;
        }
        else
        {
            const auto value = get8(operand);
            put8(operand, reg8(source));
            set_reg8(source, value);
        }
        return;
    }
    case 0x88:
    case 0x89:
    case 0x8A:
    case 0x8B:
    {
        const auto modrm = fetch8();
        const auto operand = decode_modrm(modrm);
        const unsigned r = (modrm >> 3U) & 7U;
        switch (opcode)
        {
        case 0x88:
            put8(operand, reg8(r));
            break;
        case 0x89:
            put16(operand, regs_[r]);
            break;
        case 0x8A:
            set_reg8(r, get8(operand));
            break;
        default:
            regs_[r] = get16(operand);
            break;
        }
        return;
    }
    case 0x8C:
    {
        const auto modrm = fetch8();
        put16(decode_modrm(modrm), segs_[(modrm >> 3U) & 3U]);
        return;
    }
    case 0x8D:
    {
        const auto modrm = fetch8();
        const auto operand = decode_modrm(modrm);
        regs_[(modrm >> 3U) & 7U] = operand.off;
        return;
    }
    case 0x8E:
    {
        const auto modrm = fetch8();
        const unsigned s = (modrm >> 3U) & 3U;
        segs_[s] = get16(decode_modrm(modrm));
        interrupt_shadow_ = s == 2;
        return;
    }
    case 0x8F:
    {
        const auto value = pop();
        put16(decode_modrm(fetch8()), value);
        return;
    }
    case 0x90:
        return;
    case 0x98:
        regs_[0] = sign_extend(static_cast<std::uint8_t>(regs_[0]));
        return;
    case 0x99:
        regs_[2] = (regs_[0] & 0x8000U) ? 0xFFFF : 0;
        return;
    case 0x9A:
    {
        const auto offset = fetch16();
        const auto segment = fetch16();
        push(segs_[1]);
        push(ip_);
        segs_[1] = segment;
        ip_ = offset;
        return;
    }
    case 0x9B:
        return;
    case 0x9C:
        push(flags_);
        return;
    case 0x9D:
        set_flags(pop());
        return;
    case 0x9E:
        set_flags(static_cast<std::uint16_t>((flags_ & 0xFF00U) | (regs_[0] >> 8U)));
        return;
    case 0x9F:
        regs_[0] = static_cast<std::uint16_t>((regs_[0] & 0x00FFU) | ((flags_ & 0xFFU) << 8U));
        return;
    case 0xA0:
        set_reg8(0, read8(default_segment(Seg::ds), fetch16()));
        return;
    case 0xA1:
        regs_[0] = read16(default_segment(Seg::ds), fetch16());
        return;
    case 0xA2:
        write8(default_segment(Seg::ds), fetch16(), static_cast<std::uint8_t>(regs_[0]));
        return;
    case 0xA3:
        write16(default_segment(Seg::ds), fetch16(), regs_[0]);
        return;
    case 0xA8:
        set_logic_flags((regs_[0] & 0xFFU) & fetch8(), false);
        return;
    case 0xA9:
        set_logic_flags(regs_[0] & fetch16(), true);
        return;
    case 0xC0:
    case 0xC1:
    case 0xD0:
    case 0xD1:
    case 0xD2:
    case 0xD3:
    {
        const bool wide = opcode & 1U;
        const auto modrm = fetch8();
        const auto operand = decode_modrm(modrm);
        const auto value = wide ? get16(operand) : get8(operand);
        unsigned count = 1;
        if (opcode < 0xD0)
        {
            count = fetch8();
        }
        else if (opcode >= 0xD2)
        {
            count = regs_[1] & 0xFFU;
        }
        const auto result = shift((modrm >> 3U) & 7U, value, count, wide);
        if (wide)
        {
            put16(operand, static_cast<std::uint16_t>(result));
        }
        else
        {
            put8(operand, static_cast<std::uint8_t>(result));
        }
        return;
    }
    case 0xC2:
    {
        const auto release = fetch16();
        ip_ = pop();
        regs_[4] = static_cast<std::uint16_t>(regs_[4] + release);
        return;
    }
    case 0xC3:
        ip_ = pop();
        return;
    case 0xC4:
    case 0xC5:
    {
        const auto modrm = fetch8();
        const auto operand = decode_modrm(modrm);
        regs_[(modrm >> 3U) & 7U] = read16(operand.seg, operand.off);
        segs_[opcode == 0xC4 ? 0 : 3] =
            read16(operand.seg, static_cast<std::uint16_t>(operand.off + 2));
        return;
    }
    case 0xC6:
    {
        const auto operand = decode_modrm(fetch8());
        put8(operand, fetch8());
        return;
    }
    case 0xC7:
    {
        const auto operand = decode_modrm(fetch8());
        put16(operand, fetch16());
        return;
    }
    case 0xC8:
    {
        const auto size = fetch16();
        const unsigned level = fetch8() & 0x1FU;
        push(regs_[5]);
        const auto frame = regs_[4];
        for (unsigned n = 1; n < level; ++n)
        {
            regs_[5] = static_cast<std::uint16_t>(regs_[5] - 2);
            push(read16(segs_[2], regs_[5]));
        }
        if (level > 0)
        {
            push(frame);
        }
        regs_[5] = frame;
        regs_[4] = static_cast<std::uint16_t>(regs_[4] - size);
        return;
    }
    case 0xC9:
        regs_[4] = regs_[5];
        regs_[5] = pop();
        return;
    case 0xCA:
        far_return(fetch16(), false);
        return;
    case 0xCB:
        far_return(0, false);
        return;
    case 0xCC:
        interrupt(3);
        return;
    case 0xCD:
        interrupt(fetch8());
        return;
    case 0xCE:
        if (flag(overflow))
        {
            interrupt(4);
        }
        return;
    case 0xCF:
        far_return(0, true);
        return;
    case 0xD4:
    {
        const auto base = fetch8();
        if (base == 0)
        {
            interrupt(0);
            return;
        }
        const auto al = static_cast<std::uint8_t>(regs_[0]);
        regs_[0] = static_cast<std::uint16_t>(((al / base) << 8U) | (al % base));
        set_szp(regs_[0] & 0xFFU, false);
        return;
    }
    case 0xD5:
    {
        const auto base = fetch8();
        const auto al = static_cast<std::uint8_t>((regs_[0] & 0xFFU) + (regs_[0] >> 8U) * base);
        regs_[0] = al;
        set_szp(al, false);
        return;
    }
    case 0xD6:
        set_reg8(0, flag(carry) ? 0xFF : 0);
        return;
    case 0xD7:
        set_reg8(0, read8(default_segment(Seg::ds),
                          static_cast<std::uint16_t>(regs_[3] + (regs_[0] & 0xFFU))));
        return;
    case 0xE0:
    case 0xE1:
    case 0xE2:
    {
        const auto displacement = sign_extend(fetch8());
        regs_[1] = static_cast<std::uint16_t>(regs_[1] - 1);
        const bool taken = regs_[1] != 0 && (opcode == 0xE2 || (opcode == 0xE1) == flag(zero));
        if (taken)
        {
            ip_ = static_cast<std::uint16_t>(ip_ + displacement);
        }
        return;
    }
    case 0xE3:
    {
        const auto displacement = sign_extend(fetch8());
        if (regs_[1] == 0)
        {
            ip_ = static_cast<std::uint16_t>(ip_ + displacement);
        }
        return;
    }
    case 0xE4:
        set_reg8(0, bus_.in(fetch8()));
        return;
    case 0xE5:
    {
        const std::uint16_t port = fetch8();
        regs_[0] = static_cast<std::uint16_t>(
            bus_.in(port) | (bus_.in(static_cast<std::uint16_t>(port + 1)) << 8U));
        return;
    }
    case 0xE6:
        bus_.out(fetch8(), static_cast<std::uint8_t>(regs_[0]));
        return;
    case 0xE7:
    {
        const std::uint16_t port = fetch8();
        bus_.out(port, static_cast<std::uint8_t>(regs_[0]));
        bus_.out(static_cast<std::uint16_t>(port + 1), static_cast<std::uint8_t>(regs_[0] >> 8U));
        return;
    }
    case 0xE8:
    {
        const auto displacement = fetch16();
        push(ip_);
        ip_ = static_cast<std::uint16_t>(ip_ + displacement);
        return;
    }
    case 0xE9:
    {
        const auto displacement = fetch16();
        ip_ = static_cast<std::uint16_t>(ip_ + displacement);
        return;
    }
    case 0xEA:
    {
        const auto offset = fetch16();
        segs_[1] = fetch16();
        ip_ = offset;
        return;
    }
    case 0xEB:
    {
        const auto displacement = sign_extend(fetch8());
        ip_ = static_cast<std::uint16_t>(ip_ + displacement);
        return;
    }
    case 0xEC:
        set_reg8(0, bus_.in(regs_[2]));
        return;
    case 0xED:
        regs_[0] = static_cast<std::uint16_t>(
            bus_.in(regs_[2]) | (bus_.in(static_cast<std::uint16_t>(regs_[2] + 1)) << 8U));
        return;
    case 0xEE:
        bus_.out(regs_[2], static_cast<std::uint8_t>(regs_[0]));
        return;
    case 0xEF:
        bus_.out(regs_[2], static_cast<std::uint8_t>(regs_[0]));
        bus_.out(static_cast<std::uint16_t>(regs_[2] + 1),
                 static_cast<std::uint8_t>(regs_[0] >> 8U));
        return;
    case 0xF4:
        halted_ = true;
        return;
    case 0xF5:
        set_flag(carry, !flag(carry));
        return;
    case 0xF6:
        group3(fetch8(), false);
        return;
    case 0xF7:
        group3(fetch8(), true);
        return;
    case 0xF8:
        set_flag(carry, false);
        return;
    case 0xF9:
        set_flag(carry, true);
        return;
    case 0xFA:
        set_flag(Flag::interrupt, false);
        return;
    case 0xFB:
        set_flag(Flag::interrupt, true);
        interrupt_shadow_ = true;
        return;
    case 0xFC:
        set_flag(direction, false);
        return;
    case 0xFD:
        set_flag(direction, true);
        return;
    case 0xFE:
    {
        const auto modrm = fetch8();
        const auto operand = decode_modrm(modrm);
        const bool carried = flag(carry);
        const auto result = alu(((modrm >> 3U) & 7U) == 0 ? 0 : 5, get8(operand), 1, false);
        set_flag(carry, carried);
        put8(operand, static_cast<std::uint8_t>(result));
        return;
    }
    case 0xFF:
    {
        const auto modrm = fetch8();
        const auto operand = decode_modrm(modrm);
        switch ((modrm >> 3U) & 7U)
        {
        case 0:
        case 1:
        {
            const bool carried = flag(carry);
            const auto result = alu(((modrm >> 3U) & 7U) == 0 ? 0 : 5, get16(operand), 1, true);
            set_flag(carry, carried);
            put16(operand, static_cast<std::uint16_t>(result));
            return;
        }
        case 2:
        {
            const auto target = get16(operand);
            push(ip_);
            ip_ = target;
            return;
        }
        case 3:
        {
            const auto offset = read16(operand.seg, operand.off);
            const auto segment = read16(operand.seg, static_cast<std::uint16_t>(operand.off + 2));
            push(segs_[1]);
            push(ip_);
            segs_[1] = segment;
            ip_ = offset;
            return;
        }
        case 4:
            ip_ = get16(operand);
            return;
        case 5:
        {
            const auto offset = read16(operand.seg, operand.off);
            segs_[1] = read16(operand.seg, static_cast<std::uint16_t>(operand.off + 2));
            ip_ = offset;
            return;
        }
        default:
            push(get16(operand));
            return;
        }
    }
    default:
        interrupt(6);
        return;
    }
}
} // namespace opengold::probe
