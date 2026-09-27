#ifndef OPENGOLD_TEST_COMBAT_FIXTURE_H
#define OPENGOLD_TEST_COMBAT_FIXTURE_H
#include "opengold/rules.h"
#include <cstdint>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace opengold::test
{
// Explicitly choose the former automatic policy in unrelated feature tests.
// Dedicated Savage Attacker tests independently cover both results and skipping.
inline unsigned choose_savage_damage(rules::CombatSession &session)
{
    unsigned choices{};
    while (const auto hit = session.snapshot().savage_attack_choice)
    {
        const auto verb = !hit->second_damage                        ? "savage_use"
                          : hit->first_damage >= *hit->second_damage ? "savage_first"
                                                                     : "savage_second";
        bool accepted = false;
        for (const auto &command : session.legal_commands())
            if (command.verb == verb)
            {
                accepted = session.submit(command);
                break;
            }
        if (!accepted || ++choices > 2)
            throw std::runtime_error("Savage Attacker decision did not resolve");
    }
    return choices;
}

// Format twelve gains one absent Savage Attacker decision, without altering
// actor rows, existing Temporary HP offers, RNG, logs or movement queues.
inline std::string with_savage_choice(std::string bytes)
{
    if (!bytes.starts_with("OGCOMBAT 12 "))
        throw std::runtime_error("Expected frozen format twelve");
    bytes.replace(9, 2, "13");
    return bytes + "0\n";
}

// Independent expected transformation for frozen format-eight files: replace
// identity/format and append the known Hit Dice and recovery clocks and an empty Temporary HP pool
// to each actor row. Every other byte (including recipes, RNG, turn state, effects and queues) is
// retained.
inline std::string with_hit_dice(std::string_view bytes, const rules::Identity &identity,
                                 const std::map<unsigned, unsigned> &counts,
                                 const std::map<unsigned, unsigned> &death_clocks = {})
{
    std::istringstream input{std::string(bytes)};
    std::vector<std::string> rows;
    for (std::string row; std::getline(input, row);)
        rows.push_back(std::move(row));
    if (rows.size() < 4)
        throw std::runtime_error("Incomplete frozen combat");
    std::istringstream header(rows[0]);
    std::string magic, module, previous, content;
    unsigned format{};
    header >> magic >> format >> std::quoted(module) >> std::quoted(previous) >>
        std::quoted(content);
    if (!header || magic != "OGCOMBAT" || format != 8)
        throw std::runtime_error("Expected frozen format eight");
    std::ostringstream next;
    next << "OGCOMBAT 13 " << std::quoted(module) << ' ' << std::quoted(identity.version) << ' '
         << std::quoted(identity.content);
    rows[0] = next.str();
    std::istringstream state(rows[3]);
    std::uint64_t value{};
    for (unsigned i = 0; i < 5; ++i)
        state >> value;
    unsigned size{};
    state >> size;
    if (!state || rows.size() < 4 + size || size != counts.size())
        throw std::runtime_error("Unexpected frozen actor count");
    for (unsigned i = 0; i < size; ++i)
    {
        std::istringstream actor(rows[4 + i]);
        unsigned id{};
        actor >> id;
        rows[4 + i] += ' ' + std::to_string(counts.at(id)) + ' ' +
                       std::to_string(death_clocks.contains(id) ? death_clocks.at(id) : 0) +
                       " 0 0 \"\" 0 0";
    }
    std::string result;
    for (const auto &row : rows)
        result += row + '\n';
    return result + "0\n0\n";
}
} // namespace opengold::test
#endif
