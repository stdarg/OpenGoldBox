#include "combat_fixture.h"
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/item_key.h"
#include "opengold/srd5.h"
#include "test_support.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

namespace
{
using opengold::test::check;

const auto source = std::filesystem::path(OPENGOLD_SOURCE_DIR);

std::string read(const std::filesystem::path &path)
{
    std::ifstream in(path);
    check(bool(in), "Read the rules");
    return {std::istreambuf_iterator<char>(in), {}};
}

// An AC 1 target that hits back, and the same as an Undead.
std::unique_ptr<RulesModule> module()
{
    return srd5::parse_content(read(source / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "creature risen 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "type risen undead\n");
}

Character fighter()
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "fighter";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Fighter";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    return Character(*srd5::character_rules(), d, {});
}

// A Bard with Identify and Detect Magic prepared and two level-one slots.
Character bard()
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "bard";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Bard";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    d.training = {{"class:bard", {"performance", "persuasion", "insight"}}};
    d.cantrips = {"vicious_mockery", "starry_wisp"};
    d.spells = SpellChoices{
        {},
        std::vector<std::string>{"identify", "detect_magic", "healing_word", "dissonant_whispers"},
        {},
        {}};
    return Character(*srd5::character_rules(), d, {});
}

por::Equipment original(std::uint8_t type, int bonus, std::array<std::uint8_t, 3> words = {},
                        std::array<std::uint8_t, 3> effects = {})
{
    por::Equipment item;
    item.stored.type = type;
    item.stored.magic_bonus = bonus;
    item.stored.name_components = words;
    item.stored.effect_codes = effects;
    item.stored.stack_size = 1;
    return item;
}

void item_keys()
{
    const auto flame = parse_item_key("longsword+2:flame_tongue");
    check(flame.base == "longsword" && flame.bonus == 2 && flame.power == ItemPower::flame_tongue,
          "A key carries its base, bonus and power");
    const auto cursed = parse_item_key("longsword-2:cursed");
    check(cursed.base == "longsword" && cursed.bonus == -2 && cursed.power == ItemPower::cursed,
          "A penalty is a negative bonus");
    check(parse_item_key("shield+1").base == "shield" && parse_item_key("shield+1").bonus == 1,
          "A plain enchantment");
    check(parse_item_key("light_crossbow").base == "light_crossbow" &&
              parse_item_key("por:unsupported:36").base == "por:unsupported:36" &&
              parse_item_key("longsword+0").base == "longsword+0",
          "Keys without an enchantment are all base");
    check(item_key("greatsword", 1, ItemPower::bane_undead) == "greatsword+1:undead" &&
              item_key("chain_mail", 0) == "chain_mail",
          "Keys are written as they are read");
}

// The original's records, as docs/MAGIC_ITEMS.md lists them.
void conversion()
{
    check(equipment_conversion(original(36, 1, {0, 162, 36})) == "longsword+1", "Long Sword +1");
    check(equipment_conversion(original(36, 2, {241, 163, 36}, {0, 22, 0})) ==
              "longsword+2:flame_tongue",
          "Long Sword +2 Flame Tongue");
    check(equipment_conversion(original(38, 1, {253, 162, 38}, {0, 3, 0x88})) ==
              "greatsword+1:undead",
          "Two-Handed Sword +1 +3 vs Undead");
    auto broad = original(35, -2, {255, 202, 35});
    broad.stored.cursed_raw = 1;
    check(equipment_conversion(broad) == "longsword-2:cursed", "Cursed Broad Sword -2");
    check(equipment_conversion(original(36, 3, {0, 164, 36}, {0, 82, 0x84})) == "longsword+3",
          "An undecoded special effect leaves the plain enchantment");
    check(equipment_conversion(original(55, 1)) == "chain_mail+1" &&
              equipment_conversion(original(59, 2)) == "shield+2" &&
              equipment_conversion(original(73, 1)) == "arrow+1" &&
              equipment_conversion(original(47, 2, {163, 200, 47})) == "sling+2",
          "Armor, shields, ammunition and the Sling of Seeking");
    check(equipment_conversion(original(21, 1, {157, 167, 21}, {1, 83, 0})) == "javelin:lightning",
          "The Javelin of Lightning's charge is not an enchantment");
    check(equipment_conversion(original(79, 0, {}, {20, 88, 0})) == "wand:magic_missiles",
          "A charged wand is its own item, not an enchanted one (#263)");
}

void profile()
{
    const auto rules = module();
    const auto hero = fighter();
    const auto with = [&](std::vector<std::string> gear)
    {
        return rules->character_profile(hero.sheet(), gear);
    };
    const auto plain = with({"longsword", "chain_mail", "shield"});
    const auto magic = with({"longsword+2", "chain_mail+1", "shield+2"});
    check(magic.melee_attack_bonus == plain.melee_attack_bonus + 2,
          "A +2 weapon shows on the attack bonus");
    check(magic.armor_class == plain.armor_class + 3, "Armor +1 and a Shield +2 add to AC");
    check(with({"longsword-2:cursed"}).melee_attack_bonus ==
              with({"longsword"}).melee_attack_bonus - 2,
          "A -2 weapon lowers it");
    const auto notes = magic.item_modifiers;
    check(notes.find("Longsword +2. Its magic adds +2 to attack and damage rolls.") !=
                  std::string::npos &&
              notes.find("Chain Mail +1. Its magic adds +1 to Armor Class.") != std::string::npos,
          "The sheet says what the magic adds");
}

struct Strike
{
    int bonus{}, damage{};
};

bool submit(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &command : c.legal_commands())
    {
        if (command.verb == verb && (!target || command.target == target))
        {
            return c.submit(command);
        }
    }
    return false;
}

