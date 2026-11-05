// Auto-equip (EQUIP-1 in docs/SRD-DECISIONS.md): a weapon, armor or shield a
// member gains goes on when the gear it makes is better, by the member's
// preference for defense or damage; untrained or too heavy armor, ranged
// weapons, known cursed items and, when asked, unidentified magic stay packed.
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "test_support.h"
#include <algorithm>
#include <filesystem>
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

// Original item types (the original's item list).
constexpr std::uint8_t dagger = 8, long_sword = 36, short_sword = 37, two_handed_sword = 38,
                       long_bow = 43, leather = 50, ring_mail = 53, chain_mail = 55, shield = 59;

std::unique_ptr<RulesModule> module()
{
    std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                     "data/rules/srd-5.2.1/combat.rules");
    check(bool(in), "Read the rules");
    const std::string content{std::istreambuf_iterator<char>(in), {}};
    return srd5::parse_content(content);
}

// A human Fighter whose ability rolls are each `die` three times.
Character fighter(int die = 5)
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
        r = {{die, die, die, 1}, 3};
    }
    return Character(*srd5::character_rules(), d, {});
}

// A Bard: trained in Light armor only, and no shields.
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

por::Equipment item(std::uint8_t type, int bonus = 0)
{
    por::Equipment equipment;
    equipment.stored.type = type;
    equipment.stored.magic_bonus = bonus;
    equipment.stored.stack_size = 1;
    return equipment;
}

// A Long Sword +2 whose bonus the party has not identified.
por::Equipment unidentified_sword()
{
    auto sword = item(long_sword, 2);
    sword.stored.name_components = {0, 163, long_sword};
    sword.bonuses.weapon_to_hit = 2;
    sword.stored.hidden_name_words = 6;
    return sword;
}

// The member gains the item (a free purchase); its inventory id is returned.
std::uint64_t gain(CampaignParty &party, MemberId id, const por::Equipment &gained)
{
    std::vector<std::uint64_t> before;
    for (const auto &carried : party.member(id).character.inventory().items())
    {
        before.push_back(carried.id);
    }
    party.purchase(id, gained);
    for (const auto &carried : party.member(id).character.inventory().items())
    {
        if (std::find(before.begin(), before.end(), carried.id) == before.end())
        {
            return carried.id;
        }
    }
    throw std::runtime_error("Nothing gained");
}

std::optional<GearChange> take(CampaignParty &party, MemberId id, const por::Equipment &gained,
                               UnidentifiedMagic unidentified = UnidentifiedMagic::skip)
{
    return party.auto_equip(id, gain(party, id, gained), unidentified);
}

void wield(CampaignParty &party, MemberId id, const por::Equipment &gained,
           EquipmentOperation operation = EquipmentOperation::equip)
{
    (void)party.equip(id, gain(party, id, gained), operation);
}

