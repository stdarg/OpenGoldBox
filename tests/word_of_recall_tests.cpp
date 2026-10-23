#include "opengold/campaign_party.h"
#include "opengold/coin_purse.h"
#include "opengold/character.h"
#include "opengold/npc_portraits.h"
#include "opengold/rolf_tour.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>

using namespace opengold;
using namespace opengold::por;

// Word of Recall in New Phlan (LEVELS-3): cast on the temple's square it
// prepares the party's sanctuary; cast elsewhere it brings the party back.
// Also the temple's Heal list there (TEMPLE-1). Needs the installed game
// (OPENGOLD_GAME_DIR); without it the test passes.
namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const std::string &message)
{
    if (!ok)
    {
        throw std::runtime_error(message);
    }
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
    {
        r = {{6, 5, 4, 1}, 3};
    }
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
            {
                if (spells.size() < choice.spells.size() &&
                    std::find(spells.begin(), spells.end(), spell) == spells.end())
                {
                    spells.push_back(spell);
                }
            }
            choice.spells = spells;
        }
        check(hero.advance(*rules, life, choice), "An ordinary level-up");
    }
    return hero;
}

// The temple's Heal list: its services with their prices, and Leave.
bool temple_list(const TourSnapshot &s)
{
    return std::find(s.choices.begin(), s.choices.end(), "Leave") != s.choices.end() &&
           std::any_of(s.choices.begin(), s.choices.end(),
                       [](const auto &c)
                       {
                           return c.ends_with(" gp)");
                       });
}

// Runs the town until it waits on the player. At a temple's Heal list,
// `visit` acts and must leave; without it the party leaves at once.
void settle(RolfTourSession &town, bool *temple = nullptr,
            const std::function<void(RolfTourSession &)> &visit = {})
{
    for (unsigned n = 0; n < 2000; ++n)
    {
        const auto s = town.snapshot();
        if (s.phase == TourPhase::completed)
        {
            return;
        }
        check(s.phase != TourPhase::faulted, s.diagnostic);
        if (s.phase == TourPhase::awaiting_continue)
        {
            // The temple's Heal list: Leave is its last choice.
            const bool service = temple_list(s);
            if (temple && service)
            {
                *temple = true;
            }
            if (service && visit)
            {
                visit(town);
                continue;
            }
            // At a temple, ask for its service: "DO YOU SEEK HEALING?" YES.
            const bool asked = temple && s.dialogue.find("SEEK HEALING") != std::string::npos;
            check(!asked || town.temples().empty() ||
                      !opengold::temple_priest_portrait(s, town.temples()).empty(),
                  "The temple's priest greets the party");
            town.choose(s.continue_ticket, asked || s.choices.empty() ? 0 : s.choices.size() - 1);
        }
        else if (s.phase == TourPhase::awaiting_input)
        {
            town.input(s.continue_ticket, "0");
        }
        else if (s.phase == TourPhase::shopping)
        {
            town.leave_shop(s.continue_ticket);
        }
        else
        {
            town.advance(.3);
        }
    }
    throw std::runtime_error("A town event did not settle");
}

// Walks to a square of the town map; true when the party stands on it.
bool walk_to(RolfTourSession &town, unsigned tx, unsigned ty, bool *temple = nullptr,
             const std::function<void(RolfTourSession &)> &visit = {})
{
    constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
    std::set<std::pair<int, int>> refused;
    for (unsigned step = 0; step < 400; ++step)
    {
        const auto p = town.snapshot().pose;
        if (p.x == tx && p.y == ty)
        {
            return true;
        }
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
                {
                    continue;
                }
                const auto &a = town.map().at(cell % 16, cell / 16);
                const auto &b = town.map().at(x, y);
                const unsigned reverse = (d + 2) % 4;
                if (a.doors[d] > 1 || b.doors[reverse] > 1 || (a.walls[d] && !a.doors[d]) ||
                    (b.walls[reverse] && !b.doors[reverse]))
                {
                    continue;
                }
                const int next = y * 16 + x;
                if (previous[static_cast<std::size_t>(next)] >= 0 || refused.contains({cell, next}))
                {
                    continue;
                }
                previous[static_cast<std::size_t>(next)] = cell;
                cells.push(next);
            }
        }
        int next = static_cast<int>(ty * 16 + tx);
        if (previous[static_cast<std::size_t>(next)] < 0)
        {
            return false;
        }
        while (previous[static_cast<std::size_t>(next)] != origin)
        {
            next = previous[static_cast<std::size_t>(next)];
        }
        const unsigned facing = next % 16 > int(p.x)   ? 1
                                : next % 16 < int(p.x) ? 3
                                : next / 16 > int(p.y) ? 2
                                                       : 0;
        const bool forward = index(p.facing) == facing;
        town.explore(forward ? ExplorationCommand::forward : ExplorationCommand::turn_right);
        settle(town, temple, visit);
        const auto now = town.snapshot().pose;
        if (forward && static_cast<int>(now.y * 16 + now.x) != next)
        {
            refused.insert({origin, next});
        }
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