// A fight, with a fixed seed, at the fighter's first turn.
std::unique_ptr<CombatSession> fighter_turn(const RulesModule &rules, const std::string &weapon,
                                            const std::string &enemy)
{
    const auto profile = rules.character_profile(fighter().sheet(), std::vector{weapon}).data;
    auto c = rules.create({{12, 6, std::vector<Terrain>(72)},
                           {{1, "campaign-character", "Fighter", Side::party, {1, 1}, profile},
                            {2, enemy, "Foe", Side::opposition, {2, 1}}}},
                          5);
    test::keep_initiative(*c);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
    {
        check(submit(*c, "end"), "End the foe's turn");
    }
    check(c->snapshot().actor == 1, "The fighter acts");
    return c;
}

// The fighter's first attack's roll bonus and damage, when it hits.
std::optional<Strike> logged_hit(const CombatSession &c)
{
    for (const auto &line : c.snapshot().log())
    {
        const auto hits = line.find(" hits for ");
        if (line.starts_with("Fighter -> Foe: d20 ") && hits != std::string::npos)
        {
            return Strike{std::stoi(line.substr(line.find(" + ") + 3)),
                          std::stoi(line.substr(hits + 10))};
        }
    }
    return std::nullopt;
}

std::optional<Strike> strike(const RulesModule &rules, const std::string &weapon,
                             const std::string &enemy)
{
    auto c = fighter_turn(rules, weapon, enemy);
    check(submit(*c, "melee", 2), "Attack");
    return logged_hit(*c);
}

void attacks()
{
    const auto rules = module();
    const auto plain = strike(*rules, "longsword", "target");
    const auto magic = strike(*rules, "longsword+2", "target");
    check(plain && magic, "Both attacks hit");
    check(magic->bonus == plain->bonus + 2 && magic->damage == plain->damage + 2,
          "A +2 weapon adds 2 to the attack and damage rolls");

    const auto bane = strike(*rules, "greatsword+1:undead", "target");
    const auto undead = strike(*rules, "greatsword+1:undead", "risen");
    const auto greatsword = strike(*rules, "greatsword", "target");
    check(bane && undead && greatsword, "The two-handed attacks hit");
    check(bane->bonus == greatsword->bonus + 1 && undead->bonus == greatsword->bonus + 3,
          "+1, or +3 against the Undead");
}

