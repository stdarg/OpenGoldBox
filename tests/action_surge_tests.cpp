#include "opengold/campaign_save.h"
#include "action_budget.h"
#include "campaign_fixture.h"
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
void check(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}

const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

auto module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

Character hero(unsigned level = 1, std::string race = "human", std::string klass = "fighter")
{
    CharacterDraft d;
    d.race = race;
    d.gender = "female";
    d.character_class = klass;
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Surge tester";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    Character c(*srd5::character_rules(), d, {});
    VitalState state;
    for (unsigned n = 1; n < level; ++n)
        check(c.advance(*module(), state), "Advance fixture");
    return c;
}

Command command(const CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &v : c.legal_commands())
        if (v.verb == verb && (!target || target == v.target))
            return v;
    throw std::runtime_error("Missing command: " + std::string(verb));
}

void write(const char *name, const std::string &bytes)
{
    std::ofstream out(root / "tests/fixtures" / name, std::ios::binary);
    out << bytes;
    check(bool(out), "Write fixture");
}

void freeze()
{
    auto rules = module();
    check(rules->identity().version == "0.6.23", "Freeze requires actual prior writer");
    CampaignParty p(module());
    for (auto [level, race] :
            {
                std::pair{2u, "human"}, {1u, "dwarf"}, {4u, "orc"}
            })
    {
        auto h = hero(level, race);
        h.inventory().add("longsword", "Longsword");
        auto id = p.add_pc(std::move(h));
        p.equip(id, 1);
    }
    auto state = p.checkpoint();
    for (auto &m : state.roster)
    {
        m.vitals.hit_points = 1;
        m.wealth[3] = 37;
    }
    state.time_minutes = 123;
    state.subminute_milliseconds = 456;
    state.random_state = 789;
    p.restore(state);
    auto actors = p.participants();
    actors.resize(1);
    actors[0].cell = {1, 1};
    actors.push_back({99, "vanguard", "Enemy", 1, {5, 1}});
    auto c = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, actors}, 2);
    check(c->snapshot().actor == 1, "Fighter begins");
    check(c->submit(command(*c, "second_wind")), "Spend actual Second Wind");
    p.begin_combat();
    p.apply_combat(c->snapshot());
    p.end_combat();
    write("campaign-v11-surge.ogs", encode_campaign(p, nullptr, "surge"));
    write("combat-v13-surge.save", c->save());
    check(c->submit(command(*c, "dash")), "Old Dash continuation");
    write("combat-v13-surge-continued.save", c->save());
}

// Capture with the shipped library before adding Tactical Mind/profile changes.
void freeze_mind_baseline()
{
    auto rules = module();
    check(rules->identity().version == "0.6.42", "Mind baseline requires the actual 0.6.42 writer");
    CampaignParty party(module());
    auto person = hero(2);
    const auto sword = person.inventory().add("longsword", "Prior writer sword");
    const auto id = party.add_pc(std::move(person));
    party.equip(id, sword);
    auto state = party.checkpoint();
    state.roster[0].vitals.hit_points = 1;
    state.roster[0].wealth[3] = 37;
    state.time_minutes = 123;
    state.subminute_milliseconds = 456;
    state.random_state = 789;
    party.restore(state);
    auto people = party.participants();
    check(people[0].character_profile.starts_with("PC28 "),
          "Freeze predates the next profile writer");
    people[0].cell = {1, 1};
    people.push_back({99, "vanguard", "Enemy", 1, {5, 1}});
    auto combat = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, people}, 2);
    check(combat->snapshot().actor == id && combat->submit(command(*combat, "second_wind")),
          "Freeze spends one genuine Second Wind");
    party.begin_combat();
    party.apply_combat(combat->snapshot());
    party.end_combat();
    check(party.member(id).vitals.resources == "SRD1 1 0 0 0 0",
          "Prior writer retains one Second Wind");
    write("campaign-v11-mind-before.ogs", encode_campaign(party, nullptr, "mind-before"));
    write("combat-v15-mind-before.save", combat->save());
    check(combat->submit(command(*combat, "action_surge")) &&
          combat->submit(command(*combat, "dash")),
          "Freeze actual next commands");
    write("combat-v15-mind-continued.save", combat->save());
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
    check(caught, "Invalid state must reject");
}

