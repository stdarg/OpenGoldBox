#include "opengold/campaign_party.h"
#include "opengold/character.h"
#include "opengold/rolf_tour.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>

using namespace opengold;
using namespace opengold::por;

// Word of Recall in New Phlan (LEVELS-3): cast on the temple's square it
// prepares the party's sanctuary; cast elsewhere it brings the party back.
// Needs the installed game (OPENGOLD_GAME_DIR); without it the test passes.
namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const std::string &message)
{
    if (!ok)
        throw std::runtime_error(message);
}

std::unique_ptr<rules::RulesModule> module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

// A level-11 Cleric who prepared Word of Recall at level 11.
Character cleric(std::string name = "Cleric")
{
    rules::CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "cleric";
    d.background = "acolyte";
    d.alignment = "lawful_good";
    d.name = std::move(name);
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.cantrips = std::vector<std::string>{"sacred_flame", "spare_the_dying", "resistance"};
    d.spells = rules::SpellChoices{{}, std::vector<std::string>{"cure_wounds"}, {}, {}};
    d.training = {{"class:cleric", {"medicine", "persuasion"}},
                  {"class:cleric:divine_order", {"protector"}}};
    Character hero(*srd5::character_rules(), d, {});
    rules::VitalState life{hero.sheet().hit_points, false, {}};
    const auto rules = module();
    while (hero.sheet().level < 11)
    {
        auto choice = rules->default_advancement(hero.sheet());
        if (hero.sheet().level == 10)
        {
            auto spells = hero.sheet().prepared_spells;
            spells.push_back("word_of_recall");
            for (const auto &spell : choice.spells)
                if (spells.size() < choice.spells.size() &&
                    std::find(spells.begin(), spells.end(), spell) == spells.end())
                    spells.push_back(spell);
            choice.spells = spells;
        }
        check(hero.advance(*rules, life, choice), "An ordinary level-up");
    }
    return hero;
}

void settle(RolfTourSession &town, bool *temple = nullptr)
{
    for (unsigned n = 0; n < 2000; ++n)
    {
        const auto s = town.snapshot();
        if (s.phase == TourPhase::completed)
            return;
        check(s.phase != TourPhase::faulted, s.diagnostic);
        if (s.phase == TourPhase::awaiting_continue)
        {
            // The temple's service menu: leave it with Cancel.
            const bool service = std::any_of(s.choices.begin(), s.choices.end(),
                                             [](const auto &c)
                                             {
                                                 return c == "Cancel";
                                             });
            if (temple && service)
                *temple = true;
            // At a temple, ask for its service: "DO YOU SEEK HEALING?" YES.
            const bool asked = temple && s.dialogue.find("SEEK HEALING") != std::string::npos;
            town.choose(s.continue_ticket, asked || s.choices.empty() ? 0 : s.choices.size() - 1);
        }
        else if (s.phase == TourPhase::awaiting_input)
            town.input(s.continue_ticket, "0");
        else if (s.phase == TourPhase::shopping)
            town.leave_shop(s.continue_ticket);
        else
            town.advance(.3);
    }
    throw std::runtime_error("A town event did not settle");
}

// Walks to a square of the town map; true when the party stands on it.
bool walk_to(RolfTourSession &town, unsigned tx, unsigned ty, bool *temple = nullptr)
{
    constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
    std::set<std::pair<int, int>> refused;
    for (unsigned step = 0; step < 400; ++step)
    {
        const auto p = town.snapshot().pose;
        if (p.x == tx && p.y == ty)
            return true;
        std::array<int, 256> previous;
        previous.fill(-1);
        std::queue<int> cells;
        const int origin = static_cast<int>(p.y * 16 + p.x);
        cells.push(origin);
        previous[static_cast<std::size_t>(origin)] = origin;
        while (!cells.empty())
        {
            const int cell = cells.front();
            cells.pop();
            for (unsigned d = 0; d < 4; ++d)
            {
                const int x = cell % 16 + dx[d], y = cell / 16 + dy[d];
                if (x < 0 || y < 0 || x >= 16 || y >= 16)
                    continue;
                const auto &a = town.map().at(cell % 16, cell / 16);
                const auto &b = town.map().at(x, y);
                const unsigned reverse = (d + 2) % 4;
                if (a.doors[d] > 1 || b.doors[reverse] > 1 || (a.walls[d] && !a.doors[d]) ||
                    (b.walls[reverse] && !b.doors[reverse]))
                    continue;
                const int next = y * 16 + x;
                if (previous[static_cast<std::size_t>(next)] >= 0 || refused.contains({cell, next}))
                    continue;
                previous[static_cast<std::size_t>(next)] = cell;
                cells.push(next);
            }
        }
        int next = static_cast<int>(ty * 16 + tx);
        if (previous[static_cast<std::size_t>(next)] < 0)
            return false;
        while (previous[static_cast<std::size_t>(next)] != origin)
            next = previous[static_cast<std::size_t>(next)];
        const unsigned facing = next % 16 > int(p.x)   ? 1
                                : next % 16 < int(p.x) ? 3
                                : next / 16 > int(p.y) ? 2
                                                       : 0;
        const bool forward = index(p.facing) == facing;
        town.explore(forward ? ExplorationCommand::forward : ExplorationCommand::turn_right);
        settle(town, temple);
        const auto now = town.snapshot().pose;
        if (forward && static_cast<int>(now.y * 16 + now.x) != next)
            refused.insert({origin, next});
    }
    return false;
}

void recall_checks(const char *directory)
{
    auto party = std::make_shared<CampaignParty>(module());
    // Each Cleric has one level-6 slot: one prepares the sanctuary, the other
    // recalls the party to it.
    const auto id = party->add_pc(cleric());
    const auto other = party->add_pc(cleric("Healer"));
    auto town = RolfTourSession::load(directory);
    town.campaign_party(party);
    settle(town);
    bool refused = false;
    try
    {
        town.recall(id);
    }
    catch (const std::exception &)
    {
        refused = true;
    }
    check(refused && !town.has_sanctuary(), "Without a sanctuary Word of Recall is refused");
    // The temple's door: a square by its landmark wall, (11, 2)'s south side,
    // where the service opens.
    unsigned temple_x = 16, temple_y = 16;
    for (const auto &[x, y] : std::array<std::pair<unsigned, unsigned>, 5>{
             {{11, 2}, {11, 3}, {10, 2}, {12, 2}, {11, 1}}})
    {
        bool temple = false;
        if (temple_x == 16 && walk_to(town, x, y, &temple) && temple)
        {
            temple_x = x;
            temple_y = y;
        }
    }
    check(temple_x < 16, "The party finds the temple");
    const auto slot_before = party->member(id).vitals.resources;
    town.recall(id);
    check(town.has_sanctuary() && party->member(id).vitals.resources != slot_before,
          "Cast on the temple's square, Word of Recall prepares the sanctuary");
    settle(town);
    check(walk_to(town, 5, 2), "The party walks to the training schools");
    check(town.snapshot().pose.x != temple_x || town.snapshot().pose.y != temple_y,
          "The party has left the temple");
    town.recall(other);
    settle(town);
    check(town.snapshot().pose.x == temple_x && town.snapshot().pose.y == temple_y,
          "Word of Recall brings the party back to its sanctuary");
}
} // namespace

int main()
{
    try
    {
        if (const auto directory = std::getenv("OPENGOLD_GAME_DIR"))
            recall_checks(directory);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Word of Recall checks passed\n";
}
