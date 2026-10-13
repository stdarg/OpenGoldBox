#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

// Magic Initiate, SRD 5.2.1 p. 87: two cantrips and a level-one spell from the
// Cleric, Druid or Wizard list, cast with the ability chosen for the feat; the
// level-one spell once per Long Rest without a slot, or with any slot.
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

using Picks = std::vector<std::string>;

// A level-one character of the class and background. Every roll is 15; the
// background raises Wisdom by 2 and Charisma by 1 where it can, so Wisdom (+3)
// and Intelligence (+2) give different spell attack bonuses.
CharacterDraft draft(std::string klass, std::string background)
{
    CharacterDraft d;
    d.race = "orc";
    d.gender = "female";
    d.character_class = std::move(klass);
    d.background = std::move(background);
    d.alignment = "neutral_good";
    d.name = "Initiate";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    const auto adjustments = srd5::character_rules()->adjustments(d.background);
    for (unsigned n = 0; n < adjustments.size(); ++n)
        if (adjustments[n].bonuses[Ability::wisdom] == 2 &&
                adjustments[n].bonuses[Ability::charisma] == 1)
            d.adjustment = n;
    if (d.character_class == "fighter")
        d.training = {{"class:fighter", {"acrobatics", "history"}},
            {"class:fighter:fighting_style", {"defense"}}
        };
    return d;
}

// A Fighter Acolyte whose Magic Initiate (Cleric) chose these spells and ability.
Character acolyte(Picks cantrips = {"sacred_flame", "guidance"},
                  std::string spell = "guiding_bolt", std::string ability = "wisdom")
{
    auto d = draft("fighter", "acolyte");
    d.spells = SpellChoices{{{"feat:magic_initiate_cleric:ability", {ability}},
            {"feat:magic_initiate_cleric:cantrips", cantrips},
            {"feat:magic_initiate_cleric:spell", {spell}}
        }, {}, {}, {}};
    return Character(*srd5::character_rules(), d, {});
}

const TrainingChoiceGroup *group(const SpellChoiceOptions &options, std::string_view id)
{
    const auto found = std::find_if(options.learning.begin(), options.learning.end(),
                                    [&](const auto & g)
    {
        return g.id == id;
    });
    return found == options.learning.end() ? nullptr : &*found;
}

bool offers(const TrainingChoiceGroup &group, std::string_view id)
{
    return std::any_of(group.options.begin(), group.options.end(), [&](const auto & o)
    {
        return o.id == id;
    });
}

bool has_grant(const CharacterSheet &sheet, const FeatureGrant &grant)
{
    return std::find(sheet.grants.begin(), sheet.grants.end(), grant) != sheet.grants.end();
}

bool offered(const CombatSession &c, std::string_view verb)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(), [&](const auto & command)
    {
        return command.verb == verb;
    });
}

bool submit(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && (!target || command.target == target))
            return c.submit(command);
    return false;
}

// The initiate (1) at (1,1) and an enemy (99), on the initiate's turn.
std::unique_ptr<CombatSession> battle(const RulesModule &rules, const CharacterSheet &sheet,
                                      std::optional<VitalState> state = std::nullopt,
                                      std::uint64_t seed = 5, Cell enemy = {3, 1})
{
    const auto profile = rules.character_profile(sheet, {}).data;
    auto c = rules.create({{12, 6, std::vector<Terrain>(72)},
        {   {1, "campaign-character", "Initiate", Side::party, {1, 1}, profile, state},
            {99, "vanguard", "Target", Side::opposition, enemy}
        }},
    seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*c, "end"), "Reach the initiate's turn");
    check(c->snapshot().actor == 1, "The initiate acts");
    return c;
}

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log();
    return std::any_of(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

CombatantView unit(const CombatSession &c, EntityId id)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing combatant");
}

// The free cast's pool for a spell: remaining, or -1 when there is none.
int free_cast(const std::vector<ResourcePool> &pools, std::string_view spell)
{
    for (const auto &pool : pools)
        if (pool.id == "magic_initiate:" + std::string(spell))
            return static_cast<int>(pool.remaining);
    return -1;
}

