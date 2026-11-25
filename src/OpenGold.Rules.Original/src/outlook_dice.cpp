#include "outlook_dice.h"

#include <algorithm>

namespace opengold::original::detail
{
// The chance of each total of `count` dice of `sides`, plus `bonus`, by the total.
std::vector<double> dice_chances(int count, int sides, int bonus)
{
    std::vector<double> chances{1.0};
    for (int die = 0; die < count; ++die)
    {
        std::vector<double> next(chances.size() + static_cast<std::size_t>(sides), 0.0);
        for (std::size_t total = 0; total < chances.size(); ++total)
        {
            for (int face = 1; face <= sides; ++face)
            {
                next[total + static_cast<std::size_t>(face)] += chances[total] / sides;
            }
        }
        chances = std::move(next);
    }
    std::vector<double> shifted(static_cast<std::size_t>(std::max(bonus, 0)), 0.0);
    shifted.insert(shifted.end(), chances.begin(), chances.end());
    return shifted;
}

// The chance of each sum of two amounts that fall apart from each other.
std::vector<double> sum_chances(const std::vector<double> &first, const std::vector<double> &second)
{
    std::vector<double> sum(first.size() + second.size() - 1, 0.0);
    for (std::size_t a = 0; a < first.size(); ++a)
    {
        for (std::size_t b = 0; b < second.size(); ++b)
        {
            sum[a + b] += first[a] * second[b];
        }
    }
    return sum;
}
} // namespace opengold::original::detail
