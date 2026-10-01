#include "opengold/ecl_machine.h"
#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>

using namespace opengold::por;

namespace
{
using Bytes = std::vector<std::uint8_t>;

void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

template <class F> void rejects(F fn)
{
    bool threw = false;
    try
    {
        fn();
    }
    catch (const EclError &)
    {
        threw = true;
    }
    check(threw, "Expected ECL error");
}

Bytes record(const Bytes &body)
{
    Bytes b{0, 0};
    for (int i = 0; i < 5; ++i)
        b.insert(b.end(), {1, 1, 0x14, 0x99});
    b.insert(b.end(), body.begin(), body.end());
    return b;
}

std::shared_ptr<const EclProgram> program(const Bytes &body)
{
    return std::make_shared<const EclProgram>(EclProgram::decode(record(body), "TEST:7"));
}

EclMachine machine(const Bytes &body)
{
    EclMachine vm(program(body));
    vm.bind_variable(0x9700, 0);
    check(vm.start(0), "Start");
    return vm;
}

void host_random_and_string_copy()
{
    EclMachine strings(program({9, 129, 0, 0x97, 129, 0x20, 0x97, 0}));
    strings.bind_string(0x9700, "ORCS");
    strings.bind_string(0x9720, ".....");
    check(strings.start(0) && strings.run().state == EclState::completed &&
          strings.string(0x9720) == "ORCS",
          "SAVE string reference copies text into string destination");
    EclMachine vm(program({35, 1, 0, 0x97, 1, 1, 0x97, 0, 0, 0, 0, 0}));
    vm.bind_variable(0x9700, 0);
    vm.bind_variable(0x9701, 0);
    vm.enable_host(35);
    check(vm.start(0), "Host random start");
    const auto request = vm.run().request;
    check(request.has_value(), "Surprise host request");
    auto replay = vm;
    rejects(
        [&]
    {
        (void)vm.host_random(request->id + 1, 6);
    });
    rejects(
        [&]
    {
        (void)vm.host_random(request->id, 0);
    });
    for (unsigned n = 0; n < 100; ++n)
    {
        const auto die = vm.host_random(request->id, 6);
        check(die < 6 && die == replay.host_random(request->id, 6),
              "Host dice are bounded and checkpoint deterministic");
    }
    check(vm.resume_host(request->id, {}), "Complete host dice request");
    rejects(
        [&]
    {
        (void)vm.host_random(request->id, 6);
    });
}

void decoding()
{
    check(unpack_ecl_text(Bytes{4, 32, 192}) == "ABC", "Six-bit packed text");
    rejects(
        []
    {
        (void)EclProgram::decode({}, "empty");
    });
    rejects(
        []
    {
        (void)EclProgram::decode(Bytes(70000), "huge");
    });
    rejects(
        []
    {
        (void)EclProgram::decode(record(Bytes(7680)), "beyond PoR zone");
    });
    auto bad = record({0});
    bad[2] = 0;
    rejects(
        [&]
    {
        (void)EclProgram::decode(bad, "entry");
    });
    bad = record({0});
    bad[4] = 0;
    rejects(
        [&]
    {
        (void)EclProgram::decode(bad, "target");
    });
    for (const Bytes body :
            {
                Bytes{17, 128, 3, 4}, Bytes{17, 1, 0}, Bytes{17, 9, 0}, Bytes{255},
                Bytes{43, 1, 0, 0x97, 1, 0, 0x97}
            })
    {
        const auto p = program(body);
        rejects(
            [&]
        {
            (void)p->instruction(0x9914);
        });
    }
    const auto p = program({52, 0, 1, 0});
    check(p->instruction(0x9914).next == 0x9917, "PoR one-operand ECL CLOCK grammar");
    rejects(
        [&]
    {
        (void)p->instruction(0x9918);
    });
    const auto literal = program({17, 128, 3, 4, 32, 192, 0});
    check(literal->instruction(0x9914).operands[0].text == "ABC", "Inline text operand");
}

void arithmetic()
{
    auto vm = machine({9, 2, 255, 255, 1, 0, 0x97, 4, 0, 1, 1, 0, 0x97, 1, 0, 0x97, 0});
    check(vm.run(0).instructions == 0, "Zero budget");
    check(vm.run(1).state == EclState::running && vm.variable(0x9700) == 65535, "Budget yield");
    check(vm.run().state == EclState::completed && vm.variable(0x9700) == 0,
          "16-bit addition wrap");
    check(vm.start(4), "Restart another entry after completion");
    check(vm.run().state == EclState::completed, "Restart completes");
    for (const auto [op, expected] :
    std::array<std::array<unsigned, 2>, 4> {{{5, 7}, {7, 30}, {47, 2}, {48, 11}}})
    {
        auto n = machine({static_cast<std::uint8_t>(op), 0, 3, 0, 10, 1, 0, 0x97, 0});
        check(n.run().state == EclState::completed && n.variable(0x9700) == expected,
              "Arithmetic/bitwise operand semantics");
    }
    auto unbound = machine({9, 0, 1, 1, 1, 0x97, 0});
    const auto fail = unbound.run();
    check(fail.state == EclState::faulted &&
          fail.diagnostic.find("TEST:7 @ 0x9914") != std::string::npos,
          "Unbound write with context");
    check(unbound.variable(0x9700) == 0, "Fault does not alter other variables");
    check(!unbound.start(0), "Fault cannot silently restart");
}

void control()
{
    // CALL subroutine at 9919, return to EXIT at 9918.
    auto call = machine({2, 1, 0x19, 0x99, 0, 9, 0, 42, 1, 0, 0x97, 19});
    check(call.run().state == EclState::completed && call.variable(0x9700) == 42, "GOSUB/RETURN");
    auto underflow = machine({19, 0});
    check(underflow.run().state == EclState::completed, "Empty RETURN falls through in PoR");
    auto recursion = machine({2, 1, 0x14, 0x99});
    check(recursion.run().state == EclState::faulted, "Stack limit");
    auto loop = machine({1, 1, 0x14, 0x99});
    check(loop.run(5).state == EclState::running && loop.trace().size() == 5, "Loop yields");
    check(loop.run(1000000).state == EclState::faulted && loop.trace().size() == 64,
          "Total invocation limit and bounded trace");
    auto overlap = machine({1, 1, 0x15, 0x99, 0});
    check(overlap.run().state == EclState::faulted, "Jump into operand");
    auto no_compare = machine({22, 9, 0, 1, 1, 0, 0x97, 0});
    check(no_compare.run().state == EclState::completed && no_compare.variable(0x9700) == 0,
          "Comparison flags start false");
    // False IF = skips the entire variable-length menu, without needing its destination binding.
    auto skip = machine(
    {3, 0, 1, 0, 2, 22, 43, 1, 1, 0x97, 0, 1, 128, 3, 4, 32, 192, 9, 0, 9, 1, 0, 0x97, 0});
    check(skip.run().state == EclState::completed && skip.variable(0x9700) == 9,
          "Variable-length conditional skip");
    for (unsigned op = 22; op <= 27; ++op)
    {
        const bool pass = op == 23 || op == 24 || op == 26;
        auto cond = machine({3, 0, 1, 0, 2, static_cast<std::uint8_t>(op), 9, 0, 7, 1, 0, 0x97, 0});
        check(cond.run().state == EclState::completed && cond.variable(0x9700) == (pass ? 7 : 0),
              "Conditional relation");
    }
    // Indexed goto selects the first target with index 0, and falls through for >= count.
    for (std::uint8_t index :
            {
                0, 1, 2
            })
    {
        auto indexed = machine({37, 0, index, 0, 1, 1, 0x1e, 0x99, 0, 0, 9, 0, 8, 1, 0, 0x97, 0});
        check(indexed.run().state == EclState::completed &&
              indexed.variable(0x9700) == (index == 0 ? 8 : 0),
              "Indexed goto and fallthrough");
    }
    // ON GOSUB's first/last choices must both return to the fallthrough EXIT.
    for (std::uint8_t index :
            {
                0, 1, 2
            })
    {
        auto indexed = machine({38, 0, index, 0, 2,    1,  0x20, 0x99, 1, 0x27, 0x99, 0,    9,
                                0,  8, 1,     0, 0x97, 19, 9,    0,    9, 1,    0,    0x97, 19});
        check(indexed.run().state == EclState::completed &&
              indexed.variable(0x9700) == (index < 2 ? index + 8 : 0),
              "Indexed subroutine first/last/out of range");
    }
}

void suspension()
{
    auto vm =
        machine({43, 1, 0, 0x97, 0, 2, 128, 3, 4, 32, 192, 128, 3, 4, 32, 192, 17, 1, 0, 0x97, 0});
    auto result = vm.run();
    check(result.state == EclState::waiting && result.request->choices.size() == 2,
          "Menu suspends");
    const auto id = result.request->id;
    check(vm.run().instructions == 0 && vm.run().request->id == id,
          "Waiting does not reexecute menu");
    check(!vm.resume(id + 1, 0) && !vm.resume(id) && !vm.resume(id, 2),
          "Stale/invalid menu replies");
    rejects(
        [&]
    {
        vm.bind_variable(0x9700, 99);
    });
    check(vm.resume(id, 1) && vm.variable(0x9700) == 1, "Choice uses zero-based value");
    check(!vm.resume(id, 0), "Duplicate response rejected");
    result = vm.run();
    check(result.state == EclState::waiting && result.request->text == "1",
          "Output sees menu result");
    check(!vm.resume(result.request->id, 0) && vm.resume(result.request->id),
          "Text reply validation");
    check(vm.run().state == EclState::completed, "Continuation completes");
    check(vm.start(0) && vm.run().request->id > id, "Request IDs never reused on restart");
    auto clear = machine({18, 128, 3, 4, 32, 192, 0});
    check(clear.run().request->clear, "PRINTCLEAR host intent");
    auto vertical = machine({21, 1, 0, 0x97, 128, 3, 4, 32, 192, 0, 1, 128, 3, 4, 32, 192, 0});
    const auto menu = vertical.run();
    check(menu.request && menu.request->vertical && menu.request->text == "ABC",
          "Vertical menu metadata");
    auto other = machine({9, 0, 13, 1, 0, 0x97, 0});
    check(other.run().state == EclState::completed && other.variable(0x9700) == 13 &&
          vertical.variable(0x9700) == 0,
          "Independent VM variable state");
    auto unsupported = machine({36});
    check(unsupported.run().diagnostic.find("COMBAT") != std::string::npos,
          "Game command explicitly unsupported");
    auto native = machine({45, 1, 0x1e, 0xc0});
    check(native.run().diagnostic.find("CALL") != std::string::npos, "DOS calls never executed");
}

void por_operations()
{
    auto div = machine({6, 0, 17, 0, 5, 1, 0, 0x97, 0});
    check(div.run().state == EclState::completed && div.variable(0x9700) == 3,
          "PoR DIVIDE quotient without remainder binding");
    auto zero = machine({6, 0, 17, 0, 0, 1, 0, 0x97, 0});
    check(zero.run().diagnostic.find("Division by zero") != std::string::npos &&
          zero.variable(0x9700) == 0,
          "Divide fault before mutation");
    for (const std::uint8_t op :
            {
                47, 48
            })
    {
        auto bits = machine({op, 0, 2, 0, 1, 1, 0, 0x97, 23, 9, 0, 99, 1, 0, 0x97, 0});
        check(bits.run().state == EclState::completed &&
              bits.variable(0x9700) == (op == 47 ? 0 : 99),
              "Bitwise IF <> without preceding COMPARE");
    }
    for (unsigned op = 22; op <= 27; ++op)
    {
        auto flags =
            machine({3, 0, 1,  0, 2, 20,   0, 7, 0, 7, 0, 9, 0, 9, static_cast<std::uint8_t>(op),
                     9, 0, 42, 1, 0, 0x97, 0});
        check(flags.run().state == EclState::completed &&
              flags.variable(0x9700) == (op == 22 ? 42 : 0),
              "COMPARE AND clears ordering flags");
    }
    auto parlay = machine({44, 0, 12, 0, 23, 0, 34, 0, 45, 0, 56, 1, 0, 0x97, 0});
    auto r = parlay.run();
    check(r.request && r.request->choices.size() == 5 && parlay.resume(r.request->id, 2),
          "PARLAY choice");
    check(parlay.run().state == EclState::completed && parlay.variable(0x9700) == 34,
          "PARLAY stores attitude value");
    auto blank = machine({51, 9, 0, 7, 1, 0, 0x97, 61, 0});
    r = blank.run();
    check(r.request && r.request->text == "\n" && blank.resume(r.request->id),
          "PRINT RETURN emits blank line");
    r = blank.run();
    check(blank.variable(0x9700) == 7 && r.request->clear && blank.resume(r.request->id),
          "PRINT RETURN falls through to CLEAR BOX");
    check(blank.run().state == EclState::completed, "Clear completes");
    auto noops = machine({45, 2, 0, 0, 56, 0, 1, 59, 0, 4, 1, 0, 0x49, 1, 1, 0x49, 0});
    check(noops.run().state == EclState::completed,
          "Unknown CALL/PROGRAM and broken SPELL are PoR no-ops");
    auto unknown = machine({31, 0, 0, 0, 0, 0});
    check(unknown.run().state == EclState::faulted, "Undefined opcode remains explicit");
    auto clock = machine({52, 0, 1, 0});
    check(clock.run().state == EclState::faulted, "Broken clock is not guessed");
    for (const std::uint8_t limit :
            {
                0, 1, 12, 254, 255
            })
    {
        EclMachine first(program({8, 0, limit, 1, 0, 0x97, 0})),
                   second(program({8, 0, limit, 1, 0, 0x97, 0}));
        first.bind_variable(0x9700, 0);
        second.bind_variable(0x9700, 0);
        first.seed_random(1234);
        second.seed_random(1234);
        bool saw_low = false, saw_high = false;
        for (int n = 0; n < 2048; ++n)
        {
            check(first.start(0) && second.start(0), "RNG invocation");
            check(first.run().state == EclState::completed &&
                  second.run().state == EclState::completed,
                  "RNG completes");
            const auto v = first.variable(0x9700);
            const auto maximum = std::min<unsigned>(limit, 254);
            check(v <= maximum && v == second.variable(0x9700),
                  "Seeded RNG bounds and reproducibility");
            saw_low |= v == 0;
            saw_high |= v == maximum;
        }
        check(saw_low && saw_high, "RNG reaches both endpoints");
    }
}

void memory_and_input()
{
    Bytes body{42, 1, 0x30, 0x99, 0, 1, 1, 0, 0x97, 53, 0, 200, 1, 0x30, 0x99, 0, 1, 0};
    body.resize(28, 0);
    body.insert(body.end(), {5, 77}); // Table at 9930.
    auto p = program(body);
    EclMachine vm(p), other(p);
    vm.bind_variable(0x9700, 0);
    check(vm.start(0), "Table start");
    check(vm.run().state == EclState::completed && vm.variable(0x9700) == 77,
          "Embedded byte table read");
    check(vm.variable(0x9931) == 200 && other.variable(0x9931) == 77 && p->raw().back() == 77,
          "Private writable script image");
    auto slots =
        machine({53, 2, 0x34, 0x12, 1, 0, 0x97, 0, 0, 42, 1, 0, 0x97, 0, 0, 1, 0, 0x97, 0});
    check(slots.run().state == EclState::completed && slots.variable(0x9700) == 0x1234,
          "Word-sized logical table cells");
    auto overflow = machine({42, 2, 255, 255, 0, 1, 1, 0, 0x97, 0});
    check(overflow.run().diagnostic.find("overflow") != std::string::npos,
          "Table address overflow");
    // Change a byte used by an already encountered command; a later invocation
    // must fetch the new operand, without changing another machine's image.
    auto patched = machine({9, 0, 7, 1, 0, 0x97, 53, 0, 42, 1, 0x16, 0x99, 0, 0, 0});
    check(patched.run().state == EclState::completed && patched.variable(0x9700) == 7,
          "Initial code fetch before write");
    check(patched.start(0) && patched.run().state == EclState::completed &&
          patched.variable(0x9700) == 42,
          "Modified operand fetched on restart");
    auto referenced = machine({3, 129, 0, 0x97, 2, 0, 0x97, 22, 9, 0, 8, 1, 0, 0x97, 0});
    check(referenced.run().state == EclState::completed && referenced.variable(0x9700) == 8,
          "Mixed numeric/string COMPARE uses encoded reference address");
    EclMachine strings(program({9,    128, 3, 4, 32, 192, 129, 0,  0x97, 3, 129,  0,
                                0x97, 128, 3, 4, 32, 192, 22,  17, 129,  0, 0x97, 0}));
    strings.bind_string(0x9700, "....");
    check(strings.start(0), "String start");
    auto r = strings.run();
    check(r.request && r.request->text == "ABC" && strings.string(0x9700) == "ABC",
          "String SAVE, C-string reference and mixed COMPARE");
    check(strings.resume(r.request->id) && strings.run().state == EclState::completed,
          "String continuation");
    EclMachine input(
        program({15, 0, 1, 1, 0, 0x97, 16, 0, 1, 1, 1, 0x97, 16, 0, 1, 1, 1, 0x97, 0}));
    for (std::uint16_t addr = 0x9700; addr <= 0x9729; ++addr)
        input.bind_variable(addr, 0);
    check(input.start(0), "Input start");
    r = input.run();
    check(r.request && r.request->input_limit == 6 && !input.resume(r.request->id),
          "Number input is a separate request");
    check(!input.resume_input(r.request->id, "65536") && !input.resume_input(r.request->id, "-1") &&
          !input.resume_input(r.request->id, "") && !input.resume_input(r.request->id, "1x"),
          "Invalid number keeps continuation");
    check(input.resume_input(r.request->id, "65535") && input.variable(0x9700) == 65535,
          "Input ignores encoded digit maximum");
    r = input.run();
    check(r.request->input_limit == 40 && !input.resume_input(r.request->id, std::string(41, 'X')),
          "PC string input limit");
    check(!input.resume_input(r.request->id, std::string("A\0B", 3)) &&
          input.resume_input(r.request->id, ""),
          "NUL rejected, empty input becomes space");
    r = input.run();
    check(r.request && input.string(0x9701) == " ", "Input string stored with terminator");
    check(input.resume_input(r.request->id, "Ohlo") && input.run().state == EclState::completed &&
          input.string(0x9701) == "OHLO",
          "Typed text is upper case, as the original input routine returns it");
    auto partial = machine({9, 128, 3, 4, 32, 192, 1, 0, 0x97, 0});
    check(partial.run().state == EclState::faulted && partial.variable(0x9700) == 0,
          "String writes validate full destination first");
    EclMachine bytes(program({9, 2, 255, 255, 1, 0x4b, 0xc0, 0}));
    bytes.bind_variable(0xC04B, 0);
    check(bytes.start(0), "Byte write start");
    check(bytes.run().state == EclState::completed && bytes.variable(0xC04B) == 255,
          "Map data uses byte width");
}

void host_services()
{
    EclMachine vm(program({29, 1, 0, 0x97, 50, 0, 7, 22, 9, 0, 99, 1, 0, 0x97, 0}));
    vm.bind_variable(0x9700, 0);
    vm.enable_host(29);
    vm.enable_host(50);
    check(vm.start(0), "Host start");
    auto r = vm.run();
    check(r.request && r.request->kind == EclRequestKind::host &&
          r.request->arguments[0].kind == EclArgumentKind::address &&
          r.request->arguments[0].value == 0x9700,
          "Host output address not dereferenced");
    const auto id = r.request->id;
    check(!vm.resume(id) && !vm.resume_host(id, {}),
          "Required host result cannot be acknowledged away");
    check(!vm.resume_host(id, {{{0x9700, 10}, {0x9701, 20}}}) && vm.variable(0x9700) == 0,
            "Atomic host write validation");
    check(!vm.resume_host(id, {{{0x9700, 10}, {0x9700, 20}}}),
    "Duplicate host destinations rejected");
    check(vm.resume_host(id, {{{0x9700, 10}}}) && !vm.resume_host(id, {}),
    "Host result consumed once");
    r = vm.run();
    check(r.request && r.request->instruction->opcode == 50, "FIND ITEM host request");
    check(!vm.resume_host(r.request->id, {}), "FIND ITEM requires flags");
    EclHostReply found;
    found.conditions = EclConditions{true, false, false, false, false, false};
    check(vm.resume_host(r.request->id, found) && vm.run().state == EclState::completed &&
          vm.variable(0x9700) == 99,
          "FIND ITEM controls IF");
    EclMachine transition(program({32, 0, 7, 255}));
    transition.enable_host(32);
    transition.bind_variable(0x4A00, 9);
    transition.bind_variable(0x4A20, 8);
    check(transition.start(0), "Transition start");
    r = transition.run();
    EclHostReply next;
    next.next_program = program({17, 0, 42, 0});
    check(!transition.resume_host(r.request->id, {}) && transition.resume_host(r.request->id, next),
          "NEW ECL requires resolved program");
    check(transition.state() == EclState::completed && transition.variable(0x4A00) == 0 &&
          transition.variable(0x4A20) == 8,
          "New ECL exits old script, resets local flags, preserves campaign flags");
    check(transition.start(4) && transition.run().request->text == "42", "New ECL startup entry");
    EclMachine query(
        program({30, 1, 0x1b, 0x6c, 0, 0, 1, 0, 0x97, 1, 1, 0x97, 1, 2, 0x97, 1, 3, 0x97, 0}));
    for (std::uint16_t a = 0x9700; a < 0x9704; ++a)
        query.bind_variable(a, 0);
    query.enable_host(30);
    check(query.start(0), "CHECK PARTY start");
    r = query.run();
    check(r.request->arguments[0].kind == EclArgumentKind::address &&
          r.request->arguments[0].value == 0x6c1b,
          "CHECK PARTY passes attribute address without selecting/dereferencing a character");
    check(!query.resume_host(r.request->id, {{{0x9700, 1}}}),
    "All four CHECK PARTY results required");
    check(
    query.resume_host(r.request->id, {{{0x9700, 1}, {0x9701, 3}, {0x9702, 2}, {0x9703, 0}}}) &&
    query.run().state == EclState::completed,
         "CHECK PARTY complete reply");
}

// Optional regression against the user's own DOS data. Host effects are mocked;
// the bytecode, dispatch, operands and persistent flag updates are real.
void installed_slums(const EclCatalog &catalog)
{
    const auto p = catalog.find({"ECL2.DAX", 20});
    if (!p)
        return; // Other installations need their own version-specific fixture.
    EclMachine vm(p);
    for (auto [first, last] : std::array<std::array<unsigned, 2>, 3>
{
    {{0x4900, 0x4cff}, {0x6b00, 0x6eff}, {0x9700, 0x98ff}}
})
    for (unsigned a = first; a <= last; ++a)
        vm.bind_variable(static_cast<std::uint16_t>(a), 0);
    vm.bind_variable(0xC04F, 1); // Slums event 1, as in marainein's published trace.
    for (std::uint8_t op :
            {
                11, 12, 13, 14, 28, 36
            })
        vm.enable_host(op);
    for (int visit = 0; visit < 2; ++visit)
    {
        check(vm.start(1), "Slums search entry");
        std::vector<unsigned> services;
        for (unsigned requests = 0;; ++requests)
        {
            check(requests < 50, "Slums bounded request count");
            const auto r = vm.run();
            if (r.state == EclState::faulted)
                throw EclError(r.diagnostic);
            if (r.state == EclState::completed)
                break;
            check(r.request.has_value(), "Slums request expected");
            const auto &request = *r.request;
            if (request.kind == EclRequestKind::text)
                check(vm.resume(request.id), "Slums text acknowledgement");
            else if (request.kind == EclRequestKind::menu)
            {
                check(request.choices.size() == 1 && vm.resume(request.id, 0),
                      "Slums press-return menu stores zero");
            }
            else
            {
                check(request.kind == EclRequestKind::host, "Slums host request");
                const auto opcode = request.instruction->opcode;
                services.push_back(opcode);
                if (opcode == 11)
                {
                    const auto &a = request.arguments;
                    const bool first = request.instruction->address == 0x9e5e;
                    check(a[0].value == (first ? 13 : 4) && a[1].value == (first ? 1 : 3) &&
                          a[2].value == 4,
                          "Slums monster identities/counts/icons match published trace");
                }
                EclHostReply reply;
                if (opcode == 36)
                {
                    check(vm.variable(0x6DC6) == 99 && vm.variable(0x6DCB) == 0,
                          "Slums combat inputs");
                    reply.writes = {{0x6DC7, 0}, {0x6DC8, 4}}; // Test-only combat result.
                }
                check(vm.resume_host(request.id, reply), "Slums host completion");
            }
        }
        const std::vector<unsigned> expected =
            visit == 0 ? std::vector<unsigned> {14, 12, 13, 28, 11, 11, 36}
            :
            std::vector<unsigned> {14};
        check(services == expected, "Slums event dispatch and revisit suppression");
        check(vm.variable(0x4ACA) == 255 && vm.variable(0x4ABB) == 1,
              "Slums persistent completion flag and fight count");
    }
    std::cout << "Installed Slums event 1 and revisit passed (mock combat host).\n";
}

struct Fixture
{
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("opengold-ecl-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    Fixture()
    {
        check(std::filesystem::create_directory(path), "Temporary directory");
    }

    ~Fixture()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    Fixture(const Fixture &) = delete;
    Fixture &operator=(const Fixture &) = delete;

    void write(const std::string &filename, bool invalid = false) const
    {
        auto bytes = record({0});
        // Single literal-compressed DAX record, ID 7.
        Bytes dax{9, 0,
                  7, 0,
                  0, 0,
                  0, static_cast<std::uint8_t>(bytes.size()),
                  0, static_cast<std::uint8_t>(bytes.size() + 1),
                  0};
        dax.push_back(static_cast<std::uint8_t>(bytes.size() - 1));
        dax.insert(dax.end(), bytes.begin(), bytes.end());
        if (invalid)
            dax.pop_back();
        std::ofstream out(path / filename, std::ios::binary);
        out.write(reinterpret_cast<const char *>(dax.data()),
                  static_cast<std::streamsize>(dax.size()));
        check(bool(out), "Write fixture");
    }
};

void catalogs()
{
    Fixture f;
    rejects(
        [&]
    {
        (void)EclCatalog::load(f.path);
    });
    f.write("ecl1.dax");
    f.write("ECL2.DAX");
    std::shared_ptr<const EclProgram> held;
    {
        const auto c = EclCatalog::load(f.path);
        check(c.all().size() == 2, "All banks loaded");
        held = c.find({"ECL1.DAX", 7});
        check(!c.find({"ECL1.DAX", 8}), "Missing script explicit");
    }
    EclMachine vm(held);
    check(vm.start(4) && vm.run().state == EclState::completed, "Program outlives catalog");
    f.write("ECL2.DAX", true);
    rejects(
        [&]
    {
        (void)EclCatalog::load(f.path);
    });
    check(held->source() == "ECL1.DAX:7", "Failed reload leaves existing program valid");
    if (const auto directory = std::getenv("OPENGOLD_GAME_DIR"))
    {
        const auto c = EclCatalog::load(directory);
        std::cout << "Installed ECL entry tables validated: " << c.all().size() << '\n';
        installed_slums(c);
    }
}
} // namespace

int main()
{
    try
    {
        host_random_and_string_copy();
        decoding();
        arithmetic();
        control();
        suspension();
        por_operations();
        memory_and_input();
        host_services();
        catalogs();
        std::cout << "ECL tests passed.\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
