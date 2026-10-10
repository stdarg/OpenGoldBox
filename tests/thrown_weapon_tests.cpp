#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace opengold;
using namespace opengold::rules;

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);
constexpr std::array weapons{"dagger", "handaxe", "javelin", "light_hammer",
    "spear",  "dart",    "trident"};

void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

auto module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

Command command(const CombatSession &combat, std::string_view verb, EntityId target = 0)
{
    for (const auto &option : combat.legal_commands())
        if (option.verb == verb && (!target || option.target == target))
            return option;
    throw std::runtime_error("Missing baseline command: " + std::string(verb));
}

void act(CombatSession &combat, std::string_view verb, EntityId target = 0)
{
    check(combat.submit(command(combat, verb, target)), "Accept baseline legal command");
}

Character hero()
{
    CharacterDraft draft;
    draft.race = "human";
    draft.gender = "female";
    draft.character_class = "fighter";
    draft.background = "soldier";
    draft.alignment = "neutral_good";
    draft.name = "Thrown baseline";
    draft.rolled = true;
    for (auto &roll : draft.rolls)
        roll = {{6, 5, 4, 1}, 3};
    return Character(*srd5::character_rules(), draft, {});
}

// A level-four Fighter holding a Javelin (inventory id 3) and a shield, with
// every thrown weapon carried in stacks of three.
CampaignParty thrown_party()
{
    CampaignParty party(module());
    auto character = hero();
    for (const auto weapon : weapons)
        character.add_item({.definition_id = weapon, .name = weapon, .quantity = 3});
    const auto shield = character.add_item({.definition_id = "shield",
                                                   .name = "Baseline shield"});
    const auto id = party.add_pc(std::move(character));
    party.equip(id, 3); // Javelin in the ordered inventory above.
    party.equip(id, shield);
    party.award_experience(2700, "thrown-baseline");
    for (unsigned level = 2; level <= 4; ++level)
        party.advance(id, party.default_advancement(id));
    return party;
}

void settle(CombatSession &combat)
{
    if (combat.snapshot().free_movement)
        act(combat, "end");
}

void physical_inventory()
{
    const std::array classes{"barbarian", "bard",   "cleric", "druid",    "fighter", "monk",
                             "paladin",   "ranger", "rogue",  "sorcerer", "warlock", "wizard"};
    for (const auto klass : classes)
        for (unsigned level = 1; level <= 4; ++level)
            for (const auto weapon : weapons)
            {
                if (level > 1 && std::string_view(klass) != "fighter" &&
                        std::string_view(klass) != "cleric" && std::string_view(klass) != "wizard" &&
                        std::string_view(klass) != "rogue")
                    continue;
                CampaignParty party(module());
                auto draft = hero().creation_data();
                draft.character_class = klass;
                draft.background = "sage";
                Character pc(*srd5::character_rules(), draft, {});
                const auto held = pc.add_item({.definition_id = "longsword",
                                                      .name = "Held sword"});
                const auto stack = pc.add_item({.definition_id = weapon,
                                                       .name = "Carried weapon",
                                                       .quantity = 3});
                const auto shield = pc.add_item({.definition_id = "shield",
                                                        .name = "Held shield"});
                const auto id = party.add_pc(std::move(pc));
                party.equip(id, held);
                party.equip(id, shield);
                party.award_experience(2700, "throw-levels");
                for (unsigned n = 1; n < level; ++n)
                    party.advance(id, party.default_advancement(id));
                auto actors = party.participants();
                actors.front().cell = {1, 1};
                actors.push_back({2, "vanguard", "Target", Side::opposition, {2, 1}});
                actors.push_back({3, "vanguard", "Reserve", Side::opposition, {7, 7}});
                auto rules = module();
                auto combat = rules->create({{10, 8, std::vector<Terrain>(80)}, actors}, 19);
                party.begin_combat();
                party.apply_combat(combat->snapshot());
                while (combat->snapshot().actor != id)
                    act(*combat, "end");
                auto before = combat->save();
                check(before.starts_with("OGCOMBAT 47 "),
                      "New physical encounters use the current checkpoint format");
                check(rules->restore(before)->save() == before,
                      "Physical inventory round trips before throw");
                auto offered = command(*combat, "throw", 2);
                auto rejected = offered;
                rejected.item = 100000;
                check(!combat->submit(rejected) && combat->save() == before,
                      "Rejected throw changes no state or RNG");
                const auto snapshot = combat->snapshot();
                const auto &view =
                    *std::find_if(snapshot.combatants.begin(), snapshot.combatants.end(),
                                  [&](const auto & a)
                {
                    return a.id == id;
                });
                check(view.thrown_weapons.size() == 1 &&
                      view.thrown_weapons.front().label.source ==
                      combat->snapshot().held_items.at(offered.item - 1).label.source,
                      "The Thrown dropdown names the weapon, with no count or stowing");
                check(combat->submit(offered), "Every class can throw carried weapon");
                check(!combat->submit(offered), "Stale command cannot throw twice");
                auto after = combat->save();
                check(rules->restore(after)->save() == after,
                      "Thrown outcome/pending damage round trips");
                settle(*combat);
                party.apply_combat(combat->snapshot());
                const auto thrown = combat->snapshot().held_items.at(offered.item - 1);
                check(thrown.holder == id && thrown.inventory_id == stack && thrown.stowed,
                      "Like ammunition, the thrown weapon stays carried by its thrower");
                check(party.member(id).character.inventory().find(stack)->get().quantity == 3 &&
                      party.member(id).equipped == std::vector<std::uint64_t>({held, shield}),
                      "Throwing spends no inventory and changes no held equipment");
                party.end_combat();
                const auto bytes = encode_campaign(party, nullptr, "physical");
                CampaignParty loaded(module());
                loaded.restore(
                    decode_campaign(bytes, *srd5::character_rules(), *rules, "physical", nullptr)
                    .party);
                check(encode_campaign(loaded, nullptr, "physical") == bytes,
                      "Post-combat equipment and quantities survive campaign save");
            }
}

