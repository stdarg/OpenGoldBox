#include "combat_fixture.h"
#include "forwarding_module.h"
#include "opengold/campaign_party.h"
#include "opengold/ecl_party_host.h"
#include "opengold/character_creator.h"
#include "opengold/combat_demo.h"
#include "opengold/combat_body_catalog.h"
#include "opengold/campaign_save.h"
#include "opengold/rolf_tour.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <chrono>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>
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
    bool rejected = false;
    try
    {
        f();
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    check(rejected, "Operation should reject");
}

void combat_body_assignments()
{
    const auto folder = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/art";
    const auto saved = por::CombatBodyCatalog::load(folder / "combat-body-looks.tsv",
        folder / "combat-weapon-options.tsv");
    check(saved.options.size() == 48, "Options contain ordinary shop weapons, wand and unarmed");
    for (const auto &option : saved.options)
        for (const bool shield :
                {
                    false, true
                })
        {
            const auto combination = option.id + (shield ? "_shield" : "");
            std::vector<por::CombatEquipment> gear;
            if (option.original_type)
                gear.push_back({option.original_type, option.label, {}});
            if (shield)
                gear.push_back({59, "Shield", "shield"});
            const auto selected = saved.choose(gear, 10);
            check(selected.combination == combination,
                  "Every catalog option resolves its exact equipment key");
            const auto assigned = std::find_if(saved.bodies.begin(), saved.bodies.end(),
                                               [&](const auto & body)
            {
                return body.contains(combination);
            });
            const bool has_art =
                !saved.deleted.contains(combination) && assigned != saved.bodies.end();
            check(selected.matched == has_art,
                  "Only assigned, non-deleted combinations have artwork");
            check(
                selected.body ==
                (has_art ? (saved.bodies[10].contains(combination)
                            ? 10u
                            : static_cast<unsigned>(assigned - saved.bodies.begin()))
                 : 10u),
                "Catalog selection prefers the saved body, then first assignment, otherwise fallback");
        }
    const std::vector<por::CombatEquipment> shield_only{{59, "Shield", "shield"}};
    por::CombatBodyCatalog catalog = saved;
    for (auto &body : catalog.bodies)
        body.clear();
    catalog.deleted.clear();
    catalog.bodies[32] = {"type_0_shield"};
    check(catalog.choose(shield_only, 21).matched && catalog.choose(shield_only, 21).body == 32,
          "Unarmed with shield selects the assigned derived wand-free body");
    catalog.bodies[1] = {"type_43", "type_44"};
    catalog.bodies[4] = {"type_23_shield"};
    catalog.bodies[7] = {"type_23"};
    catalog.bodies[9] = {"type_43"};
    const std::vector<por::CombatEquipment> mace_shield{{23, "Mace", "mace"},
        {59, "Shield", "shield"}},
    mace{{23, "Mace", "mace"}}, bow{{43, "Long Bow", "longbow"}},
    shortbow{{44, "Short Bow", "shortbow"}}, silver_mace{{23, "Silver Mace", "mace"}};
    check(catalog.choose(mace_shield, 31).body == 4, "Exact shield match");
    check(catalog.choose(mace, 31).body == 7, "Exact unshielded match");
    check(catalog.choose(bow, 31).body == 1 && catalog.choose(shortbow, 31).body == 1,
          "Bows share one body");
    check(catalog.choose(bow, 9).body == 9, "Prefer saved matching body");
    check(catalog.choose(bow, 31).body == 1, "Otherwise choose lowest body ID");
    check(catalog.choose(silver_mace, 31).body == 7, "Silver uses ordinary associations");
    catalog.bodies[1].erase("type_44");
    check(catalog.choose(bow, 31).matched && !catalog.choose(shortbow, 31).matched,
          "Unchecking one association preserves the other");
    catalog.bodies[4].clear();
    const auto missing = catalog.choose(mace_shield, 31);
    check(!missing.matched && missing.body == 31 && missing.combination == "type_23_shield",
          "No opposite shield substitution; saved appearance fallback is explicit");
    check(!catalog.choose({}, 30).matched && catalog.choose({}, 30).body == 30,
          "Unmapped unarmed also retains appearance");

    struct TemporaryCatalog
    {
        std::filesystem::path path =
            std::filesystem::temp_directory_path() /
            ("opengold-catalog-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".tsv");

        ~TemporaryCatalog()
        {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    } temporary;

    const auto &path = temporary.path;
    {
        std::ofstream out(path, std::ios::binary);
        for (unsigned id = 0; id < 33; ++id)
            out << id << '\t'
                << (id == 1   ? "type_43,type_44"
                    : id == 4 ? "silver_23_shield,type_23_shield"
                    : id == 7 ? "silver_23"
                    : "unreviewed")
                << '\n';
    }
    const auto migrated = por::CombatBodyCatalog::load(path, folder / "combat-weapon-options.tsv");
    check(migrated.bodies[1].size() == 2 &&
          migrated.bodies[4] == std::set<std::string> {"type_23_shield"} &&
          migrated.bodies[7].contains("type_23"),
          "Load multiple, singleton and silver assignments without duplicates");
    {
        std::ofstream out(path, std::ios::app | std::ios::binary);
        out << "deleted\ttype_43\n";
    }
    const auto deleted = por::CombatBodyCatalog::load(path, folder / "combat-weapon-options.tsv");
    check(deleted.deleted.contains("type_43") && !deleted.bodies[1].contains("type_43"),
          "Deleted combination is removed from native associations");
    check(!deleted.choose(bow, 31).matched && deleted.choose(bow, 31).body == 31,
          "Deleted combination retains saved appearance");
    check(deleted.choose(shortbow, 31).matched && deleted.choose(shortbow, 31).body == 1,
          "Deleting one combination preserves other associations");
}

std::unique_ptr<RulesModule> module()
{
    return srd5::load(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "data/rules/srd-5.2.1/combat.rules");
}

Character character(std::string klass = "fighter", std::string name = "Ada")
{
    CharacterCreator creator(srd5::character_rules(), 42);
    creator.select(CreationField::race, "human");
    creator.select(CreationField::character_class, klass);
    creator.roll();
    creator.name(std::move(name));
    for (unsigned i = 0; i < 6; ++i)
        creator.assign_roll(i, i);
    // Authored fixtures may have scores below the creator's optional starting-class minimums.
    return Character(creator.rules(), creator.draft(), creator.appearance());
}

por::Equipment item(unsigned type, unsigned price = 10)
{
    por::Equipment e;
    e.stored.type = type;
    e.stored.value = price;
    e.stored.stack_size = 1;
    return e;
}

// An intentionally different equipment policy proves Core applies rules-owned
// choices, rather than retaining the SRD single-weapon replacement decision.
class AlternateEquipmentRules final : public test::ForwardingModule
{
  public:
    AlternateEquipmentRules() : ForwardingModule(module())
    {
    }

    Identity identity() const override
    {
        return {"equipment-test", "1", "owned-plans"};
    }

    EquipmentInfo equipment_info(std::string_view) const override
    {
        return {EquipmentSlot::weapon, 1};
    }

    CharacterProfile character_profile(const CharacterSheet &sheet,
                                       std::span<const std::string> gear) const override
    {
        CharacterProfile result;
        result.hit_points = sheet.hit_points;
        result.armor_class = 10 + int(gear.size());
        return result;
    }

    EquipmentChange equipment_change(const CharacterSheet &, std::span<const std::string> gear,
                                     unsigned selected,
                                     EquipmentOperation operation) const override
    {
        if (gear[selected] == "bad_index")
            return {{unsigned(gear.size())}};
        if (gear[selected] == "duplicate_index")
            return {{selected, selected}};
        if (gear[selected] == "rejected")
            throw std::runtime_error("Alternate rules reject this equipment");
        EquipmentChange result;
        for (auto n = static_cast<unsigned>(gear.size()); n > 0; --n)
            if (operation != EquipmentOperation::unequip || n - 1 != selected)
                result.indices.push_back(n - 1);
        return result;
    }
};

void equipment_rule_boundary()
{
    CampaignParty party(std::make_unique<AlternateEquipmentRules>());
    auto person = character();
    const auto sword = person.add_item({.definition_id = "longsword", .name = "Sword"});
    const auto dagger = person.add_item({.definition_id = "dagger", .name = "Dagger"});
    std::vector<std::uint64_t> invalid;
    for (const char *key :
            {"bad_index", "duplicate_index", "rejected"
            })
        invalid.push_back(person.add_item({.definition_id = key, .name = key}));
    const auto id = party.add_pc(std::move(person));
    const auto vitals = party.member(id).vitals;
    party.equip(id, sword);
    party.equip(id, dagger);
    check(party.member(id).equipped == std::vector<std::uint64_t> {dagger, sword},
          "Core applies the module's two-weapon order");
    check(party.profile(id).armor_class == 12,
          "Profile query receives the module-selected loadout");
    const auto before = party.member(id).equipped;
    for (auto item : invalid)
    {
        rejects(
            [&]
        {
            party.equip(id, item);
        });
        check(
            party.member(id).equipped == before && party.member(id).vitals == vitals &&
            party.member(id).character.inventory().items().size() == 5,
            "Invalid rules results and rejections preserve owned items, loadout and vitals");
    }
    party.unequip(id, sword);
    check(party.member(id).equipped == std::vector<std::uint64_t> {dagger},
          "Unequip uses the module's equipment continuation");
    party.equip(id, dagger);
    party.unequip(id, sword);
    check(party.member(id).equipped == std::vector<std::uint64_t> {dagger} &&
          party.member(id).vitals == vitals,
          "Existing equip/unequip no-ops preserve state");
}

void two_weapon_equipment()
{
    for (bool recruited :
            {
                false, true
            })
    {
        CampaignParty party(module());
        auto c = character("fighter", "Hands");
        const auto sword = c.add_item({.definition_id = "longsword", .name = "Longsword"}),
                   daggers = c.add_item({.definition_id = "dagger",
                                                .name = "Dagger",
                                                .quantity = 3,
                                                .original_type = 8});
        const auto shield = c.add_item({.definition_id = "shield", .name = "Shield"}),
                   great = c.add_item({.definition_id = "greatsword", .name = "Greatsword"});
        const auto id = recruited ? party.recruit("hands:npc", c) : party.add_pc(c);
        const auto vitals = party.member(id).vitals;
        auto sourced = party.checkpoint();
        auto provenance = item(8);
        provenance.stored.stack_size = 3;
        sourced.roster.front().item_sources.emplace(daggers, provenance);
        party.restore(std::move(sourced));
        party.equip(id, sword);
        const auto unchanged = encode_campaign(party, nullptr, "hands");
        const auto choices = party.equipment_choices(id, daggers);
        check(choices.size() == 2 && choices[0].available && choices[1].available,
              "Both hand choices are rules-provided");
        check(unchanged == encode_campaign(party, nullptr, "hands"),
              "Querying hand choices has no effects");
        party.equip(id, daggers, EquipmentOperation::equip_other);
        const auto held = party.member(id).equipped;
        const auto unit = held.back();
        check(held.size() == 2 && held[0] == sword && unit != daggers &&
              party.member(id).character.inventory().find(unit)->get().quantity == 1 &&
              party.member(id).character.inventory().find(daggers)->get().quantity == 2,
              "Second hand separates one actual stack unit");
        check(party.member(id).item_sources.contains(unit) &&
              party.member(id).item_sources.at(unit).stored.type == 8,
              "A split equipped unit retains the original item provenance");
        const auto profile = party.profile(id);
        check(profile.data.starts_with("PC42 ") && profile.weapon_hands == 1 &&
              profile.equipment_positions[0].source == "Main hand" &&
              profile.equipment_positions[1].source == "Other hand",
              "Dual weapons use one hand each and report both positions");
        rejects(
            [&]
        {
            party.equip(id, shield);
        });
        check(party.member(id).equipped == held && party.member(id).vitals == vitals,
              "Illegal shield preserves loadout and resources");
        const auto saved = encode_campaign(party, nullptr, "hands");
        CampaignParty loaded(module());
        loaded.restore(
            decode_campaign(saved, *srd5::character_rules(), *module(), "hands", nullptr).party);
        check(encode_campaign(loaded, nullptr, "hands") == saved,
              "PC/NPC dual-hand campaign save roundtrips exactly");
        party.equip(id, daggers, EquipmentOperation::equip_main);
        check(party.member(id).equipped[0] != unit && party.member(id).equipped[1] == unit &&
              party.member(id).character.inventory().find(daggers)->get().quantity == 1,
              "Identical weapons in separate hands remain distinct physical units");
        party.equip(id, great);
        const auto blocked = party.equipment_choices(id, daggers);
        check(blocked[0].available && !blocked[1].available,
              "Other hand is disabled for a two-handed main weapon");
        const auto before = encode_campaign(party, nullptr, "hands");
        rejects(
            [&]
        {
            party.equip(id, daggers, EquipmentOperation::equip_other);
        });
        check(encode_campaign(party, nullptr, "hands") == before,
              "Failed hand operation does not split or consume inventory");
        party.equip(id, daggers);
        party.equip(id, shield);
        const auto another = party.member(id).character.inventory().items().front().id;
        check(!party.equipment_choices(id, another)[1].available,
              "Shield blocks the additional weapon");
    }
}

void party_combat_appearance()
{
    const auto folder = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/art";
    auto catalog = por::CombatBodyCatalog::load(folder / "combat-body-looks.tsv",
        folder / "combat-weapon-options.tsv");
    // Exercise equipment resolution against authored assignments, independently
    // of ongoing art review and intentionally unassigned production combinations.
    for (auto &body : catalog.bodies)
        body.clear();
    catalog.deleted.clear();
    catalog.bodies[0] = {"type_0"};
    catalog.bodies[2] = {"type_36"};
    catalog.bodies[33] = {"type_8"};
    catalog.bodies[20] = {"type_36_shield"};
    catalog.bodies[34] = {"type_8_shield"};
    catalog.bodies[24] = {"type_36_shield"};
    catalog.bodies[32] = {"type_0_shield"};
    CampaignParty party(module());
    auto pc = character(), npc = character("fighter", "Guard");
    auto appearance = pc.appearance();
    appearance.combat_body = 24;
    appearance.combat_head = 2;
    appearance.colors[0][0] = 3;
    appearance.portrait = "human-male-fighter-01.png";
    pc.appearance(appearance);
    appearance.tall = false;
    npc.appearance(appearance);
    const auto pc_id = party.add_pc(pc), npc_id = party.recruit("test:guard", npc);
    for (const auto id :
            {
                pc_id, npc_id
            })
    {
        const auto original = party.member(id).character.appearance();
        const auto expect = [&](unsigned body, std::string_view key)
        {
            const auto &member = party.member(id);
            const auto resolved = por::resolve_combat_appearance(member, catalog);
            check(resolved.appearance == original && resolved.selection.body == body &&
                  resolved.selection.matched && resolved.selection.combination == key,
                  "PC and recruited NPC resolve only their equipped weapon and shield");
            check(member.character.appearance() == original,
                  "Resolution never overwrites saved appearance");
        };
        for (const auto type :
                {
                    36u, 8u, 59u, 55u
                })
            party.purchase(id, item(type, 0));
        const auto gear = party.member(id).character.inventory().items();
        expect(0, "type_0"); // Carried weapons and shield have no visual effect.
        party.equip(id, gear[3].id);
        expect(0, "type_0");
        party.equip(id, gear[0].id);
        expect(2, "type_36");
        party.equip(id, gear[2].id);
        expect(24, "type_36_shield"); // Saved matching body wins over 20.
        party.unequip(id, gear[3].id);
        expect(24, "type_36_shield");
        party.unequip(id, gear[0].id);
        expect(32, "type_0_shield");
        party.equip(id, gear[1].id);
        expect(34, "type_8_shield");
        party.unequip(id, gear[2].id);
        expect(33, "type_8");
        party.unequip(id, gear[1].id);
        expect(0, "type_0");
        party.equip(id, gear[0].id);
        party.equip(id, gear[2].id);
        auto missing = catalog;
        for (auto &body : missing.bodies)
            body.erase("type_36_shield");
        const auto fallback = por::resolve_combat_appearance(party.member(id), missing);
        check(!fallback.selection.matched && fallback.appearance == original &&
              fallback.selection.label == "Long Sword & Shield",
              "An unmapped combination preserves the complete saved appearance and diagnostic");
        auto deleted = catalog;
        deleted.deleted.insert("type_36_shield");
        check(!por::resolve_combat_appearance(party.member(id), deleted).selection.matched,
              "Deleted combinations cannot select assigned artwork");
        auto invalid = party.member(id);
        invalid.equipped.push_back(999999);
        rejects(
            [&]
        {
            (void)por::resolve_combat_appearance(invalid, catalog);
        });
    }
    const auto saved = encode_campaign(party, nullptr, "equipment-art-fixture");
    const auto decoded = decode_campaign(saved, *srd5::character_rules(), *module(),
                                         "equipment-art-fixture", nullptr);
    CampaignParty restored(module());
    restored.restore(decoded.party);
    for (const auto id :
            {
                pc_id, npc_id
            })
    {
        const auto before = por::resolve_combat_appearance(party.member(id), catalog);
        const auto after = por::resolve_combat_appearance(restored.member(id), catalog);
        check(after.appearance == before.appearance &&
              after.selection.combination == before.selection.combination,
              "Campaign save/load reconstructs identical PC and NPC equipment artwork");
        check(restored.member(id).character.appearance() == party.member(id).character.appearance(),
              "Saved base appearance survives the campaign codec");
    }
    // Native definitions without original provenance still use the same mapping.
    auto native = party.member(pc_id);
    native.equipped.clear();
    native.equipped.push_back(native.character.add_item({.definition_id = "mace",
                                                                .name = "Authored mace",
                                                                .quantity = 1}));
    check(por::resolve_combat_appearance(native, catalog).selection.combination == "type_23",
          "Native weapon definition resolves without original item type");
}

void all_weapon_equipment()
{
    const auto folder = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/art";
    const auto catalog = por::CombatBodyCatalog::load(folder / "combat-body-looks.tsv",
        folder / "combat-weapon-options.tsv");
    auto hero = character();
    for (const auto &option : catalog.options)
        if (option.original_type)
            hero.add_item({.definition_id = equipment_conversion(item(option.original_type)),
                                  .name = option.label,
                                  .quantity = 1,
                                  .original_type = option.original_type});
    const auto shield = hero.add_item({.definition_id = "shield",
                                              .name = "Shield",
                                              .quantity = 1,
                                              .original_type = 59});
    CampaignParty party(module());
    const auto id = party.add_pc(std::move(hero));
    const auto inventory = party.member(id).character.inventory().items();
    check(inventory.size() == 48,
          "Every reviewer weapon plus shield uses a real campaign inventory");
    const auto original = party.member(id).character.appearance();
    for (const auto &weapon : inventory)
        if (weapon.id != shield)
        {
            party.equip(id, weapon.id);
            check(party.member(id).equipped == std::vector<std::uint64_t> {weapon.id},
                  "Every weapon replaces the previous weapon through CampaignParty");
            const auto info = party.equipment_info(id, weapon.id);
            check(info.slot == EquipmentSlot::weapon && info.hands >= 1 && info.hands <= 2,
                  "Every reviewer weapon has shared rules metadata");
            if (weapon.original_type == 1 || weapon.original_type == 31 ||
                    weapon.original_type == 6 || weapon.original_type == 22 ||
                    weapon.original_type == 33 || weapon.original_type == 39)
                check(info.hands == 1, "Versatile conversions permit one hand and a shield");
            check(party.profile(id).hit_points > 0,
                  "Every weapon produces an actual rules profile");
            check(por::resolve_combat_appearance(party.member(id), catalog).appearance == original,
                  "Every weapon retains saved anatomy");
            if (info.hands == 2)
            {
                rejects(
                    [&]
                {
                    party.equip(id, shield);
                });
                check(party.member(id).equipped == std::vector<std::uint64_t> {weapon.id},
                      "Rejected shield is atomic");
                party.unequip(id, weapon.id);
                party.equip(id, shield);
                rejects(
                    [&]
                {
                    party.equip(id, weapon.id);
                });
                check(party.member(id).equipped == std::vector<std::uint64_t> {shield},
                      "Rejected two-handed weapon preserves the shield");
                party.unequip(id, shield);
                party.equip(id, weapon.id);
            }
            else
            {
                party.equip(id, shield);
                const auto equipped = party.member(id).equipped;
                const auto two = std::find_if(inventory.begin(), inventory.end(),
                                              [&](const auto & i)
                {
                    return party.equipment_info(id, i.id).hands == 2;
                });
                rejects(
                    [&]
                {
                    party.equip(id, two->id);
                });
                check(party.member(id).equipped == equipped,
                      "Rejected two-handed replacement preserves weapon and shield");
                party.unequip(id, shield);
            }
            auto participants = party.participants();
            participants[0].cell = {1, 1};
            participants.push_back({1000, "bandit", "Target", 1, {3, 1}});
            auto rules = module();
            std::unique_ptr<CombatSession> combat;
            for (unsigned seed = 0; seed < 100; ++seed)
            {
                auto attempt =
                rules->create({{8, 5, std::vector<Terrain>(40)}, participants}, seed);
                if (attempt->snapshot().actor == id)
                {
                    combat = std::move(attempt);
                    break;
                }
            }
            check(bool(combat), "Find player initiative for each equipped weapon");
            const auto commands = combat->legal_commands();
            const auto type = weapon.original_type;
            const bool ranged = type == 2 || type == 8 || type == 9 || type == 21 || type == 31 ||
                                type == 39 || (type >= 41 && type <= 47);
            check(std::any_of(commands.begin(), commands.end(),
                              [](const auto & c)
            {
                return c.verb == "ranged";
            }) == ranged,
            "Thrown and ranged weapons offer real ranged attacks");
            // MELEE-1: as in the original game, melee reaches adjacent squares
            // only, so polearms cannot strike the enemy two squares away.
            check(std::none_of(commands.begin(), commands.end(),
                               [](const auto & c)
            {
                return c.verb == "melee";
            }),
            "Polearms strike adjacent squares only");
            check(rules->restore(combat->save())->save() == combat->save(),
                  "Every equipped weapon survives a combat checkpoint");
            const auto saved = encode_campaign(party, nullptr, "all-weapons");
            const auto decoded =
                decode_campaign(saved, *srd5::character_rules(), *module(), "all-weapons", nullptr);
            check(decoded.party.roster[0].equipped == party.member(id).equipped,
                  "Every equipped weapon survives campaign save/load");
        }
}

void goliath_occupancy()
{
    const auto human = character();
    auto draft = human.creation_data();
    draft.race = "goliath";
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*srd5::character_rules(), draft, human.appearance()));
    auto participants = party.participants();
    participants[0].cell = {2, 2};
    participants.push_back({1000, "bandit", "Above the Goliath", 1, {2, 0}});
    auto rules = module();
    std::unique_ptr<CombatSession> combat;
    for (unsigned seed = 0; seed < 100; ++seed)
    {
        auto candidate = rules->create({{6, 6, std::vector<Terrain>(36)}, participants}, seed);
        if (candidate->snapshot().actor == 1000)
        {
            combat = std::move(candidate);
            break;
        }
    }
    check(bool(combat), "Find deterministic monster turn");
    const auto moves = combat->legal_commands();
    const auto above = std::find_if(moves.begin(), moves.end(),
                                    [](const auto & c)
    {
        return c.verb == "move" && c.destination == Cell{2, 1};
    });
    check(above != moves.end(), "Monster can enter the square above a Goliath");
    check(std::none_of(moves.begin(), moves.end(),
                       [](const auto & c)
    {
        return c.verb == "move" && c.destination == Cell{2, 2};
    }),
    "Only the Goliath's lower square is occupied");
    check(combat->submit(*above), "Move into the Goliath's visual overhang");
    const auto snapshot = rules->restore(combat->save())->snapshot();
    for (const auto &actor : snapshot.combatants)
    {
        if (actor.id == id)
            check(actor.cell == Cell{2, 2}, "Goliath remains in the lower square");
        if (actor.id == 1000)
            check(actor.cell == Cell{2, 1},
                  "Monster independently occupies the upper square after save/restore");
    }
}

