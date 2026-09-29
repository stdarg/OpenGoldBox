#include "campaign_fixture.h"
#include <sstream>
#include <tuple>
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
    d.character_class = "cleric";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Sacred Flame tester";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    return d;
}

Command command(const CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &a : c.legal_commands())
        if (a.verb == verb && (!target || a.target == target))
            return a;
    throw std::runtime_error("Missing command: " + std::string(verb));
}

void freeze()
{
    auto rules = module();
    check(rules->identity().version == "0.6.22", "Freeze must use actual prior writer");
    auto creation = srd5::character_rules();
    Character h(*creation, draft(), {});
    VitalState scratch;
    for (unsigned n = 1; n < 3; ++n)
        check(h.advance(*rules, scratch), "Prior Cleric advancement");
    h.inventory().add("mace", "Mace");
    CampaignParty p(module());
    p.add_pc(std::move(h));
    p.equip(1, 1);
    auto state = p.checkpoint();
    state.roster[0].vitals.hit_points = 1;
    state.time_minutes = 123;
    state.subminute_milliseconds = 456;
    state.random_state = 789;
    state.roster[0].wealth[3] = 37;
    p.restore(std::move(state));
    auto actors = p.participants();
    actors[0].cell = {1, 1};
    actors.push_back({99, "vanguard", "Enemy", 1, {5, 1}});
    auto c = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, actors}, 2);
    check(c->snapshot().actor == 1, "Prior caster begins");
    check(c->submit(command(*c, "cure_wounds", 1)), "Prior slot spent");
    p.begin_combat();
    p.apply_combat(c->snapshot());
    p.end_combat();
    const auto base = root / "tests/fixtures";
    write(base / "campaign-v11-sacred.ogs", encode_campaign(p, nullptr, "sacred"));
    while (c->snapshot().actor == 1)
        check(c->submit(command(*c, "end")), "End caster turn");
    while (c->snapshot().actor != 1)
        check(c->submit(command(*c, "end")), "Reach next caster turn");
    write(base / "combat-v13-sacred.save", c->save());
    check(c->submit(command(*c, "cure_wounds", 1)), "Prior Cure Wounds continuation");
    write(base / "combat-v13-sacred-continued.save", c->save());
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

std::string arg(const Message &m, std::string_view name)
{
    for (const auto &a : m.arguments)
        if (a.name == name)
            return a.value;
    throw std::runtime_error("Missing message argument");
}

Character hero(unsigned level = 1, bool learned = true)
{
    auto d = draft();
    d.cantrips = learned ? std::vector<std::string> {"sacred_flame"} :
                 std::vector<std::string> {};
    Character h(*srd5::character_rules(), d, {});
    VitalState scratch;
    for (unsigned n = 1; n < level; ++n)
        check(h.advance(*module(), scratch), "Advance real Cleric choices");
    return h;
}

auto custom(std::string affinity = {}, int dex = 0)
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n" +
                               "saves target 0 " + std::to_string(dex) + " 0 0 0 0\n" + affinity);
}

