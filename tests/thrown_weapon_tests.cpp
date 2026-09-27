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
const auto fixtures = root / "tests/fixtures";
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

std::string read(const char *name)
{
    std::ifstream in(fixtures / name);
    check(bool(in), "Open historical thrown fixture");
    return {std::istreambuf_iterator<char>(in), {}};
}

void write(const char *name, const std::string &bytes)
{
    std::ofstream out(fixtures / name);
    out << bytes;
    check(bool(out), "Write actual prior-writer fixture");
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

std::string current_identity(std::string bytes, const RulesModule &rules)
{
    const auto position = bytes.find("0.6.46");
    check(position != bytes.npos, "Actual prior identity exists");
    bytes.replace(position, 6, rules.identity().version);
    return bytes;
}

void finish_second_throw(CombatSession &combat)
{
    act(combat, "end"); // Decline the first critical's free movement.
    act(combat, "action_surge");
    act(combat, "ranged", 2);
    if (combat.snapshot().free_movement)
        act(combat, "end");
}

void capture_prior_writer()
{
    auto rules = module();
    check(rules->identity().version == "0.6.46", "Capture only with the actual pre-Thrown writer");
    CampaignParty party(module());
    auto character = hero();
    for (const auto weapon : weapons)
        character.inventory().add(weapon, weapon, 3);
    const auto shield = character.inventory().add("shield", "Baseline shield");
    const auto id = party.add_pc(std::move(character));
    party.equip(id, 3); // Javelin in the ordered inventory above.
    party.equip(id, shield);
    party.award_experience(2700, "thrown-baseline");
    for (unsigned level = 2; level <= 4; ++level)
        party.advance(id, party.default_advancement(id));
    auto state = party.checkpoint();
    state.roster[0].vitals.hit_points -= 3;
    state.roster[0].wealth[3] = 37;
    state.time_minutes = 123;
    state.subminute_milliseconds = 456;
    state.random_state = 789;
    party.restore(state);
    write("campaign-v11-thrown-before.ogs", encode_campaign(party, nullptr, "thrown-before"));

    bool captured = false;
    for (unsigned seed = 0; seed < 400 && !captured; ++seed)
    {
        auto actors = party.participants();
        actors[0].cell = {1, 1};
        actors[0].ground_equipment = {1}; // Actual ground-item ledger; shield is already dropped.
        actors.push_back({2, "vanguard", "Target", 1, {5, 1}});
        actors.push_back({3, "vanguard", "Reserve", 1, {7, 7}});
        auto combat = rules->create({{10, 8, std::vector<std::uint8_t>(80)}, actors}, seed);
        while (combat->snapshot().actor != id)
            act(*combat, "end");
        act(*combat, "second_wind");
        const auto before = combat->save();
        // Capture continuation from an actual reload, including the old codec's
        // implicit format-16 movement flag, rather than unsaved constructor state.
        combat = rules->restore(before);
        check(combat->save() == before, "Prior reload is canonical before capture");
        act(*combat, "ranged", 2);
        const auto hit = combat->snapshot().savage_attack_choice;
        if (!hit || !hit->critical)
            continue;
        const auto pending = combat->save();
        act(*combat, "savage_use");
        act(*combat, "savage_second");
        check(bool(combat->snapshot().free_movement),
              "Real thrown critical opens Champion movement");
        write("combat-v16-thrown-before.save", before);
        write("combat-v16-thrown-choice.save", pending);
        write("combat-v18-thrown-free.save", combat->save());
        finish_second_throw(*combat);
        write("combat-v16-thrown-continued.save", combat->save());
        captured = true;
    }
    check(captured, "Capture actual critical thrown attack sequence");
}

void prior_writer_continuation()
{
    auto rules = module();
    auto combat = rules->restore(read("combat-v16-thrown-before.save"));
    check(combat->save() == current_identity(read("combat-v16-thrown-before.save"), *rules),
          "Prior held/ground items, spent Bonus Action and RNG round trip exactly");
    act(*combat, "ranged", 2);
    check(combat->save() == current_identity(read("combat-v16-thrown-choice.save"), *rules),
          "Prior thrown attack preserves pending Savage dice and resources");
    auto pending = rules->restore(combat->save());
    for (auto *session : {combat.get(), pending.get()})
    {
        act(*session, "savage_use");
        act(*session, "savage_second");
    }
    check(combat->save() == pending->save() &&
              combat->save() == current_identity(read("combat-v18-thrown-free.save"), *rules),
          "Prior critical preserves accepted damage and immediate Champion phase");
    auto free_move = rules->restore(combat->save());
    for (auto *session : {combat.get(), pending.get(), free_move.get()})
        finish_second_throw(*session);
    check(
        combat->save() == pending->save() && combat->save() == free_move->save() &&
            combat->save() == current_identity(read("combat-v16-thrown-continued.save"), *rules),
        "Prior writer's second throw continues byte-exactly without changing its inventory semantics");

    const auto old = read("campaign-v11-thrown-before.ogs");
    CampaignParty party(module());
    party.restore(
        decode_campaign(old, *srd5::character_rules(), *rules, "thrown-before", nullptr).party);
    const auto &member = party.member(1);
    check(member.character.sheet().level == 4 && member.wealth[3] == 37 &&
              member.equipped == std::vector<std::uint64_t>{3, 8},
          "Prior campaign retains level, wealth and equipped identities");
    for (unsigned index = 0; index < weapons.size(); ++index)
    {
        const auto item = member.character.inventory().find(index + 1);
        check(item && item->get().definition_id == weapons[index] && item->get().quantity == 3,
              "Each actual prior carried/held thrown stack retains its quantity and identity");
    }
    const auto saved = encode_campaign(party, nullptr, "thrown-before");
    const auto body = [](const std::string &bytes)
    {
        return bytes.substr(bytes.find('\n', bytes.find('\n') + 1) + 1);
    };
    check(body(saved) == body(current_identity(old, *rules)),
          "Prior campaign body changes only module identity");
    CampaignParty copy(module());
    copy.restore(
        decode_campaign(saved, *srd5::character_rules(), *rules, "thrown-before", nullptr).party);
    check(encode_campaign(copy, nullptr, "thrown-before") == saved,
          "Campaign migration is canonical");
}

void settle(CombatSession &combat)
{
    if (combat.snapshot().savage_attack_choice)
        act(combat, "savage_skip");
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
                const auto held = pc.inventory().add("longsword", "Held sword");
                const auto stack = pc.inventory().add(weapon, "Carried weapon", 3);
                const auto shield = pc.inventory().add("shield", "Held shield");
                const auto id = party.add_pc(std::move(pc));
                party.equip(id, held);
                party.equip(id, shield);
                party.award_experience(2700, "throw-levels");
                for (unsigned n = 1; n < level; ++n)
                    party.advance(id, party.default_advancement(id));
                auto actors = party.participants();
                actors.front().cell = {1, 1};
                actors.push_back({2, "vanguard", "Target", 1, {2, 1}});
                actors.push_back({3, "vanguard", "Reserve", 1, {7, 7}});
                auto rules = module();
                auto combat = rules->create({{10, 8, std::vector<std::uint8_t>(80)}, actors}, 19);
                party.begin_combat();
                party.apply_combat(combat->snapshot());
                while (combat->snapshot().actor != id)
                    act(*combat, "end");
                auto before = combat->save();
                check(before.starts_with(std::string_view(klass) == "rogue" ? "OGCOMBAT 21 "
                                                                            : "OGCOMBAT 19 "),
                      "New physical encounters use their semantic checkpoint");
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
                                  [&](const auto &a)
                                  {
                                      return a.id == id;
                                  });
                check(view.thrown_weapons.size() == 1 &&
                          view.thrown_weapons.front().label.source.find("stow") !=
                              std::string::npos,
                      "Necessary stowing is shown before the attack");
                check(combat->submit(offered), "Every class can throw carried weapon");
                check(!combat->submit(offered), "Stale command cannot spend a second weapon");
                party.apply_combat(combat->snapshot());
                check(party.member(id).character.inventory().find(stack)->get().quantity == 2,
                      "Exactly one actual inventory unit spent");
                check(party.member(id).equipped == std::vector<std::uint64_t>{shield},
                      "Sword stowed while shield remains equipped");
                auto after = combat->save();
                check(rules->restore(after)->save() == after,
                      "Thrown outcome/pending damage round trips");
                settle(*combat);
                party.apply_combat(combat->snapshot());
                const auto landed = combat->snapshot();
                const auto item = std::find_if(landed.held_items.begin(), landed.held_items.end(),
                                               [&](const auto &i)
                                               {
                                                   return !i.holder && i.definition == weapon;
                                               });
                check(item != landed.held_items.end() && item->cell == Cell{2, 1} &&
                          item->quantity == 1 && item->inventory_id == stack,
                      "Thrown unit lands at target with original source identity");
                const auto original_token = item->id;
                act(*combat, "pick_up", original_token);
                party.apply_combat(combat->snapshot());
                unsigned count = 0;
                for (const auto &i : party.member(id).character.inventory().items())
                    if (i.definition_id == weapon)
                        count += i.quantity;
                check(count == 3 && party.state().detached_items.empty(),
                      "Pickup restores exactly one reachable weapon");
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
        CampaignParty party(module());
        party.restore(decode_campaign(read("campaign-v11-thrown-before.ogs"),
                                      *srd5::character_rules(), *rules, "thrown-before", nullptr)
                          .party);
        auto actors = party.participants();
        actors.front().cell = {1, 1};
        actors.push_back({2, "vanguard", "Target", 1, {5, 1}});
        actors.push_back({3, "vanguard", "Reserve", 1, {7, 7}});
        auto combat = rules->create({{10, 8, std::vector<std::uint8_t>(80)}, actors}, seed);
        party.begin_combat();
        party.apply_combat(combat->snapshot());
        while (combat->snapshot().actor != 1)
            act(*combat, "end");
        act(*combat, "ranged", 2);
        const auto state = combat->snapshot();
        if (!state.savage_attack_choice || !state.savage_attack_choice->critical)
            continue;
        party.apply_combat(state);
        check(party.member(1).character.inventory().find(3)->get().quantity == 2,
              "Existing ranged command also consumes held Thrown units");
        auto restored = rules->restore(combat->save());
        check(restored->save() == combat->save(),
              "Critical thrown choice preserves damage context after weapon leaves hand");
        for (auto *session : {combat.get(), restored.get()})
        {
            act(*session, "savage_use");
            act(*session, "savage_second");
        }
        check(restored->save() == combat->save() && bool(combat->snapshot().free_movement),
              "Savage reroll retains thrown weapon and Champion trigger");
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
                                 [&](const auto &i)
                                 {
                                     return i.id == command.item;
                                 });
                if (item->inventory_id == 3)
                {
                    throw_again = command;
                    break;
                }
            }
        check(combat->submit(throw_again), "Surge can draw and throw another carried unit");
        settle(*combat);
        party.apply_combat(combat->snapshot());
        check(party.member(1).character.inventory().find(3)->get().quantity == 1,
              "Second throw spends another unit from same source stack");
        tested = true;
    }
    check(tested, "Exercise actual critical/Savage/Champion throw sequence");
}

