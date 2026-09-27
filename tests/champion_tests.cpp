#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "dice.h"
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
    check(bad, "Malformed state must reject");
}

auto module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

auto custom()
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    std::string text{std::istreambuf_iterator<char>(in), {}};
    return srd5::parse_content(text +
                               "\ncreature dummy 40 1000 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

Character hero(unsigned level = 3, std::string klass = "fighter", bool savage = false)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = klass;
    d.background = savage ? "soldier" : "sage";
    d.name = "Champion";
    d.alignment = "neutral_good";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    Character h(*srd5::character_rules(), d, {});
    VitalState life;
    for (unsigned n = 1; n < level; ++n)
        check(h.advance(*module(), life), "Ordinary level advancement");
    return h;
}

CombatantView unit(const CombatSession &c, EntityId id = 1)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing actor");
}

bool has(const CombatSession &c, std::string_view verb)
{
    for (const auto &v : c.legal_commands())
        if (v.verb == verb)
            return true;
    return false;
}

Command cmd(const CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &v : c.legal_commands())
        if (v.verb == verb && (!target || v.target == target))
            return v;
    throw std::runtime_error("Missing command: " + std::string(verb));
}

void act(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    check(c.submit(cmd(c, verb, target)), "Accepted legal command");
}

auto battle(const RulesModule &rules, const Character &h, unsigned seed,
            std::vector<std::string> gear = {"longsword"}, bool prone = false)
{
    auto c = rules.create({{8, 8, std::vector<std::uint8_t>(64)},
                           {{1,
                             "campaign-character",
                             "Champion",
                             0,
                             {2, 2},
                             rules.character_profile(h.sheet(), gear).data,
                             prone ? std::optional<VitalState>{{h.sheet().hit_points, false,
                                                                "SRD3 2 0 0 0 0 0 FX4 1 0 0 1"}}
                                   : std::nullopt},
                            {2, "dummy", "Target", 1, {3, 2}}}},
                          seed);
    while (c->snapshot().actor != 1)
        act(*c, "end");
    return c;
}

int attack_roll(const CombatSession &c)
{
    const auto state = c.snapshot();
    for (auto it = state.log_messages.rbegin(); it != state.log_messages.rend(); ++it)
        if (it->source.starts_with("{actor} ->"))
            for (const auto &a : it->arguments)
                if (a.name == "roll")
                    return std::stoi(a.value);
    return 0;
}

std::uint64_t random_state(const CombatSession &c)
{
    std::istringstream in(c.save());
    std::string row;
    for (unsigned n = 0; n < 3; ++n)
        std::getline(in, row);
    std::uint64_t result{};
    in >> result;
    return result;
}

void grants_and_checks()
{
    auto rules = module();
    for (unsigned level = 1; level <= 4; ++level)
    {
        auto h = hero(level);
        const bool champion = level >= 3;
        auto has_grant = [&](std::string_view id)
        {
            return std::any_of(h.sheet().grants.begin(), h.sheet().grants.end(),
                               [&](const auto &g)
                               {
                                   return g.id == id;
                               });
        };
        check(has_grant("subclass:champion") == champion &&
                  has_grant("feature:remarkable_athlete") == champion,
              "Subclass and both feature grants follow ordinary level three acquisition");
        check(rules->character_profile(h.sheet(), {})
                  .data.starts_with(level == 4   ? "PC39 "
                                    : champion   ? "PC31 "
                                    : level == 2 ? "PC30 "
                                                 : "PC28 "),
              "Champion profile is conditional");
        const auto athletics = rules->ability_check(h.sheet(), {}, 0, "athletics", {}, {});
        check(athletics.advantage == champion,
              "Strength Athletics gains Advantage only for Champion");
        check(!rules->ability_check(h.sheet(), {}, 1, "athletics", {}, {}).advantage &&
                  !rules->ability_check(h.sheet(), {}, 0, "acrobatics", {}, {}).advantage,
              "Other ability/skill combinations excluded");
        const auto armored = rules->ability_check(h.sheet(), std::vector<std::string>{"hide"}, 0,
                                                  "athletics", {}, {});
        check(armored.advantage == champion && !armored.disadvantage,
              "Trained Fighter armor does not cancel Advantage");
        if (champion)
        {
            auto forged = h.sheet();
            std::erase_if(forged.grants,
                          [](const auto &g)
                          {
                              return g.id == "feature:improved_critical";
                          });
            rejects(
                [&]
                {
                    (void)rules->character_profile(forged, {});
                });
        }
        for (unsigned seed = 0; seed < 24; ++seed)
        {
            std::uint64_t rng = seed;
            const int first = srd5::roll_die(rng, 20);
            const int second = champion ? srd5::roll_die(rng, 20) : first;
            auto c = battle(*custom(), h, seed);
            check(unit(*c).initiative == std::max(first, second) + h.sheet().modifiers[1],
                  "Independent maximum-of-two initiative expectation");
        }
    }
}