bool has(const CombatSession &c, std::string_view verb)
{
    for (const auto &v : c.legal_commands())
        if (v.verb == verb)
            return true;
    return false;
}

void act(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    check(c.submit(command(c, verb, target)), "Submit legal command");
}

CombatantView unit(const CombatSession &c, EntityId id = 1)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing actor");
}

std::string read(const char *name)
{
    std::ifstream in(root / "tests/fixtures" / name);
    check(bool(in), "Read fixture");
    return {std::istreambuf_iterator<char>(in), {}};
}

std::uint64_t rng(const CombatSession &c)
{
    std::istringstream in(c.save());
    std::string line;
    for (unsigned i = 0; i < 3; ++i)
        std::getline(in, line);
    std::uint64_t n{};
    in >> n;
    return n;
}

unsigned remaining(const Character &h, const VitalState &state)
{
    for (const auto &p : module()->recovery_info(h.sheet(), state).resources)
        if (p.id == "action_surge")
        {
            check(p.capacity == 1 && p.short_rest_recovery == 1,
                  "Independent level-2–4 pool capacity and recharge");
            return p.remaining;
        }
    return 99;
}

auto battle(const Character &h, std::vector<std::string> gear = {},
std::optional<VitalState> state = {}, unsigned seed = 2)
{
    auto rules = module();
    auto c = rules->create({{10, 8, std::vector<std::uint8_t>(80)},
        {   {
                1,
                "campaign-character",
                "Fighter",
                0,
                {1, 1},
                rules->character_profile(h.sheet(), gear).data,
                state
            },
            {2, "vanguard", "Enemy", 1, {2, 1}},
            {3, "vanguard", "Enemy 2", 1, {8, 1}}
        }},
    seed);
    while (c->snapshot().actor != 1)
        act(*c, "end");
    return c;
}

void budgets()
{
    using opengold::srd5::detail::ActionBudget;
    ActionBudget b;
    check(b.spend(true) && !b.available(), "Ordinary Magic consumes the ordinary action");
    b.surge = true;
    check(b.available() && !b.available(true) && !b.spend(true) && b.surge,
          "Surge cannot fund Magic or be consumed by a rejected Magic action");
    check(b.spend() && !b.available(), "Surge funds exactly one non-Magic action");
    b = {true, true};
    check(b.spend() && b.normal && !b.surge && b.spend(true),
          "Using Surge first preserves the ordinary action for Magic");
    b = {true, true};
    check(b.spend(true) && !b.normal && b.surge && b.spend(),
          "Using Magic first preserves the restricted action");
}

void grants()
{
    auto rules = module();
    for (unsigned level = 1; level <= 4; ++level)
    {
        auto h = hero(level);
        const FeatureGrant expected{"feature:action_surge", "class:fighter", 2, {}};
        check((std::find(h.sheet().grants.begin(), h.sheet().grants.end(), expected) !=
               h.sheet().grants.end()) == (level >= 2),
              "Action Surge is acquired exactly at Fighter level two");
        auto c = battle(h);
        check(has(*c, "action_surge") == (level >= 2), "Only entitled Fighters can activate");
        check(remaining(h, unit(*c).persistent) == (level >= 2 ? 1u : 99u),
              "Source-derived pool only exists at supported levels");
        if (level >= 2)
        {
            auto bad = h.sheet();
            std::erase_if(bad.grants,
                          [](const auto & g)
            {
                return g.id == "feature:action_surge";
            });
            rejects(
                [&]
            {
                (void)rules->character_profile(bad, {});
            });
            bad = h.sheet();
            for (auto &g : bad.grants)
                if (g.id == "feature:action_surge")
                    g.level = 1;
            rejects(
                [&]
            {
                (void)rules->character_profile(bad, {});
            });
            auto profile = rules->character_profile(h.sheet(), {}).data;
            profile.replace(0, 4, "PC12");
            rejects(
                [&]
            {
                (void)rules->create({{8, 8, std::vector<std::uint8_t>(64)},
                    {   {1, "campaign-character", "Forged", 0, {1, 1}, profile},
                        {2, "vanguard", "Enemy", 1, {3, 1}}
                    }},
                2);
            });
        }
    }
    for (const auto &klass : srd5::character_rules()->choices(CreationField::character_class))
        if (klass.id != "fighter")
            check(!has(*battle(hero(1, "human", klass.id)), "action_surge"),
                  "Other starting classes do not gain Action Surge");
}

