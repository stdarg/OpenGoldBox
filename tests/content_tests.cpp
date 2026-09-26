#include "opengold/srd5.h"
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

using namespace opengold;
namespace
{
void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
void rejects(std::string_view bytes)
{
    try
    {
        (void)srd5::parse_content(bytes);
    }
    catch (const std::runtime_error &)
    {
        return;
    }
    throw std::runtime_error("Malformed content was accepted: " +
                             std::string(bytes.substr(0, 200)));
}
constexpr auto header = "OPENGOLD_SRD5 1 synthetic.1\n";
constexpr std::array<int, 19> definition{12, 11, 1,  30,  3, 1, 6, 1, 3, 1,
                                         8,  1,  80, 320, 0, 0, 0, 1, 0};
std::string row(std::array<int, 19> values = definition)
{
    std::ostringstream out;
    out << "creature bandit";
    for (auto value : values)
        out << ' ' << value;
    return out.str() + '\n';
}
// This executable links only the SRD static library (and its rules interface),
// proving rest mechanics can execute without Core, campaign assets or Godot.
void standalone_rest_rules()
{
    using namespace rules;
    const auto module = srd5::parse_content(std::string(header) + row());
    auto progress = module->begin_rest(RestKind::long_rest);
    auto step = module->advance_rest(progress, 70 * 60000, RestWork::sleep);
    check(step.progress && step.benefit == RestBenefit::none,
          "Sleep advances without early benefits");
    step = module->interrupt_rest(*step.progress, RestInterruption::damage);
    check(step.progress && step.progress->interrupted && step.benefit == RestBenefit::short_rest &&
              module->remaining_rest(*step.progress) == 470 * 60000,
          "SRD library determines interruption and benefits");
    progress = module->resume_rest(*step.progress);
    module->validate_rest(progress);
    step = module->advance_rest(progress, module->remaining_rest(progress), progress.work);
    check(!step.progress && step.benefit == RestBenefit::long_rest &&
              step.completed_duration_milliseconds == 540 * 60000,
          "Standalone SRD library completes the extended rest");
    progress = module->begin_rest(RestKind::short_rest);
    check(!module->interrupt_rest(progress, RestInterruption::initiative).progress,
          "SRD Short Rest interruption cancels");
}
void malformed_content()
{
    for (const auto *invalid : {"", "OPENGOLD_SRD5", "OPENGOLD_SRD5 2 synthetic.1\n",
                                "OTHER 1 synthetic.1\n", "OPENGOLD_SRD5 1 synthetic.1 extra\n"})
        rejects(std::string(invalid) + row());
    rejects(header);
    rejects(std::string(header) + "# comments without definitions\n");
    rejects("OPENGOLD_SRD5 1 " + std::string(81, 'x') + '\n' + row());
    rejects(std::string(header) + row() + row());
    rejects(std::string(header) + "creature missing-fields 12 11\n");
    rejects(std::string(header) + "spell bandit" + row().substr(15));
    rejects(std::string(header) + "creature " + std::string(81, 'x') + row().substr(15));
    rejects(std::string(header) + row().substr(0, row().size() - 1) + " extra\n");
    rejects(std::string(header) + "creature bandit 9999999999999999999999" + row().substr(18));
    // Both ends of every numeric field, plus non-grid movement and reversed ranges.
    constexpr std::array<int, 19> low{1, 1,   -10, 5,  -10, 1, 2,   -10, -10, 1,
                                      2, -10, 5,   80, 0,   0, -10, 1,   0};
    constexpr std::array<int, 19> high{40, 1000, 20,  120, 30, 10, 20, 30, 30, 10,
                                       20, 30,   320, 600, 10, 20, 30, 4,  7};
    for (std::size_t i = 0; i < definition.size(); ++i)
    {
        for (auto boundary : {low[i], high[i]})
        {
            auto values = definition;
            values[i] = boundary;
            check(bool(srd5::parse_content(std::string(header) + row(values))),
                  "Valid field boundary accepts");
        }
        for (auto invalid : {low[i] - 1, high[i] + 1})
        {
            auto values = definition;
            values[i] = invalid;
            rejects(std::string(header) + row(values));
        }
    }
    auto values = definition;
    values[3] = 31;
    rejects(std::string(header) + row(values));
    auto maximum = std::string(header) + row() + "#";
    maximum.resize(65536, 'x');
    check(bool(srd5::parse_content(maximum)), "Maximum content size accepts");
    maximum += 'x';
    rejects(maximum);
}
void identities_and_sessions()
{
    const auto bytes = std::string(header) + row();
    std::string crlf;
    for (auto c : bytes)
    {
        if (c == '\n')
            crlf += '\r';
        crlf += c;
    }
    auto module = srd5::parse_content(std::string(bytes)); // Temporary source must not be borrowed.
    auto windows = srd5::parse_content(crlf);
    check(module->identity() == windows->identity(), "Line endings preserve rules identity");
    auto changed = definition;
    changed[1] = 12;
    check(module->identity() != srd5::parse_content(std::string(header) + row(changed))->identity(),
          "Changing mechanics changes rules identity");
    rules::Encounter encounter{
        {4, 4, std::vector<std::uint8_t>(16)},
        {{1, "bandit", "Ally", 0, {0, 0}}, {2, "bandit", "Enemy", 1, {3, 3}}}};
    const auto checkpoint = module->create(encounter, 42)->save();
    check(windows->restore(checkpoint)->save() == checkpoint,
          "Parsed content creates and restores a combat session");
    const auto path =
        std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/rules/srd-5.2.1/combat.rules";
    std::ifstream input(path, std::ios::binary);
    check(bool(input), "Pinned content fixture opens");
    const std::string installed{std::istreambuf_iterator<char>(input), {}};
    check(srd5::load(path)->identity() == srd5::parse_content(installed)->identity(),
          "File loading and in-memory parsing agree on pinned identity");
}
} // namespace
int main()
{
    try
    {
        standalone_rest_rules();
        malformed_content();
        identities_and_sessions();
        std::cout << "Rules content tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
