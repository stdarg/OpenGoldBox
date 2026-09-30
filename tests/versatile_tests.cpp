#include "opengold/srd5.h"
#include "opengold/campaign_save.h"
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
void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

template <class F> void rejects(F f)
{
    bool rejected = false;
    try
    {
        f();
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    check(rejected, "Invalid grip/equipment must reject");
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

void damage_and_resources()
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
        for (const unsigned hands :
                {
                    1u, 2u
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
                    const std::array<std::string, 1> gear{weapon.key};
                    auto profile = rules->character_profile(sheet, gear, {hands});
                    check(profile.equipment.weapon_hands == hands && profile.grips.size() == 2 &&
                          profile.grips[1].available,
                          "Rules report the selected grip and both legal options");
                    Encounter e{{8, 8, std::vector<std::uint8_t>(64)},
                        {   {1, "campaign-character", "Hero", 0, {1, 1}, profile.data},
                            {2, "vanguard", "Target", 1, {thrown ? 3 : 2, 1}}
                        }};
                    auto combat = rules->create(e, seed);
                    check(combat->snapshot().actor == 1, "Fixed seed starts the hero");
                    if (hands == 2)
                    {
                        check(combat->submit(command(*combat, "grip_one")) &&
                              combat->submit(command(*combat, "grip_two")),
                              "Change grip before attacking");
                    }
                    const auto before = unit(*combat, 1);
                    const auto attack = command(*combat, thrown ? "ranged" : "melee");
                    check(combat->submit(attack), "Weapon attack accepted");
                    check(unit(*combat, 2).hit_points ==
                          28 - damage(seed, thrown || hands == 1 ? weapon.one : weapon.two),
                          "Melee/thrown/critical damage matches fixed SRD dice oracle");
                    const auto after = unit(*combat, 1);
                    check(!after.action && after.bonus_action == before.bonus_action &&
                          after.reaction == before.reaction &&
                          after.movement_feet == before.movement_feet,
                          "Weapon attack spends only its action");
                    const auto saved = combat->save();
                    auto restored = rules->restore(saved);
                    check(restored->save() == saved,
                          "Selected grip and attack resources round trip");
                    if (thrown)
                    {
                        check(after.grips.empty(),
                              "A thrown-away weapon no longer offers grip changes");
                        const auto state = combat->snapshot();
                        check(state.held_items.size() == 1 && !state.held_items[0].holder &&
                              state.held_items[0].cell == Cell{3, 1},
                              "Hit and miss both leave the weapon on its target square");
                        continue;
                    }
                    const auto change = command(*combat, hands == 1 ? "grip_two" : "grip_one");
                    check(combat->submit(change) && restored->submit(change) &&
                          combat->save() == restored->save(),
                          "Grip can change after spending the action and resumes identically");
                    const auto changed = unit(*combat, 1);
                    check(changed.persistent == after.persistent &&
                          changed.action == after.action &&
                          changed.bonus_action == after.bonus_action &&
                          changed.reaction == after.reaction &&
                          changed.movement_feet == after.movement_feet &&
                          combat->snapshot().elapsed_milliseconds == 0,
                          "Grip change spends and restores no resources or elapsed time");
                    const auto unchanged = combat->save();
                    check(!combat->submit(change) && combat->save() == unchanged,
                          "Stale grip command is atomic");
                }
    for (const auto &weapon : weapons)
    {
        const std::array<std::string, 2> gear{weapon.key, "shield"};
        const auto profile = rules->character_profile(sheet, gear, {1});
        check(profile.armor_class == 14 && !profile.grips[1].available,
              "One hand retains trained shield AC; two hands unavailable");
        rejects(
            [&]
        {
            (void)rules->character_profile(sheet, gear, {2});
        });
        rejects(
            [&]
        {
            (void)rules->character_profile(sheet, gear, {3});
        });
        auto combat = rules->create({{8, 8, std::vector<std::uint8_t>(64)},
            {   {1, "campaign-character", "Hero", 0, {1, 1}, profile.data},
                {2, "vanguard", "Target", 1, {2, 1}}
            }},
        0);
        const auto saved = combat->save();
        std::istringstream input(saved);
        std::string row, corrupt;
        unsigned line = 0;
        while (std::getline(input, row))
        {
            if (line++ == 4)
                row.replace(row.find_last_of(' ') + 1, std::string::npos, "2");
            corrupt += row + '\n';
        }
        rejects(
            [&]
        {
            (void)rules->restore(corrupt);
        });
        check(combat->save() == saved, "Malformed saved shield/grip leaves the session intact");
        Command invalid{combat->snapshot().revision, 1, 0, "grip_two"};
        check(!combat->submit(invalid) && combat->save() == saved,
              "Forged two-hand command with a shield rejects atomically");
    }
    for (const std::string key :
            {"greatsword", "shortbow", "mace", "wand"
            })
    {
        const std::array gear{key};
        check(rules->character_profile(sheet, gear).grips.empty(),
              "Fixed-grip weapons and foci do not offer Versatile");
        rejects(
            [&]
        {
            (void)rules->character_profile(
            sheet, gear, {key == "greatsword" || key == "shortbow" ? 1u : 2u});
        });
    }
}

void reaction_continuation()
{
    auto rules = module();
    const std::array<std::string, 1> gear{"longsword"};
    const auto profile = rules->character_profile(hero().sheet(), gear, {1});
    Encounter e{{8, 8, std::vector<std::uint8_t>(64)},
        {   {1, "campaign-character", "Reactor", 0, {1, 1}, profile.data},
            {2, "vanguard", "Mover", 1, {2, 1}}
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
    check(combat->snapshot().reaction_pending && combat->snapshot().actor == 1,
          "Hero may choose grip during own opportunity reaction");
    const auto before = unit(*combat, 1);
    const auto target = unit(*combat, 2);
    const auto elapsed = combat->snapshot().elapsed_milliseconds;
    const auto two = command(*combat, "grip_two");
    check(combat->submit(two), "Switch grip during a pending reaction");
    check(combat->snapshot().reaction_pending && unit(*combat, 1).reaction == before.reaction &&
          unit(*combat, 2).cell == target.cell &&
          combat->snapshot().elapsed_milliseconds == elapsed,
          "Grip selection neither resolves nor bypasses the pending attack");
    auto restored = rules->restore(combat->save());
    const auto attack = command(*combat, "opportunity");
    check(combat->submit(attack) && restored->submit(attack) && combat->save() == restored->save(),
          "Opportunity damage and interrupted movement continue identically after save");
    check(!unit(*combat, 1).reaction && unit(*combat, 1).action == before.action &&
          unit(*combat, 1).equipment.weapon_hands == 2,
          "Only the opportunity reaction is consumed");
}

void campaign()
{
    auto rules = module();
    CampaignParty party(module());
    auto character = hero();
    const auto staff = character.inventory().add("quarterstaff", "Quarterstaff", 1),
               shield = character.inventory().add("shield", "Shield", 1),
               mace = character.inventory().add("mace", "Mace", 1);
    const auto id = party.add_pc(std::move(character));
    party.equip(id, staff);
    auto wounded = party.checkpoint();
    wounded.roster[0].vitals = {5, false, "SRD9 1 0 0 0 0 0 1 0 0 0 \"\" 0 0 0 FX7 1 0 0 0"};
    party.restore(wounded);
    const auto vitals = party.member(id).vitals;
    party.set_grip(id, 2);
    const auto saved = encode_campaign(party, nullptr, "grip");
    rejects(
        [&]
    {
        party.equip(id, shield);
    });
    check(encode_campaign(party, nullptr, "grip") == saved,
          "Shield rejection preserves equipment, grip, vitals and campaign state");
    auto decoded = decode_campaign(saved, *srd5::character_rules(), *rules, "grip", nullptr);
    CampaignParty restored(module());
    restored.restore(std::move(decoded.party));
    check(encode_campaign(restored, nullptr, "grip") == saved &&
          restored.profile(id).equipment.weapon_hands == 2,
          "Campaign persists the selected grip canonically");
    party.set_grip(id, 1);
    party.equip(id, shield);
    const auto with_shield = encode_campaign(party, nullptr, "grip");
    rejects(
        [&]
    {
        party.set_grip(id, 2);
    });
    check(encode_campaign(party, nullptr, "grip") == with_shield,
          "Grip rejection preserves shield and all campaign state");
    party.unequip(id, shield);
    party.set_grip(id, 2);
    party.equip(id, mace);
    check(party.profile(id).equipment.weapon_hands == 1 && party.member(id).vitals == vitals,
          "Replacing the weapon clears its grip without refreshing vitals");
    party.equip(id, staff);
    party.set_grip(id, 2);
    party.unequip(id, staff);
    check(party.member(id).equipment.weapon_hands == 0, "Removing weapon clears grip choice");
    party.equip(id, staff);
    auto participants = party.participants();
    participants[0].cell = {1, 1};
    participants.push_back({2, "vanguard", "Target", 1, {2, 1}});
    auto combat = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, participants}, 0);
    party.begin_combat();
    rejects(
        [&]
    {
        party.set_grip(id, 2);
    });
    check(combat->submit(command(*combat, "grip_two")), "Combat owns grip while active");
    party.apply_combat(combat->snapshot());
    party.end_combat();
    check(party.member(id).equipment.weapon_hands == 2 && party.member(id).vitals.hit_points == 5,
          "Combat hands the chosen grip and wounds back to the campaign");
    check(party.participants()[0].character_profile ==
          rules
          ->character_profile(party.member(id).character.sheet(),
                              std::array<std::string, 1> {"quarterstaff"}, {2})
          .data,
          "Next encounter uses the retained grip");
    auto invalid = party.checkpoint();
    invalid.roster[0].equipped.push_back(shield);
    const auto valid = encode_campaign(party, nullptr, "grip");
    rejects(
        [&]
    {
        party.restore(invalid);
    });
    check(encode_campaign(party, nullptr, "grip") == valid,
          "Invalid equipment checkpoint does not replace the live campaign");
    auto body = valid.substr(valid.find('\n', valid.find('\n') + 1) + 1);
    const auto grip = body.find("2 6 \"feature:fighting_style\"");
    check(grip != body.npos,
          "Single-member fixture stores grip before Fighter and language grants");
    body[grip] = '3';
    std::uint64_t checksum = 14695981039346656037ULL;
    for (unsigned char c : body)
    {
        checksum ^= c;
        checksum *= 1099511628211ULL;
    }
    rejects(
        [&]
    {
        (void)decode_campaign("OPENGOLD-CAMPAIGN 19\n" + std::to_string(checksum) + "\n" + body,
        *srd5::character_rules(), *rules, "grip", nullptr);
    });
    check(encode_campaign(party, nullptr, "grip") == valid,
          "Malformed serialized grip cannot replace the current campaign");
}

// The Godot grip view check loads this checkpoint: a Fighter holding a
// Battleaxe in two hands acts first with an unspent action, and the only enemy
// is out of reach.
void grip_fixture()
{
    auto rules = module();
    const std::array<std::string, 1> gear{"battleaxe"};
    const auto profile = rules->character_profile(hero().sheet(), gear, EquipmentState{2});
    Encounter encounter{{8, 8, std::vector<std::uint8_t>(64)},
        {   {1, "campaign-character", "Grip tester", 0, {1, 1}, profile.data},
            {2, "vanguard", "Target", 1, {6, 6}}
        }};
    std::unique_ptr<CombatSession> combat;
    for (unsigned seed = 0; seed < 100; ++seed)
    {
        combat = rules->create(encounter, seed);
        if (combat->snapshot().actor == 1)
            break;
    }
    check(combat->snapshot().actor == 1, "The two-handed Fighter acts first");
    const auto fighter = unit(*combat, 1);
    check(fighter.equipment.weapon_hands == 2 && fighter.action && fighter.bonus_action,
          "The Fighter holds the Battleaxe in two hands with both actions unspent");
    (void)command(*combat, "dash");
    const auto folder = std::filesystem::path(OPENGOLD_BINARY_DIR) / "grip-fixtures";
    std::filesystem::create_directories(folder);
    std::ofstream out(folder / "grips.save", std::ios::binary);
    out << combat->save();
    check(bool(out), "Write the grip view fixture");
}
} // namespace

int main()
{
    try
    {
        damage_and_resources();
        reaction_continuation();
        campaign();
        grip_fixture();
        std::cout
                << "Versatile tests passed: seven weapons, damage, shields, reactions and campaign\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
