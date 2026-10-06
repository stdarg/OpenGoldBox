#include <sstream>
#include "status_effects.h"
namespace fx = opengold::srd5::detail;
#include "opengold/campaign_save.h"
#include "opengold/character_pool.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <fstream>
#include <iostream>
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

const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

auto module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

void write(const std::filesystem::path &p, const std::string &s)
{
    std::ofstream out(p, std::ios::binary);
    out << s;
    check(bool(out), "Write fixture");
}

CharacterDraft draft()
{
    CharacterDraft d;
    d.race = "orc";
    d.gender = "female";
    d.character_class = "sorcerer";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Cantrip tester";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.rolls[5] = {{6, 6, 6, 1}, 3};
    return d;
}

Command command(const CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &a : c.legal_commands())
        if (a.verb == verb && (!target || a.target == target))
            return a;
    throw std::runtime_error("Missing command: " + std::string(verb));
}

std::string read(const std::filesystem::path &p)
{
    std::ifstream in(p);
    check(bool(in), "Read fixture");
    return {std::istreambuf_iterator<char>(in), {}};
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
    check(caught, "Invalid spell data must reject");
}

CombatantView unit(const CombatSession &c, EntityId id = 1)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing actor");
}

bool has(const CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &a : c.legal_commands())
        if (a.verb == verb && (!target || a.target == target))
            return true;
    return false;
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

Message attack(const CombatSession &c)
{
    for (const auto &m : c.snapshot().log_messages)
        if (m.source.starts_with("{actor} -> {target}: d20"))
            return m;
    throw std::runtime_error("Missing attack log");
}

std::string arg(const Message &m, std::string_view name)
{
    for (const auto &a : m.arguments)
        if (a.name == name)
            return a.value;
    throw std::runtime_error("Missing attack argument");
}

auto custom(std::string affinity = {})
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               affinity);
}

auto battle(const RulesModule &rules, const Character &h, unsigned seed = 13, Cell target = {3, 1},
std::vector<std::string> gear = {}, unsigned side = 1,
std::optional<VitalState> vital = {})
{
    auto profile = rules.character_profile(h.sheet(), gear);
    auto c = rules.create({{28, 8, std::vector<std::uint8_t>(224)},
        {   {1, "campaign-character", "Caster", 0, {1, 1}, profile.data},
            {2, "target", "Target", side, target, {}, vital}
        }},
    seed);
    // A side-zero target requires a living opposing actor to keep combat active.
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 2; ++turns)
        check(c->submit(command(*c, "end")), "Reach caster");
    check(c->snapshot().actor == 1, "Independent seed reaches caster");
    return c;
}

fx::EffectState effects(const VitalState &state)
{
    auto at = state.resources.find("FX");
    if (at == state.resources.npos)
        return {};
    std::istringstream in(state.resources.substr(at));
    return fx::read_effects(in);
}

const std::vector<std::string> choices{"fire_bolt", "poison_spray", "ray_of_frost",
    "shocking_grasp"};

Character hero(std::vector<std::string> spells = choices)
{
    auto d = draft();
    d.cantrips = std::move(spells);
    return Character(*srd5::character_rules(), d, {});
}

void access()
{
    auto rules = module();
    auto creation = srd5::character_rules();
    auto d = draft();
    auto options = creation->cantrip_options(d);
    check(options.count == 4 && options.options.size() == 8,
          "Eight supported Sorcerer options, four choices");
    check(rules->spell_access(creation->evaluate(d, true)).cantrips.empty(),
          "A draft without selections has no invented cantrips");
    const auto h = hero();
    auto access = rules->spell_access(h.sheet());
    check(access.cantrip_choices == 4 && access.cantrips.size() == 4,
          "Four explicit starting choices");
    for (const auto &spell : access.cantrips)
        check(spell.source_id == "class:sorcerer:spellcasting" && spell.acquired_level == 1,
              "Real source and acquisition level");
    for (auto bad : std::vector<std::vector<std::string>> {{"fire_bolt", "fire_bolt"},
    {"eldritch_blast"},
    {"sacred_flame"},
    {"magic_missile"},
    {"unknown"}
})
    {
        d.cantrips = bad;
        rejects(
            [&]
        {
            (void)creation->evaluate(d, true);
        });
    }
    for (unsigned mode = 0; mode < 4; ++mode)
    {
        auto bad = h.sheet();
        auto &g = *std::find_if(bad.grants.begin(), bad.grants.end(),
                                [](const auto & g)
        {
            return g.id == "spell:fire_bolt";
        });
        if (mode == 0)
            g.source_id = "class:wizard:spellcasting";
        if (mode == 1)
            g.level = 2;
        if (mode == 2)
            g.choices = {{"access", "spellbook"}};
        if (mode == 3)
            bad.grants.push_back(g);
        rejects(
            [&]
        {
            (void)rules->character_profile(bad, {});
        });
    }
    auto profile = rules->character_profile(h.sheet(), {}).data;
    check(profile.starts_with("PC42 1 2 4 fire_bolt poison_spray ray_of_frost shocking_grasp "),
          "The recipe lists all four selected cantrips");
    profile.replace(0, 4, "PC28");
    rejects(
        [&]
    {
        (void)rules->create({{8, 8, std::vector<std::uint8_t>(64)},
            {   {1, "campaign-character", "Forged", 0, {1, 1}, profile},
                {99, "vanguard", "Enemy", 1, {5, 1}}
            }},
        13);
    });
    auto absent = battle(*custom(), hero({}));
    for (const auto &spell : choices)
        check(!has(*absent, spell), "Unselected spell unavailable");
    por::CharacterArt art;
    Image head;
    head.width = 88;
    head.height = 40;
    head.rgba.assign(88 * 40 * 4, 128);
    Image body;
    body.width = 88;
    body.height = 48;
    body.rgba.assign(88 * 48 * 4, 128);
    art.heads.emplace(1, por::PortraitPart{"fixture", head});
    art.bodies.emplace(1, por::PortraitPart{"fixture", body});
    unsigned count = 0;
    for (const auto &preset : character_pool(*creation, art))
        if (preset.sheet().character_class == "Sorcerer")
        {
            ++count;
            check(rules->spell_access(preset.sheet()).cantrips.size() == 4,
                  "Preset choices pre-generated");
        }
    check(count > 0, "Preset path exercised");
}

