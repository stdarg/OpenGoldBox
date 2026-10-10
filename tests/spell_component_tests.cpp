#include "opengold/campaign_save.h"
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

const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

auto module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

void write(const std::filesystem::path &p, const std::string &bytes)
{
    std::ofstream out(p, std::ios::binary);
    out << bytes;
    check(bool(out), "Fixture written");
}

Character hero(std::string klass)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = klass;
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Component tester";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    Character c(*srd5::character_rules(), d, {});
    auto rules = module();
    VitalState vital{c.sheet().hit_points, false, {}};
    for (unsigned level = 2; level <= 3; ++level)
    {
        auto choice = rules->default_advancement(c.sheet());
        // A Cleric prepares every available spell up to its count; a Wizard
        // keeps the default preparation of its book.
        if (klass == "cleric")
            choice.spells = {"cure_wounds", "healing_word", "inflict_wounds", "shield_of_faith",
                             "bless"
                            };
        if (level == 3)
        {
            for (const auto *spell : {"blindness", "scorching_ray"})
                if (klass == "wizard" &&
                        std::find(choice.spells.begin(), choice.spells.end(), spell) == choice.spells.end())
                    choice.spells.push_back(spell);
            if (klass == "cleric")
                choice.spells.push_back("blindness");
            // The Life Domain keeps Bless and Cure Wounds prepared from level three.
            if (klass == "cleric")
                choice.spells = {"healing_word", "inflict_wounds", "shield_of_faith", "blindness",
                                 "command", "protection_from_evil_and_good"
                                };
        }
        check(c.advance(*rules, vital, choice),
              "Level-three caster prepared through real advancement");
    }
    return c;
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
    for (const auto &a : c.legal_commands())
        if (a.verb == verb)
            return true;
    return false;
}

Command command(const CombatSession &c, std::string_view verb)
{
    for (const auto &a : c.legal_commands())
        if (a.verb == verb)
            return a;
    throw std::runtime_error("Missing command: " + std::string(verb));
}

auto battle(const RulesModule &rules, const Character &h, const std::vector<std::string> &gear)
{
    const auto p = rules.character_profile(h.sheet(), gear);
    auto c = rules.create({{8, 8, std::vector<Terrain>(64)},
        {   {
                1,
                "campaign-character",
                "Caster",
                Side::party,
                {1, 1},
                p.data,
                VitalState{h.sheet().hit_points - 10, false, {}}
            },
            {99, "vanguard", "Enemy", Side::opposition, {3, 1}}
        }},
    2);
    if (c->snapshot().actor != 1)
        check(c->submit(command(*c, "end")),
              "Reach the caster turn after armor initiative penalty");
    check(c->snapshot().actor == 1, "Caster has its turn");
    return c;
}

unsigned slots(const CombatantView &actor, unsigned level)
{
    std::istringstream in(actor.persistent.resources);
    std::string magic;
    unsigned winds{}, first{}, second{};
    in >> magic >> winds >> first >> second;
    check(bool(in) && magic == "SRD11", "Level-three caster uses stored level-one/two slots");
    return level == 1 ? first : second;
}

