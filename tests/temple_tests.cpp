// A temple's services (TEMPLE-1): each casts its SRD spell on a member it
// would help and is paid from the party's gold; one that would not help, or
// that the party cannot pay for, changes nothing.
#include "opengold/campaign_party.h"
#include "opengold/coin_purse.h"
#include "opengold/srd5.h"
#include "opengold/temple_catalog.h"
#include "status_effects.h"
#include "test_support.h"
#include <filesystem>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace opengold;
using namespace opengold::rules;

namespace
{
namespace fx = opengold::srd5::detail;
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const std::string &message)
{
    if (!ok)
    {
        throw std::runtime_error(message);
    }
}

std::unique_ptr<RulesModule> module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

// A level-10 Fighter, so a Cure has room to heal.
Character fighter()
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "fighter";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Patient";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    const auto rules = srd5::character_rules();
    for (bool filled = true; filled;)
    {
        filled = false;
        for (const auto &group : rules->training_options(d))
        {
            if (!d.training.contains(group.id))
            {
                for (unsigned i = 0; i < group.count; ++i)
                {
                    d.training[group.id].push_back(group.options.at(i).id);
                }
                filled = true;
                break;
            }
        }
    }
    Character hero(*rules, d, {});
    VitalState life;
    const auto combat = module();
    while (hero.sheet().level < 10)
    {
        check(hero.advance(*combat, life), "An ordinary level-up");
    }
    return hero;
}

struct Temple
{
    CampaignParty party{module()};
    MemberId patient{};
};

// A party of one with `gold`, its member at `hit_points`, or dead; `effects`
// adds the conditions it suffers.
Temple temple(int hit_points, std::uint32_t gold,
              const std::function<void(fx::EffectState &)> &effects = {}, bool dead = false)
{
    Temple t;
    t.patient = t.party.add_pc(fighter());
    auto state = t.party.checkpoint();
    auto &m = state.roster.front();
    // A complete vital record, with room for the effects.
    m.vitals = {m.character.sheet().hit_points, false, {}};
    t.party.rule_module().recover(m.vitals, m.character.sheet());
    m.vitals.hit_points = hit_points;
    m.vitals.dead = dead;
    if (effects)
    {
        const auto at = m.vitals.resources.find("FX8");
        check(at != std::string::npos, "The record holds its effects");
        std::istringstream in(m.vitals.resources.substr(at));
        auto read = fx::read_effects(in);
        effects(read);
        std::ostringstream out;
        fx::write_effects(out, read);
        m.vitals.resources.replace(at, std::string::npos, out.str());
    }
    coins(state.wealth, Coin::gold) = gold;
    t.party.restore(std::move(state));
    return t;
}

constexpr fx::EffectSource foe{.scope = 9, .actor = 99};

void add(fx::EffectState &effects, fx::EffectKind kind, int value = 0)
{
    fx::apply_spell_benefit(effects, foe, "Foe", kind, value);
}

std::uint32_t gold(const Temple &t)
{
    return coins(t.party.state().wealth, Coin::gold);
}

const PartyMember &patient(const Temple &t)
{
    return t.party.member(t.patient);
}

// The service helps, is paid for, and afterwards is no longer needed.
void cures(Temple &t, TempleService service, std::uint32_t price, const std::string &what)
{
    check(t.party.temple_service_helps(t.patient, service), what + " is needed");
    const auto before = gold(t);
    t.party.temple_service(t.patient, service, price);
    check(gold(t) == before - price, what + " is paid for");
    check(!t.party.temple_service_helps(t.patient, service), what + " is no longer needed");
}

void cure_checks()
{
    // Cure Wounds from a level-1, -3 or -5 slot: 2d8, 6d8 or 10d8, + 3.
    for (const auto &[service, dice] :
         std::array<std::pair<TempleService, int>, 3>{{{TempleService::cure_light_wounds, 2},
                                                       {TempleService::cure_serious_wounds, 6},
                                                       {TempleService::cure_critical_wounds, 10}}})
    {
        auto t = temple(1, 1000);
        const int maximum = t.party.hit_point_maximum(t.patient);
        t.party.temple_service(t.patient, service, 100);
        const int healed = patient(t).vitals.hit_points - 1;
        check(healed >= std::min(maximum - 1, dice + 3) && healed <= dice * 8 + 3 && gold(t) == 900,
              "A Cure heals its dice + 3 for its price");
    }
    auto whole = temple(0, 1000);
    auto state = whole.party.checkpoint();
    state.roster.front().vitals.hit_points = whole.party.hit_point_maximum(whole.patient);
    whole.party.restore(state);
    check(!whole.party.temple_service_helps(whole.patient, TempleService::cure_light_wounds),
          "An unwounded member needs no Cure");
    const auto before = whole.party.checkpoint();
    test::rejects(
        [&]
        {
            whole.party.temple_service(whole.patient, TempleService::cure_light_wounds, 100);
        },
        "A service that would not help is refused");
    check(whole.party.checkpoint().wealth == before.wealth, "and costs nothing");
    auto poor = temple(1, 99);
    test::rejects(
        [&]
        {
            poor.party.temple_service(poor.patient, TempleService::cure_light_wounds, 100);
        },
        "A party that cannot pay is refused");
    check(gold(poor) == 99 && patient(poor).vitals.hit_points == 1, "and nothing changes");
}