void rolls()
{
    for (const auto &spell : choices)
        for (unsigned seed :
                {
                    0u, 3u, 13u, 40u
                })
            for (const std::string defense :
                    {"", "resistance", "vulnerability", "immunity"
                    })
            {
                const std::string type = spell == "fire_bolt"      ? "fire"
                                         : spell == "poison_spray" ? "poison"
                                         : spell == "ray_of_frost" ? "cold"
                                         : "lightning";
                auto rules = custom(
                                 defense.empty() ? "" : "affinity target test " + defense + " " + type + "\n");
                auto c = battle(*rules, hero(), seed,
                                spell == "shocking_grasp" ? Cell{2, 1} : Cell{3, 1});
                auto copy = rules->restore(c->save());
                auto before = unit(*c);
                const auto random = rng(*c);
                auto ticket = command(*c, spell, 2);
                check(c->submit(ticket) && copy->submit(ticket) && c->save() == copy->save(),
                      "Selected spell checkpoint continuation");
                // Independent SplitMix64: natural 20/10/17/1. Damage dice differ at seed 3.
                const int raw =
                    seed == 40   ? 0
                    : seed == 0  ? ((spell == "ray_of_frost" || spell == "shocking_grasp") ? 9 : 13)
                    : seed == 13 ? ((spell == "ray_of_frost" || spell == "shocking_grasp") ? 4 : 8)
                    : spell == "poison_spray" ? 12
                    : 8;
                const int expected = defense == "immunity"        ? 0
                                     : defense == "resistance"    ? raw / 2
                                     : defense == "vulnerability" ? raw * 2
                                     : raw;
                check(unit(*c, 2).hit_points == 1000 - expected,
                      "Independent typed damage and critical/miss values");
                check(arg(attack(*c), "bonus") == "6", "Charisma 18 rather than Intelligence 15");
                check(rng(*c) == random + 0x9e3779b97f4a7c15ULL * (seed == 0    ? 3u
                        : seed == 40 ? 1u
                        : 2u),
                      "Exact random draw budget");
                auto after = unit(*c);
                check(!after.action && after.bonus_action && after.reaction &&
                      after.movement_feet == before.movement_feet &&
                      after.persistent == before.persistent,
                      "Only Magic action spent");
                if (spell == "ray_of_frost")
                    check(fx::speed_penalty(effects(unit(*c, 2).persistent)) ==
                          (seed == 40 ? 0 : 10),
                          "Hit slows even through Cold immunity");
                if (spell == "shocking_grasp")
                    check(
                        unit(*c, 2).reaction && fx::opportunity_blocked(effects(
                                    unit(*c, 2).persistent)) == (seed != 40),
                        "Shocking Grasp suppresses opportunities through immunity without spending Reaction");
                const auto saved = c->save();
                check(!c->submit(ticket) && c->save() == saved, "Repeated cast rejects atomically");
            }
}

void timing()
{
    auto rules = custom();
    for (const auto &spell : std::vector<std::string> {"ray_of_frost", "shocking_grasp"})
    {
        auto c = battle(*rules, hero(), 13, {2, 1});
        check(c->submit(command(*c, spell, 2)), "Hit applies timed effect");
        auto copy = rules->restore(c->save());
        check(c->submit(command(*c, "end")) && copy->submit(command(*copy, "end")) &&
              c->save() == copy->save(),
              "Target-turn effect continuation");
        if (spell == "shocking_grasp")
            check(!fx::opportunity_blocked(effects(unit(*c, 2).persistent)),
                  "Suppression ends at target turn");
        else
        {
            check(fx::speed_penalty(effects(unit(*c, 2).persistent)) == 10,
                  "Slow remains on target turn");
            check(c->submit(command(*c, "end")), "Reach caster turn");
            check(fx::speed_penalty(effects(unit(*c, 2).persistent)) == 0,
                  "Slow ends at caster turn");
        }
    }
}