// Picks the choice that starts with `label`; false when there is none.
bool pick(RolfTourSession &town, std::string_view label)
{
    const auto s = town.snapshot();
    for (std::size_t n = 0; n < s.choices.size(); ++n)
    {
        if (s.choices[n].starts_with(label))
        {
            return town.choose(s.continue_ticket, n);
        }
    }
    return false;
}

bool offers(const RolfTourSession &town, std::string_view label)
{
    const auto &choices = town.snapshot().choices;
    return std::any_of(choices.begin(), choices.end(),
                       [&](const auto &c)
                       {
                           return c.starts_with(label);
                       });
}

// The Temple of Tyr's Heal list (TEMPLE-1): its services at the prices
// data/shops gives it, the members a service would help, a confirmed price,
// and Resurrection only on Easy and Normal.
void temple_list_checks(const char *directory)
{
    auto party = std::make_shared<CampaignParty>(module());
    party->add_pc(cleric());
    const auto patient = party->add_pc(cleric("Patient"));
    auto state = party->checkpoint();
    for (auto &m : state.roster)
    {
        if (m.id == patient)
        {
            m.vitals.hit_points = 1;
        }
    }
    coins(state.wealth, Coin::gold) = 150;
    party->restore(std::move(state));
    auto town = RolfTourSession::load(directory);
    town.campaign_party(party);
    // Tyr's squares, with its Cure Light Wounds at 120 gp and no Cure Disease.
    std::vector<TempleOffering> offerings = original_temple_offerings();
    std::erase_if(offerings,
                  [](const auto &o)
                  {
                      return o.service == rules::TempleService::cure_disease;
                  });
    for (auto &o : offerings)
    {
        if (o.service == rules::TempleService::cure_light_wounds)
        {
            o.price_gp = 120;
        }
    }
    town.temples(
        {{"GEO3.DAX", 0, {{11, 3}, {8, 4}, {8, 6}}, offerings, "NPCs/bishop-braccio.png"}});
    settle(town);
    bool visited = false;
    const auto visit = [&](RolfTourSession &t)
    {
        visited = true;
        check(opengold::speaking_npc_portrait(t.snapshot(), t.shops(), t.temples()) ==
                  "NPCs/bishop-braccio.png",
              "Bishop Braccio presides over Tyr's Heal list");
        check(offers(t, "Cure Light Wounds (120 gp)") && !offers(t, "Cure Disease") &&
                  offers(t, "Resurrection (11,000 gp)") && offers(t, "Raise Dead (5,500 gp)"),
              "Tyr lists its own prices; Resurrection on Normal");
        // Nobody is blind: the list says so and stays.
        check(pick(t, "Cure Blindness") &&
                  t.snapshot().dialogue == "No one in the party needs Cure Blindness." &&
                  temple_list(t.snapshot()),
              "A service nobody needs is refused");
        check(pick(t, "Cure Light Wounds") && offers(t, "Patient") && !offers(t, "Cleric") &&
                  offers(t, "Back"),
              "Only the wounded member is listed");
        check(pick(t, "Patient") &&
                  t.snapshot().dialogue ==
                      "The clerics will cast Cure Light Wounds on Patient for 120 gp." &&
                  offers(t, "Pay"),
              "The price is confirmed first");
        check(pick(t, "Back") && offers(t, "Patient") && pick(t, "Patient"), "Back, and again");
        check(pick(t, "Pay") &&
                  t.snapshot().dialogue == "The clerics cast Cure Light Wounds on Patient." &&
                  temple_list(t.snapshot()),
              "Paying casts the spell and returns to the list");
        const auto &healed = party->member(patient);
        check(coins(party->state().wealth, Coin::gold) == 30 && healed.vitals.hit_points > 1,
              "The party pays 120 gp and the patient is healed");
        // 30 gp cannot pay for another.
        if (healed.vitals.hit_points < party->hit_point_maximum(patient))
        {
            check(pick(t, "Cure Light Wounds") && pick(t, "Patient") &&
                      t.snapshot().dialogue == "The party's gold cannot pay 120 gp.",
                  "An unaffordable service is refused");
            check(pick(t, "Back"), "Back to the list");
        }
        check(pick(t, "Leave") && !temple_list(t.snapshot()), "Leave lets the script go on");
    };
    for (const auto &[x, y] : std::array<std::pair<unsigned, unsigned>, 5>{
             {{11, 2}, {11, 3}, {10, 2}, {12, 2}, {11, 1}}})
    {
        if (!visited)
        {
            bool at_temple = false;
            walk_to(town, x, y, &at_temple, visit);
        }
    }
    check(visited, "The party reaches the Temple of Tyr's Heal list");
    // On Hard the temple casts no Resurrection.
    auto hard = RolfTourSession::load(directory);
    hard.campaign_party(party);
    hard.difficulty(Difficulty::hard);
    settle(hard);
    bool hard_visited = false;
    const auto hard_visit = [&](RolfTourSession &t)
    {
        hard_visited = true;
        check(!offers(t, "Resurrection") && offers(t, "Cure Light Wounds (100 gp)"),
              "No Resurrection on Hard; a temple the data does not name charges the original's "
              "prices");
        check(pick(t, "Leave"), "Leave");
    };
    for (const auto &[x, y] : std::array<std::pair<unsigned, unsigned>, 5>{
             {{11, 2}, {11, 3}, {10, 2}, {12, 2}, {11, 1}}})
    {
        if (!hard_visited)
        {
            bool at_temple = false;
            walk_to(hard, x, y, &at_temple, hard_visit);
        }
    }
    check(hard_visited, "The party reaches the temple on Hard");
}