void creation_checks()
{
    auto creation = srd5::character_rules();
    auto d = draft("fighter", "acolyte");
    const auto options = creation->spell_choice_options(d);
    const auto *ability = group(options, "feat:magic_initiate_cleric:ability");
    const auto *cantrips = group(options, "feat:magic_initiate_cleric:cantrips");
    const auto *spell = group(options, "feat:magic_initiate_cleric:spell");
    check(options.learning.size() == 3 && ability && cantrips && spell && !options.may_prepare,
          "A Fighter Acolyte learns Magic Initiate (Cleric) spells and prepares nothing");
    check(ability->count == 1 && ability->options.size() == 3 &&
          offers(*ability, "intelligence") && offers(*ability, "wisdom") &&
          offers(*ability, "charisma"),
          "The feat's ability is Intelligence, Wisdom or Charisma");
    check(cantrips->count == 2 && offers(*cantrips, "sacred_flame") &&
          offers(*cantrips, "guidance") && !offers(*cantrips, "fire_bolt"),
          "Two cantrips from the Cleric list only");
    check(spell->count == 1 && offers(*spell, "guiding_bolt") && offers(*spell, "bless") &&
          !offers(*spell, "magic_missile") && !offers(*spell, "aid"),
          "One level-one spell from the Cleric list only");
    check(creation->spell_choice_options(draft("fighter", "soldier")).learning.empty(),
          "A Soldier brings no spells");
    check(group(creation->spell_choice_options(draft("rogue", "sage")),
                "feat:magic_initiate_wizard:spell"),
          "A Sage brings Magic Initiate (Wizard)");

    const auto sheet = acolyte().sheet();
    check(has_grant(sheet, {"feat:magic_initiate_cleric", "background:acolyte", 1, {}}) &&
          has_grant(sheet, {"spellcasting_ability:wisdom", "feat:magic_initiate_cleric", 1, {}}) &&
          has_grant(sheet, {"spell:sacred_flame", "feat:magic_initiate_cleric", 1,
                            {{"access", "cantrip"}}}) &&
          has_grant(sheet, {"spell:guiding_bolt", "feat:magic_initiate_cleric", 1,
                            {{"access", "always_prepared"}}}),
          "The background's feat and each pick are sourced grants");
    const auto rules = module();
    const auto access = rules->spell_access(sheet);
    check(access.cantrips.empty() && access.initiate.size() == 3 &&
          access.always_prepared == std::vector<std::string> {"guiding_bolt"},
          "The feat's spells stand apart from class spells; its level-one spell is always prepared");

    rejects([&]
    {
        (void)acolyte({"sacred_flame", "guidance", "resistance"});
    }, "Only two cantrips");
    rejects([&]
    {
        (void)acolyte({"sacred_flame", "fire_bolt"});
    }, "No cantrip from another list");
    rejects([&]
    {
        (void)acolyte({"sacred_flame", "guidance"}, "magic_missile");
    }, "No level-one spell from another list");
    rejects([&]
    {
        (void)acolyte({"sacred_flame", "guidance"}, "guiding_bolt", "strength");
    }, "Strength is not a spellcasting ability");

    // A Cleric Acolyte cannot learn one cantrip twice through class and feat.
    auto cleric = draft("cleric", "acolyte");
    cleric.cantrips = std::vector<std::string> {"sacred_flame", "spare_the_dying", "resistance"};
    check(!offers(*group(creation->spell_choice_options(cleric),
                         "feat:magic_initiate_cleric:cantrips"),
                  "sacred_flame"),
          "Magic Initiate does not offer a cantrip the class already teaches");
    cleric.spells = SpellChoices{{{"feat:magic_initiate_cleric:cantrips", {"sacred_flame"}}},
        std::vector<std::string> {}, {}, {}};
    rejects([&]
    {
        (void)Character(*creation, cleric, {});
    }, "A cantrip learned twice is rejected");
}

