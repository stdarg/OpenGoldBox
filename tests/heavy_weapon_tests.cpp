#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <array>
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
    check(caught, "Invalid command must reject");
}

std::string read(const std::filesystem::path &path)
{
    std::ifstream in(path);
    check(bool(in), "Fixture exists");
    return {std::istreambuf_iterator<char>(in), {}};
}

auto module(bool target = false)
{
    auto content =
        read(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/rules/srd-5.2.1/combat.rules");
    if (target)
        content += "\ncreature target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n";
    return srd5::parse_content(content);
}

Character hero(std::string klass = "fighter", int strength = 12, int dexterity = 15,
               std::string race = "human")
{
    CharacterDraft d;
    d.race = race;
    d.gender = "female";
    d.character_class = klass;
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Heavy tester";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.rolls[0] = {{strength - 8, 4, 4, 1}, 3};
    d.rolls[1] = {{dexterity - 8, 4, 4, 1}, 3};
    // 12 and 13 use the same modifier: only the Heavy threshold differs.
    if (strength == 15)
        d.rolls[0] = {{6, 5, 4, 1}, 3};
    if (dexterity == 15)
        d.rolls[1] = {{6, 5, 4, 1}, 3};
    return Character(*srd5::character_rules(), d, {});
}

CombatantView unit(const CombatSession &c, EntityId id = 1)
{
    for (const auto &u : c.snapshot().combatants)
        if (u.id == id)
            return u;
    throw std::runtime_error("Missing actor");
}

Command command(const CombatSession &c, std::string_view verb)
{
    for (const auto &a : c.legal_commands())
        if (a.verb == verb)
            return a;
    throw std::runtime_error("Missing command: " + std::string(verb));
}

void act(CombatSession &c, std::string_view verb)
{
    check(c.submit(command(c, verb)), "Action accepted");
}

std::uint64_t rng(const CombatSession &c)
{
    std::istringstream in(c.save());
    std::string row;
    for (int i = 0; i < 3; ++i)
        std::getline(in, row);
    std::uint64_t value{};
    in >> value;
    return value;
}

std::string argument(const Message &m, std::string_view key)
{
    for (const auto &a : m.arguments)
        if (a.name == key)
            return a.value;
    throw std::runtime_error("Missing message argument");
}

Message attack(const CombatSession &c)
{
    for (const auto &m : c.snapshot().log_messages)
        if (m.source.starts_with("{actor} -> {target}: d20"))
            return m;
    throw std::runtime_error("Missing attack result");
}

auto battle(const RulesModule &rules, const Character &h, const std::vector<std::string> &gear,
unsigned seed = 13, Cell target = {2, 1}, bool blind_target = false,
bool blind_hero = false)
{
    const auto profile = rules.character_profile(h.sheet(), gear);
    const std::string effect = "SRD11 0 0 0 0 0 0 0 0 0 0 \"\" 0 0 0 0 0 0 "
                               "FX8 2 1 1 1 77 99 \"Source caster\" 38 60000 6000 0";
    Encounter e{{40, 8, std::vector<std::uint8_t>(320)},
        {   {1, "campaign-character", "Hero", 0, {1, 1}, profile.data},
            {2, "target", "Target", 1, target}
        }};
    if (blind_target)
        e.participants[1].state = VitalState{1000, false, effect};
    if (blind_hero)
        e.participants[0].state = VitalState{h.sheet().hit_points, false, effect};
    auto c = rules.create(e, seed);
    check(c->snapshot().actor == 1, "Golden seed begins with the hero");
    return c;
}

struct Weapon
{
    const char *key;
    bool ranged;
    int critical, normal, disadvantaged;
};

// Independent SRD pp.89/91 and fixed SplitMix seed oracles. Seed 0 rolls
// initiative 16/1 then attack 20/5. Seed 13 rolls 16/2 then 17/8.
// Damage includes +1 ability; each table entry describes the actual dice.
constexpr std::array weapons
{
    Weapon{"glaive", false, 14, 9, 9},       Weapon{"greatsword", false, 15, 7, 8},
    Weapon{"halberd", false, 14, 9, 9},      Weapon{"pike", false, 14, 9, 9},
    Weapon{"longbow", true, 10, 5, 5},       Weapon{"greataxe", false, 14, 9, 5},
    Weapon{"lance", false, 14, 9, 9},        Weapon{"maul", false, 15, 7, 8},
    Weapon{"heavy_crossbow", true, 14, 9, 9}};

void thresholds()
{
    auto rules = module(true);
    unsigned classes = 0;
    for (const auto &klass : srd5::character_rules()->choices(CreationField::character_class))
    {
        ++classes;
        for (const auto &w : weapons)
            for (const int score :
                    {
                        12, 13
                    })
            {
                const auto h = hero(klass.id, w.ranged ? 15 : score, w.ranged ? score : 15);
                // Honor the existing character creator's house prerequisites.
                const auto requirements = srd5::character_rules()->class_requirements(klass.id);
                const auto qualifies = [&](unsigned ability)
                {
                    return h.sheet().scores[ability] >= 13;
                };
                if (requirements.any ? !std::any_of(requirements.abilities.begin(),
                                                    requirements.abilities.end(), qualifies)
                        : !std::all_of(requirements.abilities.begin(),
                                       requirements.abilities.end(), qualifies))
                    continue;
                const auto profile =
                    rules->character_profile(h.sheet(), std::array<std::string, 1> {w.key});
                check(!profile.strength_dexterity_disadvantage && profile.movement_feet == 30,
                      "Heavy never applies armor/check/save/initiative or speed penalties");
                bool explained = false;
                for (const auto &m : profile.item_messages)
                    if (m.source.find("(Heavy)") != m.source.npos)
                    {
                        auto label = argument(m, "item");
                        for (auto &ch : label)
                        {
                            if (ch == ' ')
                                ch = '_';
                            else if (ch >= 'A' && ch <= 'Z')
                                ch = char(ch - 'A' + 'a');
                        }
                        explained =
                            label == w.key && argument(m, "score") == std::to_string(score) &&
                            argument(m, "ability") == (w.ranged ? "Dexterity" : "Strength") &&
                            (m.source.find("Disadvantage") != m.source.npos) == (score == 12);
                    }
                check(
                    explained && profile.item_modifiers.find("(Heavy)") != std::string::npos,
                    "Existing Modifiers presentation identifies item, required ability, current score and result");
                for (unsigned seed :
                        {
                            0u, 13u, 40u
                        })
                {
                    auto c = battle(*rules, h, {w.key}, seed, w.ranged ? Cell{3, 1} : Cell{2, 1});
                    check(unit(*c).initiative == (seed == 40 ? 19 : 16) + h.sheet().modifiers[1],
                          "Heavy never consumes a second initiative roll");
                    auto copy = rules->restore(c->save());
                    const auto before = unit(*c);
                    const auto ticket = command(*c, w.ranged ? "ranged" : "melee");
                    check(c->submit(ticket) && copy->submit(ticket) && c->save() == copy->save(),
                          "Saved attack resumes with identical Heavy result and RNG");
                    const auto result = attack(*c);
                    check(argument(result, "roll") == std::to_string(seed == 40 ? 1
                            : score == 12
                            ? (seed == 0 ? 5 : 8)
                            : (seed == 0 ? 20 : 17)),
                          "Attack selects the independently known one/two-d20 result");
                    check(argument(result, "disadvantage") ==
                          (score == 12 ? " (disadvantage)" : ""),
                          "Only scores below 13 have the Heavy attack penalty");
                    const int damage = seed == 40    ? 0
                                       : score == 13 ? (seed == 0 ? w.critical : w.normal)
                                       : seed == 13  ? w.disadvantaged
                                       : (std::string_view(w.key) == "greatsword" ||
                                          std::string_view(w.key) == "maul")
                                       ? 4
                                       : std::string_view(w.key) == "longbow" ? 5
                                       : 9;
                    check(unit(*c, 2).hit_points == 1000 - damage,
                          "Heavy changes the attack roll, never the damage dice or modifier");
                    const auto after = unit(*c);
                    check(!after.action && after.bonus_action == before.bonus_action &&
                          after.reaction == before.reaction &&
                          after.movement_feet == before.movement_feet &&
                          after.persistent == before.persistent,
                          "Weapon attack spends only its Action, never extra resources");
                    const auto saved = c->save();
                    check(!c->submit(ticket) && c->save() == saved,
                          "Stale attack rejection remains atomic");
                }
            }
    }
    check(classes == 12, "Every SRD class is exercised");
    for (const char *race :
            {"halfling", "gnome"
            })
    {
        auto c = battle(*rules, hero("fighter", 13, 13, race), {"greatsword"});
        act(*c, "melee");
        check(argument(attack(*c), "disadvantage").empty(),
              "Small species with Strength 13 are not penalized by the obsolete 2014 rule");
    }
}

void contextual_modifiers()
{
    auto rules = module(true);
    const auto weak = hero("wizard", 12, 12);
    for (const std::string key :
            {"mace", "quarterstaff", "light_crossbow", "shortbow", "longsword"
            })
    {
        const bool ranged = key == "light_crossbow" || key == "shortbow";
        auto c = battle(*rules, weak, {key}, 13, ranged ? Cell{3, 1} : Cell{2, 1});
        act(*c, ranged ? "ranged" : "melee");
        check(
            argument(attack(*c), "roll") == "17" && argument(attack(*c), "disadvantage").empty(),
            "Non-Heavy weapons do not inherit the requirement, including other two-handed weapons");
    }
    for (const auto key :
            {"longbow", "greatsword"
            })
    {
        auto spell = battle(*rules, weak, {key}, 13, {3, 1});
        auto control = battle(*rules, weak, {}, 13, {3, 1});
        act(*spell, "fire_bolt");
        act(*control, "fire_bolt");
        check(argument(attack(*spell), "roll") == "17" && rng(*spell) == rng(*control) &&
              unit(*spell, 2).hit_points == unit(*control, 2).hit_points,
              "Heavy equipment cannot penalize spell attacks or disable casting");
    }
    auto unarmed = battle(*rules, weak, {"longbow"});
    act(*unarmed, "melee");
    check(argument(attack(*unarmed), "roll") == "17" && unit(*unarmed, 2).hit_points == 998,
          "Unarmed fallback with a Heavy ranged weapon is unaffected");
    auto armor = battle(*rules, weak, {"greatsword", "leather"}, 40);
    auto armor_only = battle(*rules, hero("wizard", 13, 12), {"greatsword", "leather"}, 40);
    act(*armor, "melee");
    act(*armor_only, "melee");
    check(argument(attack(*armor), "roll") == argument(attack(*armor_only), "roll") &&
          rng(*armor) == rng(*armor_only) &&
          unit(*armor, 2).hit_points == unit(*armor_only, 2).hit_points,
          "Untrained armor and Heavy still roll only two attack dice");
    // Untrained armor consumes an additional initiative die, so use Blinded
    // on both sides to test cancellation without changing the golden RNG input.
    auto canceled = battle(*rules, weak, {"greatsword"}, 13, {2, 1}, true, true);
    act(*canceled, "melee");
    check(argument(attack(*canceled), "roll") == "17" &&
          argument(attack(*canceled), "disadvantage").empty(),
          "One Advantage source cancels Heavy plus Blinded Disadvantage");
    auto far = battle(*rules, weak, {"longbow"}, 13, {33, 1});
    auto near = battle(*rules, weak, {"longbow"}, 13, {2, 1});
    act(*far, "ranged");
    act(*near, "ranged");
    check(argument(attack(*far), "roll") == "8" && argument(attack(*near), "roll") == "8" &&
          rng(*far) == rng(*near),
          "Heavy combines with long-range or nearby-enemy Disadvantage without extra dice");
}

void campaign()
{
    auto rules = module(true);
    CampaignParty party(module(true));
    auto h = hero();
    const auto sword = h.inventory().add("greatsword", "Greatsword", 1),
               bow = h.inventory().add("longbow", "Longbow", 1),
               shield = h.inventory().add("shield", "Shield", 1);
    const auto id = party.add_pc(std::move(h));
    party.equip(id, sword);
    auto state = party.checkpoint();
    state.roster[0].vitals = {5, false, "SRD11 1 0 0 0 0 0 1 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0"};
    party.restore(state);
    const auto vitals = party.member(id).vitals;
    const auto saved = encode_campaign(party, nullptr, "heavy");
    rejects(
        [&]
    {
        party.equip(id, shield);
    });
    check(encode_campaign(party, nullptr, "heavy") == saved,
          "Rejected shield preserves Heavy equipment and expenditure");
    party.equip(id, bow);
    check(party.profile(id).item_modifiers.find("Requirement met.") != std::string::npos &&
          party.member(id).vitals == vitals,
          "Switching weapon re-evaluates its own ability without recharging");
    party.unequip(id, bow);
    check(party.profile(id).item_modifiers.find("(Heavy)") == std::string::npos,
          "Unequipping removes the source");
    party.equip(id, sword);
    party.award_experience(2700, "heavy-asi");
    for (unsigned level = 2; level <= 4; ++level)
    {
        auto choice = party.default_advancement(id);
        if (level == 4)
        {
            choice.abilities = {};
            choice.abilities[0] = 1;
            choice.abilities[4] = 1;
        }
        party.advance(id, choice);
    }
    check(party.member(id).character.sheet().scores[0] == 13 &&
          party.profile(id).item_modifiers.find("Requirement met.") != std::string::npos,
          "Level-four ASI crossing 13 removes the penalty without changing the +1 modifier");
    const auto bytes = encode_campaign(party, nullptr, "heavy");
    CampaignParty copy(module(true));
    copy.restore(decode_campaign(bytes, *srd5::character_rules(), *rules, "heavy", nullptr).party);
    check(encode_campaign(copy, nullptr, "heavy") == bytes,
          "Campaign reconstruction retains threshold-crossing advancement and spent resources");
    auto actors = copy.participants();
    actors[0].cell = {1, 1};
    actors.push_back({99, "target", "Target", 1, {2, 1}});
    auto c = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, actors}, 13);
    act(*c, "melee");
    check(argument(attack(*c), "roll") == "8" && argument(attack(*c), "disadvantage").empty(),
          "Next campaign encounter uses the advanced score");
}