void actions()
{
    auto rules = module();
    for (unsigned level = 2; level <= 4; ++level)
        for (bool first :
                {
                    false, true
                })
            for (const auto verb :
                    {"dash", "dodge", "disengage", "melee", "ranged"
                    })
            {
                auto h = hero(level);
                auto c = battle(
                             h, {std::string_view(verb) == "ranged" ? "light_crossbow" : "longsword"});
                const auto before = unit(*c);
                const auto random = rng(*c);
                if (!first)
                    act(*c, "dodge");
                const auto ticket = command(*c, "action_surge");
                act(*c, "action_surge");
                check(
                    rng(*c) == random && unit(*c).action &&
                    unit(*c).bonus_action == before.bonus_action &&
                    unit(*c).reaction == before.reaction &&
                    unit(*c).movement_feet == before.movement_feet,
                    "Activation consumes no RNG, ordinary action, Bonus Action, Reaction or movement");
                const auto pending = c->save();
                check(!c->submit(ticket) && c->save() == pending, "Duplicate activation is atomic");
                check(!has(*c, "action_surge") && remaining(h, unit(*c).persistent) == 0,
                      "Feature use is spent immediately");
                auto copy = rules->restore(pending);
                const auto next = command(*c, verb);
                check(c->submit(next) && copy->submit(next) && c->save() == copy->save(),
                      "Pending allowance restores with exact action continuation");
                check(
                    unit(*c).action == first,
                    "Non-Magic action spends the restricted allowance before the ordinary action");
                if (first)
                    act(*c, "dash");
                check(!unit(*c).action, "Exactly two total actions this turn");
                const auto done = c->save();
                check(!c->submit({c->snapshot().revision, 1, 0, "dodge"}) && c->save() == done,
                      "Third action rejects without mutation");
                check(rules->restore(done)->save() == done,
                      "Spent allowance and all action results restore");
            }
    auto h = hero(2, "orc");
    auto c = battle(h);
    act(*c, "action_surge");
    act(*c, "dash");
    act(*c, "dash");
    act(*c, "adrenaline_rush");
    check(unit(*c).movement_feet == 120 && !unit(*c).action && !unit(*c).bonus_action &&
          unit(*c).reaction,
          "Two Dashes and independent Bonus Action Dash grant four speed allowances");
    check(rules->restore(c->save())->save() == c->save(),
          "Four-speed movement checkpoint is valid");
    c = battle(h);
    act(*c, "action_surge");
    act(*c, "end");
    while (c->snapshot().actor != 1)
        act(*c, "end");
    check(!has(*c, "action_surge") && unit(*c).action && unit(*c).movement_feet == 30,
          "Unused extra allowance expires without refund or next-turn carryover");
    act(*c, "dash");
    check(!has(*c, "dodge"), "Next turn has only its normal action");
    c = battle(h);
    act(*c, "action_surge");
    Command move;
    for (const auto &v : c->legal_commands())
        if (v.verb == "move" && v.destination == Cell{0, 1})
            move = v;
    check(c->submit(move) && c->snapshot().reaction_pending,
          "Movement during Surge still provokes");
    check(!has(*c, "action_surge"), "Reaction window cannot activate the feature");
    auto copy = rules->restore(c->save());
    act(*c, "decline");
    act(*copy, "decline");
    check(c->save() == copy->save() && unit(*c).action,
          "Pending reaction preserves both action allowances");
    auto dead = rules->create({{8, 8, std::vector<std::uint8_t>(64)},
        {   {
                1,
                "campaign-character",
                "Dead Fighter",
                0,
                {1, 1},
                rules->character_profile(h.sheet(), {}).data,
                VitalState{0, true, {}}
            },
            {2, "vanguard", "Enemy", 1, {3, 1}}
        }},
    2);
    check(!has(*dead, "action_surge"), "Dead character cannot activate");
}

