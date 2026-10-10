#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "feature_grants.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace opengold;
using namespace opengold::rules;

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const char *why)
{
    if (!ok)
        throw std::runtime_error(why);
}

template <class F> void rejects(F f)
{
    bool bad = false;
    try
    {
        f();
    }
    catch (const std::exception &)
    {
        bad = true;
    }
    check(bad, "Invalid state must reject");
}

std::string read(const std::filesystem::path &p)
{
    std::ifstream in(p);
    check(bool(in), "Read fixture");
    return {std::istreambuf_iterator<char>(in), {}};
}

auto module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

auto custom()
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature patient 10 100 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

Character hero(std::string klass = "fighter", unsigned level = 2, bool trained = false)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = klass;
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Medic";
    d.rolled = true;
    for (auto &roll : d.rolls)
        roll = {{6, 5, 4, 1}, 3};
    if (trained)
        d.training["class:" + klass] = {"medicine"};
    Character h(*srd5::character_rules(), d, {});
    VitalState vitals;
    for (unsigned n = 1; n < level; ++n)
        check(h.advance(*module(), vitals), "Ordinary advancement");
    return h;
}

CombatantView unit(const CombatSession &c, EntityId id = 1)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing actor");
}

Command cmd(const CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &a : c.legal_commands())
        if (a.verb == verb && (!target || a.target == target))
            return a;
    throw std::runtime_error("Missing command: " + std::string(verb));
}

void act(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    check(c.submit(cmd(c, verb, target)), "Legal command accepted");
}

bool has(const CombatSession &c, std::string_view verb)
{
    for (const auto &a : c.legal_commands())
        if (a.verb == verb)
            return true;
    return false;
}

std::uint64_t random(const CombatSession &c)
{
    std::istringstream in(c.save());
    std::string line;
    for (int i = 0; i < 3; ++i)
        std::getline(in, line);
    std::uint64_t value{};
    in >> value;
    return value;
}

bool stable(const CombatSession &c)
{
    return unit(c, 2).status.find("Stable") != std::string::npos;
}

auto battle(const RulesModule &rules, const Character &h, unsigned seed = 0, Cell target = {2, 1},
Side side = Side::party)
{
    auto c = rules.create(
    {
        {8, 8, std::vector<Terrain>(64)},
        {   {
                1,
                "campaign-character",
                "Medic",
                Side::party,
                {1, 1},
                rules.character_profile(h.sheet(), std::vector<std::string>{"quarterstaff"}).data
            },
            {
                2,
                "patient",
                "Patient",
                side,
                target,
                {},
                VitalState{0, false, "SRD11 0 0 0 1 1 0 0 6000 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0"}
            },
            {99, "vanguard", "Enemy", Side::opposition, {6, 6}}
        }},
    seed);
    while (c->snapshot().actor != 1)
        act(*c, "end");
    return c;
}

void grants()
{
    auto rules = module();
    for (unsigned level = 1; level <= 4; ++level)
    {
        auto h = hero("fighter", level);
        const bool mind = level >= 2;
        check(srd5::detail::has_grant(h.sheet().grants, "feature:tactical_mind") == mind,
              "Tactical Mind attained from level two");
        auto profile = rules->character_profile(h.sheet(), {}).data;
        check(profile.starts_with("PC42 "), "Every profile uses the current profile tag");
        if (mind)
        {
            profile.replace(0, 4, "PC29");
            rejects(
                [&]
            {
                (void)rules->create({{8, 8, std::vector<Terrain>(64)},
                    {   {1, "campaign-character", "Forged", Side::party, {1, 1}, profile},
                        {2, "vanguard", "Enemy", Side::opposition, {5, 5}}
                    }},
                1);
            });
        }
    }
}

