#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/coin_purse.h"
#include "opengold/character.h"
#include "opengold/npc_portraits.h"
#include "opengold/rolf_tour.h"
#include "opengold/script_secrets.h"
#include "opengold/shop_data.h"
#include "opengold/srd5.h"
#include "test_support.h"
#include "tour_access.h"
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
// 50 gp, and magic gear: Leather Armor +1, a Stone of Good Luck that works
// from the pack, and a Wand of Secrets that points out the shop's door and the
// original's script-held secret doors and traps. Needs the installed game
// (OPENGOLD_GAME_DIR); without it the test passes.
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

// Every dialogue settle() answered, for checks on what a step said.
std::string transcript;

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
            transcript += s.dialogue + "\n";
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
    coins(state.wealth, Coin::gold) = 6000;
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
              sells("Short Bow +1", 400) && sells("Leather Armor +1", 4000) &&
              sells("Dust of Disappearance", 200) && sells("Wand of Secrets", 400) &&
              sells("Boots of Elvenkind", 400) &&
              sells("Cloak of Elvenkind", 400) && sells("Stone of Good Luck", 400) &&
              sells("Thieves' Tools", 25) && sells("Basic Poison", 100) && stock.size() == 13,
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
    // Bought, the magic dagger, the armor, the boots and the wand are what
    // they say; the boots are worn and the armor protects one point better.
    for (const std::string_view label :
         {"Dagger +1", "Boots of Elvenkind", "Leather Armor +1", "Wand of Secrets"})
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
    const auto rules = module();
    const auto &sheet = party->member(buyer).character.sheet();
    const std::array<std::string, 1> plain{"leather"}, magic{"leather+1"};
    check(bought("leather+1") != pack.end() && bought("leather+1")->name == "Leather Armor +1" &&
              rules->character_profile(sheet, magic).armor_class ==
                  rules->character_profile(sheet, plain).armor_class + 1,
          "The armor is Leather Armor +1, one point better than plain leather");
    const auto wand = bought("wand_of_secrets");
    check(wand != pack.end() && wand->quantity == 3 &&
              wand_charges(party->member(buyer), wand->id) == 3,
          "The Wand of Secrets comes with 3 charges");
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

// A Stone of Good Luck works from the pack, and a second adds nothing.
void stone_checks()
{
    auto party = party_with({rogue()});
    const auto id = party->state().slots[0];
    const auto before = party->profile(id);
    const auto check_before = party->ability_check(id, rules::Ability::strength, "athletics");
    por::Equipment stone;
    stone.stored.type = 218;
    stone.stored.stored_name = "Stone of Good Luck";
    stone.stored.stack_size = 1;
    check(party->award_loot({}, {stone}, "test:stone"), "The Rogue carries a stone");
    const auto with_stone = party->profile(id);
    check(party->member(id).equipped.empty() &&
              with_stone.item_save_bonus == before.item_save_bonus + 1 &&
              party->ability_check(id, rules::Ability::strength, "athletics").total ==
                  check_before.total + 1 &&
              with_stone.item_modifiers.find("Source: carried Stone of Good Luck") !=
                  std::string::npos,
          "A carried Stone of Good Luck adds 1 to checks and saving throws");
    check(party->award_loot({}, {stone}, "test:second-stone") &&
              party->profile(id).item_save_bonus == with_stone.item_save_bonus,
          "A second stone adds nothing");
}

// Adds a level-13 Wizard who prepared Teleport.
MemberId add_teleport_wizard(CampaignParty &party)
{
    rules::CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "wizard";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Tamsin";
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
    party.award_experience(120000, "wizard-xp");
    while (party.member(id).character.sheet().level < 13)
    {
        auto choice = party.default_advancement(id);
        if (party.member(id).character.sheet().level == 12)
        {
            (*choice.spell_learning)["spellbook:13"] = {"teleport", "finger_of_death"};
            if (std::find(choice.spells.begin(), choice.spells.end(), "teleport") ==
                choice.spells.end())
            {
                choice.spells.back() = "teleport";
            }
        }
        party.advance(id, choice);
    }
    return id;
}

// A Wand of Secrets with `charges` for the party: who takes it, and its ID.
std::pair<MemberId, std::uint64_t> give_wand(CampaignParty &party, unsigned charges)
{
    por::Equipment wand;
    wand.stored.type = 219;
    wand.stored.stored_name = "Wand of Secrets";
    wand.stored.stack_size = static_cast<std::uint8_t>(charges);
    const auto before = party.state().roster;
    // Each award is a reward of its own, or a second would be refused.
    static unsigned wands = 0;
    check(party.award_loot({}, {wand}, "test:wand:" + std::to_string(++wands)),
          "The party takes the wand");
    for (const auto &member : party.state().roster)
    {
        const auto was = std::find_if(before.begin(), before.end(),
                                      [&](const auto &m)
                                      {
                                          return m.id == member.id;
                                      });
        const auto items = member.character.inventory().items();
        if (was != before.end() && items.size() > was->character.inventory().items().size())
        {
            return {member.id, items.back().id};
        }
    }
    throw std::runtime_error("Nobody took the wand");
}

