#include "opengold/mersenne_twister.h"

#include <charconv>
#include <vector>

namespace opengold
{
namespace
{
// MT19937's constants (Matsumoto and Nishimura, 1998).
constexpr std::size_t shift_size = 397;
constexpr std::uint32_t matrix = 0x9908b0dfU;
constexpr std::uint32_t upper_bit = 0x80000000U;
constexpr std::uint32_t lower_bits = 0x7fffffffU;
constexpr std::uint32_t initialization = 1812433253U;

std::uint32_t temper(std::uint32_t value)
{
    value ^= value >> 11;
    value ^= (value << 7) & 0x9d2c5680U;
    value ^= (value << 15) & 0xefc60000U;
    return value ^ (value >> 18);
}

// The unsigned decimal numbers of `text`, separated by spaces; none if any
// other character or a number above 32 bits is there, or there are more than
// `limit`.
std::optional<std::vector<std::uint64_t>> numbers(std::string_view text, std::size_t limit)
{
    std::vector<std::uint64_t> result;
    const char *at = text.data();
    const char *const end = at + text.size();
    while (true)
    {
        while (at != end && (*at == ' ' || *at == '\n' || *at == '\t' || *at == '\r'))
        {
            ++at;
        }
        if (at == end)
        {
            return result;
        }
        std::uint64_t value{};
        const auto [stop, error] = std::from_chars(at, end, value);
        if (error != std::errc{} || value > 0xffffffffU || result.size() == limit)
        {
            return std::nullopt;
        }
        result.push_back(value);
        at = stop;
        if (at != end && *at != ' ' && *at != '\n' && *at != '\t' && *at != '\r')
        {
            return std::nullopt;
        }
    }
}
} // namespace

void MersenneTwister::seed(result_type seed)
{
    words_[0] = seed;
    for (std::size_t i = 1; i < state_size; ++i)
    {
        const auto previous = words_[i - 1];
        words_[i] = initialization * (previous ^ (previous >> 30)) + static_cast<std::uint32_t>(i);
    }
    next_ = state_size;
}

void MersenneTwister::twist()
{
    for (std::size_t i = 0; i < state_size; ++i)
    {
        const auto mixed = (words_[i] & upper_bit) | (words_[(i + 1) % state_size] & lower_bits);
        words_[i] = words_[(i + shift_size) % state_size] ^ (mixed >> 1) ^
                    ((mixed & 1U) != 0 ? matrix : 0U);
    }
    next_ = 0;
}

MersenneTwister::result_type MersenneTwister::operator()()
{
    if (next_ >= state_size)
    {
        twist();
    }
    return temper(words_[next_++]);
}

std::string MersenneTwister::state_text() const
{
    std::string text;
    text.reserve(state_size * 11 + 4);
    for (const auto word : words_)
    {
        text += std::to_string(word);
        text += ' ';
    }
    text += std::to_string(next_);
    return text;
}

std::optional<MersenneTwister> MersenneTwister::from_state_text(std::string_view text)
{
    const auto read = numbers(text, state_size + 1);
    if (!read || read->size() < state_size)
    {
        return std::nullopt;
    }
    MersenneTwister engine;
    for (std::size_t i = 0; i < state_size; ++i)
    {
        engine.words_[i] = static_cast<result_type>((*read)[i]);
    }
    // libc++ and MSVC write the last 624 values made, oldest first: the block
    // a fresh twist starts from, so the next draw twists, as after a seed.
    engine.next_ = state_size;
    if (read->size() == state_size + 1)
    {
        if ((*read)[state_size] > state_size)
        {
            return std::nullopt;
        }
        engine.next_ = static_cast<std::size_t>((*read)[state_size]);
    }
    return engine;
}
} // namespace opengold