void condition_checks()
{
    const int full = fighter().sheet().hit_points;
    auto blind = temple(full, 1000,
                        [](auto &e)
                        {
                            fx::apply_poisoned(e, foe, "Foe", 6000, fx::EffectKind::dazzled);
                        });
    cures(blind, TempleService::cure_blindness, 1000, "Cure Blindness");
    auto poisoned = temple(full, 1000,
                           [](auto &e)
                           {
                               fx::apply_poisoned(e, foe, "Foe", 6000, fx::EffectKind::poisoned);
                           });
    check(!poisoned.party.temple_service_helps(poisoned.patient, TempleService::cure_disease),
          "Poison is no disease");
    cures(poisoned, TempleService::neutralize_poison, 1000, "Neutralize Poison");
    auto diseased = temple(full, 1000,
                           [](auto &e)
                           {
                               add(e, fx::EffectKind::contagion, 2);
                           });
    check(!diseased.party.temple_service_helps(diseased.patient, TempleService::neutralize_poison),
          "Contagion's disease is not a poison to neutralize");
    cures(diseased, TempleService::cure_disease, 1000, "Cure Disease");
    auto cursed = temple(full, 3500,
                         [](auto &e)
                         {
                             add(e, fx::EffectKind::cursed);
                         });
    cures(cursed, TempleService::remove_curse, 3500, "Remove Curse");
    auto stone = temple(full, 2000,
                        [](auto &e)
                        {
                            add(e, fx::EffectKind::petrified, 1);
                        });
    cures(stone, TempleService::stone_to_flesh, 2000, "Stone to Flesh");
}

// Remove Curse frees a member from a cursed item it holds, which is lost.
void cursed_item_checks()
{
    auto t = temple(fighter().sheet().hit_points, 3500);
    por::Equipment broad;
    broad.stored.type = 35;
    broad.stored.magic_bonus = -2;
    broad.stored.name_components = {255, 202, 35};
    broad.stored.stack_size = 1;
    broad.stored.cursed_raw = 1;
    check(t.party.award_loot({}, {broad}, "test:cursed"), "Collect the cursed sword");
    const auto sword = patient(t).character.inventory().items().back().id;
    for (const auto worn : patient(t).equipped)
    {
        t.party.unequip(t.patient, worn);
    }
    t.party.equip(t.patient, sword);
    cures(t, TempleService::remove_curse, 3500, "Remove Curse on a cursed sword");
    check(!patient(t).character.inventory().find(sword), "The cursed sword is put down and lost");
}

void raising_checks()
{
    auto recent = temple(
        0, 5500,
        [](auto &e)
        {
            add(e, fx::EffectKind::deceased);
        },
        true);
    check(!recent.party.temple_service_helps(recent.patient, TempleService::resurrection),
          "Resurrection is for those Raise Dead cannot help");
    cures(recent, TempleService::raise_dead, 5500, "Raise Dead");
    check(!patient(recent).vitals.dead && patient(recent).vitals.hit_points == 1,
          "Raise Dead returns the member with 1 Hit Point");
    auto dust = temple(
        0, 11000,
        [](auto &e)
        {
            add(e, fx::EffectKind::deceased);
            add(e, fx::EffectKind::dust);
        },
        true);
    check(!dust.party.temple_service_helps(dust.patient, TempleService::raise_dead),
          "Raise Dead cannot help a member turned to dust");
    cures(dust, TempleService::resurrection, 11000, "Resurrection");
    check(!patient(dust).vitals.dead &&
              patient(dust).vitals.hit_points == dust.party.hit_point_maximum(dust.patient),
          "Resurrection returns the member with all its Hit Points");
    // Dead more than ten days: the deceased mark has run out.
    auto long_dead = temple(0, 11000, {}, true);
    check(!long_dead.party.temple_service_helps(long_dead.patient, TempleService::raise_dead) &&
              long_dead.party.temple_service_helps(long_dead.patient, TempleService::resurrection),
          "Only Resurrection helps a member dead more than ten days");
}

void catalog_checks()
{
    const auto offerings = original_temple_offerings();
    check(offerings.size() == 10 && offerings[2].service == TempleService::cure_light_wounds &&
              offerings[2].price_gp == 100 && offerings[6].price_gp == 5500 &&
              offerings[9].service == TempleService::resurrection && offerings[9].price_gp == 11000,
          "The original's nine services and prices, and Resurrection's");
    check(temple_service_named("stone_to_flesh") == TempleService::stone_to_flesh &&
              !temple_service_named("heal"),
          "Services are named by the data's ids");
}
} // namespace

int main()
{
    try
    {
        catalog_checks();
        cure_checks();
        condition_checks();
        cursed_item_checks();
        raising_checks();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Temple tests passed\n";
}