std::vector<std::string> worn(const CampaignParty &party, MemberId id)
{
    std::vector<std::string> names;
    for (const auto equipped : party.member(id).equipped)
    {
        names.push_back(party.member(id).character.inventory().find(equipped)->get().name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool changed(const std::optional<GearChange> &change, std::vector<std::string> equipped,
             std::vector<std::string> unequipped)
{
    if (!change)
    {
        return false;
    }
    auto off = change->unequipped;
    std::sort(off.begin(), off.end());
    std::sort(unequipped.begin(), unequipped.end());
    return change->equipped == equipped && off == unequipped && change->burned_by.empty();
}

// A Long Sword does more than a Short Sword and takes its place; another
// Short Sword then stays in the pack.
void better_weapon()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    wield(party, id, item(short_sword));
    check(changed(take(party, id, item(long_sword)), {"Long sword"}, {"Short sword"}) &&
              worn(party, id) == std::vector<std::string>{"Long sword"},
          "A Long Sword replaces a held Short Sword");
    check(!take(party, id, item(short_sword)) &&
              worn(party, id) == std::vector<std::string>{"Long sword"},
          "A weaker Short Sword stays in the pack");
    check(!take(party, id, item(long_bow)), "A bow stays in the pack: a fight draws it");
}

// With a shield, Maximize defense keeps it over a two-handed sword, and
// Maximize damage takes the sword, the shield going to the pack; back on
// defense, a shield goes on with the Long Sword from the pack.
void shield_or_both_hands()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    wield(party, id, item(long_sword));
    wield(party, id, item(shield));
    check(party.member(id).gear_preference == GearPreference::defense &&
              !take(party, id, item(two_handed_sword)),
          "Maximize defense keeps the shield");
    party.set_gear_preference(id, GearPreference::damage);
    check(changed(take(party, id, item(two_handed_sword)), {"Two-handed sword"},
                  {"Long sword", "Shield"}),
          "Maximize damage takes the two-handed sword");
    party.set_gear_preference(id, GearPreference::defense);
    const auto change = take(party, id, item(shield));
    check(change && change->equipped.front() == "Shield" &&
              worn(party, id) == std::vector<std::string>{"Long sword", "Shield"},
          "On defense, a shield goes on with a one-handed weapon from the pack");
}

// Better armor goes on; a Bard's untrained armor and shield, and armor a weak
// Fighter lacks the Strength for, stay in the pack.
void armor()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    check(changed(take(party, id, item(leather)), {"Leather armor"}, {}),
          "Armor goes on when none is worn");
    check(changed(take(party, id, item(chain_mail)), {"Chain armor"}, {"Leather armor"}),
          "Chain Mail replaces Leather Armor");
    check(!take(party, id, item(leather)), "Worse armor stays in the pack");

    const auto bard_id = party.add_pc(bard());
    check(!take(party, bard_id, item(chain_mail)) && !take(party, bard_id, item(shield)),
          "A Bard's untrained armor and shield stay in the pack");
    check(changed(take(party, bard_id, item(leather)), {"Leather armor"}, {}),
          "A Bard puts on Leather Armor");

    const auto weak = party.add_pc(fighter(2));
    check(party.member(weak).character.sheet().scores[Ability::strength] < 13 &&
              !take(party, weak, item(chain_mail)),
          "Chain Mail asks Strength 13");
    check(changed(take(party, weak, item(ring_mail)), {"Ring armor"}, {}),
          "Ring Mail asks for no Strength");
}

// Holding two daggers, a Short Sword takes the main hand: the dagger it
// replaces goes to the pack and the other stays for the off-hand attack.
void two_weapons()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    wield(party, id, item(dagger));
    wield(party, id, item(dagger), EquipmentOperation::equip_other);
    check(changed(take(party, id, item(short_sword)), {"Short sword"}, {"Dagger"}) &&
              worn(party, id) == std::vector<std::string>{"Dagger", "Short sword"},
          "A Short Sword replaces the weaker of two daggers");
}

// An unidentified magic item says so; it stays packed when asked, and is
// otherwise judged by its plain stats, its hidden bonus unknown. A cursed
// item the party knows is cursed stays packed.
void magic()
{
    check(unidentified_sword().label() == "Long sword (unidentified magic)",
          "An unidentified magic item is named as one");
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    wield(party, id, item(short_sword));
    check(!take(party, id, unidentified_sword(), UnidentifiedMagic::skip),
          "Unidentified magic stays packed when asked");
    const auto change = take(party, id, unidentified_sword(), UnidentifiedMagic::equip);
    check(changed(change, {"Long sword (unidentified magic)"}, {"Short sword"}),
          "Otherwise it goes on");
    check(!take(party, id, item(long_sword)), "As a plain Long Sword: another is no better");

    party.set_gear_preference(id, GearPreference::damage);
    auto cursed = item(two_handed_sword);
    cursed.stored.cursed_raw = 1;
    check(!take(party, id, cursed), "A known cursed sword stays packed");
    check(take(party, id, item(two_handed_sword)).has_value(),
          "The same sword uncursed would go on");
}

// Each member's preference is saved.
void saved_preference()
{
    CampaignParty party(module());
    const auto id = party.add_pc(fighter());
    party.set_gear_preference(id, GearPreference::damage);
    const auto rules = module();
    CampaignParty restored(module());
    restored.restore(decode_campaign(encode_campaign(party, nullptr, "gear"),
                                     *srd5::character_rules(), *rules, "gear", nullptr)
                         .party);
    check(restored.member(id).gear_preference == GearPreference::damage,
          "A save keeps the member's gear preference");
}
} // namespace

int main()
{
    try
    {
        better_weapon();
        shield_or_both_hands();
        armor();
        two_weapons();
        magic();
        saved_preference();
        std::cout << "Auto-equip tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