void outcomes()
{
    auto rules = custom();
    constexpr std::uint64_t increment = 0x9e3779b97f4a7c15ULL;
    for (unsigned level = 1; level <= 4; ++level)
    {
        auto h = hero("fighter", level);
        bool success = false, failure = false, boosted = false, still_failed = false, one = false,
             twenty = false;
        for (unsigned seed = 0; seed < 160; ++seed)
        {
            auto c = battle(*rules, h, seed);
            if (!has(*c, "stabilize"))
                continue;
            const auto before = unit(*c);
            const auto rng = random(*c);
            auto original = rules->restore(c->save());
            const auto ticket = cmd(*c, "stabilize", 2);
            act(*c, "stabilize", 2);
            check(!unit(*c).action && unit(*c).bonus_action &&
                  unit(*c).hit_points == before.hit_points,
                  "Help spends Action only and never heals actor");
            const auto check_choice = c->snapshot().ability_check_choice;
            if (check_choice)
            {
                failure = true;
                check(level >= 2 && c->legal_commands().size() == 2 && c->movement_reach(1).empty(),
                      "Only check decisions legal while pending");
                check(check_choice->modifier == (h.sheet().scores[Ability::wisdom] - 10) / 2 &&
                      check_choice->difficulty == 10,
                      "Actual Wisdom and fixed DC");
                auto saved = c->save();
                check(!c->submit(ticket) && !c->submit({c->snapshot().revision, 1, 0, "end"}) &&
                      c->save() == saved,
                      "Stale and unrelated commands are atomic while pending");
                auto malformed = saved;
                auto tail = malformed.rfind('\n', malformed.size() - 2);
                malformed.replace(tail + 1, malformed.size() - tail - 1, "1 999 1 0\n");
                rejects(
                    [&]
                {
                    (void)rules->restore(malformed);
                });
                malformed = saved;
                malformed.replace(malformed.find(module()->identity().version), 6, "0.6.44");
                rejects(
                    [&]
                {
                    (void)rules->restore(malformed);
                });
                auto copy = rules->restore(saved);
                check(copy->save() == saved, "Pending choice round trip is canonical");
                auto decline = rules->restore(saved);
                act(*decline, "mind_skip");
                check(!stable(*decline) && random(*decline) == rng + increment,
                      "Decline spends no d10, recovery roll or Second Wind");
                check(unit(*decline, 2).persistent == unit(*original, 2).persistent,
                      "Decline preserves target mortality and recovery timing exactly");
                act(*c, "mind_use");
                act(*copy, "mind_use");
                check(c->save() == copy->save(), "Boost choice continuation is byte exact");
                check(random(*c) == rng + increment * (stable(*c) ? 3 : 2),
                      "Exactly check, d10 and only-success recovery rolls");
                check(unit(*c).hit_points == before.hit_points && unit(*c).bonus_action,
                      "Tactical Mind neither heals nor uses Bonus Action");
                boosted |= stable(*c);
                still_failed |= !stable(*c);
                if (!stable(*c))
                    check(unit(*c, 2).persistent == unit(*original, 2).persistent,
                          "Failed boost preserves original target state");
                auto expected = unit(*decline).persistent.resources;
                if (stable(*c))
                {
                    auto at = expected.find(' ');
                    expected.replace(at + 1, 1, std::to_string((level == 4 ? 3 : 2) - 1));
                }
                check(unit(*c).persistent.resources == expected,
                      "Second Wind spent exactly on boosted success");
            }
            else
            {
                success |= stable(*c);
                failure |= !stable(*c);
                check(random(*c) == rng + increment * (stable(*c) ? 2 : 1),
                      "Unboosted check rolls recovery only on success");
            }
            for (const auto &message : c->snapshot().log_messages())
                for (const auto &arg : message.arguments)
                    if (arg.name == "roll")
                    {
                        if (arg.value == "1")
                            one = true;
                        if (arg.value == "20")
                            twenty = true;
                    }
            if (stable(*c))
                check(unit(*c, 2).hit_points == 0 && !has(*c, "stabilize"),
                      "Success stabilizes without healing or waking");
        }
        check(
            success && failure && one && twenty && (level == 1 || (boosted && still_failed)),
            "Independent success, failure, d10 branches and natural extremes exercised at every Fighter level");
    }
}

