// Instruction checks for the probe's CPU, against results documented for
// the 8086 and 80186.
#include "cpu.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using namespace opengold::probe;

class Ram final : public Bus
{
  public:
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x100000, 0);
    std::vector<std::uint8_t> host_calls;

    std::uint8_t read(std::uint32_t linear) override
    {
        return bytes[linear];
    }

    void write(std::uint32_t linear, std::uint8_t value) override
    {
        bytes[linear] = value;
    }

    std::uint8_t in(std::uint16_t) override
    {
        return 0x5A;
    }

    void out(std::uint16_t, std::uint8_t) override
    {
    }

    void host_interrupt(std::uint8_t number) override
    {
        host_calls.push_back(number);
    }
};

void check(bool ok, const std::string &message)
{
    if (!ok)
    {
        throw std::runtime_error(message);
    }
}

// Runs `code` at 1000:0000 with SS:SP at 2000:1000 until IP passes its end.
struct Program
{
    Ram ram;
    Cpu cpu{ram};

    explicit Program(const std::vector<std::uint8_t> &code)
    {
        std::copy(code.begin(), code.end(), ram.bytes.begin() + 0x10000);
        cpu.set_seg(Seg::cs, 0x1000);
        cpu.set_seg(Seg::ds, 0x3000);
        cpu.set_seg(Seg::es, 0x3000);
        cpu.set_seg(Seg::ss, 0x2000);
        cpu.set_reg(Reg::sp, 0x1000);
        cpu.set_ip(0);
        for (unsigned n = 0; n < 10000 && cpu.seg(Seg::cs) == 0x1000 && cpu.ip() < code.size(); ++n)
        {
            cpu.step();
        }
    }
};

void arithmetic_flags()
{
    // mov al,0x7F; add al,1 -> 0x80, overflow and sign, no carry, adjust.
    Program add({0xB0, 0x7F, 0x04, 0x01});
    check((add.cpu.reg(Reg::ax) & 0xFF) == 0x80 && add.cpu.flag(overflow) && add.cpu.flag(sign) &&
              !add.cpu.flag(carry) && add.cpu.flag(adjust) && !add.cpu.flag(parity),
          "ADD sets overflow, sign and adjust");
    // mov ax,0; sub ax,1 -> FFFF with carry, no overflow.
    Program sub({0xB8, 0x00, 0x00, 0x2D, 0x01, 0x00});
    check(sub.cpu.reg(Reg::ax) == 0xFFFF && sub.cpu.flag(carry) && !sub.cpu.flag(overflow) &&
              sub.cpu.flag(parity),
          "SUB borrows");
    // stc; mov al,5; adc al,5 -> 11.
    Program adc({0xF9, 0xB0, 0x05, 0x14, 0x05});
    check((adc.cpu.reg(Reg::ax) & 0xFF) == 11, "ADC adds the carry");
    // mov al,0x15; add al,0x27; daa -> 0x42 (BCD 15 + 27).
    Program daa({0xB0, 0x15, 0x04, 0x27, 0x27});
    check((daa.cpu.reg(Reg::ax) & 0xFF) == 0x42 && !daa.cpu.flag(carry),
          "DAA adjusts BCD addition");
    // inc keeps the carry: stc; mov bx,0xFFFF; inc bx.
    Program inc({0xF9, 0xBB, 0xFF, 0xFF, 0x43});
    check(inc.cpu.reg(Reg::bx) == 0 && inc.cpu.flag(carry) && inc.cpu.flag(zero), "INC keeps CF");
    // neg: mov al,1; neg al -> FF, carry.
    Program neg({0xB0, 0x01, 0xF6, 0xD8});
    check((neg.cpu.reg(Reg::ax) & 0xFF) == 0xFF && neg.cpu.flag(carry), "NEG sets CF for nonzero");
}

void multiply_divide()
{
    // mov ax,300; mov bx,200; mul bx -> DX:AX = 60000.
    Program mul({0xB8, 0x2C, 0x01, 0xBB, 0xC8, 0x00, 0xF7, 0xE3});
    check(mul.cpu.reg(Reg::ax) == 60000 && mul.cpu.reg(Reg::dx) == 0 && !mul.cpu.flag(carry),
          "MUL");
    // mov ax,-7; cwd; mov bx,2; idiv bx -> AX -3, DX -1.
    Program idiv({0xB8, 0xF9, 0xFF, 0x99, 0xBB, 0x02, 0x00, 0xF7, 0xFB});
    check(idiv.cpu.reg(Reg::ax) == 0xFFFD && idiv.cpu.reg(Reg::dx) == 0xFFFF,
          "IDIV truncates toward zero");
    // mov ax,1000; mov cl,7; div cl -> AL 142, AH 6.
    Program div({0xB8, 0xE8, 0x03, 0xB1, 0x07, 0xF6, 0xF1});
    check(div.cpu.reg(Reg::ax) == ((6 << 8) | 142), "DIV r/m8");
    // imul cx, ax, -3 (69 /r iw) with ax=5.
    Program imul({0xB8, 0x05, 0x00, 0x69, 0xC8, 0xFD, 0xFF});
    check(imul.cpu.reg(Reg::cx) == 0xFFF1, "IMUL with an immediate");
}

