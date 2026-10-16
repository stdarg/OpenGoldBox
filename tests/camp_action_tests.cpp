#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/srd5.h"
#include "status_effects.h"
#include "test_support.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

// Healing outside combat from the Camp dialog (CLASS-3), through CampaignParty.
namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

using opengold::test::check;
using opengold::test::rejects;

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
    {
        r = {{6, 5, 4, 1}, 3};
    }
    if (!prepared.empty())
    {
        d.spells = SpellChoices{{}, std::move(prepared), {}, {}};
    }
    if (klass == "paladin")
    {
        d.training = {{"class:paladin", {"athletics", "medicine"}}};
    }
    if (klass == "ranger")
    {
        d.training = {{"class:ranger", {"athletics", "nature", "perception"}}};
    }
    if (klass == "cleric")
    {
        d.cantrips = std::vector<std::string>{"sacred_flame"};
        d.training = {{"class:cleric", {"medicine", "persuasion"}},
                      {"class:cleric:divine_order", {"protector"}}};
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
    {
        ids.push_back(action.id);
    }
    return ids;
}

unsigned remaining(const CampaignParty &party, MemberId id, std::string_view pool)
{
    for (const auto &resource : party.recovery_info(id).resources)
    {
        if (resource.id == pool)
        {
            return resource.remaining;
        }
    }
    return 0;
}

// Sets a member's Hit Points, keeping fresh resources.
void wound(CampaignParty &party, MemberId id, int hit_points)
{
    auto state = party.checkpoint();
    for (auto &m : state.roster)
    {
        if (m.id == id)
        {
            m.vitals = {hit_points, false, ""};
        }
    }
    party.restore(std::move(state));
}

// Sets a member's Hit Points, keeping its resources and effects.
void hurt(CampaignParty &party, MemberId id, int hit_points)
{
    auto state = party.checkpoint();
    for (auto &m : state.roster)
    {
        if (m.id == id)
        {
            m.vitals.hit_points = hit_points;
        }
    }
    party.restore(std::move(state));
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
    check(action_ids(party, paladin) ==
              std::vector<std::string>{"lay_on_hands", "lay_on_hands_cure", "cure_wounds"},
          "A Paladin offers Lay On Hands, its cure and Cure Wounds in camp");
    check(action_ids(party, fighter).empty(), "A Fighter has no camp actions");

    const int full = party.member(fighter).character.sheet().hit_points;
    wound(party, fighter, full - 3);
    party.use_camp_action(paladin, fighter, "lay_on_hands");
    check(hp(party, fighter) == full && remaining(party, paladin, "lay_on_hands") == 2,
          "Lay On Hands restores what is missing and spends only that");

    const auto before = saved(party);
    rejects(
        [&]
        {
            party.use_camp_action(paladin, fighter, "lay_on_hands");
        },
        "A member at full HP cannot be healed");
    rejects(
        [&]
        {
            party.use_camp_action(fighter, paladin, "lay_on_hands");
        },
        "A Fighter cannot use Lay On Hands");
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
    const auto cleric = party.add_pc(
        created("cleric", "Cleric", {"cure_wounds", "healing_word", "bless", "shield_of_faith"}));
    const auto fighter = party.add_pc(created("fighter", "Fighter", {}));
    check(action_ids(party, cleric) == std::vector<std::string>{"cure_wounds", "healing_word"},
          "A Cleric offers its prepared healing spells, not Bless");
    wound(party, fighter, 1);
    party.use_camp_action(cleric, fighter, "cure_wounds");
    check(hp(party, fighter) > 1 && remaining(party, cleric, "spell_slot:1") == 1,
          "Cure Wounds heals and spends a level-one slot");
    wound(party, fighter, 1);
    party.use_camp_action(cleric, fighter, "healing_word");
    check(remaining(party, cleric, "spell_slot:1") == 0 && action_ids(party, cleric).empty(),
          "Without slots no spell is offered");
    rejects(
        [&]
        {
            party.use_camp_action(cleric, fighter, "cure_wounds");
        },
        "A spell needs a slot");

    wound(party, fighter, 0);
    check(hp(party, fighter) == 0, "The Fighter is unconscious");
    CampaignParty refreshed(module());
    refreshed.restore(party.checkpoint());
    auto state = refreshed.checkpoint();
    for (auto &m : state.roster)
    {
        if (m.id == cleric)
        {
            m.vitals = {m.vitals.hit_points, false, ""};
        }
    }
    refreshed.restore(std::move(state));
    refreshed.use_camp_action(cleric, fighter, "healing_word");
    check(hp(refreshed, fighter) > 0, "Healing raises an unconscious member");

    const auto bytes = saved(refreshed);
    CampaignParty restored(module());
    restored.restore(
        decode_campaign(bytes, *srd5::character_rules(), *module(), "camp-actions", nullptr).party);
    check(saved(restored) == bytes, "A camp heal round-trips through a save");
}