void roster_and_equipment()
{
    CampaignParty party(module());
    auto original = character();
    const auto pc = party.add_pc(original);
    original.add_item({.definition_id = "other", .name = "External item"});
    check(party.member(pc).character.inventory().empty(), "Party owns character independently");
    for (unsigned i = 1; i < 6; ++i)
        party.add_pc(character());
    rejects(
        [&]
    {
        party.add_pc(character());
    });
    const auto npc = party.recruit("MON:explicit-profile", character());
    party.recruit("MON:second-profile", character());
    rejects(
        [&]
    {
        party.recruit("MON:third-profile", character());
    });
    party.set_wealth(npc, {0, 0, 0, 123, 0, 0, 0});
    party.remove(npc);
    party.recruit("MON:explicit-profile", character());
    check(party.member(npc).wealth[3] == 123 && party.state().roster.size() == 8,
          "Re-recruit preserves original NPC instance");
    party.remove(pc);
    party.rejoin(pc);
    check(party.state().slots[0] == pc, "Rejoin restores PC position");
    party.set_wealth(pc, {0, 0, 0, 100, 0, 0, 0});
    party.purchase(pc, item(36));
    party.purchase(pc, item(59));
    const auto items = party.member(pc).character.inventory().items();
    const auto sword = items[0].id, shield = items[1].id;
    const auto ac = party.profile(pc).armor_class;
    party.equip(pc, sword);
    party.equip(pc, shield);
    check(party.profile(pc).armor_class == ac + 2 && party.member(pc).wealth[3] == 80,
          "Purchased shield changes actual rules AC");
    check(por::party_has_item(party, 59) && !por::party_has_item(party, 55), "Party item query uses original types");
    party.purchase(pc, item(55));
    party.equip(pc, party.member(pc).character.inventory().items()[2].id);
    check(party.profile(pc).armor_class == 18, "Armor and shield combine in rules module");
    check(party.profile(pc).item_modifiers.find("Shield: +2 AC") != std::string::npos &&
          party.profile(pc).item_modifiers.find("Chain mail") != std::string::npos,
          "Modifier report includes every equipped effect");
    rejects(
        [&]
    {
        party.purchase(pc, item(50, 500));
    });
    check(party.member(pc).wealth[3] == 70, "Unaffordable buy is atomic");
    party.purchase(pc, item(49));
    rejects(
        [&]
    {
        party.equip(pc, party.member(pc).character.inventory().items().back().id);
    });
    check(party.member(pc).equipped.size() == 3, "Unsupported item does not change equipment");
    party.unequip(pc, shield);
    check(party.profile(pc).armor_class == 16, "Unequipping updates AC");
    party.begin_combat();
    rejects(
        [&]
    {
        party.remove(pc);
    });
    rejects(
        [&]
    {
        party.purchase(pc, item(8));
    });
    party.end_combat();
    CampaignParty wizard(module());
    auto mage = wizard.add_pc(character("wizard"));
    wizard.set_wealth(mage, {0, 0, 0, 50, 0, 0, 0});
    wizard.purchase(mage, item(59));
    const int naked_ac = wizard.profile(mage).armor_class;
    wizard.equip(mage, 1);
    check(wizard.profile(mage).armor_class == naked_ac,
          "Untrained shield is allowed but adds no AC");
    auto bard = wizard.add_pc(character("bard"));
    check(wizard.profile(bard).armor_class > 0,
          "All classes can display an equipment profile outside combat");
}

