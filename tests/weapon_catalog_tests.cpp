#include "combat_fixture.h"
#include "opengold/campaign_save.h"
#include "campaign_fixture.h"
#include "opengold/srd5.h"
#include "weapons.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
using namespace opengold;
using namespace opengold::rules;
namespace catalog = opengold::srd5::detail;

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
    check(caught, "Invalid equipment must reject");
}

const auto fixtures = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures";

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

struct Expected
{
    std::string key, damage, type, properties, ammunition, mastery;
    bool martial{}, ranged{};
    int range{}, long_range{}, versatile{};
    unsigned weight{}, cost{};

    bool has(std::string_view property) const
    {
        return ("," + properties + ",").find("," + std::string(property) + ",") !=
               std::string::npos;
    }
};

std::vector<Expected> expectations()
{
    std::istringstream in(read(fixtures / "weapons-srd-5.2.1.tsv"));
    std::string row;
    std::vector<Expected> result;
    while (std::getline(in, row))
    {
        if (row.empty() || row[0] == '#')
            continue;
        std::istringstream fields(row);
        Expected e;
        fields >> e.key >> e.martial >> e.ranged >> e.damage >> e.type >> e.properties >> e.range >>
            e.long_range >> e.versatile >> e.ammunition >> e.mastery >> e.weight >> e.cost;
        check(bool(fields), "Complete independent source row");
        result.push_back(e);
    }
    return result;
}

Character hero(std::string klass = "fighter", std::string background = "sage")
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = klass;
    d.background = background;
    d.alignment = "neutral_good";
    d.name = "Catalog tester";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.rolls[0] = {{6, 4, 4, 1}, 3};
    d.rolls[1] = {{6, 5, 5, 1}, 3};
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

bool has(const CombatSession &c, std::string_view verb)
{
    for (const auto &a : c.legal_commands())
        if (a.verb == verb)
            return true;
    return false;
}

void act(CombatSession &c, std::string_view verb)
{
    check(c.submit(command(c, verb)), "Command accepted");
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
    throw std::runtime_error("Missing argument");
}

Message attack(const CombatSession &c)
{
    for (const auto &m : c.snapshot().log_messages)
        if (m.source.starts_with("{actor} -> {target}: d20"))
            return m;
    throw std::runtime_error("Missing attack");
}

auto battle(const RulesModule &rules, const Character &h, const std::string &weapon,
            unsigned seed = 13, Cell target = {3, 1})
{
    auto profile = rules.character_profile(h.sheet(), std::array{weapon});
    auto c = rules.create({{64, 4, std::vector<std::uint8_t>(256)},
                           {{1, "campaign-character", "Hero", 0, {1, 1}, profile.data},
                            {2, "target", "Target", 1, target}}},
                          seed);
    check(c->snapshot().actor == 1, "Golden seed begins with hero");
    return c;
}