auto battle(const RulesModule &rules, const Character &h, unsigned seed = 13, Cell target = {3, 1},
std::vector<std::string> gear = {}, unsigned side = 1,
std::optional<VitalState> vital = {})
{
    auto profile = rules.character_profile(h.sheet(), gear);
    auto c = rules.create({{10, 8, std::vector<std::uint8_t>(80)},
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

void access()
{
    auto creation = srd5::character_rules();
    auto rules = module();
    auto d = draft();
    check(rules->spell_access(creation->evaluate(d, true)).cantrips.empty(),
          "Historical Cleric has no invented cantrips");
    auto h = hero();
    auto access = rules->spell_access(h.sheet());
    check(access.cantrip_choices == 3 && access.cantrips.size() == 1 &&
          access.cantrips[0].source_id == "class:cleric:spellcasting" &&
          access.cantrips[0].acquired_level == 1,
          "Cleric cantrip has its actual source and entitlement");
    for (unsigned level = 1; level <= 4; ++level)
    {
        const auto a = rules->spell_access(hero(level).sheet());
        check(a.cantrip_choices == (level == 4 ? 4u : 3u) && a.cantrips == access.cantrips,
              "Advancement preserves the selected starting cantrip and correct entitlement");
    }
    for (const auto &bad : std::vector<std::vector<std::string>> {{"sacred_flame", "sacred_flame"},
    {"fire_bolt"},
    {"poison_spray"},
    {"cure_wounds"},
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
    for (const auto &klass : creation->choices(CreationField::character_class))
        if (klass.id != "cleric")
        {
            d = draft();
            d.character_class = klass.id;
            d.cantrips = std::vector<std::string> {"sacred_flame"};
            rejects(
                [&]
            {
                (void)creation->evaluate(d, true);
            });
        }
    auto profile = rules->character_profile(h.sheet(), {}).data;
    check(profile.starts_with("PC28 1 0 130 "), "New recipe records sourced Cleric access");
    profile.replace(0, 4, "PC11");
    rejects(
        [&]
    {
        (void)rules->create({{8, 8, std::vector<std::uint8_t>(64)},
            {   {1, "campaign-character", "Forged", 0, {1, 1}, profile},
                {2, "vanguard", "Target", 1, {3, 1}}
            }},
        13);
    });
    profile.replace(profile.find("130"), 3, "2");
    rejects(
        [&]
    {
        (void)rules->create({{8, 8, std::vector<std::uint8_t>(64)},
            {   {1, "campaign-character", "Forged", 0, {1, 1}, profile},
                {2, "vanguard", "Target", 1, {3, 1}}
            }},
        13);
    });
    auto invalid = h.sheet();
    for (auto &g : invalid.grants)
        if (g.id == "spell:sacred_flame")
            g.source_id = "class:wizard:spellcasting";
    rejects(
        [&]
    {
        (void)rules->character_profile(invalid, {});
    });
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
    unsigned clerics = 0;
    for (const auto &preset : character_pool(*creation, art))
        if (preset.sheet().character_class == "Cleric")
        {
            ++clerics;
            check(rules->spell_access(preset.sheet()).cantrips.size() == 1,
                  "Preset Clerics have pre-generated supported selections");
        }
    check(clerics == 4, "All Cleric presets verified");
}

Message save_message(const CombatSession &c)
{
    for (const auto &m : c.snapshot().log_messages)
        if (m.source.starts_with("{name} Dexterity save:"))
            return m;
    throw std::runtime_error("Missing Dexterity save");
}

void rolls()
{
    // Independent SplitMix64 values after the two initiative draws:
    // seed 40 -> save 1, d8 5; seed 0 -> save 20, d8 5;
    // seed 2 -> save 12, d8 5. Ordinary saves have no automatic 1/20 result.
    for (unsigned level = 1; level <= 4; ++level)
        for (unsigned seed :
                {
                    0u, 2u, 40u
                })
            for (const auto &defense :
                    std::vector<std::string> {"", "resistance", "vulnerability", "immunity"})
            {
                auto rules =
                    custom(defense.empty() ? "" : "affinity target test " + defense + " radiant\n");
                auto c = battle(*rules, hero(level), seed);
                auto copy = rules->restore(c->save());
                const auto before = unit(*c);
                const auto random = rng(*c);
                const auto ticket = command(*c, "sacred_flame", 2);
                check(c->submit(ticket) && copy->submit(ticket) && c->save() == copy->save(),
                      "Saved actual cantrip command resumes identically");
                const bool success = seed == 0 || (seed == 2 && level < 4);
                const int raw = success ? 0 : 5, damage = defense == "immunity"        ? 0
                    : defense == "resistance"    ? raw / 2
                    : defense == "vulnerability" ? raw * 2
                    : raw;
                const auto save = save_message(*c);
                check(
                    arg(save, "roll") == std::to_string(seed == 0   ? 20
                                                        : seed == 2 ? 12
                                                        : 1) &&
                    arg(save, "dc") == std::to_string(level == 4 ? 13 : 12) &&
                    arg(save, "bonus") == "0",
                    "Wisdom/proficiency DC and target Dexterity bonus are independent from AC and Intelligence");
                check(
                    unit(*c, 2).hit_points == 1000 - damage &&
                    arg(save, "result") == (success ? "success" : "failure"),
                    "Save success deals zero damage; failure deals unmodified d8 Radiant damage with typed defenses");
                check(rng(*c) == random + 0x9e3779b97f4a7c15ULL * (success ? 1u : 2u),
                      "No attack roll, critical dice or damage roll on a successful save");
                const auto after = unit(*c);
                check(!after.action && after.bonus_action && after.reaction &&
                      after.movement_feet == before.movement_feet &&
                      after.persistent == before.persistent,
                      "Only Action is spent; pools, movement and other budgets remain");
                const auto saved = c->save();
                check(!c->submit(ticket) && c->save() == saved, "Stale cast is atomic");
            }
    for (const auto [seed, bonus, success] :
    std::vector<std::tuple<unsigned, int, bool>> {{40, 20, true}, {0, -10, false}})
    {
        auto rules = custom({}, bonus);
        auto c = battle(*rules, hero(), seed);
        check(c->submit(command(*c, "sacred_flame", 2)), "Extreme save cast");
        check(arg(save_message(*c), "result") == (success ? "success" : "failure"),
              "Natural 1 can succeed and natural 20 can fail without critical damage");
        check(unit(*c, 2).hit_points == (success ? 1000 : 995),
              "Failed natural 20 save still deals only one d8");
    }
}

void targets()
{
    auto rules = custom();
    const auto h = hero();
    for (int feet :
            {
                5, 60, 65
            })
    {
        auto profile = rules->character_profile(h.sheet(), {});
        auto c = rules->create({{16, 8, std::vector<std::uint8_t>(128)},
            {   {1, "campaign-character", "Caster", 0, {1, 1}, profile.data},
                {2, "target", "Enemy", 1, {1 + feet / 5, 1}}
            }},
        40);
        check(has(*c, "sacred_flame", 2) == (feet <= 60), "Range includes 60 feet and excludes 65");
        if (feet <= 60)
        {
            check(c->submit(command(*c, "sacred_flame", 2)) && unit(*c, 2).hit_points == 995,
                  "Adjacent hostile does not impose attack-roll Disadvantage on a save spell");
        }
        else
        {
            auto before = c->save();
            check(!c->submit({c->snapshot().revision, 1, 2, "sacred_flame"}) && c->save() == before,
                  "Out-of-range rejection is atomic");
        }
    }
    for (const auto &gear : std::vector<std::vector<std::string>>
{
    {}, {"shield"}, {"mace"}, {"mace", "shield"}, {"plate"}
})
    {
        auto c = battle(*rules, h, 13, {3, 1}, gear);
        const bool allowed = gear.size() < 2 && (gear.empty() || gear[0] != "plate");
        check(has(*c, "sacred_flame", 2) == allowed,
              "Somatic hands and untrained heavy armor restrict casting");
        check(unit(*c).known_cantrips == std::vector<std::string> {"sacred_flame"},
              "Known spell remains listed while unavailable");
        if (!allowed)
        {
            auto before = c->save();
            check(!c->submit({c->snapshot().revision, 1, 2, "sacred_flame"}) && c->save() == before,
                  "Blocked casting preserves state/RNG");
        }
    }
    check(!has(*battle(*rules, hero(1, false)), "sacred_flame"),
          "Cantrip is not an unconditional class flag");
    auto profile = rules->character_profile(h.sheet(), {});
    Battlefield board{10, 8, std::vector<std::uint8_t>(80)};
    board.terrain[12] = 1;
    auto blocked = rules->create({board,
        {   {1, "campaign-character", "Caster", 0, {1, 1}, profile.data},
            {2, "target", "Enemy", 1, {3, 1}}
        }},
    13);
    check(!has(*blocked, "sacred_flame", 2), "Total cover remains ineligible");
    const auto blocked_before = blocked->save();
    check(!blocked->submit({blocked->snapshot().revision, 1, 2, "sacred_flame"}) &&
          blocked->save() == blocked_before,
          "Total-cover rejection preserves all state");
    auto c = battle(*rules, h, 40);
    const auto before = c->save();
    for (EntityId id :
            {
                0u, 999u
            })
        check(!c->submit({c->snapshot().revision, 1, id, "sacred_flame"}) && c->save() == before,
              "Unknown and noncreature targets reject atomically");
    const auto hp = unit(*c).hit_points;
    check(c->submit(command(*c, "sacred_flame", 1)) && unit(*c).hit_points == hp - 5,
          "Self-target resolves a Dexterity save and actual damage");
    for (bool down :
            {
                false, true
            })
        for (bool dead :
                {
                    false, true
                })
        {
            auto c = rules->create({{10, 8, std::vector<std::uint8_t>(80)},
                {   {1, "campaign-character", "Caster", 0, {1, 1}, profile.data},
                    {
                        2,
                        "target",
                        "Ally",
                        0,
                        {2, 1},
                        {},
                        dead   ? std::optional<VitalState>{{0, true, {}}}
: down ? std::optional<VitalState>{{0, false, {}}}
: std::nullopt
                    },
                    {3, "target", "Enemy", 1, {8, 1}}
                }},
            13);
            check(has(*c, "sacred_flame", 2) == !dead,
                  "Living allies, including unconscious targets, are eligible; corpses are not");
            const auto before = c->save();
            const auto random = rng(*c);
            if (dead)
            {
                check(!c->submit({c->snapshot().revision, 1, 2, "sacred_flame"}) &&
                      c->save() == before,
                      "Dead-target rejection is atomic");
                continue;
            }
            check(c->submit(command(*c, "sacred_flame", 2)), "Ally cast");
            if (down)
            {
                check(rng(*c) == random + 0x9e3779b97f4a7c15ULL,
                      "Automatic unconscious failure rolls damage only");
                check(
                    unit(*c, 2).persistent.description.find("1 failures") != std::string::npos,
                    "Saving-throw damage at zero HP adds one death failure, never a close-range critical");
            }
        }
}

void modifiers()
{
    auto rules = custom();
    auto c = battle(*rules, hero(4), 2);
    check(c->submit(command(*c, "end")) && c->submit(command(*c, "dodge")) &&
          c->submit(command(*c, "end")),
          "Target Dodges before caster turn");
    const auto start = rng(*c);
    check(c->submit(command(*c, "sacred_flame", 2)), "Cast against Dodge");
    check(arg(save_message(*c), "roll") == "17" && unit(*c, 2).hit_points == 1000 &&
          rng(*c) == start + 2 * 0x9e3779b97f4a7c15ULL,
          "Dodge grants Advantage on Dexterity saves");
    auto d = draft();
    d.character_class = "wizard";
    Character wizard(*srd5::character_rules(), d, {});
    auto target = rules->character_profile(wizard.sheet(), std::vector<std::string> {"plate"});
    auto caster = rules->character_profile(hero().sheet(), {});
    auto armored = rules->create({{10, 8, std::vector<std::uint8_t>(80)},
        {   {1, "campaign-character", "Caster", 0, {1, 1}, caster.data},
            {2, "campaign-character", "Armored", 1, {3, 1}, target.data}
        }},
    13);
    const auto random = rng(*armored);
    check(armored->submit(command(*armored, "sacred_flame", 2)), "Cast against untrained armor");
    check(arg(save_message(*armored), "roll") == "8" &&
          arg(save_message(*armored), "bonus") == "2" &&
          rng(*armored) == random + 3 * 0x9e3779b97f4a7c15ULL,
          "Untrained armor imposes Dexterity save Disadvantage");
    bool checked = false;
    for (unsigned seed = 0; seed < 32 && !checked; ++seed)
    {
        auto blinded = rules->create({{10, 8, std::vector<std::uint8_t>(80)},
            {   {1, "campaign-character", "Caster", 0, {1, 1}, caster.data},
                {2, "blindness-adept", "Enemy", 1, {3, 1}}
            }},
        seed);
        if (blinded->snapshot().actor == 1)
            check(blinded->submit(command(*blinded, "end")), "Reach enemy caster");
        check(blinded->submit(command(*blinded, "blindness", 1)), "Actual Blindness source casts");
        if (unit(*blinded).conditions.empty())
            continue;
        check(blinded->submit(command(*blinded, "end")), "Reach blinded Cleric");
        check(!has(*blinded, "sacred_flame"),
              "Blindness blocks every sight-required target including self");
        const auto before = blinded->save();
        check(!blinded->submit({blinded->snapshot().revision, 1, 2, "sacred_flame"}) &&
              blinded->save() == before,
              "Sight rejection is atomic");
        checked = true;
    }
    check(checked, "Actual Blindness interaction exercised");
}

void campaign()
{
    auto rules = module();
    auto creation = srd5::character_rules();
    for (bool npc :
            {
                false, true
            })
        for (unsigned level = 1; level <= 4; ++level)
        {
            CampaignParty p(module());
            auto h = hero(level);
            h.inventory().add("quarterstaff", "Quarterstaff");
            const auto id =
                npc ? p.recruit("fixture:sacred", std::move(h)) : p.add_pc(std::move(h));
            p.equip(id, 1);
            p.set_grip(id, 2);
            auto state = p.checkpoint();
            state.roster[0].vitals.hit_points -= 3;
            state.roster[0].wealth[3] = 37;
            state.time_minutes = 123;
            state.subminute_milliseconds = 456;
            p.restore(std::move(state));
            auto actors = p.participants();
            actors[0].cell = {1, 1};
            actors.push_back({99, "vanguard", "Enemy", 1, {5, 1}});
            auto c = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, actors}, 40);
            const auto old = unit(*c, id).persistent;
            check(c->submit(command(*c, "sacred_flame", 99)), "Ordinary campaign Cleric casts");
            p.begin_combat();
            p.apply_combat(c->snapshot());
            p.end_combat();
            check(p.member(id).vitals == old && p.member(id).equipment.weapon_hands == 2,
                  "Handoff retains wounds, pools and grip");
            const auto saved = encode_campaign(p, nullptr, "sacred");
            CampaignParty restored(module());
            restored.restore(decode_campaign(saved, *creation, *rules, "sacred", nullptr).party);
            check(encode_campaign(restored, nullptr, "sacred") == saved &&
                  restored.profile(id).data == p.profile(id).data,
                  "Campaign replay preserves explicit choices and all resources exactly");
        }
}

void legacy()
{
    auto rules = module();
    auto creation = srd5::character_rules();
    const auto base = root / "tests/fixtures";
    auto upgrade = [&](std::string s)
    {
        auto at = s.find("0.6.22");
        check(at != s.npos, "Old identity exists");
        s.replace(at, 6, rules->identity().version);
        return s;
    };
    const auto old = read(base / "campaign-v11-sacred.ogs");
    CampaignParty p(module());
    p.restore(decode_campaign(old, *creation, *rules, "sacred", nullptr).party);
    auto body = [](const auto & s)
    {
        return s.substr(s.find('\n', s.find('\n') + 1) + 1);
    };
    check(body(encode_campaign(p, nullptr, "sacred")) ==
          test::with_background_training_grants(body(upgrade(old))),
          "Prior campaign changes only module identity and owed background grants");
    check(rules->spell_access(p.member(1).character.sheet()).cantrips.empty(),
          "No Sacred Flame appears in historical Cleric saves");
    auto c = rules->restore(read(base / "combat-v13-sacred.save"));
    check(c->save() == upgrade(read(base / "combat-v13-sacred.save")),
          "Old combat keeps every recipe, pool, wound, clock and RNG byte");
    check(c->submit(command(*c, "cure_wounds", 1)), "Prior Cure Wounds continuation");
    check(c->save() == rules->restore(read(base / "combat-v13-sacred-continued.save"))->save(),
          "Actual prior writer healing continuation is identical");
}

void ui_fixtures()
{
    const auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "sacred-fixtures";
    std::filesystem::create_directories(path);
    auto rules = module();
    for (const auto &name :
            {"known", "blocked", "unknown"
            })
    {
        const auto h = hero(3, std::string_view(name) != "unknown");
        const auto profile =
            rules->character_profile(h.sheet(), std::string_view(name) == "blocked"
                                     ? std::vector<std::string> {"mace", "shield"}
                                     : std::vector<std::string> {"quarterstaff"});
        auto c =
        rules->create({{12, 9, std::vector<std::uint8_t>(108)},
            {   {1, "campaign-character", "Sacred Cleric", 0, {1, 1}, profile.data},
                {2, "vanguard", "Ally", 0, {3, 1}},
                {99, "vanguard", "Enemy", 1, {5, 1}}
            }},
        2);
        write(path / (std::string(name) + ".save"), c->save());
    }
}
} // namespace

int main(int argc, char **)
{
    try
    {
        if (argc == 2)
        {
            freeze();
            return 0;
        }
        access();
        rolls();
        targets();
        modifiers();
        campaign();
        legacy();
        ui_fixtures();
        std::cout << "Sacred Flame tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