void opportunity_attack()
{
    auto rules = module(true);
    auto c = battle(*rules, hero("fighter", 12, 15), {"greatsword"});
    act(*c, "end");
    Command retreat;
    for (const auto &v : c->legal_commands())
        if (v.verb == "move" && v.destination == Cell{4, 1})
            retreat = v;
    check(c->submit(retreat) && c->snapshot().reaction_pending,
          "Leaving reach offers the weak Heavy wielder an Opportunity Attack");
    auto declined = rules->restore(c->save());
    auto restored = rules->restore(c->save());
    const auto reaction = command(*c, "opportunity");
    const auto before = unit(*c);
    check(c->submit(reaction) && restored->submit(reaction) && c->save() == restored->save(),
          "Heavy opportunity attack resumes identically");
    check(argument(attack(*c), "disadvantage") == " (disadvantage)" && !unit(*c).reaction &&
          unit(*c).action == before.action,
          "Heavy Disadvantage applies to Opportunity Attacks, which spend only the Reaction");
    act(*declined, "decline");
    check(!declined->snapshot().reaction_pending && unit(*declined).reaction,
          "Declining keeps the Reaction");
}
} // namespace

int main()
{
    try
    {
        thresholds();
        contextual_modifiers();
        campaign();
        opportunity_attack();
        std::cout << "Heavy weapon tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