void definitions()
{
    const std::map<std::string, catalog::Ammunition> ammo{
        {"none", catalog::Ammunition::none},
        {"arrow", catalog::Ammunition::arrow},
        {"bolt", catalog::Ammunition::bolt},
        {"sling_bullet", catalog::Ammunition::sling_bullet},
        {"firearm_bullet", catalog::Ammunition::firearm_bullet},
        {"needle", catalog::Ammunition::needle}};
    const std::map<std::string, catalog::Mastery> masteries{
        {"cleave", catalog::Mastery::cleave}, {"graze", catalog::Mastery::graze},
        {"nick", catalog::Mastery::nick},     {"push", catalog::Mastery::push},
        {"sap", catalog::Mastery::sap},       {"slow", catalog::Mastery::slow},
        {"topple", catalog::Mastery::topple}, {"vex", catalog::Mastery::vex}};
    const std::map<std::string, catalog::DamageType> types{
        {"bludgeoning", catalog::DamageType::bludgeoning},
        {"piercing", catalog::DamageType::piercing},
        {"slashing", catalog::DamageType::slashing}};
    const auto expected = expectations();
    std::set<std::string> keys;
    check(expected.size() == 38 && catalog::weapons.size() == 39,
          "All 38 SRD weapons plus the existing plain focus");
    for (const auto &e : expected)
    {
        const auto *w = catalog::weapon(e.key);
        check(w && keys.insert(e.key).second, "Each source weapon has exactly one definition");
        check(!w->label.empty() && w->label.front() >= 'A' && w->label.front() <= 'Z' &&
                  w->label.find('_') == std::string_view::npos,
              "Display names are human-readable labels, independent of stable keys");
        const auto damage = w->fixed_damage
                                ? std::to_string(w->fixed_damage)
                                : std::to_string(w->dice) + "d" + std::to_string(w->sides);
        check(damage == e.damage && w->type == types.at(e.type) && w->martial == e.martial &&
                  w->ranged == e.ranged,
              "Category, damage and damage type match SRD table");
        check(w->range == e.range && w->long_range == e.long_range &&
                  w->hands == (e.has("two-handed") ? 2u : 1u) &&
                  w->reach == (e.has("reach") ? 10 : 5) && w->versatile_sides == e.versatile,
              "Ranges, reach, hands and Versatile match source");
        check(w->finesse == e.has("finesse") && w->light == e.has("light") &&
                  w->heavy == e.has("heavy") && w->thrown == e.has("thrown") &&
                  w->loading == e.has("loading") &&
                  w->mounted_one_handed == e.has("mounted-one-handed"),
              "Every weapon property matches the source, including Lance's conditional hands");
        check(w->ammunition == ammo.at(e.ammunition) && w->mastery == masteries.at(e.mastery) &&
                  w->weight_quarters == e.weight && w->cost_cp == e.cost,
              "Ammunition/mastery identity, weight and cost are source-verified metadata");
    }
}

void all_classes()
{
    auto rules = module(true);
    const std::map<std::string, int> normal{{"1d4", 4},  {"1d6", 2},  {"1d8", 4},
                                            {"1d10", 8}, {"1d12", 8}, {"2d6", 6}};
    const std::map<std::string, int> critical{{"1d4", 5},   {"1d6", 7},   {"1d8", 9},
                                              {"1d10", 13}, {"1d12", 13}, {"2d6", 14}};
    unsigned classes = 0;
    for (const auto &klass : srd5::character_rules()->choices(CreationField::character_class))
    {
        ++classes;
        const auto h = hero(klass.id);
        check(h.sheet().modifiers[0] == 2 && h.sheet().modifiers[1] == 3,
              "Asymmetric ability oracle");
        for (const auto &e : expectations())
            for (unsigned seed : {0u, 13u, 40u})
            {
                const bool trained =
                    !e.martial || klass.id == "barbarian" || klass.id == "fighter" ||
                    klass.id == "paladin" || klass.id == "ranger" ||
                    (klass.id == "rogue" && (e.has("finesse") || e.has("light"))) ||
                    (klass.id == "monk" && e.has("light"));
                const int modifier = e.ranged || e.has("finesse") ? 3 : 2;
                auto c = battle(*rules, h, e.key, seed, e.ranged ? Cell{3, 1} : Cell{2, 1});
                auto copy = rules->restore(c->save());
                const auto before = unit(*c);
                const auto ticket = command(*c, e.ranged ? "ranged" : "melee");
                check(c->submit(ticket) && copy->submit(ticket) && c->save() == copy->save(),
                      "All class/weapon attacks survive checkpoint continuation");
                const auto result = attack(*c);
                check(argument(result, "roll") == std::to_string(seed == 0    ? 20
                                                                 : seed == 13 ? 17
                                                                              : 1) &&
                          argument(result, "bonus") == std::to_string(modifier + (trained ? 2 : 0)),
                      "Independent seed and all-class proficiency/ability expectations");
                const int damage =
                    seed == 40 ? 0
                    : e.damage == "1"
                        ? 1
                        : (seed == 0 ? critical.at(e.damage) : normal.at(e.damage)) + modifier;
                check(
                    unit(*c, 2).hit_points == 1000 - damage,
                    "Normal, critical and miss damage match independent golden rolls, including fixed Blowgun damage");
                check(!unit(*c).action && unit(*c).bonus_action == before.bonus_action &&
                          unit(*c).reaction == before.reaction &&
                          unit(*c).movement_feet == before.movement_feet &&
                          unit(*c).persistent == before.persistent,
                      "Each attack consumes only its action");
                const auto saved = c->save();
                check(!c->submit(ticket) && c->save() == saved,
                      "Stale attack is atomic for every weapon and class");
            }
    }
    check(classes == 12, "All twelve SRD classes covered");
}