void transfer_and_recovery()
{
    auto rules = module();
    auto draft = hero().creation_data();
    draft.background = "sage";
    CampaignParty party(module());
    Character pc(*srd5::character_rules(), draft, {});
    const auto sword = pc.inventory().add("longsword", "Keep in hand");
    const auto stack = pc.inventory().add("javelin", "Large stack", 1000000);
    const auto first = party.add_pc(std::move(pc));
    party.equip(first, sword);
    const auto second = party.add_pc(Character(*srd5::character_rules(), draft, {}));
    auto actors = party.participants();
    actors[0].cell = {1, 1};
    actors[1].cell = {3, 2};
    actors.push_back({99, "vanguard", "Target", 1, {3, 1}});
    auto combat = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, actors}, 1);
    party.begin_combat();
    party.apply_combat(combat->snapshot());
    while (combat->snapshot().actor != first)
        act(*combat, "end");
    check(combat->snapshot().held_items.size() == 2,
          "Large stack does not allocate one record per unit");
    act(*combat, "throw", 99);
    settle(*combat);
    party.apply_combat(combat->snapshot());
    check(party.member(first).equipped == std::vector<std::uint64_t>{sword},
          "Free hand draws and throws without unnecessarily stowing held weapon");
    check(party.member(first).character.inventory().find(stack)->get().quantity == 999999 &&
              combat->snapshot().held_items.size() == 3,
          "Large stack splits exactly one physical unit");
    const auto state = combat->snapshot();
    auto forged = state;
    ++forged.held_items[0].quantity;
    bool rejected = false;
    try
    {
        party.apply_combat(forged);
    }
    catch (const std::runtime_error &)
    {
        rejected = true;
    }
    check(rejected &&
              party.member(first).character.inventory().find(stack)->get().quantity == 999999,
          "Forged quantity handoff rejects transactionally");
    while (combat->snapshot().actor != second)
        act(*combat, "end");
    act(*combat, "pick_up");
    party.apply_combat(combat->snapshot());
    check(party.member(second).character.inventory().items().size() == 1 &&
              party.member(second).character.inventory().items()[0].quantity == 1 &&
              party.member(second).character.inventory().items()[0].name == "Large stack",
          "Companion pickup preserves source metadata and transfers exactly one unit");
    party.end_combat();
    const auto saved = encode_campaign(party, nullptr, "transfer");
    CampaignParty copy(module());
    copy.restore(
        decode_campaign(saved, *srd5::character_rules(), *rules, "transfer", nullptr).party);
    check(encode_campaign(copy, nullptr, "transfer") == saved,
          "Transferred equipment ownership survives campaign save");

    bool recovered = false;
    for (unsigned seed = 0; seed < 100 && !recovered; ++seed)
    {
        CampaignParty victory(module());
        Character h(*srd5::character_rules(), draft, {});
        auto item = h.inventory().add("javelin", "Recover me", 3);
        auto id = victory.add_pc(std::move(h));
        auto units = victory.participants();
        units[0].cell = {1, 1};
        units.push_back({99, "vanguard", "Weak enemy", 1, {5, 1}, {}, VitalState{1}});
        auto battle = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, units}, seed);
        victory.begin_combat();
        victory.apply_combat(battle->snapshot());
        while (battle->snapshot().actor != id)
            act(*battle, "end");
        act(*battle, "throw", 99);
        if (battle->snapshot().outcome != Outcome::victory)
            continue;
        check(battle->safe_recovery().items.size() == 1,
              "Victory offers reachable thrown unit for safe collection");
        victory.apply_combat(battle->snapshot(), battle->safe_recovery());
        victory.end_combat();
        unsigned quantity = 0;
        for (const auto &i : victory.member(id).character.inventory().items())
            quantity += i.quantity;
        check(quantity == 3 && victory.state().detached_items.empty() &&
                  victory.member(id).character.inventory().find(item)->get().quantity == 2,
              "Safe recovery returns thrown unit to owner while retaining remaining stack ID");
        recovered = true;
    }
    check(recovered, "Victory recovery exercised with an actual lethal throw");
}