void combat_checks()
{
    const auto rules = module();
    const auto sheet = acolyte().sheet();
    auto c = battle(*rules, sheet);
    check(offered(*c, "sacred_flame") && offered(*c, "guiding_bolt_initiate") &&
          !offered(*c, "guiding_bolt"),
          "A Fighter casts the cantrip, and the level-one spell only with the free cast");
    check(free_cast(unit(*c, 1).resources, "guiding_bolt") == 1, "The free cast is shown full");
    const auto commands = c->legal_commands();
    auto forged = *std::find_if(commands.begin(), commands.end(), [](const auto & command)
    {
        return command.verb == "guiding_bolt_initiate";
    });
    forged.target = 1;
    check(!c->submit(forged) && free_cast(unit(*c, 1).resources, "guiding_bolt") == 1,
          "A rejected cast keeps the free cast");
    check(submit(*c, "guiding_bolt_initiate", 99), "Cast Guiding Bolt without a slot");
    // Wisdom 17 (+3) and the +2 Proficiency Bonus; the class's Intelligence would give +4.
    check(logged(*c, "Initiate -> Target: d20 ") && logged(*c, " + 5 vs AC"),
          "Its spell attack uses the feat's Wisdom");
    check(!offered(*c, "guiding_bolt_initiate") &&
          free_cast(unit(*c, 1).resources, "guiding_bolt") == 0,
          "One free cast per Long Rest");
    const auto spent = unit(*c, 1).persistent;
    const auto restored = rules->restore(c->save());
    check(free_cast(unit(*restored, 1).resources, "guiding_bolt") == 0,
          "A combat checkpoint keeps the spent free cast");
    auto again = battle(*rules, sheet, spent);
    check(!offered(*again, "guiding_bolt_initiate") && offered(*again, "sacred_flame"),
          "The spent free cast stays spent in the next fight");
    auto rested = spent;
    rules->recover(rested, sheet);
    check(free_cast(rules->recovery_info(sheet, rested).resources, "guiding_bolt") == 1 &&
          offered(*battle(*rules, sheet, rested), "guiding_bolt_initiate"),
          "A Long Rest restores the free cast");

    // Sacred Flame: DC 8 + Wisdom +3 + Proficiency +2.
    auto save = battle(*rules, sheet);
    check(submit(*save, "sacred_flame", 99) && logged(*save, "vs DC 13"),
          "The feat's cantrip uses the feat's ability for its save DC");
}

// A caster spends a slot on its own turn and may still use the free cast: the
// one-slot-a-turn rule counts slots, not spells.
void slot_rule_checks()
{
    const auto rules = module();
    auto d = draft("cleric", "acolyte");
    d.training = {{"class:cleric", {"medicine", "persuasion"}},
        {"class:cleric:divine_order", {"protector"}}
    };
    d.cantrips = std::vector<std::string> {"sacred_flame", "spare_the_dying", "resistance"};
    d.spells = SpellChoices{{{"feat:magic_initiate_cleric:ability", {"wisdom"}},
            {"feat:magic_initiate_cleric:cantrips", {"guidance"}},
            {"feat:magic_initiate_cleric:spell", {"healing_word"}}
        },
        std::vector<std::string> {"guiding_bolt", "bless", "cure_wounds", "shield_of_faith"}, {}, {}};
    const Character cleric(*srd5::character_rules(), d, {});
    const auto preparation = rules->spell_choice_options(cleric.sheet(),
                             SpellChoiceContext::long_rest).preparation;
    check(std::none_of(preparation.begin(), preparation.end(), [](const auto & o)
    {
        return o.id == "healing_word";
    }), "The feat's spell is always prepared, outside the Cleric's preparation");
    VitalState hurt{cleric.sheet().hit_points - 4, false, {}};
    auto c = battle(*rules, cleric.sheet(), hurt);
    check(offered(*c, "healing_word") && offered(*c, "healing_word_initiate"),
          "With a slot left, the feat's spell is cast from a slot or for free");
    check(submit(*c, "guiding_bolt", 99), "Guiding Bolt spends the turn's slot");
    check(!offered(*c, "healing_word") && offered(*c, "healing_word_initiate"),
          "After the turn's slot, only the free cast remains");
    const auto wounded = unit(*c, 1).hit_points;
    check(submit(*c, "healing_word_initiate", 1) && unit(*c, 1).hit_points > wounded &&
          free_cast(unit(*c, 1).resources, "healing_word") == 0,
          "The Bonus Action Healing Word heals with the free cast");
}

// Shield's reaction uses the free cast when no slot pays for it.
void shield_checks()
{
    const auto rules = module();
    auto d = draft("fighter", "sage");
    d.spells = SpellChoices{{{"feat:magic_initiate_wizard:ability", {"intelligence"}},
            {"feat:magic_initiate_wizard:cantrips", {"fire_bolt", "ray_of_frost"}},
            {"feat:magic_initiate_wizard:spell", {"shield"}}
        }, {}, {}, {}};
    const Character fighter(*srd5::character_rules(), d, {});
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(*rules, fighter.sheet(), std::nullopt, seed, {2, 1});
        check(submit(*c, "end"), "The Fighter waits");
        for (unsigned turns = 0; c->snapshot().actor != 99 && turns < 4; ++turns)
            check(submit(*c, "end"), "Reach the enemy's turn");
        check(submit(*c, "melee", 1), "The enemy attacks the Fighter");
        if (!c->snapshot().reaction_pending || !offered(*c, "shield"))
            continue;
        check(submit(*c, "shield") && logged(*c, "casts Shield"),
              "Shield is cast as a reaction");
        check(free_cast(unit(*c, 1).resources, "shield") == 0, "It spends the free cast");
        return;
    }
    throw std::runtime_error("No seed lets the enemy hit the Fighter");
}