// The leader speaks for the party: the first member until another is made
// leader, the next conscious member while the leader is down, and saved.
void party_leader()
{
    CampaignParty party(module());
    const auto first = party.add_pc(character("fighter", "Ada"));
    const auto second = party.add_pc(character("fighter", "Bea"));
    check(party.leader() == first && party.spokesman() == first, "The first member leads at first");
    party.make_leader(second);
    check(party.leader() == second && party.spokesman() == second, "Another member can be made leader");
    auto down = party.checkpoint();
    for (auto &member : down.roster)
        if (member.id == second)
            member.vitals.hit_points = 0;
    party.restore(down);
    check(party.leader() == second && party.spokesman() == first,
          "The next conscious member speaks while the leader is down");
    const auto saved = encode_campaign(party, nullptr, "leader");
    CampaignParty loaded(module());
    loaded.restore(
        decode_campaign(saved, *srd5::character_rules(), *module(), "leader", nullptr).party);
    check(loaded.leader() == second, "The leader is saved");
    party.remove(second);
    check(party.leader() == first && encode_campaign(party, nullptr, "leader") != saved,
          "A leader who leaves the party hands the lead back to the first member");
    rejects([&] { party.make_leader(second); });
    // Losing someone outside the party is refused and changes nothing.
    const auto before = encode_campaign(party, nullptr, "leader");
    rejects([&] { party.lose(second); });
    check(!party.member(second).vitals.dead &&
          encode_campaign(party, nullptr, "leader") == before,
          "A refused loss leaves the member alive and the party unchanged");
}

void class_weapon_proficiency()
{
    auto rules = module();
    const auto make_character = [](std::string klass)
    {
        CharacterDraft draft;
        draft.race = "human";
        draft.gender = "female";
        draft.character_class = klass;
        draft.alignment = "neutral_good";
        draft.background = "soldier";
        draft.name = klass;
        draft.rolled = true;
        for (auto &roll : draft.rolls)
            roll = {{6, 5, 4, 1}, 3};
        draft.rolls[0] = {{4, 4, 4, 1}, 3}; // Soldier produces STR 14, DEX 16.
        return Character(*srd5::character_rules(), draft, {});
    };

    // Independent expectations from SRD 5.2.1's starting-class traits.
    struct Training
    {
        const char *klass;
        bool light_martial, other_martial;
    };

    const std::array expectations
    {
        Training{"barbarian", true, true}, Training{"bard", false, false},
        Training{"cleric", false, false},  Training{"druid", false, false},
        Training{"fighter", true, true},   Training{"monk", true, false},
        Training{"paladin", true, true},   Training{"ranger", true, true},
        Training{"rogue", true, false},    Training{"sorcerer", false, false},
        Training{"warlock", false, false}, Training{"wizard", false, false}};
    for (const auto &expected : expectations)
    {
        const auto pc = make_character(expected.klass);
        check(pc.sheet().scores[0] == 14 && pc.sheet().scores[1] == 16,
              "Weapon fixture has independent Strength and Dexterity modifiers");
        for (const std::string weapon :
                {"shortsword", "scimitar"
                })
        {
            const std::array gear{weapon};
            const auto profile = rules->character_profile(pc.sheet(), gear);
            check(profile.melee_attack_bonus == (expected.light_martial ? 5 : 3),
                  "Light Finesse martial weapons apply the starting class proficiency");
            check(profile.item_modifiers.find(expected.light_martial
                                              ? "+2 class proficiency"
                                              : "without proficiency") != std::string::npos,
                  "Equipment explanation agrees with the attack bonus");
            check(
                srd5::equipment_note(pc.sheet(), weapon)
                .starts_with(expected.light_martial ? "Class training:" : "Untrained weapon:"),
                "Equipment notes agree with class weapon training");
        }
        const std::array<std::string, 1> longsword{"longsword"}, mace{"mace"};
        check(rules->character_profile(pc.sheet(), longsword).melee_attack_bonus ==
              (expected.other_martial ? 4 : 2),
              "Rogue and Monk do not gain all martial weapons");
        // Martial Arts: a Monk swings the mace, a Monk weapon, with Dexterity.
        check(rules->character_profile(pc.sheet(), mace).melee_attack_bonus ==
              (std::string_view{expected.klass} == "monk" ? 5 : 4),
              "Every starting class retains simple weapon proficiency");
    }
    for (const std::string klass :
            {"rogue", "monk"
            })
        for (const std::string weapon :
                {"shortsword", "scimitar"
                })
        {
            auto pc = make_character(klass);
            const auto item_id = pc.add_item({.definition_id = weapon, .name = weapon});
            CampaignParty party(module());
            const auto id = party.add_pc(std::move(pc));
            party.equip(id, item_id);
            auto wounded = party.checkpoint();
            wounded.roster[0].vitals.hit_points = 3;
            party.restore(std::move(wounded));
            const auto saved = encode_campaign(party, nullptr, "weapon-proficiency");
            auto decoded = decode_campaign(saved, *srd5::character_rules(), *rules,
                                           "weapon-proficiency", nullptr);
            CampaignParty loaded(module());
            loaded.restore(std::move(decoded.party));
            check(loaded.profile(id).melee_attack_bonus == 5,
                  "Campaign reload recomputes the correct Rogue/Monk attack bonus");
            check(loaded.member(id).vitals == party.member(id).vitals &&
                  loaded.member(id).equipped == party.member(id).equipped,
                  "Campaign reload preserves wounds, resources and equipment");
            check(encode_campaign(loaded, nullptr, "weapon-proficiency") == saved,
                  "Proficient equipment round trips canonically");
            auto participants = loaded.participants();
            participants[0].cell = {1, 1};
            participants.push_back({1000, "bandit", "Target", 1, {2, 1}});
            std::unique_ptr<CombatSession> combat;
            for (unsigned seed = 0; seed < 100; ++seed)
            {
                auto attempt =
                rules->create({{4, 4, std::vector<Terrain>(16)}, participants}, seed);
                if (attempt->snapshot().actor == id)
                {
                    combat = std::move(attempt);
                    break;
                }
            }
            check(bool(combat), "Find player initiative for proficiency regression");
            auto restored = rules->restore(combat->save());
            const auto commands = combat->legal_commands();
            const auto attack = std::find_if(commands.begin(), commands.end(),
                                             [](const auto & c)
            {
                return c.verb == "melee";
            });
            check(attack != commands.end() && combat->submit(*attack) && restored->submit(*attack),
                  "Original and restored actors can attack");
            check(combat->save() == restored->save(),
                  "Proficient attacks resume deterministically from checkpoints");
            const auto messages = combat->snapshot().log_messages();
            check(std::any_of(messages.begin(), messages.end(),
                              [](const auto & message)
            {
                return message.source.starts_with("{actor} -> {target}: d20") &&
                       std::any_of(
                           message.arguments.begin(), message.arguments.end(),
                           [](const auto & arg)
                {
                    return arg.name == "bonus" && arg.value == "5";
                });
            }),
            "Actual combat uses +5 for the Dexterity-16 Rogue/Monk weapon attack");
        }
}

void stabilization_handoff()
{
    CampaignParty party(module());
    const auto hero = party.add_pc(character());
    party.add_pc(character("fighter", "Conscious ally"));
    auto state = party.checkpoint();
    state.roster[0].vitals = {0, false, "SRD11 1 0 0 2 1 0 1 6000 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0"};
    party.restore(std::move(state));
    auto participants = party.participants();
    participants[0].cell = {1, 1};
    participants[1].cell = {1, 3};
    participants.push_back({1000, "bandit", "Enemy", 1, {5, 3}});
    auto rules = module();
    std::unique_ptr<CombatSession> stable;
    for (unsigned seed = 0; seed < 200 && !stable; ++seed)
    {
        auto candidate = rules->create({{8, 5, std::vector<Terrain>(40)}, participants}, seed);
        for (unsigned turns = 0; turns < 4; ++turns)
        {
            const auto snapshot = candidate->snapshot();
            const auto actor = std::find_if(snapshot.combatants.begin(), snapshot.combatants.end(),
                                            [&](const auto & a)
            {
                return a.id == hero;
            });
            check(actor != snapshot.combatants.end(), "Campaign actor remains in combat");
            if (actor->hit_points > 0 || actor->dead)
                break;
            if (actor->persistent.resources.starts_with("SRD11 1 0 0 0 0 1 1 0 "))
            {
                stable = std::move(candidate);
                break;
            }
            const auto commands = candidate->legal_commands();
            const auto end = std::find_if(commands.begin(), commands.end(),
                                          [](const auto & c)
            {
                return c.verb == "end";
            });
            check(end != commands.end() && candidate->submit(*end),
                  "Advance a conscious actor while waiting for stabilization");
        }
    }
    check(bool(stable), "Third death-save success stabilizes the campaign character");
    party.begin_combat();
    party.apply_combat(stable->snapshot());
    party.end_combat();
    check(party.member(hero).vitals.resources.starts_with("SRD11 1 0 0 0 0 1 1 0 "),
          "Combat handoff preserves Stable with zero counters and one spent Second Wind");
    const auto saved = encode_campaign(party, nullptr, "stabilization");
    auto loaded =
        decode_campaign(saved, *srd5::character_rules(), *rules, "stabilization", nullptr);
    CampaignParty restored(module());
    restored.restore(std::move(loaded.party));
    check(restored.member(hero).vitals == party.member(hero).vitals,
          "Stable state and cleared counters survive campaign save/reload");
}

void remaining_turn_handoff()
{
    CampaignParty party(module());
    const auto hero = party.add_pc(character());
    auto state = party.checkpoint();
    state.roster[0].vitals = {3, false, "SRD11 1 0 0 0 0 0 1 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0"};
    party.restore(std::move(state));
    auto participants = party.participants();
    participants[0].cell = {1, 1};
    participants.push_back({1000, "vanguard", "Enemy", 1, {2, 1}});
    auto rules = module();
    std::unique_ptr<CombatSession> combat;
    for (unsigned seed = 0; seed < 100 && !combat; ++seed)
    {
        auto candidate = rules->create({{8, 5, std::vector<Terrain>(40)}, participants}, seed);
        if (candidate->snapshot().actor == hero)
            combat = std::move(candidate);
    }
    check(bool(combat), "Find a campaign character's initial turn");
    party.begin_combat();
    party.apply_combat(combat->snapshot());
    const auto use = [&](std::string_view verb)
    {
        const auto commands = combat->legal_commands();
        const auto found = std::find_if(commands.begin(), commands.end(),
                                        [&](const auto & c)
        {
            return c.verb == verb;
        });
        check(found != commands.end() && combat->submit(*found),
              "Use remaining campaign turn resource");
        party.apply_combat(combat->snapshot());
    };
    use("melee");
    check(combat->snapshot().actor == hero && party.state().time_minutes == 0 &&
          party.state().subminute_milliseconds == 0,
          "An attack keeps the campaign turn and clock at the same initiative slot");
    use("second_wind");
    check(party.member(hero).vitals.hit_points > 3 &&
          party.member(hero).vitals.resources ==
          "SRD11 0 0 0 0 0 0 1 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0",
          "Post-attack Second Wind updates campaign HP and spends its last use");
    use("end");
    check(party.state().subminute_milliseconds == 3000,
          "Explicit End Turn advances the campaign clock once");
    for (unsigned commands = 0; commands < 300 && combat->snapshot().outcome == Outcome::ongoing;
            ++commands)
    {
        check(combat->submit(choose_demo_command(*combat)),
              "Complete campaign combat with explicit AI turn endings");
        party.apply_combat(combat->snapshot());
    }
    check(combat->snapshot().outcome != Outcome::ongoing,
          "Campaign combat finishes after attacks retain remaining turns");
    party.end_combat();
    const auto saved = encode_campaign(party, nullptr, "remaining-turn");
    auto loaded =
        decode_campaign(saved, *srd5::character_rules(), *rules, "remaining-turn", nullptr);
    CampaignParty restored(module());
    restored.restore(std::move(loaded.party));
    check(encode_campaign(restored, nullptr, "remaining-turn") == saved,
          "Campaign reload preserves post-combat HP, spent recovery and exact clock");
}

