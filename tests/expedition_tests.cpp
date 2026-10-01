#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character_creator.h"
#include "opengold/coin_purse.h"
#include "opengold/combat_demo.h"
#include "opengold/rolf_tour.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <queue>
#include <set>
#include <stdexcept>

using namespace opengold;

namespace
{
void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

template <class F> bool rejects(F f)
{
    try
    {
        f();
    }
    catch (const std::exception &)
    {
        return true;
    }
    return false;
}

std::unique_ptr<rules::RulesModule> module()
{
    return srd5::load(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "data/rules/srd-5.2.1/combat.rules");
}

// A player's fighter: best rolls to Strength, Constitution, then Dexterity.
Character character(std::string name, std::uint64_t seed = 42)
{
    CharacterCreator creator(srd5::character_rules(), seed);
    creator.select(rules::CreationField::race, "human");
    creator.select(rules::CreationField::character_class, "fighter");
    creator.roll();
    creator.name(std::move(name));
    std::array<unsigned, 6> best{0, 1, 2, 3, 4, 5};
    const auto &rolls = creator.draft().rolls;
    std::stable_sort(best.begin(), best.end(), [&](unsigned a, unsigned b)
    {
        return rolls[a].total() > rolls[b].total();
    });
    constexpr std::array<unsigned, 6> priority{0, 2, 1, 4, 5, 3};
    for (unsigned i = 0; i < 6; ++i)
        creator.assign_roll(best[i], priority[i]);
    return Character(creator.rules(), creator.draft(), creator.appearance());
}

void coin_purse_tests()
{
    const Purse gold_and_silver{0, 96, 0, 145, 0, 3, 1};
    const auto view = script_coins(gold_and_silver);
    check(view == Purse{15460, 1546, 309, 154, 15, 3, 1},
          "Scripts see what the whole coin purse affords in each denomination");
    const auto unchanged = settle_script_coins(gold_and_silver, view);
    check(unchanged.purse == gold_and_silver && !unchanged.exchange,
          "A script that only reads coins leaves the purse alone");

    auto inn = view;
    --inn[4];
    const auto paid = settle_script_coins(gold_and_silver, inn);
    check(paid.purse == Purse{0, 96, 0, 135, 0, 3, 1} && paid.exchange &&
          paid.exchange->owed == Coins{0, 0, 0, 0, 1} &&
          paid.exchange->paid == Coins{0, 0, 0, 10, 0} && paid.exchange->change == Coins{},
          "One platinum is paid with ten gold");

    const Purse platinum{0, 0, 0, 5, 1, 0, 0};
    auto exact = script_coins(platinum);
    --exact[4];
    const auto from_platinum = settle_script_coins(platinum, exact);
    check(from_platinum.purse == Purse{0, 0, 0, 5, 0, 0, 0} && !from_platinum.exchange,
          "A purse holding the asked-for coin pays it directly");

    const Purse one_platinum{0, 0, 0, 0, 1, 0, 0};
    auto five_gold = script_coins(one_platinum);
    five_gold[3] -= 5;
    const auto broken = settle_script_coins(one_platinum, five_gold);
    check(broken.purse == Purse{0, 0, 0, 5, 0, 0, 0} && broken.exchange &&
          broken.exchange->paid == Coins{0, 0, 0, 0, 1} &&
          broken.exchange->change == Coins{0, 0, 0, 5, 0},
          "A larger coin is broken and the change returned in gold");

    const Purse one_gold{0, 0, 0, 1, 0, 0, 0};
    auto three_silver = script_coins(one_gold);
    three_silver[1] -= 3;
    check(settle_script_coins(one_gold, three_silver).purse == Purse{0, 7, 0, 0, 0, 0, 0},
          "Change below a gold piece comes back in silver");

    auto treasure = view;
    treasure[1] += 50;
    check(settle_script_coins(gold_and_silver, treasure).purse == Purse{0, 146, 0, 145, 0, 3, 1},
          "Coins a script adds are added as given");

    auto gem = view;
    --gem[5];
    check(settle_script_coins(gold_and_silver, gem).purse == Purse{0, 96, 0, 145, 0, 2, 1},
          "Gems are taken as gems, never converted to coins");

    auto everything = view;
    std::fill(everything.begin(), everything.begin() + 5, 0);
    check(rejects([&]
    {
        (void)settle_script_coins(gold_and_silver, everything);
    }),
    "A script cannot take more than the purse is worth");
}

std::shared_ptr<const por::EclProgram> program(std::vector<std::uint8_t> body)
{
    std::vector<std::uint8_t> bytes{0, 0};
    for (int n = 0; n < 5; ++n)
        bytes.insert(bytes.end(), {1, 1, 0x15, 0x99});
    bytes.push_back(0);
    bytes.insert(bytes.end(), body.begin(), body.end());
    return std::make_shared<const por::EclProgram>(
               por::EclProgram::decode(bytes, "expedition payment"));
}

void settle_synthetic(por::RolfTourSession &town)
{
    for (unsigned n = 0; n < 100 && town.snapshot().phase == por::TourPhase::running; ++n)
        town.advance(.5);
    check(town.snapshot().phase == por::TourPhase::completed, "Synthetic event completes");
}

// The script pays one platinum (SAVE 14 into the shown 15), stores the character
// (one read), then ends the event (a second read). Only the first may charge.
void script_payment_tests()
{
    auto party = std::make_shared<CampaignParty>(module());
    const auto payer = party->add_pc(character("Arden"));
    party->set_wealth(payer, {0, 96, 0, 145, 0, 0, 0});
    auto resources = std::make_shared<por::PhlanResources>();
    auto payment = program({9, 0, 14, 1, 0xC3, 0x6B, 10, 0, 128, 0});
    por::RolfTourSession town({}, payment, {}, 0x9914, {}, resources);
    town.campaign_party(party);
    settle_synthetic(town);
    check(town.script_variable(0x6BC3) == 15, "The script sees fifteen affordable platinum");
    check(town.explore(por::ExplorationCommand::look), "Payment event starts");
    settle_synthetic(town);
    check(party->member(payer).wealth == Purse{0, 96, 0, 135, 0, 0, 0},
          "The payment is charged exactly once across both script reads");
    check(town.snapshot().payments.size() == 1 && town.snapshot().payments[0].payer == "Arden" &&
          town.snapshot().payments[0].coins.paid == Coins{0, 0, 0, 10, 0},
          "The exchange is reported for the payer");
    check(town.script_variable(0x6BC3) == 14 && town.script_variable(0x6BC1) == 144,
          "The script then sees the settled purse");

    auto reader = program({10, 0, 128, 0});
    por::RolfTourSession reading({}, reader, {}, 0x9914, {}, resources);
    reading.campaign_party(party);
    settle_synthetic(reading);
    reading.explore(por::ExplorationCommand::look);
    settle_synthetic(reading);
    check(party->member(payer).wealth == Purse{0, 96, 0, 135, 0, 0, 0} &&
          reading.snapshot().payments.empty(),
          "Events that only read coins never change the purse");
}

// Runs a synthetic event until it waits or ends, acknowledging a rollback
// notice. Returns true when the event failed and was rolled back.
bool settle_or_rollback(por::RolfTourSession &town, bool until_idle = true)
{
    for (unsigned n = 0; n < 100 && town.snapshot().phase == por::TourPhase::running; ++n)
        town.advance(.5);
    const auto &s = town.snapshot();
    const bool rolled_back = s.phase == por::TourPhase::awaiting_continue &&
                             s.dialogue.starts_with("This event is not supported yet");
    if (rolled_back)
        check(town.choose(s.continue_ticket, 0), "The rollback notice is acknowledged");
    if (until_idle)
        check(town.snapshot().phase == por::TourPhase::completed, "Synthetic event ends");
    return rolled_back;
}

// Runs one synthetic Look. False means the event failed and was rolled back.
bool look(por::RolfTourSession &town)
{
    check(town.explore(por::ExplorationCommand::look), "The event starts");
    return !settle_or_rollback(town);
}

std::vector<Purse> purses(const PartyState &state)
{
    std::vector<Purse> result;
    for (const auto &member : state.roster)
        result.push_back(member.wealth);
    return result;
}

unsigned claims_starting(const PartyState &state, std::string_view prefix)
{
    return unsigned(std::count_if(state.claimed_rewards.begin(), state.claimed_rewards.end(),
                                  [&](const auto & id)
    {
        return id.starts_with(prefix);
    }));
}

constexpr std::string_view synthetic_treasure = "por:ECL3:0:treasure:";

// The shape of Ohlo's hand-in: TREASURE 150 pp, 1 jewelry and one random item,
// COMBAT with no monsters, then the completion flag 0x4A04. Optionally guarded
// by that flag, and optionally failing afterwards while the second member is selected.
std::shared_ptr<const por::EclProgram> treasure_program(bool guarded, bool failing)
{
    std::vector<std::uint8_t> body;
    if (guarded)
        body.insert(body.end(), {3, 1, 0x04, 0x4A, 0, 255, 22, 0});
    body.insert(body.end(), {39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 150, 0, 0, 0, 1, 0, 129, 36});
    body.insert(body.end(), {9, 0, 255, 1, 0x04, 0x4A});
    if (failing)
        body.insert(body.end(), {3, 1, 0xB4, 0x6D, 0, 1, 22, 40, 0, 0, 0, 0, 0, 0});
    body.push_back(0);
    return program(body);
}

void script_treasure_tests()
{
    auto party = std::make_shared<CampaignParty>(module());
    party->add_pc(character("Arden"));
    party->add_pc(character("Bryn", 2));
    auto resources = std::make_shared<por::PhlanResources>();
    por::RolfTourSession town({}, treasure_program(true, true), {}, 0x9914, {}, resources);
    town.campaign_party(party);
    settle_synthetic(town);

    party->select(1);
    const auto before = party->checkpoint();
    check(!look(town) && town.script_diagnostics().size() == 1,
          "A failure after the award reports and rolls back the event");
    check(purses(party->checkpoint()) == purses(before) &&
          party->state().claimed_rewards == before.claimed_rewards &&
          town.script_variable(0x4A04) == 0,
          "The rollback restores purses, claims and the quest flag together");

    party->select(0);
    check(look(town), "The retried event completes");
    const auto paid = party->checkpoint();
    check(paid.roster[0].wealth == Purse{0, 0, 0, 0, 150, 0, 1} &&
          paid.roster[1].wealth == Purse{} && claims_starting(paid, synthetic_treasure) == 1 &&
          town.script_variable(0x4A04) == 255 && paid.roster[0].experience == 0,
          "The first living member receives 150 pp and the jewelry once, with no XP");
    check(town.snapshot().dialogue.find("Not awarded: 1 random original treasure item") !=
          std::string::npos,
          "The unsupported random item is reported, not silently dropped");

    check(look(town) && purses(party->checkpoint()) == purses(paid) &&
          party->state().claimed_rewards == paid.claimed_rewards,
          "The completed flag keeps a revisit from paying again");

    // Were a script to reach the same TREASURE again, it is refused, not paid twice.
    por::RolfTourSession repeat({}, treasure_program(false, false), {}, 0x9914, {}, resources);
    auto other = std::make_shared<CampaignParty>(module());
    other->add_pc(character("Cora", 3));
    repeat.campaign_party(other);
    settle_synthetic(repeat);
    check(look(repeat), "The unguarded treasure pays once");
    const auto once = other->checkpoint();
    check(!look(repeat) && purses(other->checkpoint()) == purses(once) &&
          other->state().claimed_rewards == once.claimed_rewards &&
          repeat.script_diagnostics().back().find("already awarded") != std::string::npos,
          "A repeated award is an explicit, rolled-back failure");
}

// Full purses defer the treasure without loss; a reload keeps it pending.
void deferred_treasure_tests()
{
    auto party = std::make_shared<CampaignParty>(module());
    const auto arden = party->add_pc(character("Arden"));
    party->set_wealth(arden, {0, 0, 0, 0, 65535 - 100, 0, 0});
    const auto script = treasure_program(true, false);
    auto resources = std::make_shared<por::PhlanResources>();
    resources->programs.emplace(0, script);
    por::RolfTourSession town({}, script, {}, 0x9914, {}, resources);
    town.campaign_party(party);
    settle_synthetic(town);

    check(look(town) && town.script_variable(0x4A04) == 255 &&
          party->member(arden).wealth == Purse{0, 0, 0, 0, 65535 - 100, 0, 0} &&
          claims_starting(party->checkpoint(), synthetic_treasure) == 0 &&
          town.snapshot().dialogue.find("Loot is retained") != std::string::npos,
          "A purse without room defers the whole award and the quest still completes");
    check(look(town) && party->member(arden).wealth == Purse{0, 0, 0, 0, 65535 - 100, 0, 0},
          "The award stays pending while there is no room");

    const auto rules = module();
    const auto bytes = encode_campaign(*party, &town, "treasure");
    auto loaded = decode_campaign(bytes, *srd5::character_rules(), *rules, "treasure", &town);
    auto reloaded = std::make_shared<CampaignParty>(module());
    reloaded->restore(std::move(loaded.party));
    auto restored = std::move(*loaded.town);
    restored.attach_restored_party(reloaded);
    check(encode_campaign(*reloaded, &restored, "treasure") == bytes,
          "The pending treasure survives a reload");

    reloaded->set_wealth(arden, {0, 0, 0, 0, 100, 0, 0});
    check(look(restored) && reloaded->member(arden).wealth == Purse{0, 0, 0, 0, 250, 0, 1} &&
          claims_starting(reloaded->checkpoint(), synthetic_treasure) == 1,
          "Once there is room the deferred treasure is collected exactly once");
    check(look(restored) && reloaded->member(arden).wealth == Purse{0, 0, 0, 0, 250, 0, 1},
          "Collected treasure is not collected again");
}

// One synthetic door: (0,0) east is locked (code 2); (1,0) west is an ordinary door.
por::GeoMap locked_door_map()
{
    por::GeoMap map;
    map.cells[0].walls[1] = 1;
    map.cells[0].doors[1] = 2;
    map.cells[1].walls[3] = 1;
    map.cells[1].doors[3] = 1;
    return map;
}

// Steps east and answers the Locked menu. True when the party got through or the
// destination's event failed (it is then rolled back).
bool try_door(por::RolfTourSession &town, std::size_t choice)
{
    check(town.explore(por::ExplorationCommand::forward), "The step starts");
    settle_or_rollback(town, false);
    const auto &s = town.snapshot();
    check(s.dialogue == "Locked." && s.choices == std::vector<std::string> {"Bash", "Exit"},
          "A locked door offers the original Bash and Exit");
    check(town.choose(s.continue_ticket, choice), "The door choice is accepted");
    const bool rolled_back = settle_or_rollback(town);
    return rolled_back || town.snapshot().pose.x == 1;
}

void locked_door_tests()
{
    auto party = std::make_shared<CampaignParty>(module());
    party->add_pc(character("Arden"));
    party->add_pc(character("Bryn", 2));
    auto resources = std::make_shared<por::PhlanResources>();
    // The destination's search fails, so an opened door must be rolled back too.
    const auto failing = program({3, 1, 0x4B, 0xC0, 0, 1, 22, 40, 0, 0, 0, 0, 0, 0, 0});
    por::RolfTourSession town(locked_door_map(), failing, {}, 0x9914, {}, resources);
    town.campaign_party(party);
    settle_synthetic(town);
    town.explore(por::ExplorationCommand::turn_right);

    const auto random = party->state().random_state;
    check(!try_door(town, 1) && town.snapshot().pose.x == 0 &&
          party->state().random_state == random && town.map().at(0, 0).doors[1] == 2,
          "Exit leaves the door locked and rolls nothing");

    unsigned tries = 0;
    while (!try_door(town, 0))
    {
        check(town.snapshot().dialogue.find("The door holds.") != std::string::npos &&
              town.map().at(0, 0).doors[1] == 2,
              "A failed Bash reports the checks and leaves the door locked");
        check(++tries < 40, "The party eventually forces the door");
    }
    check(town.snapshot().pose.x == 0 && town.map().at(0, 0).doors[1] == 2 &&
          town.script_diagnostics().size() == 1,
          "A failure beyond the door rolls back the move and relocks it");

    const auto passable = program({0});
    por::RolfTourSession open(locked_door_map(), passable, {}, 0x9914, {}, resources);
    open.campaign_party(party);
    settle_synthetic(open);
    open.explore(por::ExplorationCommand::turn_right);
    for (tries = 0; !try_door(open, 0); ++tries)
        check(tries < 40, "The party eventually forces the door");
    check(open.snapshot().pose.x == 1 && open.map().at(0, 0).doors[1] == 1 &&
          open.snapshot().dialogue.find("tries to force the door: Strength (Athletics)") !=
          std::string::npos &&
          open.snapshot().dialogue.find("The door bursts open.") != std::string::npos,
          "A successful Bash reports each check, unlocks the door and moves the party");
    open.explore(por::ExplorationCommand::turn_around);
    open.explore(por::ExplorationCommand::forward);
    settle_synthetic(open);
    check(open.snapshot().pose.x == 0, "The opened door stays open in both directions");
}

using Answer = std::function<std::size_t(const por::TourSnapshot &)>;

// Original peaceful answers. Roaming groups are fled from, as in the audited
// trip, so the four-orc paper event is the expedition's only fight.
std::size_t peaceful(const por::TourSnapshot &s)
{
    if (s.dialogue == "Choose a party member.")
        return 0;
    if (s.choices.size() == 5 && s.choices[0] == "Fight")
        return 2;
    for (const auto *safe :
            {"NO", "LEAVE", "RUN", "GO", "NONE", "EXIT"
            })
        for (std::size_t n = 0; n < s.choices.size(); ++n)
            if (s.choices[n] == safe)
                return n;
    return s.choices.size() - 1;
}

void fight(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party)
{
    if (town.monster_picture())
        check(town.start_encounter(), "The monster close-up is dismissed");
    check(town.pending_encounter().has_value(), "The original encounter reaches combat");
    CombatDemo combat(module());
    combat.campaign_party(party);
    combat.encounter(*town.pending_encounter(), 42);
    for (unsigned n = 0; n < 4000 && combat.combat().snapshot().outcome == rules::Outcome::ongoing;
            ++n)
        check(combat.submit(choose_demo_command(combat.combat())), "Combat accepts the command");
    const auto result = combat.combat().snapshot();
    check(result.outcome == rules::Outcome::victory, "The party wins its fight");
    check(town.resolve_combat(result), "Exploration accepts the victory");
}

// Plays the current event until exploration resumes or a shop opens.
void settle(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party,
            const Answer &answer = peaceful, std::string_view typed = "0")
{
    for (unsigned n = 0; n < 5000; ++n)
    {
        const auto &s = town.snapshot();
        switch (s.phase)
        {
        case por::TourPhase::completed:
        case por::TourPhase::shopping:
            check(town.script_diagnostics().empty(), "The route uses only supported events");
            return;
        case por::TourPhase::running:
            town.advance(.5);
            break;
        case por::TourPhase::awaiting_continue:
            check(town.choose(s.continue_ticket, answer(s)), "The script accepts the answer");
            break;
        case por::TourPhase::awaiting_input:
            check(town.input(s.continue_ticket, typed), "The script accepts the input");
            break;
        case por::TourPhase::combat:
            fight(town, party);
            break;
        default:
            throw std::runtime_error("Event failed: " + s.diagnostic);
        }
    }
    throw std::runtime_error("Event did not finish");
}

void face(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party,
          unsigned facing)
{
    while (town.snapshot().pose.facing != facing)
    {
        town.explore(por::ExplorationCommand::turn_right);
        settle(town, party);
    }
}

void step(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party, unsigned facing,
          const Answer &answer = peaceful, std::string_view typed = "0")
{
    face(town, party, facing);
    town.explore(por::ExplorationCommand::forward);
    settle(town, party, answer, typed);
}

// Breadth-first walk over open edges, through the original movement events.
// With bash_doors, locked doors are part of the route and are retried until forced.
void walk_to(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party, unsigned tx,
             unsigned ty, const Answer &answer = peaceful, bool bash_doors = false)
{
    const auto locked = [&](unsigned wall, unsigned door)
    {
        return bash_doors && wall && door == 2;
    };
    constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
    std::set<std::pair<int, int>> refused;
    for (unsigned moves = 0; moves < 400; ++moves)
    {
        const auto p = town.snapshot().pose;
        if (p.x == tx && p.y == ty)
            return;
        std::array<int, 256> previous;
        previous.fill(-1);
        std::queue<int> cells;
        const int origin = p.y * 16 + p.x;
        cells.push(origin);
        previous[origin] = origin;
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
                const auto blocked = [&](unsigned wall, unsigned door)
                {
                    return !locked(wall, door) && (door > 1 || (wall && !door));
                };
                if (blocked(a.walls[d], a.doors[d]) || blocked(b.walls[reverse], b.doors[reverse]))
                    continue;
                const int next = y * 16 + x;
                if (previous[next] >= 0 || refused.contains({cell, next}))
                    continue;
                previous[next] = cell;
                cells.push(next);
            }
        }
        int next = int(ty * 16 + tx);
        check(previous[next] >= 0, "The destination is reachable");
        while (previous[next] != origin)
            next = previous[next];
        const unsigned facing = next % 16 > int(p.x)   ? 1
                                : next % 16 < int(p.x) ? 3
                                : next / 16 > int(p.y) ? 2
                                : 0;
        step(town, party, facing, answer);
        const auto now = town.snapshot().pose;
        const int reached = int(now.y * 16 + now.x);
        const auto &edge = town.map().at(p.x, p.y);
        const auto &far = town.map().at(next % 16, next / 16);
        const bool door_held = locked(edge.walls[facing], edge.doors[facing]) ||
                               locked(far.walls[(facing + 2) % 4], far.doors[(facing + 2) % 4]);
        // A held door is bashed again. Fleeing a roaming group relocates the
        // party; quest walks then replan rather than giving up on the edge.
        const bool fled = bash_doors && reached != origin;
        if (reached != next && !door_held && !fled)
            refused.insert({origin, next});
    }
    throw std::runtime_error("Walk did not arrive");
}