void level_four_checks()
{
    auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(acolyte());
    party.award_experience(2700, "initiate-xp");
    for (unsigned level = 1; level < 3; ++level)
        party.advance(id, party.default_advancement(id));
    const auto options = party.advancement_options(id);
    const auto feat = [&](std::string_view name)
    {
        return std::find_if(options.feats.begin(), options.feats.end(), [&](const auto & f)
        {
            return f.id == name;
        });
    };
    check(feat("magic_initiate_cleric") != options.feats.end() &&
          !feat("magic_initiate_cleric")->available &&
          feat("magic_initiate_druid")->available && feat("magic_initiate_wizard")->available,
          "Magic Initiate repeats only with another spell list");
    auto choice = party.default_advancement(id);
    choice.feat = "magic_initiate_druid";
    choice.abilities = {};
    const auto &sheet = party.member(id).character.sheet();
    const auto learning =
        rules->spell_choice_options(rules->spell_choice_sheet(sheet, choice),
                                    SpellChoiceContext::advancement);
    check(group(learning, "feat:magic_initiate_druid:ability") &&
          group(learning, "feat:magic_initiate_druid:cantrips")->acquired_level == 4 &&
          offers(*group(learning, "feat:magic_initiate_druid:spell"), "entangle") &&
          !group(learning, "feat:magic_initiate_cleric:spell"),
          "The level-four feat offers its own list's spells");
    rejects([&]
    {
        party.advance(id, choice);
    }, "The feat's spells must be chosen");
    choice.spell_learning = TrainingChoices{{"feat:magic_initiate_druid:ability", {"charisma"}},
        {"feat:magic_initiate_druid:cantrips", {"produce_flame", "starry_wisp"}},
        {"feat:magic_initiate_druid:spell", {"entangle"}}
    };
    auto twice = choice;
    twice.feat = "magic_initiate_cleric";
    rejects([&]
    {
        party.advance(id, twice);
    }, "The same list cannot be taken twice");
    party.advance(id, choice);
    const auto &leveled = party.member(id).character.sheet();
    check(leveled.level == 4 &&
          has_grant(leveled, {"feat:magic_initiate_druid", "class:fighter:ability_score_improvement",
                              4, {}}) &&
          has_grant(leveled, {"spell:entangle", "feat:magic_initiate_druid", 4,
                              {{"access", "always_prepared"}}}),
          "Level four grants Magic Initiate (Druid) and its picks");
    const auto pools = rules->recovery_info(leveled, party.member(id).vitals).resources;
    check(free_cast(pools, "guiding_bolt") == 1 && free_cast(pools, "entangle") == 1,
          "Each feat has its own free cast");
    auto c = battle(*rules, leveled, party.member(id).vitals);
    check(offered(*c, "produce_flame") && offered(*c, "guiding_bolt_initiate") &&
          offered(*c, "entangle_initiate"),
          "Both feats' spells are castable");
    const auto saved = encode_campaign(party, nullptr, "initiate");
    check(saved.find("feat:magic_initiate_druid") != std::string::npos,
          "The campaign save records the feat");
}

