#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/coin_purse.h"
#include "opengold/character.h"
#include "opengold/npc_portraits.h"
#include "opengold/rolf_tour.h"
#include "opengold/shop_data.h"
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

// The Rogue's Shop behind New Phlan's inn at (4,13) (SECRET-1, #271): a wall
// closes it from the inn, and its secret door in the south wall is found by
// the original's search rolls, picked by a Rogue or opened by Knock, never
// bashed, and stays open in saves. The shop sells the Poisoner's Kit for
// 50 gp. Needs the installed game (OPENGOLD_GAME_DIR); without it the test
// passes.
namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);
constexpr unsigned shop_x = 4, shop_y = 13;

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

Character rogue()
{
    rules::CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "rogue";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Thief";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    d.training = {{"class:rogue", {"acrobatics", "stealth", "perception", "investigation"}},
                  {"class:rogue:expertise", {"investigation", "perception"}}};
    return Character(*srd5::character_rules(), d, {});
}

// An elf finds secret doors more easily, as in the original.
Character elf_cleric()
{
    rules::CharacterDraft d;
    d.race = "elf";
    d.gender = "male";
    d.character_class = "cleric";
    d.background = "acolyte";
    d.alignment = "lawful_good";
    d.name = "Elf";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    d.cantrips = std::vector<std::string>{"sacred_flame", "spare_the_dying", "resistance"};
    d.spells = rules::SpellChoices{{}, std::vector<std::string>{"cure_wounds"}, {}, {}};
    d.training = {{"class:cleric", {"medicine", "persuasion"}},
                  {"class:cleric:divine_order", {"protector"}}};
    return Character(*srd5::character_rules(), d, {});
}

// Adds a level-3 Wizard who prepared Knock.
void add_knock_wizard(CampaignParty &party)
{
    rules::CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "wizard";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Wizard";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    d.cantrips = std::vector<std::string>{"fire_bolt", "ray_of_frost", "chill_touch"};
    const std::vector<std::string> spells{"magic_missile"};
    d.spells = rules::SpellChoices{{{"spellbook:1", spells}}, spells, {}, {}};
    d.training = {{"class:wizard", {"medicine", "nature"}}};
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(900, "knock-xp");
    while (party.member(id).character.sheet().level < 3)
    {
        auto choice = party.default_advancement(id);
        if (party.member(id).character.sheet().level == 2)
        {
            // The book learns Knock and Misty Step in place of the default
            // picks, and Knock is prepared in place of one of those.
            auto &learned = (*choice.spell_learning)["spellbook:3"];
            const auto defaults = learned;
            const auto prepared = choice.spells.size();
            learned = {"knock", "misty_step"};
            std::erase_if(choice.spells,
                          [&](const auto &spell)
                          {
                              return std::find(defaults.begin(), defaults.end(), spell) !=
                                     defaults.end();
                          });
            for (const auto &spell : learned)
            {
                if (choice.spells.size() < prepared &&
                    std::find(choice.spells.begin(), choice.spells.end(), spell) ==
                        choice.spells.end())
                {
                    choice.spells.push_back(spell);
                }
            }
        }
        party.advance(id, choice);
    }
}

// Runs the town until it waits on the player. Dialogue choices take the
// last (leaving, or the inn's NO); the locked menu is left to the caller.
void settle(RolfTourSession &town)
{
    for (unsigned n = 0; n < 2000; ++n)
    {
        const auto s = town.snapshot();
        check(s.phase != TourPhase::faulted, s.diagnostic);
        if (s.phase == TourPhase::completed || s.phase == TourPhase::shopping ||
            (s.phase == TourPhase::awaiting_continue && s.dialogue == "Locked."))
        {
            return;
        }
        if (s.phase == TourPhase::awaiting_continue)
        {
            town.choose(s.continue_ticket, s.choices.empty() ? 0 : s.choices.size() - 1);
        }
        else if (s.phase == TourPhase::awaiting_input)
        {
            town.input(s.continue_ticket, "0");
        }
        else
        {
            town.advance(.3);
        }
    }
    throw std::runtime_error("A town event did not settle");
}

void step(RolfTourSession &town, ExplorationCommand command)
{
    check(town.explore(command), "The party can act");
    settle(town);
}

void face(RolfTourSession &town, MapDirection facing)
{
    while (town.snapshot().pose.facing != facing)
    {
        step(town, ExplorationCommand::turn_right);
    }
}