void all_classes()
{
    auto rules = custom();
    for (const auto *klass :
            {"barbarian", "bard", "cleric", "druid", "fighter", "monk", "paladin",
             "ranger", "rogue", "sorcerer", "warlock", "wizard"
            })
    {
        const unsigned max_level = std::string(klass) == "wizard" || std::string(klass) == "cleric"
                                   ? 4
                                   : std::string(klass) == "rogue" ? 2
                                   : 1;
        for (unsigned level = 1; level <= max_level; ++level)
        {
            auto h = hero(klass, level);
            auto c = battle(*rules, h);
            check(has(*c, "stabilize"),
                  "All twelve classes at supported levels can attempt stabilization");
            act(*c, "stabilize", 2);
            check(!c->snapshot().ability_check_choice,
                  "Other classes/level one cannot use Tactical Mind");
        }
    }
    for (const auto *klass :
            {"bard", "cleric", "druid", "paladin"
            })
    {
        auto h = hero(klass, 1, true);
        auto c = battle(*rules, h);
        act(*c, "stabilize", 2);
        bool saw = false;
        for (const auto &m : c->snapshot().log_messages())
            for (const auto &a : m.arguments)
                if (a.name == "modifier")
                {
                    const int wisdom = h.sheet().scores[Ability::wisdom];
                    check(a.value == std::to_string(2 + (wisdom - 10) / 2),
                          "Medicine proficiency is used");
                    saw = true;
                }
        check(saw, "Medicine log includes modifier");
    }
}

void legality_and_surge()
{
    auto rules = custom();
    auto h = hero();
    auto far = battle(*rules, h, 0, {3, 1});
    check(!has(*far, "stabilize"), "Stabilize requires adjacent target");
    auto enemy = battle(*rules, h, 0, {2, 1}, Side::opposition);
    check(has(*enemy, "stabilize"), "Dying enemies are legal targets");
    bool covered = false;
    for (unsigned seed = 0; seed < 100 && !covered; ++seed)
    {
        auto c = battle(*rules, h, seed);
        act(*c, "action_surge");
        act(*c, "stabilize", 2);
        if (!c->snapshot().ability_check_choice)
            continue;
        check(unit(*c).action, "Help uses restricted Surge first, preserving ordinary Action");
        auto copy = rules->restore(c->save());
        act(*c, "mind_skip");
        act(*copy, "mind_skip");
        check(c->save() == copy->save() && has(*c, "stabilize"),
              "Surge-backed decision retains original Action across reload");
        covered = true;
    }
    check(covered, "Surge-backed failed check exercised");
}

void campaign_and_rest()
{
    auto rules = module();
    for (unsigned level = 2; level <= 4; ++level)
    {
        bool covered = false;
        for (unsigned seed = 0; seed < 120 && !covered; ++seed)
        {
            CampaignParty party(module());
            auto medic = hero("fighter", level);
            const auto id = party.add_pc(medic);
            const auto patient = party.add_pc(hero("wizard", 1));
            auto state = party.checkpoint();
            rules->set_hit_points(state.roster[1].vitals, state.roster[1].character.sheet(), 0);
            party.restore(state);
            auto actors = party.participants();
            actors[0].cell = {1, 1};
            actors[1].cell = {2, 1};
            actors.push_back({99, "vanguard", "Enemy", Side::opposition, {6, 6}});
            auto c = rules->create({{8, 8, std::vector<Terrain>(64)}, actors}, seed);
            while (c->snapshot().actor != id)
                act(*c, "end");
            if (!has(*c, "stabilize"))
                continue;
            act(*c, "stabilize", patient);
            if (!c->snapshot().ability_check_choice)
                continue;
            act(*c, "mind_use");
            if (unit(*c, patient).status.find("Stable") == std::string::npos)
                continue;
            party.begin_combat();
            party.apply_combat(c->snapshot());
            party.end_combat();
            const auto capacity = level == 4 ? 3u : 2u;
            auto winds = [&](const CampaignParty & p)
            {
                for (const auto &pool :
                        rules->recovery_info(p.member(id).character.sheet(), p.member(id).vitals)
                        .resources)
                    if (pool.id == "second_wind")
                        return pool.remaining;
                throw std::runtime_error("Missing Second Wind");
            };
            check(winds(party) == capacity - 1 && party.member(patient).vitals.hit_points == 0,
                  "Actual combat handoff preserves successful boost cost and Stable target");
            auto bytes = encode_campaign(party, nullptr, "medicine");
            CampaignParty loaded(module());
            loaded.restore(
                decode_campaign(bytes, *srd5::character_rules(), *rules, "medicine", nullptr)
                .party);
            check(encode_campaign(loaded, nullptr, "medicine") == bytes,
                  "Attained-level grants and post-check recovery save exactly");
            check(bool(loaded.rest(RestKind::short_rest)) && winds(loaded) == capacity,
                  "Short Rest recharges shared Second Wind pool");
            if (loaded.state().short_rest)
                loaded.finish_short_rest(loaded.state().short_rest->ticket);
            check(bool(loaded.rest(RestKind::long_rest)) &&
                  loaded.member(patient).vitals.hit_points > 0 && winds(loaded) == capacity,
                  "Long Rest preserves normal recovery and full shared resource");
            covered = true;
        }
        check(covered, "Real party handoff/rest at every eligible Fighter level");
    }
}