void eligibility()
{
    for (const auto &spell : choices)
    {
        auto rules = custom();
        const int range = spell == "fire_bolt"      ? 120
                          : spell == "poison_spray" ? 30
                          : spell == "ray_of_frost" ? 60
                          : 5;
        for (int feet :
                {
                    range, range + 5
                })
        {
            auto c = battle(*rules, hero(), 13, {1 + feet / 5, 1});
            check(has(*c, spell, 2) == (feet == range), "Source-specific spell range boundary");
            if (feet > range)
            {
                auto saved = c->save();
                check(!c->submit({c->snapshot().revision, 1, 2, spell}) && c->save() == saved,
                      "Illegal range atomicity");
            }
        }
        for (auto gear :
        std::vector<std::vector<std::string>> {{"quarterstaff", "shield"}, {"plate"}})
        {
            auto c = battle(*rules, hero(), 13, {2, 1}, gear);
            auto saved = c->save();
            check(!has(*c, spell) && !c->submit({c->snapshot().revision, 1, 2, spell}) &&
                  c->save() == saved,
                  "Hand/armor blockers apply");
        }
        auto c = battle(*rules, hero(), 13, {2, 1});
        check(c->submit(command(*c, spell, 2)), "Adjacent cast allowed");
        check(arg(attack(*c), "disadvantage") ==
              (spell == "shocking_grasp" ? "" : " (disadvantage)"),
              "Only ranged spells suffer adjacent hostile Disadvantage");
    }
}

void campaign()
{
    auto rules = module();
    for (const auto &spell : choices)
        for (bool npc :
                {
                    false, true
                })
        {
            CampaignParty party(module());
            auto h = hero();
            h.inventory().add("quarterstaff", "Quarterstaff");
            const auto id = npc ? party.recruit("fixture:sorcerer-cantrip", std::move(h))
                            : party.add_pc(std::move(h));
            party.equip(id, 1);
            auto state = party.checkpoint();
            state.roster[0].vitals.hit_points -= 2;
            state.roster[0].wealth[3] = 37;
            party.restore(state);
            auto actors = party.participants();
            actors[0].cell = {1, 1};
            actors.push_back({99, "vanguard", "Enemy", 1, {2, 1}});
            auto c = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, actors}, 13);
            check(c->submit(command(*c, spell, 99)),
                  "Ordinary party profile provides Sorcerer cast");
            party.begin_combat();
            party.apply_combat(c->snapshot());
            party.end_combat();
            check(party.member(id).vitals == unit(*c, id).persistent &&
                  party.member(id).wealth == state.roster[0].wealth,
                  "Handoff preserves actual damage/effects and wealth");
            auto saved = encode_campaign(party, nullptr, "sorcerer-cantrip");
            CampaignParty restored(module());
            restored.restore(decode_campaign(saved, *srd5::character_rules(), *rules,
                                             "sorcerer-cantrip", nullptr)
                             .party);
            check(encode_campaign(restored, nullptr, "sorcerer-cantrip") == saved &&
                  restored.member(id).character.creation_data().cantrips ==
                  party.member(id).character.creation_data().cantrips,
                  "Exact campaign reconstruction retains explicit choices");
            for (auto kind :
                    {
                        RestKind::short_rest, RestKind::long_rest
                    })
            {
                check(bool(restored.rest(kind)), "Camp/inn rest");
                if (restored.state().short_rest)
                    restored.finish_short_rest(restored.state().short_rest->ticket);
                saved = encode_campaign(restored, nullptr, "sorcerer-cantrip");
                CampaignParty again(module());
                again.restore(decode_campaign(saved, *srd5::character_rules(), *rules,
                                              "sorcerer-cantrip", nullptr)
                              .party);
                check(encode_campaign(again, nullptr, "sorcerer-cantrip") == saved &&
                      rules->spell_access(again.member(id).character.sheet()).cantrips.size() ==
                      4,
                      "Rest save/reload preserves all learned cantrips");
            }
        }
}

void fixtures()
{
    auto rules = module();
    auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "sorcerer-fixtures";
    std::filesystem::create_directories(path);
    for (const std::string kind :
            {"known", "blocked", "unknown"
            })
    {
        auto h = hero(kind == "unknown" ? std::vector<std::string> {} : choices);
        auto profile = rules->character_profile(
                           h.sheet(), kind == "blocked" ? std::vector<std::string> {"quarterstaff", "shield"}
                           : std::vector<std::string> {});
        auto c = rules->create({{12, 9, std::vector<std::uint8_t>(108)},
            {   {1, "campaign-character", "Sorcerer", 0, {1, 1}, profile.data},
                {2, "vanguard", "Ally", 0, {2, 1}},
                {99, "vanguard", "Enemy", 1, {5, 1}}
            }},
        2);
        write(path / (kind + ".save"), c->save());
    }
}
} // namespace

int main()
{
    try
    {
        access();
        rolls();
        timing();
        eligibility();
        campaign();
        fixtures();
        std::cout << "Sorcerer cantrip tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