// Walks to a square of the town map; true when the party stands on it.
bool walk_to(RolfTourSession &town, unsigned tx, unsigned ty)
{
    std::set<std::pair<int, int>> refused;
    for (unsigned steps = 0; steps < 400; ++steps)
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
                const auto side = MapDirection{d};
                const int x = cell % 16 + step_x(side), y = cell / 16 + step_y(side);
                if (x < 0 || y < 0 || x >= 16 || y >= 16)
                {
                    continue;
                }
                const auto &a = town.map().at(cell % 16, cell / 16);
                const auto &b = town.map().at(x, y);
                const auto back = index(reversed(side));
                if (a.doors[d] > 1 || b.doors[back] > 1 || (a.walls[d] && !a.doors[d]) ||
                    (b.walls[back] && !b.doors[back]))
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
        const auto facing = next % 16 > int(p.x)   ? MapDirection::east
                            : next % 16 < int(p.x) ? MapDirection::west
                            : next / 16 > int(p.y) ? MapDirection::south
                                                   : MapDirection::north;
        face(town, facing);
        step(town, ExplorationCommand::forward);
        const auto now = town.snapshot().pose;
        if (static_cast<int>(now.y * 16 + now.x) != next)
        {
            refused.insert({origin, next});
        }
    }
    return false;
}

std::shared_ptr<CampaignParty> party_with(std::vector<Character> members)
{
    auto party = std::make_shared<CampaignParty>(module());
    for (auto &member : members)
    {
        party->add_pc(std::move(member));
    }
    auto state = party->checkpoint();
    coins(state.wealth, Coin::gold) = 2000;
    party->restore(std::move(state));
    return party;
}

RolfTourSession town_with_shops(const char *directory)
{
    auto town = RolfTourSession::load(directory);
    town.shops(load_shop_data(root / "data/shops").shops);
    return town;
}

// With OPENGOLD_ROGUE_SHOP_FIXTURES set, saves the party in the alley as
// `name`.ogs there, for screenshots of the secret door.
void write_fixture(const CampaignParty &party, const RolfTourSession &town, const char *directory,
                   const std::string &name)
{
    if (const auto *fixtures = std::getenv("OPENGOLD_ROGUE_SHOP_FIXTURES"))
    {
        write_campaign_file(std::filesystem::path(fixtures) / (name + ".ogs"),
                            encode_campaign(party, &town, campaign_asset_identity(directory)));
    }
}

bool door_found(const RolfTourSession &town)
{
    return town.map().at(shop_x, shop_y + 1).doors[index(MapDirection::north)] != 0;
}

bool has_choice(const TourSnapshot &s, std::string_view choice)
{
    return std::find(s.choices.begin(), s.choices.end(), choice) != s.choices.end();
}

std::size_t choice_index(const TourSnapshot &s, std::string_view choice)
{
    return static_cast<std::size_t>(std::find(s.choices.begin(), s.choices.end(), choice) -
                                    s.choices.begin());
}

// Searches from the alley south of the shop with Look until the door is found.
void find_the_door(RolfTourSession &town)
{
    for (unsigned looks = 0; looks < 200; ++looks)
    {
        step(town, ExplorationCommand::look);
        const auto &s = town.snapshot();
        check(!s.secret_door_searches.empty(), "Looking in the alley searches for the secret door");
        for (const auto &search : s.secret_door_searches)
        {
            check(search.needed == (search.member == "Elf" ? 2 : 1),
                  "Searching finds the door on a 1, an elf on a 1 or 2");
        }
        if (s.secret_door_found)
        {
            check(s.dialogue.find("You find a secret door in the north wall.") != std::string::npos,
                  "Finding the door says so");
            return;
        }
    }
    throw std::runtime_error("The party never found the secret door");
}

// From the alley, steps north into the shop: opens the locked menu, tries
// `method` until the door opens, and returns once the shop is open.
void open_the_door(RolfTourSession &town, std::string_view method)
{
    for (unsigned tries = 0; tries < 200; ++tries)
    {
        check(walk_to(town, shop_x, shop_y + 1), "The party stands in the alley");
        face(town, MapDirection::north);
        step(town, ExplorationCommand::forward);
        auto s = town.snapshot();
        check(s.phase == TourPhase::awaiting_continue && s.dialogue == "Locked.",
              "The secret door is locked");
        check(!has_choice(s, "Bash") && has_choice(s, method) && has_choice(s, "Exit"),
              "The secret door cannot be bashed");
        town.choose(s.continue_ticket, choice_index(s, method));
        settle(town);
        if (town.snapshot().phase == TourPhase::shopping)
        {
            check(town.snapshot().pose.x == shop_x && town.snapshot().pose.y == shop_y,
                  "The opened door leads into the shop");
            return;
        }
        // A failed pick waits for the party to move before the next.
        face(town, MapDirection::east);
        step(town, ExplorationCommand::forward);
    }
    throw std::runtime_error("The secret door never opened");
}