void flame_tongue()
{
    const auto rules = module();
    check(parse_item_key("longsword+2:flame_tongue:attuned").base ==
              "longsword+2:flame_tongue:attuned",
          "No item is attuned");

    auto hidden = fighter_turn(*rules, "longsword+2:flame_tongue:unidentified", "target");
    check(submit(*hidden, "ignite_flame_tongue"), "An unidentified Flame Tongue lights too");

    auto c = fighter_turn(*rules, "longsword+2:flame_tongue", "target");
    check(submit(*c, "ignite_flame_tongue") && !submit(*c, "douse_flame_tongue"),
          "Lighting the blade is a Bonus Action");
    const auto restored = rules->restore(c->save());
    check(restored->save() == c->save(), "A checkpoint keeps the blade lit");
    check(submit(*c, "melee", 2) && test::logged(*c, "Fire damage from Flame Tongue."),
          "A lit blade's hit deals extra Fire damage");

    auto dark = fighter_turn(*rules, "longsword+2:flame_tongue", "target");
    check(submit(*dark, "melee", 2) && logged_hit(*dark) &&
              !test::logged(*dark, "Fire damage from Flame Tongue."),
          "An unlit blade deals no Fire damage");

    const auto hero = fighter();
    const auto notes = [&](const std::string &key)
    {
        return rules->character_profile(hero.sheet(), std::vector{key}).item_modifiers;
    };
    check(notes("longsword+2:flame_tongue").find("A Bonus Action lights the blade") !=
              std::string::npos,
          "The sheet says how the fire works");
}

// Nothing needs attunement: a rest does not identify a found Flame Tongue.
// Its fire works at once, unidentified (flame_tongue).
void no_attunement()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    // Found with its "+2" and "Flame Tongue" hidden, as in the original.
    auto flame = original(36, 2, {241, 163, 36}, {0, 22, 0});
    flame.stored.hidden_name_words = 6;
    flame.decoded_name = "Long Sword +2 Flame Tongue";
    flame.unidentified_name = "Long Sword";
    check(party.award_loot({}, {flame}, "test:flame"), "Collect Flame Tongue");
    const auto sword = party.member(id).character.inventory().items().back().id;
    const auto name = [&]
    {
        return party.member(id).character.inventory().find(sword)->get().name;
    };
    check(name() == "Long Sword", "Flame Tongue is found as a Long Sword");
    party.equip(id, sword);
    const auto rested = party.rest(RestKind::short_rest);
    check(rested && rested->spending, "Finish a Short Rest");
    party.finish_short_rest(*rested->spending);
    check(name() == "Long Sword", "A rest does not reveal its name");
}

// The original's cursed Broad Sword cannot be put down once equipped.
void cursed_sword()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    auto broad = original(35, -2, {255, 202, 35});
    broad.stored.cursed_raw = 1;
    check(party.award_loot({}, {broad, original(37, 0, {0, 0, 37})}, "test:cursed"),
          "Collect the cursed sword and a short sword");
    const auto items = party.member(id).character.inventory().items();
    const auto sword = items[items.size() - 2].id, other = items.back().id;
    party.unequip(id, other);
    party.equip(id, sword);
    test::rejects(
        [&]
        {
            party.unequip(id, sword);
        },
        "A cursed sword cannot be unequipped");
    test::rejects(
        [&]
        {
            party.equip(id, other, EquipmentOperation::equip_main);
        },
        "Another weapon cannot replace it");
    check(party.profile(id).item_modifiers.find("It is cursed") != std::string::npos,
          "The sheet says the sword is cursed");
}