struct Expedition
{
    std::shared_ptr<CampaignParty> party;
    por::RolfTourSession town;
};

// Six created fighters with the new-character purse, after Rolf's tour.
Expedition create_party(const std::filesystem::path &directory)
{
    auto party = std::make_shared<CampaignParty>(module());
    std::uint64_t seed = 1;
    for (const auto *name :
            {"Arden", "Bryn", "Cora", "Darin", "Elin", "Fenn"
            })
        party->set_wealth(party->add_pc(character(name, seed++)), {0, 0, 0, 250, 0, 0, 0});
    auto town = por::RolfTourSession::load(directory);
    town.campaign_party(party);
    settle(town, party);
    return {party, std::move(town)};
}

void buy_and_equip(Expedition &trip)
{
    auto &[party, town] = trip;
    walk_to(town, party, 13, 8);
    town.explore(por::ExplorationCommand::look);
    settle(town, party, [](const por::TourSnapshot & s)
    {
        return s.dialogue.find("SHOP") != std::string::npos ? std::size_t{0} : peaceful(s);
    });
    check(town.snapshot().phase == por::TourPhase::shopping, "The arms shop opens");
    const auto &stock = town.shop_stock();
    for (unsigned slot = 0; slot < 6; ++slot)
    {
        party->select(slot);
        // Original long sword, chain mail and shield records.
        for (const unsigned type : {36u, 55u, 59u})
        {
            const auto offered = std::find_if(stock.begin(), stock.end(), [&](const auto & item)
            {
                return item.stored.type == type;
            });
            check(offered != stock.end(), "The arms shop stocks fighter gear");
            check(town.buy(town.snapshot().continue_ticket, std::size_t(offered - stock.begin())),
                  "The party buys its gear");
        }
    }
    party->select(0);
    check(town.leave_shop(town.snapshot().continue_ticket), "The party leaves the shop");
    settle(town, party);
    for (const auto id : party->state().slots)
        if (id)
        {
            for (const auto &item : party->member(id).character.inventory().items())
                party->equip(id, item.id);
            check(party->member(id).equipped.size() == 3 &&
                  party->member(id).wealth == Purse{0, 0, 0, 250 - 15 - 75 - 15, 0, 0, 0},
                  "Each fighter pays in gold and equips sword, mail and shield");
        }
}