// Every New Phlan temple data/shops names (TEMPLE-1): each of its squares opens
// its own Heal list, with all ten services on Normal, and its own priest. Each temple here charges
// its own Cure Light Wounds price so the list shows whose it is.
void every_temple_checks(const char *directory)
{
    struct Site
    {
        std::string name;
        std::vector<std::pair<unsigned, unsigned>> markers;
        std::uint32_t cure_price;
    };

    const std::array<Site, 3> sites{{{"Tyr", {{11, 3}, {8, 4}, {8, 6}}, 101},
                                     {"Sune", {{1, 1}, {3, 1}, {1, 3}}, 102},
                                     {"Tempus", {{0, 12}, {2, 12}}, 103}}};
    std::vector<TempleSite> temples;
    for (const auto &site : sites)
    {
        auto offerings = original_temple_offerings();
        offerings.at(2).price_gp = site.cure_price;
        temples.push_back({"GEO3.DAX", 0, site.markers, offerings, "priest-" + site.name + ".png"});
    }
    auto party = std::make_shared<CampaignParty>(module());
    party->add_pc(cleric());
    auto town = RolfTourSession::load(directory);
    town.campaign_party(party);
    town.temples(temples);
    settle(town);
    std::map<std::pair<unsigned, unsigned>, std::string> lists;
    const auto visit = [&](RolfTourSession &t)
    {
        const auto &s = t.snapshot();
        std::string price = "none";
        for (const auto &site : sites)
        {
            if (offers(t, "Cure Light Wounds (" + std::to_string(site.cure_price) + " gp)"))
            {
                price = site.name;
            }
        }
        check(s.choices.size() == 11, "Every service is on the list, and Leave");
        check(opengold::speaking_npc_portrait(s, t.shops(), t.temples()) ==
                  "priest-" + price + ".png",
              "The temple's own priest presides over its Heal list");
        lists[{s.pose.x, s.pose.y}] = price;
        check(pick(t, "Leave"), "Leave");
    };
    for (const auto &site : sites)
    {
        for (const auto &[x, y] : site.markers)
        {
            bool at_temple = false;
            check(walk_to(town, x, y, &at_temple, visit), "The party reaches a temple's square");
        }
    }
    for (const auto &site : sites)
    {
        for (const auto &marker : site.markers)
        {
            const auto list = lists.find(marker);
            check(list != lists.end() && list->second == site.name,
                  "The Temple of " + site.name + " opens its own Heal list on (" +
                      std::to_string(marker.first) + ", " + std::to_string(marker.second) + ")");
        }
    }
}
} // namespace

int main()
{
    try
    {
        if (const auto directory = std::getenv("OPENGOLD_GAME_DIR"))
        {
            recall_checks(directory);
            temple_list_checks(directory);
            every_temple_checks(directory);
        }
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Word of Recall checks passed\n";
}
