#include "opengold/srd5.h"
#include "opengold/campaign_save.h"
#include <algorithm>
#include <array>
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

auto module()
{
    return srd5::load(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "data/rules/srd-5.2.1/combat.rules");
}

Character hero()
{
    CharacterDraft draft;
    draft.race = "human";
    draft.gender = "female";
    draft.character_class = "fighter";
    draft.background = "sage";
    draft.alignment = "neutral_good";
    draft.name = "Grip tester";
    draft.rolled = true;
    for (auto &roll : draft.rolls)
        roll = {{6, 5, 4, 1}, 3};
    return Character(*srd5::character_rules(), draft, {});
}

CombatantView unit(const CombatSession &session, EntityId id)
{
    for (const auto &a : session.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing actor");
}

Command command(const CombatSession &session, std::string_view verb)
{
    for (const auto &c : session.legal_commands())
        if (c.verb == verb)
            return c;
    throw std::runtime_error("Missing command: " + std::string(verb));
}

bool has_grip_command(const CombatSession &session)
{
    const auto commands = session.legal_commands();
    return std::any_of(commands.begin(), commands.end(), [](const auto & c)
    {
        return c.verb.starts_with("grip");
    });
}

struct WeaponCase
{
    const char *key;
    int one, two;
    bool thrown;
};

// Independent SRD 5.2.1 pp. 90-91 expectations, including the 2024 War Pick.
constexpr std::array weapons
{
    WeaponCase{"quarterstaff", 6, 8, false}, WeaponCase{"spear", 6, 8, true},
    WeaponCase{"battleaxe", 8, 10, false},   WeaponCase{"longsword", 8, 10, false},
    WeaponCase{"trident", 8, 10, true},      WeaponCase{"warhammer", 8, 10, false},
    WeaponCase{"war_pick", 8, 10, false}};

// The grip follows the other hand: two hands when it is empty, one otherwise.
void damage_follows_other_hand()
{
    auto rules = module();
    const auto sheet = hero().sheet();
    check(sheet.modifiers[0] == 2 && sheet.modifiers[1] == 2,
          "Fixed damage fixture has +2 Strength/Dexterity");
    // Fixed SplitMix64 seed oracles: two initiative rolls, then attack/damage.
    // Seed 0: critical 20; seed 13: ordinary 17; seed 40: natural 1.
    const auto damage = [](int seed, int sides)
    {
        return seed == 40  ? 0
               : seed == 0 ? (sides == 6   ? 9
                              : sides == 8 ? 11
                              : 15)
               : (sides == 6   ? 4
                  : sides == 8 ? 6
                  : 10);
    };
    for (const auto &weapon : weapons)
        for (const bool shield :
                {
                    false, true
                })
            for (const int seed :
                    {
                        0, 13, 40
                    })
                for (const bool thrown :
                        {
                            false, true
                        })
                {
                    if (thrown && !weapon.thrown)
                        continue;
                    std::vector<std::string> gear{weapon.key};
                    if (shield)
                        gear.emplace_back("shield");
                    const auto profile = rules->character_profile(sheet, gear);
                    check(profile.weapon_hands == (shield ? 1u : 2u),
                          "Rules report two hands only when the other hand is empty");
                    check(!shield || profile.armor_class == 14,
                          "One-handed grip keeps the trained shield's AC");
                    Encounter e{{8, 8, std::vector<Terrain>(64)},
                        {   {1, "campaign-character", "Hero", Side::party, {1, 1}, profile.data},
                            {2, "vanguard", "Target", Side::opposition, {thrown ? 3 : 2, 1}}
                        }};
                    auto combat = rules->create(e, seed);
                    check(combat->snapshot().actor == 1, "Fixed seed starts the hero");
                    check(!has_grip_command(*combat), "No grip command is offered");
                    const auto before = unit(*combat, 1);
                    check(combat->submit(command(*combat, thrown ? "ranged" : "melee")),
                          "Weapon attack accepted");
                    const int sides = thrown || shield ? weapon.one : weapon.two;
                    check(unit(*combat, 2).hit_points == 28 - damage(seed, sides),
                          "Melee/thrown/critical damage matches fixed SRD dice oracle");
                    const auto log = combat->snapshot().log();
                    const std::string grip = thrown ? " damage." : shield ? " damage (one-handed)."
                                             : " damage (two-handed).";
                    check(seed == 40 || log.back().ends_with(grip),
                          "The damage line names the grip a melee hit used");
                    const auto after = unit(*combat, 1);
                    check(!after.action && after.bonus_action == before.bonus_action &&
                          after.reaction == before.reaction &&
                          after.movement_feet == before.movement_feet,
                          "Weapon attack spends only its action");
                    const auto saved = combat->save();
                    check(rules->restore(saved)->save() == saved,
                          "Attack resources round trip");
                }
    for (const auto &weapon : weapons)
    {
        const std::array<std::string, 2> gear{weapon.key, "dagger"};
        check(rules->character_profile(sheet, gear).weapon_hands == 1,
              "A second held weapon keeps a Versatile weapon one-handed");
    }
    for (const auto &[key, hands] :
            std::array<std::pair<const char *, unsigned>, 3> {{{"greatsword", 2}, {"shortbow", 2}, {"mace", 1}}})
    {
        const std::array<std::string, 1> gear{key};
        check(rules->character_profile(sheet, gear).weapon_hands == hands,
              "Fixed-grip weapons keep their own hand count");
    }
}

void reaction_continuation()
{
    auto rules = module();
    const std::array<std::string, 1> gear{"longsword"};
    const auto profile = rules->character_profile(hero().sheet(), gear);
    Encounter e{{8, 8, std::vector<Terrain>(64)},
        {   {1, "campaign-character", "Reactor", Side::party, {1, 1}, profile.data},
            {2, "vanguard", "Mover", Side::opposition, {2, 1}}
        }};
    std::unique_ptr<CombatSession> combat;
    for (unsigned seed = 0; seed < 100; ++seed)
    {
        combat = rules->create(e, seed);
        if (combat->snapshot().actor == 2)
            break;
    }
    auto move = command(*combat, "move");
    move.destination = {3, 1};
    check(combat->submit(move), "Enemy attempts to leave reach");
    check(combat->snapshot().reaction_pending && combat->snapshot().actor == 1 &&
          !has_grip_command(*combat),
          "The reaction offers no grip decision");
    const auto before = unit(*combat, 1);
    auto restored = rules->restore(combat->save());
    const auto attack = command(*combat, "opportunity");
    check(combat->submit(attack) && restored->submit(attack) && combat->save() == restored->save(),
          "Opportunity damage and interrupted movement continue identically after save");
    check(!unit(*combat, 1).reaction && unit(*combat, 1).action == before.action,
          "Only the opportunity reaction is consumed");
}

void campaign()
{
    auto rules = module();
    CampaignParty party(module());
    auto character = hero();
    const auto staff = character.add_item({.definition_id = "quarterstaff",
                                                  .name = "Quarterstaff",
                                                  .quantity = 1}),
               shield = character.add_item({.definition_id = "shield",
                                                   .name = "Shield",
                                                   .quantity = 1});
    const auto id = party.add_pc(std::move(character));
    party.equip(id, staff);
    auto wounded = party.checkpoint();
    wounded.roster[0].vitals = {5, false, "SRD11 1 0 0 0 0 0 1 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0"};
    party.restore(wounded);
    const auto vitals = party.member(id).vitals;
    check(party.profile(id).weapon_hands == 2, "An empty other hand wields the staff two-handed");
    party.equip(id, shield);
    check(party.profile(id).weapon_hands == 1 && party.member(id).vitals == vitals,
          "Equipping a shield makes the staff one-handed without touching vitals");
    const auto saved = encode_campaign(party, nullptr, "grip");
    auto decoded = decode_campaign(saved, *srd5::character_rules(), *rules, "grip", nullptr);
    CampaignParty restored(module());
    restored.restore(std::move(decoded.party));
    check(encode_campaign(restored, nullptr, "grip") == saved &&
          restored.profile(id).weapon_hands == 1,
          "The campaign stores equipment, and the grip is derived from it");
    party.unequip(id, shield);
    check(party.profile(id).weapon_hands == 2, "Removing the shield frees the other hand");
    auto participants = party.participants();
    check(participants[0].character_profile ==
          rules->character_profile(party.member(id).character.sheet(),
                                   std::array<std::string, 1> {"quarterstaff"}).data,
          "The next encounter uses the current equipment");
}
} // namespace

int main()
{
    try
    {
        damage_follows_other_hand();
        reaction_continuation();
        campaign();
        std::cout << "Versatile tests passed: seven weapons, automatic grip, reactions and campaign\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