unsigned orc_rewards(const CampaignParty &party)
{
    const auto &claimed = party.state().claimed_rewards;
    return unsigned(std::count(claimed.begin(), claimed.end(), "por:ECL2:20:search1:orcs:v1") +
                    std::count(claimed.begin(), claimed.end(), "por:ECL2:20:search1:orcs:v1:loot"));
}

void defeat_four_orcs(Expedition &trip)
{
    auto &[party, town] = trip;
    walk_to(town, party, 0, 4);
    step(town, party, 3);
    check(town.snapshot().area_id == 20, "The west gate leads into the Slums");
    // The route passes (13,1), where the orcs argue over their papers.
    walk_to(town, party, 12, 1);
    check(town.script_variable(0x4ACA) == 255 && orc_rewards(*party) == 2,
          "The four-orc victory sets its original flag and claims XP and loot once");
    unsigned silver = 0;
    for (const auto &member : party->state().roster)
    {
        check(member.experience == 300, "Every fighter earns the victory XP");
        silver += member.wealth[1];
    }
    check(silver == 96, "The orcs' original silver is collected");
    walk_to(town, party, 15, 4);
    step(town, party, 1);
    check(town.snapshot().area_id == 0 && town.snapshot().pose.x == 0 &&
          town.snapshot().pose.y == 4,
          "The east edge of the Slums returns to New Phlan");
}

