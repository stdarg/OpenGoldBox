#ifndef OPENGOLD_DICE_H
#define OPENGOLD_DICE_H

#include "opengold/random_state.h"
#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace opengold
{
struct RolledDie
{
    int sides{}, result{};
};

// While set, every die rolled on this thread is added here: a fight's log shows
// the dice behind a damage total. DiceTally sets it.
inline thread_local std::vector<RolledDie> *dice_tally = nullptr;

// Told of every die rolled, for the log file (diagnostic_log.h); none by default.
inline std::atomic<void (*)(int sides, int result)> dice_observer{nullptr};

// Adds the dice this thread rolls to `rolled` while it lives.
class DiceTally
{
  public:
    explicit DiceTally(std::vector<RolledDie> &rolled) : previous_(dice_tally)
    {
        dice_tally = &rolled;
    }

    ~DiceTally()
    {
        dice_tally = previous_;
    }

    DiceTally(const DiceTally &) = delete;
    DiceTally &operator=(const DiceTally &) = delete;

  private:
    std::vector<RolledDie> *previous_;
};

// SplitMix64 with rejection sampling. Preserve the sequence for existing saves
// and seeded encounters across all SRD dice consumers.
inline int roll_die(std::uint64_t &state, int sides)
{
    // A die needs a side; zero would divide by zero below. Original game
    // scripts supply some dice, so bad data must be refused, not rolled.
    if (sides < 1)
    {
        throw std::invalid_argument("A die needs at least one side");
    }
    const auto count = static_cast<std::uint64_t>(sides);
    const auto threshold = (std::uint64_t{0} - count) % count;
    std::uint64_t value;
    do
    {
        value = (state += 0x9e3779b97f4a7c15ULL);
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        value ^= value >> 31;
    } while (value < threshold);
    const int result = static_cast<int>(value % count) + 1;
    if (dice_tally)
    {
        dice_tally->push_back({sides, result});
    }
    if (const auto observe = dice_observer.load(std::memory_order_relaxed))
    {
        observe(sides, result);
    }
    return result;
}

inline int roll_die(RandomState &state, int sides)
{
    return roll_die(state.value, sides);
}
} // namespace opengold
#endif