void boundaries()
{
    auto rules = module(true);
    const auto h = hero();
    for (const auto &e : expectations())
    {
        const auto info = rules->equipment_info(e.key);
        check(info.slot == EquipmentSlot::weapon, "Every catalog key is equippable");
        if (e.has("two-handed"))
            rejects(
                [&]
                {
                    (void)rules->character_profile(h.sheet(),
                                                   std::array<std::string, 2>{e.key, "shield"});
                });
        else
            check(
                rules->character_profile(h.sheet(), std::array<std::string, 2>{e.key, "shield"})
                        .armor_class == 15,
                "One-handed equipment preserves the shield; ammo loading is a separate property action");
        if (e.range)
        {
            for (const int feet : {e.range, e.range + 5, e.long_range, e.long_range + 5, 310})
            {
                if (feet > 310)
                    continue; // Larger long ranges are source-checked; the battlefield is capped at
                              // 64 cells.
                auto c = battle(*rules, h, e.key, 13, {1 + feet / 5, 1});
                check(has(*c, "ranged") == (feet <= e.long_range),
                      "Exact normal/long-range command boundaries");
                if (feet <= e.long_range)
                {
                    act(*c, "ranged");
                    check(argument(attack(*c), "disadvantage") ==
                              (feet > e.range ? " (disadvantage)" : ""),
                          "Only beyond-normal range applies range Disadvantage");
                }
            }
        }
        if (!e.ranged)
        {
            auto c = battle(*rules, h, e.key, 13, {3, 1});
            check(has(*c, "melee") == e.has("reach"),
                  "Reach weapons alone hit a target ten feet away");
        }
    }
    // No damage dice are rolled or doubled for Blowgun, even with Savage Attacker.
    for (unsigned seed : {0u, 13u})
    {
        auto c = battle(*rules, hero("fighter", "soldier"), "blowgun", seed);
        const auto before = rng(*c);
        act(*c, "ranged");
        check(unit(*c, 2).hit_points == 999 && rng(*c) == before + 0x9e3779b97f4a7c15ULL &&
                  argument(attack(*c), "savage").empty(),
              "Blowgun fixed damage never adds ability/Savage Attacker dice or critical damage");
    }
    for (int dexterity : {3, 20})
    {
        auto draft = h.creation_data();
        draft.background = dexterity == 20 ? "criminal" : "sage";
        draft.rolls[1] =
            dexterity == 20 ? AbilityRoll{{6, 6, 6, 1}, 3} : AbilityRoll{{1, 1, 1, 1}, 3};
        Character extreme(*srd5::character_rules(), draft, {});
        check(extreme.sheet().scores[1] == dexterity, "Fixed damage extreme ability fixture");
        auto c = battle(*rules, extreme, "blowgun");
        act(*c, "ranged");
        check(unit(*c, 2).hit_points == 999,
              "Negative or maximum Dexterity never changes Blowgun's fixed damage");
    }
    const auto profile = rules->character_profile(h.sheet(), std::array<std::string, 1>{"blowgun"});
    check(profile.item_modifiers.find("damage is fixed at 1 without an ability modifier") !=
              std::string::npos,
          "Existing equipment explanation describes fixed damage accurately");
}