// Lessons of the First Ones grants an Origin feat, Magic Initiate among them.
void lessons_checks()
{
    auto rules = module();
    auto d = draft("warlock", "acolyte");
    d.training = {{"class:warlock", {"arcana", "deception"}},
        {"class:warlock:invocations", {"eldritch_mind"}}
    };
    d.cantrips = std::vector<std::string> {"eldritch_blast", "chill_touch"};
    d.spells = SpellChoices{{{"feat:magic_initiate_cleric:ability", {"charisma"}},
            {"feat:magic_initiate_cleric:cantrips", {"sacred_flame", "guidance"}},
            {"feat:magic_initiate_cleric:spell", {"bless"}}
        },
        std::vector<std::string> {"hex", "hellish_rebuke"}, {}, {}};
    CampaignParty party(module());
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(2700, "lessons-xp");
    const auto options = party.advancement_options(id).training.front().options;
    const auto invocation = [&](std::string_view name)
    {
        return std::any_of(options.begin(), options.end(), [&](const auto & o)
        {
            return o.id == name;
        });
    };
    check(invocation("lessons_magic_initiate_druid") &&
          invocation("lessons_magic_initiate_wizard") &&
          !invocation("lessons_magic_initiate_cleric"),
          "Lessons offers Magic Initiate for each list the Acolyte does not already have");
    auto choice = party.default_advancement(id);
    choice.training["class:warlock:invocations:2"] = {"lessons_magic_initiate_wizard",
                                                      "devils_sight"
                                                     };
    const auto &sheet = party.member(id).character.sheet();
    check(group(rules->spell_choice_options(rules->spell_choice_sheet(sheet, choice),
                                            SpellChoiceContext::advancement),
                "feat:magic_initiate_wizard:spell"),
          "Choosing it offers the Wizard list's spells at level two");
    (*choice.spell_learning)["feat:magic_initiate_wizard:ability"] = {"charisma"};
    (*choice.spell_learning)["feat:magic_initiate_wizard:cantrips"] = {"fire_bolt", "ray_of_frost"};
    (*choice.spell_learning)["feat:magic_initiate_wizard:spell"] = {"shield"};
    party.advance(id, choice);
    const auto &leveled = party.member(id).character.sheet();
    check(has_grant(leveled, {"invocation:lessons_magic_initiate_wizard",
                              "class:warlock:invocations:2", 2, {}}) &&
          has_grant(leveled, {"spell:shield", "feat:magic_initiate_wizard", 2,
                              {{"access", "always_prepared"}}}),
          "The invocation grants Magic Initiate (Wizard) and its picks");
    const auto pools = rules->recovery_info(leveled, party.member(id).vitals).resources;
    check(free_cast(pools, "bless") == 1 && free_cast(pools, "shield") == 1,
          "The background's and the invocation's feats each have a free cast");
}

void camp_checks()
{
    const auto rules = module();
    const auto healer = acolyte({"sacred_flame", "guidance"}, "cure_wounds").sheet();
    VitalState state{healer.hit_points - 6, false, {}};
    const auto actions = rules->camp_actions(healer, state);
    check(std::any_of(actions.begin(), actions.end(), [](const auto & a)
    {
        return a.id == "cure_wounds_initiate";
    }) && std::none_of(actions.begin(), actions.end(), [](const auto & a)
    {
        return a.id == "cure_wounds";
    }), "At camp a Fighter casts Cure Wounds only with the free cast");
    RandomState random{11};
    const auto before = state.hit_points;
    rules->use_camp_action(healer, state, healer, state, "cure_wounds_initiate", random);
    check(state.hit_points > before, "The free Cure Wounds heals");
    const auto after = rules->camp_actions(healer, state);
    check(std::none_of(after.begin(), after.end(), [](const auto & a)
    {
        return a.id == "cure_wounds_initiate";
    }), "The free cast is spent until a Long Rest");
}

// Tampered grants are rejected wherever a profile is built from them.
void provenance_checks()
{
    const auto rules = module();
    const auto sheet = acolyte().sheet();
    (void)rules->character_profile(sheet, {});
    const auto tampered = [&](auto change)
    {
        auto copy = sheet;
        change(copy.grants);
        rejects([&]
        {
            (void)rules->character_profile(copy, {});
        }, "Tampered Magic Initiate grants are rejected");
    };
    tampered([](auto & grants)
    {
        grants.push_back({"spell:resistance", "feat:magic_initiate_cleric", 1, {{"access", "cantrip"}}});
    });
    tampered([](auto & grants)
    {
        grants.push_back({"spell:shield", "feat:magic_initiate_wizard", 1,
                          {{"access", "always_prepared"}}});
    });
    tampered([](auto & grants)
    {
        grants.push_back({"spellcasting_ability:charisma", "feat:magic_initiate_cleric", 1, {}});
    });
    tampered([](auto & grants)
    {
        std::erase_if(grants, [](const auto & g)
        {
            return g.id == "feat:magic_initiate_cleric";
        });
    });
}
} // namespace

int main()
{
    try
    {
        creation_checks();
        combat_checks();
        slot_rule_checks();
        shield_checks();
        level_four_checks();
        lessons_checks();
        camp_checks();
        provenance_checks();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