// Loot from the original keeps its enchantment in the party and in a save.
void campaign()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    const auto plain = party.profile(id).melee_attack_bonus;
    check(party.award_loot({}, {original(36, 2, {0, 163, 36})}, "test:magic-loot"),
          "Collect a Long Sword +2");
    const auto &sword = party.member(id).character.inventory().items().back();
    check(sword.definition_id == "longsword+2", "The loot converts with its bonus");
    party.equip(id, sword.id);
    check(party.profile(id).melee_attack_bonus == plain + 2, "Equipped, it adds its bonus");

    CampaignParty restored(module());
    const auto rules = module();
    restored.restore(decode_campaign(encode_campaign(party, nullptr, "magic"),
                                     *srd5::character_rules(), *rules, "magic", nullptr)
                         .party);
    check(restored.member(id).character.inventory().items().back().definition_id == "longsword+2" &&
              restored.profile(id).melee_attack_bonus == plain + 2,
          "A save keeps the item and its bonus");
}
// The original hides some of a found item's name words until it is identified
// (#262). The rules then name it without its enchantment, which still counts.
void identification()
{
    check(parse_item_key("longsword+2:unidentified").unidentified &&
              parse_item_key("longsword+2:flame_tongue:unidentified").unidentified &&
              parse_item_key("ring_of_fire_resistance:unidentified").base ==
                  "ring_of_fire_resistance" &&
              parse_item_key("longsword+2:unidentified:cursed").base ==
                  "longsword+2:unidentified:cursed",
          "Unidentified is the last flag of an enchanted key");

    std::vector<std::string> words(256);
    words[36] = "Long Sword";
    words[163] = "+2";
    words[241] = "Flame Tongue";
    auto record = original(36, 2, {241, 163, 36}).stored;
    check(por::item_name(record, words) == "Long Sword +2 Flame Tongue" &&
              por::item_name(record, words, 6) == "Long Sword" &&
              por::item_name(record, words, 4) == "Long Sword +2",
          "Hidden name words are left out");

    auto generic = original(36, 2, {0, 163, 36});
    generic.bonuses.weapon_to_hit = 2;
    generic.stored.hidden_name_words = 6;
    check(!generic.identified() && generic.label().find("+2") == std::string::npos,
          "Unidentified, a sword is named without its bonus");
    generic.identify();
    check(generic.identified() && generic.label().ends_with(" +2") && !generic.stored.raw[53],
          "Identified, it has its bonus");
    auto words_only = original(36, 0, {0, 0, 36});
    words_only.stored.hidden_name_words = 6;
    check(words_only.identified(), "A mask over empty words hides nothing");

}

