#ifndef OPENGOLD_MERSENNE_TWISTER_H
#define OPENGOLD_MERSENNE_TWISTER_H
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace opengold
{
// The script machine's dice: MT19937, drawing exactly the numbers
// std::mt19937 draws from the same seed (the standard fixes that sequence),
// with a saved text the project owns. The standard libraries write their
// engine's state differently (libc++ and MSVC 624 words, libstdc++ 624 words
// and the position), so a save holding a library's text did not load on
// another platform.
class MersenneTwister
{
  public:
    using result_type = std::uint32_t;
    static constexpr std::size_t state_size = 624;

    explicit MersenneTwister(result_type seed = 5489U)
    {
        this->seed(seed);
    }

    void seed(result_type seed);
    result_type operator()();

    [[nodiscard]] static constexpr result_type min()
    {
        return 0;
    }

    [[nodiscard]] static constexpr result_type max()
    {
        return std::numeric_limits<result_type>::max();
    }

    // The saved text: the 624 state words and the position of the next one,
    // as libstdc++ writes std::mt19937.
    [[nodiscard]] std::string state_text() const;
    // Reads state_text(). Nothing else.
    [[nodiscard]] static std::optional<MersenneTwister> from_state_text(std::string_view text);

    friend bool operator==(const MersenneTwister &, const MersenneTwister &) = default;

  private:
    void twist();

    // A block of state words and the index of the next one to temper; at
    // state_size the block is spent and the next draw makes a new one.
    std::array<result_type, state_size> words_{};
    std::size_t next_{state_size};
};
} // namespace opengold
#endif
