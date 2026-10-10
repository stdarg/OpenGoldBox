#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

std::unique_ptr<RulesModule> module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

Character created(std::string klass, std::string name)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = std::move(klass);
    d.background = "acolyte";
    d.alignment = "lawful_good";
    d.name = std::move(name);
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    return Character(*srd5::character_rules(), d, {});
}

CombatantView unit(const CombatSession &c, EntityId id)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing combatant");
}

std::vector<EntityId> lay_on_hands_targets(const CombatSession &c)
{
    std::vector<EntityId> targets;
    for (const auto &command : c.legal_commands())
        if (command.verb == "lay_on_hands")
            targets.push_back(command.target);
    std::sort(targets.begin(), targets.end());
    return targets;
}

unsigned pool(const CombatSession &c)
{

    for (const auto &resource : unit(c, 1).resources)
        if (resource.id == "lay_on_hands")
            return resource.remaining;
    throw std::runtime_error("Missing Lay On Hands pool");
}

// A Paladin at (1,1), a wounded ally beside it with `ally_hp`, another ally
// 10 feet away and an enemy far off. Returns the session on the Paladin's turn.
std::unique_ptr<CombatSession> battle(const RulesModule &rules, const Character &paladin,
                                      const Character &ally, int ally_hp,
                                      const VitalState *paladin_state = nullptr)
{
    const auto paladin_profile = rules.character_profile(paladin.sheet(), {}).data;
    const auto ally_profile = rules.character_profile(ally.sheet(), {}).data;
    std::vector<Participant> setup{
        {1, "campaign-character", "Paladin", Side::party, {1, 1}, paladin_profile},
        {2, "campaign-character", "Ally", Side::party, {2, 1}, ally_profile,
            VitalState{ally_hp, false, {}}},
        {3, "campaign-character", "Distant", Side::party, {1, 3}, ally_profile,
            VitalState{1, false, {}}},
        {99, "vanguard", "Enemy", Side::opposition, {10, 6}}};
    if (paladin_state)
        setup[0].state = *paladin_state;
    auto c = rules.create({{12, 8, std::vector<Terrain>(96)}, setup}, 7);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
    {
        bool ended = false;
        for (const auto &command : c->legal_commands())
            if (command.verb == "end")
            {
                ended = c->submit(command);
                break;
            }
        check(ended, "Reach the Paladin's turn");
    }
    check(c->snapshot().actor == 1, "The Paladin acts");
    return c;
}

void combat_checks()
{
    auto rules = module();
    const auto paladin = created("paladin", "Paladin");
    const auto ally = created("fighter", "Ally");
    const int ally_max = ally.sheet().hit_points;

    auto c = battle(*rules, paladin, ally, 2);
    check(pool(*c) == 5, "A level-one Paladin's pool holds five Hit Points");
    check(lay_on_hands_targets(*c) == std::vector<EntityId> {2},
          "Only a wounded ally within 5 feet is offered; the unhurt Paladin and the distant ally are not");
    const auto before = c->save();
    auto copy = rules->restore(before);
    check(copy->save() == before, "The pool survives a combat checkpoint");
    const auto offered = c->legal_commands();
    const auto bonus = std::find_if(offered.begin(), offered.end(), [](const auto & command)
    {
        return command.verb == "lay_on_hands";
    });
    check(bonus != offered.end() && c->submit(*bonus), "Lay On Hands resolves");
    const int healed = std::min(5, ally_max - 2);
    check(unit(*c, 2).hit_points == 2 + healed && pool(*c) == unsigned(5 - healed),
          "It restores what is missing up to the pool, spending only what it restored");
    check(!unit(*c, 1).bonus_action && unit(*c, 1).action,
          "It spends the Bonus Action and keeps the Action");
    check(lay_on_hands_targets(*c).empty(), "One Bonus Action per turn");

    auto dying = battle(*rules, paladin, ally, 0);
    check(lay_on_hands_targets(*dying) == std::vector<EntityId> {2},
          "An ally at 0 Hit Points can be healed");
    const auto dying_offers = dying->legal_commands();
    for (const auto &command : dying_offers)
        if (command.verb == "lay_on_hands")
        {
            check(dying->submit(command), "Lay On Hands reaches a dying ally");
            break;
        }
    check(unit(*dying, 2).conscious && unit(*dying, 2).hit_points == 5,
          "A dying ally regains consciousness");
}

// A Paladin beside a wounded ally, for tests/lay_on_hands_view_tests.gd.
void write_ui_fixture()
{
    auto rules = module();
    const auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "lay-fixtures";
    std::filesystem::create_directories(path);
    auto c = battle(*rules, created("paladin", "Paladin"), created("fighter", "Ally"), 2);
    std::ofstream out(path / "adjacent.save", std::ios::binary);
    out << c->save();
    check(bool(out), "Write the UI fixture");
}

void campaign_checks()
{
    auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(created("paladin", "Paladin"));
    const auto pool_of = [&]
    {
        for (const auto &resource : rules->recovery_info(party.member(id).character.sheet(),
                party.member(id).vitals).resources)
            if (resource.id == "lay_on_hands")
                return resource;
        throw std::runtime_error("Missing Lay On Hands pool");
    };
    check(pool_of().capacity == 5 && pool_of().remaining == 5 && !pool_of().short_rest_recovery,
          "The pool is five per level and not restored by a Short Rest");
    party.award_experience(2700, "paladin-xp");
    for (unsigned level = 2; level <= 4; ++level)
        party.advance(id, party.default_advancement(id));
    check(pool_of().capacity == 20 && pool_of().remaining == 20,
          "Each Paladin level adds five Hit Points to the pool");
    const auto bytes = encode_campaign(party, nullptr, "lay-on-hands");
    CampaignParty restored(module());
    restored.restore(
        decode_campaign(bytes, *srd5::character_rules(), *rules, "lay-on-hands", nullptr).party);
    check(encode_campaign(restored, nullptr, "lay-on-hands") == bytes,
          "The pool round-trips through a campaign save");
}
} // namespace

int main()
{
    try
    {
        combat_checks();
        campaign_checks();
        write_ui_fixture();
        std::cout << "Lay On Hands tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