void untrained_equipment()
{
    auto rules = module();
    const auto mage = character("wizard");
    const auto &s = mage.sheet();
    const std::array<std::string, 1> sword{"longsword"}, mace{"mace"}, armor{"leather"},
    shield{"shield"};
    check(rules->character_profile(s, sword).melee_attack_bonus == s.modifiers[0],
          "Untrained longsword omits proficiency");
    check(rules->character_profile(s, mace).melee_attack_bonus == s.modifiers[0] + 2,
          "SRD wizard is proficient in simple weapons including mace");
    const auto p = rules->character_profile(s, armor);
    check(p.strength_dexterity_disadvantage && p.armor_class == 11 + s.modifiers[1],
          "Untrained armor keeps AC with disadvantage");
    check(p.item_modifiers.find("cannot cast spells") != std::string::npos,
          "Untrained penalty has an equipment source");
    check(!rules->character_profile(s, {}).strength_dexterity_disadvantage,
          "Removing armor clears disadvantage");
    for (const auto &c : srd5::character_rules()->choices(CreationField::character_class))
    {
        const auto pc = character(c.id);
        const bool no_light = c.id == "monk" || c.id == "sorcerer" || c.id == "wizard";
        check(rules->character_profile(pc.sheet(), armor).strength_dexterity_disadvantage ==
              no_light,
              "Light armor training matches all SRD classes");
        const bool shield_training = c.id == "barbarian" || c.id == "cleric" || c.id == "druid" ||
                                     c.id == "fighter" || c.id == "paladin" || c.id == "ranger";
        const auto guarded = rules->character_profile(pc.sheet(), shield);
        const int base = 10 + pc.sheet().modifiers[1] +
                         (c.id == "barbarian" ? std::max(0, pc.sheet().modifiers[2]) : 0);
        check(guarded.armor_class == base + (shield_training ? 2 : 0),
              "Shield training and Monk unarmored restriction match SRD");
    }
    Encounter e
    {
        {4, 4, std::vector<Terrain>(16)},
        {{1, "campaign-character", "Mage", 0, {1, 1}, p.data}, {2, "bandit", "Bandit", 1, {2, 1}}}};
    bool tested = false;
    for (unsigned seed = 0; seed < 100 && !tested; ++seed)
    {
        auto combat = rules->create(e, seed);
        if (combat->snapshot().actor != 1)
            continue;
        const auto commands = combat->legal_commands();
        check(std::none_of(commands.begin(), commands.end(),
                           [](const auto & c)
        {
            return c.verb == "fire_bolt" || c.verb == "magic_missile";
        }),
        "Untrained armor prevents spellcasting");
        const auto hit = std::find_if(commands.begin(), commands.end(),
                                      [](const auto & c)
        {
            return c.verb == "melee";
        });
        check(hit != commands.end() && combat->submit(*hit), "Untrained armored attack is allowed");
        const auto log = combat->snapshot().log();
        check(std::any_of(log.begin(), log.end(),
                          [](const auto & line)
        {
            return line.find("disadvantage") != std::string::npos;
        }),
        "Armor penalty applies to actual attack rolls");
        auto restored = rules->restore(combat->save());
        check(restored->save() == combat->save(),
              "Untrained equipment penalties survive combat restore");
        tested = true;
    }
    check(tested, "Exercised armored wizard combat");
}

void finish(CombatDemo &fight)
{
    for (unsigned n = 0; n < 2000 && fight.combat().snapshot().outcome == Outcome::ongoing; ++n)
        check(fight.submit(choose_demo_command(fight.combat())), "Accepted combat command");
    check(fight.combat().snapshot().outcome != Outcome::ongoing, "Fight terminates");
}

void combat_handoff()
{
    auto party = std::make_shared<CampaignParty>(module());
    auto pc = party->add_pc(character("wizard", "Mage"));
    const auto guard = party->recruit("guard", character());
    party->set_wealth(pc, {0, 0, 0, 50, 0, 0, 0});
    party->purchase(pc, item(8));
    party->equip(pc, 1);
    auto start = party->checkpoint();
    start.roster[0].vitals.hit_points -= 2;
    party->restore(start);
    const auto hp = party->member(pc).vitals.hit_points;
    CombatDemo fight(module());
    fight.campaign_party(party);
    fight.training();
    const auto before = fight.combat().snapshot();
    const auto mage = std::find_if(before.combatants.begin(), before.combatants.end(),
                                   [&](const auto & a)
    {
        return a.id == pc;
    });
    check(mage != before.combatants.end() && mage->hit_points == hp && mage->name == "Mage",
          "Combat starts with created identity and live HP");
    const auto initial = party->member(pc).vitals.resources;
    finish(fight);
    check(!party->in_combat(), "Combat releases party edits on finish");
    for (const auto &actor : fight.combat().snapshot().combatants)
        if (actor.side == 0)
        {
            const auto &member = party->member(actor.id);
            const auto growth = member.character.sheet().hit_points - actor.max_hit_points;
            check(
                member.vitals.hit_points ==
                actor.hit_points + (actor.hit_points > 0 ? growth : 0) &&
                member.vitals.dead == actor.dead,
                "Victory applies combat HP followed by rules advancement without reviving anyone");
        }
    const auto finished = fight.combat().snapshot();
    check(std::find_if(
              finished.combatants.begin(), finished.combatants.end(),
              [&](const auto & a)
    {
        return a.id == pc;
    })->persistent.resources != initial,
    "Mage spent spell resources before advancement");
    check(party->member(pc).character.inventory().items().size() == 1 &&
          party->member(pc).equipped.size() == 1,
          "Inventory survives combat");
    const auto guard_state = party->member(guard).vitals;
    party->remove(guard);
    party->rejoin(guard);
    check(party->member(guard).vitals == guard_state,
          "Recruitment preserves combat HP and resources");
    if (!party->member(pc).vitals.dead && party->member(pc).vitals.hit_points > 0)
    {
        const auto retained = party->member(pc).vitals;
        fight.training(80);
        check(party->member(pc).vitals == retained, "Next encounter does not refill resources");
        finish(fight);
    }
}

struct EncounterObservation
{
    Encounter encounter;
    std::uint64_t seed{};
};

// Observe the adapter boundary while retaining real rules validation and combat.
class ObservedModule final : public test::ForwardingModule
{
  public:
    explicit ObservedModule(std::shared_ptr<EncounterObservation> observation)
        : ForwardingModule(module()), observation_(std::move(observation))
    {
    }

    std::unique_ptr<CombatSession> create(Encounter encounter, std::uint64_t seed) const override
    {
        observation_->encounter = encounter;
        observation_->seed = seed;
        return ForwardingModule::create(std::move(encounter), seed);
    }

  private:
    std::shared_ptr<EncounterObservation> observation_;
};

CampaignEncounter encounter_fixture()
{
    CampaignEncounter encounter;
    encounter.field.geometry = {40, 26, std::vector<Terrain>(40 * 26)};
    for (int y = 0; y < 26; ++y)
        encounter.field.geometry.terrain[y * 40 + 10] = Terrain::obstacle;
    encounter.field.geometry.terrain[13 * 40 + 25] = Terrain::difficult;
    encounter.field.tiles.resize(40 * 26, 7);
    encounter.enemies = {{1000, "bandit", "First enemy", 1, {}},
        {1001, "bandit", "Second enemy", 1, {}}
    };
    return encounter;
}

void campaign_encounters()
{
    auto party = std::make_shared<CampaignParty>(module());
    const auto pc = party->add_pc(character("wizard", "Wounded mage"));
    party->add_pc(character("fighter", "Companion"));
    auto checkpoint = party->checkpoint();
    checkpoint.roster[0].vitals = {
        1, false, "SRD11 0 0 0 0 0 0 1 0 0 0 \"\" 0 0 1 0 0 0 FX8 1 0 0"};
    party->restore(checkpoint);
    const auto wounded = party->member(pc).vitals;
    const auto still_wounded = [&](const VitalState & state)
    {
        return state.hit_points == wounded.hit_points && state.dead == wounded.dead &&
               state.resources == wounded.resources;
    };
    const std::array<Cell, 4> enemy_origins{{{20, 8}, {31, 13}, {30, 18}, {19, 13}}};
    for (unsigned facing = 0; facing < 4; ++facing)
    {
        auto observed = std::make_shared<EncounterObservation>();
        {
            CombatDemo fight(std::make_unique<ObservedModule>(observed));
            fight.campaign_party(party);
            auto encounter = encounter_fixture();
            encounter.facing = por::MapDirection{facing};
            encounter.surprise = facing;
            fight.encounter(encounter, 1234);
            check(observed->seed == 1234, "Encounter forwards deterministic seed");
            const auto &handed = observed->encounter;
            check(handed.battlefield.terrain == encounter.field.geometry.terrain &&
                  fight.battlefield_tiles() == encounter.field.tiles,
                  "Placement preserves original walls, difficult terrain and tiles");
            check(handed.participants.size() == 4,
                  "Every party member and enemy reaches the rules module");
            std::set<Cell> positions;
            for (const auto &participant : handed.participants)
            {
                check(participant.cell.x > 10 &&
                      handed.battlefield.at(participant.cell) != Terrain::obstacle,
                      "All participants remain in the party's reachable component");
                check(positions.insert(participant.cell).second,
                      "Combatants occupy distinct cells");
                check(participant.surprised == (participant.side == 0 ? facing == 1 : facing == 2),
                      "Original surprise codes select the correct side");
            }
            check(handed.participants[0].cell == Cell{25, 13} &&
                  handed.participants[2].cell == enemy_origins[facing],
                  "Party origin and enemy formation follow encounter facing");
            check(handed.participants[0].state && still_wounded(*handed.participants[0].state) &&
                  still_wounded(party->member(pc).vitals),
                  "Encounter setup preserves wounds and spent resources");
            check(party->in_combat(), "Successful encounter owns the party edit lock");
            rejects(
                [&]
            {
                fight.encounter(encounter, 1234);
            });
            rejects(
                [&]
            {
                (void)fight.save_combat();
            });
            rejects(
                [&]
            {
                fight.restore_combat({});
            });
        }
        check(!party->in_combat(), "Encounter teardown releases the party");
    }
    CombatDemo fight(module());
    fight.campaign_party(party);
    const auto rejected = [&](CampaignEncounter encounter)
    {
        rejects(
            [&]
        {
            fight.encounter(std::move(encounter), 1234);
        });
        check(!fight.has_combat() && !party->in_combat() && still_wounded(party->member(pc).vitals),
              "Failed encounter setup leaves party and combat ownership unchanged");
    };
    auto invalid = encounter_fixture();
    invalid.surprise = 4;
    rejected(invalid);
    invalid = encounter_fixture();
    invalid.field.geometry.terrain.pop_back();
    rejected(invalid);
    invalid = encounter_fixture();
    invalid.field.geometry.width = 65;
    rejected(invalid);
    invalid = encounter_fixture();
    std::fill(invalid.field.geometry.terrain.begin(), invalid.field.geometry.terrain.end(),
              Terrain::obstacle);
    rejected(invalid);
    // Enough floor cells in total, but diagonal corner contact is not a passage.
    invalid = encounter_fixture();
    const auto open = Terrain::open, wall = Terrain::obstacle;
    invalid.field.geometry = {3, 3, {open, wall, open, wall, open, wall, open, wall, open}};
    rejected(invalid);
    invalid = encounter_fixture();
    invalid.enemies[0].definition = "unsupported";
    rejected(invalid);
    invalid = encounter_fixture();
    invalid.enemies[0].id = pc;
    rejected(invalid);
    fight.encounter(encounter_fixture(), 1234);
    check(fight.has_combat() && party->in_combat(),
          "Valid encounter can start after rejected attempts");
}

