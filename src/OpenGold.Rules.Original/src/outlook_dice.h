#ifndef OPENGOLD_ORIGINAL_OUTLOOK_DICE_H
#define OPENGOLD_ORIGINAL_OUTLOOK_DICE_H
#include <cstddef>
#include <utility>
#include <vector>

// The dice behind an outlook: every way a rule that draws dice can go, and
// the chances of what dice total.
namespace opengold::original::detail
{
// The dice a run of a rule draws, served from a script of outcomes, so that
// each_way can try every way the run can go. The first die the script does
// not cover is noted, and any it does not cover falls as a 1 for a run whose
// result is set aside.
class ScriptedDice
{
  public:
    explicit ScriptedDice(const std::vector<int> &script) : script_(script)
    {
    }

    int operator()(int sides)
    {
        if (drawn_ < script_.size())
        {
            return script_[drawn_++];
        }
        if (unscripted_sides_ == 0)
        {
            unscripted_sides_ = sides;
        }
        ++drawn_;
        return 1;
    }

    // The sides of the first die the script did not cover, 0 for none.
    [[nodiscard]] int unscripted_sides() const noexcept
    {
        return unscripted_sides_;
    }

  private:
    const std::vector<int> &script_;
    std::size_t drawn_{};
    int unscripted_sides_{};
};

// Every way a run of a rule that draws dice can go, each with its chance:
// `run` (given the dice to draw from) is run again for each outcome of each
// die it draws, and `take` given each way's chance and the run's result.
// The run must change nothing but its dice, as the rule's const parts do.
template <typename Result, typename Run, typename Take>
void each_way(const Run &run, const Take &take)
{
    std::vector<std::pair<std::vector<int>, double>> pending{{{}, 1.0}};
    while (!pending.empty())
    {
        auto [script, chance] = std::move(pending.back());
        pending.pop_back();
        ScriptedDice dice(script);
        const Result result = run(dice);
        const int sides = dice.unscripted_sides();
        if (sides == 0)
        {
            take(chance, result);
            continue;
        }
        for (int outcome = 1; outcome <= sides; ++outcome)
        {
            auto next = script;
            next.push_back(outcome);
            pending.emplace_back(std::move(next), chance / sides);
        }
    }
}

// The chance of each total of `count` dice of `sides`, plus `bonus`, by the total.
[[nodiscard]] std::vector<double> dice_chances(int count, int sides, int bonus);
// The chance of each sum of two amounts that fall apart from each other.
[[nodiscard]] std::vector<double> sum_chances(const std::vector<double> &first,
                                              const std::vector<double> &second);
} // namespace opengold::original::detail
#endif