// Accepts the inn's quote ("YES") or declines it, and names the payer.
Answer inn_answer(bool stay, std::string payer)
{
    return [stay, payer](const por::TourSnapshot & s)
    {
        for (std::size_t n = 0; n < s.choices.size(); ++n)
            if (s.choices[n] == (stay ? "YES" : "NO") || s.choices[n] == payer)
                return n;
        return peaceful(s);
    };
}

std::vector<Purse> purses(const CampaignParty &party)
{
    std::vector<Purse> result;
    for (const auto &member : party.state().roster)
        result.push_back(member.wealth);
    return result;
}

// The original inn (ECL3:0, south of (4,11)) asks the chosen character for one platinum.
void rest_at_inn(Expedition &trip)
{
    auto &[party, town] = trip;
    walk_to(town, party, 4, 11);
    const auto leave_inn = [&]
    {
        if (town.snapshot().pose.y == 12)
            step(town, party, 0);
    };

    auto before = party->checkpoint();
    const auto start = purses(*party);
    step(town, party, 2, inn_answer(false, ""));
    check(town.snapshot().dialogue == "THEN YOU MUST LEAVE." && purses(*party) == start &&
          party->state().roster[0].vitals == before.roster[0].vitals &&
          party->state().time_minutes - before.time_minutes < 1,
          "Declining the quote costs nothing and grants no rest");
    leave_inn();

    // A payer worth less than one platinum (9 gp 9 sp) is refused by the script.
    party->set_wealth(party->state().roster.at(2).id, {0, 9, 0, 9, 0, 0, 0});
    before = party->checkpoint();
    const auto unpaid = purses(*party);
    step(town, party, 2, inn_answer(true, "Cora"));
    check(town.snapshot().dialogue == "YOU DON'T HAVE ENOUGH PLATINUM." &&
          purses(*party) == unpaid && town.snapshot().payments.empty() &&
          party->state().time_minutes - before.time_minutes < 1,
          "An unaffordable stay is refused with every purse unchanged");
    leave_inn();

    before = party->checkpoint();
    step(town, party, 2, inn_answer(true, "Arden"));
    auto expected = unpaid;
    expected[0][3] -= 10;
    check(purses(*party) == expected, "Arden pays exactly ten gold for the platinum, once");
    const auto &payments = town.snapshot().payments;
    check(payments.size() == 1 && payments[0].payer == "Arden" &&
          payments[0].coins.owed == Coins{0, 0, 0, 0, 1} &&
          payments[0].coins.paid == Coins{0, 0, 0, 10, 0} && payments[0].coins.change == Coins{},
          "The exchange is reported to the player");
    check(party->state().time_minutes >= before.time_minutes + 480,
          "The long rest advances the clock eight hours");
    for (const auto &member : party->state().roster)
        check(member.vitals.hit_points == member.character.sheet().hit_points &&
              member.last_rest_minutes,
              "The long rest restores every fighter");
    leave_inn();
}

