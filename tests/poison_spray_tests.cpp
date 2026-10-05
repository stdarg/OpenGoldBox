#include <sstream>
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
    d.character_class = "wizard";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Cantrip tester";
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

Character hero(unsigned level = 1, bool poison = true)
{
    auto d = draft();
    d.cantrips = poison ? std::vector<std::string> {"fire_bolt", "poison_spray"}
                 :
                 std::vector<std::string> {"fire_bolt"};
    Character h(*srd5::character_rules(), d, {});
    VitalState scratch;
    for (unsigned n = 1; n < level; ++n)
        check(h.advance(*module(), scratch), "Advance real Wizard choices");
    return h;
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
    unsigned wizards = 0;
    for (const auto &preset : character_pool(*creation, art))
        if (preset.sheet().character_class == "Wizard")
        {
            ++wizards;
            check(preset.creation_data().cantrips ==
                  std::optional{std::vector<std::string>{"fire_bolt", "poison_spray",
                          "ray_of_frost"
                                                        }} &&
                  rules->spell_access(preset.sheet()).cantrips.size() == 3,
                  "Preset Wizards arrive with pre-generated available cantrips");
            const auto &book = preset.creation_data().spells->learning.at("spellbook:1");
            const auto &prepared = *preset.creation_data().spells->prepared;
            check(preset.creation_data().spells && !book.empty() && book.front() == "magic_missile" &&
                  !prepared.empty() && prepared.front() == "magic_missile",
                  "Preset Wizards pre-generate independent book and preparation selections");
        }
    check(wizards > 0, "Preset Wizard path exercised");
    const auto original = creation->evaluate(d, true);
    check(rules->spell_access(original).cantrips.size() == 1 &&
          rules->spell_access(original).cantrips[0].id == "fire_bolt",
          "Missing draft choices retain legacy Fire Bolt only");
    d.cantrips.emplace();
    auto sheet = creation->evaluate(d, true);
    check(rules->spell_access(sheet).cantrips.empty(),
          "Explicit empty selection remains pending without silently refilling");
    d.cantrips = std::vector<std::string> {"poison_spray"};
    sheet = creation->evaluate(d, true);
    const auto access = rules->spell_access(sheet);
    check(access.cantrip_choices == 3 && access.cantrips.size() == 1 &&
          access.cantrips[0].id == "poison_spray" &&
          access.cantrips[0].source_id == "class:wizard:spellcasting" &&
          access.cantrips[0].acquired_level == 1,
          "Actual creation selection has Wizard source and first acquisition level");
    for (auto bad : std::vector<std::vector<std::string>>
{
    {"poison_spray", "poison_spray"},
    {"magic_missile"},
    {"unknown"},
    {"poison_spray", "fire_bolt", "fire_bolt", "poison_spray"}
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
        if (klass.id != "wizard" && klass.id != "warlock" && klass.id != "sorcerer")
        {
            d = draft();
            d.character_class = klass.id;
            d.cantrips = std::vector<std::string> {"poison_spray"};
            rejects(
                [&]
            {
                (void)creation->evaluate(d, true);
            });
        }
    for (unsigned level = 1; level <= 4; ++level)
    {
        auto h = hero(level);
        auto current = rules->spell_access(h.sheet());
        check(current.cantrip_choices == (level == 4 ? 4u : 3u) &&
              current.cantrips.size() == (level == 4 ? 3u : 2u) &&
              current.cantrips[1].acquired_level == 1,
              "Advancement retains chosen cantrips, source and correct entitlement");
    }
    auto profile = rules->character_profile(hero().sheet(), {}).data;
    check(profile.starts_with("PC42 1 ") && profile.find(" poison_spray ") != profile.npos,
          "Explicit cantrip choice belongs to the profile recipe");
    profile.replace(0, 4, "PC10");
    rejects(
        [&]
    {
        (void)rules->create({{8, 8, std::vector<std::uint8_t>(64)},
            {   {1, "campaign-character", "Forged", 0, {1, 1}, profile},
                {2, "vanguard", "Enemy", 1, {3, 1}}
            }},
        13);
    });
    auto invalid = hero().sheet();
    for (auto &g : invalid.grants)
        if (g.id == "spell:poison_spray")
            g.source_id = "species:tiefling";
    rejects(
        [&]
    {
        (void)rules->character_profile(invalid, {});
    });
}

void rolls()
{
    // Independent SplitMix64 golden values: two initiative rolls, then attack.
    // seed 0: natural 20, 5+8 damage; 13: natural 17, 8 damage; 40: natural 1.
    for (unsigned level = 1; level <= 4; ++level)
        for (unsigned seed :
                {
                    0u, 13u, 40u
                })
            for (const auto &defense :
                    std::vector<std::string> {"", "resistance", "vulnerability", "immunity"})
            {
                auto rules =
                    custom(defense.empty() ? "" : "affinity target test " + defense + " poison\n");
                const auto h = hero(level);
                auto c = battle(*rules, h, seed);
                auto copy = rules->restore(c->save());
                const auto before = unit(*c);
                const auto ticket = command(*c, "poison_spray", 2);
                const auto start_rng = rng(*c);
                check(c->submit(ticket) && copy->submit(ticket) && c->save() == copy->save(),
                      "Actual selected Poison Spray resumes identically after checkpoint");
                const auto hit = attack(*c);
                check(
                    arg(hit, "roll") == std::to_string(seed == 0    ? 20
                                                       : seed == 13 ? 17
                                                       : 1) &&
                    arg(hit, "bonus") == std::to_string(level == 4 ? 6 : 5),
                    "Spell attack uses Intelligence and proficiency, not weapon modifier or a save");
                const int raw = seed == 0    ? 13
                                : seed == 13 ? 8
                                : 0,
                                expected = defense == "immunity"        ? 0
                                           : defense == "resistance"    ? raw / 2
                                           : defense == "vulnerability" ? raw * 2
                                           : raw;
                check(
                    unit(*c, 2).hit_points == 1000 - expected,
                    "d12 Poison damage, critical doubling and typed defenses match independent values");
                const auto after = unit(*c);
                check(!after.action && after.bonus_action && after.reaction &&
                      after.movement_feet == before.movement_feet &&
                      after.persistent == before.persistent,
                      "Cantrip spends Action only; slots, resources and movement stay available");
                check(rng(*c) == start_rng + 0x9e3779b97f4a7c15ULL * (seed == 0    ? 3u
                        : seed == 13 ? 2u
                        : 1u),
                      "No extra saving throw, damage modifier or RNG draw");
                check(unit(*c, 2).conditions.empty(), "Poison damage does not impose Poisoned");
                const auto saved = c->save();
                check(!c->submit(ticket) && c->save() == saved, "Stale cast rejects atomically");
            }
    auto rules = custom();
    auto c = battle(*rules, hero(), 13, {2, 1});
    check(c->submit(command(*c, "poison_spray", 2)), "Nearby spell attack");
    check(arg(attack(*c), "roll") == "8" && arg(attack(*c), "disadvantage") == " (disadvantage)" &&
          unit(*c, 2).hit_points == 996,
          "Conscious adjacent hostile imposes ranged-attack Disadvantage");
}

void eligibility()
{
    auto rules = custom();
    const auto h = hero();
    for (int feet :
            {
                5, 30, 35
            })
    {
        auto c = battle(*rules, h, 13, {1 + feet / 5, 1});
        check(has(*c, "poison_spray", 2) == (feet <= 30), "Range includes 30 feet and excludes 35");
        if (feet > 30)
        {
            const auto before = c->save();
            check(!c->submit({c->snapshot().revision, 1, 2, "poison_spray"}) && c->save() == before,
                  "Out-of-range cast preserves all state");
        }
    }
    for (const auto &gear : std::vector<std::vector<std::string>> {{},
    {"shield"},
    {"quarterstaff"},
    {"longbow"},
    {"wand", "shield"},
    {"quarterstaff", "shield"},
    {"plate"}
})
    {
        auto c = battle(*rules, h, 13, {3, 1}, gear);
        while (c->snapshot().actor != 1)
            check(c->submit(command(*c, "end")), "Reach armored caster");
        check(unit(*c).known_cantrips == std::vector<std::string> {"fire_bolt", "poison_spray"},
              "Knowledge remains visible while armor or hands block casting");
        const bool allowed = gear.size() < 2 && (gear.empty() || gear[0] != "plate");
        check(has(*c, "poison_spray", 2) == allowed,
              "Sourced spell honors occupied hands and untrained armor");
        if (!allowed)
        {
            const auto before = c->save();
            check(!c->submit({c->snapshot().revision, 1, 2, "poison_spray"}) && c->save() == before,
                  "Unavailable Somatic cast is atomic");
        }
    }
    auto unknown = battle(*rules, hero(1, false));
    check(!has(*unknown, "poison_spray"), "Unlearned cantrip is never auto-granted to Wizards");
    auto profile = rules->character_profile(h.sheet(), {});
    Battlefield board{10, 8, std::vector<std::uint8_t>(80)};
    board.terrain[1 * 10 + 2] = 1;
    auto blocked = rules->create({board,
        {   {1, "campaign-character", "Caster", 0, {1, 1}, profile.data},
            {2, "target", "Enemy", 1, {3, 1}}
        }},
    13);
    check(!has(*blocked, "poison_spray", 2), "Opaque obstruction blocks the path");
    auto normal = battle(*rules, h);
    const auto before = normal->save();
    for (EntityId bad :
            {
                0u, 999u
            })
        check(!normal->submit({normal->snapshot().revision, 1, bad, "poison_spray"}) &&
              normal->save() == before,
              "No terrain/object/unknown target can be forged");
    check(has(*normal, "poison_spray", 1), "A creature-targeting spell can target its own caster");
    const auto hp = unit(*normal).hit_points;
    check(normal->submit(command(*normal, "poison_spray", 1)) && unit(*normal).hit_points == hp - 8,
          "Self-targeting resolves an actual spell attack and damage");
    auto dead = rules->create({{10, 8, std::vector<std::uint8_t>(80)},
        {   {1, "campaign-character", "Caster", 0, {1, 1}, profile.data},
            {2, "target", "Dead", 1, {3, 1}, {}, VitalState{0, true, {}}},
            {3, "target", "Enemy", 1, {8, 1}}
        }},
    13);
    check(!has(*dead, "poison_spray", 2), "A corpse is not an eligible creature target");
    const auto dead_before = dead->save();
    check(!dead->submit({dead->snapshot().revision, 1, 2, "poison_spray"}) &&
          dead->save() == dead_before,
          "Dead-target rejection preserves all state");
}

void allies_and_unconscious()
{
    auto rules = custom();
    const auto h = hero();
    auto profile = rules->character_profile(h.sheet(), {});
    for (bool unconscious :
            {
                false, true
            })
        for (bool adjacent :
                {
                    false, true
                })
        {
            Encounter e{{10, 8, std::vector<std::uint8_t>(80)},
                {   {1, "campaign-character", "Caster", 0, {1, 1}, profile.data},
                    {
                        2,
                        "target",
                        "Ally",
                        0,
                        {adjacent ? 2 : 3, 1},
                        {},
unconscious ? std::optional<VitalState>{{0, false, {}}} :
                        std::nullopt
                    },
                    {3, "target", "Enemy", 1, {8, 1}}
                }};
            auto c = rules->create(e, 13);
            while (c->snapshot().actor != 1)
                check(c->submit(command(*c, "end")), "Reach caster with third initiative slot");
            check(has(*c, "poison_spray", 2),
                  "Living ally remains a legal target, including at zero HP");
            auto copy = rules->restore(c->save());
            const auto ticket = command(*c, "poison_spray", 2);
            check(c->submit(ticket) && copy->submit(ticket) && c->save() == copy->save(),
                  "Ally target and mortality state resume identically");
            const auto hit = attack(*c);
            check(arg(hit, "disadvantage") == (unconscious && adjacent ? " (advantage)" : ""),
                  "Unconscious gives Advantage; distant Prone cancels it");
            if (unconscious)
            {
                check(
                    arg(hit, "hit") == (adjacent ? "CRITICAL" : "hits"),
                    "Any close unconscious hit is critical without changing the rolled natural value");
                check(unit(*c, 2).persistent.description.find(
                          adjacent ? "2 failures" : "1 failures") != std::string::npos,
                      "Unconscious damage adds critical or ordinary death failures");
            }
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
        for (unsigned level = 1; level <= 4; ++level)
        {
            CampaignParty p(module());
            auto h = hero(level);
            h.inventory().add("quarterstaff", "Quarterstaff");
            const auto id =
                npc ? p.recruit("fixture:poison", std::move(h)) : p.add_pc(std::move(h));
            p.equip(id, 1);
            auto state = p.checkpoint();
            state.roster[0].vitals.hit_points -= 3;
            state.random_state = 123;
            state.roster[0].wealth[3] = 37;
            p.restore(std::move(state));
            auto actors = p.participants();
            actors[0].cell = {1, 1};
            actors.push_back({99, "vanguard", "Enemy", 1, {5, 1}});
            auto c = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, actors}, 13);
            const auto old = unit(*c, id).persistent;
            check(c->submit(command(*c, "poison_spray", 99)),
                  "Ordinary party grants produce actual casting commands");
            p.begin_combat();
            p.apply_combat(c->snapshot());
            p.end_combat();
            check(p.member(id).vitals == old && p.profile(id).weapon_hands == 2,
                  "Cantrip handoff retains wounds, pools and the two-handed staff");
            const auto saved = encode_campaign(p, nullptr, "poison");
            check(saved.starts_with("OPENGOLD-CAMPAIGN 21\n"),
                  "Every campaign uses the current save format");
            CampaignParty restored(module());
            restored.restore(decode_campaign(saved, *creation, *rules, "poison", nullptr).party);
            check(encode_campaign(restored, nullptr, "poison") == saved &&
                  restored.member(id).character.creation_data().cantrips ==
                  p.member(id).character.creation_data().cantrips,
                  "Replay preserves chosen cantrips, history and resources exactly");
            check(restored.member(id).wealth[3] == 37 &&
                  restored.profile(id).data == p.profile(id).data,
                  "Inventory, wealth, equipment and cast access retained");
        }
}

void ui_fixtures()
{
    const auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "poison-fixtures";
    std::filesystem::create_directories(path);
    auto rules = module();
    for (const auto &name :
            {"known", "blocked", "unknown"
            })
    {
        const auto h = hero(3, std::string_view(name) != "unknown");
        const auto profile =
            rules->character_profile(h.sheet(), std::string_view(name) == "blocked"
                                     ? std::vector<std::string> {"wand", "shield"}
                                     : std::vector<std::string> {"quarterstaff"});
        const auto c =
        rules->create({{12, 9, std::vector<std::uint8_t>(108)},
            {   {1, "campaign-character", "Poison Wizard", 0, {1, 1}, profile.data},
                {2, "vanguard", "Ally", 0, {3, 1}},
                {99, "vanguard", "Enemy", 1, {5, 1}}
            }},
        2);
        write(path / (std::string(name) + ".save"), c->save());
    }
}

} // namespace

int main()
{
    try
    {
        access();
        rolls();
        eligibility();
        allies_and_unconscious();
        campaign();
        ui_fixtures();
        std::cout << "Poison Spray tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