// The Wand of Secrets (SECRET-1): pointed in the alley it finds the hidden
// door of the Rogue's Shop; pointed again it finds nothing; spent it stays
// in the pack, and a Long Rest gives back 1d3 charges.
void wand_in_town_checks(const char *directory)
{
    auto party = party_with({rogue()});
    const auto [user, wand] = give_wand(*party, 3);
    auto town = town_with_shops(directory);
    town.campaign_party(party);
    settle(town);
    check(walk_to(town, shop_x, shop_y + 1), "The party stands in the alley");
    face(town, MapDirection::north);
    write_fixture(*party, town, directory, "alley-wand");
    // Two squares east, the door is as near as the shop's square behind it,
    // but fewer steps away from the alley.
    check(walk_to(town, shop_x + 2, shop_y + 1) && !door_found(town),
          "The party stands further down the alley");
    const auto sign = town.point_wand_of_secrets(user, wand);
    check(sign && sign->x == shop_x && sign->y == shop_y + 1 &&
              sign->side == MapDirection::north && door_found(town),
          "The wand points to the shop's door in the north wall of the alley, and it is found");
    check(!town.point_wand_of_secrets(user, wand), "No other hidden door is near");
    check(wand_charges(party->member(user), wand) == 1, "Each use spends a charge");
    (void)town.point_wand_of_secrets(user, wand);
    const auto &pack = party->member(user).character.inventory();
    check(pack.find(wand) && wand_charges(party->member(user), wand) == 0,
          "The spent wand stays in the pack");
    test::rejects(
        [&]
        {
            (void)town.point_wand_of_secrets(user, wand);
        },
        "A spent wand cannot be used");
    check(party->rest(RestKind::long_rest).has_value(), "The party takes a Long Rest");
    const auto charges = wand_charges(party->member(user), wand);
    check(charges >= 1 && charges <= 3 &&
              party->member(user).character.inventory().find(wand)->get().quantity == charges,
          "A Long Rest gives the wand back 1d3 charges");
}

// Points the wand where Teleport took the party: the door it finds must be
// `expected`, and the script's flag for it is then set.
void point_on_map(RolfTourSession &town, MemberId user, std::uint64_t wand,
                  const ScriptSecretDoor &expected, WandSign at)
{
    const auto sign = town.point_wand_of_secrets(user, wand);
    check(sign && sign->x == at.x && sign->y == at.y && sign->side == at.side,
          "The wand points to the nearest secret door on map " + std::to_string(expected.map));
    check((test::TourAccess::machine(town).variable(EclAddress{expected.flag}) & expected.found) ==
              expected.found,
          "The door the wand points to is found");
}

// The original's script-held secret doors, the Pyramid's and Kuto's Well's
// catacombs': the wand sets the flag each script lets the party through by
// (script_door_table_checks). And the traps of Kovel Mansion and the kobold
// caves.
void wand_on_script_doors(const char *directory)
{
    auto party = party_with({rogue()});
    const auto [holder, wand] = give_wand(*party, 3);
    const auto wizard = add_teleport_wizard(*party);
    auto town = town_with_shops(directory);
    town.campaign_party(party);
    settle(town);
    const auto rested = party->member(wizard).vitals.resources;
    const auto teleport = [&](unsigned map)
    {
        test::TourAccess::mark_visited(town, map, teleport_spot(map)->x, teleport_spot(map)->y);
        auto state = party->checkpoint();
        for (auto &member : state.roster)
        {
            if (member.id == wizard)
            {
                member.vitals.resources = rested;
            }
        }
        party->restore(std::move(state));
        town.teleport(wizard, "teleport", map);
        settle(town);
        check(town.snapshot().area_id == map, "Teleport reaches map " + std::to_string(map));
    };
    const auto door = [](unsigned map, unsigned x, unsigned y)
    {
        return *std::find_if(script_secret_doors.begin(), script_secret_doors.end(),
                             [&](const auto &d)
                             {
                                 return d.map == map && d.x == x && d.y == y;
                             });
    };
    // From the prison of souls' spot (7,8) the nearest is the door between
    // (4,10) and (5,10), two squares away at (5,10).
    teleport(22);
    point_on_map(town, holder, wand, door(22, 4, 10), {5, 10, MapDirection::west});
    // Kuto's Well's catacombs: the door south of (5,7), four squares from
    // the spot (7,3).
    teleport(32);
    point_on_map(town, holder, wand, door(32, 5, 7), {5, 7, MapDirection::south});

    // Traps whose scripts ask for Find Traps. In Kovel Mansion, from the spot
    // (2,7), the doorway trap at (5,6) is as near as the weapons' at (3,4)
    // and comes first; pointed out, the next use points to the weapons'.
    const auto [user, traps_wand] = give_wand(*party, 3);
    teleport(14);
    const auto doorway = town.point_wand_of_secrets(user, traps_wand);
    check(doorway && doorway->trap && doorway->x == 5 && doorway->y == 6,
          "The wand points to Kovel Mansion's nearest trap");
    const auto weapons = town.point_wand_of_secrets(user, traps_wand);
    check(weapons && weapons->trap && weapons->x == 3 && weapons->y == 4,
          "A trap pointed out is not pointed to again");
    // A save keeps the traps pointed out: the next nearest is the caskets'.
    const auto bytes = encode_campaign(*party, &town, "wand-traps");
    auto loaded = decode_campaign(bytes, *srd5::character_rules(), *module(), "wand-traps", &town);
    auto reloaded = std::make_shared<CampaignParty>(module());
    reloaded->restore(std::move(loaded.party));
    loaded.town->attach_restored_party(reloaded);
    const auto caskets = loaded.town->point_wand_of_secrets(user, traps_wand);
    check(caskets && caskets->trap && caskets->x == 6 && caskets->y == 7,
          "A save keeps the traps the wand pointed out");
    // In the kobold caves, from (7,7), the net at (10,9) is disarmed, so the
    // wand points to the spikes at (11,9); stepping there, the script sees
    // the trap as Find Traps would.
    teleport(13);
    test::TourAccess::set_variable(town, EclAddress{0x4A04}, 1);
    const auto spikes = town.point_wand_of_secrets(user, traps_wand);
    check(spikes && spikes->trap && spikes->x == 11 && spikes->y == 9,
          "The wand points past a disarmed trap to the next");
    check(walk_to(town, 10, 9), "The party reaches the disarmed net");
    face(town, MapDirection::east);
    transcript.clear();
    step(town, ExplorationCommand::forward);
    check(transcript.find("YOU FIND A TRAP") != std::string::npos,
          "The trap the wand pointed out is seen");
}