// Walks back to the completed four-orc event and looks: nothing may repeat.
void revisit_orcs(Expedition &trip)
{
    auto &[party, town] = trip;
    walk_to(town, party, 0, 4);
    step(town, party, 3);
    walk_to(town, party, 12, 1);
    // Roaming groups met on the way may be fought; the revisit itself pays nothing.
    const auto before = party->checkpoint();
    town.explore(por::ExplorationCommand::look);
    settle(town, party);
    check(town.script_variable(0x4ACA) == 255 && orc_rewards(*party) == 2 &&
          party->state().claimed_rewards == before.claimed_rewards,
          "The completed event does not fight or reward again");
    for (std::size_t n = 0; n < before.roster.size(); ++n)
        check(party->state().roster[n].experience == before.roster[n].experience &&
              party->state().roster[n].wealth == before.roster[n].wealth,
              "Revisiting keeps XP and purses");
}

constexpr std::string_view ohlo_reward = "por:ECL2:20:treasure:a390:v1";

// Ohlo's commission: bash locked doors, TALK and parley NICE, accept, SPEAK at
// the booth, GIVE the potion. Roaming groups are still fled from.
std::size_t quest_answer(const por::TourSnapshot &s)
{
    for (const auto *wanted :
            {"Bash", "TALK", "NICE", "ACCEPT THE COMMISSION", "SPEAK", "GIVE"
            })
        for (std::size_t n = 0; n < s.choices.size(); ++n)
            if (s.choices[n] == wanted)
                return n;
    return peaceful(s);
}