void goodberry_checks()
{
    CampaignParty party(module());
    const auto ranger = party.add_pc(created("ranger", "Ranger", {"goodberry", "longstrider"}));
    const auto fighter = party.add_pc(created("fighter", "Fighter", {}));
    check(action_ids(party, ranger) == std::vector<std::string>{"goodberry"},
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

void prayer_of_healing_checks()
{
    CampaignParty party(module());
    const auto cleric = party.add_pc(
        created("cleric", "Cleric", {"cure_wounds", "healing_word", "bless", "shield_of_faith"}));
    party.award_experience(900, "prayer-xp");
    party.advance(cleric, party.default_advancement(cleric));
    auto third = party.default_advancement(cleric);
    third.spells.back() = "prayer_of_healing";
    party.advance(cleric, third);
    std::vector<MemberId> fighters;
    for (const auto *name : {"First", "Second", "Third"})
    {
        fighters.push_back(party.add_pc(created("fighter", name, {})));
    }
    const auto actions = party.camp_actions(cleric);
    const auto prayer = std::find_if(actions.begin(), actions.end(),
                                     [](const auto &a)
                                     {
                                         return a.id == "prayer_of_healing";
                                     });
    check(prayer != actions.end() && prayer->whole_party,
          "Prayer of Healing is a whole-party camp spell");
    wound(party, fighters[0], 1);
    wound(party, fighters[1], 2);
    party.use_camp_action(cleric, cleric, "prayer_of_healing");
    check(hp(party, fighters[0]) > 1 && hp(party, fighters[1]) > 2 &&
              remaining(party, cleric, "spell_slot:2") == 1,
          "Every wounded member regains Hit Points from one level-two slot");
    hurt(party, fighters[0], 1);
    const auto before = saved(party);
    rejects(
        [&]
        {
            party.use_camp_action(cleric, cleric, "prayer_of_healing");
        },
        "A member healed by Prayer of Healing waits for a Long Rest");
    check(saved(party) == before, "The refused prayer changes nothing");
    (void)party.rest(RestKind::long_rest);
    if (party.state().spell_rest)
    {
        party.keep_rest_spells(cleric);
    }
    hurt(party, fighters[0], 1);
    party.use_camp_action(cleric, cleric, "prayer_of_healing");
    check(hp(party, fighters[0]) > 1, "After a Long Rest the prayer heals again");
}

// Aid in camp: the three members with the lowest Hit Point maximum not
// already aided each gain 5 to it and to their Hit Points (AID-1).
void aid_checks()
{
    CampaignParty party(module());
    const auto cleric = party.add_pc(
        created("cleric", "Cleric", {"cure_wounds", "healing_word", "bless", "shield_of_faith"}));
    party.award_experience(900, "aid-xp");
    party.advance(cleric, party.default_advancement(cleric));
    // The Life Domain's Aid is always prepared from level three.
    party.advance(cleric, party.default_advancement(cleric));
    std::vector<MemberId> fighters;
    for (const auto *name : {"First", "Second", "Third"})
    {
        fighters.push_back(party.add_pc(created("fighter", name, {})));
    }
    // A level-one Cleric has the lowest maximum.
    const auto low = party.add_pc(
        created("cleric", "Low", {"cure_wounds", "healing_word", "bless", "shield_of_faith"}));
    const auto actions = party.camp_actions(cleric);
    check(std::any_of(actions.begin(), actions.end(),
                      [](const auto &a)
                      {
                          return a.id == "aid" && a.whole_party;
                      }),
          "Aid is a whole-party camp spell");
    std::map<MemberId, int> before;
    for (const auto id : {cleric, fighters[0], fighters[1], fighters[2], low})
    {
        before[id] = party.hit_point_maximum(id);
    }
    check(before[low] < before[fighters[0]] && before[fighters[2]] < before[cleric],
          "The level-one Cleric's maximum is lowest, the level-three Cleric's highest");
    party.use_camp_action(cleric, cleric, "aid");
    check(party.hit_point_maximum(low) == before[low] + 5 && hp(party, low) == before[low] + 5 &&
              party.hit_point_maximum(fighters[0]) == before[fighters[0]] + 5 &&
              party.hit_point_maximum(fighters[1]) == before[fighters[1]] + 5 &&
              party.hit_point_maximum(fighters[2]) == before[fighters[2]] &&
              party.hit_point_maximum(cleric) == before[cleric] &&
              remaining(party, cleric, "spell_slot:2") == 1,
          "Aid raises the three lowest maximums by 5 from one level-two slot");
    party.use_camp_action(cleric, cleric, "aid");
    check(party.hit_point_maximum(fighters[2]) == before[fighters[2]] + 5 &&
              party.hit_point_maximum(cleric) == before[cleric] + 5 &&
              party.hit_point_maximum(low) == before[low] + 5,
          "A second Aid passes over the members already aided");
}

// Lay On Hands spends 5 points to end Poisoned in camp too.
void lay_on_hands_cure_checks()
{
    namespace fx = opengold::srd5::detail;
    CampaignParty party(module());
    const auto paladin = party.add_pc(created("paladin", "Paladin", {"cure_wounds"}));
    const auto fighter = party.add_pc(created("fighter", "Fighter", {}));
    // A fight gives the Fighter a complete vital record; then it is Poisoned.
    Encounter e{{12, 6, std::vector<Terrain>(72)}, party.participants()};
    e.participants.push_back({99, "bandit", "Bandit", Side::opposition, {10, 5}});
    auto fight = party.rule_module().create(e, 3);
    party.begin_combat();
    party.apply_combat(fight->snapshot());
    party.end_combat();
    auto state = party.checkpoint();
    auto &vitals = state.roster.at(1).vitals;
    const auto at = vitals.resources.find("FX8");
    std::istringstream in(vitals.resources.substr(at));
    auto effects = fx::read_effects(in);
    fx::apply_poisoned(effects, {.scope = 9, .actor = 99}, "Bandit", 12000);
    std::ostringstream out;
    fx::write_effects(out, effects);
    vitals.resources.replace(at, std::string::npos, out.str());
    party.restore(std::move(state));
    const auto poisoned = [&]
    {
        const auto &resources = party.member(fighter).vitals.resources;
        std::istringstream read(resources.substr(resources.find("FX8")));
        const auto active = fx::read_effects(read).active;
        return std::any_of(active.begin(), active.end(),
                           [](const auto &effect)
                           {
                               return effect.kind == fx::EffectKind::poisoned;
                           });
    };
    check(poisoned(), "The Fighter is Poisoned");
    party.use_camp_action(paladin, fighter, "lay_on_hands_cure");
    check(!poisoned() && remaining(party, paladin, "lay_on_hands") == 0,
          "The cure ends Poisoned for 5 points");
    bool refused = false;
    try
    {
        party.use_camp_action(paladin, paladin, "lay_on_hands_cure");
    }
    catch (const std::exception &)
    {
        refused = true;
    }
    check(refused, "With the pool spent, the cure is no longer offered");
}

} // namespace

int main()
{
    try
    {
        lay_on_hands_checks();
        lay_on_hands_cure_checks();
        spell_checks();
        goodberry_checks();
        prayer_of_healing_checks();
        aid_checks();
        std::cout << "Camp action tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