// The doors the wand knows are the ones the scripts search: each is on its
// map's squares with the script's search events, facing the way the
// script's table says. The traps are on their events' squares.
void script_door_table_checks(const char *directory)
{
    const auto maps = MapCatalog::load(directory);
    const auto scripts = EclCatalog::load(directory);
    struct Table
    {
        unsigned map, events, directions, masks;
        std::uint16_t flag;
    };
    for (const auto &table : {Table{22, 11, 0x9C95, 0x9CA0, 0x4A4B}, Table{23, 10, 0x9C40, 0x9C4A, 0x4A4F}})
    {
        const auto &geo = maps.find({"GEO7.DAX", static_cast<std::uint8_t>(table.map)})->get();
        const auto &raw = scripts.find({"ECL7.DAX", static_cast<std::uint8_t>(table.map)})->raw();
        const auto byte = [&](unsigned address)
        {
            return raw.at(address - EclProgram::origin + 2);
        };
        for (unsigned y = 0; y < 16; ++y)
        {
            for (unsigned x = 0; x < 16; ++x)
            {
                const auto event = geo.at(x, y).event_number();
                if (!event || event > table.events)
                {
                    continue;
                }
                const auto side = MapDirection{byte(table.directions + event - 1)};
                const auto mask = byte(table.masks + event - 1);
                const auto known = std::any_of(
                    script_secret_doors.begin(), script_secret_doors.end(),
                    [&](const auto &d)
                    {
                        const bool here = d.x == x && d.y == y && d.side == side;
                        const bool facing =
                            static_cast<int>(d.x) + step_x(d.side) == static_cast<int>(x) &&
                            static_cast<int>(d.y) + step_y(d.side) == static_cast<int>(y) &&
                            reversed(d.side) == side;
                        return d.map == table.map && d.flag == table.flag && d.found == mask &&
                               (here || facing);
                    });
                check(known, "The wand knows the secret door at " + std::to_string(x) + "," +
                                 std::to_string(y) + " of map " + std::to_string(table.map));
            }
        }
    }
    // Each trap is on a square whose event springs one: Kovel Mansion's
    // doorways (3-8), cabinets (10, 12), caskets (13) and weapons (14), the
    // kobold caves' pit (1), deadfall (3), net (4) and spikes (5).
    for (const auto &trap : script_traps)
    {
        const auto archive = trap.map == 14 ? "GEO3.DAX" : "GEO8.DAX";
        const auto event = maps.find({archive, static_cast<std::uint8_t>(trap.map)})
                               ->get()
                               .at(trap.x, trap.y)
                               .event_number();
        const bool springs = trap.map == 14 ? (event >= 3 && event <= 8) || event == 10 ||
                                                  event == 12 || event == 13 || event == 14
                                            : event == 1 || (event >= 3 && event <= 5);
        check(springs, "A trap's square has its event at " + std::to_string(trap.x) + "," +
                           std::to_string(trap.y) + " of map " + std::to_string(trap.map));
    }
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
        stone_checks();
        rogue_checks(directory);
        knock_checks(directory);
        script_door_table_checks(directory);
        wand_in_town_checks(directory);
        wand_on_script_doors(directory);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    std::cout << "Rogue's Shop checks passed\n";
}
