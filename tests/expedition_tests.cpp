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
            const Answer &answer = peaceful)
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
            check(town.input(s.continue_ticket, "0"), "The script accepts the input");
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

// Breadth-first walk over open edges, through the original movement events.
void walk_to(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party, unsigned tx,
             unsigned ty)
{
    constexpr std::array<int, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
    std::set<std::pair<int, int>> refused;
    for (unsigned step = 0; step < 400; ++step)
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
                if (a.doors[d] > 1 || b.doors[reverse] > 1 || (a.walls[d] && !a.doors[d]) ||
                        (b.walls[reverse] && !b.doors[reverse]))
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
        face(town, party, facing);
        town.explore(por::ExplorationCommand::forward);
        settle(town, party);
        const auto now = town.snapshot().pose;
        if (int(now.y * 16 + now.x) != next)
            refused.insert({origin, next});
    }
    throw std::runtime_error("Walk did not arrive");
}

void step(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party, unsigned facing,
          const Answer &answer = peaceful)
{
    face(town, party, facing);
    town.explore(por::ExplorationCommand::forward);
    settle(town, party, answer);
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
    const auto before = party->checkpoint();
    walk_to(town, party, 0, 4);
    step(town, party, 3);
    walk_to(town, party, 12, 1);
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

std::filesystem::path revisit_result(const std::filesystem::path &save)
{
    return std::filesystem::path(save).concat(".revisit");
}

// Runs in a fresh process: load the save, revisit the orcs, record the result.
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

    auto command = "\"" + executable.string() + "\" --revisit \"" + save.string() + "\"";
#ifdef _WIN32
    command = "\"" + command + "\""; // cmd.exe strips one outer pair of quotes.
#endif
    check(std::system(command.c_str()) == 0, "A fresh process reloads and revisits the event");
    revisit_orcs(trip);
    check(read_campaign_file(revisit_result(save)) ==
          encode_campaign(*trip.party, &trip.town, assets),
          "Continuing after a reload matches continuing without one");
    std::filesystem::remove_all(folder);
    std::cout << "Installed first expedition: created, equipped, defeated the four orcs, "
              "returned, paid the inn with change, rested, reloaded and revisited.\n";
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