// A camp interruption hands the whole party over resting; each conscious
// member wakes up Prone, and no enemy is affected.
void camp_ambush_encounter()
{
    auto party = std::make_shared<CampaignParty>(module());
    party->add_pc(character("wizard", "Sleeping mage"));
    party->add_pc(character("fighter", "Sleeping guard"));
    auto observed = std::make_shared<EncounterObservation>();
    CombatDemo fight(std::make_unique<ObservedModule>(observed));
    fight.campaign_party(party);
    auto ambush = encounter_fixture();
    ambush.party_resting = true;
    fight.encounter(ambush, 1234);
    for (const auto &participant : observed->encounter.participants)
        check(participant.resting == (participant.side == 0),
              "Every party member, and no enemy, starts the camp ambush resting");
    const auto log = fight.combat().snapshot().log();
    for (const auto *line :
            {
                "Sleeping mage wakes up prone.", "Sleeping guard wakes up prone."
            })
        check(std::count(log.begin(), log.end(), line) == 1,
              "Each interrupted sleeper wakes up Prone, once");
}

void allied_campaign_movement()
{
    auto party = std::make_shared<CampaignParty>(module());
    const auto mover = party->add_pc(character());
    party->add_pc(character("cleric", "Ally"));
    party->add_pc(character("wizard", "Second ally"));
    CampaignEncounter encounter;
    encounter.field.geometry = {7, 3, std::vector<Terrain>(21, Terrain::obstacle)};
    for (int x = 0; x < 7; ++x)
        encounter.field.geometry.terrain[7 + x] = Terrain::open;
    encounter.enemies = {{1000, "bandit", "Enemy", 1, {6, 1}}};
    encounter.positions = {{0, 1}, {1, 1}, {2, 1}, {6, 1}};
    std::unique_ptr<CombatDemo> fight;
    for (unsigned seed = 0; seed < 100 && !fight; ++seed)
    {
        auto candidate = std::make_unique<CombatDemo>(module());
        candidate->campaign_party(party);
        candidate->encounter(encounter, seed);
        if (candidate->combat().snapshot().actor == mover)
            fight = std::move(candidate);
    }
    check(bool(fight), "Find an initial turn for a normally created campaign member");
    const auto before = party->member(mover).vitals;
    const auto commands = fight->combat().legal_commands();
    const auto move = std::find_if(commands.begin(), commands.end(),
                                   [](const auto & c)
    {
        return c.verb == "move" && c.destination == Cell{4, 1};
    });
    check(move != commands.end() && fight->submit(*move),
          "Campaign member can move through two created allies");
    check(party->member(mover).vitals == before && party->state().time_minutes == 0 &&
          party->state().subminute_milliseconds == 0,
          "Allied transit preserves campaign vitals/resources and does not end the turn");
    finish(*fight);
    check(!party->in_combat(),
          "Campaign combat with allied transit finishes and unlocks the party");
    const auto saved = encode_campaign(*party, nullptr, "allied-transit");
    auto rules = module();
    auto loaded =
        decode_campaign(saved, *srd5::character_rules(), *rules, "allied-transit", nullptr);
    CampaignParty restored(module());
    restored.restore(std::move(loaded.party));
    check(encode_campaign(restored, nullptr, "allied-transit") == saved,
          "Campaign handoff and reload retain exact post-transit vitals, resources and time");
}

void standalone_checkpoints()
{
    CombatDemo fight(module());
    rejects(
        [&]
    {
        (void)fight.save_combat();
    });
    fight.training(42);
    const auto checkpoint = fight.save_combat();
    rejects(
        [&]
    {
        fight.restore_combat("malformed");
    });
    check(fight.save_combat() == checkpoint, "Failed restore preserves the live combat session");
    CombatDemo restored(module());
    restored.restore_combat(checkpoint);
    const auto command = choose_demo_command(fight.combat());
    check(fight.submit(command) && restored.submit(command),
          "Restored adapter accepts the same command");
    check(fight.save_combat() == restored.save_combat(),
          "Adapter checkpoint resumes deterministically");
}

// A replacement module can create a session whose initial snapshot is invalid.
// Rejection must not strand a party in combat or install the rejected session.
class InvalidInitialSession final : public CombatSession
{
  public:
    Snapshot snapshot() const override
    {
        return {};
    }

    std::vector<Command> legal_commands() const override
    {
        return {};
    }

    std::vector<Cell> movement_reach(EntityId) const override
    {
        return {};
    }

    bool submit(const Command &) override
    {
        return false;
    }

    std::string save() const override
    {
        return {};
    }
};

class InvalidInitialModule final : public test::ForwardingModule
{
  public:
    InvalidInitialModule() : ForwardingModule(module())
    {
    }

    std::unique_ptr<CombatSession> create(Encounter, std::uint64_t) const override
    {
        return std::make_unique<InvalidInitialSession>();
    }
};

void combat_ownership()
{
    auto party = std::make_shared<CampaignParty>(module());
    const auto id = party->add_pc(character());
    const auto before = party->member(id).vitals;
    CombatDemo invalid(std::make_unique<InvalidInitialModule>());
    invalid.campaign_party(party);
    rejects(
        [&]
    {
        invalid.training();
    });
    check(!invalid.has_combat() && !party->in_combat(),
          "Failed handoff releases its lock without installing combat");
    check(party->member(id).vitals == before, "Failed handoff preserves party vitals");
    party->select(0);
    {
        CombatDemo active(module());
        active.campaign_party(party);
        active.training();
        check(party->in_combat(), "Successful handoff holds the edit lock");
        const auto checkpoint = active.combat().save();
        rejects(
            [&]
        {
            active.training();
        });
        check(party->in_combat() && active.combat().save() == checkpoint,
              "Rejected restart retains active combat and lock");
        {
            CombatDemo contender(module());
            contender.campaign_party(party);
            rejects(
                [&]
            {
                contender.training();
            });
        }
        check(party->in_combat(), "Rejected contender cannot release another session's lock");
    }
    check(!party->in_combat(), "Destroying an unfinished combat releases the edit lock");
    party->select(0);
}

void progression_and_services()
{
    CampaignParty party(module());
    const auto pc = party.add_pc(character("fighter", "Progress"));
    auto damaged = party.checkpoint();
    damaged.roster[0].vitals.hit_points -= 2;
    party.restore(damaged);
    const auto starting_hp = party.member(pc).character.sheet().hit_points;
    party.award_experience(300, "quest:slums");
    check(party.member(pc).experience == 300 && party.member(pc).character.sheet().level == 1 &&
          party.can_advance(pc),
          "XP enables manual advancement without changing the level");
    const auto preview = party.preview_advancement(pc, party.default_advancement(pc));
    check(preview.character.sheet().level == 2 &&
          party.member(pc).character.sheet().hit_points == starting_hp,
          "Advancement preview does not mutate the character");
    party.advance(pc, party.default_advancement(pc));
    check(party.member(pc).character.sheet().hit_points > starting_hp &&
          party.profile(pc).hit_points == party.member(pc).character.sheet().hit_points,
          "Level HP applies to combat profile");
    party.award_experience(300, "quest:slums");
    check(party.member(pc).experience == 300, "Repeated reward id does not award XP twice");
    check(party.rest() && party.time_hours() == 8 &&
          party.member(pc).vitals.hit_points == party.member(pc).character.sheet().hit_points,
          "Long rest restores HP and advances campaign time");
    check(!party.rest() && party.time_hours() == 8,
          "Repeated long rest is denied without advancing time");
    party.advance_time(std::chrono::minutes(16 * 60));
    check(party.rest() && party.time_hours() == 32, "Long rest is allowed after the required wait");
    rejects(
        [&]
    {
        party.temple_heal(pc);
    });
    auto wounded = party.checkpoint();
    wounded.roster[0].vitals = {
        0, false, "SRD11 0 0 0 0 0 1 1 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0", "Unconscious"};
    party.restore(wounded);
    check(!party.rest() && party.time_hours() == 32,
          "Unconscious members cannot start a long rest");
    party.set_wealth(pc, {0, 0, 0, 50, 0, 0, 0});
    auto before = party.checkpoint();
    auto temple_before = party.member(pc).vitals;
    rejects(
        [&]
    {
        party.temple_heal(pc);
    });
    check(party.member(pc).wealth[3] == 50 && party.member(pc).vitals == temple_before,
          "Rejected temple request is atomic");
    check(party.state().random_state == before.random_state,
          "Rejected payment preserves random state");
    party.set_wealth(pc, {0, 0, 0, 100, 0, 0, 0});
    party.temple_heal(pc);
    check(party.member(pc).vitals.hit_points > 0 && party.member(pc).wealth[3] == 0 &&
          party.member(pc).vitals.resources ==
          "SRD11 0 0 0 0 0 0 1 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0",
          "Healing charges once, clears death saves and preserves spent resources");
    const auto checkpoint = party.checkpoint();
    CampaignParty restored(module());
    restored.restore(checkpoint);
    check(restored.member(pc).vitals == party.member(pc).vitals && restored.time_hours() == 32 &&
          restored.member(pc).wealth[3] == 0,
          "Native checkpoint retains recovery, payments and clock");
    restored.award_experience(300, "quest:slums");
    check(restored.member(pc).experience == 300, "Checkpoint retains claimed rewards");
    auto dead = checkpoint;
    dead.roster[0].vitals = {0, true, "SRD11 0 0 0 0 3 0 1 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0"};
    party.restore(dead);
    rejects(
        [&]
    {
        party.temple_heal(pc);
    });
    check(party.member(pc).vitals.dead, "Cheap healing cannot resurrect");
    party.restore(checkpoint);
    party.remove(pc);
    rejects(
        [&]
    {
        party.temple_heal(pc);
    });
    check(!party.rest(), "Empty party cannot rest");
    party.rejoin(pc);
    rejects(
        [&]
    {
        party.award_experience(std::numeric_limits<unsigned>::max(), "overflow");
    });
    check(party.member(pc).experience == 300 && party.state().claimed_rewards.size() == 1,
          "Overflow does not partially award XP");
    party.award_experience(600, "next quest");
    check(party.member(pc).experience == 900 && party.member(pc).character.sheet().level == 2 &&
          party.can_advance(pc),
          "Further XP waits for another explicit confirmation");
}

void caster_advancement()
{
    for (const auto *klass :
            {"wizard", "cleric"
            })
    {
        CampaignParty party(module());
        auto c = character(klass);
        auto draft = c.creation_data();
        draft.race = "dwarf";
        c = Character(*srd5::character_rules(), draft, {});
        const auto pc = party.add_pc(c);
        auto spent = party.checkpoint();
        const bool wizard = std::string_view(klass) == "wizard";
        spent.roster[0].vitals = {c.sheet().hit_points - 2, false,
                                  wizard ? "SRD11 0 0 0 0 0 0 1 0 0 0 \"\" 0 0 1 0 0 0 FX8 1 0 0"
                                  : "SRD11 0 0 0 0 0 0 1 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0"};
        party.restore(spent);
        party.award_experience(299, "below");
        check(party.member(pc).character.sheet().level == 1, "Below threshold does not advance");
        party.award_experience(1, "threshold");
        party.advance(pc, party.default_advancement(pc));
        const auto &m = party.member(pc);
        const auto growth = std::max(1, c.sheet().hit_die / 2 + 1 + c.sheet().modifiers[2]) + 1;
        check(m.character.sheet().hit_points == c.sheet().hit_points + growth &&
              m.vitals.hit_points == m.character.sheet().hit_points - 2,
              "Dwarven growth preserves HP deficit");
        check(m.vitals.resources ==
              (wizard ? "SRD11 0 1 0 0 0 0 2 0 0 0 \"\" 0 0 1 0 0 0 FX8 1 0 0"
               // A level-two Cleric gains two Channel Divinity uses.
               : "SRD11 0 1 0 0 0 0 2 0 0 0 \"\" 0 0 0 0 0 2 FX8 1 0 0"),
              "Advancement grants new slot without refilling spent slots");
        check(party.rest() &&
              party.member(pc).vitals.resources ==
              (wizard ? "SRD11 0 3 0 0 0 0 2 0 0 0 \"\" 0 0 1 0 0 0 FX8 1 0 0"
               : "SRD11 0 3 0 0 0 0 2 0 0 0 \"\" 0 0 0 0 0 2 FX8 1 0 0"),
              "Level-two long rest restores three slots and safely stands the rested character");
        auto participants = party.participants();
        participants.push_back({1000, "bandit", "Bandit", 1, {9, 4}});
        auto rules = module();
        auto combat = rules->create({{12, 9, std::vector<Terrain>(108)}, participants}, 42);
        const auto saved = combat->save();
        check(rules->restore(saved)->save() == saved,
              "Advanced profile and resources round-trip through combat checkpoint");
    }
}