void campaign()
{
    auto rules = module();
    const auto h = hero();
    for (const auto &e : expectations())
    {
        auto c = h;
        const auto item = c.inventory().add(e.key, e.key, 1);
        const auto shield = c.inventory().add("shield", "Shield", 1);
        CampaignParty party(module());
        const auto id = party.add_pc(std::move(c));
        auto state = party.checkpoint();
        state.roster[0].vitals = {5, false, "SRD1 1 0 0 0 0"};
        party.restore(state);
        party.equip(id, item);
        const auto before = encode_campaign(party, nullptr, "catalog");
        if (e.has("two-handed"))
        {
            rejects(
                [&]
                {
                    party.equip(id, shield);
                });
            check(encode_campaign(party, nullptr, "catalog") == before,
                  "Rejected shield changes no equipment, HP or resources");
        }
        CampaignParty restored(module());
        restored.restore(
            decode_campaign(before, *srd5::character_rules(), *rules, "catalog", nullptr).party);
        check(encode_campaign(restored, nullptr, "catalog") == before,
              "All 38 catalog weapons reconstruct canonically through campaign saves");
        check(restored.member(id).vitals == party.member(id).vitals &&
                  restored.participants()[0].character_profile ==
                      party.participants()[0].character_profile,
              "Next encounter inherits identical equipment and expenditure");
    }
    por::Equipment original;
    original.stored.type = 45;
    check(
        equipment_conversion(original) == "longbow",
        "Original type 45 remains Fine Composite Long Bow; it must not become the new SRD Heavy Crossbow");
    original.stored.value = 50;
    original.stored.stack_size = 1;
    CampaignParty bought(module());
    const auto buyer = bought.add_pc(hero());
    bought.set_wealth(buyer, {0, 0, 0, 100, 0, 0, 0});
    bought.purchase(buyer, original);
    const auto purchased = bought.member(buyer).character.inventory().items().front();
    check(
        purchased.definition_id == "longbow" && bought.member(buyer).wealth[3] == 50 &&
            bought.member(buyer).item_sources.at(purchased.id).stored.type == 45,
        "Ordinary purchase retains original price, quantity, provenance and existing bow conversion");
    bought.equip(buyer, purchased.id);
    check(bought.profile(buyer).equipment.weapon_hands == 2,
          "Purchased Fine Composite Long Bow equips through normal campaign inventory");
    original.stored.magic_bonus = 1;
    check(equipment_conversion(original) == "por:unsupported:45",
          "Unimplemented enchanted variants remain rejected");
}

void legacy()
{
    auto rules = module();
    const auto old = read(fixtures / "campaign-v10-catalog.ogs");
    CampaignParty party(module());
    party.restore(
        decode_campaign(old, *srd5::character_rules(), *rules, "catalog-fixture", nullptr).party);
    check(party.member(1).character.inventory().find(1)->get().definition_id == "longbow",
          "Legacy original type-45 bow must retain its established conversion");
    check(party.member(2).character.inventory().find(1)->get().definition_id == "longbow" &&
              party.member(3).character.inventory().find(1)->get().definition_id == "longbow",
          "Real Longbow and item without source provenance are preserved");
    auto expected = old.substr(old.find('\n', old.find('\n') + 1) + 1);
    expected.replace(expected.find("0.6.16"), 6, rules->identity().version);
    const auto bytes = encode_campaign(party, nullptr, "catalog-fixture");
    check(
        bytes.substr(bytes.find('\n', bytes.find('\n') + 1) + 1) ==
            test::with_initial_wizard_spell_grants(expected),
        "Migration adds only explicit spell/Sage grants and module identity, retaining original and authored weapons, grants, pools, wounds and clock");
    CampaignParty again(module());
    again.restore(
        decode_campaign(bytes, *srd5::character_rules(), *rules, "catalog-fixture", nullptr).party);
    check(encode_campaign(again, nullptr, "catalog-fixture") == bytes,
          "Correction occurs only once");
    const auto previous = read(fixtures / "combat-v12-catalog.save");
    auto c = rules->restore(previous);
    auto same = previous;
    same.replace(same.find("0.6.16"), 6, rules->identity().version);
    check(
        c->save() == test::with_savage_choice(same),
        "Combat recipes have no original-item provenance and retain their saved weapon/RNG/resources");
    act(*c, "ranged");
    check(c->save() == rules->restore(read(fixtures / "combat-v12-catalog-continued.save"))->save(),
          "Frozen prior-writer combat continuation remains exact");
}
} // namespace

int main()
{
    try
    {
        definitions();
        all_classes();
        boundaries();
        campaign();
        legacy();
        std::cout << "Weapon catalog tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
