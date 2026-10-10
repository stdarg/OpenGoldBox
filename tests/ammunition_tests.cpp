#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace opengold;
using namespace opengold::rules;

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

auto module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

void act(CombatSession &session, std::string_view verb, EntityId target = 0)
{
    for (const auto &command : session.legal_commands())
    {
        if (command.verb == verb && (!target || command.target == target))
        {
            check(session.submit(command), "Accept ammunition command");
            return;
        }
    }
    throw std::runtime_error("Missing ammunition command: " + std::string(verb));
}

void continue_attack(CombatSession &session)
{
    act(session, "ranged", 2);
    if (session.snapshot().free_movement)
        act(session, "end");
    act(session, "action_surge");
    act(session, "ranged", 2);
}

void purchased_ammunition()
{
    auto rules = module();
    CharacterDraft draft;
    draft.race = "human";
    draft.gender = "female";
    draft.character_class = "fighter";
    draft.background = "soldier";
    draft.alignment = "neutral_good";
    draft.name = "Ammunition baseline";
    draft.rolled = true;
    for (auto &roll : draft.rolls)
        roll = {{6, 5, 4, 1}, 3};
    Character hero(*srd5::character_rules(), draft, {});
    const auto bow = hero.add_item({.definition_id = "longbow", .name = "Baseline longbow"});
    hero.add_item({.definition_id = "dagger", .name = "Carried dagger", .quantity = 2});
    CampaignParty party(module());
    const auto id = party.add_pc(std::move(hero));
    party.equip(id, bow);
    for (const auto [type, quantity] :
            {
                std::pair{73u, 7u}, {73u, 3u}, {28u, 5u}
            })
    {
        por::Equipment ammunition;
        ammunition.stored.type = type;
        ammunition.stored.stack_size = quantity;
        party.purchase(id, ammunition); // Real original-item provenance and conversion.
    }

    party.award_experience(2700, "ammunition-baseline");
    for (unsigned level = 2; level <= 4; ++level)
        party.advance(id, party.default_advancement(id));
    const auto campaign = encode_campaign(party, nullptr, "ammunition-before");
    party.restore(
        decode_campaign(campaign, *srd5::character_rules(), *rules, "ammunition-before", nullptr)
        .party);
    const auto &member = party.member(1);
    check(member.character.sheet().level == 4 && member.equipped == std::vector<std::uint64_t> {1},
          "Campaign retains advancement and equipped item identity");
    for (const auto [item_id, quantity] :
            {
                std::pair{3u, 7u}, {4u, 3u}, {5u, 5u}
            })
    {
        const auto item = member.character.inventory().find(item_id);
        check(item && item->get().quantity == quantity && member.item_sources.contains(item_id) &&
              item->get().original_type == (item_id == 5 ? 28 : 73) &&
              item->get().definition_id == equipment_conversion(member.item_sources.at(item_id)),
              "Campaign retains distinct ammunition stacks without invented stock");
        check(item->get().definition_id == (item_id == 5 ? "bolt" : "arrow"),
              "Ordinary original ammunition converts to its supported SRD supply");
    }

    auto actors = party.participants();
    actors.front().cell = {1, 1};
    actors.push_back({2, "vanguard", "Target", Side::opposition, {5, 1}});
    actors.push_back({3, "vanguard", "Reserve", Side::opposition, {7, 7}});
    auto combat = rules->create({{10, 8, std::vector<Terrain>(80)}, actors}, 1);
    while (combat->snapshot().actor != id)
        act(*combat, "end");
    auto copy = rules->restore(combat->save());
    check(copy->save() == combat->save(), "Combat round trip retains exact state");
    continue_attack(*combat);
    continue_attack(*copy);
    check(combat->save() == copy->save(),
          "Restored ranged and Action Surge continuation remains exact");
}

void inventory_paths()
{
    auto rules = module();
    unsigned classes = 0;
    for (const auto &klass : srd5::character_rules()->choices(CreationField::character_class))
    {
        ++classes;
        CharacterDraft draft;
        draft.race = "human";
        draft.gender = "female";
        draft.character_class = klass.id;
        draft.background = "soldier";
        draft.alignment = "neutral_good";
        draft.name = "Ammunition carrier";
        draft.rolled = true;
        for (auto &roll : draft.rolls)
            roll = {{6, 5, 4, 1}, 3};
        Character hero(*srd5::character_rules(), draft, {});
        for (const auto key :
                {"arrow", "bolt", "sling_bullet", "firearm_bullet", "needle"
                })
            hero.add_item({.definition_id = key, .name = key, .quantity = 21});
        const auto bow = hero.add_item({.definition_id = "shortbow", .name = "Shortbow"});
        CampaignParty party(module());
        const auto id = party.add_pc(std::move(hero));
        party.equip(id, bow);
        const auto before = encode_campaign(party, nullptr, "ammunition-inventory");
        for (std::uint64_t item = 1; item <= 5; ++item)
        {
            check(party.equipment_info(id, item).slot == EquipmentSlot::carried,
                  "All five ammunition types are recognized carried supplies");
            bool rejected = false;
            try
            {
                party.equip(id, item);
            }
            catch (const std::runtime_error &)
            {
                rejected = true;
            }
            check(rejected && encode_campaign(party, nullptr, "ammunition-inventory") == before,
                  "Equipping ammunition rejects atomically without displacing the weapon");
        }
        CampaignParty restored(module());
        restored.restore(decode_campaign(before, *srd5::character_rules(), *rules,
                                         "ammunition-inventory", nullptr)
                         .party);
        check(encode_campaign(restored, nullptr, "ammunition-inventory") == before,
              "Every class preserves all five authored ammunition stacks across saves");
        for (const auto type :
                {
                    28u, 73u
                })
        {
            por::Equipment original;
            original.stored.type = type;
            original.stored.stack_size = 7;
            check(equipment_conversion(original) == (type == 28 ? "bolt" : "arrow"),
                  "Original ordinary quarrels and arrows map to matching ammunition");
            for (unsigned variant = 0; variant < 3; ++variant)
            {
                auto special = original;
                if (variant == 0)
                    special.stored.magic_bonus = 1;
                else if (variant == 1)
                    special.stored.cursed_raw = 1;
                else
                    special.stored.effect_codes[0] = 1;
                check(equipment_conversion(special) == "por:unsupported:" + std::to_string(type),
                      "Magic, cursed and effect-bearing ammunition remains unsupported");
                party.purchase(id, special);
            }
        }
        const auto with_special = encode_campaign(party, nullptr, "ammunition-inventory");
        restored.restore(decode_campaign(with_special, *srd5::character_rules(), *rules,
                                         "ammunition-inventory", nullptr)
                         .party);
        check(encode_campaign(restored, nullptr, "ammunition-inventory") == with_special,
              "Special original ammunition retains quantities and complete provenance");
    }
    check(classes == 12, "Ammunition inventory and save paths cover all twelve classes");
}
} // namespace

int main()
{
    try
    {
        purchased_ammunition();
        inventory_paths();
        std::cout << "Ammunition checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