void critical_and_movement()
{
    auto rules = custom();
    for (unsigned level = 1; level <= 4; ++level)
        for (bool unarmed : {false, true})
        {
            bool nineteen = false, twenty = false;
            for (unsigned seed = 0; seed < 400 && !(nineteen && twenty); ++seed)
            {
                auto h = hero(level);
                auto c = battle(*rules, h, seed,
                                unarmed ? std::vector<std::string>{}
                                        : std::vector<std::string>{"longsword"});
                const auto before = unit(*c);
                auto expected_rng = random_state(*c);
                const int expected_roll = srd5::roll_die(expected_rng, 20);
                act(*c, "melee", 2);
                const int roll = attack_roll(*c);
                check(roll == expected_roll, "Attack consumes one independent d20");
                if (roll != 19 && roll != 20)
                    continue;
                nineteen |= roll == 19;
                twenty |= roll == 20;
                const bool critical = roll == 20 || (level >= 3 && roll == 19);
                check(
                    (unit(*c, 2).hit_points < 1000) == critical,
                    "Expanded critical automatically hits even AC 40; old levels require natural 20");
                if (critical)
                {
                    const int expected_damage = unarmed ? 1 + h.sheet().modifiers[0]
                                                        : srd5::roll_die(expected_rng, 8) +
                                                              srd5::roll_die(expected_rng, 8) +
                                                              h.sheet().modifiers[0];
                    check(
                        unit(*c, 2).hit_points == 1000 - expected_damage &&
                            random_state(*c) == expected_rng,
                        "Critical doubles weapon dice, never flat damage, with exact RNG consumption");
                }
                check(bool(c->snapshot().free_movement) == (critical && level >= 3),
                      "Weapon and unarmed criticals offer free movement only to Champions");
                if (!c->snapshot().free_movement)
                    continue;
                check(c->snapshot().free_movement->remaining_feet == 15 && !has(*c, "dash") &&
                          !has(*c, "action_surge"),
                      "Half Speed allowance and exclusive legal actions");
                const auto saved = c->save();
                auto copy = rules->restore(saved);
                check(copy->save() == saved, "Pending movement round trips canonically");
                check(!c->submit({c->snapshot().revision, 1, 0, "dash"}) && c->save() == saved,
                      "Invalid action preserves pending movement and RNG");
                auto malformed = saved;
                const auto tail = malformed.rfind('\n', malformed.size() - 2);
                malformed.replace(tail + 1, malformed.size() - tail - 1, "1 2 19 999 0 2 2\n");
                rejects(
                    [&]
                    {
                        (void)rules->restore(malformed);
                    });
                Command step;
                for (const auto &offered : c->legal_commands())
                    if (offered.verb == "move" && offered.destination == Cell{1, 2})
                        step = offered;
                check(step.actor == 1, "Legal escape from target's reach");
                check(c->submit(step) && copy->submit(step) && c->save() == copy->save(),
                      "Free movement resumes identically and provokes no reaction");
                check(c->snapshot().free_movement &&
                          c->snapshot().free_movement->remaining_feet == 10 &&
                          !c->snapshot().reaction_pending,
                      "Free movement spends its own budget");
                act(*c, "end");
                check(!c->snapshot().free_movement && c->snapshot().actor == 1 &&
                          unit(*c).movement_feet == before.movement_feet && !unit(*c).action &&
                          unit(*c).reaction == before.reaction,
                      "Finish free move does not end turn or restore/spend other budgets");
            }
            check(nineteen && twenty,
                  "Both critical thresholds exercised at each level and weapon mode");
        }
}

void savage_and_prone()
{
    auto rules = custom();
    bool covered = false;
    for (unsigned seed = 0; seed < 400 && !covered; ++seed)
    {
        auto c = battle(*rules, hero(3, "fighter", true), seed, {"longsword"}, true);
        act(*c, "melee", 2);
        if (!c->snapshot().savage_attack_choice)
            continue;
        check(!c->snapshot().free_movement, "Free movement waits until Savage choice resolves");
        act(*c, "savage_use");
        auto copy = rules->restore(c->save());
        act(*c, "savage_second");
        act(*copy, "savage_second");
        check(c->save() == copy->save() && c->snapshot().free_movement,
              "Accepted critical damage opens free movement after Savage");
        auto step = cmd(*c, "move");
        check(c->submit(step), "Prone Champion can crawl");
        check(c->snapshot().free_movement->remaining_feet == 5,
              "Crawling charges doubled cost from the free allowance");
        check(!has(*c, "move") && has(*c, "end"), "Unusable remainder can be declined");
        covered = true;
    }
    check(covered, "Savage/Prone critical exercised");
}