void rogue_checks(const char *directory)
{
    auto party = party_with({rogue(), elf_cleric()});
    auto town = town_with_shops(directory);
    town.campaign_party(party);
    settle(town);
    const auto &map = town.map();
    check(map.at(shop_x, shop_y).walls[index(MapDirection::north)] &&
              map.at(shop_x, shop_y - 1).walls[index(MapDirection::south)] &&
              map.at(shop_x, shop_y).event_raw == 0,
          "A wall closes the shop from the inn, whose event it no longer has");
    check(map.at(shop_x, shop_y).doors[index(MapDirection::south)] == 0 &&
              map.at(shop_x, shop_y).walls[index(MapDirection::south)],
          "The secret door looks like the wall it hides in");
    check(!walk_to(town, shop_x, shop_y), "Nobody walks into the hidden shop");
    check(walk_to(town, shop_x, shop_y + 1), "The party reaches the alley behind the shop");
    if (!door_found(town))
    {
        face(town, MapDirection::north);
        write_fixture(*party, town, directory, "alley-hidden");
    }
    // Stepping in without searching, only the elf rolls.
    for (unsigned steps = 0; steps < 10 && !door_found(town); ++steps)
    {
        face(town, MapDirection::east);
        step(town, ExplorationCommand::forward);
        face(town, MapDirection::west);
        step(town, ExplorationCommand::forward);
        const auto &searches = town.snapshot().secret_door_searches;
        check(door_found(town) || (searches.size() == 1 && searches[0].member == "Elf" &&
                                   searches[0].needed == 1),
              "Without searching, only an elf can find the door");
    }
    if (!door_found(town))
    {
        find_the_door(town);
    }
    check(town.map().at(shop_x, shop_y + 1).doors[index(MapDirection::north)] == 2 &&
              town.map().at(shop_x, shop_y + 1).walls[index(MapDirection::north)] == 7,
          "The found door is drawn as a locked door");
    open_the_door(town, "Pick");
    check(opengold::speaking_npc_portrait(town.snapshot(), town.shops()) ==
              "NPCs/kovel-old-thief.png",
          "The shop shows its shopkeeper");
    const auto &stock = town.shop_stock();
    const auto kit = std::find_if(stock.begin(), stock.end(),
                                  [](const auto &item)
                                  {
                                      return item.stored.type == 212;
                                  });
    check(kit != stock.end() && kit->stored.value == 50, "The shop sells the Poisoner's Kit for 50 gp");
    const auto sells = [&](std::string_view label, unsigned price)
    {
        return std::any_of(stock.begin(), stock.end(),
                           [&](const auto &item)
                           {
                               return item.label() == label && item.stored.value == price;
                           });
    };
    check(sells("Magic Lock Picks", 50) && sells("Dagger +1", 400) && sells("Short Sword +1", 400) &&
              sells("Short Bow +1", 400) && sells("Dust of Disappearance", 200) &&
              sells("Boots of Elvenkind", 400) &&
              sells("Cloak of Elvenkind", 400) && sells("Stone of Good Luck", 400) &&
              sells("Thieves' Tools", 25) && sells("Basic Poison", 100) && stock.size() == 11,
          "The shop sells its rogue's gear, the magic items known by name");
    const auto gold = coins(party->state().wealth, Coin::gold);
    check(town.buy(town.snapshot().continue_ticket, static_cast<std::size_t>(kit - stock.begin())),
          "The party buys the kit");
    const auto &items = party->member(party->selected()).character.inventory().items();
    check(coins(party->state().wealth, Coin::gold) == gold - 50 &&
              std::any_of(items.begin(), items.end(),
                          [](const auto &item)
                          {
                              return item.definition_id == "poisoners_kit";
                          }),
          "The kit costs 50 gp and joins the buyer's pack");
    // Bought, the magic dagger and the boots are what they say, and the boots
    // are worn.
    for (const std::string_view label : {"Dagger +1", "Boots of Elvenkind"})
    {
        const auto item = std::find_if(stock.begin(), stock.end(),
                                       [&](const auto &offered)
                                       {
                                           return offered.label() == label;
                                       });
        check(town.buy(town.snapshot().continue_ticket,
                       static_cast<std::size_t>(item - stock.begin())),
              "The party buys " + std::string(label));
    }
    const auto buyer = party->selected();
    const auto &pack = party->member(buyer).character.inventory().items();
    const auto bought = [&](std::string_view definition)
    {
        return std::find_if(pack.begin(), pack.end(),
                            [&](const auto &item)
                            {
                                return item.definition_id == definition;
                            });
    };
    check(bought("dagger+1") != pack.end() && bought("boots_of_elvenkind") != pack.end() &&
              bought("dagger+1")->name == "Dagger +1",
          "The dagger is a Dagger +1, named so");
    party->equip(buyer, bought("boots_of_elvenkind")->id);
    check(party->profile(buyer).item_modifiers.find("Boots of Elvenkind") != std::string::npos,
          "The boots are worn");
    check(town.leave_shop(town.snapshot().continue_ticket) &&
              town.snapshot().phase == TourPhase::completed,
          "Leaving the shop ends the visit");
    // The door stays open in a save.
    const auto bytes = encode_campaign(*party, &town, "rogue-shop");
    auto loaded = decode_campaign(bytes, *srd5::character_rules(), *module(), "rogue-shop", &town);
    check(loaded.town && loaded.town->map().at(shop_x, shop_y).doors[index(MapDirection::south)] == 1,
          "The opened secret door stays open after loading");
    // Back out through the open door and in again: the shop opens at once.
    step(town, ExplorationCommand::turn_around);
    step(town, ExplorationCommand::forward);
    check(town.snapshot().pose.y == shop_y + 1, "The party walks out through the open door");
    step(town, ExplorationCommand::turn_around);
    write_fixture(*party, town, directory, "alley-open");
    step(town, ExplorationCommand::forward);
    check(town.snapshot().phase == TourPhase::shopping, "The open door leads straight into the shop");
    check(town.leave_shop(town.snapshot().continue_ticket), "The party leaves again");
}

