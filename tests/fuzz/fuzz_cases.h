#ifndef OPENGOLD_TEST_FUZZ_CASES_H
#define OPENGOLD_TEST_FUZZ_CASES_H
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace opengold::test
{
struct FuzzSeed
{
    std::string name;
    std::vector<std::uint8_t> bytes;
};
void exercise_formats(std::span<const std::uint8_t> bytes);
void exercise_checkpoint(std::span<const std::uint8_t> bytes);
std::vector<FuzzSeed> format_seeds();
std::vector<FuzzSeed> checkpoint_seeds();
} // namespace opengold::test
#endif