void reaction_continuation()
{
    auto rules = custom();
    for (bool block : {false, true})
        for (bool lethal : {false, true})
        {
            bool covered = false;
            for (unsigned seed = 0; seed < 400 && !covered; ++seed)
            {
                auto h = hero();
                auto c = rules->create(
                    {{8, 8, std::vector<std::uint8_t>(64)},
                     {{1,
                       "campaign-character",
                       "Champion",
                       0,
                       {2, 2},
                       rules->character_profile(h.sheet(), std::vector<std::string>{"longsword"})
                           .data},
                      {2,
                       "dummy",
                       "Mover",
                       1,
                       {3, 2},
                       {},
                       lethal ? std::optional<VitalState>{{1, false, {}}} : std::nullopt},
                      {3, "dummy", "Reserve", 1, {7, 7}}}},
                    seed);
                while (c->snapshot().actor != 2)
                    act(*c, "end");
                Command ticket;
                for (const auto &v : c->legal_commands())
                    if (v.verb == "move" && v.destination == Cell{4, 2})
                        ticket = v;
                check(ticket.actor == 2 && c->submit(ticket) && c->snapshot().reaction_pending,
                      "Actual movement provokes Champion reaction");
                act(*c, "opportunity", 2);
                if (!c->snapshot().free_movement)
                    continue;
                check(c->snapshot().actor == 1 && !unit(*c).reaction && unit(*c).action,
                      "Critical reaction interrupts movement without spending Champion Action");
                auto copy = rules->restore(c->save());
                check(copy->save() == c->save(),
                      "Critical reaction phase restores exactly, including a killed mover");
                const Cell destination = block ? Cell{4, 2} : Cell{2, 1};
                Command step;
                for (const auto &v : c->legal_commands())
                    if (v.verb == "move" && v.destination == destination)
                        step = v;
                check(step.actor == 1 && c->submit(step) && copy->submit(step),
                      "Reaction Champion can move before original route resumes");
                auto moved = rules->restore(c->save());
                check(moved->save() == c->save(),
                      "New reactor location preserves pending original path");
                act(*c, "end");
                act(*copy, "end");
                act(*moved, "end");
                check(c->save() == copy->save() && c->save() == moved->save(),
                      "Original path continues deterministically after free move");
                check(unit(*c, 2).cell == (block || lethal ? Cell{3, 2} : Cell{4, 2}),
                      "Blocked or dead mover stops; otherwise original move resumes");
                covered = true;
            }
            check(covered, "Live reaction movement case exercised");
        }
}

void campaign_and_cancellation()
{
    auto rules = module();
    for (unsigned level = 3; level <= 4; ++level)
    {
        bool covered = false;
        for (unsigned seed = 0; seed < 160 && !covered; ++seed)
        {
            CampaignParty party(module());
            auto h = hero(1);
            h.inventory().add("longsword", "Sword");
            const auto id = party.add_pc(std::move(h));
            party.equip(id, 1);
            party.award_experience(2700, "champion-levels");
            for (unsigned n = 2; n <= level; ++n)
                party.advance(id, party.default_advancement(id));
            auto state = party.checkpoint();
            state.roster[0].vitals.hit_points = 1;
            party.restore(state);
            auto actors = party.participants();
            actors[0].cell = {2, 2};
            actors.push_back({2, "vanguard", "Enemy", 1, {3, 2}});
            actors.push_back({3, "vanguard", "Reserve", 1, {6, 6}});
            auto c = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, actors}, seed);
            while (c->snapshot().actor != id)
                act(*c, "end");
            act(*c, "second_wind");
            const auto hp = unit(*c).hit_points;
            act(*c, "melee", 2);
            if (!c->snapshot().free_movement)
                continue;
            act(*c, "end");
            check(unit(*c).hit_points == hp && !unit(*c).bonus_action,
                  "Passive Champion features preserve HP and spent Second Wind budget");
            party.begin_combat();
            party.apply_combat(c->snapshot());
            party.end_combat();
            const auto bytes = encode_campaign(party, nullptr, "champion-current");
            CampaignParty copy(module());
            copy.restore(decode_campaign(bytes, *srd5::character_rules(), *rules,
                                         "champion-current", nullptr)
                             .party);
            check(encode_campaign(copy, nullptr, "champion-current") == bytes &&
                      copy.ability_check(id, 0, "athletics").advantage,
                  "Ordinary campaign save/reload keeps subclass and check source");
            check(bool(copy.rest(RestKind::short_rest)), "Champion uses ordinary Short Rest");
            if (copy.state().short_rest)
                copy.finish_short_rest(copy.state().short_rest->ticket);
            check(bool(copy.rest(RestKind::long_rest)), "Champion uses ordinary Long Rest");
            check(copy.profile(id).data.starts_with(level == 4 ? "PC39 " : "PC31 "),
                  "Rests retain Champion entitlement");
            covered = true;
        }
        check(covered, "All attained Champion levels use real combat/campaign/rest path");
    }
    auto h = hero();
    for (unsigned seed = 0; seed < 24; ++seed)
    {
        Participant p{1,
                      "campaign-character",
                      "Surprised Champion",
                      0,
                      {1, 1},
                      rules->character_profile(h.sheet(), {}).data};
        p.surprised = true;
        std::uint64_t rng = seed;
        const int expected = srd5::roll_die(rng, 20) + h.sheet().modifiers[1];
        auto c = rules->create(
            {{8, 8, std::vector<std::uint8_t>(64)}, {p, {2, "vanguard", "Enemy", 1, {6, 6}}}},
            seed);
        check(unit(*c).initiative == expected,
              "Surprise Disadvantage cancels Champion Initiative Advantage without extra dice");
    }
}