// Steps west from (14,10) into Ohlo's room until the forced door lets the party
// in. Returns the party as it was just before the step that reached Ohlo.
PartyState enter_ohlo_room(Expedition &trip, std::uint16_t until_commission)
{
    auto &[party, town] = trip;
    walk_to(town, party, 14, 10, quest_answer, true);
    auto before = party->checkpoint();
    for (unsigned tries = 0; town.script_variable(0x4A04) != until_commission; ++tries)
    {
        check(tries < 40, "The party forces Ohlo's door");
        before = party->checkpoint();
        step(town, party, 3, quest_answer);
    }
    const auto pose = town.snapshot().pose;
    check(pose.x == 14 && pose.y == 10 && pose.facing == 1,
          "Ohlo's script returns the party outside his door");
    return before;
}

unsigned ohlo_rewards(const CampaignParty &party)
{
    const auto &claimed = party.state().claimed_rewards;
    return unsigned(std::count(claimed.begin(), claimed.end(), ohlo_reward));
}

// From New Phlan: accept Ohlo's commission, fetch the potion from the Old Rope
// Guild booth, hand it in for the reward, then return to New Phlan.
void ohlo_quest(Expedition &trip)
{
    auto &[party, town] = trip;
    // As in the audit, the party first spends the four orcs' 300 XP on level two.
    for (const auto id : party->state().slots)
        if (id)
        {
            check(party->can_advance(id), "The orcs' experience earns level two");
            party->advance(id, party->default_advancement(id));
        }
    walk_to(town, party, 0, 4);
    step(town, party, 3);
    check(town.snapshot().area_id == 20, "The west gate leads into the Slums");

    const auto accepting = enter_ohlo_room(trip, 250);
    check(town.script_variable(0x4A81) == 0 &&
          purses(*party) == purses(accepting) &&
          party->state().claimed_rewards == accepting.claimed_rewards,
          "Accepting the commission sets its flag and pays nothing");

    walk_to(town, party, 14, 12, quest_answer, true);
    step(town, party, 1, quest_answer, "ohlo");
    check(town.script_variable(0x4A81) == 250 &&
          town.snapshot().dialogue.find("RETURNS WITH A PACKAGE") != std::string::npos,
          "Speaking Ohlo's name at the booth obtains the potion");

    const auto before = enter_ohlo_room(trip, 255);
    check(town.script_variable(0x4A81) == 255 && ohlo_rewards(*party) == 1,
          "Handing in the potion completes the quest and claims the reward once");
    const auto after = party->checkpoint();
    auto claims = before.claimed_rewards;
    claims.emplace_back(ohlo_reward);
    check(after.claimed_rewards == claims, "The hand-in claims only Ohlo's reward");
    check(after.roster[0].wealth[4] == before.roster[0].wealth[4] + 150 &&
          after.roster[0].wealth[6] == before.roster[0].wealth[6] + 1,
          "The first living member receives Ohlo's 150 pp and jewelry");
    for (std::size_t n = 0; n < after.roster.size(); ++n)
    {
        check(after.roster[n].experience == before.roster[n].experience,
              "The original hand-in grants no experience");
        if (n)
            check(after.roster[n].wealth == before.roster[n].wealth,
                  "Only the first living member is paid");
    }
    walk_to(town, party, 15, 4);
    step(town, party, 1);
    check(town.snapshot().area_id == 0, "The party returns to New Phlan");
}

