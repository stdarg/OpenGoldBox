#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace opengold;
using namespace opengold::rules;
namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);
const auto fixtures = root / "tests/fixtures";
void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
auto module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}
std::string read(const char *name)
{
    std::ifstream file(fixtures / name);
    check(bool(file), "Open actual prior-writer ammunition fixture");
    return {std::istreambuf_iterator<char>(file), {}};
}
void write(const char *name, const std::string &bytes)
{
    std::ofstream file(fixtures / name);
    file << bytes;
    check(bool(file), "Write actual prior-writer ammunition fixture");
}
void act(CombatSession &session, std::string_view verb, EntityId target = 0)
{
    for (const auto &command : session.legal_commands())
    {
        if (command.verb == verb && (!target || command.target == target))
        {
            check(session.submit(command), "Accept actual baseline ammunition command");
            return;
        }
    }
    throw std::runtime_error("Missing baseline command: " + std::string(verb));
}
void continue_attack(CombatSession &session)
{
    act(session, "ranged", 2);
    if (session.snapshot().free_movement)
        act(session, "end");
    act(session, "action_surge");
    act(session, "ranged", 2);
}
void capture_prior_writer()
{
    auto rules = module();
    check(rules->identity().version == "0.6.47",
          "Capture requires the actual pre-ammunition writer");
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
    const auto bow = hero.inventory().add("longbow", "Baseline longbow");
    hero.inventory().add("dagger", "Carried dagger", 2); // Activates real format 19.
    CampaignParty party(module());
    const auto id = party.add_pc(std::move(hero));
    party.equip(id, bow);
    for (const auto [type, quantity] : {std::pair{73u, 7u}, {73u, 3u}, {28u, 5u}})
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
    (void)decode_campaign(campaign, *srd5::character_rules(), *rules, "ammunition-before", nullptr);
    write("campaign-v11-ammunition-before.ogs", campaign);
    auto actors = party.participants();
    actors.front().cell = {1, 1};
    actors.push_back({2, "vanguard", "Target", 1, {5, 1}});
    actors.push_back({3, "vanguard", "Reserve", 1, {7, 7}});
    auto combat = rules->create({{10, 8, std::vector<std::uint8_t>(80)}, actors}, 1);
    while (combat->snapshot().actor != id)
        act(*combat, "end");
    combat = rules->restore(combat->save());
    write("combat-v19-ammunition-before.save", combat->save());
    continue_attack(*combat);
    write("combat-v19-ammunition-continued.save", combat->save());
}
std::string current_identity(std::string bytes, const RulesModule &rules)
{
    const auto position = bytes.find("0.6.47");
    check(position != bytes.npos, "Actual prior-writer identity exists");
    bytes.replace(position, 6, rules.identity().version);
    return bytes;
}
void prior_writer_continuation()
{
    auto rules = module();
    CampaignParty party(module());
    party.restore(decode_campaign(read("campaign-v11-ammunition-before.ogs"),
                                  *srd5::character_rules(), *rules, "ammunition-before", nullptr)
                      .party);
    const auto &member = party.member(1);
    check(member.character.sheet().level == 4 && member.equipped == std::vector<std::uint64_t>{1},
          "Prior campaign retains advancement and equipped item identity");
    for (const auto [id, quantity] : {std::pair{3u, 7u}, {4u, 3u}, {5u, 5u}})
    {
        const auto item = member.character.inventory().find(id);
        check(item && item->get().quantity == quantity && member.item_sources.contains(id) &&
                  item->get().original_type == (id == 5 ? 28 : 73) &&
                  item->get().definition_id == equipment_conversion(member.item_sources.at(id)),
              "Prior campaign retains distinct ammunition stacks without invented stock");
        check(item->get().definition_id == (id == 5 ? "bolt" : "arrow"),
              "Old ordinary original ammunition migrates to its supported SRD supply");
    }
    const auto before = read("combat-v19-ammunition-before.save");
    auto combat = rules->restore(before);
    check(combat->save() == current_identity(before, *rules),
          "Prior combat round trip retains exact state");
    continue_attack(*combat);
    check(combat->save() == current_identity(read("combat-v19-ammunition-continued.save"), *rules),
          "Actual old ranged and Action Surge continuation remains exact");
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
        for (const auto key : {"arrow", "bolt", "sling_bullet", "firearm_bullet", "needle"})
            hero.inventory().add(key, key, 21);
        const auto bow = hero.inventory().add("shortbow", "Shortbow");
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
        for (const auto type : {28u, 73u})
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
int main(int argc, char **argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--capture-prior-writer")
            capture_prior_writer();
        else
        {
            check(argc == 1, "Unknown ammunition test argument");
            prior_writer_continuation();
            inventory_paths();
        }
        std::cout << "Ammunition prior-writer checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
