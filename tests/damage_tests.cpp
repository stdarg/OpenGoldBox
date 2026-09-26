#include "combat_fixture.h"
#include "campaign_fixture.h"
#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "damage.h"
#include "damage_roll.h"
#include "sneak_attack.h"
#include "life_cycle.h"
#include "weapons.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
using namespace opengold;
using namespace opengold::rules;
namespace damage = opengold::srd5::detail;
using damage::AffinityKind;
using damage::DamageAffinity;
using damage::DamagePart;
using damage::DamageType;
namespace
{
void check(bool ok, const char *message)
{
    if (!ok)
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
    check(caught, "Invalid damage data must reject");
}
std::string read(const std::filesystem::path &p)
{
    std::ifstream in(p);
    check(bool(in), "Fixture exists");
    return {std::istreambuf_iterator<char>(in), {}};
}
std::string content()
{
    return read(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/rules/srd-5.2.1/combat.rules");
}
auto module()
{
    return srd5::parse_content(content());
}
Character hero(std::string race = "dwarf", std::string klass = "fighter", unsigned level = 1)
{
    CharacterDraft d;
    d.race = race;
    d.gender = "female";
    d.character_class = klass;
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Damage tester";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    Character result(*srd5::character_rules(), d, {});
    VitalState scratch;
    for (unsigned i = 1; i < level; ++i)
        check(result.advance(*module(), scratch), "Fixture level is supported");
    return result;
}
auto resolve(std::initializer_list<DamagePart> parts,
             std::initializer_list<DamageAffinity> affinities)
{
    return damage::resolve_damage(parts, affinities);
}
void sneak_attack_foundation()
{
    // Independent SRD catalog list, including fixed-damage Blowgun and thrown Dart.
    const std::vector<std::string_view> eligible{
        "dagger",        "dart",           "light_crossbow", "shortbow", "sling",
        "rapier",        "scimitar",       "shortsword",     "whip",     "blowgun",
        "hand_crossbow", "heavy_crossbow", "longbow",        "musket",   "pistol"};
    for (const auto &weapon : damage::weapons)
    {
        const bool suitable =
            std::find(eligible.begin(), eligible.end(), weapon.key) != eligible.end();
        for (int mode : {-1, 0, 1})
            for (bool ally : {false, true})
                for (bool is_weapon : {false, true})
                {
                    const bool circumstances = mode == 1 || (mode == 0 && ally);
                    check(damage::sneak_attack_eligible(
                              {is_weapon, weapon.finesse, weapon.ranged, mode, ally}) ==
                              (suitable && circumstances && is_weapon),
                          "Independent catalog and net roll circumstances determine eligibility");
                }
    }
    check(!damage::sneak_attack_eligible({true, false, false, 1, true}),
          "Thrown non-Finesse Melee weapons remain ineligible despite range");
    check(damage::sneak_attack_eligible({true, true, false, 0, true}),
          "Opposing Advantage/Disadvantage cancel; ally permits normal Finesse hit");
    rejects(
        []
        {
            (void)damage::sneak_attack_eligible({true, true, false, 2, true});
        });
    const std::array<int, 20> counts{1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10};
    for (unsigned level = 1; level <= counts.size(); ++level)
    {
        const auto dice = damage::sneak_attack_dice(level);
        check(dice.count == counts[level - 1] && dice.sides == 6 && dice.bonus == 0,
              "Source table uses Rogue level and adds no ability modifier");
    }
    rejects(
        []
        {
            (void)damage::sneak_attack_dice(0);
        });
    rejects(
        []
        {
            (void)damage::sneak_attack_dice(21);
        });
}
void arithmetic()
{
    const DamageAffinity resist{AffinityKind::resistance, DamageType::fire, "first"},
        vulnerable{AffinityKind::vulnerability, DamageType::fire, "second"};
    check(
        resolve({{DamageType::fire, 28 - 5}}, {resist, vulnerable}).total == 22,
        "SRD example: adjustment to 23, resistance rounds down to 11, vulnerability doubles to 22");
    check(resolve({{DamageType::fire, 1}}, {resist, vulnerable}).total == 0,
          "Opposing affinities do not cancel rounding");
    check(resolve({{DamageType::fire, 5}, {DamageType::cold, 5}}, {resist}).total == 7,
          "Resistance affects only its type within a mixed instance");
    check(resolve({{DamageType::fire, 5}, {DamageType::fire, 5}}, {resist}).total == 5,
          "Same-type components of one instance combine before rounding");
    check(resolve({{DamageType::fire, 5}}, {resist}).total +
                  resolve({{DamageType::fire, 5}}, {resist}).total ==
              4,
          "Separate instances each round independently");
    check(resolve({{DamageType::fire, 9}},
                  {resist, {AffinityKind::resistance, std::nullopt, "all"}, resist})
                  .total == 4,
          "Overlapping all/type grants never stack resistance");
    check(resolve({{DamageType::fire, 9}},
                  {vulnerable, {AffinityKind::vulnerability, std::nullopt, "all"}})
                  .total == 18,
          "Vulnerability never stacks");
    check(resolve({{DamageType::fire, 9}},
                  {resist, vulnerable, {AffinityKind::immunity, DamageType::fire, "immune"}})
                  .total == 0,
          "Immunity prevents damage despite vulnerability");
    check(resolve({{DamageType::fire, 0}}, {resist, vulnerable}).total == 0 &&
              resolve({}, {}).total == 0,
          "Empty and zero damage never manufacture damage");
    for (unsigned i = 0; i < 13; ++i)
    {
        const auto type = static_cast<DamageType>(i);
        check(!damage::damage_name(type).empty(), "Every SRD damage type has a presentation label");
        check(resolve({{type, 7}}, {{AffinityKind::resistance, type, "source"}}).total == 3,
              "Every type uses the same halving rule");
    }
    rejects(
        [&]
        {
            (void)resolve({{DamageType::count, 1}}, {});
        });
    rejects(
        [&]
        {
            (void)resolve({{DamageType::fire, -1}}, {});
        });
    rejects(
        [&]
        {
            (void)resolve({{DamageType::fire, std::numeric_limits<int>::max()}}, {vulnerable});
        });
    rejects(
        [&]
        {
            (void)resolve({{DamageType::fire, 1}},
                          {{static_cast<AffinityKind>(99), std::nullopt, "bad"}});
        });
    rejects(
        [&]
        {
            (void)damage::damage_type("unknown");
        });
    damage::LifeState state{0, 0, 0, true, false, {0, 7200000}};
    const auto before = state;
    damage::damage_life(state, resolve({{DamageType::fire, 1}}, {resist}).total, 20, true);
    check(state == before,
          "Damage reduced to zero neither ends Stable nor adds a critical death failure");
    state = {5, 0, 0, false, false, {}};
    damage::damage_life(state, resolve({{DamageType::fire, 25}}, {resist}).total, 20);
    check(!state.dead && state.hp == 0 && state.failures == 0,
          "Massive damage uses the post-resistance amount");
}
CombatantView unit(const CombatSession &combat, EntityId id)
{
    for (const auto &a : combat.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing actor");
}
Command command(const CombatSession &combat, std::string_view verb, EntityId target = 0)
{
    for (const auto &c : combat.legal_commands())
        if (c.verb == verb && (!target || c.target == target))
            return c;
    throw std::runtime_error("Missing command: " + std::string(verb));
}
void turn(CombatSession &combat, EntityId id)
{
    for (unsigned n = 0; n < 8 && combat.snapshot().actor != id; ++n)
        check(combat.submit(command(combat, "end")), "Wait for actor");
    check(combat.snapshot().actor == id, "Actor gets a turn");
}
std::uint64_t rng(const CombatSession &combat)
{
    std::istringstream in(combat.save());
    std::string line;
    for (unsigned n = 0; n < 3; ++n)
        std::getline(in, line);
    std::uint64_t value{};
    in >> value;
    return value;
}
std::string attacker()
{
    return "creature toxin 10 500 0 30 30 1 4 3 30 1 4 3 80 320 0 4 30 3 5\nspellcasting toxin 4 21\ndamage_types toxin poison poison\n";
}
void species_combat()
{
    auto rules = srd5::parse_content(content() + attacker());
    for (const auto &c : srd5::character_rules()->choices(CreationField::character_class))
    {
        const auto dwarf = hero("dwarf", c.id), human = hero("human", c.id);
        const FeatureGrant resilience{"trait:dwarven_resilience", "species:dwarf", 1, {}};
        check(std::find(dwarf.sheet().grants.begin(), dwarf.sheet().grants.end(), resilience) !=
                  dwarf.sheet().grants.end(),
              "All twelve Dwarf classes receive sourced Poison resistance");
        check(dwarf.sheet().racial_modifiers.find("Resistance to Poison") != std::string::npos,
              "Existing racial explanation describes the implemented resistance");
        auto invalid = dwarf.sheet();
        std::erase_if(invalid.grants,
                      [&](const auto &g)
                      {
                          return g == resilience;
                      });
        rejects(
            [&]
            {
                (void)rules->character_profile(invalid, {});
            });
        const auto encounter = [&](const Character &h)
        {
            auto profile = rules->character_profile(h.sheet(), {});
            return Encounter{{8, 8, std::vector<std::uint8_t>(64)},
                             {{1, "campaign-character", "Target", 0, {2, 2}, profile.data},
                              {2, "toxin", "Poison attacker", 1, {3, 2}}}};
        };
        auto d = rules->create(encounter(dwarf), 42), h = rules->create(encounter(human), 42);
        turn(*d, 2);
        turn(*h, 2);
        const auto d_before = unit(*d, 1).hit_points, h_before = unit(*h, 1).hit_points;
        auto copy = rules->restore(d->save());
        const auto attack = command(*d, "melee", 1);
        check(d->submit(attack) && copy->submit(attack) && h->submit(command(*h, "melee", 1)),
              "Actual poison attacks accept ordinary combat commands");
        check(copy->save() == d->save() && rng(*h) == rng(*d),
              "Resistance adds no RNG draw and continues after a checkpoint");
        const int raw = h_before - unit(*h, 1).hit_points,
                  reduced = d_before - unit(*d, 1).hit_points;
        check(raw > 0 && raw < h_before && reduced == raw / 2,
              "A created Dwarf halves actual poison damage independently of class");
        check(!unit(*d, 1).dead && unit(*d, 2).action == false,
              "Resistance preserves normal action expenditure and vitality");
        const auto snapshot = d->snapshot();
        check(std::any_of(snapshot.log_messages.begin(), snapshot.log_messages.end(),
                          [](const auto &m)
                          {
                              return m.source == "{name}: {type} damage {before} -> {after}.";
                          }),
              "Existing combat log explains adjusted damage");
        const auto before = d->save();
        check(!d->submit(attack) && d->save() == before,
              "Stale commands cannot apply resistance/damage twice");
    }
}
void weapons_and_spells()
{
    // The target's one immunity identifies the attack's actual damage type.
    const auto target = "creature target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n";
    const std::map<std::string_view, DamageType> expected_types{
        {"club", DamageType::bludgeoning},
        {"dagger", DamageType::piercing},
        {"handaxe", DamageType::slashing},
        {"javelin", DamageType::piercing},
        {"light_hammer", DamageType::bludgeoning},
        {"mace", DamageType::bludgeoning},
        {"quarterstaff", DamageType::bludgeoning},
        {"spear", DamageType::piercing},
        {"dart", DamageType::piercing},
        {"light_crossbow", DamageType::piercing},
        {"shortbow", DamageType::piercing},
        {"sling", DamageType::bludgeoning},
        {"battleaxe", DamageType::slashing},
        {"flail", DamageType::bludgeoning},
        {"glaive", DamageType::slashing},
        {"greatsword", DamageType::slashing},
        {"halberd", DamageType::slashing},
        {"longsword", DamageType::slashing},
        {"morningstar", DamageType::piercing},
        {"pike", DamageType::piercing},
        {"scimitar", DamageType::slashing},
        {"shortsword", DamageType::piercing},
        {"trident", DamageType::piercing},
        {"warhammer", DamageType::bludgeoning},
        {"war_pick", DamageType::piercing},
        {"longbow", DamageType::piercing},
        {"greatclub", DamageType::bludgeoning},
        {"sickle", DamageType::slashing},
        {"greataxe", DamageType::slashing},
        {"lance", DamageType::piercing},
        {"maul", DamageType::bludgeoning},
        {"rapier", DamageType::piercing},
        {"whip", DamageType::slashing},
        {"blowgun", DamageType::piercing},
        {"hand_crossbow", DamageType::piercing},
        {"heavy_crossbow", DamageType::piercing},
        {"musket", DamageType::piercing},
        {"pistol", DamageType::piercing}};
    for (const auto &weapon : damage::weapons)
    {
        if (weapon.key == "wand")
            continue;
        check(weapon.type == expected_types.at(weapon.key),
              "Weapon metadata agrees with the independent SRD damage-type table");
        const auto kind = damage::damage_name(expected_types.at(weapon.key));
        std::string lower(kind);
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](char c)
                       {
                           return char(c >= 'A' && c <= 'Z' ? c + 32 : c);
                       });
        auto rules = srd5::parse_content(content() + target + "affinity target shell immunity " +
                                         lower + '\n');
        const auto character = hero("human", "fighter", 4);
        const std::array gear{std::string(weapon.key)};
        const auto profile = rules->character_profile(character.sheet(), gear);
        auto combat =
            rules->create({{8, 8, std::vector<std::uint8_t>(64)},
                           {{1, "campaign-character", "Armed fighter", 0, {2, 2}, profile.data},
                            {2, "target", "Target", 1, {3, 2}}}},
                          42);
        turn(*combat, 1);
        const auto verb = weapon.ranged ? "ranged" : "melee";
        check(combat->submit(command(*combat, verb, 2)) && unit(*combat, 2).hit_points == 1000,
              "Weapon damage uses its SRD type before HP loss");
        test::choose_savage_damage(*combat);
        check(unit(*combat, 2).hit_points == 1000, "Selected damage remains immune");
        const auto logs = combat->snapshot().log_messages;
        check(std::any_of(logs.begin(), logs.end(),
                          [](const auto &m)
                          {
                              return m.source == "{name}: {type} damage {before} -> {after}.";
                          }),
              "The weapon actually hit and immunity was applied");
    }
    for (const auto verb : {"fire_bolt", "scorching_ray", "magic_missile"})
    {
        const std::string type = std::string_view(verb) == "magic_missile" ? "force" : "fire";
        auto rules = srd5::parse_content(content() + attacker() + target +
                                         "affinity target shell immunity " + type + '\n');
        auto combat =
            rules->create({{8, 8, std::vector<std::uint8_t>(64)},
                           {{1, "toxin", "Caster", 0, {0, 0}}, {2, "target", "Target", 1, {7, 7}}}},
                          42);
        turn(*combat, 1);
        check(combat->submit(command(*combat, verb, 2)) && unit(*combat, 2).hit_points == 1000,
              "Fire Bolt, Scorching Ray and Magic Missile respect typed immunity");
        test::choose_savage_damage(*combat);
        check(unit(*combat, 2).hit_points == 1000, "Selected damage remains immune");
        const auto logs = combat->snapshot().log_messages;
        check(std::any_of(logs.begin(), logs.end(),
                          [](const auto &m)
                          {
                              return m.source == "{name}: {type} damage {before} -> {after}.";
                          }),
              "Spell damage actually reached immunity resolution");
    }
    // Fixed seed: three 1d4+1 rolls are separate force-damage instances.
    auto base = srd5::parse_content(content() + attacker() + target);
    auto protected_rules = srd5::parse_content(content() + attacker() + target +
                                               "affinity target ward resistance force\n");
    Encounter encounter{{8, 8, std::vector<std::uint8_t>(64)},
                        {{1, "toxin", "Caster", 0, {0, 0}}, {2, "target", "Target", 1, {7, 7}}}};
    auto raw = base->create(encounter, 42), resisted = protected_rules->create(encounter, 42);
    turn(*raw, 1);
    turn(*resisted, 1);
    check(raw->submit(command(*raw, "magic_missile", 2)) &&
              resisted->submit(command(*resisted, "magic_missile", 2)),
          "Missile conformance case casts");
    check(rng(*raw) == rng(*resisted),
          "Dart resistance preserves the established per-dart RNG sequence");
    int expected = 0, instances = 0;
    for (const auto &m : resisted->snapshot().log_messages)
        if (m.source == "{name}: {type} damage {before} -> {after}.")
        {
            for (const auto &a : m.arguments)
                if (a.name == "before")
                    expected += std::stoi(a.value) / 2;
            ++instances;
        }
    check(instances == 3 && 1000 - unit(*resisted, 2).hit_points == expected,
          "Each Magic Missile dart rounds separately before summing HP loss");
}
std::string fixture(const char *name)
{
    return read(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures" / name);
}
std::string upgrade(std::string bytes, const Identity &identity)
{
    std::istringstream in(bytes);
    std::vector<std::string> rows;
    for (std::string row; std::getline(in, row);)
        rows.push_back(row);
    std::ostringstream header;
    header << "OGCOMBAT 13 " << std::quoted(identity.module) << ' ' << std::quoted(identity.version)
           << ' ' << std::quoted(identity.content);
    rows[0] = header.str();
    for (unsigned i = 4; i < 8; ++i)
        rows[i] += " 0 \"\" 0 0";
    std::string result;
    for (const auto &row : rows)
        result += row + '\n';
    return result + "0\n0\n";
}
void migration()
{
    auto rules = module();
    const auto old = fixture("campaign-v10-damage.ogs");
    const auto disk =
        decode_campaign(old, *srd5::character_rules(), *rules, "damage-fixture", nullptr);
    CampaignParty party(module());
    party.restore(disk.party);
    for (const auto &member : party.state().roster)
    {
        const bool dwarf = member.id % 2 == 1;
        check(std::any_of(member.character.sheet().grants.begin(),
                          member.character.sheet().grants.end(),
                          [](const auto &g)
                          {
                              return g.id == "trait:dwarven_resilience";
                          }) == dwarf,
              "Old campaign gains exactly the fixed resistance justified by its species");
    }
    check(party.member(1).vitals.resources == "SRD5 1 0 0 0 0 1 1 0 4321000 FX1 1 0" &&
              party.member(1).vitals.hit_points == 0 && party.member(3).vitals.dead &&
              party.state().random_state == 42 && party.state().time_minutes == 1234 &&
              party.state().subminute_milliseconds == 5678,
          "Migration preserves Stable timing, spent dice/Wind, death, RNG and clock");
    const auto bytes = encode_campaign(party, nullptr, "damage-fixture");
    CampaignParty copy(module());
    copy.restore(
        decode_campaign(bytes, *srd5::character_rules(), *rules, "damage-fixture", nullptr).party);
    check(encode_campaign(copy, nullptr, "damage-fixture") == bytes,
          "Sourced resistance migration is canonical and not reapplied on reload");
    auto combat = rules->restore(fixture("combat-v10-damage.save"));
    check(combat->save() == upgrade(fixture("combat-v10-damage.save"), rules->identity()),
          "Old combat preserves all actor fields, profiles, RNG, timers and pending reactions");
    const auto decline = command(*combat, "decline");
    check(combat->submit(decline) &&
              combat->save() ==
                  upgrade(fixture("combat-v10-damage-continued.save"), rules->identity()),
          "Pre-change writer's pending movement continues byte-for-byte apart from identity");
    CampaignParty pending(module());
    const auto pending_id = pending.add_pc(hero("dwarf", "fighter", 2));
    auto waiting = pending.checkpoint();
    waiting.roster[0].vitals = party.member(1).vitals;
    pending.restore(waiting);
    pending.complete_training(pending_id, *srd5::character_rules(),
                              {{"origin:languages", {"elvish", "orc"}},
                               {"class:fighter:fighting_style", {"archery"}},
                               {"class:fighter", {"athletics", "history"}},
                               {"background:soldier:gaming_set", {"dice"}}});
    check(pending.member(pending_id).vitals == waiting.roster[0].vitals,
          "Completing missing training preserves resistance and the full vital continuation");
    const auto &dwarf = party.member(1);
    const auto before = dwarf.vitals;
    rejects(
        [&]
        {
            party.complete_training(1, *srd5::character_rules(),
                                    {{"origin:languages", {"elvish", "orc"}}});
        });
    check(party.member(1).vitals == before,
          "Repeated completed training cannot change a resistant character");
    party.award_experience(900, "damage-test");
    party.advance(1, party.default_advancement(1));
    check(party.member(1).vitals.hit_points == 0 &&
              party.member(1).vitals.resources == "SRD5 1 0 0 0 0 1 2 0 4321000 FX1 1 0",
          "Advancement preserves resistance and mortality while adding only the earned Hit Die");
}
void malformed()
{
    for (const auto row :
         {"damage_types missing fire force\n", "damage_types toxin acid unknown\n",
          "damage_types toxin acid fire extra\n", "affinity toxin s unknown fire\n",
          "affinity toxin s resistance unknown\n", "affinity toxin s immunity all extra\n",
          "affinity toxin s immunity\n",
          "affinity toxin s immunity all\naffinity toxin s resistance fire\n"})
        rejects(
            [&]
            {
                (void)srd5::parse_content(
                    content() + attacker().substr(0, attacker().find("damage_types")) + row);
            });
    auto sheet = hero().sheet();
    auto grant = std::find_if(sheet.grants.begin(), sheet.grants.end(),
                              [](const auto &g)
                              {
                                  return g.id == "trait:dwarven_resilience";
                              });
    grant->source_id = "species:human";
    rejects(
        [&]
        {
            (void)module()->character_profile(sheet, {});
        });
}
void damage_rolls()
{
    using damage::DamageDieRule;
    for (int face = 1; face <= 12; ++face)
    {
        check(damage::damage_die_value(face, DamageDieRule::normal) == face,
              "Declining a die replacement preserves every face");
        check(damage::damage_die_value(face, DamageDieRule::great_weapon_fighting) ==
                  (face < 3 ? 3 : face),
              "Great Weapon Fighting raises only 1 and 2 to 3");
    }
    // Independent SplitMix64 seed-zero sequence: d6 2,1,2,5,2,1,6,3.
    // These expected sums distinguish individual replacements from a total
    // floor, rerolls, doubled modifiers and replacing only ordinary hit dice.
    std::uint64_t normal = 0, style = 0;
    check(damage::roll_damage(normal, {2, 6, 4}) == 7 &&
              damage::roll_damage(style, {2, 6, 4}, false, DamageDieRule::great_weapon_fighting) ==
                  10,
          "Replace each die before adding the flat modifier");
    check(normal == 4354685564936845354ULL && style == normal, "No reroll or additional RNG draw");
    normal = style = 0;
    check(damage::roll_damage(normal, {2, 6, 4}, true) == 14 &&
              damage::roll_damage(style, {2, 6, 4}, true, DamageDieRule::great_weapon_fighting) ==
                  18,
          "Critical hits replace all four dice while adding the modifier once");
    check(normal == 8709371129873690708ULL && style == normal,
          "Critical replacement preserves the next random state");
    check(damage::roll_damage(style, {2, 6, 4}, true, DamageDieRule::great_weapon_fighting) == 19 &&
              style == 17418742259747381416ULL,
          "A second Savage-style critical roll independently replaces its four dice");
    normal = style = 0;
    check(damage::roll_damage(normal, {2, 6, -10}) == 0 &&
              damage::roll_damage(style, {2, 6, -10}, false,
                                  DamageDieRule::great_weapon_fighting) == 0,
          "Negative flat modifiers remain unchanged; total damage cannot be negative");
    normal = 0;
    const int signed_weapon = damage::roll_damage_component(normal, {2, 6, -4});
    check(signed_weapon == -1 && std::max(0, signed_weapon + 4) == 3,
          "Signed weapon modifier applies before adding Sneak dice and flooring attack total");
    style = 7;
    check(damage::roll_damage(style, {0, 0, 1}, true, DamageDieRule::great_weapon_fighting) == 1 &&
              style == 7,
          "Fixed damage gets neither extra damage nor random draws");
}
void gwf_prior_continuation()
{
    auto rules = module();
    const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures";
    auto current = [&](std::string bytes)
    {
        const auto at = bytes.find("0.6.29");
        check(at != bytes.npos, "Prior writer identity exists");
        bytes.replace(at, 6, rules->identity().version);
        return bytes;
    };
    const auto campaign = read(root / "campaign-v11-gwf-before.ogs");
    CampaignParty party(module());
    party.restore(
        decode_campaign(campaign, *srd5::character_rules(), *rules, "gwf-fixture", nullptr).party);
    auto body = [](const auto &s)
    {
        return s.substr(s.find('\n', s.find('\n') + 1) + 1);
    };
    check(body(encode_campaign(party, nullptr, "gwf-fixture")) ==
              test::with_tactical_mind_grants(body(current(campaign))),
          "Prior campaign changes only module identity and fixed Tactical Mind grant");
    auto combat = rules->restore(read(root / "combat-v14-gwf-first.save"));
    check(combat->save() == current(read(root / "combat-v14-gwf-first.save")),
          "Actual critical first-roll choice restores exactly");
    auto act = [&](std::string_view verb)
    {
        for (const auto &c : combat->legal_commands())
            if (c.verb == verb)
            {
                check(combat->submit(c), "Prior continuation command accepted");
                return;
            }
        throw std::runtime_error("Missing continuation command");
    };
    act("savage_use");
    check(combat->save() == current(read(root / "combat-v14-gwf-second.save")),
          "Second critical damage roll matches the pre-extraction writer's RNG and result");
    combat = rules->restore(combat->save());
    act("savage_second");
    check(combat->save() == current(read(root / "combat-v14-gwf-resolved.save")),
          "Applying the saved roll matches prior HP, action and Savage expenditure");
}
void freeze_gwf()
{
    auto rules = module();
    check(rules->identity().version == "0.6.29",
          "Requires the actual pre-Great Weapon Fighting writer");
    auto draft = hero("human").creation_data();
    draft.training = {{"origin:languages", {"elvish", "dwarvish"}},
                      {"class:fighter:fighting_style", {"defense"}}};
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*srd5::character_rules(), draft, {}));
    party.award_experience(2700, "gwf-fixture");
    for (int i = 0; i < 2; ++i)
        party.advance(id, party.default_advancement(id));
    auto choice = party.default_advancement(id);
    choice.feat = "archery";
    choice.abilities = {};
    party.advance(id, choice);
    auto state = party.checkpoint();
    state.roster[0].vitals.hit_points -= 3;
    state.roster[0].vitals.resources = "SRD1 1 0 0 0 0";
    party.restore(state);
    const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures";
    auto write = [&](const char *name, const std::string &bytes)
    {
        std::ofstream out(root / name, std::ios::binary);
        out << bytes;
        check(bool(out), "Write actual prior GWF fixture");
    };
    write("campaign-v11-gwf-before.ogs", encode_campaign(party, nullptr, "gwf-fixture"));
    auto members = party.participants();
    members[0].cell = {1, 1};
    members[0].character_profile = rules
                                       ->character_profile(party.member(id).character.sheet(),
                                                           std::array<std::string, 1>{"greatsword"})
                                       .data;
    members.push_back({99, "vanguard", "Enemy", 1, {2, 1}});
    auto combat = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, members}, 0);
    auto act = [&](std::string_view verb)
    {
        for (const auto &c : combat->legal_commands())
            if (c.verb == verb)
            {
                check(combat->submit(c), "Prior writer command accepted");
                return;
            }
        throw std::runtime_error("Missing prior writer command");
    };
    check(combat->snapshot().actor == id, "Prior writer initiative starts with Fighter");
    act("melee");
    check(combat->snapshot().savage_attack_choice &&
              combat->snapshot().savage_attack_choice->critical,
          "Prior writer critical hit awaits Savage choice");
    write("combat-v14-gwf-first.save", combat->save());
    act("savage_use");
    write("combat-v14-gwf-second.save", combat->save());
    act("savage_second");
    write("combat-v14-gwf-resolved.save", combat->save());
}

} // namespace
int main(int argc, char **argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--freeze-gwf")
        {
            freeze_gwf();
            return 0;
        }
        sneak_attack_foundation();
        damage_rolls();
        gwf_prior_continuation();
        arithmetic();
        species_combat();
        weapons_and_spells();
        migration();
        malformed();
        std::cout << "Typed damage tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