// After completion, Ohlo's room and the booth are quiet and pay nothing again.
void revisit_ohlo(Expedition &trip)
{
    auto &[party, town] = trip;
    if (town.snapshot().area_id == 0)
    {
        walk_to(town, party, 0, 4);
        step(town, party, 3);
    }
    // Each quiet visit is checked on its own, apart from roaming fights on the way.
    const auto visit = [&](unsigned x, unsigned y, unsigned facing)
    {
        walk_to(town, party, x, y, quest_answer, true);
        const auto before = party->checkpoint();
        step(town, party, facing, quest_answer, "OHLO");
        const bool arrived = town.snapshot().pose.x == x + (facing == 1 ? 1 : -1);
        check(!arrived || (purses(*party) == purses(before) &&
                           party->state().claimed_rewards == before.claimed_rewards),
              "A completed quest location pays nothing again");
    };
    for (unsigned tries = 0; town.snapshot().pose.x != 13; ++tries)
    {
        check(tries < 40, "The party forces Ohlo's door again");
        visit(14, 10, 3);
    }
    step(town, party, 1);
    visit(14, 12, 1);
    // 0x4A04 is area-local (cleared on entering the Slums); 0x4A81 records completion.
    check(town.snapshot().pose.x == 15 && town.script_variable(0x4A81) == 255 &&
          ohlo_rewards(*party) == 1,
          "Revisiting Ohlo and the booth keeps the quest complete without a second reward");
}

