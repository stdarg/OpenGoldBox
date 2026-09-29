#include "opengold/campaign_save.h"
#include "opengold/combat_demo.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace opengold;
using namespace opengold::rules;

namespace
{
void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

template <class F> void rejects(F f)
{
    bool caught = false;
    try
    {
        f();
    }
    catch (const std::exception &)
    {
        caught = true;
    }
    check(caught, "Malformed state must reject");
}

std::string read(const std::filesystem::path &path)
{
    std::ifstream in(path);
    check(bool(in), "Fixture exists");
    return {std::istreambuf_iterator<char>(in), {}};
}

auto module()
{
    return srd5::load(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "data/rules/srd-5.2.1/combat.rules");
}

Character hero(std::string klass = "fighter", std::string race = "orc")
{
    CharacterDraft d;
    d.race = race;
    d.gender = "female";
    d.character_class = klass;
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Orc tester";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    return Character(*srd5::character_rules(), d, {});
}

ResourcePool pool(const Character &c, const VitalState &state)
{
    for (auto p : module()->recovery_info(c.sheet(), state).resources)
        if (p.id == "adrenaline_rush")
            return p;
    return {};
}

CombatantView unit(const CombatSession &c, EntityId id = 1)
{
    for (auto a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing actor");
}

Command command(const CombatSession &c, std::string_view verb)
{
    for (auto a : c.legal_commands())
        if (a.verb == verb)
            return a;
    throw std::runtime_error("Missing command: " + std::string(verb));
}

bool has(const CombatSession &c, std::string_view verb)
{
    for (auto a : c.legal_commands())
        if (a.verb == verb)
            return true;
    return false;
}

void act(CombatSession &c, std::string_view verb)
{
    check(c.submit(command(c, verb)), "Command accepted");
}

auto battle(const Character &c, TemporaryHitPoints temporary = {})
{
    auto rules = module();
    VitalState state{c.sheet().hit_points - 1};
    if (temporary.amount)
        rules->grant_temporary_hit_points(state, c.sheet(), temporary, TemporaryHpChoice::use_new);
    Encounter e{{20, 20, std::vector<std::uint8_t>(400)},
        {   {
                1,
                "campaign-character",
                c.sheet().name,
                 0,
                {2, 2},
                rules->character_profile(c.sheet(), {}).data,
                state
            },
            {99, "vanguard", "Opponent", 1, {18, 18}}
        }};
    auto combat = rules->create(e, 42);
    while (combat->snapshot().actor != 1)
        act(*combat, "end");
    return combat;
}

void next_turn(CombatSession &c)
{
    act(c, "end");
    while (c.snapshot().actor != 1)
        act(c, "end");
}

void ordinary_classes()
{
    auto rules = module();
    for (const auto &klass : srd5::character_rules()->choices(CreationField::character_class))
    {
        const auto c = hero(klass.id);
        auto combat = battle(c);
        const auto before = unit(*combat);
        const auto rush = command(*combat, "adrenaline_rush");
        check(pool(c, before.persistent).remaining == 2, "All twelve classes start with PB uses");
        check(std::any_of(c.sheet().grants.begin(), c.sheet().grants.end(),
                          [](const auto & g)
        {
            return g.id == "trait:adrenaline_rush" &&
                   g.source_id == "species:orc" && g.level == 1;
        }),
        "Orc entitlement has species provenance");
        check(combat->submit(rush), "Normally created Orc can use Adrenaline Rush");
        const auto after = unit(*combat);
        check(!after.bonus_action && after.action &&
              after.movement_feet == before.movement_feet + 30 &&
              after.hit_points == before.hit_points &&
              after.temporary_hp == TemporaryHitPoints{2, "species:orc/trait:adrenaline_rush"},
              "Bonus Action Dash grants PB Temporary HP without healing or using the action");
        check(pool(c, after.persistent).remaining == 1 && !combat->snapshot().temporary_hp_offer &&
              !has(*combat, "adrenaline_rush"),
              "Empty pool autoaccepts and consumes one use");
        const auto saved = combat->save();
        check(!combat->submit(rush) && combat->save() == saved,
              "Stale grant cannot duplicate movement or HP");
        act(*combat, "dash");
        check(unit(*combat).movement_feet == 90 && !unit(*combat).action,
              "Normal Dash and Bonus Action Dash combine");
        check(rules->restore(combat->save())->save() == combat->save(),
              "Both Dash budgets survive restore");
        next_turn(*combat);
        check(unit(*combat).movement_feet == 30,
              "New turn resets only its movement/Bonus Action budget");
        act(*combat, "adrenaline_rush");
        check(bool(combat->snapshot().temporary_hp_offer),
              "Second grant requires an explicit decision");
        act(*combat, "temp_hp_keep");
        next_turn(*combat);
        check(!has(*combat, "adrenaline_rush") && pool(c, unit(*combat).persistent).remaining == 0,
              "Exhausted uses stay spent next turn");
        auto state = unit(*combat).persistent;
        rules->recover_short_rest(state, c.sheet());
        check(pool(c, state).remaining == 2 &&
              rules->recovery_info(c.sheet(), state).temporary_hp.amount == 2,
              "Short Rest fully recharges uses and retains Temporary HP");
        rules->recover(state, c.sheet());
        check(pool(c, state).remaining == 2 &&
              rules->recovery_info(c.sheet(), state).temporary_hp.amount == 0,
              "Long Rest recharges and expires Temporary HP");
    }
    auto human = battle(hero("fighter", "human"));
    check(!has(*human, "adrenaline_rush"), "Other species do not gain the trait");
    auto c = hero();
    auto combat = battle(c);
    act(*combat, "second_wind");
    check(!has(*combat, "adrenaline_rush"), "Second Wind competes for the Bonus Action");
    combat = battle(c);
    act(*combat, "adrenaline_rush");
    check(!has(*combat, "second_wind"), "Adrenaline Rush spends the shared Bonus Action");
}

void decisions()
{
    auto rules = module();
    const auto c = hero();
    for (int previous :
            {
                1, 2, 7
            })
        for (const auto verb :
                {"temp_hp_keep", "temp_hp_use"
                })
        {
            auto combat = battle(c, {previous, "spell:fixture"});
            const auto stale = command(*combat, "dash");
            act(*combat, "adrenaline_rush");
            const auto snapshot = combat->snapshot();
            check(snapshot.temporary_hp_offer &&
                  snapshot.temporary_hp_offer->current ==
                  TemporaryHitPoints{previous, "spell:fixture"} &&
                  snapshot.temporary_hp_offer->offered.amount == 2,
                  "Decision exposes both amounts and sources");
            check(combat->legal_commands().size() == 2 && combat->movement_reach(1).empty(),
                  "All other actions/movement wait for the choice");
            const auto pending = combat->save();
            check(!combat->submit(stale) && combat->save() == pending,
                  "Pre-offer ticket cannot bypass replacement");
            auto copy = rules->restore(pending);
            check(copy->save() == pending,
                  "Pending choice roundtrips exactly without RNG/time/refund");
            const auto choice = command(*combat, verb);
            check(combat->submit(choice) && copy->submit(choice) && combat->save() == copy->save(),
                  "Restored choice has identical continuation");
            const auto after = unit(*combat);
            check(after.temporary_hp ==
                  (std::string_view(verb) == "temp_hp_keep"
                   ? TemporaryHitPoints{previous, "spell:fixture"}
                   : TemporaryHitPoints{2, "species:orc/trait:adrenaline_rush"}),
                  "Explicit smaller/equal/larger choice never stacks");
            check(pool(c, after.persistent).remaining == 1 && !after.bonus_action && after.action &&
                  after.movement_feet == 60,
                  "Both choices retain all spent costs");
            const auto saved = combat->save();
            check(!combat->submit(choice) && combat->save() == saved,
                  "Resolved choice ticket cannot apply twice");
            auto corrupt = pending;
            const auto at = corrupt.rfind("2 \"species:orc/trait:adrenaline_rush\"");
            check(at != corrupt.npos, "Pending offer is serialized");
            corrupt.replace(at, 1, "3");
            rejects(
                [&]
            {
                (void)rules->restore(corrupt);
            });
        }
    auto combat = battle(c, {7, "spell:fixture"});
    act(*combat, "adrenaline_rush");
    check(choose_demo_command(*combat).verb == "temp_hp_keep",
          "Automated combat can resolve its own replacement decision");
    // Inputs for the real Godot Load/Save/buttons test: created through ordinary character rules.
    const auto directory = std::filesystem::path(OPENGOLD_BINARY_DIR) / "adrenaline-fixtures";
    std::filesystem::create_directories(directory);
    std::ofstream(directory / "adrenaline-pending.save", std::ios::binary) << combat->save();
    auto initial = battle(c);
    std::ofstream(directory / "adrenaline-initial.save", std::ios::binary) << initial->save();
    const auto data = unit(*combat).persistent;
    check(data.resources.starts_with("SRD7 "), "Orc uses persist even with other-source HP");
    for (const auto remaining :
            {"-1", "3", "2147483648"
            })
    {
        auto bad = data;
        auto at = bad.resources.find(" FX1");
        check(at != bad.resources.npos, "Effect boundary");
        auto start = bad.resources.rfind(' ', at - 1);
        bad.resources.replace(start + 1, at - start - 1, remaining);
        rejects(
            [&]
        {
            rules->validate_character_state(c.sheet(), bad);
        });
    }
}

void movement()
{
    auto rules = module();
    const auto c = hero();
    Encounter e{{10, 10, std::vector<std::uint8_t>(100)},
        {   {
                1,
                "campaign-character",
                c.sheet().name,
                 0,
                {2, 2},
                rules->character_profile(c.sheet(), {}).data
            },
            {99, "vanguard", "Opponent", 1, {3, 2}}
        }};
    auto combat = rules->create(e, 42);
    while (combat->snapshot().actor != 1)
        act(*combat, "end");
    act(*combat, "adrenaline_rush");
    const auto offered = combat->legal_commands();
    const auto move = std::find_if(offered.begin(), offered.end(),
                                   [](const auto & c)
    {
        return c.verb == "move" && c.destination == Cell{0, 2};
    });
    check(move != offered.end() && combat->submit(*move) && combat->snapshot().reaction_pending,
          "Bonus Dash movement still provokes opportunity attacks");
    check(!has(*combat, "adrenaline_rush") && !combat->snapshot().temporary_hp_offer,
          "Opportunity choice cannot activate another Bonus Action or replacement");
    auto copy = rules->restore(combat->save());
    act(*combat, "decline");
    act(*copy, "decline");
    check(combat->save() == copy->save() && unit(*combat).movement_feet == 50 &&
          unit(*combat).temporary_hp.amount == 2,
          "Restored opportunity decline spends only remaining movement and preserves pool");
}

void campaign()
{
    auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(hero()), reserve = party.add_pc(hero());
    party.remove(reserve);
    auto combat = battle(party.member(id).character);
    act(*combat, "adrenaline_rush");
    party.begin_combat();
    party.apply_combat(combat->snapshot());
    party.end_combat();
    check(pool(party.member(id).character, party.member(id).vitals).remaining == 1,
          "Encounter updates campaign use count");
    auto bytes = encode_campaign(party, nullptr, "adrenaline");
    CampaignParty copy(module());
    copy.restore(
        decode_campaign(bytes, *srd5::character_rules(), *rules, "adrenaline", nullptr).party);
    check(encode_campaign(copy, nullptr, "adrenaline") == bytes,
          "Campaign save/load preserves pool and spent use");
    copy.complete_training(id, *srd5::character_rules(),
    {
        {"origin:languages", {"elvish", "orc"}},
        {"class:fighter:fighting_style", {"archery"}},
        {"class:fighter:weapon_mastery", {"dagger", "longsword", "shortbow"}},
        {"class:fighter", {"athletics", "history"}},
        {"background:soldier:gaming_set", {"dice"}}
    });
    copy.award_experience(900, "rush-xp");
    copy.advance(id, copy.default_advancement(id));
    check(pool(copy.member(id).character, copy.member(id).vitals).remaining == 1,
          "Training and level growth do not refill spent uses");
    auto actors = copy.participants();
    actors.push_back({99, "vanguard", "Opponent", 1, {18, 18}});
    auto second = rules->create({{20, 20, std::vector<std::uint8_t>(400)}, actors}, 42);
    check(unit(*second).temporary_hp.amount == 2 &&
          pool(copy.member(id).character, unit(*second).persistent).remaining == 1,
          "Next encounter inherits buffer and expenditure");
    check(bool(copy.rest(RestKind::short_rest)), "Campaign Short Rest completes");
    check(pool(copy.member(id).character, copy.member(id).vitals).remaining == 2,
          "Campaign recharge uses the shared rest operation");
}

void legacy()
{
    auto rules = module();
    const auto path = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures";
    const auto before = read(path / "combat-v11-adrenaline.save");
    auto combat = rules->restore(before);
    check(combat->snapshot().reaction_pending, "Old writer's opportunity queue survives");
    check(unit(*combat).temporary_hp == TemporaryHitPoints{7, "spell:fixture"},
          "Orc migration preserves old Temporary HP");
    check(unit(*combat).resources.at(0).remaining == 2,
          "Old Orc starts only the newly introduced resource at capacity");
    act(*combat, "decline");
    auto expected = rules->restore(read(path / "combat-v11-adrenaline-continued.save"));
    check(combat->save() == expected->save(),
          "Frozen old writer has identical movement, RNG, clocks and resources after continuation");
    CampaignParty party(module());
    party.restore(decode_campaign(read(path / "campaign-v10-adrenaline.ogs"),
                                  *srd5::character_rules(), *rules, "adrenaline-fixture", nullptr)
                  .party);
    const auto &c = party.member(1);
    check(c.vitals.hit_points == 0 && !c.vitals.dead &&
          rules->recovery_info(c.character.sheet(), c.vitals).temporary_hp.amount == 7 &&
          pool(c.character, c.vitals).remaining == 2,
          "Legacy campaign Orc retains mortality/resources/pool and gains the new use capacity");
    check(c.vitals.resources == "SRD7 1 0 0 0 0 1 1 0 4321000 7 \"spell:fixture\" 2 FX1 1 0",
          "Legacy migration preserves exact spent Wind, die and recovery deadline");
    const auto bytes = encode_campaign(party, nullptr, "adrenaline-fixture");
    CampaignParty again(module());
    again.restore(
        decode_campaign(bytes, *srd5::character_rules(), *rules, "adrenaline-fixture", nullptr)
        .party);
    check(encode_campaign(again, nullptr, "adrenaline-fixture") == bytes,
          "Migration becomes canonical and never recharges a current save");
}
} // namespace

int main()
{
    try
    {
        ordinary_classes();
        decisions();
        movement();
        campaign();
        legacy();
        std::cout << "Adrenaline Rush tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
