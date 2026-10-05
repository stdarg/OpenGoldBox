#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

// Healing outside combat from the Camp dialog (CLASS-3), through CampaignParty.
namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

template <class F> void rejects(F f, const char *message)
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
    check(caught, message);
}

std::unique_ptr<RulesModule> module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

Character created(std::string klass, std::string name, std::vector<std::string> prepared)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = klass;
    d.background = "acolyte";
    d.alignment = "lawful_good";
    d.name = std::move(name);
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    if (!prepared.empty())
        d.spells = SpellChoices{{}, std::move(prepared), {}, {}};
    if (klass == "paladin")
        d.training = {{"class:paladin", {"athletics", "insight"}}};
    if (klass == "ranger")
        d.training = {{"class:ranger", {"athletics", "nature", "perception"}}};
    if (klass == "cleric")
    {
        d.cantrips = std::vector<std::string> {"sacred_flame"};
        d.training = {{"class:cleric", {"medicine", "persuasion"}},
            {"class:cleric:divine_order", {"protector"}}
        };
    }
    return Character(*srd5::character_rules(), d, {});
}

std::string saved(const CampaignParty &p)
{
    return encode_campaign(p, nullptr, "camp-actions");
}

std::vector<std::string> action_ids(const CampaignParty &party, MemberId id)
{
    std::vector<std::string> ids;
    for (const auto &action : party.camp_actions(id))
        ids.push_back(action.id);
    return ids;
}

unsigned remaining(const CampaignParty &party, MemberId id, std::string_view pool)
{
    for (const auto &resource : party.recovery_info(id).resources)
        if (resource.id == pool)
            return resource.remaining;
    return 0;
}

// Sets a member's Hit Points, keeping fresh resources.
void wound(CampaignParty &party, MemberId id, int hit_points)
{
    auto state = party.checkpoint();
    for (auto &m : state.roster)
        if (m.id == id)
            m.vitals = {hit_points, false, ""};
    party.restore(state);
}

int hp(const CampaignParty &party, MemberId id)
{
    return party.member(id).vitals.hit_points;
}

void lay_on_hands_checks()
{
    CampaignParty party(module());
    const auto paladin = party.add_pc(created("paladin", "Paladin", {"cure_wounds"}));
    const auto fighter = party.add_pc(created("fighter", "Fighter", {}));
    check(action_ids(party, paladin) == std::vector<std::string> {"lay_on_hands", "cure_wounds"},
          "A Paladin offers Lay On Hands and Cure Wounds in camp");
    check(action_ids(party, fighter).empty(), "A Fighter has no camp actions");

    const int full = party.member(fighter).character.sheet().hit_points;
    wound(party, fighter, full - 3);
    party.use_camp_action(paladin, fighter, "lay_on_hands");
    check(hp(party, fighter) == full && remaining(party, paladin, "lay_on_hands") == 2,
          "Lay On Hands restores what is missing and spends only that");

    const auto before = saved(party);
    rejects([&]
    {
        party.use_camp_action(paladin, fighter, "lay_on_hands");
    }, "A member at full HP cannot be healed");
    rejects([&]
    {
        party.use_camp_action(fighter, paladin, "lay_on_hands");
    }, "A Fighter cannot use Lay On Hands");
    check(saved(party) == before, "Refused camp actions change nothing");

    const int own = party.member(paladin).character.sheet().hit_points;
    wound(party, paladin, 1);
    party.use_camp_action(paladin, paladin, "lay_on_hands");
    check(hp(party, paladin) == std::min(own, 1 + 5) &&
          remaining(party, paladin, "lay_on_hands") == unsigned(5 - (hp(party, paladin) - 1)),
          "A Paladin may lay hands on itself");
}

void spell_checks()
{
    CampaignParty party(module());
    const auto cleric = party.add_pc(created("cleric", "Cleric", {"cure_wounds", "healing_word",
                                     "bless", "shield_of_faith"
                                                                 }));
    const auto fighter = party.add_pc(created("fighter", "Fighter", {}));
    check(action_ids(party, cleric) == std::vector<std::string> {"cure_wounds", "healing_word"},
          "A Cleric offers its prepared healing spells, not Bless");
    wound(party, fighter, 1);
    party.use_camp_action(cleric, fighter, "cure_wounds");
    check(hp(party, fighter) > 1 && remaining(party, cleric, "spell_slot:1") == 1,
          "Cure Wounds heals and spends a level-one slot");
    wound(party, fighter, 1);
    party.use_camp_action(cleric, fighter, "healing_word");
    check(remaining(party, cleric, "spell_slot:1") == 0 && action_ids(party, cleric).empty(),
          "Without slots no spell is offered");
    rejects([&]
    {
        party.use_camp_action(cleric, fighter, "cure_wounds");
    }, "A spell needs a slot");

    wound(party, fighter, 0);
    check(hp(party, fighter) == 0, "The Fighter is unconscious");
    CampaignParty refreshed(module());
    refreshed.restore(party.checkpoint());
    auto state = refreshed.checkpoint();
    for (auto &m : state.roster)
        if (m.id == cleric)
            m.vitals = {m.vitals.hit_points, false, ""};
    refreshed.restore(state);
    refreshed.use_camp_action(cleric, fighter, "healing_word");
    check(hp(refreshed, fighter) > 0, "Healing raises an unconscious member");

    const auto bytes = saved(refreshed);
    CampaignParty restored(module());
    restored.restore(decode_campaign(bytes, *srd5::character_rules(), *module(), "camp-actions",
                                     nullptr).party);
    check(saved(restored) == bytes, "A camp heal round-trips through a save");
}
void goodberry_checks()
{
    CampaignParty party(module());
    const auto ranger = party.add_pc(created("ranger", "Ranger", {"goodberry", "longstrider"}));
    const auto fighter = party.add_pc(created("fighter", "Fighter", {}));
    check(action_ids(party, ranger) == std::vector<std::string> {"goodberry"},
          "A Ranger offers Goodberry in camp, not Longstrider");
    const int full = party.member(fighter).character.sheet().hit_points;
    wound(party, fighter, 1);
    party.use_camp_action(ranger, fighter, "goodberry");
    check(hp(party, fighter) == std::min(full, 11), "Goodberry restores up to 10 HP");
    wound(party, fighter, full - 2);
    party.use_camp_action(ranger, fighter, "goodberry");
    check(hp(party, fighter) == full && action_ids(party, ranger).empty(),
          "Goodberry heals only what is missing and spends a slot each time");
}
} // namespace

int main()
{
    try
    {
        lay_on_hands_checks();
        spell_checks();
        goodberry_checks();
        std::cout << "Camp action tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