// Ohlo's route crosses much of the Slums, where unavoidable roaming fights are
// currently lost by this automated party (docs/QUESTS.md). Run it on request.
bool ohlo_route_requested()
{
    const auto *flag = std::getenv("OPENGOLD_OHLO_ROUTE");
    return flag && std::string_view(flag) == "1";
}

std::filesystem::path revisit_result(const std::filesystem::path &save)
{
    return std::filesystem::path(save).concat(".revisit");
}

// Runs in a fresh process: load the save, revisit the orcs and Ohlo, record the result.
void reload_and_revisit(const std::filesystem::path &save, const std::filesystem::path &directory)
{
    const auto assets = campaign_asset_identity(directory);
    const auto bytes = read_campaign_file(save);
    const auto rules = module();
    const auto base = por::RolfTourSession::load(directory);
    auto loaded = decode_campaign(bytes, *srd5::character_rules(), *rules, assets, &base);
    auto party = std::make_shared<CampaignParty>(module());
    party->restore(std::move(loaded.party));
    Expedition trip{party, std::move(*loaded.town)};
    trip.town.attach_restored_party(party);
    check(encode_campaign(*party, &trip.town, assets) == bytes,
          "The reloaded campaign is the saved campaign");
    check(!party->rest(), "The inn's rest timer survives the reload");
    revisit_orcs(trip);
    if (ohlo_route_requested())
        revisit_ohlo(trip);
    write_campaign_file(revisit_result(save), encode_campaign(*party, &trip.town, assets));
}

void installed_first_expedition(const std::filesystem::path &executable,
                                const std::filesystem::path &directory)
{
    const auto assets = campaign_asset_identity(directory);
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto folder =
        std::filesystem::temp_directory_path() / ("opengold-expedition-" + std::to_string(stamp));
    std::filesystem::create_directories(folder);
    const auto save = folder / "expedition.ogs";

    auto trip = create_party(directory);
    buy_and_equip(trip);
    defeat_four_orcs(trip);
    const auto after_return = encode_campaign(*trip.party, &trip.town, assets);
    write_campaign_file(save, after_return);
    rest_at_inn(trip);
    write_campaign_file(save, encode_campaign(*trip.party, &trip.town, assets));
    check(read_campaign_file(std::filesystem::path(save).concat(".bak")) == after_return,
          "Replacing a save keeps the previous one as a backup");
    if (ohlo_route_requested())
    {
        ohlo_quest(trip);
        write_campaign_file(save, encode_campaign(*trip.party, &trip.town, assets));
    }

    auto command = "\"" + executable.string() + "\" --revisit \"" + save.string() + "\"";
#ifdef _WIN32
    command = "\"" + command + "\""; // cmd.exe strips one outer pair of quotes.
#endif
    check(std::system(command.c_str()) == 0, "A fresh process reloads and revisits the event");
    revisit_orcs(trip);
    if (ohlo_route_requested())
        revisit_ohlo(trip);
    check(read_campaign_file(revisit_result(save)) ==
          encode_campaign(*trip.party, &trip.town, assets),
          "Continuing after a reload matches continuing without one");
    std::filesystem::remove_all(folder);
    std::cout << "Installed first expedition: created, equipped, defeated the four orcs, "
              "returned, paid the inn with change, rested, reloaded and revisited.\n";
    if (ohlo_route_requested())
        std::cout << "Installed Ohlo route: commission, booth, reward, reload and revisit.\n";
}

} // namespace

int main(int argc, char **argv)
{
    try
    {
        const auto *directory = std::getenv("OPENGOLD_GAME_DIR");
        if (argc == 3 && std::string_view(argv[1]) == "--revisit")
        {
            reload_and_revisit(argv[2], directory);
            return 0;
        }
        coin_purse_tests();
        script_payment_tests();
        script_treasure_tests();
        deferred_treasure_tests();
        locked_door_tests();
        if (directory && *directory)
            installed_first_expedition(std::filesystem::absolute(argv[0]), directory);
        std::cout << "Expedition tests passed.\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