void critical_stack()
{
    auto rules = module();
    bool tested = false;
    for (unsigned seed = 0; seed < 200 && !tested; ++seed)
    {
        auto party = thrown_party();
        auto actors = party.participants();
        actors.front().cell = {1, 1};
        actors.push_back({2, "vanguard", "Target", Side::opposition, {5, 1}});
        actors.push_back({3, "vanguard", "Reserve", Side::opposition, {7, 7}});
        auto combat = rules->create({{10, 8, std::vector<Terrain>(80)}, actors}, seed);
        party.begin_combat();
        party.apply_combat(combat->snapshot());
        while (combat->snapshot().actor != 1)
            act(*combat, "end");
        act(*combat, "ranged", 2);
        const auto state = combat->snapshot();
        if (!state.free_movement)
            continue;
        party.apply_combat(state);
        check(party.member(1).character.inventory().find(3)->get().quantity == 3,
              "A ranged attack with a held Thrown weapon keeps it");
        auto restored = rules->restore(combat->save());
        check(restored->save() == combat->save(),
              "Critical thrown hit with automatic Savage damage retains the Champion trigger");
        check(rules->restore(combat->save())->save() == combat->save(),
              "Physical inventory and Champion phase coexist in save");
        act(*combat, "end");
        act(*combat, "action_surge");
        auto throw_again = command(*combat, "throw", 2);
        for (const auto &command : combat->legal_commands())
            if (command.verb == "throw" && command.target == 2)
            {
                const auto snapshot = combat->snapshot();
                const auto item =
                    std::find_if(snapshot.held_items.begin(), snapshot.held_items.end(),
                                 [&](const auto & i)
                {
                    return i.id == command.item;
                });
                if (item->inventory_id == 3)
                {
                    throw_again = command;
                    break;
                }
            }
        check(combat->submit(throw_again), "Surge can throw the same weapon again");
        settle(*combat);
        party.apply_combat(combat->snapshot());
        check(party.member(1).character.inventory().find(3)->get().quantity == 3,
              "A second throw still spends nothing");
        tested = true;
    }
    check(tested, "Exercise actual critical/Savage/Champion throw sequence");
}

void large_stack()
{
    auto rules = module();
    auto draft = hero().creation_data();
    draft.background = "sage";
    CampaignParty party(module());
    Character pc(*srd5::character_rules(), draft, {});
    const auto sword = pc.add_item({.definition_id = "longsword", .name = "Keep in hand"});
    const auto stack = pc.add_item({.definition_id = "javelin",
                                           .name = "Large stack",
                                           .quantity = 1000000});
    const auto first = party.add_pc(std::move(pc));
    party.equip(first, sword);
    auto actors = party.participants();
    actors[0].cell = {1, 1};
    actors.push_back({99, "vanguard", "Target", Side::opposition, {3, 1}});
    auto combat = rules->create({{8, 8, std::vector<Terrain>(64)}, actors}, 1);
    party.begin_combat();
    party.apply_combat(combat->snapshot());
    while (combat->snapshot().actor != first)
        act(*combat, "end");
    check(combat->snapshot().held_items.size() == 2,
          "Large stack does not allocate one record per unit");
    act(*combat, "throw", 99);
    settle(*combat);
    party.apply_combat(combat->snapshot());
    check(party.member(first).equipped == std::vector<std::uint64_t> {sword} &&
          party.member(first).character.inventory().find(stack)->get().quantity == 1000000 &&
          combat->snapshot().held_items.size() == 2,
          "Throwing from a stack neither splits it nor changes the held weapon");
    party.end_combat();
    const auto saved = encode_campaign(party, nullptr, "transfer");
    CampaignParty copy(module());
    copy.restore(
        decode_campaign(saved, *srd5::character_rules(), *rules, "transfer", nullptr).party);
    check(encode_campaign(copy, nullptr, "transfer") == saved,
          "Equipment after throwing survives campaign save");
}

void control_fixture()
{
    auto pc = hero();
    auto draft = pc.creation_data();
    draft.background = "sage";
    pc = Character(*srd5::character_rules(), draft, {});
    const auto sword = pc.add_item({.definition_id = "longsword", .name = "Sword"});
    const auto shield = pc.add_item({.definition_id = "shield", .name = "Shield"});
    for (auto weapon : weapons)
        pc.add_item({.definition_id = weapon, .name = weapon, .quantity = 3});
    CampaignParty party(module());
    auto id = party.add_pc(std::move(pc));
    party.equip(id, sword);
    party.equip(id, shield);
    auto actors = party.participants();
    actors.front().cell = {1, 1};
    actors.push_back({2, "vanguard", "Target", Side::opposition, {2, 1}});
    actors.push_back({3, "vanguard", "Reserve", Side::opposition, {7, 7}});
    auto rules = module();
    auto combat = rules->create({{10, 8, std::vector<Terrain>(80)}, actors}, 1);
    while (combat->snapshot().actor != id)
        act(*combat, "end");
    const auto folder = std::filesystem::path(OPENGOLD_BINARY_DIR) / "thrown-fixtures";
    std::filesystem::create_directories(folder);
    std::ofstream(folder / "before.save", std::ios::binary) << combat->save();
}

} // namespace

int main()
{
    try
    {
        physical_inventory();
        critical_stack();
        large_stack();
        control_fixture();
        std::cout << "Thrown weapon checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
