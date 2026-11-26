#include "opengold/mersenne_twister.h"
#include "test_support.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <sstream>
#include <string>

using opengold::MersenneTwister;
using opengold::test::check;

namespace
{
std::string fixture(const std::string &name)
{
    std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures" / name);
    check(static_cast<bool>(in), "Missing fixture " + name);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// The engine draws the standard's MT19937 sequence: the dice of every
// fixed-dice route stay as they were with std::mt19937.
void draws_what_std_mt19937_draws()
{
    for (const std::uint32_t seed : {5489U, 0U, 1U, 0x4f474246U, 0xffffffffU})
    {
        std::mt19937 standard(seed);
        MersenneTwister engine(seed);
        for (int i = 0; i < 20000; ++i)
        {
            check(engine() == standard(), "Draw " + std::to_string(i) + " of seed " +
                                              std::to_string(seed) + " differs from std::mt19937");
        }
    }
    // The standard names the 10,000th draw of the default seed.
    MersenneTwister engine;
    std::uint32_t last{};
    for (int i = 0; i < 10000; ++i)
    {
        last = engine();
    }
    check(last == 4123659995U, "The 10,000th draw of seed 5489 must be 4123659995");
}

// A saved engine continues with the same draws, at every position in a block.
void round_trips_its_text()
{
    for (const int drawn : {0, 1, 2, 396, 397, 623, 624, 625, 1247, 1248, 5000})
    {
        MersenneTwister engine(20261126U);
        for (int i = 0; i < drawn; ++i)
        {
            (void)engine();
        }
        const auto text = engine.state_text();
        auto loaded = MersenneTwister::from_state_text(text);
        check(loaded.has_value(), "The engine must read its own text");
        check(*loaded == engine, "The read engine must equal the saved one");
        check(loaded->state_text() == text, "The text must round-trip unchanged");
        for (int i = 0; i < 3000; ++i)
        {
            check((*loaded)() == engine(), "A loaded engine must draw what the saved one draws");
        }
    }
}

// What the host's standard library writes for std::mt19937 loads and goes on
// with the same draws: on macOS libc++'s text, on Linux libstdc++'s, on
// Windows MSVC's.
void reads_the_host_library_text()
{
    for (const int drawn : {0, 1, 623, 624, 625, 1000, 5000})
    {
        std::mt19937 standard(7U);
        standard.discard(static_cast<unsigned long long>(drawn));
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << standard;
        auto loaded = MersenneTwister::from_state_text(out.str());
        check(loaded.has_value(), "The host library's std::mt19937 text must load");
        for (int i = 0; i < 3000; ++i)
        {
            check((*loaded)() == standard(),
                  "After " + std::to_string(drawn) +
                      " draws the loaded text must go on as std::mt19937");
        }
    }
}

// Saves written before this engine hold libc++'s or libstdc++'s text; both
// load on every platform. Both fixtures are seed 5489 after 1,000 draws.
void reads_both_libraries_saved_text()
{
    for (const auto *name : {"mt19937-libcxx-5489-1000.txt", "mt19937-libstdcxx-5489-1000.txt"})
    {
        auto loaded = MersenneTwister::from_state_text(fixture(name));
        check(loaded.has_value(), std::string("The text of ") + name + " must load");
        for (const std::uint32_t expected :
             {2500741117U, 4263797064U, 2322457777U, 1155622524U, 3736368257U})
        {
            check((*loaded)() == expected, std::string(name) + " must go on with its draws");
        }
    }
    // libstdc++'s form is the one the engine writes: a Linux save made before
    // stays byte-identical when written again.
    MersenneTwister engine;
    for (int i = 0; i < 1000; ++i)
    {
        (void)engine();
    }
    auto linux_text = fixture("mt19937-libstdcxx-5489-1000.txt");
    while (!linux_text.empty() && (linux_text.back() == '\n' || linux_text.back() == '\r'))
    {
        linux_text.pop_back();
    }
    check(engine.state_text() == linux_text, "The engine must write libstdc++'s text");
}

void rejects_other_text()
{
    MersenneTwister engine;
    const auto words = engine.state_text();
    const auto without_position = words.substr(0, words.rfind(' '));
    check(MersenneTwister::from_state_text(without_position).has_value(),
          "624 words are libc++'s form");
    check(!MersenneTwister::from_state_text("").has_value(), "Empty text is no engine");
    check(!MersenneTwister::from_state_text("1 2 3").has_value(), "Three words are no engine");
    check(!MersenneTwister::from_state_text(words + " 7").has_value(), "626 words are no engine");
    check(!MersenneTwister::from_state_text(without_position + " 625").has_value(),
          "A position past the block is no engine");
    check(!MersenneTwister::from_state_text(without_position + " x").has_value(),
          "A word that is no number is no engine");
    check(
        !MersenneTwister::from_state_text("4294967296" + words.substr(words.find(' '))).has_value(),
        "A word above 32 bits is no engine");
}
} // namespace

int main()
{
    try
    {
        draws_what_std_mt19937_draws();
        round_trips_its_text();
        reads_the_host_library_text();
        reads_both_libraries_saved_text();
        rejects_other_text();
        std::cout << "Mersenne Twister tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