void attacks_and_malformed()
{
    auto rules = module();
    auto h = hero(2);
    auto c = battle(h, {"longsword"});
    const auto random = rng(*c);
    act(*c, "melee", 2);
    check(unit(*c, 2).hit_points == 24,
          "Independent seed 2: natural 17 hits, d8 2 plus Strength 2 deals four");
    act(*c, "action_surge");
    act(*c, "melee", 2);
    check(
        unit(*c, 2).hit_points == 11 && rng(*c) == random + 5 * 0x9e3779b97f4a7c15ULL,
        "Extra attack rolls independently: natural 20, critical dice 7 and 4 plus Strength 2 deal thirteen");
    c = battle(h);
    act(*c, "action_surge");
    const auto valid = c->save();
    for (const auto suffix :
            {"2 1 1", "-1 1 1", "1 1 1", "0 0 1"
            })
    {
        std::istringstream in(valid);
        std::string edited, line;
        while (std::getline(in, line))
        {
            if (line.starts_with("1 \"campaign-character\""))
            {
                auto at = line.size();
                for (unsigned i = 0; i < 3; ++i)
                    at = line.rfind(' ', at - 1);
                line.replace(at + 1, line.size() - at - 1, suffix);
            }
            edited += line + '\n';
        }
        rejects(
            [&]
        {
            (void)rules->restore(edited);
        });
        check(c->save() == valid, "Malformed allowance cannot mutate the running encounter");
    }
    auto wrong_version = valid;
    wrong_version.replace(9, 2, "13");
    rejects(
        [&]
    {
        (void)rules->restore(wrong_version);
    });
}

void savage()
{
    auto rules = module();
    auto h = hero(4);
    auto state = VitalState{h.sheet().hit_points};
    // Normal level-four feat entitlement, not a forged combat definition.
    h = hero(3);
    state = {h.sheet().hit_points};
    AdvancementChoice choice = rules->default_advancement(h.sheet());
    choice.feat = "savage_attacker";
    choice.abilities = {};
    check(h.advance(*rules, state, choice), "Acquire Savage Attacker");
    bool exercised = false;
    for (unsigned trial = 0; trial < 100 && !exercised; ++trial)
    {
        auto c = battle(h, {"longsword"}, {}, trial);
        act(*c, "action_surge");
        act(*c, "melee", 2);
        if (!c->snapshot().savage_attack_choice)
            continue;
        check(unit(*c).action, "Surge attack leaves normal action during pending damage choice");
        auto copy = rules->restore(c->save());
        act(*c, "savage_skip");
        act(*copy, "savage_skip");
        check(c->save() == copy->save(),
              "Pending Savage decision resumes with the unused ordinary action");
        exercised = true;
    }
    check(exercised, "Actual extra attack exercises pending damage choice");
}

void recovery()
{
    auto rules = module();
    auto h = hero(2);
    auto c = battle(h);
    act(*c, "action_surge");
    auto spent = unit(*c).persistent;
    check(spent.resources.starts_with("SRD8 "), "Spent pool has explicit versioned continuation");
    for (bool long_rest :
            {
                false, true
            })
    {
        auto state = spent;
        if (long_rest)
            rules->recover(state, h.sheet());
        else
            rules->recover_short_rest(state, h.sheet());
        check(remaining(h, state) == 1, "Either completed rest fully recharges one use");
    }
    auto state = spent;
    check(h.advance(*rules, state) && remaining(h, state) == 0,
          "Advancing to level three does not replenish spent Surge");
    check(h.advance(*rules, state) && remaining(h, state) == 0,
          "Advancing to level four does not replenish spent Surge");
    for (const auto value :
            {"-1", "2", "2147483648"
            })
    {
        auto bad = spent;
        const auto at = bad.resources.find(" FX1");
        const auto begin = bad.resources.rfind(' ', at - 1) + 1;
        bad.resources.replace(begin, at - begin, value);
        rejects(
            [&]
        {
            rules->validate_character_state(hero(2).sheet(), bad);
        });
    }
}