void shifts()
{
    // mov al,0x81; rol al,1 -> 0x03, CF 1.
    Program rol({0xB0, 0x81, 0xD0, 0xC0});
    check((rol.cpu.reg(Reg::ax) & 0xFF) == 0x03 && rol.cpu.flag(carry), "ROL");
    // mov ax,0x8000; sar ax,4 (C1 /7 ib) -> F800.
    Program sar({0xB8, 0x00, 0x80, 0xC1, 0xF8, 0x04});
    check(sar.cpu.reg(Reg::ax) == 0xF800, "SAR keeps the sign");
    // mov al,1; stc; rcr al,1 -> 0x80, CF 1.
    Program rcr({0xB0, 0x01, 0xF9, 0xD0, 0xD8});
    check((rcr.cpu.reg(Reg::ax) & 0xFF) == 0x80 && rcr.cpu.flag(carry), "RCR rotates through CF");
    // mov ax,0x4000; shl ax,1 -> 0x8000, OF set.
    Program shl({0xB8, 0x00, 0x40, 0xD1, 0xE0});
    check(shl.cpu.reg(Reg::ax) == 0x8000 && shl.cpu.flag(overflow), "SHL sets OF on a sign change");
}

void memory_and_strings()
{
    // mov word [bx+si+2], 0x1234 with bx=0x10, si=0x20: DS:0x32.
    Program store({0xBB, 0x10, 0x00, 0xBE, 0x20, 0x00, 0xC7, 0x40, 0x02, 0x34, 0x12});
    check(store.ram.bytes[0x30032] == 0x34 && store.ram.bytes[0x30033] == 0x12,
          "MOV through [BX+SI+disp8]");
    // [bp] defaults to SS: mov bp,0x10; mov byte [bp+0],7.
    Program stack({0xBD, 0x10, 0x00, 0xC6, 0x46, 0x00, 0x07});
    check(stack.ram.bytes[0x20010] == 7, "[BP] uses SS");
    // cld; mov si,0; mov di,0x100; mov cx,3; rep movsb: copies DS:0..2 to ES:100.
    Program copy({0xFC, 0xBE, 0x00, 0x00, 0xBF, 0x00, 0x01, 0xB9, 0x03, 0x00, 0xF3, 0xA4});
    check(copy.cpu.reg(Reg::cx) == 0 && copy.cpu.reg(Reg::di) == 0x103, "REP MOVSB counts down CX");
    // repne scasb finds AL: mov di,0; mov al,0; mov cx,10; repne scasb over zeroes stops at once.
    Program scan({0xBF, 0x00, 0x00, 0xB0, 0x00, 0xB9, 0x0A, 0x00, 0xF2, 0xAE});
    check(scan.cpu.reg(Reg::cx) == 9 && scan.cpu.flag(zero), "REPNE SCASB stops on a match");
    // cs: override on a load: mov al, cs:[0] reads the code's first byte.
    Program over({0x2E, 0xA0, 0x00, 0x00});
    check((over.cpu.reg(Reg::ax) & 0xFF) == 0x2E, "A segment override applies to the next access");
}

void control_flow()
{
    // call +3; hlt-padding; the callee sets bx and returns: e8 01 00 | f4 | bb 09 00 c3
    Program call({0xE8, 0x01, 0x00, 0xF4, 0xBB, 0x09, 0x00, 0xC3});
    check(call.cpu.reg(Reg::bx) == 9 && call.cpu.halted(), "CALL and RET");
    // mov cx,5; xor ax,ax; inc ax; loop -3 -> AX 5.
    Program loop({0xB9, 0x05, 0x00, 0x31, 0xC0, 0x40, 0xE2, 0xFD});
    check(loop.cpu.reg(Reg::ax) == 5 && loop.cpu.reg(Reg::cx) == 0, "LOOP");
    // enter 4,0 then leave restores SP and BP.
    Program frame({0xBD, 0x34, 0x12, 0xC8, 0x04, 0x00, 0x00, 0xC9});
    check(frame.cpu.reg(Reg::sp) == 0x1000 && frame.cpu.reg(Reg::bp) == 0x1234, "ENTER and LEAVE");
    // pusha; popa keeps every register.
    Program all({0xB8, 0x11, 0x11, 0xBE, 0x22, 0x22, 0x60, 0x31, 0xC0, 0x31, 0xF6, 0x61});
    check(all.cpu.reg(Reg::ax) == 0x1111 && all.cpu.reg(Reg::si) == 0x2222 &&
              all.cpu.reg(Reg::sp) == 0x1000,
          "PUSHA and POPA");
}

void interrupts()
{
    // Vector 0x21 at 0000:0084 points at 4000:0000, which holds the host trap
    // and IRET; INT 21h runs the host service and returns after the INT.
    Ram ram;
    ram.bytes[0x84] = 0x00;
    ram.bytes[0x85] = 0x00;
    ram.bytes[0x86] = 0x00;
    ram.bytes[0x87] = 0x40;
    const std::vector<std::uint8_t> handler{0x0F, 0xFF, 0x21, 0xCF};
    std::copy(handler.begin(), handler.end(), ram.bytes.begin() + 0x40000);
    const std::vector<std::uint8_t> code{0xCD, 0x21, 0xBB, 0x01, 0x00};
    std::copy(code.begin(), code.end(), ram.bytes.begin() + 0x10000);
    Cpu cpu(ram);
    cpu.set_seg(Seg::cs, 0x1000);
    cpu.set_seg(Seg::ss, 0x2000);
    cpu.set_reg(Reg::sp, 0x1000);
    cpu.set_flags(Flag::interrupt);
    for (unsigned n = 0; n < 4; ++n)
    {
        cpu.step();
    }
    check(ram.host_calls == std::vector<std::uint8_t>{0x21} && cpu.reg(Reg::bx) == 1 &&
              cpu.reg(Reg::sp) == 0x1000 && cpu.flag(Flag::interrupt),
          "INT reaches the host trap and IRET returns with flags restored");
}
} // namespace

int main()
{
    try
    {
        arithmetic_flags();
        multiply_divide();
        shifts();
        memory_and_strings();
        control_flow();
        interrupts();
        std::cout << "DOS probe CPU tests passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