// A Rogue with Magic Lock Picks picks with Advantage and uses up one set.
void lock_pick_checks()
{
    auto party = party_with({rogue()});
    const auto id = party->state().slots[0];
    por::Equipment picks;
    picks.stored.type = 215;
    picks.stored.stored_name = "Magic Lock Picks";
    picks.stored.stack_size = 1;
    check(party->award_loot({}, {picks, picks}, "test:picks"), "The Rogue carries two sets");
    const auto sets = [&]
    {
        std::uint32_t count = 0;
        for (const auto &item : party->member(id).character.inventory().items())
        {
            count += item.definition_id == "magic_lock_picks" ? item.quantity : 0;
        }
        return count;
    };
    check(sets() == 2, "Two sets of picks in the pack");
    const auto attempts = party->try_door(DoorMethod::pick, 15);
    check(attempts.size() == 1 && attempts[0].magic_lock_picks && sets() == 1,
          "Picking a lock uses up one set for Advantage");
    (void)party->try_door(DoorMethod::pick, 15);
    check(sets() == 0 && !party->try_door(DoorMethod::pick, 15).at(0).magic_lock_picks,
          "With no picks left the Rogue picks without them");
}

// A party with no Rogue opens the door with Knock.
void knock_checks(const char *directory)
{
    auto party = std::make_shared<CampaignParty>(module());
    add_knock_wizard(*party);
    auto town = town_with_shops(directory);
    town.campaign_party(party);
    settle(town);
    check(walk_to(town, shop_x, shop_y + 1), "The Wizard reaches the alley");
    find_the_door(town);
    open_the_door(town, "Knock");
}
} // namespace

int main()
{
    const char *directory = std::getenv("OPENGOLD_GAME_DIR");
    if (!directory || !std::filesystem::exists(directory))
    {
        std::cout << "Rogue's Shop checks skipped: OPENGOLD_GAME_DIR is not set\n";
        return 0;
    }
    try
    {
        lock_pick_checks();
        rogue_checks(directory);
        knock_checks(directory);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    std::cout << "Rogue's Shop checks passed\n";
}