// Spells have no components (CLASS-11): weapons, wands and a shield in hand
// never block casting; untrained armor still does.
void expectations()
{
    auto rules = module();
    for (const auto &klass :
            {"cleric", "wizard"
            })
    {
        const auto h = hero(klass);
        const bool wizard = std::string_view(klass) == "wizard";
        const std::vector<std::string> spells =
            wizard ? std::vector<std::string> {"fire_bolt", "magic_missile", "magic_missile_2",
                                               "scorching_ray", "blindness"
                                              }
            :
            std::vector<std::string> {"cure_wounds", "cure_wounds_2", "healing_word", "blindness"};
        for (const auto &gear : std::vector<std::vector<std::string>> {{},
        {"shield"},
        {"mace"},
        {"wand"},
        {"greatsword"},
        {"longbow"},
        {"quarterstaff"},
        {"mace", "shield"},
        {"wand", "shield"},
        {"quarterstaff", "shield"}
    })
        {
            const auto p = rules->character_profile(h.sheet(), gear);
            check(p.spell_modifiers.find("Somatic") == std::string::npos,
                  "No Modifiers text about occupied hands");
            auto c = battle(*rules, h, gear);
            const auto initial = c->save();
            for (const auto &verb : spells)
            {
                check(has(*c, verb), "Full hands never block a spell");
                auto cast = rules->restore(initial);
                const auto before = unit(*cast);
                check(cast->submit(command(*cast, verb)), "The spell resolves normally");
                check(unit(*cast).armor_class == before.armor_class,
                      "Casting keeps the shield's AC");
                const unsigned spent = verb == "fire_bolt"                               ? 0
                                       : verb.ends_with("_2") || verb == "scorching_ray" ||
                                       verb == "blindness" ? 2
                                       : 1;
                for (unsigned level :
                        {
                            1u, 2u
                        })
                    check(slots(unit(*cast), level) ==
                          slots(before, level) - (spent == level ? 1 : 0),
                          "Casting spends exactly its chosen slot; a cantrip spends none");
                check(rules->restore(cast->save())->save() == cast->save(),
                      "Resolved cast round trips exactly");
            }
        }
        auto armored = battle(*rules, h, {"plate"});
        check(!has(*armored, "blindness") && !has(*armored, spells.front()),
              "Untrained armor still prohibits casting");
    }
}

CampaignParty party(bool npc = false)
{
    CampaignParty p(module());
    auto h = hero("cleric");
    h.add_item({.definition_id = "mace", .name = "Mace"});
    h.add_item({.definition_id = "shield", .name = "Shield"});
    const auto id = npc ? p.recruit("fixture:cleric", std::move(h)) : p.add_pc(std::move(h));
    p.equip(id, 1);
    p.equip(id, 2);
    auto state = p.checkpoint();
    state.roster[0].vitals.hit_points -= 10;
    state.time_minutes = 123;
    state.subminute_milliseconds = 456;
    state.random_state.value = 789;
    state.roster[0].wealth[3] = 37;
    p.restore(std::move(state));
    return p;
}

auto campaign_battle(const RulesModule &rules, const CampaignParty &p)
{
    auto actors = p.participants();
    actors[0].cell = {1, 1};
    actors.push_back({99, "vanguard", "Enemy", Side::opposition, {3, 1}});
    return rules.create({{8, 8, std::vector<Terrain>(64)}, actors}, 2);
}

void campaign()
{
    auto rules = module();
    for (bool npc :
            {
                false, true
            })
    {
        auto p = party(npc);
        const auto bytes = encode_campaign(p, nullptr, "components");
        CampaignParty copy(module());
        copy.restore(
            decode_campaign(bytes, *srd5::character_rules(), *rules, "components", nullptr).party);
        check(encode_campaign(copy, nullptr, "components") == bytes,
              "Equipment, wounds, advancement, resources and clocks survive campaign reload");
        auto armed = campaign_battle(*rules, copy);
        check(has(*armed, "cure_wounds") && has(*armed, "healing_word"),
              "A Cleric with mace and shield casts in a party encounter");
    }
}

void ui_fixtures()
{
    const auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "component-fixtures";
    std::filesystem::create_directories(path);
    auto rules = module();
    for (const auto &klass :
            {"cleric", "wizard"
            })
        for (bool shield :
                {
                    false, true
                })
        {
            const auto h = hero(klass);
            std::vector<std::string> gear{"quarterstaff"};
            if (shield)
                gear.push_back("shield");
            write(path / (std::string(klass) + (shield ? "-shield.save" : "-free.save")),
                  battle(*rules, h, gear)->save());
        }
}
} // namespace

int main()
{
    try
    {
        expectations();
        campaign();
        ui_fixtures();
        std::cout << "Spell component tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