void temple_pooling()
{
    CampaignParty party(module());
    const auto payer = party.add_pc(character()), target = party.add_pc(character("wizard"));
    party.set_wealth(payer, {0, 0, 0, 40, 0, 0, 0});
    party.set_wealth(target, {0, 0, 0, 50, 0, 0, 0});
    auto state = party.checkpoint();
    state.roster[1].vitals = {0, false, "SRD11 0 0 0 0 0 1 1 0 0 0 \"\" 0 0 1 0 0 0 FX8 1 0 0"};
    party.restore(state);
    rejects(
        [&]
    {
        party.temple_heal(target);
    });
    check(party.member(payer).wealth[3] == 40 && party.member(target).wealth[3] == 50 &&
          party.member(target).vitals == state.roster[1].vitals,
          "Insufficient pooled funds debit neither purse");
    party.set_wealth(target, {0, 0, 0, 60, 0, 0, 0});
    party.temple_heal(target);
    check(party.member(payer).wealth[3] == 0 && party.member(target).wealth[3] == 0 &&
          party.member(target).vitals.hit_points > 0 &&
          party.member(payer).vitals == state.roster[0].vitals,
          "Pooled service heals only the requested target");
}

void dynamic_checkpoint()
{
    auto rules = module();
    const auto c = character("wizard");
    const auto profile = rules->character_profile(c.sheet(), {});
    Encounter e{{4, 4, std::vector<Terrain>(16)},
        {   {1, "campaign-character", "Mage", 0, {0, 0}, profile.data},
            {2, "bandit", "Bandit", 1, {3, 3}}
        }};
    auto session = rules->create(e, 42);
    const auto bytes = session->save();
    auto restored = rules->restore(bytes);
    check(restored->save() == bytes, "Dynamic character profile round-trips exactly");
    const auto command = choose_demo_command(*session);
    check(session->submit(command) && restored->submit(command), "Restored command accepted");
    check(session->save() == restored->save(), "Dynamic checkpoint deterministic continuation");
    for (const auto &c : srd5::character_rules()->choices(CreationField::character_class))
    {
        const auto pc = character(c.id);
        const auto p = rules->character_profile(pc.sheet(), {});
        const int expected = 10 + pc.sheet().modifiers[1] +
                             (c.id == "monk"        ? pc.sheet().modifiers[4]
                              : c.id == "barbarian" ? pc.sheet().modifiers[2]
                              : 0);
        check(p.armor_class == std::max(expected, 10 + pc.sheet().modifiers[1]) &&
              p.hit_points == pc.sheet().hit_points,
              "All twelve classes have correct unarmored AC and HP profiles");
        CampaignParty party(module());
        const auto id = party.add_pc(pc);
        auto participants = party.participants();
        participants[0].cell = {0, 0};
        participants.push_back({1000, "bandit", "Bandit", 1, {3, 3}});
        auto fight = rules->create({{4, 4, std::vector<Terrain>(16)}, participants}, 42);
        const auto snapshot = fight->snapshot();
        check(std::any_of(snapshot.combatants.begin(), snapshot.combatants.end(),
                          [&](const auto & actor)
        {
            return actor.id == id && actor.hit_points == pc.sheet().hit_points;
        }),
        "Every created class enters campaign combat with its derived HP");
        check(rules->restore(fight->save())->save() == fight->save(),
              "Every class combat checkpoint restores");
    }
    e.participants[0].state = VitalState{99999, false, {}};
    rejects(
        [&]
    {
        (void)rules->create(e, 42);
    });
    e.participants[0].state =
        VitalState{1, false, "SRD11 0 99 0 0 0 0 1 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0"};
    rejects(
        [&]
    {
        (void)rules->create(e, 42);
    });
}