// In the party an unidentified item keeps its name hidden, also in a save,
// until the shop's ID reveals it for 200 gp.
void shop_identification()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    const auto plain = party.profile(id).melee_attack_bonus;
    auto hidden = original(36, 2, {0, 163, 36});
    hidden.bonuses.weapon_to_hit = 2;
    hidden.stored.hidden_name_words = 6;
    check(party.award_loot({}, {hidden}, "test:hidden"), "Collect an unidentified Long Sword +2");
    const auto sword = party.member(id).character.inventory().items().back().id;
    party.equip(id, sword);
    const auto name = [&](const CampaignParty &p)
    {
        return p.member(id).character.inventory().find(sword)->get().name;
    };
    const auto told = [&](const CampaignParty &p)
    {
        const auto profile = p.profile(id);
        bool labelled = false;
        for (const auto &position : profile.equipment_positions)
        {
            labelled = labelled || position.source.find("+2") != std::string::npos;
        }
        return labelled || profile.item_modifiers.find("Its magic adds +2") != std::string::npos;
    };
    check(name(party).find("+2") == std::string::npos && !told(party) &&
              party.profile(id).melee_attack_bonus == plain + 2,
          "Unidentified, its bonus counts but is not shown");

    CampaignParty restored(module());
    const auto rules = module();
    restored.restore(decode_campaign(encode_campaign(party, nullptr, "hidden"),
                                     *srd5::character_rules(), *rules, "hidden", nullptr)
                         .party);
    check(name(restored) == name(party) && !told(restored), "A save keeps it unidentified");

    Purse purse{};
    coins(purse, Coin::gold) = 300;
    restored.set_wealth(purse);
    check(restored.identify_at_shop(id, sword) && name(restored).ends_with(" +2") &&
              told(restored) && coins(restored.state().wealth, Coin::gold) == 100,
          "The shop's ID reveals the bonus for 200 gp");
    test::rejects(
        [&]
        {
            (void)restored.identify_at_shop(id, sword);
        },
        "ID needs 200 gp");
    coins(purse, Coin::gold) = 200;
    restored.set_wealth(purse);
    check(!restored.identify_at_shop(id, sword) &&
              coins(restored.state().wealth, Coin::gold) == 0,
          "An item with nothing hidden still costs the fee, as in the original");
}
// Detect Magic, cast outside combat, marks magic items for ten minutes; then
// Identify, cast on an item, reveals its name (#262).
void identification_spells()
{
    CampaignParty party(module());
    const auto fighter_id = party.add_pc(fighter());
    const auto bard_id = party.add_pc(bard());
    auto hidden = original(36, 2, {0, 163, 36});
    hidden.bonuses.weapon_to_hit = 2;
    hidden.stored.hidden_name_words = 6;
    check(party.award_loot({}, {hidden, original(37, 0, {0, 0, 37})}, "test:spells"),
          "Collect a hidden Long Sword +2 and a short sword");
    const auto holder = [&](const por::Equipment &item)
    {
        for (const auto id : {fighter_id, bard_id})
        {
            for (const auto &carried : party.member(id).character.inventory().items())
            {
                const auto &source = party.member(id).item_sources.at(carried.id);
                if (source.stored.magic_bonus == item.stored.magic_bonus &&
                    source.stored.type == item.stored.type)
                {
                    return std::pair{id, carried.id};
                }
            }
        }
        throw std::runtime_error("Loot not found");
    };
    const auto [sword_holder, sword] = holder(hidden);
    const auto [plain_holder, plain] = holder(original(37, 0, {0, 0, 37}));

    const auto camp = party.camp_actions(bard_id);
    check(std::none_of(camp.begin(), camp.end(),
                       [](const auto &action)
                       {
                           return action.id.starts_with("identify");
                       }) &&
              std::any_of(camp.begin(), camp.end(),
                          [](const auto &action)
                          {
                              return action.id == "detect_magic" && action.whole_party;
                          }),
          "Cast / Use offers Detect Magic for the party, but not Identify");
    test::rejects(
        [&]
        {
            party.use_camp_action(bard_id, sword_holder, "identify");
        },
        "Identify needs an item");

    check(!party.detecting_magic(), "No Detect Magic yet");
    party.use_camp_action(bard_id, bard_id, "detect_magic");
    check(party.detecting_magic() && party.magical(sword_holder, sword) &&
              !party.magical(plain_holder, plain),
          "Detect Magic senses the magic sword, not the plain one");
    CampaignParty restored(module());
    const auto rules = module();
    restored.restore(decode_campaign(encode_campaign(party, nullptr, "detect"),
                                     *srd5::character_rules(), *rules, "detect", nullptr)
                         .party);
    check(restored.detecting_magic(), "A save keeps Detect Magic");
    party.advance_time(std::chrono::minutes{9});
    check(party.detecting_magic(), "Detect Magic lasts ten minutes");
    party.advance_time(std::chrono::minutes{1});
    check(!party.detecting_magic(), "Then it ends");

    check(party.identifier() == bard_id, "The Bard can cast Identify");
    check(party.cast_identify(bard_id, sword_holder, sword) &&
              party.member(sword_holder).character.inventory().find(sword)->get().name.ends_with(
                  " +2"),
          "Identify reveals the sword's bonus");
    check(!party.identifier(), "Both level-one slots are spent");
    test::rejects(
        [&]
        {
            (void)party.cast_identify(bard_id, plain_holder, plain);
        },
        "Identify needs a slot");
}
} // namespace

int main()
{
    try
    {
        item_keys();
        conversion();
        profile();
        attacks();
        campaign();
        flame_tongue();
        no_attunement();
        cursed_sword();
        identification();
        shop_identification();
        identification_spells();
        std::cout << "Enchanted item tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