void repeated_criticals_and_terrain()
{
    auto rules = custom();
    bool covered = false;
    for (unsigned seed = 0; seed < 1000 && !covered; ++seed)
    {
        auto h = hero();
        Battlefield board{8, 8, std::vector<std::uint8_t>(64)};
        board.terrain[2 * 8 + 1] = 2;
        auto c = rules->create(
            {board,
             {{1,
               "campaign-character",
               "Champion",
               0,
               {2, 2},
               rules->character_profile(h.sheet(), std::vector<std::string>{"longsword"}).data},
              {2, "dummy", "Target", 1, {3, 2}}}},
            seed);
        while (c->snapshot().actor != 1)
            act(*c, "end");
        act(*c, "melee", 2);
        if (!c->snapshot().free_movement)
            continue;
        auto copy = rules->restore(c->save());
        Command step;
        for (const auto &v : copy->legal_commands())
            if (v.verb == "move" && v.destination == Cell{1, 2})
                step = v;
        check(step.actor == 1 && copy->submit(step) &&
                  copy->snapshot().free_movement->remaining_feet == 5,
              "Difficult Terrain consumes ten feet of free movement");
        act(*c, "end");
        act(*c, "action_surge");
        act(*c, "melee", 2);
        if (!c->snapshot().free_movement)
            continue;
        check(c->snapshot().free_movement->remaining_feet == 15,
              "A second critical earns a fresh allowance in the same turn");
        check(rules->restore(c->save())->save() == c->save(),
              "Second critical preserves spent Surge continuation");
        act(*c, "end");
        check(!unit(*c).action && unit(*c).movement_feet == 30,
              "Declining both free moves preserves normal movement and both spent actions");
        covered = true;
    }
    check(covered, "Multiple critical triggers exercised in one actual turn");
}

void fixtures()
{
    auto rules = module();
    const auto folder = std::filesystem::path(OPENGOLD_BINARY_DIR) / "champion-fixtures";
    std::filesystem::create_directories(folder);
    bool done = false;
    for (unsigned seed = 0; seed < 400 && !done; ++seed)
    {
        auto h = hero();
        auto c = rules->create(
            {{8, 8, std::vector<std::uint8_t>(64)},
             {{1,
               "campaign-character",
               "Champion",
               0,
               {2, 2},
               rules->character_profile(h.sheet(), std::vector<std::string>{"longsword"}).data},
              {2, "vanguard", "Enemy", 1, {3, 2}},
              {3, "vanguard", "Reserve", 1, {6, 6}}}},
            seed);
        while (c->snapshot().actor != 1)
            act(*c, "end");
        act(*c, "melee", 2);
        if (!c->snapshot().free_movement)
            continue;
        std::ofstream(folder / "pending.save") << c->save();
        done = true;
    }
    check(done, "Actual critical UI fixture captured");
}
} // namespace

int main()
{
    try
    {
        grants_and_checks();
        critical_and_movement();
        savage_and_prone();
        reaction_continuation();
        campaign_and_cancellation();
        repeated_criticals_and_terrain();
        fixtures();
        std::cout << "Champion tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