void fixtures()
{
    auto dir = std::filesystem::path(OPENGOLD_BINARY_DIR) / "medicine-fixtures";
    std::filesystem::create_directories(dir);
    auto rules = custom();
    auto h = hero();
    bool done = false;
    for (unsigned seed = 0; seed < 100 && !done; ++seed)
    {
        auto c = battle(*rules, h, seed);
        const auto before = c->save();
        act(*c, "stabilize", 2);
        if (!c->snapshot().ability_check_choice)
            continue;
        // Custom content is used only in native tests; game fixtures require the standard pack.
        auto normal = module();
        auto actors =
        std::vector<Participant> {{
                1,
                "campaign-character",
                "Medic",
                Side::party,
                {1, 1},
                normal->character_profile(h.sheet(), {}).data
            },
            {
                2,
                "vanguard",
                "Patient",
                Side::party,
                {2, 1},
                {},
                VitalState{0, false, "SRD11 2 0 0 0 0 0 0 6000 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0"}
            },
            {99, "vanguard", "Enemy", Side::opposition, {6, 6}}
        };
        auto ui = normal->create({{8, 8, std::vector<Terrain>(64)}, actors}, seed);
        while (ui->snapshot().actor != 1)
            act(*ui, "end");
        if (unit(*ui, 2).hit_points > 0 || !has(*ui, "stabilize"))
            continue;
        const auto available = ui->save();
        act(*ui, "stabilize", 2);
        if (!ui->snapshot().ability_check_choice)
            continue;
        std::ofstream(dir / "available.save", std::ios::binary) << available;
        std::ofstream(dir / "pending.save", std::ios::binary) << ui->save();
        done = true;
    }
    check(done, "UI fixture produced from actual check");
    auto normal = module();
    auto rogue = hero("rogue", 2);
    done = false;
    for (unsigned seed = 0; seed < 100 && !done; ++seed)
    {
        auto c = normal->create({{8, 8, std::vector<Terrain>(64)},
            {   {
                    1,
                    "campaign-character",
                    "Rogue medic",
                    Side::party,
                    {1, 1},
                    normal->character_profile(rogue.sheet(), {}).data
                },
                {
                    2,
                    "vanguard",
                    "Patient",
                    Side::party,
                    {2, 1},
                    {},
                    VitalState{0, false, "SRD11 2 0 0 0 0 0 0 6000 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0"}
                },
                {99, "vanguard", "Enemy", Side::opposition, {6, 6}}
            }},
        seed);
        while (c->snapshot().actor != 1)
            act(*c, "end");
        if (!has(*c, "stabilize"))
            continue;
        std::ofstream(dir / "combined.save", std::ios::binary) << c->save();
        done = true;
    }
    check(done, "Combined Cunning/Stabilize fixture available");
}
} // namespace

int main()
{
    try
    {
        grants();
        outcomes();
        all_classes();
        legality_and_surge();
        campaign_and_rest();
        fixtures();
        std::cout << "Medicine and Tactical Mind tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