void campaign()
{
    auto rules = module();
    auto creation = srd5::character_rules();
    for (bool npc :
            {
                false, true
            })
    {
        CampaignParty p(module());
        auto h = hero();
        h.inventory().add("longsword", "Longsword");
        const auto id = npc ? p.recruit("surge:companion", h) : p.add_pc(h);
        p.equip(id, 1);
        p.award_experience(2700, "surge-xp");
        p.advance(id, p.default_advancement(id));
        auto c = battle(p.member(id).character, {"longsword"});
        act(*c, "action_surge");
        act(*c, "dash");
        p.begin_combat();
        p.apply_combat(c->snapshot());
        p.end_combat();
        check(remaining(p.member(id).character, p.member(id).vitals) == 0,
              "Campaign handoff preserves spent resource");
        const auto bytes = encode_campaign(p, nullptr, "surge");
        CampaignParty copy(module());
        copy.restore(decode_campaign(bytes, *creation, *rules, "surge", nullptr).party);
        check(encode_campaign(copy, nullptr, "surge") == bytes,
              "Campaign replay retains grants, state and history");
        copy.complete_training(
            id, *creation,
        {
            {"origin:languages", {"elvish", "orc"}},
            {"class:fighter:fighting_style", {"archery"}},
            {"class:fighter:weapon_mastery", {"dagger", "longsword", "shortbow"}},
            {"class:fighter", {"athletics", "history"}}
        });
        check(remaining(copy.member(id).character, copy.member(id).vitals) == 0,
              "Training completion does not refund uses");
        copy.advance(id, copy.default_advancement(id));
        check(remaining(copy.member(id).character, copy.member(id).vitals) == 0,
              "Campaign advancement preserves expenditure");
        const auto rested = copy.rest(RestKind::short_rest);
        check(bool(rested), "Rest completes");
        check(remaining(copy.member(id).character, copy.member(id).vitals) == 1,
              "Completed campaign Short Rest recharges the pool");
        const auto rest = copy.state().short_rest;
        check(bool(rest), "Rest spending session exists");
        copy.finish_short_rest(rest->ticket);
    }
}

void legacy()
{
    auto rules = module();
    auto creation = srd5::character_rules();
    auto old = read("campaign-v11-surge.ogs");
    CampaignParty p(module());
    p.restore(decode_campaign(old, *creation, *rules, "surge", nullptr).party);
    for (unsigned id = 1; id <= 3; ++id)
    {
        const auto &m = p.member(id);
        check(m.wealth[3] == 37 && m.equipped == std::vector<std::uint64_t> {1},
              "Old equipment and wealth remain");
        check(
            remaining(m.character, m.vitals) == (id == 2 ? 99u : 1u),
            "Prior attained levels gain only their justified, previously unspendable Surge entitlement");
    }
    check(p.state().random_state == 789 && p.state().time_minutes == 123 &&
          p.state().subminute_milliseconds == 456,
          "Migration preserves RNG and clock");
    check(p.member(1).vitals.resources == "SRD1 1 0 0 0 0",
          "Spent Second Wind bytes remain unchanged");
    auto upgraded = encode_campaign(p, nullptr, "surge");
    auto body = [](const std::string & bytes)
    {
        return bytes.substr(bytes.find('\n', bytes.find('\n') + 1) + 1);
    };
    auto expected = body(old);
    const auto identity = expected.find("0.6.23");
    check(identity != expected.npos, "Frozen campaign identity exists");
    expected.replace(identity, 6, rules->identity().version);
    check(
        body(upgraded) == test::with_background_training_grants(
            test::with_action_surge_grants(expected, {true, false, true})),
        "Every old campaign byte is retained except module identity and justified Surge/background grants");
    CampaignParty again(module());
    again.restore(decode_campaign(upgraded, *creation, *rules, "surge", nullptr).party);
    check(encode_campaign(again, nullptr, "surge") == upgraded, "Migration is canonical");
    auto version = [&](std::string bytes)
    {
        const auto at = bytes.find("0.6.23");
        check(at != bytes.npos, "Prior identity exists");
        bytes.replace(at, 6, rules->identity().version);
        return bytes;
    };
    auto c = rules->restore(read("combat-v13-surge.save"));
    check(
        c->save() == version(read("combat-v13-surge.save")) && !has(*c, "action_surge"),
        "Old in-flight combat retains its recorded feature access without inventing an allowance");
    act(*c, "dash");
    check(c->save() == version(read("combat-v13-surge-continued.save")),
          "Actual old Dash continuation stays byte-exact apart from identity");
}

