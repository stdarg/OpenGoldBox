#include "concentration.h"
#include <iostream>
#include <limits>
#include <sstream>
using namespace opengold::srd5::detail;

namespace
{
void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

const Concentration first{{7, 1, 9}, 600000},
    second{{7, 2, 9}, 600000}; // Silence's maximum duration.

void lifecycle()
{
    ConcentrationState state, other;
    check(!state.begin(first), "First application replaces nothing");
    check(state.begin(second) == first.source && state.active()->source == second.source,
          "Recast retires precisely the previous application");
    other.begin({{8, 1, 9}, 1000});
    check(other.end() == ConcentrationSource{8, 1, 9} && state.active(),
          "Separate encounter/owner state stays independent");
    for (auto invalid : {Concentration{{0, 1, 9}, 1}, Concentration{{7, 0, 9}, 1},
                         Concentration{{7, 1, 0}, 1}, Concentration{{7, 1, 9}, 0}})
    {
        auto before = state;
        bool rejected = false;
        try
        {
            state.begin(invalid);
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        check(rejected && state == before, "Invalid replacement preserves old concentration");
    }
    auto before = state;
    bool rejected = false;
    try
    {
        state.begin(first, true);
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    check(rejected && state == before, "Incapacitated caster cannot start concentration");
    auto copy = state;
    check(!state.elapse(599999) && state.active()->remaining_ms == 1,
          "Effect remains until exact deadline");
    check(state.elapse(1) == second.source && copy.elapse(600000) == second.source && state == copy,
          "Time partitioning ends the same source");
    check(!state.end(), "Ending empty state is inert");
    state.begin(first);
    check(state.incapacitate() == first.source && !state.active(),
          "Incapacitation immediately ends concentration");
    state.begin(first);
    check(state.end() == first.source && !state.active(),
          "Voluntary release returns cleanup identity");
}

std::string encode(const ConcentrationState &state)
{
    std::ostringstream out;
    write_concentration(out, state);
    return out.str();
}

ConcentrationState decode(const std::string &bytes)
{
    std::istringstream in(bytes);
    auto result = read_concentration(in);
    in >> std::ws;
    check(in.eof(), "No trailing record fields");
    return result;
}

void persistence()
{
    ConcentrationState state;
    check(encode(state) == "CN1 0" && decode("CN1 0") == state, "Canonical empty state");
    state.begin(first);
    state.elapse(1234);
    auto bytes = encode(state);
    check(bytes == "CN1 1 7 1 9 598766", "Independent canonical active record");
    auto restored = decode(bytes);
    check(restored == state, "Application identity and remaining time retained");
    std::uint64_t a = 0, b = 0;
    auto left = state.damage(22, 100, {}, false, a), right = restored.damage(22, 100, {}, false, b);
    check(a == b && left.save->natural == right.save->natural && encode(state) == encode(restored),
          "Same external RNG and saved state give exact damage continuation");
    check(state.elapse(598766) == first.source && restored.elapse(598766) == first.source &&
              state == restored,
          "Restored expiry removes identical source");
    for (const char *bad : {"", "CN2 0", "CN1 -1", "CN1 2", "CN1 1", "CN1 1 0 1 9 1",
                            "CN1 1 7 0 9 1", "CN1 1 7 1 0 1", "CN1 1 7 1 9 0", "CN1 1 7 1 9 -1",
                            "CN1 1 -7 1 9 1", "CN1 1 7 1 9 18446744073709551616",
                            "CN1 1 7 1 4294967296 1", "CN1 1 7 1 9 12junk", "CN1 0 trailing"})
    {
        state.begin(first);
        auto before = state;
        bool rejected = false;
        try
        {
            state = decode(bad);
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        check(rejected && state == before, "Malformed record cannot replace existing state");
    }
    // This is a new subrecord, not a historical campaign fixture. Integration
    // tests must supply actual old-writer combat/campaign files when embedding it.
}

void damage()
{
    for (int amount : {0, 1, 19, 20, 21, 22, 59, 60, 61, std::numeric_limits<int>::max()})
    {
        ConcentrationState state;
        state.begin(first);
        std::uint64_t random = 13;
        const auto result = state.damage(amount, 100, {}, false, random);
        const int expected = amount < 22 ? 10 : amount == 22 ? 11 : amount == 59 ? 29 : 30;
        check(amount ? result.save && result.save->dc == expected && !result.ended
                     : !result.save && !result.ended,
              "Independent damage DC boundaries and zero damage");
        check(random == (amount ? 13 + 0x9e3779b97f4a7c15ULL : 13),
              "Exactly one save draw for positive damage");
    }
    // Use extreme bonuses to prove saves do not use attack-style automatic results.
    bool one = false, twenty = false;
    for (unsigned seed = 0; seed < 200; ++seed)
    {
        for (int bonus : {-100, 100})
        {
            ConcentrationState state;
            state.begin(first);
            std::uint64_t random = seed;
            auto result = state.damage(1, bonus, {}, false, random);
            one |= result.save->natural == 1;
            twenty |= result.save->natural == 20;
            check(result.save->success == (bonus == 100) && bool(state.active()) == (bonus == 100),
                  "Natural 1/20 never override saving throw total");
            check(bonus == 100 ? !result.ended : result.ended == first.source,
                  "Failure identifies the expired application");
        }
    }
    check(one && twenty, "Both natural extremes exercised");
    for (auto modifiers :
         {RollModifiers{true, false}, RollModifiers{false, true}, RollModifiers{true, true}})
    {
        ConcentrationState state;
        state.begin(first);
        std::uint64_t random = 13;
        auto result = state.damage(25, 100, modifiers, false, random);
        check(result.save->mode == modifiers.mode() &&
                  random == 13 + 0x9e3779b97f4a7c15ULL * (modifiers.mode() ? 2 : 1),
              "Shared advantage/disadvantage cancellation and draw count");
    }
    ConcentrationState state;
    state.begin(first);
    std::uint64_t random = 13;
    auto result = state.damage(10, 0, {}, true, random);
    check(!result.save && result.ended == first.source && random == 13,
          "Lethal/incapacitating damage ends without save");
    check(!state.damage(10, 0, {}, false, random).save && random == 13,
          "No concentration means no save");
    state.begin(first);
    auto before = state;
    bool rejected = false;
    try
    {
        state.damage(-1, 0, {}, false, random);
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    check(rejected && state == before && random == 13,
          "Invalid damage leaves state and RNG intact");
}
} // namespace

int main()
{
    try
    {
        lifecycle();
        damage();
        persistence();
        std::cout << "Concentration transitions passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