void control_fixture()
{
    auto pc = hero();
    auto draft = pc.creation_data();
    draft.background = "sage";
    pc = Character(*srd5::character_rules(), draft, {});
    const auto sword = pc.inventory().add("longsword", "Sword");
    const auto shield = pc.inventory().add("shield", "Shield");
    for (auto weapon : weapons)
        pc.inventory().add(weapon, weapon, 3);
    CampaignParty party(module());
    auto id = party.add_pc(std::move(pc));
    party.equip(id, sword);
    party.equip(id, shield);
    auto actors = party.participants();
    actors.front().cell = {1, 1};
    actors.push_back({2, "vanguard", "Target", 1, {2, 1}});
    actors.push_back({3, "vanguard", "Reserve", 1, {7, 7}});
    auto rules = module();
    auto combat = rules->create({{10, 8, std::vector<std::uint8_t>(80)}, actors}, 1);
    while (combat->snapshot().actor != id)
        act(*combat, "end");
    const auto folder = std::filesystem::path(OPENGOLD_BINARY_DIR) / "thrown-fixtures";
    std::filesystem::create_directories(folder);
    std::ofstream(folder / "before.save") << combat->save();
}

} // namespace

int main(int argc, char **argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--capture-prior-writer")
            capture_prior_writer();
        else
        {
            check(argc == 1, "Unknown test argument");
            prior_writer_continuation();
            physical_inventory();
            critical_stack();
            transfer_and_recovery();
            control_fixture();
        }
        std::cout << "Thrown prior-writer checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