void combat_demo_fixture()
{
    const auto *directory = std::getenv("OPENGOLD_GAME_DIR");
    if (!directory || !*directory)
        return;
    auto characters = srd5::character_rules();
    auto scene = make_combat_demo(module(), *characters, directory);
    {
        // Play-testing: chosen classes in their kits at a chosen level.
        const std::vector<std::string> classes{"druid", "warlock", "druid"};
        const auto custom = make_combat_demo(module(), *characters, directory,
        {.classes = classes, .level = 4});
        const auto &roster = custom.party->state().roster;
        check(roster.size() == 3 && custom.encounter.positions.size() ==
              3 + custom.encounter.enemies.size() &&
              std::all_of(roster.begin(), roster.end(), [](const auto & m)
        {
            return m.character.sheet().level == 4 && !m.equipped.empty();
        }) &&
        roster[0].character.creation_data().name != roster[2].character.creation_data().name,
        "A play-test party has the chosen classes, kits and level, two Druids distinct");
    }
    auto mapped = make_combat_demo(module(), *characters, directory,
    {
        .body_catalog_file =
        std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/art/combat-body-looks.tsv"
    });
    const auto character_art = por::CharacterArt::load(directory);
    const auto looks = por::CombatBodyCatalog::load(
                           std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/art/combat-body-looks.tsv",
                           std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/art/combat-weapon-options.tsv");
    for (std::size_t i = 0; i < mapped.encounter.art.size(); ++i)
    {
        const auto &image = mapped.encounter.art[i];
        if (i < 6)
        {
            const auto resolved =
                por::resolve_combat_appearance(mapped.party->member(image.entity), looks);
            check(image.image.rgba == resolved.icon(character_art, por::IconPose::ready).rgba && image.action &&
                  image.action->rgba == resolved.icon(character_art, por::IconPose::action).rgba,
                  "Showcase uses the shared equipment appearance in both poses");
        }
        else
        {
            check(image.image.rgba == scene.encounter.art[i].image.rgba && image.action &&
                  scene.encounter.art[i].action &&
                  image.action->rgba == scene.encounter.art[i].action->rgba,
                  "Equipment mapping never changes encounter creature artwork");
        }
    }
    const auto heroes = scene.party->participants();
    check(heroes.size() == 6 && scene.encounter.enemies.size() == 13 &&
          scene.encounter.art.size() == 19 && scene.encounter.positions.size() == 19,
          "Showcase contains six visible heroes, twelve Kobolds and one leader");
    std::set<std::string> classes;
    unsigned goliaths = 0;
    for (std::size_t i = 0; i < 6; ++i)
    {
        const auto &hero = scene.party->member(heroes[i].id);
        classes.insert(hero.character.creation_data().character_class);
        goliaths += hero.character.creation_data().race == "goliath";
        check(hero.character.sheet().level == 1 && hero.equipped.size() == 2,
              "Every showcase hero is level one and has a weapon and armor equipped");
        const auto &inventory = hero.character.inventory();
        const auto weapon = inventory.find(hero.equipped[0]),
                   armor = inventory.find(hero.equipped[1]);
        check(weapon && armor &&
              (armor->get().definition_id == "chain_mail" ||
               armor->get().definition_id == "leather") &&
              (weapon->get().definition_id == "longsword" ||
               weapon->get().definition_id == "mace" ||
               weapon->get().definition_id == "dagger" ||
               weapon->get().definition_id == "quarterstaff"),
              "Equipped items are a weapon and armor");
        const auto profile = scene.party->profile(hero.id);
        check(!profile.strength_dexterity_disadvantage, "Showcase armor is class trained");
    }
    check(classes.size() == 6 && goliaths >= 1, "Showcase has six distinct classes and a Goliath");
    std::set<Cell> kobolds;
    unsigned leaders = 0;
    for (std::size_t i = 0; i < scene.encounter.enemies.size(); ++i)
    {
        const auto &actor = scene.encounter.enemies[i];
        const bool leader = actor.definition == "slums-kobold-leader";
        check((leader || actor.definition == "slums-kobold") && actor.side == 1,
              "Surrounding enemies use Kobold rules");
        leaders += leader;
        kobolds.insert(scene.encounter.positions[i + 6]);
    }
    check(leaders == 1 && kobolds.contains({6, 4}), "One leader occupies the top of the ring");
    const auto leader_index =
        std::find_if(scene.encounter.enemies.begin(), scene.encounter.enemies.end(),
                     [](const auto & actor)
    {
        return actor.definition == "slums-kobold-leader";
    }) -
    scene.encounter.enemies.begin();
    check(scene.encounter.art[6 + leader_index].image.rgba != scene.encounter.art[6].image.rgba,
          "Leader uses its own original combat icon");
    const auto catalog = por::CreatureCatalog::load(directory);
    const auto normal = catalog.find({2, 0}), chief = catalog.find({2, 1}),
               sword_chief = catalog.find({2, 11});
    check(normal && chief && sword_chief, "Original Slums Kobold records exist");
    const auto has_gear = [](const auto & creature, std::string_view name)
    {
        return std::any_of(creature.equipment.begin(), creature.equipment.end(),
                           [&](const auto & item)
        {
            return item.label() == name;
        });
    };
    check(!has_gear(normal->get(), "Short bow") && has_gear(chief->get(), "Short bow") &&
          has_gear(chief->get(), "Arrows") && !has_gear(sword_chief->get(), "Short bow"),
          "Only original Kobold leader record 1 carries a bow and arrows");
    for (const auto &cell : std::array<Cell, 5> {{{9, 4}, {9, 7}, {7, 8}, {4, 8}, {4, 5}}})
    check(!kobolds.contains(cell), "Removed Kobolds leave six openings in the ring");
    CombatDemo fight(module());
    fight.campaign_party(scene.party);
    auto invalid = scene.encounter;
    invalid.positions.pop_back();
    rejects(
        [&]
    {
        fight.encounter(invalid, 42);
    });
    check(!scene.party->in_combat(), "Invalid authored formation does not lock the party");
    const auto expected_positions = scene.encounter.positions;
    fight.encounter(std::move(scene.encounter), 42);
    check(fight.has_combat() && fight.combat().snapshot().combatants.size() == 19,
          "Game campaign encounter handoff starts the complete Kobold fight");
    for (std::size_t i = 0; i < expected_positions.size(); ++i)
    {
        const auto id = i < 6 ? heroes[i].id : static_cast<EntityId>(1000 + i - 6);
        const auto &actors = fight.combat().snapshot().combatants;
        const auto actor = std::find_if(actors.begin(), actors.end(),
                                        [&](const auto & value)
        {
            return value.id == id;
        });
        check(actor != actors.end() && actor->cell == expected_positions[i],
              "Game combat preserves the surrounded formation");
    }
}

using Bytes = std::vector<std::uint8_t>;

std::shared_ptr<const por::EclProgram> program(Bytes body)
{
    Bytes bytes{0, 0};
    for (int n = 0; n < 5; ++n)
        bytes.insert(bytes.end(), {1, 1, 0x15, 0x99});
    bytes.push_back(0);
    bytes.insert(bytes.end(), body.begin(), body.end());
    return std::make_shared<const por::EclProgram>(
               por::EclProgram::decode(bytes, "party integration"));
}

void settle(por::RolfTourSession &town)
{
    for (unsigned n = 0; n < 100 && town.snapshot().phase == por::TourPhase::running; ++n)
        town.advance(.5);
    check(town.snapshot().phase != por::TourPhase::faulted, "Town script fault");
}

void monster_picture_before_combat()
{
    // SETUP MONSTER selects sprite 4 and picture 4; after COMBAT is requested the
    // picture must hold combat back until the player dismisses it.
    auto gate = program({32, 0, 20, 0});
    auto encounter = program({58, 33, 0, 20, 0, 2, 0, 255, 12, 0, 4, 0, 0, 0, 4,
                              11, 0, 4, 0, 1, 0, 4, 36, 0});
    auto resources = std::make_shared<por::PhlanResources>();
    resources->map = por::GeoMap{};
    resources->programs[0] = gate;
    resources->programs[20] = encounter;
    auto district = std::make_shared<por::PhlanResources>();
    district->map = por::GeoMap{};
    district->encounter_creatures[4].stored.name = "Test orc";
    district->conversions[4] = {"slums-orc", 100, 75};
    district->combat_archive = {9, 0, 4, 0, 0, 0, 0, 25, 0, 26, 0, 24};
    district->combat_archive.resize(37, 0);
    district->combat_archive[12] = 1;
    district->combat_archive[14] = 2;
    district->combat_archive[20] = 1;
    // One uncompressed DAX record holding three 8x1 approach-sprite frames.
    district->sprite_archive = {9, 0, 4, 0, 0, 0, 0, 76, 0, 77, 0, 75, 3};
    for (int frame = 0; frame < 3; ++frame)
    {
        Bytes header(21, 0);
        header[4] = 1;
        header[6] = 1;
        district->sprite_archive.insert(district->sprite_archive.end(), header.begin(),
                                        header.end());
        district->sprite_archive.insert(district->sprite_archive.end(), 4, 0x11);
    }
    district->animations[4] = {{17, {}}, {15, {}}};
    district->script = 20; // the Slums script loads it
    district->bank = 2;
    resources->districts[20] = district;
    auto party = std::make_shared<CampaignParty>(module());
    (void)party->add_pc(character("fighter"));
    por::RolfTourSession town({}, gate, {}, 0x9914, {}, resources);
    town.campaign_party(party);
    settle(town);
    (void)town.observe_view();
    check(town.explore(por::ExplorationCommand::look), "Synthetic gate starts the encounter");
    settle(town);
    check(town.snapshot().phase == por::TourPhase::combat && !town.pending_encounter(),
          "Combat waits while the monster close-up shows");
    check(town.monster_picture() && town.monster_picture()->size() == 2 &&
          (*town.monster_picture())[0].delay == 17,
          "The close-up is SETUP MONSTER's PIC record");
    check(town.start_encounter() && town.pending_encounter() && !town.monster_picture(),
          "A key press dismisses the close-up and releases combat");
    check(!town.start_encounter(), "The close-up is dismissed only once");
}

void rejected_combat_handoff()
{
    // Enter a synthetic Slums district, change HP, then request one actual orc.
    // A rejected handoff must restore the entire event, not fabricate a victory.
    auto gate = program({32, 0, 20, 0});
    auto encounter =
        program({58, 33, 0, 20, 0, 2, 0, 255, 9, 0, 3, 1, 0x19, 0x6c, 11, 0, 4, 0, 1, 0, 4, 36, 0});
    auto resources = std::make_shared<por::PhlanResources>();
    resources->map = por::GeoMap{};
    resources->programs[0] = gate;
    resources->programs[20] = encounter;
    auto district = std::make_shared<por::PhlanResources>();
    district->map = por::GeoMap{};
    district->encounter_creatures[4].stored.name = "Test orc";
    district->conversions[4] = {"slums-orc", 100, 75};
    // One literal DAX record with an authored 16x1 combat icon, no original data.
    district->combat_archive = {9, 0, 4, 0, 0, 0, 0, 25, 0, 26, 0, 24};
    district->combat_archive.resize(37, 0);
    district->combat_archive[12] = 1;
    district->combat_archive[14] = 2;
    district->combat_archive[20] = 1;
    district->script = 20; // the Slums script loads it
    district->bank = 2;
    resources->districts[20] = district;
    for (const auto *klass :
            {"rogue", "fighter"
            })
    {
        auto party = std::make_shared<CampaignParty>(module());
        const auto id = party->add_pc(character(klass));
        const auto before = party->checkpoint();
        por::RolfTourSession town({}, gate, {}, 0x9914, {}, resources);
        town.campaign_party(party);
        settle(town);
        (void)town.observe_view();
        const auto known_before = town.snapshot().seen;
        check(!town.reject_combat("Stale rejection"),
              "No combat rejection outside a pending handoff");
        check(town.explore(por::ExplorationCommand::look),
              "Synthetic gate starts an encounter event");
        settle(town);
        check(town.pending_encounter().has_value() &&
              town.snapshot().phase == por::TourPhase::combat,
              "Script reaches the actual campaign combat boundary");
        check(party->member(id).vitals.hit_points == 3, "Event applies its pre-combat HP change");
        (void)town.observe_view();
        {
            CombatDemo combat(module());
            combat.campaign_party(party);
            combat.encounter(*town.pending_encounter(), 42);
            check(party->in_combat() && combat.has_combat(),
                  "Every supported class reaches campaign combat");
            check(!town.reject_combat("Cannot cancel active combat"),
                  "Rejection cannot bypass an active combat owner");
        }
        check(town.reject_combat("Combat initialization failed"),
              "Failed handoff rolls back its event");
        check(!town.pending_encounter() && !party->in_combat(),
              "Rollback clears the encounter and edit lock");
        check(party->member(id).vitals == before.roster[0].vitals &&
              party->state().claimed_rewards == before.claimed_rewards,
              "Rollback restores party HP and resources without rewards");
        check(town.snapshot().area_id == 0 &&
              town.script_variable(0x6C19) == before.roster[0].vitals.hit_points,
              "Rollback restores the map and original script state");
        check(town.snapshot().seen == known_before,
              "Failed district event restores exploration knowledge atomically");
        check(town.snapshot().dialogue.find("Combat initialization failed") != std::string::npos,
              "The actual failure remains visible in the exploration notice");
        check(!town.reject_combat("Duplicate"), "Rejection is applied once");
        check(town.choose(town.snapshot().continue_ticket, 0) && town.can_leave(),
              "Acknowledging the notice restores navigation");
        check(town.explore(por::ExplorationCommand::turn_right),
              "Exploration accepts commands after the failed encounter");
    }
}

// A synthetic district whose gate starts one encounter with a Test orc.
std::shared_ptr<por::PhlanResources> orc_encounter_resources()
{
    auto gate = program({32, 0, 20, 0});
    auto encounter = program({58, 33, 0, 20, 0, 2, 0, 255, 11, 0, 4, 0, 1, 0, 4, 36, 0});
    auto resources = std::make_shared<por::PhlanResources>();
    resources->map = por::GeoMap{};
    resources->programs[0] = gate;
    resources->programs[20] = encounter;
    auto district = std::make_shared<por::PhlanResources>();
    district->map = por::GeoMap{};
    district->encounter_creatures[4].stored.name = "Test orc";
    district->conversions[4] = {"slums-orc", 100, 75};
    district->combat_archive = {9, 0, 4, 0, 0, 0, 0, 25, 0, 26, 0, 24};
    district->combat_archive.resize(37, 0);
    district->combat_archive[12] = 1;
    district->combat_archive[14] = 2;
    district->combat_archive[20] = 1;
    district->script = 20; // the Slums script loads it
    district->bank = 2;
    resources->districts[20] = district;
    return resources;
}

// Walks the synthetic gate into its encounter and returns the fight's opening
// snapshot.
Snapshot start_orc_encounter(por::RolfTourSession &town, const std::shared_ptr<CampaignParty> &party)
{
    town.campaign_party(party);
    settle(town);
    check(town.explore(por::ExplorationCommand::look), "Synthetic gate starts the encounter");
    settle(town);
    check(town.pending_encounter().has_value(), "The encounter reaches combat");
    CombatDemo combat(module());
    combat.campaign_party(party);
    combat.encounter(*town.pending_encounter(), 42);
    return combat.combat().snapshot();
}

// A member who died in an earlier fight sits out the next one. Winning it must
// still return the party to exploration with its rewards.
void victory_beside_dead_member()
{
    const auto resources = orc_encounter_resources();
    auto party = std::make_shared<CampaignParty>(module());
    (void)party->add_pc(character("fighter"));
    const auto fallen = party->add_pc(character("fighter", "Bo"));
    auto state = party->checkpoint();
    state.roster[1].vitals.hit_points = 0;
    state.roster[1].vitals.dead = true;
    party->restore(state);
    por::RolfTourSession town({}, resources->programs.at(0), {}, 0x9914, {}, resources);
    auto result = start_orc_encounter(town, party);
    check(std::none_of(result.combatants.begin(), result.combatants.end(),
                       [&](const auto & unit)
    {
        return unit.id == fallen;
    }),
    "The dead member does not fight");
    result.outcome = Outcome::victory;
    for (auto &unit : result.combatants)
        if (unit.side == 1)
            unit.hit_points = 0;
    check(town.resolve_combat(result) && town.snapshot().phase != por::TourPhase::combat,
          "The victory resolves although a dead member sat it out");
}

// When every monster runs off the field the party wins, but nothing is left to
// pay experience or treasure. The fight must still end, once.
void victory_when_every_monster_fled()
{
    const auto resources = orc_encounter_resources();
    auto party = std::make_shared<CampaignParty>(module());
    (void)party->add_pc(character("fighter"));
    por::RolfTourSession town({}, resources->programs.at(0), {}, 0x9914, {}, resources);
    auto result = start_orc_encounter(town, party);
    const auto before = party->checkpoint();
    result.outcome = Outcome::victory;
    for (auto &unit : result.combatants)
        if (unit.side == 1)
            unit.fled = true;
    check(town.resolve_combat(result) && town.snapshot().phase != por::TourPhase::combat,
          "A victory over monsters that all fled ends the fight");
    check(party->state().claimed_rewards == before.claimed_rewards &&
          party->state().roster[0].experience == before.roster[0].experience,
          "Monsters that all got away are worth no experience or treasure");
}

void recovery_hosts()
{
    auto party = std::make_shared<CampaignParty>(module());
    const auto pc = party->add_pc(character());
    auto state = party->checkpoint();
    state.roster[0].vitals = {1, false, "SRD11 0 0 0 0 0 0 1 0 0 0 \"\" 0 0 0 0 0 0 FX8 1 0 0"};
    party->restore(state);
    auto resources = std::make_shared<por::PhlanResources>();
    auto p = program({0});
    por::RolfTourSession allowed({}, p, {}, 0x9914, {}, resources);
    allowed.campaign_party(party);
    settle(allowed);
    check(allowed.explore(por::ExplorationCommand::camp), "Camp starts pre-camp entry");
    settle(allowed);
    check(allowed.can_leave() && party->time_hours() == 8 &&
          party->member(pc).vitals.hit_points == party->member(pc).character.sheet().hit_points,
          "Allowed ECL camp recovers party");
    check(allowed.script_variable(0x6c19) == party->member(pc).vitals.hit_points,
          "Camp synchronizes script HP");
    check(allowed.script_variable(0x49c9) == 20, "Camp updates original hour register");
    allowed.explore(por::ExplorationCommand::look);
    settle(allowed);
    check(party->member(pc).vitals.hit_points == party->member(pc).character.sheet().hit_points,
          "Next event does not overwrite recovered HP");
    party->restore(state);
    auto denied_program = program({9, 0, 255, 1, 0xd3, 0x6d, 0});
    por::RolfTourSession denied({}, denied_program, {}, 0x9914, {}, resources);
    denied.campaign_party(party);
    settle(denied);
    denied.explore(por::ExplorationCommand::camp);
    settle(denied);
    check(denied.can_leave() && party->state().time_minutes == 0 &&
          party->member(pc).vitals.hit_points == 1,
          "Denied pre-camp gives no time or recovery");
    // Slot 2 arms a guaranteed five-minute interruption. Slot 3 records execution.
    Bytes bytes{0, 0};
    for (int n = 0; n < 5; ++n)
        bytes.insert(bytes.end(), {1, 1, 0x15, 0x99});
    bytes.push_back(0);
    Bytes pre{9, 0, 1, 1, 0xd2, 0x6d, 9, 0, 101, 1, 0xd3, 0x6d, 0};
    const unsigned interrupt = 0x9915 + static_cast<unsigned>(pre.size());
    bytes[16] = interrupt & 255;
    bytes[17] = interrupt >> 8;
    bytes.insert(bytes.end(), pre.begin(), pre.end());
    bytes.insert(bytes.end(), {9, 0, 1, 1, 0x10, 0x98, 0});
    auto interrupted_program = std::make_shared<const por::EclProgram>(
                                   por::EclProgram::decode(bytes, "camp interruption"));
    por::RolfTourSession interrupted({}, interrupted_program, {}, 0x9914, {}, resources);
    interrupted.campaign_party(party);
    settle(interrupted);
    interrupted.explore(por::ExplorationCommand::camp);
    settle(interrupted);
    check(interrupted.can_leave() && interrupted.script_variable(0x9810) == 1 &&
          party->state().time_minutes == 5 && party->member(pc).vitals.hit_points == 1,
          "Interruption entry runs without granting rest benefits");
    check(interrupted.script_variable(0x49c7) == 5,
          "Interruption advances original minute register");
    party->restore(state);
    auto inn_program = program({56, 0, 9, 0});
    por::RolfTourSession inn({}, inn_program, {}, 0x9914, {}, resources);
    inn.campaign_party(party);
    settle(inn);
    inn.explore(por::ExplorationCommand::look);
    settle(inn);
    check(inn.can_leave() && party->time_hours() == 8 && inn.script_variable(0x49c9) == 20,
          "PROGRAM 9 replies with restored HP and advanced clock");
    party->restore(state);
    auto temple_program = program({9, 0, 1, 1, 0xe2, 0x6d, 36, 9, 0, 1, 1, 0x11, 0x98, 0});
    por::RolfTourSession temple({}, temple_program, {}, 0x9914, {}, resources);
    temple.campaign_party(party);
    settle(temple);
    temple.explore(por::ExplorationCommand::look);
    settle(temple);
    const auto ticket = temple.snapshot().continue_ticket;
    check(temple.snapshot().choices.size() == 2 && !temple.choose(ticket + 1, 0),
          "Temple offers wounded target and cancel with stale-ticket rejection");
    check(!temple.choose(ticket, 0) && party->member(pc).vitals.hit_points == 1 &&
          party->state().random_state == state.random_state,
          "Unaffordable temple choice leaves request and party unchanged");
    party->set_wealth(pc, {0, 0, 0, 100, 0, 0, 0});
    check(temple.choose(ticket, 0) && !temple.choose(ticket, 0),
          "Temple payment accepted exactly once");
    settle(temple);
    check(temple.can_leave() && temple.script_variable(0x6de2) == 0 &&
          temple.script_variable(0x9811) == 1 && party->member(pc).wealth[3] == 0 &&
          party->member(pc).vitals.hit_points > 1,
          "Temple resumes ECL with healed HP and charged purse");
    // An unsupported continuation refunds the complete service, including dice.
    party->restore(state);
    party->set_wealth(pc, {0, 0, 0, 100, 0, 0, 0});
    auto failed = program({9, 0, 1, 1, 0xe2, 0x6d, 36, 56, 0, 0, 0});
    por::RolfTourSession rollback({}, failed, {}, 0x9914, {}, resources);
    rollback.campaign_party(party);
    settle(rollback);
    rollback.explore(por::ExplorationCommand::look);
    settle(rollback);
    check(rollback.choose(rollback.snapshot().continue_ticket, 0),
          "Service before unsupported continuation");
    settle(rollback);
    if (party->member(pc).wealth[3] != 100 || party->member(pc).vitals.hit_points != 1 ||
            party->state().random_state != state.random_state)
        throw std::runtime_error(
            "Failed service rollback: gold=" + std::to_string(party->member(pc).wealth[3]) +
            ", HP=" + std::to_string(party->member(pc).vitals.hit_points) + ", " +
            rollback.snapshot().dialogue);
    check(rollback.choose(rollback.snapshot().continue_ticket, 0) && rollback.can_leave(),
          "Rollback clears pending temple ticket");
}

void reward_reentry()
{
    auto party = std::make_shared<CampaignParty>(module());
    const auto pc = party->add_pc(character("wizard"));
    party->recruit("guard", character());
    for (unsigned visit = 0; visit < 2; ++visit)
    {
        if (visit)
        {
            party->advance_time(std::chrono::minutes(24 * 60));
            check(party->rest(), "Recover before second preview");
            party->keep_rest_spells(pc);
        }
        CombatDemo fight(module());
        fight.campaign_party(party);
        fight.training();
        finish(fight);
        check(fight.combat().snapshot().outcome == Outcome::victory, "Representative victory");
        check(party->member(pc).experience == 300 && party->state().claimed_rewards.size() == 1,
              "Recreated combat scene cannot duplicate its reward");
        check(!fight.submit({}), "Finished combat rejects more commands");
    }
}

// WHO; write selected HP; store; FIND ITEM; shop; exit.
std::shared_ptr<const por::EclProgram> shop_program()
{
    return program({57, 0,    0,    9,    0,    3,    1,    0x19, 0x6c, 10,   0,    129,  10,
                    0,  0,    39,   0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
                    0,  0,    0,    0,    0,    54,   9,    0,    1,    1,    0x6c, 0x6e, 36,
                    29, 1,    0x10, 0x98, 30,   1,    0x1b, 0x6c, 0,    0,    1,    0x11, 0x98,
                    1,  0x12, 0x98, 1,    0x13, 0x98, 1,    0x14, 0x98, 54,   0,    7,    0,
                    70, 50,   0,    59,   22,   9,    0,    1,    1,    0x15, 0x98, 0});
}

void shop_buyer_switch()
{
    auto party = std::make_shared<CampaignParty>(module());
    auto first = party->add_pc(character("fighter", "First")),
         second = party->add_pc(character("cleric", "Second"));
    party->set_wealth(first, {0, 0, 0, 100, 0, 0, 0});
    party->set_wealth(second, {0, 0, 0, 200, 0, 0, 0});
    auto p = shop_program();
    auto resources = std::make_shared<por::PhlanResources>();
    resources->programs.emplace(0, p);
    resources->treasure[54] = {item(59)};
    resources->npc_profiles.emplace(7, character("fighter", "Script guard"));
    por::RolfTourSession town({}, p, {}, 0x9914, {}, resources);
    town.campaign_party(party);
    settle(town);
    check(town.can_select_member(), "Members can be selected while exploring");
    town.explore(por::ExplorationCommand::look);
    settle(town);
    check(!town.can_select_member(), "Selection is frozen during WHO and other prompts");
    town.choose(town.snapshot().continue_ticket, 1);
    settle(town);
    check(town.snapshot().phase == por::TourPhase::shopping && town.can_select_member(),
          "The buyer can change while shopping");
    party->select(0);
    check(town.buy(town.snapshot().continue_ticket, 0), "Buy for the newly chosen buyer");
    check(party->member(first).wealth[3] == 90 && party->member(second).wealth[3] == 200,
          "The purchase debits only the new buyer");
    town.leave_shop(town.snapshot().continue_ticket);
    settle(town);
    check(town.script_variable(0x6BC1) == 90,
          "Leaving the shop hands the final buyer to the original script");
}

void script_handoff()
{
    auto party = std::make_shared<CampaignParty>(module());
    auto first = party->add_pc(character("fighter", "First")),
         second = party->add_pc(character("cleric", "Second"));
    party->set_wealth(first, {0, 0, 0, 100, 0, 0, 0});
    party->set_wealth(second, {0, 0, 0, 200, 0, 0, 0});
    auto p = shop_program();
    auto resources = std::make_shared<por::PhlanResources>();
    resources->programs.emplace(0, p);
    resources->treasure[54] = {item(59)};
    resources->npc_profiles.emplace(7, character("fighter", "Script guard"));
    por::RolfTourSession town({}, p, {}, 0x9914, {}, resources);
    town.campaign_party(party);
    settle(town);
    town.explore(por::ExplorationCommand::look);
    settle(town);
    check(town.snapshot().choices.size() == 2, "WHO displays real party");
    const auto ticket = town.snapshot().continue_ticket;
    check(!town.choose(ticket + 1, 1) && town.choose(ticket, 1) && !town.choose(ticket, 1),
          "WHO handles ticket once");
    settle(town);
    check(party->selected() == second && party->member(second).vitals.hit_points == 3,
          "ECL HP writes target selected real character");
    check(town.snapshot().phase == por::TourPhase::shopping,
          "Original shop opens for selected member");
    check(party->selected() == second, "Script slot scans do not change the chosen buyer");
    check(town.buy(town.snapshot().continue_ticket, 0), "Buy for selected member");
    check(party->member(second).wealth[3] == 190 && party->member(first).wealth[3] == 100,
          "Purchase debits only selected purse");
    check(por::party_has_item(*party, 59), "Script item query sees purchase");
    town.leave_shop(town.snapshot().continue_ticket);
    settle(town);
    check(town.script_variable(0x6BC1) == 190 && town.can_leave(),
          "Shop return synchronizes selected character");
    // PARTY STRENGTH ran with the level-one fighter at full HP and the cleric at 3 HP:
    // (HP + 5 for THAC0 20) / 10 and (4 + 3 + 5) / 10.
    const auto first_hp = party->member(first).character.sheet().hit_points;
    check(town.script_variable(0x9810) == (first_hp + 5) / 10 + 1 &&
          town.script_variable(0x9811) == 12 &&
          town.script_variable(0x9812) == 12 && town.script_variable(0x9813) == 12 &&
          town.script_variable(0x9814) == 12,
          "ECL movement queries share encounter-menu conversion units; the fourth is the slowest");
    check(town.script_variable(0x9815) == 1,
          "FIND ITEM drives actual bytecode branch after purchase");
    check(party->state().slots[6] && party->member(party->state().slots[6]).morale == 70,
          "ADD NPC uses explicit conversion and requested morale");
    party->equip(second, 1);
    check(party->profile(second).armor_class ==
          12 + party->member(second).character.sheet().modifiers[1],
          "Cleric equips purchased shield");
    // A supported store followed by an unsupported query must roll the party back.
    const auto hp = party->member(second).vitals.hit_points;
    auto failed =
        program({10, 0, 1, 9,    0,    0, 1,    0x19, 0x6c, 10,   0,    129, 30,   1,    0xa7, 0x6b,
                 0,  0, 1, 0x11, 0x98, 1, 0x12, 0x98, 1,    0x13, 0x98, 1,   0x14, 0x98, 0});
    por::RolfTourSession rollback({}, failed, {}, 0x9914, {}, resources);
    rollback.campaign_party(party);
    settle(rollback);
    rollback.explore(por::ExplorationCommand::look);
    settle(rollback);
    check(rollback.snapshot().phase == por::TourPhase::awaiting_continue &&
          party->member(second).vitals.hit_points == hp,
          "Unsupported event restores authoritative party checkpoint");
}
} // namespace

