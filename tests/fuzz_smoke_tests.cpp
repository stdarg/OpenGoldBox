#include "fuzz/fuzz_cases.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>

namespace
{
using namespace opengold::test;
using Exercise = void (*)(std::span<const std::uint8_t>);

void corpus(const std::filesystem::path &directory, const std::vector<FuzzSeed> &seeds)
{
    std::filesystem::create_directories(directory);
    for (const auto &seed : seeds)
    {
        std::ofstream output(directory / seed.name, std::ios::binary);
        output.write(reinterpret_cast<const char *>(seed.bytes.data()), seed.bytes.size());
        if (!output)
            throw std::runtime_error("Could not write synthetic corpus");
    }
}

std::size_t mutate(const std::vector<FuzzSeed> &seeds, Exercise exercise, std::mt19937 &random)
{
    std::size_t count = 0;
    for (const auto &seed : seeds)
    {
        exercise(seed.bytes);
        for (unsigned iteration = 0; iteration < 160; ++iteration)
        {
            auto bytes = seed.bytes;
            const auto at = random() % bytes.size();
            switch (iteration % 5)
            {
            case 0:
                bytes[at] ^= 1u << (random() % 8);
                break;
            case 1:
                bytes[at] = random() % 256;
                break;
            case 2:
                bytes.resize(at);
                break;
            case 3:
                bytes.erase(bytes.begin() + at);
                break;
            case 4:
            {
                const auto copies = 1 + random() % 8;
                const auto value = static_cast<std::uint8_t>(random() % 256);
                bytes.insert(bytes.begin() + at, copies, value);
                break;
            }
            }
            try
            {
                exercise(bytes);
            }
            catch (const std::exception &error)
            {
                throw std::runtime_error(seed.name + " mutation " + std::to_string(iteration) +
                                         ": " + error.what());
            }
            ++count;
        }
    }
    return count;
}
} // namespace

int main(int argc, char **argv)
{
    try
    {
        if (argc == 3 && std::string_view(argv[1]) == "--write-corpus")
        {
            const std::filesystem::path root(argv[2]);
            corpus(root / "formats", format_seeds());
            corpus(root / "checkpoint", checkpoint_seeds());
            std::cout << "Wrote authored fuzz seeds to " << root << '\n';
            return 0;
        }
        if (argc == 3 && (std::string_view(argv[1]) == "--replay-formats" ||
                          std::string_view(argv[1]) == "--replay-checkpoint"))
        {
            std::ifstream input(argv[2], std::ios::binary);
            if (!input)
                throw std::runtime_error("Could not read replay input");
            const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
            (std::string_view(argv[1]) == "--replay-formats" ? exercise_formats
             : exercise_checkpoint)(bytes);
            std::cout << "Fuzz input replay passed\n";
            return 0;
        }
        if (argc != 1)
            throw std::runtime_error(
                "Use --write-corpus DIR or --replay-{formats,checkpoint} FILE");
        // mt19937 output and modulo selection are reproducible across standard
        // libraries; uniform_int_distribution does not promise that property.
        std::mt19937 random(0x4f474246);
        auto count = mutate(format_seeds(), exercise_formats, random);
        count += mutate(checkpoint_seeds(), exercise_checkpoint, random);
        exercise_formats({});
        exercise_checkpoint({});
        std::cout << count << " deterministic fuzz mutations and seed invariants passed\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
