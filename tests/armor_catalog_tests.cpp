#include "opengold/campaign_save.h"
#include <filesystem>
#include "opengold/srd5.h"
#include "armor.h"
#include "status_effects.h"
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
    check(caught, "Invalid equipment/check must reject");
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
        content += "\ncreature target 1 1000 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n";
    return srd5::parse_content(content);
}

struct Expected
{
    std::string key, category;
    int ac{}, strength{};
    bool stealth{};
    unsigned weight{}, cost{}, don{}, doff{};
};

std::vector<Expected> expectations()
{
    std::istringstream in(read(fixtures / "armor-srd-5.2.1.tsv"));
    std::string row;
    std::vector<Expected> result;
    while (std::getline(in, row))
    {
        if (row.empty() || row[0] == '#')
            continue;
        std::istringstream fields(row);
        Expected e;
        fields >> e.key >> e.category >> e.ac >> e.strength >> e.stealth >> e.weight >> e.cost >>
               e.don >> e.doff;
        check(bool(fields), "Complete independent source row");
        result.push_back(e);
    }
    return result;
}

Character hero(std::string klass = "fighter", int strength = 15, int dexterity = 16,
               std::string race = "human", std::string background = "sage")
{
    CharacterDraft d;
    d.race = race;
    d.gender = "female";
    d.character_class = klass;
    d.background = background;
    d.alignment = "neutral_good";
    d.name = "Armor tester";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    const auto roll = [](int score)
    {
        if (score == 3)
            return AbilityRoll{{1, 1, 1, 1}, 3};
        return AbilityRoll{{score - 12, 6, 6, 1}, 3};
    };
    // Low Strength uses three ordinary dice; Dexterity 3 is the negative-modifier boundary.
d.rolls[0] = strength < 13 ? AbilityRoll{{4, 4, strength - 8, 1}, 3} :
    roll(strength);
    d.rolls[1] = dexterity < 13 && dexterity != 3 ? AbilityRoll{{4, 4, dexterity - 8, 1}, 3}
:
    roll(dexterity);
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

std::string argument(const Message &m, std::string_view key)
{
    for (const auto &a : m.arguments)
        if (a.name == key)
            return a.value;
    throw std::runtime_error("Missing argument");
}

Message attack(const CombatSession &c)
{
    for (const auto &m : c.snapshot().log_messages())
        if (m.source.starts_with("{actor} -> {target}: d20"))
            return m;
    throw std::runtime_error("Missing attack");
}

void definitions()
{
    const std::map<std::string, catalog::ArmorCategory> categories
    {
        {"light", catalog::ArmorCategory::light},
        {"medium", catalog::ArmorCategory::medium},
        {"heavy", catalog::ArmorCategory::heavy},
        {"shield", catalog::ArmorCategory::shield}};
    const auto expected = expectations();
    check(expected.size() == 13 && catalog::armors.size() == 13,
          "All 12 suits and Shield from the SRD table");
    std::set<std::string> keys;
    for (const auto &e : expected)
    {
        const auto *a = catalog::armor(e.key);
        check(a && keys.insert(e.key).second, "Every source entry has exactly one definition");
        check(a->category == categories.at(e.category) && a->base_ac == e.ac &&
              a->strength == e.strength && a->stealth_disadvantage == e.stealth,
              "Armor mechanics match independent table");
        check(a->weight_pounds == e.weight && a->cost_cp == e.cost && a->don_seconds == e.don &&
              a->doff_seconds == e.doff,
              "Weight, price and timing match independent table");
        check(!a->label.empty() && a->label.front() >= 'A' && a->label.front() <= 'Z' &&
              a->label.find('_') == std::string_view::npos,
              "Display name is human-readable");
    }
}

void all_classes()
{
    auto rules = module(true);
    // Independent starting traits from SRD pp.28,31,36,41,47,49,53,57,61,64,70,77.
    const std::map<std::string, std::string> training{{"barbarian", "light medium shield"},
        {"bard", "light"},
        {"cleric", "light medium shield"},
        {"druid", "light shield"},
        {"fighter", "light medium heavy shield"},
        {"monk", ""},
        {"paladin", "light medium heavy shield"},
        {"ranger", "light medium shield"},
        {"rogue", "light"},
        {"sorcerer", ""},
        {"warlock", "light"},
        {"wizard", ""}};
    check(srd5::character_rules()->choices(CreationField::character_class).size() ==
          training.size(),
          "All twelve classes covered");
    for (const auto &[klass, trained] : training)
    {
        const auto h = hero(klass);
        check(h.sheet().modifiers[1] == 3, "Fixed Dexterity +3 oracle");
        for (const auto &e : expectations())
        {
            const bool proficient = trained.find(e.category) != std::string::npos,
                  shield = e.category == "shield", penalty = !shield && !proficient;
            const std::array<std::string, 2> gear{e.key, "dagger"};
            const auto profile = rules->character_profile(h.sheet(), gear);
            const int ac = shield ? 13 + (klass == "barbarian" ? h.sheet().modifiers[2] : 0) +
                           (proficient ? 2 : 0)
                           : e.ac + (e.category == "light"    ? 3
                                     : e.category == "medium" ? 2
                                     : 0);
            check(
                profile.armor_class == ac && profile.strength_dexterity_disadvantage == penalty,
                "Every class/armor uses correct AC and training, including Druid's Light-only starting training");
            check(rules->equipment_info(e.key).slot ==
                  (shield ? EquipmentSlot::shield : EquipmentSlot::armor),
                  "All entries equip through the shared equipment contract");
            for (unsigned ability = 0; ability < 6; ++ability)
            {
                const auto plain = rules->ability_check(h.sheet(), gear, ability, ""),
                           stealth = rules->ability_check(h.sheet(), gear, ability, "stealth");
                check(
                    plain.disadvantage == (penalty && ability < 2) &&
                    stealth.disadvantage ==
                    ((penalty && ability < 2) || (e.stealth && ability == 1)),
                    "Only applicable ability checks receive armor penalties; alternate-ability Stealth is distinct");
            }
            auto c = rules->create({{4, 4, std::vector<std::uint8_t>(16)},
                {   {1, "campaign-character", "Hero", 0, {1, 1}, profile.data},
                    {2, "target", "Target", 1, {2, 1}}
                }},
            2);
            check(c->snapshot().actor == 1 && unit(*c).initiative == (penalty ? 10 : 14),
                  "Actual initiative uses seed-2 rolls 11/7 once for untrained armor");
            if (klass == "wizard")
                check(
                    has(*c, "fire_bolt") == !penalty && has(*c, "magic_missile") == !penalty,
                    "Untrained armor blocks casting; a dagger and shield do not (CLASS-11)");
            auto copy = rules->restore(c->save());
            const auto before = unit(*c);
            const auto ticket = command(*c, "melee");
            check(c->submit(ticket) && copy->submit(ticket) && c->save() == copy->save(),
                  "All class/armor combinations preserve exact checkpoint continuation");
            check(argument(attack(*c), "roll") == std::to_string(penalty ? 10 : 12) &&
                  argument(attack(*c), "disadvantage") == (penalty ? " (disadvantage)" : ""),
                  "Actual melee attack applies the armor penalty, independent seed oracle");
            check(
                unit(*c, 2).hit_points == 1000 - (penalty ? 7 : 4),
                "Finesse dagger damage follows actual dice without an invented armor damage penalty");
            check(!unit(*c).action && unit(*c).bonus_action == before.bonus_action &&
                  unit(*c).reaction == before.reaction &&
                  unit(*c).movement_feet == before.movement_feet &&
                  unit(*c).persistent == before.persistent,
                  "Attacks retain unrelated resources and movement");
            const auto saved = c->save();
            check(!c->submit(ticket) && c->save() == saved, "Stale armor attack is atomic");
        }
    }
}

void boundaries()
{
    auto rules = module();
    for (const auto &e : expectations())
        if (e.category != "shield")
        {
            for (int dex :
                    {
                        3, 10, 14, 18
                    })
            {
                const auto h = hero("fighter", 15, dex);
                const int modifier = dex == 3 ? -4 : dex == 10 ? 0 : dex == 14 ? 2 : 4;
                check(h.sheet().modifiers[1] == modifier, "Dexterity boundary fixture");
                const int added = e.category == "light"    ? modifier
                                  : e.category == "medium" ? std::min(2, modifier)
                                  : 0;
                check(
                    rules->character_profile(h.sheet(), std::array{e.key}).armor_class ==
                    e.ac + added,
                    "Light/full Dexterity, Medium capped positive/uncapped negative, Heavy ignores both");
            }
            for (const auto race :
                    {"human", "dwarf", "goliath"
                    })
                for (int strength :
                        {
                            12, 13, 14, 15
                        })
                {
                    const auto h = hero("fighter", strength, 16, race);
                    const int speed = std::string_view(race) == "goliath" ? 35 : 30;
                    check(rules->character_profile(h.sheet(), std::array{e.key}).movement_feet ==
                          speed - (strength < e.strength ? 10 : 0),
                          "Strength thresholds apply once to every species, including Dwarf");
                }
            const auto h = hero();
            const std::array<std::string, 2> guarded{e.key, "shield"};
            check(rules->character_profile(h.sheet(), guarded).armor_class ==
                  e.ac +
                  (e.category == "light"    ? 3
                   : e.category == "medium" ? 2
                   : 0) +
                  2,
                  "Trained shield adds to each armor AC");
            rejects(
                [&]
            {
                (void)rules->character_profile(h.sheet(),
                std::array<std::string, 2>{e.key, e.key});
            });
        }
    const auto h = hero();
    rejects(
        [&]
    {
        (void)rules->character_profile(h.sheet(),
        std::array<std::string, 2>{"shield", "shield"});
    });
    rejects(
        [&]
    {
        (void)rules->character_profile(
        h.sheet(), std::array<std::string, 3>{"plate", "greatsword", "shield"});
    });
    rejects(
        [&]
    {
        (void)rules->ability_check(h.sheet(), {}, 6, "");
    });
    rejects(
        [&]
    {
        (void)rules->ability_check(h.sheet(), {}, 1, "unknown");
    });
    const auto rogue = hero("rogue", 15, 16, "human", "criminal");
    const auto both = rules->ability_check(rogue.sheet(), std::array<std::string, 1> {"padded"}, 1,
                                           "stealth");
    check(both.proficiency == 2 && both.disadvantage,
          "Skill proficiency and armor Disadvantage remain independent");
    // Existing saving throw engine consumes the same profile penalty as live combat.
    const auto mage = hero("wizard");
    const auto p = rules->character_profile(mage.sheet(), std::array<std::string, 1> {"plate"});
    for (unsigned ability = 0; ability < 6; ++ability)
    {
        const auto modifiers = catalog::saving_modifiers(static_cast<catalog::Ability>(ability),
            p.strength_dexterity_disadvantage, false);
        check(modifiers.disadvantage == (ability < 2),
              "Untrained armor affects Strength/Dexterity saves only");
    }
}

void campaign()
{
    auto rules = module();
    for (const auto &e : expectations())
    {
        auto h = hero();
        const auto item = h.add_item({.definition_id = e.key, .name = e.key, .quantity = 1}),
                   other =
                       h.add_item({.definition_id =
                                              e.category == "shield" ? "shield" : "leather",
                                          .name = "Other",
                                          .quantity = 1});
        CampaignParty party(module());
        const auto id = party.add_pc(std::move(h));
        auto state = party.checkpoint();
        state.roster[0].vitals = {5, false, "SRD11 1 0 0 0 0 0 1 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0"};
        party.restore(state);
        party.equip(id, item);
        const auto before = encode_campaign(party, nullptr, "armor");
        rejects(
            [&]
        {
            party.equip(id, other);
        });
        check(encode_campaign(party, nullptr, "armor") == before,
              "Second armor/shield rejects without changing equipment, wounds, resources or time");
        CampaignParty restored(module());
        restored.restore(
            decode_campaign(before, *srd5::character_rules(), *rules, "armor", nullptr).party);
        check(encode_campaign(restored, nullptr, "armor") == before &&
              restored.participants()[0].character_profile ==
              party.participants()[0].character_profile,
              "All thirteen entries survive campaign reconstruction and next encounter adaptation");
        check(restored.ability_check(id, 1, "stealth").disadvantage == e.stealth,
              "Campaign check query incorporates equipped armor after reload");
        restored.unequip(id, item);
        check(!restored.ability_check(id, 1, "stealth").disadvantage &&
              restored.member(id).vitals == party.member(id).vitals,
              "Unequipping removes penalties without restoring resources");
    }
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
        std::cout << "Armor catalog tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