void original_loot()
{
    CampaignParty party(module());
    const auto first = party.add_pc(character()), second = party.add_pc(character("cleric", "Bo"));
    party.set_wealth(first, {0, 65530, 0, 0, 0, 0, 0});
    party.set_wealth(second, {0, 0, 0, 0, 0, 0, 0});
    por::Equipment scroll;
    scroll.stored.type = 62;
    scroll.stored.magic_bonus = 1;
    scroll.stored.stack_size = 1;
    check(party.award_loot({0, 96, 0, 0, 0, 0, 0}, {scroll}, "test:loot"),
          "Collect original coins and item");
    check(party.member(first).wealth[1] == 65535 && party.member(second).wealth[1] == 91,
          "Coin overflow continues into another purse");
    check(party.member(first).item_sources.size() == 1 &&
          party.member(first).character.inventory().items()[0].definition_id ==
          "por:unsupported:62",
          "Magic loot retains provenance without inventing a rules conversion");
    check(party.award_loot({0, 96, 0, 0, 0, 0, 0}, {scroll}, "test:loot") &&
          party.member(second).wealth[1] == 91,
          "Duplicate loot cannot pay twice");
    party.set_wealth(second, {0, 65535, 0, 0, 0, 0, 0});
    const auto before = party.checkpoint();
    check(!party.award_loot({0, 1, 0, 0, 0, 0, 0}, {scroll}, "test:overflow"),
          "Full party purses retain pending loot");
    check(party.state().claimed_rewards == before.claimed_rewards &&
          party.member(first).item_sources.size() == 1,
          "Failed collection changes neither reward history nor inventory");
}

// PARTY STRENGTH reads equivalent AD&D values (docs/PARTY.md): Gold Box THAC0 by
// class group and level, AC of worn armor, current HP and spellcaster levels.
void party_strength()
{
    CampaignParty party(module());
    auto armored = character("fighter", "Armored");
    const auto mail = armored.add_item({.definition_id = "chain_mail",
                                               .name = "Chain mail"});
    const auto shield = armored.add_item({.definition_id = "shield", .name = "Shield"});
    const auto fighter = party.add_pc(std::move(armored));
    party.equip(fighter, mail);
    party.equip(fighter, shield);
    const auto cleric = party.add_pc(character("cleric", "Cleric"));
    const auto wizard = party.add_pc(character("wizard", "Wizard"));
    const auto rogue = party.add_pc(character("rogue", "Rogue"));
    const auto set_hit_points = [&](std::map<MemberId, int> hit_points)
    {
        auto state = party.checkpoint();
        for (auto &member : state.roster)
            member.vitals.hit_points = hit_points.at(member.id);
        party.restore(std::move(state));
    };

    // Fighter (5 + 5 for THAC0 20) / 10 = 1; Cleric (4 x level 1 + 5 + 5) / 10 = 1;
    // Wizard (5 + 8 x level 1, THAC0 21) / 10 = 1; Rogue (4 + 5) / 10 = 0.
    // Chain mail and shield are AD&D AC 4, which adds nothing above AC 0.
    set_hit_points({{fighter, 5}, {cleric, 5}, {wizard, 5}, {rogue, 4}});
    check(por::party_strength(party) == 3, "Level-one party strength uses AD&D-equivalent values");

    party.award_experience(900, "strength-levels");
    for (const auto id : {fighter, wizard})
        for (unsigned level = 2; level <= 3; ++level)
            party.advance(id, party.default_advancement(id));
    // Fighter level 3 is THAC0 18: (5 + 15) / 10 = 2. Wizard level 3: (5 + 24) / 10 = 2.
    set_hit_points({{fighter, 5}, {cleric, 5}, {wizard, 5}, {rogue, 4}});
    check(por::party_strength(party) == 5, "Fighter THAC0 and magic-user levels follow the original");

    // Fighter (20 + 15) / 10 = 3.
    set_hit_points({{fighter, 20}, {cleric, 5}, {wizard, 5}, {rogue, 4}});
    check(por::party_strength(party) == 6, "Current hit points add with the other terms");
}

int main()
{
    try
    {
        party_strength();
        two_weapon_equipment();
        equipment_rule_boundary();
        combat_body_assignments();
        party_combat_appearance();
        all_weapon_equipment();
        goliath_occupancy();
        original_loot();
        roster_and_equipment();
        party_leader();
        class_weapon_proficiency();
        stabilization_handoff();
        remaining_turn_handoff();
        untrained_equipment();
        combat_handoff();
        campaign_encounters();
        camp_ambush_encounter();
        allied_campaign_movement();
        standalone_checkpoints();
        combat_ownership();
        progression_and_services();
        caster_advancement();
        temple_pooling();
        dynamic_checkpoint();
        combat_demo_fixture();
        script_handoff();
        shop_buyer_switch();
        rejected_combat_handoff();
        victory_beside_dead_member();
        victory_when_every_monster_fled();
        monster_picture_before_combat();
        recovery_hosts();
        reward_reentry();
        std::cout << "Party integration tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