void mind_prior_writer()
{
    auto rules = module();
    const auto normalize = [&](std::string bytes)
    {
        const auto pos = bytes.find("0.6.42");
        check(pos != bytes.npos, "Frozen Mind baseline identity exists");
        bytes.replace(pos, 6, rules->identity().version);
        return bytes;
    };
    auto combat = rules->restore(read("combat-v15-mind-before.save"));
    check(combat->save() == normalize(read("combat-v15-mind-before.save")),
          "Pre-Mind combat preserves the actual old writer");
    act(*combat, "action_surge");
    act(*combat, "dash");
    check(combat->save() == normalize(read("combat-v15-mind-continued.save")),
          "Pre-Mind actions, resources, movement and RNG continue byte-exactly");
    CampaignParty party(module());
    party.restore(decode_campaign(read("campaign-v11-mind-before.ogs"), *srd5::character_rules(),
                                  *rules, "mind-before", nullptr)
                  .party);
    check(party.member(1).character.sheet().level == 2 &&
          party.member(1).vitals.resources == "SRD1 1 0 0 0 0" &&
          party.member(1).wealth[3] == 37 &&
          party.member(1).equipped == std::vector<std::uint64_t> {1},
          "Pre-Mind campaign retains level, spent resources, wealth and equipment");
    check(party.state().time_minutes == 123 && party.state().subminute_milliseconds == 456 &&
          party.state().random_state == 789,
          "Pre-Mind campaign retains its clock and RNG");
    const auto canonical = encode_campaign(party, nullptr, "mind-before");
    CampaignParty copy(module());
    copy.restore(
        decode_campaign(canonical, *srd5::character_rules(), *rules, "mind-before", nullptr).party);
    check(encode_campaign(copy, nullptr, "mind-before") == canonical,
          "Pre-Mind campaign migration is canonical");
}

// Actual pre-Champion writer, never run against a later module.
void freeze_champion_baseline()
{
    auto rules = module();
    check(rules->identity().version == "0.6.45", "Champion capture requires actual prior writer");
    CampaignParty party(module());
    for (unsigned level = 1; level <= 4; ++level)
    {
        auto h = hero(level);
        auto item = h.inventory().add("longsword", "Champion baseline sword");
        auto id = party.add_pc(std::move(h));
        party.equip(id, item);
    }
    auto state = party.checkpoint();
    for (auto &m : state.roster)
    {
        m.vitals.hit_points = 1;
        m.wealth[3] = 37;
    }
    state.time_minutes = 123;
    state.subminute_milliseconds = 456;
    state.random_state = 789;
    party.restore(state);
    write("campaign-v11-champion-before.ogs", encode_campaign(party, nullptr, "champion-before"));
    auto h = hero(3);
    VitalState health{h.sheet().hit_points};
    auto choice = rules->default_advancement(h.sheet());
    choice.feat = "savage_attacker";
    choice.abilities = {};
    check(h.advance(*rules, health, choice), "Baseline ordinary feat acquisition");
    auto c = battle(h, {"longsword"});
    act(*c, "melee", 2);
    check(bool(c->snapshot().savage_attack_choice), "Baseline pending Savage choice");
    write("combat-v14-champion-before.save", c->save());
    act(*c, "savage_skip");
    act(*c, "action_surge");
    act(*c, "melee", 2);
    check(c->snapshot().savage_attack_choice && c->snapshot().savage_attack_choice->critical,
          "Baseline real critical hit");
    write("combat-v14-champion-critical.save", c->save());
    act(*c, "savage_use");
    act(*c, "savage_second");
    write("combat-v14-champion-resolved.save", c->save());
}

void champion_prior_writer()
{
    auto rules = module();
    const auto normalize = [&](std::string bytes)
    {
        const auto pos = bytes.find("0.6.45");
        check(pos != bytes.npos, "Frozen Champion identity exists");
        bytes.replace(pos, 6, rules->identity().version);
        return bytes;
    };
    auto c = rules->restore(read("combat-v14-champion-before.save"));
    check(c->save() == normalize(read("combat-v14-champion-before.save")),
          "Pre-Champion pending weapon choice round trips exactly");
    act(*c, "savage_skip");
    act(*c, "action_surge");
    act(*c, "melee", 2);
    check(c->save() == normalize(read("combat-v14-champion-critical.save")),
          "Pre-Champion critical retains prior damage and timing");
    act(*c, "savage_use");
    act(*c, "savage_second");
    check(c->save() == normalize(read("combat-v14-champion-resolved.save")),
          "Prior writer continues exactly without gaining free movement");
    CampaignParty party(module());
    party.restore(decode_campaign(read("campaign-v11-champion-before.ogs"),
                                  *srd5::character_rules(), *rules, "champion-before", nullptr)
                  .party);
    for (unsigned id = 1; id <= 4; ++id)
        check(party.member(id).character.sheet().level == id &&
              party.member(id).vitals.hit_points == 1 && party.member(id).wealth[3] == 37,
              "Prior Fighter levels retain wounds, level and wealth");
    const auto canonical = encode_campaign(party, nullptr, "champion-before");
    CampaignParty copy(module());
    copy.restore(
        decode_campaign(canonical, *srd5::character_rules(), *rules, "champion-before", nullptr)
        .party);
    check(encode_campaign(copy, nullptr, "champion-before") == canonical,
          "Prior Fighter campaign migrates canonically");
}

void ui_fixtures()
{
    const auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "surge-fixtures";
    std::filesystem::create_directories(path);
    std::ofstream(path / "level1.save", std::ios::binary) << battle(hero(1))->save();
    std::ofstream(path / "cleric.save", std::ios::binary)
            << battle(hero(1, "human", "cleric"))->save();
    for (unsigned level = 3; level <= 4; ++level)
        std::ofstream(path / ("level" + std::to_string(level) + ".save"), std::ios::binary)
                << battle(hero(level, "orc"), {"longsword"})->save();
    auto h = hero(3);
    auto state = VitalState{h.sheet().hit_points};
    auto choice = module()->default_advancement(h.sheet());
    choice.feat = "savage_attacker";
    choice.abilities = {};
    check(h.advance(*module(), state, choice), "UI feat fixture");
    bool captured = false;
    for (unsigned seed = 0; seed < 100 && !captured; ++seed)
    {
        auto pending = battle(h, {"longsword"}, {}, seed);
        act(*pending, "melee", 2);
        if (!pending->snapshot().savage_attack_choice)
            continue;
        std::ofstream(path / "decision.save", std::ios::binary) << pending->save();
        captured = true;
    }
    check(captured, "UI pending damage choice");
    auto c = battle(hero(2), {"longsword"});
    std::ofstream(path / "available.save", std::ios::binary) << c->save();
    act(*c, "action_surge");
    std::ofstream(path / "pending.save", std::ios::binary) << c->save();
}

} // namespace

int main(int argc, char **argv)
{
    try
    {
        if (argc == 2)
        {
            if (std::string_view(argv[1]) == "--freeze-champion-baseline")
                freeze_champion_baseline();
            else if (std::string_view(argv[1]) == "--freeze-mind-baseline")
                freeze_mind_baseline();
            else
                freeze();
            return 0;
        }
        auto run = [](const char *name, auto test)
        {
            try
            {
                test();
            }
            catch (const std::exception &e)
            {
                throw std::runtime_error(std::string(name) + ": " + e.what());
            }
        };
        run("budgets", budgets);
        run("grants", grants);
        run("actions", actions);
        run("attacks and malformed", attacks_and_malformed);
        run("savage", savage);
        run("recovery", recovery);
        run("campaign", campaign);
        run("legacy", legacy);
        run("mind prior writer", mind_prior_writer);
        run("champion prior writer", champion_prior_writer);
        ui_fixtures();
        std::cout << "Action Surge tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
