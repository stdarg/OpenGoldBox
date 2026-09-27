#include "opengold/campaign_save.h"
#include "opengold/character_creator.h"
#include "opengold/character_pool.h"
#include "opengold/srd5.h"
#include "combat_fixture.h"
#include "campaign_fixture.h"
#include <algorithm>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <map>
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
    bool caught = false;
    try
    {
        f();
    }
    catch (const std::exception &)
    {
        caught = true;
    }
    check(caught, "Invalid training must reject");
}

auto module()
{
    return srd5::load(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "data/rules/srd-5.2.1/combat.rules");
}

CharacterDraft draft(std::string klass = "rogue", std::string background = "criminal")
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = klass;
    d.background = background;
    d.alignment = "neutral_good";
    d.name = "Training tester";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    return d;
}

TrainingChoices choices()
{
    return {{"origin:languages", {"elvish", "dwarvish"}},
        {"class:rogue", {"acrobatics", "investigation", "perception", "persuasion"}},
        {"class:rogue:expertise", {"stealth", "perception"}},
        {"class:rogue:thieves_cant", {"undercommon"}},
        {"class:rogue:weapon_mastery", {"dagger", "shortbow"}}};
}

void complete_mastery(CharacterDraft &d)
{
    for (const auto &group : srd5::character_rules()->training_options(d))
        if (group.id.ends_with(":weapon_mastery"))
        {
            auto &selected = d.training[group.id];
            for (const auto &option : group.options)
                if (selected.size() < group.count &&
                        std::find(selected.begin(), selected.end(), option.id) == selected.end())
                    selected.push_back(option.id);
        }
}

Character hero(const CharacterDraft &d)
{
    return Character(*srd5::character_rules(), d, {});
}

const SkillTraining &skill(const CharacterSheet &sheet, std::string_view id)
{
    const auto found = std::find_if(sheet.training.skills.begin(), sheet.training.skills.end(),
                                    [&](const auto & s)
    {
        return s.id == id;
    });
    check(found != sheet.training.skills.end(), "Skill exists");
    return *found;
}

std::string fixture(const char *name)
{
    std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures" / name);
    check(bool(in), "Frozen fixture exists");
    return {std::istreambuf_iterator<char>(in), {}};
}

void replace(std::string &text, std::string_view from, std::string_view to)
{
    const auto pos = text.find(from);
    check(pos != text.npos, "Fixture field exists");
    text.replace(pos, from.size(), to);
}

std::string corrupt(std::string bytes, std::string_view from, std::string_view to)
{
    auto body = bytes.substr(bytes.find('\n', bytes.find('\n') + 1) + 1);
    replace(body, from, to);
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : body)
    {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    return bytes.substr(0, bytes.find('\n') + 1) + std::to_string(hash) + '\n' + body;
}

// Complete newly owed advancement choices alongside the historical feature under test.
TrainingChoices with_advancement_training(const Character &character,
        const CharacterRules &creation, const RulesModule &rules,
        TrainingChoices choices)
{
    for (const auto &advancement : character.advancements())
        for (const auto &[id, values] : advancement.training)
            choices.emplace(id, values);
    auto draft = character.creation_data();
    draft.training = choices;
    for (const auto &group : creation.training_options(draft))
        if (group.id.ends_with(":weapon_mastery") && !choices.contains(group.id))
            for (unsigned i = 0; i < group.count; ++i)
                choices[group.id].push_back(group.options.at(i).id);
    const auto candidate = character.preview_training(creation, rules, choices, false);
    for (const auto &group : rules.training_options(candidate.sheet()))
        if (!choices.contains(group.id))
        {
            check(group.options.size() >= group.count,
                  "Historical training supplies eligible advancement choices");
            for (unsigned i = 0; i < group.count; ++i)
                choices[group.id].push_back(group.options[i].id);
        }
    return choices;
}

bool preserved_advancement(const Character &after, const Character &before)
{
    auto actual = after.advancements();
    const auto &expected = before.advancements();
    if (actual.size() != expected.size())
        return false;
    for (unsigned i = 0; i < actual.size(); ++i)
    {
        for (const auto &[id, values] : expected[i].training)
            if (!actual[i].training.contains(id) || actual[i].training.at(id) != values)
                return false;
        actual[i].training = expected[i].training;
    }
    return actual == expected;
}

std::vector<std::string> expected_class_skills(std::string_view klass)
{
    const std::map<std::string_view, std::string_view> lists
    {
        {"barbarian", "animal_handling athletics intimidation nature perception survival"},
        {
            "bard",
            "acrobatics animal_handling arcana athletics deception history insight intimidation investigation medicine nature perception performance persuasion religion sleight_of_hand stealth survival"
        },
        {"cleric", "history insight medicine persuasion religion"},
        {"druid", "animal_handling arcana insight medicine nature perception religion survival"},
        {
            "fighter",
            "acrobatics animal_handling athletics history insight intimidation perception persuasion survival"
        },
        {"monk", "acrobatics athletics history insight religion stealth"},
        {"paladin", "athletics insight intimidation medicine persuasion religion"},
        {
            "ranger",
            "animal_handling athletics insight investigation nature perception stealth survival"
        },
        {
            "rogue",
            "acrobatics athletics deception insight intimidation investigation perception persuasion sleight_of_hand stealth"
        },
        {"sorcerer", "arcana deception insight intimidation persuasion religion"},
        {"warlock", "arcana deception history intimidation investigation nature religion"},
        {"wizard", "arcana history insight investigation medicine nature religion"}};
    std::istringstream in{std::string(lists.at(klass))};
    std::vector<std::string> result;
    for (std::string id; in >> id;)
        result.push_back(id);
    return result;
}

unsigned expected_skill_count(std::string_view klass)
{
    return klass == "rogue" ? 4 : klass == "bard" || klass == "ranger" ? 3 : 2;
}

std::vector<std::string> chosen_class_skills(std::string_view klass)
{
    auto list = expected_class_skills(klass);
    list.resize(expected_skill_count(klass));
    return list;
}

void all_class_skills()
{
    auto creation = srd5::character_rules();
    auto rules = module();
    for (const auto &klass : creation->choices(CreationField::character_class))
        for (const auto bg :
                {"acolyte", "criminal", "sage", "soldier"
                })
        {
            auto d = draft(klass.id, bg);
            d.training = {{"origin:languages", {"elvish", "dwarvish"}}};
            if (klass.id == "rogue")
            {
                d.training = choices();
                d.training["class:rogue:expertise"] = {"investigation", "perception"};
            }
            if (klass.id == "fighter")
                d.training["class:fighter:fighting_style"] = {"defense"};
            if (klass.id == "bard")
                d.training["class:bard:instruments"] = {"flute", "lute", "viol"};
            if (klass.id == "monk")
                d.training["class:monk:tools"] = {"flute"};
            if (std::string_view(bg) == "soldier")
                d.training["background:soldier:gaming_set"] = {"dice"};
            const auto groups = creation->training_options(d);
            const auto found = std::find_if(groups.begin(), groups.end(),
                                            [&](const auto & g)
            {
                return g.id == "class:" + klass.id;
            });
            check(found != groups.end() && found->control == TrainingChoiceControl::checkboxes &&
                  found->count == expected_skill_count(klass.id),
                  "All twelve class counts and existing checkbox controls match SRD Core Traits");
            std::vector<std::string> offered;
            for (const auto &o : found->options)
                offered.push_back(o.id);
            std::sort(offered.begin(), offered.end());
            auto expected = expected_class_skills(klass.id);
            std::sort(expected.begin(), expected.end());
            check(offered == expected, "Exact class skill list includes no omissions or additions");
            if (klass.id != "rogue")
                check(!hero(d).sheet().training.complete,
                      "New class skill choices are required, not invented");
            complete_mastery(d);
            for (const auto &first : expected)
            {
                auto selected = chosen_class_skills(klass.id);
                if (std::find(selected.begin(), selected.end(), first) == selected.end())
                    selected.back() = first;
                d.training["class:" + klass.id] = selected;
                if (klass.id == "rogue")
                    d.training["class:rogue:expertise"] = {selected[0], selected[1]};
                const auto h = hero(d);
                check(h.sheet().training.complete,
                      "Every legal class option can complete Training");
                for (const auto &id : selected)
                {
                    const auto &item = skill(h.sheet(), id);
                    check(item.proficient, "Selected class skill gains proficiency");
                    check(std::find(h.sheet().grants.begin(), h.sheet().grants.end(),
                                    FeatureGrant{"skill:" + id, "class:" + klass.id, 1, {}}) !=
                          h.sheet().grants.end(),
                          "Class skill has exact level-one provenance");
                    check(item.bonus ==
                          h.sheet().modifiers[item.ability] + (item.expertise ? 4 : 2),
                          "Overlapping background and class grants add proficiency once");
                }
                auto bad = d;
                bad.training["class:" + klass.id][1] = selected.front();
                rejects(
                    [&]
                {
                    (void)hero(bad);
                });
                bad = d;
                for (const auto &id : expected)
                    if (std::find(selected.begin(), selected.end(), id) == selected.end())
                    {
                        bad.training["class:" + klass.id].push_back(id);
                        break;
                    }
                rejects(
                    [&]
                {
                    (void)hero(bad);
                });
                bad = d;
                bad.training["class:" + klass.id] = {"unknown_skill"};
                rejects(
                    [&]
                {
                    (void)hero(bad);
                });
                if (klass.id != "bard")
                {
                    bad = d;
                    for (const auto &id : expected_class_skills("bard"))
                        if (std::find(expected.begin(), expected.end(), id) == expected.end())
                        {
                            bad.training["class:" + klass.id][0] = id;
                            break;
                        }
                    rejects(
                        [&]
                    {
                        (void)hero(bad);
                    });
                }
                auto forged = h.sheet();
                for (auto &grant : forged.grants)
                    if (grant.source_id == "class:" + klass.id && grant.id.starts_with("skill:"))
                    {
                        grant.level = 2;
                        break;
                    }
                rejects(
                    [&]
                {
                    (void)rules->character_profile(forged, {});
                });
                CampaignParty party(module());
                party.add_pc(h);
                const auto bytes = encode_campaign(party, nullptr, "class-skills-new");
                CampaignParty copy(module());
                copy.restore(
                    decode_campaign(bytes, *creation, *rules, "class-skills-new", nullptr).party);
                check(encode_campaign(copy, nullptr, "class-skills-new") == bytes,
                      "All-class skill choices save and reload canonically");
                if (klass.id != "rogue")
                    rejects(
                        [&]
                {
                    (void)decode_campaign(
                    corrupt(bytes, module()->identity().version, "0.6.29"), *creation,
                    *rules, "class-skills-new", nullptr);
                });
            }
        }
    for (const auto &from : creation->choices(CreationField::character_class))
        for (const auto &to : creation->choices(CreationField::character_class))
        {
            CharacterCreator creator(srd5::character_rules(), 42);
            creator.select(CreationField::character_class, from.id);
            auto expected = chosen_class_skills(from.id);
            for (const auto &id : expected)
                creator.training_choice("class:" + from.id, id, true);
            const auto allowed = expected_class_skills(to.id);
            std::erase_if(expected,
                          [&](const auto & id)
            {
                return std::find(allowed.begin(), allowed.end(), id) == allowed.end();
            });
            if (expected.size() > expected_skill_count(to.id))
                expected.resize(expected_skill_count(to.id));
            creator.select(CreationField::character_class, to.id);
            const auto found = creator.draft().training.find("class:" + to.id);
            check(
                expected.empty()
                ? found == creator.draft().training.end()
                : found != creator.draft().training.end() && found->second == expected,
                "Every class transition preserves legal skill choices in selection order up to the new limit");
            check(from.id == to.id || !creator.draft().training.contains("class:" + from.id),
                  "Preserved choices belong to the new class entitlement");
        }
    const auto prior = fixture("campaign-v11-class-skills-before.ogs");
    CampaignParty party(module());
    party.restore(decode_campaign(prior, *creation, *rules, "class-skills", nullptr).party);
    auto body = [](const auto & s)
    {
        return s.substr(s.find('\n', s.find('\n') + 1) + 1);
    };
    auto expected = body(prior);
    replace(expected, "0.6.29", rules->identity().version);
    expected = test::with_alert_grants(
                   test::with_sneak_attack_grants(test::with_tactical_mind_grants(expected)));
    check(
        body(encode_campaign(party, nullptr, "class-skills")) ==
        test::with_druid_herbalism_grants(expected),
        "Prior twelve-class campaign adds only identity and fixed Druid Herbalism Kit; all other state is preserved");
    const auto frozen = fixture("combat-v14-class-skills-before.save");
    auto migrated = frozen;
    replace(migrated, "0.6.29", rules->identity().version);
    check(rules->restore(frozen)->save() == migrated,
          "Actual prior combat retains every class's recorded profile and state");
    for (const auto &member : party.checkpoint().roster)
    {
        const auto klass = member.character.creation_data().character_class;
        check(!member.character.sheet().training.complete,
              "Historical characters retain missing skill or mastery choices as pending");
        if (klass == "rogue")
            continue;
        auto selected = member.character.creation_data().training;
        selected["class:" + klass] = chosen_class_skills(klass);
        if (klass == "bard")
            selected["class:bard:instruments"] = {"flute", "lute", "viol"};
        if (klass == "monk")
            selected["class:monk:tools"] = {"flute"};
        if (member.character.creation_data().background == "soldier")
            selected["background:soldier:gaming_set"] = {"dice"};
        selected =
            with_advancement_training(member.character, *creation, *rules, std::move(selected));
        party.complete_training(member.id, *creation, selected);
        const auto &after = party.member(member.id);
        check(after.character.sheet().training.complete && after.vitals == member.vitals &&
              preserved_advancement(after.character, member.character) &&
              after.character.sheet().hit_points == member.character.sheet().hit_points,
              "Completing class skills preserves exact vitals and advancement");
    }
}

void bard_instruments()
{
    auto creation = srd5::character_rules();
    auto rules = module();
    const std::vector<std::string> instruments{"bagpipes", "drum", "dulcimer",  "flute", "horn",
            "lute",     "lyre", "pan_flute", "shawm", "viol"};
    auto d = draft("bard", "soldier");
    d.training = {{"origin:languages", {"elvish", "dwarvish"}},
        {"class:bard", {"performance", "persuasion", "perception"}},
        {"background:soldier:gaming_set", {"dice"}}
    };
    check(!hero(d).sheet().training.complete,
          "Bard must choose instruments after skills and languages");
    const auto groups = creation->training_options(d);
    const auto &group = *std::find_if(groups.begin(), groups.end(),
                                      [](const auto & g)
    {
        return g.id == "class:bard:instruments";
    });
    std::vector<std::string> offered;
    for (const auto &option : group.options)
        offered.push_back(option.id);
    check(group.id == "class:bard:instruments" && group.count == 3 && offered == instruments,
          "Bard offers all ten SRD instruments and exactly three choices");
    for (unsigned a = 0; a < 8; ++a)
        for (unsigned b = a + 1; b < 9; ++b)
            for (unsigned c = b + 1; c < 10; ++c)
            {
                d.training[group.id] = {instruments[a], instruments[b], instruments[c]};
                const auto h = hero(d);
                const auto &sheet = h.sheet();
                check(sheet.training.complete && sheet.training.tools.size() == 4,
                      "Every distinct instrument triple completes Training");
                for (const auto &id : d.training.at(group.id))
                {
                    check(std::find(sheet.grants.begin(), sheet.grants.end(),
                                    FeatureGrant{"tool:" + id, group.id, 1, {}}) !=
                          sheet.grants.end(),
                          "Instrument grant has Bard provenance");
                    const auto alone = creation->ability_check(sheet, 5, {}, id);
                    const auto combined = creation->ability_check(sheet, 5, "performance", id);
                    check(alone.proficiency == 2 && !alone.tool_advantage &&
                          alone.total == sheet.modifiers[5] + 2,
                          "Instrument adds proficiency once");
                    check(combined.proficiency == 2 && combined.tool_advantage &&
                          combined.total == alone.total,
                          "Skill and instrument give Advantage without double proficiency");
                }
                CampaignParty party(module());
                party.add_pc(h);
                const auto bytes = encode_campaign(party, nullptr, "bard-new");
                CampaignParty restored(module());
                restored.restore(
                    decode_campaign(bytes, *creation, *rules, "bard-new", nullptr).party);
                check(encode_campaign(restored, nullptr, "bard-new") == bytes,
                      "Every instrument triple persists canonically");
            }
    for (const std::vector<std::string> bad :
            {
                std::vector<std::string> {"flute", "flute"}, {"piano"}, {"flute", "lute", "viol", "horn"}
            })
    {
        d.training[group.id] = bad;
        rejects(
            [&]
        {
            (void)hero(d);
        });
    }
    d.training[group.id] = {"flute", "lute", "viol"};
    const auto h = hero(d);
    auto profile = rules->character_profile(h.sheet(), {}).data;
    const Encounter encounter{{8, 8, std::vector<std::uint8_t>(64)},
        {   {1, "campaign-character", "Bard", 0, {1, 1}, profile},
            {99, "vanguard", "Enemy", 1, {6, 6}}
        }};
    const auto current = rules->create(encounter, 13)->save();
    check(rules->restore(current)->save() == current,
          "Current Bard instrument combat profile round trips");
    for (const auto *source :
            {"class:rogue:instruments", "background:sage"
            })
    {
        auto forged = encounter;
        replace(forged.participants[0].character_profile, "class:bard:instruments", source);
        rejects(
            [&]
        {
            (void)rules->create(forged, 13);
        });
    }
    replace(profile, "PC28", "PC19");
    rejects(
        [&]
    {
        (void)rules->create({{8, 8, std::vector<std::uint8_t>(64)},
            {   {1, "campaign-character", "Bard", 0, {1, 1}, profile},
                {99, "vanguard", "Enemy", 1, {6, 6}}
            }},
        13);
    });
    CampaignParty party(module());
    party.add_pc(h);
    const auto bytes = encode_campaign(party, nullptr, "bard-new");
    rejects(
        [&]
    {
        (void)decode_campaign(corrupt(bytes, module()->identity().version, "0.6.30"), *creation,
        *rules, "bard-new", nullptr);
    });
    d.character_class = "wizard";
    rejects(
        [&]
    {
        (void)hero(d);
    });
    CharacterCreator creator(srd5::character_rules(), 42);
    creator.select(CreationField::character_class, "bard");
    creator.training_choice(group.id, "flute", true);
    creator.training_choice(group.id, "lute", true);
    creator.training_choice(group.id, "viol", true);
    const auto before = creator.draft().training;
    rejects(
        [&]
    {
        creator.training_choice(group.id, "horn", true);
    });
    check(creator.draft().training == before, "Excess instrument choice rejection is atomic");
    creator.select(CreationField::background, "sage");
    check(creator.draft().training.at(group.id) == before.at(group.id),
          "Background change preserves instruments");
    creator.select(CreationField::character_class, "wizard");
    check(!creator.draft().training.contains(group.id),
          "Class change removes Bard-only entitlement");
}

void monk_tools()
{
    auto creation = srd5::character_rules();
    auto rules = module();
    std::vector<std::string> expected{"alchemists_supplies",
                                      "brewers_supplies",
                                      "calligraphers_supplies",
                                      "carpenters_tools",
                                      "cartographers_tools",
                                      "cobblers_tools",
                                      "cooks_utensils",
                                      "glassblowers_tools",
                                      "jewelers_tools",
                                      "leatherworkers_tools",
                                      "masons_tools",
                                      "painters_supplies",
                                      "potters_tools",
                                      "smiths_tools",
                                      "tinkers_tools",
                                      "weavers_tools",
                                      "woodcarvers_tools",
                                      "bagpipes",
                                      "drum",
                                      "dulcimer",
                                      "flute",
                                      "horn",
                                      "lute",
                                      "lyre",
                                      "pan_flute",
                                      "shawm",
                                      "viol"};
    auto d = draft("monk", "sage");
    d.training = {{"origin:languages", {"elvish", "dwarvish"}},
        {"class:monk", {"history", "insight"}}
    };
    check(!hero(d).sheet().training.complete, "Monk must choose a tool");
    const auto groups = creation->training_options(d);
    const auto &group = groups.back();
    std::vector<std::string> offered;
    for (const auto &option : group.options)
        offered.push_back(option.id);
    std::sort(offered.begin(), offered.end());
    std::sort(expected.begin(), expected.end());
    check(group.id == "class:monk:tools" && group.count == 1 && offered == expected,
          "Monk has exactly seventeen artisan tools and ten instruments");
    for (const auto &id : expected)
    {
        d.training[group.id] = {id};
        const auto h = hero(d);
        const auto &sheet = h.sheet();
        check(sheet.training.complete, "Each permitted Monk tool completes Training");
        const auto grant = FeatureGrant{"tool:" + id, group.id, 1, {}};
        check(std::find(sheet.grants.begin(), sheet.grants.end(), grant) != sheet.grants.end(),
              "Monk tool records exact level-one source");
        const auto tool = std::find_if(sheet.training.tools.begin(), sheet.training.tools.end(),
                                       [&](const auto & t)
        {
            return t.id == id;
        });
        check(tool != sheet.training.tools.end() &&
              tool->sources.size() == (id == "calligraphers_supplies" ? 2u : 1u),
              "Background overlap retains both sources without duplicate tool entries");
        const auto alone = creation->ability_check(sheet, 3, {}, id),
                   combined = creation->ability_check(sheet, 3, "history", id);
        check(alone.proficiency == 2 && alone.total == sheet.modifiers[3] + 2 &&
              !alone.tool_advantage,
              "Tool proficiency adds once including background overlap");
        check(combined.proficiency == 2 && combined.total == alone.total && combined.tool_advantage,
              "Tool and skill add Advantage without duplicate proficiency");
        CampaignParty party(module());
        party.add_pc(h);
        const auto bytes = encode_campaign(party, nullptr, "monk-new");
        CampaignParty restored(module());
        restored.restore(decode_campaign(bytes, *creation, *rules, "monk-new", nullptr).party);
        check(encode_campaign(restored, nullptr, "monk-new") == bytes,
              "Every Monk tool persists canonically");
        const auto profile = rules->character_profile(sheet, {}).data;
        Encounter encounter{{8, 8, std::vector<std::uint8_t>(64)},
            {   {1, "campaign-character", "Monk", 0, {1, 1}, profile},
                {99, "vanguard", "Enemy", 1, {6, 6}}
            }};
        const auto saved = rules->create(encounter, 13)->save();
        check(rules->restore(saved)->save() == saved, "Every Monk tool combat profile persists");
        replace(encounter.participants[0].character_profile, "PC28", "PC20");
        rejects(
            [&]
        {
            (void)rules->create(encounter, 13);
        });
        rejects(
            [&]
        {
            (void)decode_campaign(corrupt(bytes, module()->identity().version, "0.6.31"),
            *creation, *rules, "monk-new", nullptr);
        });
        rejects(
            [&]
        {
            (void)decode_campaign(corrupt(bytes, "class:monk:tools", "class:bard:instruments"),
            *creation, *rules, "monk-new", nullptr);
        });
    }
    for (const std::vector<std::string> bad :
            {
    std::vector<std::string> {"flute", "flute"},
{"flute", "lute"},
{"thieves_tools"},
{"herbalism_kit"},
{"piano"}
        })
    {
        d.training[group.id] = bad;
        rejects(
            [&]
        {
            (void)hero(d);
        });
    }
    CharacterCreator creator(srd5::character_rules(), 42);
    creator.select(CreationField::character_class, "bard");
    for (const auto *id :
            {"lute", "flute", "viol"
            })
        creator.training_choice("class:bard:instruments", id, true);
    creator.select(CreationField::character_class, "monk");
    check(creator.draft().training.at(group.id) == std::vector<std::string> {"lute"},
          "Bard to Monk keeps first compatible instrument");
    const auto before = creator.draft().training;
    rejects(
        [&]
    {
        creator.training_choice(group.id, "flute", true);
    });
    check(creator.draft().training == before, "Excess Monk tool rejects atomically");
    creator.select(CreationField::character_class, "bard");
    check(creator.draft().training.at("class:bard:instruments") == std::vector<std::string> {"lute"},
          "Monk instrument transfers back to Bard without inventing remaining choices");
    creator.select(CreationField::character_class, "monk");
    creator.training_choice(group.id, "lute", false);
    creator.training_choice(group.id, "smiths_tools", true);
    creator.select(CreationField::background, "soldier");
    check(creator.draft().training.at(group.id) == std::vector<std::string> {"smiths_tools"},
          "Background change keeps a valid Monk tool");
    creator.select(CreationField::character_class, "bard");
    check(!creator.draft().training.contains("class:bard:instruments"),
          "Artisan tool cannot transfer to Bard's instrument entitlement");
}

void druid_herbalism()
{
    auto creation = srd5::character_rules();
    auto rules = module();
    for (const auto &klass : creation->choices(CreationField::character_class))
        for (const auto *background :
                {"sage", "acolyte", "criminal", "soldier"
                })
        {
            auto d = draft(klass.id, background);
            if (klass.id == "druid")
                d.training = {{"origin:languages", {"elvish", "dwarvish"}},
                {"class:druid", {"nature", "medicine"}}
            };
            if (std::string_view(background) == "soldier")
                d.training["background:soldier:gaming_set"] = {"dice"};
            const auto h = hero(d);
            const auto &sheet = h.sheet();
            const bool druid = klass.id == "druid";
            const auto found =
                std::find_if(sheet.training.tools.begin(), sheet.training.tools.end(),
                             [](const auto & t)
            {
                return t.id == "herbalism_kit";
            });
            check((found != sheet.training.tools.end()) == druid,
                  "Only Druids receive fixed Herbalism Kit proficiency");
            const auto alone = creation->ability_check(sheet, 3, {}, "herbalism_kit");
            check(alone.proficiency == (druid ? 2 : 0) &&
                  alone.total == sheet.modifiers[3] + (druid ? 2 : 0),
                  "Herbalism checks use Intelligence modifier and proficiency once");
            if (!druid)
                continue;
            check(sheet.training.complete &&
                  found->sources ==
            std::vector<FeatureGrant> {{"tool:herbalism_kit", "class:druid", 1, {}}},
            "Druid's fixed grant is complete with exact source");
            const auto combined = creation->ability_check(sheet, 3, "nature", "herbalism_kit");
            check(combined.total == alone.total && combined.tool_advantage,
                  "Applicable skill plus Herbalism Kit adds Advantage");
            for (unsigned mode = 0; mode < 3; ++mode)
            {
                auto bad = sheet;
                if (mode == 0)
                    std::erase_if(bad.grants,
                                  [](const auto & g)
                {
                    return g.id == "tool:herbalism_kit";
                });
                else if (mode == 1)
                    bad.grants.push_back({"tool:herbalism_kit", "class:druid", 1, {}});
                else
                    for (auto &g : bad.grants)
                        if (g.id == "tool:herbalism_kit")
                            g.source_id = "background:sage";
                rejects(
                    [&]
                {
                    (void)rules->character_profile(bad, {});
                });
            }
            auto profile = rules->character_profile(sheet, {}).data;
            Encounter encounter{{8, 8, std::vector<std::uint8_t>(64)},
                {   {1, "campaign-character", "Druid", 0, {1, 1}, profile},
                    {99, "vanguard", "Enemy", 1, {6, 6}}
                }};
            const auto current = rules->create(encounter, 13)->save();
            check(rules->restore(current)->save() == current,
                  "Druid's current Herbalism Kit profile round trips");
            replace(encounter.participants[0].character_profile,
                    profile.substr(0, profile.find(' ')), "PC21");
            rejects(
                [&]
            {
                (void)rules->create(encounter, 13);
            });
            CampaignParty party(module());
            party.add_pc(h);
            const auto bytes = encode_campaign(party, nullptr, "druid-new");
            rejects(
                [&]
            {
                (void)decode_campaign(corrupt(bytes, module()->identity().version, "0.6.32"),
                *creation, *rules, "druid-new", nullptr);
            });
        }
    const auto prior = fixture("campaign-v11-druid-herbalism-before.ogs");
    CampaignParty party(module());
    party.restore(decode_campaign(prior, *creation, *rules, "druid-herbalism", nullptr).party);
    auto body = [](const auto & text)
    {
        return text.substr(text.find('\n', text.find('\n') + 1) + 1);
    };
    auto expected = body(prior);
    replace(expected, "0.6.32", rules->identity().version);
    expected = test::with_alert_grants(
                   test::with_sneak_attack_grants(test::with_tactical_mind_grants(expected)));
    const auto current = encode_campaign(party, nullptr, "druid-herbalism");
    check(
        body(current) == test::with_druid_herbalism_grants(expected),
        "Every old Druid campaign byte stays except identity/checksum and exactly one fixed grant per Druid");
    for (const auto &member : party.state().roster)
    {
        check(member.character.sheet().training.complete ==
              (member.character.creation_data().background != "soldier"),
              "Fixed proficiency adds no choice; historical Soldier Gaming Set remains pending");
        check(member.character.creation_data().training.at("class:druid") ==
              std::vector<std::string> {"nature", "medicine"},
              "Historical Druid skills stay ordered");
        check(member.vitals.hit_points == member.character.sheet().hit_points - 2 &&
              member.vitals.resources == "SRD1 0 0 0 0 0",
              "Druid wounds and resources survive");
    }
    CampaignParty again(module());
    again.restore(decode_campaign(current, *creation, *rules, "druid-herbalism", nullptr).party);
    check(encode_campaign(again, nullptr, "druid-herbalism") == current,
          "Repeated migration/save does not duplicate fixed proficiency");
    const auto combat = fixture("combat-v14-druid-herbalism-before.save");
    auto migrated = combat;
    replace(migrated, "0.6.32", rules->identity().version);
    check(rules->restore(combat)->save() == migrated,
          "Prior Druid combat retains exact continuation and original profile");
}

void soldier_gaming()
{
    auto creation = srd5::character_rules();
    auto rules = module();
    const std::string group_id = "background:soldier:gaming_set";
    const std::vector<std::string> variants{"dice", "dragonchess", "playing_cards",
                                            "three_dragon_ante"};
    for (const auto &klass : creation->choices(CreationField::character_class))
        for (const auto &variant : variants)
        {
            auto d = draft(klass.id, "soldier");
            d.training = {{"origin:languages", {"elvish", "dwarvish"}},
                {"class:" + klass.id, chosen_class_skills(klass.id)}
            };
            if (klass.id == "rogue")
            {
                d.training["class:rogue:expertise"] = {d.training["class:rogue"][0],
                                                       d.training["class:rogue"][1]
                                                      };
                d.training["class:rogue:thieves_cant"] = {"undercommon"};
            }
            if (klass.id == "fighter")
                d.training["class:fighter:fighting_style"] = {"defense"};
            if (klass.id == "bard")
                d.training["class:bard:instruments"] = {"flute", "lute", "viol"};
            if (klass.id == "monk")
                d.training["class:monk:tools"] = {"smiths_tools"};
            const auto groups = creation->training_options(d);
            const auto &offered = *std::find_if(groups.begin(), groups.end(),
                                                [&](const auto & group)
            {
                return group.id == group_id;
            });
            std::vector<std::string> ids;
            for (const auto &option : offered.options)
                ids.push_back(option.id);
            check(offered.id == group_id && offered.count == 1 && ids == variants,
                  "Every Soldier class offers exactly the four Gaming Set variants");
            check(!hero(d).sheet().training.complete,
                  "Gaming Set choice is required independently of class choices");
            d.training[group_id] = {variant};
            complete_mastery(d);
            const auto h = hero(d);
            const auto &sheet = h.sheet();
            check(sheet.training.complete,
                  "Every variant completes every class's Soldier training");
            const auto tool = std::find_if(sheet.training.tools.begin(), sheet.training.tools.end(),
                                           [&](const auto & t)
            {
                return t.id == variant;
            });
            check(tool != sheet.training.tools.end() &&
                  tool->sources ==
            std::vector<FeatureGrant> {{"tool:" + variant, group_id, 1, {}}},
            "Gaming Set has exact level-one Soldier provenance");
            const auto alone = creation->ability_check(sheet, 4, {}, variant),
                       combined = creation->ability_check(sheet, 4, "intimidation", variant);
            check(alone.proficiency == 2 && alone.total == sheet.modifiers[4] + 2 &&
                  !alone.tool_advantage,
                  "Gaming Set adds proficiency once to Wisdom checks");
            check(combined.proficiency == 2 && combined.total == alone.total &&
                  combined.tool_advantage,
                  "Applicable trained skill adds Advantage without duplicate proficiency");
            CampaignParty party(module());
            party.add_pc(h);
            const auto bytes = encode_campaign(party, nullptr, "gaming-new");
            CampaignParty restored(module());
            restored.restore(
                decode_campaign(bytes, *creation, *rules, "gaming-new", nullptr).party);
            check(encode_campaign(restored, nullptr, "gaming-new") == bytes,
                  "Every class and Gaming Set combination saves canonically");
            rejects(
                [&]
            {
                (void)decode_campaign(corrupt(bytes, module()->identity().version, "0.6.33"),
                *creation, *rules, "gaming-new", nullptr);
            });
            const auto profile = rules->character_profile(sheet, {}).data;
            Encounter encounter{{8, 8, std::vector<std::uint8_t>(64)},
                {   {1, "campaign-character", "Soldier", 0, {1, 1}, profile},
                    {99, "vanguard", "Enemy", 1, {6, 6}}
                }};
            const auto current = rules->create(encounter, 13)->save();
            check(rules->restore(current)->save() == current,
                  "Gaming Set current combat continuation is canonical");
            auto older_sheet = sheet;
            std::erase_if(older_sheet.grants,
                          [](const auto & g)
            {
                return g.id.starts_with("mastery:");
            });
            encounter.participants[0].character_profile =
                rules->character_profile(older_sheet, {}).data;
            replace(encounter.participants[0].character_profile,
                    klass.id == "wizard"  ? "PC32"
                    : klass.id == "rogue" ? "PC35"
                    : "PC28",
                    "PC22");
            rejects(
                [&]
            {
                (void)rules->create(encounter, 13);
            });
            auto wrong = sheet;
            for (auto &grant : wrong.grants)
                if (grant.source_id == group_id)
                    grant.source_id = "background:criminal";
            rejects(
                [&]
            {
                (void)rules->character_profile(wrong, {});
            });
            for (const std::vector<std::string> bad :
                    {
            std::vector<std::string> {"dice", "dice"},
        {"dice", "dragonchess"},
        {"flute"},
        {"thieves_tools"},
        {"chess"}
                })
            {
                auto broken = d;
                broken.training[group_id] = bad;
                rejects(
                    [&]
                {
                    (void)hero(broken);
                });
            }
            auto wrong_background = d;
            wrong_background.background = "criminal";
            rejects(
                [&]
            {
                (void)hero(wrong_background);
            });
        }
    CharacterCreator creator(srd5::character_rules(), 42);
    creator.select(CreationField::background, "soldier");
    creator.training_choice(group_id, "playing_cards", true);
    const auto before = creator.draft().training;
    rejects(
        [&]
    {
        creator.training_choice(group_id, "dice", true);
    });
    check(creator.draft().training == before, "Rejected second Gaming Set is atomic");
    for (const auto &klass : creation->choices(CreationField::character_class))
    {
        creator.select(CreationField::character_class, klass.id);
        check(creator.draft().training.at(group_id) == std::vector<std::string> {"playing_cards"},
              "All class changes retain Soldier's Gaming Set");
    }
    creator.select(CreationField::background, "sage");
    check(!creator.draft().training.contains(group_id),
          "Background change removes ineligible Gaming Set grant");
    const auto prior = fixture("campaign-v11-soldier-gaming-before.ogs");
    CampaignParty party(module());
    party.restore(decode_campaign(prior, *creation, *rules, "soldier-gaming", nullptr).party);
    auto body = [](const auto & text)
    {
        return text.substr(text.find('\n', text.find('\n') + 1) + 1);
    };
    auto expected = body(prior);
    replace(expected, "0.6.33", rules->identity().version);
    expected = test::with_alert_grants(
                   test::with_sneak_attack_grants(test::with_tactical_mind_grants(expected)));
    check(
        body(encode_campaign(party, nullptr, "soldier-gaming")) == expected,
        "All twelve historical Soldiers retain every field except module identity/checksum and fixed Tactical Mind grant");
    const auto old = party.checkpoint();
    check(old.roster.size() == 12, "Prior gaming fixture covers twelve classes");
    for (const auto &member : old.roster)
    {
        check(!member.character.sheet().training.complete &&
              !member.character.creation_data().training.contains(group_id),
              "Old Soldier Gaming Set choices stay pending");
        auto selected = member.character.creation_data().training;
        selected[group_id] = {"three_dragon_ante"};
        selected =
            with_advancement_training(member.character, *creation, *rules, std::move(selected));
        party.complete_training(member.id, *creation, selected);
        const auto &after = party.member(member.id);
        check(after.character.sheet().training.complete && after.vitals == member.vitals &&
              preserved_advancement(after.character, member.character),
              "Completing Gaming Set keeps wounds, resources and advancement history");
        for (const auto &grant : member.character.sheet().grants)
            check(std::find(after.character.sheet().grants.begin(),
                            after.character.sheet().grants.end(),
                            grant) != after.character.sheet().grants.end(),
                  "Completion retains existing feat and training grants");
    }
    const auto completed = encode_campaign(party, nullptr, "soldier-gaming");
    CampaignParty again(module());
    again.restore(decode_campaign(completed, *creation, *rules, "soldier-gaming", nullptr).party);
    check(encode_campaign(again, nullptr, "soldier-gaming") == completed,
          "Completed old Gaming Set choices persist canonically");
    const auto frozen = fixture("combat-v14-soldier-gaming-before.save");
    auto migrated = frozen;
    replace(migrated, "0.6.33", rules->identity().version);
    check(rules->restore(frozen)->save() == migrated,
          "Old twelve-class combat preserves exact profiles, feat state and RNG");
}

void creation_controls()
{
    CharacterCreator creator(srd5::character_rules(), 42);
    creator.select(CreationField::character_class, "rogue");
    creator.select(CreationField::background, "criminal");
    creator.next();
    creator.next();
    for (unsigned tries = 0;; ++tries)
    {
        check(tries < 100, "Roll a qualified Rogue fixture");
        creator.roll();
        for (unsigned i = 0; i < 6; ++i)
            creator.assign_roll(i, i);
        if (creator.rules().class_eligible(creator.draft(), "rogue"))
            break;
    }
    creator.next();
    creator.next();
    check(creator.step() == CreationStep::training && !creator.training_complete(),
          "Training follows Class and starts incomplete");
    rejects(
        [&]
    {
        creator.next();
    });
    rejects(
        [&]
    {
        creator.training_choice("unknown", "elvish", true);
    });
    rejects(
        [&]
    {
        creator.training_choice("origin:languages", "abyssal", true);
    });
    rejects(
        [&]
    {
        creator.training_choice("class:rogue:expertise", "arcana", true);
    });
    // Independent, authored choices exercise dependent groups in UI order.
    const auto selected = choices();
    for (const auto *group :
            {"origin:languages", "class:rogue", "class:rogue:expertise",
             "class:rogue:thieves_cant", "class:rogue:weapon_mastery"
            })
        for (const auto &value : selected.at(group))
            creator.training_choice(group, value, true);
    check(creator.training_complete() && creator.sheet().training.complete,
          "Every required choice permits completion");
    const auto before = creator.draft().training;
    creator.training_choice("origin:languages", "elvish", true);
    rejects(
        [&]
    {
        creator.training_choice("origin:languages", "orc", true);
    });
    check(creator.draft().training == before,
          "Duplicate selection is idempotent; excessive selection rejects atomically");
    creator.next();
    creator.back();
    check(creator.draft().training == before, "Back preserves selected training");
    creator.training_choice("class:rogue", "perception", false);
    check(creator.draft().training.at("class:rogue:expertise") ==
          std::vector<std::string> {"stealth"},
          "Removing a skill removes only its dependent Expertise");
    creator.training_choice("class:rogue", "perception", true);
    creator.training_choice("class:rogue:expertise", "perception", true);
    creator.back();
    creator.back();
    creator.select(CreationField::background, "sage");
    check(creator.draft().training.at("class:rogue:expertise") ==
          std::vector<std::string> {"perception"},
          "Changing background removes only lost proficiency's Expertise");
    check(std::is_permutation(creator.draft().training.at("class:rogue").begin(),
                              creator.draft().training.at("class:rogue").end(),
                              selected.at("class:rogue").begin(), selected.at("class:rogue").end()),
          "Changing background preserves valid skill choices");
    creator.select(CreationField::background, "criminal");
    creator.training_choice("class:rogue:expertise", "stealth", true);
    creator.training_choice("class:rogue:thieves_cant", "undercommon", false);
    creator.training_choice("class:rogue:thieves_cant", "orc", true);
    creator.training_choice("origin:languages", "dwarvish", false);
    creator.training_choice("origin:languages", "orc", true);
    check(!creator.draft().training.contains("class:rogue:thieves_cant"),
          "Moving a language into starting choices invalidates only the duplicate Rogue choice");
    creator.training_choice("class:rogue:thieves_cant", "undercommon", true);
    creator.select(CreationField::character_class, "fighter");
    check(
        creator.draft().training.size() == 3 && creator.draft().training.at("origin:languages") ==
        std::vector<std::string>({"elvish", "orc"}),
        "Class change preserves languages and compatible skills while clearing Rogue-only groups");
    check(!creator.training_complete(), "Fighter requires its own starting style choice");
    creator.training_choice("class:fighter:fighting_style", "defense", true);
    check(creator.draft().training.at("class:fighter") ==
          std::vector<std::string>({"acrobatics", "persuasion"}),
          "Class change keeps the first two valid selected skills");
    creator.training_choice("class:fighter:weapon_mastery", "longsword", true);
    check(creator.training_complete(),
          "Fighter languages, style, skills and three mastery kinds complete supported training");
    creator.training_choice("class:fighter:fighting_style", "archery", true);
    check(creator.draft().training.at("class:fighter:fighting_style") ==
          std::vector<std::string> {"archery"},
          "Single selection replaces the prior style atomically");
    const auto style_before = creator.draft().training;
    rejects(
        [&]
    {
        creator.training_choice("class:fighter:fighting_style", "unimplemented", true);
    });
    check(creator.draft().training == style_before,
          "Invalid replacement preserves the prior style");
    creator.select(CreationField::background, "sage");
    check(creator.draft().training == style_before, "Changing background preserves a valid style");
    creator.select(CreationField::background, "criminal");
    creator.select(CreationField::character_class, "rogue");
    check(!creator.draft().training.contains("class:fighter:fighting_style") &&
          !creator.training_complete(),
          "Returning to Rogue clears the invalid style without inventing cleared choices");
    for (const auto *group :
            {"class:rogue", "class:rogue:expertise", "class:rogue:thieves_cant",
             "class:rogue:weapon_mastery"
            })
        for (const auto &value : selected.at(group))
            creator.training_choice(group, value, true);
    creator.next();
    creator.next();
    creator.next();
    creator.name("Created Rogue");
    creator.next();
    creator.next();
    const auto finished = creator.create_character();
    check(finished.sheet().training.complete,
          "Finished character retains the training selected through the creator");
    CampaignParty party(module());
    const auto id = party.add_pc(finished);
    const auto encoded = encode_campaign(party, nullptr, "creator");
    auto loaded = decode_campaign(encoded, creator.rules(), *module(), "creator", nullptr);
    CampaignParty restored(module());
    restored.restore(std::move(loaded.party));
    check(restored.member(id).character.creation_data().training ==
          finished.creation_data().training,
          "Manually selected training survives party save/load");
    rejects(
        [&]
    {
        creator.training_choice("origin:languages", "elvish", false);
    });
    creator.restart();
    check(creator.draft().training.empty() && !creator.training_complete(),
          "Restart clears training selections");
}

void preset_training()
{
    por::CharacterArt art;
    Image head;
    head.width = 88;
    head.height = 40;
    head.rgba.assign(88 * 40 * 4, 128);
    Image body;
    body.width = 88;
    body.height = 48;
    body.rgba.assign(88 * 48 * 4, 128);
    art.heads.emplace(1, por::PortraitPart{"fixture", head});
    art.bodies.emplace(1, por::PortraitPart{"fixture", body});
    auto creation = srd5::character_rules();
    const auto pool = character_pool(*creation, art), again = character_pool(*creation, art);
    check(pool.size() == 48, "Pool contains four presets for all twelve classes");
    std::map<std::string, unsigned> classes;
    for (unsigned i = 0; i < pool.size(); ++i)
    {
        const auto &c = pool[i];
        ++classes[c.creation_data().character_class];
        if (c.creation_data().background == "criminal")
            check(std::count(c.sheet().grants.begin(), c.sheet().grants.end(),
                             FeatureGrant{"feat:alert", "background:criminal", 1, {}}) == 1,
                  "Criminal presets receive exactly one fixed Alert grant");
        check(c.sheet().training.complete &&
              c.creation_data().training == again[i].creation_data().training,
              "Preset training is complete and deterministic");
        if (c.creation_data().character_class == "fighter")
        {
            const auto &picked = c.creation_data().training.at("class:fighter:fighting_style");
            check(picked.size() == 1 &&
                  std::find(c.sheet().grants.begin(), c.sheet().grants.end(),
                            FeatureGrant
            {
                "feat:" + picked[0], "class:fighter:fighting_style", 1, {}}) !=
            c.sheet().grants.end(),
             "Every preset Fighter has one pre-generated style and its grant");
        }
        check(c.sheet().training.languages.size() ==
              (c.creation_data().character_class == "rogue" ? 5u : 3u),
              "Preset languages are distinct and include all fixed and selected grants");
        CampaignParty party(module());
        const auto id = party.add_pc(c);
        const auto bytes = encode_campaign(party, nullptr, "preset");
        auto decoded = decode_campaign(bytes, *creation, *module(), "preset", nullptr);
        party.restore(std::move(decoded.party));
        check(party.member(id).character.creation_data().training == c.creation_data().training &&
              party.member(id).character.sheet().training.complete,
              "Preset training remains complete after adding and saving");
    }
    check(classes.size() == 12 && std::all_of(classes.begin(), classes.end(),
            [](const auto & c)
    {
        return c.second == 4;
    }),
    "Every class has four completed presets");
}

void grants_and_checks()
{
    auto creation = srd5::character_rules();
    auto d = draft();
    auto sheet = hero(d).sheet();
    check(!sheet.training.complete && sheet.training.skills.size() == 18,
          "Missing choices stay pending while all ordinary skill modifiers are available");
    check(sheet.training.languages.size() == 2 && sheet.training.tools.size() == 1,
          "Common, Thieves' Cant and Thieves' Tools are fixed grants");
    check(sheet.training.tools[0].sources.size() == 2,
          "Rogue and Criminal tool grants retain both sources");
    check(skill(sheet, "stealth").bonus == 5 && !skill(sheet, "stealth").expertise,
          "Criminal grants Dexterity +3 plus proficiency +2 without inventing Expertise");
    d.training = choices();
    sheet = hero(d).sheet();
    check(
        sheet.training.complete && sheet.training.languages.size() == 5,
        "Two standard languages and a distinct Rogue language complete the fixed language grants");
    check(skill(sheet, "stealth").bonus == 7 && skill(sheet, "perception").bonus == 6,
          "Expertise adds doubled +2 proficiency to the governing ability");
    check(skill(sheet, "investigation").bonus == 4 && skill(sheet, "arcana").bonus == 2,
          "Proficient and untrained skills use different bonuses");
    check(skill(sheet, "stealth").sources.size() == 2,
          "Expertise and background proficiency have separate provenance");
    auto result = creation->ability_check(sheet, 1, {}, "thieves_tools");
    check(result.ability_modifier == 3 && result.proficiency == 2 && result.total == 5 &&
          !result.tool_advantage && result.sources.size() == 2,
          "Duplicate tool grants add proficiency only once");
    result = creation->ability_check(sheet, 1, "sleight_of_hand", "thieves_tools");
    check(result.total == 5 && result.tool_advantage && !result.expertise,
          "Using a proficient skill and tool grants advantage without stacking proficiency");
    result = creation->ability_check(sheet, 1, "stealth", "thieves_tools");
    check(result.total == 7 && result.tool_advantage && result.expertise,
          "Tool proficiency does not add again on top of Expertise");
    result = creation->ability_check(sheet, 0, "stealth", {});
    check(result.total == 6,
          "A rule can choose another governing ability without changing training");
    rejects(
        [&]
    {
        creation->ability_check(sheet, 6, "stealth", {});
    });
    rejects(
        [&]
    {
        creation->ability_check(sheet, 1, "unknown", {});
    });
    rejects(
        [&]
    {
        creation->ability_check(sheet, 1, {}, "unknown");
    });
    // Independent proficiency table boundaries. This query is shared math,
    // not a claim that Rogue advancement beyond level one is integrated.
    for (const auto [level, bonus] :
            {
                std::pair{1, 2}, std::pair{4, 2}, std::pair{5, 3}, std::pair{9, 4}, std::pair{13, 5},
                std::pair{17, 6}, std::pair{20, 6}
            })
    {
        auto later = sheet;
        later.level = level;
        check(creation->ability_check(later, 1, "stealth", {}).total == 3 + 2 * bonus,
              "Expertise uses the character-level proficiency table");
    }
    d.training["class:rogue"] = {"stealth", "investigation", "perception", "persuasion"};
    sheet = hero(d).sheet();
    check(skill(sheet, "stealth").sources.size() == 3 && skill(sheet, "stealth").bonus == 7,
          "Overlapping class/background skill and Expertise grants do not stack bonuses");
    for (const auto &klass : creation->choices(CreationField::character_class))
    {
        auto other = draft(klass.id, "sage");
        other.training = {{"origin:languages", {"common_sign_language", "orc"}}};
        const auto s = hero(other).sheet();
        check(s.training.languages.size() == (klass.id == "rogue" ? 4u : 3u),
              "Starting languages are available for every class");
    }
}

void invalid_choices()
{
    const auto valid = []()
    {
        auto d = draft();
        d.training = choices();
        return d;
    };
    const std::vector<std::pair<std::string, std::vector<std::string>>> bad
    {
        {"origin:languages", {"elvish", "elvish"}},
        {"origin:languages", {"common", "elvish"}},
        {"origin:languages", {"abyssal", "elvish"}},
        {"origin:languages", {"elvish", "dwarvish", "orc"}},
        {"class:rogue", {"acrobatics", "arcana", "perception", "persuasion"}},
        {"class:rogue", {"acrobatics", "acrobatics", "perception", "persuasion"}},
        {"class:rogue:expertise", {"arcana", "stealth"}},
        {"class:rogue:expertise", {"thieves_tools", "stealth"}},
        {"class:rogue:expertise", {"stealth", "stealth"}},
        {"class:rogue:thieves_cant", {"elvish"}},
        {"class:rogue:thieves_cant", {"thieves_cant"}},
        {"class:rogue:thieves_cant", {"unknown"}},
        {"class:rogue:expertise", {"stealth", "perception", "persuasion"}},
        {"unknown", {"elvish"}}};
    for (const auto &[group, values] : bad)
    {
        auto d = valid();
        d.training[group] = values;
        rejects(
            [&]
        {
            (void)hero(d);
        });
    }
    auto d = valid();
    d.character_class = "fighter";
    rejects(
        [&]
    {
        (void)hero(d);
    });
    d = valid();
    d.training["class:rogue"].erase(d.training["class:rogue"].begin() + 2);
    rejects(
        [&]
    {
        (void)hero(d);
    }); // Cannot retain Perception Expertise after losing its proficiency.
    d = valid();
    d.training.erase("class:rogue:expertise");
    check(!hero(d).sheet().training.complete, "Incomplete selections remain explicitly pending");
    auto sheet = hero(valid()).sheet();
    auto rules = module();
    for (const auto &grant :
    std::vector<FeatureGrant> {{"skill:arcana", "class:rogue", 1, {}},
    {"expertise:stealth", "class:rogue:expertise", 1, {}},
    {"tool:thieves_tools", "background:criminal", 1, {}},
    {"language:abyssal", "origin:languages", 1, {}}
})
    {
        auto invalid = sheet;
        invalid.grants.push_back(grant);
        rejects(
            [&]
        {
            (void)rules->character_profile(invalid, {});
        });
    }
    auto invalid = sheet;
    std::erase_if(invalid.grants,
                  [](const auto & g)
    {
        return g.id == "language:common";
    });
    rejects(
        [&]
    {
        (void)rules->character_profile(invalid, {});
    });
}

void persistence()
{
    auto d = draft();
    d.training = choices();
    CampaignParty party(module());
    const auto id = party.add_pc(hero(d));
    auto state = party.checkpoint();
    state.roster[0].vitals.hit_points -= 2;
    party.restore(state);
    const auto bytes = encode_campaign(party, nullptr, "training-fixture");
    auto loaded =
        decode_campaign(bytes, *srd5::character_rules(), *module(), "training-fixture", nullptr);
    CampaignParty restored(module());
    restored.restore(std::move(loaded.party));
    check(encode_campaign(restored, nullptr, "training-fixture") == bytes &&
          restored.member(id).character.creation_data().training == d.training,
          "Choice order, source grants and wounded state round trip exactly");
    for (const auto &bad :
            {
                corrupt(bytes, "\"elvish\"", "\"abyssal\""),
                corrupt(bytes, "\"tool:thieves_tools\"", "\"tool:unknown\"")
            })
        rejects(
            [&]
    {
        (void)decode_campaign(bad, *srd5::character_rules(), *module(), "training-fixture",
        nullptr);
    });
    check(encode_campaign(party, nullptr, "training-fixture") == bytes,
          "Failed load does not replace live state");
    auto rules = module();
    auto members = party.participants();
    members[0].cell = {1, 1};
    members.push_back({99, "vanguard", "Enemy", 1, {5, 1}});
    auto combat = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, members}, 42);
    const auto checkpoint = combat->save();
    check(checkpoint.find("expertise:stealth") != checkpoint.npos &&
          rules->restore(checkpoint)->save() == checkpoint,
          "Combat recipes retain training and Expertise provenance");
    auto invalid = checkpoint;
    replace(invalid, "class:rogue:expertise", "class:rogue:invalid");
    rejects(
        [&]
    {
        (void)rules->restore(invalid);
    });
    auto old = decode_campaign(fixture("campaign-v8-training.ogs"), *srd5::character_rules(),
                               *rules, "training-fixture", nullptr);
    CampaignParty migrated(module());
    migrated.restore(std::move(old.party));
    for (unsigned n = 1; n <= 4; ++n)
    {
        const auto &m = migrated.member(n);
        check(m.character.creation_data().training.empty() &&
              !m.character.sheet().training.complete,
              "Old saves do not receive invented training choices");
        check(m.vitals.hit_points == m.character.sheet().hit_points - 2,
              "Training migration preserves wounds");
    }
    check(migrated.member(3).vitals.resources == "SRD1 1 0 0 0 0" &&
          migrated.member(4).vitals.resources == "SRD2 0 1 1 0 0 0",
          "Migration preserves spent feat/class resources");
    check(migrated.member(1).character.sheet().training.tools.front().sources.size() == 2,
          "Older Rogue/Criminal gains the two known fixed tool sources");
    const auto pending = encode_campaign(migrated, nullptr, "training-fixture");
    auto again =
        decode_campaign(pending, *srd5::character_rules(), *rules, "training-fixture", nullptr);
    CampaignParty twice(module());
    twice.restore(std::move(again.party));
    check(encode_campaign(twice, nullptr, "training-fixture") == pending,
          "Unresolved choices remain pending across repeated saves");
    auto old_combat = fixture("combat-v8-training.save");
    auto migrated_combat = rules->restore(old_combat);
    old_combat = test::with_hit_dice(old_combat, rules->identity(),
    {{1, 1}, {2, 1}, {3, 4}, {4, 4}, {99, 0}});
    check(
        migrated_combat->save() == old_combat,
        "Legacy combat retains every old recipe, RNG, wound and resource without injecting new choices");
    auto fighter = draft("fighter");
    fighter.training = {{"origin:languages", {"elvish", "orc"}}};
    CampaignParty growing(module());
    const auto f = growing.add_pc(hero(fighter));
    growing.award_experience(2700, "training-xp");
    for (unsigned level = 2; level <= 3; ++level)
        growing.advance(f, growing.default_advancement(f));
    auto choice = growing.default_advancement(f);
    choice.abilities = {};
    choice.abilities[1] = 2;
    growing.advance(f, choice);
    check(skill(growing.member(f).character.sheet(), "stealth").bonus == 6,
          "Level-up rebuilds skill totals after an ability modifier changes");
}

void draft_review_editor()
{
    auto d = draft();
    d.training = {{"origin:languages", {"elvish"}}, {"class:rogue", {"perception"}}};
    const auto original = d;
    CharacterCreator editor(srd5::character_rules(), d);
    check(editor.step() == CreationStep::training && !editor.training_complete(),
          "Existing draft starts at incomplete Training");
    rejects(
        [&]
    {
        editor.training_choice("origin:languages", "elvish", false);
    });
    auto conflict = draft();
    conflict.training = {{"class:rogue:thieves_cant", {"elvish"}}};
    CharacterCreator locked(srd5::character_rules(), conflict);
    rejects(
        [&]
    {
        locked.training_choice("origin:languages", "elvish", true);
    });
    check(locked.draft().training == conflict.training,
          "Indirect pruning cannot remove locked language choices");
    editor.training_choice("origin:languages", "dwarvish", true);
    for (const auto &[group, values] : choices())
        for (const auto &value : values)
            editor.training_choice(group, value, true);
    check(editor.training_complete() && d.training == original.training,
          "Draft review fills choices without modifying original");
    check(editor.draft().name == original.name && editor.draft().cantrips == original.cantrips &&
          editor.draft().rolls == original.rolls,
          "Review retains unrelated creation fields");
}

CampaignParty review_fixture()
{
    auto rules = module();
    auto saved = decode_campaign(fixture("campaign-v8-training.ogs"), *srd5::character_rules(),
                                 *rules, "training-fixture", nullptr);
    CampaignParty party(module());
    party.restore(std::move(saved.party));
    party.remove(2);
    auto state = party.checkpoint();
    auto &member = state.roster.front();
    auto d = member.character.creation_data();
    d.training = {{"origin:languages", {"elvish"}},
        {"class:rogue", {"perception"}},
        {"class:rogue:expertise", {"stealth"}}
    };
    Character partial(*srd5::character_rules(), d, member.character.appearance());
    partial.inventory() = member.character.inventory();
    VitalState healthy{partial.sheet().hit_points};
    for (const auto &choice : member.character.advancements())
        partial.advance(*rules, healthy, choice);
    member.character = std::move(partial);
    party.restore(std::move(state));
    auto fighter = draft("fighter");
    fighter.training = {{"class:fighter:fighting_style", {"archery"}}};
    party.add_pc(hero(fighter));
    return party;
}

void write_review_fixture()
{
    const auto *directory = std::getenv("OPENGOLD_GAME_DIR");
    if (!directory || !*directory)
        return;
    auto party = review_fixture();
    const auto path = std::filesystem::path(OPENGOLD_BINARY_DIR) / "training-review.ogs";
    write_campaign_file(path, encode_campaign(party, nullptr, campaign_asset_identity(directory)));
}

void verify_review_result(const char *path)
{
    const auto *directory = std::getenv("OPENGOLD_GAME_DIR");
    check(directory && *directory, "Review verification requires original game directory");
    const auto assets = campaign_asset_identity(directory);
    auto rules = module();
    auto creation = srd5::character_rules();
    auto original = review_fixture();
    auto saved = decode_campaign(read_campaign_file(path), *creation, *rules, assets, nullptr);
    for (const auto &member : saved.party.roster)
    {
        check(member.character.sheet().training.complete, "UI completed all supported training");
        original.complete_training(member.id, *creation, member.character.training_choices());
    }
    // Roster selection is a UI action, independent from training application.
    auto expected = original.checkpoint();
    expected.selected = saved.party.selected;
    original.restore(std::move(expected));
    CampaignParty actual(module());
    actual.restore(std::move(saved.party));
    check(
        encode_campaign(original, nullptr, assets) == encode_campaign(actual, nullptr, assets),
        "UI review only changes selected training; wounds, resources, gear, advancement and campaign history survive");
}

void complete_saved_training()
{
    auto creation = srd5::character_rules();
    auto rules = module();
    auto loaded = decode_campaign(fixture("campaign-v8-training.ogs"), *creation, *rules,
                                  "training-fixture", nullptr);
    CampaignParty party(module());
    party.restore(std::move(loaded.party));
    party.set_wealth(3, {0, 0, 0, 37, 0, 0, 2});
    por::Equipment sword;
    sword.stored.type = 36;
    sword.stored.stack_size = 1;
    sword.stored.value = 10;
    party.purchase(3, sword);
    party.equip(3, 1);
    party.set_grip(3, 2);
    auto state = party.checkpoint();
    state.time_minutes = 100;
    state.subminute_milliseconds = 1234;
    state.random_state = 918273;
    state.roster[2].last_rest_minutes = 50;
    state.roster[2].last_rest_subminute_milliseconds = 789;
    state.roster[2].vitals.resources =
        "SRD3 1 0 0 0 0 0 FX1 2 1 1 1 77 99 \"Source caster\" 13 43000 2000";
    state.roster[2].vitals.description = "Blinded";
    party.restore(state);
    party.remove(2); // Pending reserve members can also complete their choices.
    const auto original = encode_campaign(party, nullptr, "training-fixture");
    auto invalid = choices();
    invalid["class:rogue:expertise"] = {"arcana", "stealth"};
    rejects(
        [&]
    {
        party.complete_training(1, *creation, invalid);
    });
    rejects(
        [&]
    {
        party.complete_training(1, *creation, {});
    });
    rejects(
        [&]
    {
        party.complete_training(99, *creation, choices());
    });
    check(encode_campaign(party, nullptr, "training-fixture") == original,
          "Invalid and incomplete confirmation leaves every campaign field unchanged");
    party.begin_combat();
    rejects(
        [&]
    {
        (void)party.preview_training(1, *creation, choices());
    });
    rejects(
        [&]
    {
        party.complete_training(1, *creation, choices());
    });
    party.end_combat();
    check(encode_campaign(party, nullptr, "training-fixture") == original,
          "Combat rejects training preview and confirmation without changing state");
    for (MemberId id = 1; id <= 4; ++id)
    {
        auto selected =
        id <= 2 ? choices() : TrainingChoices{{"origin:languages", {"elvish", "orc"}}};
        if (id == 2)
            selected["class:rogue:expertise"] = {"investigation", "perception"};
        const auto before = encode_campaign(party, nullptr, "training-fixture");
        const auto previous = party.member(id);
        if (previous.character.sheet().character_class == "Fighter")
            selected["class:fighter:fighting_style"] = {"archery"};
        if (previous.character.creation_data().character_class != "rogue")
            selected["class:" + previous.character.creation_data().character_class] =
                chosen_class_skills(previous.character.creation_data().character_class);
        if (previous.character.creation_data().background == "soldier")
            selected["background:soldier:gaming_set"] = {"dice"};
        selected =
            with_advancement_training(previous.character, *creation, *rules, std::move(selected));
        const auto preview = party.preview_training(id, *creation, selected);
        check(encode_campaign(party, nullptr, "training-fixture") == before,
              "Opening, editing and discarding a preview has no campaign effects");
        const auto &character = preview.character;
        const auto &old = previous.character;
        check(character.sheet().training.complete && character.training_choices() == selected,
              "Preview includes the requested complete training");
        check(
            character.sheet().level == old.sheet().level &&
            character.sheet().scores == old.sheet().scores &&
            character.sheet().hit_point_modifiers == old.sheet().hit_point_modifiers &&
            character.sheet().hit_points == old.sheet().hit_points &&
            character.sheet().prepared_spells == old.sheet().prepared_spells &&
            preserved_advancement(character, old),
            "Reconstruction preserves attained levels, ASI, Constitution history, feat and spell choices");
        check(character.appearance() == old.appearance() &&
              std::equal(character.inventory().items().begin(),
                         character.inventory().items().end(), old.inventory().items().begin(),
                         old.inventory().items().end()),
              "Preview preserves appearance and full item records");
        for (const auto &grant : old.sheet().grants)
            check(std::find(character.sheet().grants.begin(), character.sheet().grants.end(),
                            grant) != character.sheet().grants.end(),
                  "Existing grants retain exact provenance");
        check(preview.vitals == previous.vitals,
              "Preview preserves wounds, effects, timers, death state and spent resources exactly");
        auto expected = party.checkpoint();
        expected.roster[id - 1] = preview;
        CampaignParty comparison(module());
        comparison.restore(expected);
        party.complete_training(id, *creation, selected);
        check(
            encode_campaign(party, nullptr, "training-fixture") ==
            encode_campaign(comparison, nullptr, "training-fixture"),
            "Confirmation changes only the selected character, exactly as previewed; gear, wealth, clock, RNG and roster are unchanged");
    }
    check(skill(party.member(1).character.sheet(), "stealth").bonus == 7,
          "Confirmed Criminal Expertise changes the check bonus");
    const auto completed = encode_campaign(party, nullptr, "training-fixture");
    rejects(
        [&]
    {
        party.complete_training(1, *creation, choices());
    });
    rejects(
        [&]
    {
        party.complete_training(3, *creation, {{"origin:languages", {"dwarvish", "orc"}}});
    });
    check(encode_campaign(party, nullptr, "training-fixture") == completed,
          "Completed training cannot be reopened as a respec");
    auto reloaded = decode_campaign(completed, *creation, *rules, "training-fixture", nullptr);
    CampaignParty restored(module());
    restored.restore(std::move(reloaded.party));
    check(encode_campaign(restored, nullptr, "training-fixture") == completed,
          "Completed training and unchanged campaign resources survive exact save/reload");
    auto members = restored.participants();
    members.push_back({99, "vanguard", "Enemy", 1, {6, 6}});
    auto combat = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, members}, 42);
    check(rules->restore(combat->save())->save() == combat->save(),
          "Completed training is accepted in the next combat and its checkpoint");

    auto partial = draft();
    partial.training = {{"origin:languages", {"elvish"}}, {"class:rogue", {"perception"}}};
    CampaignParty incomplete(module());
    const auto id = incomplete.add_pc(hero(partial));
    const auto pending = encode_campaign(incomplete, nullptr, "partial");
    auto replacement = choices();
    replacement["origin:languages"] = {"dwarvish", "orc"};
    rejects(
        [&]
    {
        incomplete.complete_training(id, *creation, replacement);
    });
    check(encode_campaign(incomplete, nullptr, "partial") == pending,
          "Already chosen entries cannot be replaced while other groups are pending");
    auto unconscious = incomplete.checkpoint();
    unconscious.roster[0].vitals = {0, false, "SRD1 0 0 1 2 0"};
    incomplete.restore(unconscious);
    incomplete.complete_training(id, *creation, choices());
    check(incomplete.member(id).vitals == unconscious.roster[0].vitals,
          "Training completion does not stabilize or heal an unconscious character");
}

void sage_training()
{
    auto rules = module();
    auto creation = srd5::character_rules();
    for (const auto &klass : creation->choices(CreationField::character_class))
    {
        auto d = draft(klass.id, "sage");
        const auto adjustments = creation->adjustments("sage");
        for (unsigned i = 0; i < adjustments.size(); ++i)
            if (adjustments[i].bonuses[3] == 0)
            {
                d.adjustment = i;
                break;
            }
        auto sheet = hero(d).sheet();
        check(sheet.scores[3] == 15, "Authored Intelligence stays 15");
        for (const auto id :
                {"arcana", "history"
                })
        {
            const auto &trained = skill(sheet, id);
            check(trained.proficient && trained.bonus == 4 && trained.sources.size() == 1 &&
                  trained.sources[0].source_id == "background:sage",
                  "Every starting class gets sourced +2 Sage proficiency");
        }
        const auto tool = creation->ability_check(sheet, 3, {}, "calligraphers_supplies");
        check(tool.total == 4 && tool.proficiency == 2 && tool.sources.size() == 1 &&
              tool.sources[0].id == "tool:calligraphers_supplies",
              "Fixed tool proficiency participates in ability-check API");
        const auto combined = creation->ability_check(sheet, 3, "arcana", "calligraphers_supplies");
        check(combined.total == 4 && combined.tool_advantage,
              "Applicable skill plus tool grants Advantage, not doubled proficiency");
        auto bad = sheet;
        std::erase_if(bad.grants,
                      [](const auto & g)
        {
            return g.id == "skill:arcana";
        });
        rejects(
            [&]
        {
            (void)rules->character_profile(bad, {});
        });
        bad = sheet;
        bad.grants.push_back({"skill:arcana", "background:sage", 1, {}});
        rejects(
            [&]
        {
            (void)rules->character_profile(bad, {});
        });
        if (klass.id == "rogue")
        {
            d.training = {{"class:rogue:expertise", {"arcana", "history"}}};
            sheet = hero(d).sheet();
            check(skill(sheet, "arcana").expertise && skill(sheet, "arcana").bonus == 6,
                  "Rogue may apply Expertise to background-granted Arcana");
            const auto original = rules->character_profile(sheet, {}).data;
            auto legacy = original;
            replace(legacy,
                    std::to_string(sheet.grants.size()) +
                    " \"feature:sneak_attack\" \"class:rogue\" 1 0",
                    std::to_string(sheet.grants.size() - 1));
            auto pc15 = legacy;
            replace(pc15, pc15.starts_with("PC35 ") ? "PC35" : "PC28", "PC15");
            auto prior = rules->create({{8, 8, std::vector<std::uint8_t>(64)},
                {   {1, "campaign-character", "Prior Sage", 0, {1, 1}, pc15},
                    {2, "vanguard", "Enemy", 1, {5, 1}}
                }},
            1);
            auto checkpoint = prior->save();
            replace(checkpoint, module()->identity().version, "0.6.26");
            check(rules->restore(checkpoint)->save() == prior->save(),
                  "Valid PC15 Sage Expertise remains accepted under the preceding rules identity");
            auto old = legacy;
            replace(old, old.starts_with("PC35 ") ? "PC35" : "PC28", "PC14");
            rejects(
                [&]
            {
                (void)rules->create({{8, 8, std::vector<std::uint8_t>(64)},
                    {   {1, "campaign-character", "Forged", 0, {1, 1}, old},
                        {2, "vanguard", "Enemy", 1, {5, 1}}
                    }},
                1);
            });
        }
    }
    const auto bytes = fixture("campaign-v11-sage-before.ogs");
    auto loaded = decode_campaign(bytes, *creation, *rules, "sage-migration", nullptr);
    CampaignParty party(module());
    party.restore(std::move(loaded.party));
    const auto &member = party.member(1);
    const auto &sheet = member.character.sheet();
    check(sheet.level == 3 && skill(sheet, "arcana").proficient &&
          skill(sheet, "history").proficient,
          "Prior-writer campaign receives owed fixed grants at reconstruction");
    check(member.vitals.hit_points == sheet.hit_points - 3 &&
          member.vitals.resources == "SRD2 0 1 1 0 0 0",
          "Migration preserves wounds and spent level-one/two slots");
    check(member.character.creation_data().training ==
    TrainingChoices{{"origin:languages", {"elvish", "dwarvish"}}} &&
    member.character.creation_data().cantrips ==
                    std::optional(std::vector<std::string> {"fire_bolt", "ray_of_frost"}),
                    "Migration preserves explicit choices");
    auto expected_body = bytes.substr(bytes.find('\n', bytes.find('\n') + 1) + 1);
    replace(expected_body, "0.6.25", rules->identity().version);
    expected_body = test::with_tactical_mind_grants(expected_body);
    const auto current = encode_campaign(party, nullptr, "sage-migration");
    auto again = decode_campaign(current, *creation, *rules, "sage-migration", nullptr);
    CampaignParty restored(module());
    restored.restore(std::move(again.party));
    check(encode_campaign(restored, nullptr, "sage-migration") == current,
          "Current Sage campaign is canonical after reload");
    check(current.substr(current.find('\n', current.find('\n') + 1) + 1) ==
          test::with_background_training_grants(expected_body),
          "Every prior campaign byte remains except identity and exactly three fixed Sage grants");
    auto members = party.participants();
    members[0].cell = {1, 1};
    members.push_back({99, "vanguard", "Enemy", 1, {6, 6}});
    const auto combat = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, members}, 42);
    check(rules->restore(combat->save())->save() == combat->save(),
          "New Sage recipe retains combat continuation");
    auto forged_combat = combat->save();
    replace(forged_combat, module()->identity().version, "0.6.25");
    rejects(
        [&]
    {
        (void)rules->restore(forged_combat);
    });
    party.award_experience(1800, "sage-four");
    auto choice = party.default_advancement(1);
    choice.abilities = {};
    choice.abilities[3] = 2;
    party.advance(1, choice);
    check(party.member(1).character.sheet().scores[3] == 18 &&
          skill(party.member(1).character.sheet(), "arcana").bonus == 6,
          "Intelligence ASI recomputes Sage skill bonus at level four");
    for (const auto &bad :
            {
                corrupt(current, "skill:arcana", "skill:nature"),
                corrupt(current, module()->identity().version, "0.6.25")
            })
        rejects(
            [&]
    {
        (void)decode_campaign(bad, *creation, *rules, "sage-migration", nullptr);
    });
}

void remaining_backgrounds()
{
    auto creation = srd5::character_rules();
    auto rules = module();
    for (const auto &klass : creation->choices(CreationField::character_class))
        for (const auto background :
                {"acolyte", "soldier"
                })
        {
            const bool acolyte = background == std::string_view("acolyte");
            auto d = draft(klass.id, background);
            auto sheet = hero(d).sheet();
            const auto first = acolyte ? "insight" : "athletics",
                       second = acolyte ? "religion" : "intimidation";
            for (const auto id :
                    {
                        first, second
                    })
            {
                const auto &trained = skill(sheet, id);
                check(trained.proficient && trained.sources.size() == 1 &&
                      trained.sources[0].source_id == "background:" + std::string(background),
                      "All starting classes receive fixed background provenance");
            }
            check(skill(sheet, first).bonus == 5 && skill(sheet, second).bonus == (acolyte ? 5 : 4),
                  "Independent fixed-background ability and proficiency totals");
            if (acolyte)
            {
                const auto result =
                    creation->ability_check(sheet, 3, "religion", "calligraphers_supplies");
                check(result.total == 5 && result.proficiency == 2 && result.tool_advantage,
                      "Acolyte skill and tool grant Advantage without stacking proficiency");
            }
            auto bad = sheet;
            std::erase_if(bad.grants,
                          [&](const auto & g)
            {
                return g.id == "skill:" + std::string(first);
            });
            rejects(
                [&]
            {
                (void)rules->character_profile(bad, {});
            });
            bad = sheet;
            bad.grants.push_back(
            {"skill:" + std::string(first), "background:" + std::string(background), 1, {}});
            rejects(
                [&]
            {
                (void)rules->character_profile(bad, {});
            });
            if (klass.id == "rogue")
            {
                d.training = {{"class:rogue", {first, "acrobatics", "perception", "persuasion"}},
                    {"class:rogue:expertise", {first, second}}
                };
                sheet = hero(d).sheet();
                check(
                    skill(sheet, first).bonus == 7 && skill(sheet, first).sources.size() == 3 &&
                    skill(sheet, second).bonus == (acolyte ? 7 : 6),
                    "Rogue Expertise accepts background skills; overlapping class and background grants do not stack");
            }
            auto old = rules->character_profile(sheet, {}).data;
            replace(old,
                    klass.id == "wizard"  ? "PC32"
                    : klass.id == "rogue" ? "PC35"
                    : "PC28",
                    "PC15");
            rejects(
                [&]
            {
                (void)rules->create({{8, 8, std::vector<std::uint8_t>(64)},
                    {   {1, "campaign-character", "Forged", 0, {1, 1}, old},
                        {2, "vanguard", "Enemy", 1, {5, 1}}
                    }},
                1);
            });
        }
    const auto bytes = fixture("campaign-v11-backgrounds-before.ogs");
    CampaignParty party(module());
    party.restore(
        decode_campaign(bytes, *creation, *rules, "backgrounds-migration", nullptr).party);
    for (MemberId id :
            {
                1, 2
            })
    {
        const auto &m = party.member(id);
        check(m.character.sheet().level == 3 &&
              m.vitals.hit_points == m.character.sheet().hit_points - 3,
              "Background migration retains attained levels and wounds");
        check(m.character.creation_data().training ==
        TrainingChoices{{"origin:languages", {"elvish", "dwarvish"}}},
        "Migration retains explicit choices");
        check(m.vitals.resources == (id == 1 ? "SRD2 0 1 1 0 0 0" : "SRD1 1 0 0 0 0"),
              "Migration retains spent spell/feat resources");
    }
    auto body = [](const auto & b)
    {
        return b.substr(b.find('\n', b.find('\n') + 1) + 1);
    };
    auto expected = body(bytes);
    replace(expected, "0.6.26", rules->identity().version);
    expected = test::with_alert_grants(
                   test::with_sneak_attack_grants(test::with_tactical_mind_grants(expected)));
    const auto current = encode_campaign(party, nullptr, "backgrounds-migration");
    check(
        body(current) == test::with_background_training_grants(expected, 2),
        "Every historical campaign byte remains except identity and exactly five owed fixed grants");
    CampaignParty again(module());
    again.restore(
        decode_campaign(current, *creation, *rules, "backgrounds-migration", nullptr).party);
    check(encode_campaign(again, nullptr, "backgrounds-migration") == current,
          "Migrated background campaign round trips canonically");
    rejects(
        [&]
    {
        (void)decode_campaign(corrupt(current, module()->identity().version, "0.6.26"),
        *creation, *rules, "backgrounds-migration", nullptr);
    });
    auto members = party.participants();
    members[0].cell = {1, 1};
    members[1].cell = {2, 1};
    members.push_back({99, "vanguard", "Enemy", 1, {6, 6}});
    const auto combat = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, members}, 42);
    check(rules->restore(combat->save())->save() == combat->save(),
          "PC28 preserves combat continuation");
    auto forged = combat->save();
    replace(forged, module()->identity().version, "0.6.26");
    rejects(
        [&]
    {
        (void)rules->restore(forged);
    });
    party.award_experience(3600, "backgrounds-four");
    for (MemberId id :
            {
                1, 2
            })
    {
        auto choice = party.default_advancement(id);
        choice.abilities = {};
        choice.abilities[id == 1 ? 4 : 0] = 2;
        party.advance(id, choice);
    }
    check(skill(party.member(1).character.sheet(), "insight").bonus == 6 &&
          skill(party.member(2).character.sheet(), "athletics").bonus == 6,
          "Level-four ability improvements recompute fixed skill modifiers");
}

void starting_styles()
{
    auto rules = module();
    auto creation = srd5::character_rules();
    for (const auto &klass : creation->choices(CreationField::character_class))
        for (const auto style :
                {"defense", "archery"
                })
        {
            auto d = draft(klass.id, "sage");
            d.training = {{"origin:languages", {"elvish", "dwarvish"}},
                {"class:fighter:fighting_style", {style}}
            };
            if (klass.id != "fighter")
            {
                rejects(
                    [&]
                {
                    (void)hero(d);
                });
                continue;
            }
            d.training["class:fighter"] = {"athletics", "history"};
            complete_mastery(d);
            auto c = hero(d);
            const auto &sheet = c.sheet();
            check(sheet.training.complete && std::find(sheet.grants.begin(), sheet.grants.end(),
                    FeatureGrant{"feat:" + std::string(style),
                                 "class:fighter:fighting_style",
                                 1,
                                 {}}) != sheet.grants.end(),
                  "Starting style has a distinct level-one entitlement and completes training");
            check(rules->character_profile(sheet, std::array<std::string, 1> {"breastplate"})
                  .armor_class == (style == std::string_view("defense") ? 17 : 16),
                  "Starting Defense adds exactly one AC with armor");
            check(rules->character_profile(sheet, {}).armor_class == 12 &&
                  rules->character_profile(sheet, std::array<std::string, 1> {"shield"})
                  .armor_class == 14,
                  "Starting Defense does not improve unarmored or shield-only AC");
            auto invalid = d;
            invalid.training["class:fighter:fighting_style"] = {"archery", "defense"};
            rejects(
                [&]
            {
                (void)hero(invalid);
            });
            invalid = d;
            invalid.training["class:fighter:fighting_style"] = {"dueling"};
            rejects(
                [&]
            {
                (void)hero(invalid);
            });
            auto profile =
                rules->character_profile(sheet, std::array<std::string, 1> {"shortbow"}).data;
            Encounter encounter{{8, 8, std::vector<std::uint8_t>(64)},
                {   {1, "campaign-character", "Starter", 0, {1, 1}, profile},
                    {99, "vanguard", "Enemy", 1, {5, 1}}
                }};
            auto battle = rules->create(encounter, 13);
            check(battle->snapshot().actor == 1, "Independent starting-style initiative seed");
            const auto actions = battle->legal_commands();
            const auto attack = std::find_if(actions.begin(), actions.end(),
                                             [](const auto & a)
            {
                return a.verb == "ranged";
            });
            check(attack != actions.end() && battle->submit(*attack),
                  "Starting style participates in ordinary attack");
            bool checked = false;
            for (const auto &m : battle->snapshot().log_messages)
                if (m.source.starts_with("{actor} -> {target}: d20"))
                    for (const auto &arg : m.arguments)
                        if (arg.name == "bonus")
                        {
                            check(
                                arg.value == (style == std::string_view("archery") ? "6" : "4"),
                                "Starting Archery contributes exactly +2 to the real ranged attack");
                            checked = true;
                        }
            check(checked && rules->restore(battle->save())->save() == battle->save(),
                  "Starting style and spent attack retain canonical combat continuation");
            auto style_only = sheet;
            std::erase_if(style_only.grants,
                          [](const auto & g)
            {
                return g.id.starts_with("mastery:");
            });
            profile =
                rules->character_profile(style_only, std::array<std::string, 1> {"shortbow"}).data;
            replace(profile, "PC28", "PC17");
            encounter.participants[0].character_profile = profile;
            rejects(
                [&]
            {
                (void)rules->create(encounter, 13);
            });
            CampaignParty p(module());
            auto id = p.add_pc(c);
            p.award_experience(2700, "starting-style");
            for (int i = 0; i < 2; ++i)
                p.advance(id, p.default_advancement(id));
            auto choice = p.default_advancement(id);
            choice.feat = style;
            choice.abilities = {};
            const auto old = encode_campaign(p, nullptr, "starting-style");
            rejects(
                [&]
            {
                p.advance(id, choice);
            });
            check(encode_campaign(p, nullptr, "starting-style") == old,
                  "Duplicate style cannot consume a level-four entitlement");
            choice = p.default_advancement(id);
            choice.abilities = {};
            choice.abilities[2] = 2;
            p.advance(id, choice);
            check(p.member(id).character.sheet().scores[2] == 19,
                  "Starting style coexists with a Constitution ASI");
            const auto bytes = encode_campaign(p, nullptr, "starting-style");
            CampaignParty again(module());
            again.restore(
                decode_campaign(bytes, *creation, *rules, "starting-style", nullptr).party);
            check(encode_campaign(again, nullptr, "starting-style") == bytes,
                  "Style plus retroactive Constitution HP history survives reload");
        }
    const auto old = fixture("campaign-v11-styles-before.ogs");
    CampaignParty p(module());
    p.restore(decode_campaign(old, *creation, *rules, "style-migration", nullptr).party);
    auto body = [](const auto & s)
    {
        return s.substr(s.find('\n', s.find('\n') + 1) + 1);
    };
    auto expected = body(old);
    replace(expected, "0.6.28", rules->identity().version);
    expected = test::with_alert_grants(
                   test::with_sneak_attack_grants(test::with_tactical_mind_grants(expected)));
    check(
        body(encode_campaign(p, nullptr, "style-migration")) == expected,
        "Actual prior campaign retains every choice, grant, wound and resource byte except module identity");
    auto old_combat = fixture("combat-v14-styles-before.save");
    auto expected_combat = old_combat;
    replace(expected_combat, "0.6.28", rules->identity().version);
    check(rules->restore(old_combat)->save() == expected_combat,
          "Actual PC17 combat retains all recipes and continuation state");
    for (MemberId id :
            {
                1, 2, 3, 4
            })
    {
        const auto before = p.member(id);
        check(!before.character.sheet().training.complete,
              "Old Fighters retain a pending style regardless of attained level");
        auto choices = before.character.creation_data().training;
        choices["class:fighter"] = {"athletics", "history"};
        choices["class:fighter:fighting_style"] = {id == 2 ? "archery" : "defense"};
        choices =
            with_advancement_training(before.character, *creation, *rules, std::move(choices));
        if (id == 2)
        {
            auto duplicate = choices;
            duplicate["class:fighter:fighting_style"] = {"defense"};
            const auto bytes = encode_campaign(p, nullptr, "style-migration");
            rejects(
                [&]
            {
                p.complete_training(id, *creation, duplicate);
            });
            check(encode_campaign(p, nullptr, "style-migration") == bytes,
                  "Completion cannot duplicate an old advancement feat");
        }
        p.complete_training(id, *creation, choices);
        const auto &after = p.member(id);
        check(
            after.character.sheet().training.complete && after.vitals == before.vitals &&
            preserved_advancement(after.character, before.character) &&
            after.character.sheet().hit_points == before.character.sheet().hit_points,
            "Completing old style preserves vitals, levels, HP history and previous feat choices");
    }
    const auto bytes = encode_campaign(p, nullptr, "style-migration");
    CampaignParty again(module());
    again.restore(decode_campaign(bytes, *creation, *rules, "style-migration", nullptr).party);
    check(encode_campaign(again, nullptr, "style-migration") == bytes,
          "Completed historical starting styles save canonically");
    rejects(
        [&]
    {
        (void)decode_campaign(corrupt(bytes, module()->identity().version, "0.6.28"), *creation,
        *rules, "style-migration", nullptr);
    });
}

void freeze_cunning_action()
{
    auto rules = module();
    check(rules->identity().version == "0.6.34", "Requires actual writer before Cunning Action");
    CampaignParty party(module());
    for (const auto *background :
            {"sage", "criminal", "acolyte", "soldier"
            })
    {
        auto d = draft("rogue", background);
        d.race = "orc";
        d.name = std::string("Rogue ") + background;
        d.training = choices();
        d.training["class:rogue:expertise"] = {"investigation", "perception"};
        if (std::string_view(background) == "soldier")
            d.training["background:soldier:gaming_set"] = {"dice"};
        party.add_pc(hero(d));
    }
    auto state = party.checkpoint();
    for (auto &member : state.roster)
    {
        member.vitals.hit_points -= 2;
        member.vitals.resources = "SRD1 0 0 0 0 0";
    }
    party.restore(state);
    const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures";
    std::ofstream campaign(root / "campaign-v11-cunning-before.ogs", std::ios::binary);
    campaign << encode_campaign(party, nullptr, "cunning");
    check(bool(campaign), "Write prior Cunning Action campaign");
    std::vector<Participant> members;
    for (const auto &member : party.state().roster)
        members.push_back(
    {
        member.id,
        "campaign-character",
        member.character.sheet().name,
        0,
        {int(member.id), 1},
        rules
        ->character_profile(member.character.sheet(), std::array<std::string, 1>{"dagger"})
        .data,
        member.vitals});
    members.push_back({99, "vanguard", "Enemy", 1, {10, 6}});
    auto combat = rules->create({{12, 8, std::vector<std::uint8_t>(96)}, members}, 42);
    auto act = [&](std::string_view verb)
    {
        const auto legal = combat->legal_commands();
        const auto found = std::find_if(legal.begin(), legal.end(),
                                        [&](const auto & c)
        {
            return c.verb == verb;
        });
        check(found != legal.end() && combat->submit(*found), "Prior-writer command accepted");
    };
    for (unsigned turns = 0; combat->snapshot().actor != 1; ++turns)
    {
        check(turns < 8, "Find first Rogue turn");
        act("end");
    }
    act("dash");
    act("adrenaline_rush");
    std::ofstream out(root / "combat-v13-cunning-before.save", std::ios::binary);
    out << combat->save();
    check(bool(out), "Write prior Cunning Action spent-budget combat");
}

void cunning_prior_writer()
{
    auto rules = module();
    auto creation = srd5::character_rules();
    const auto prior = fixture("campaign-v11-cunning-before.ogs");
    CampaignParty party(module());
    party.restore(decode_campaign(prior, *creation, *rules, "cunning", nullptr).party);
    auto body = [](const auto & text)
    {
        return text.substr(text.find('\n', text.find('\n') + 1) + 1);
    };
    auto expected = body(prior);
    replace(expected, "0.6.34", rules->identity().version);
    expected = test::with_alert_grants(
                   test::with_sneak_attack_grants(test::with_tactical_mind_grants(expected)));
    check(
        body(encode_campaign(party, nullptr, "cunning")) == expected,
        "Prior Rogue campaign retains every field except module identity/checksum and fixed Tactical Mind grant");
    check(party.state().roster.size() == 4, "Cunning baseline covers all four backgrounds");
    for (const auto &member : party.state().roster)
        check(member.character.sheet().level == 1 && !member.character.sheet().training.complete &&
              member.character.sheet().training.masteries.empty() &&
              member.vitals.hit_points == member.character.sheet().hit_points - 2,
              "Prior Rogues retain existing training, level and wounds with mastery pending");
    const auto frozen = fixture("combat-v13-cunning-before.save");
    auto migrated = frozen;
    replace(migrated, "0.6.34", rules->identity().version);
    auto combat = rules->restore(frozen);
    check(combat->save() == migrated, "Prior Cunning baseline preserves exact combat continuation");
    const auto snapshot = combat->snapshot();
    const auto actor = std::find_if(snapshot.combatants.begin(), snapshot.combatants.end(),
                                    [](const auto & c)
    {
        return c.id == 1;
    });
    check(snapshot.actor == 1 && actor != snapshot.combatants.end() && !actor->action &&
          !actor->bonus_action && actor->movement_feet == 90 && actor->temporary_hp.amount == 2,
          "Both spent action budgets, stacked Dash movement and Temporary HP survive");
}

void freeze_druid_herbalism()
{
    auto rules = module();
    check(rules->identity().version == "0.6.32",
          "Requires actual writer before Druid Herbalism Kit choices");
    CampaignParty party(module());
    for (const auto *background :
            {"sage", "criminal", "acolyte", "soldier"
            })
    {
        auto d = draft("druid", background);
        d.name = std::string("Druid ") + background;
        d.training = {{"origin:languages", {"elvish", "dwarvish"}},
            {"class:druid", {"nature", "medicine"}}
        };
        party.add_pc(hero(d));
    }
    auto state = party.checkpoint();
    for (auto &member : state.roster)
    {
        member.vitals.hit_points -= 2;
        member.vitals.resources = "SRD1 0 0 0 0 0";
    }
    party.restore(state);
    const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures";
    std::ofstream campaign(root / "campaign-v11-druid-herbalism-before.ogs", std::ios::binary);
    campaign << encode_campaign(party, nullptr, "druid-herbalism");
    check(bool(campaign), "Write actual prior Druid campaign");
    std::vector<Participant> members;
    for (const auto &member : party.state().roster)
        members.push_back({member.id,
                           "campaign-character",
                           member.character.sheet().name,
                           0,
    {int(member.id), 1},
    rules->character_profile(member.character.sheet(), {}).data,
    member.vitals});
    members.push_back({99, "vanguard", "Enemy", 1, {6, 6}});
    std::ofstream combat(root / "combat-v14-druid-herbalism-before.save", std::ios::binary);
    combat << rules->create({{8, 8, std::vector<std::uint8_t>(64)}, members}, 13)->save();
    check(bool(combat), "Write actual prior Druid combat");
}

void freeze_monk_tools()
{
    auto rules = module();
    check(rules->identity().version == "0.6.31", "Requires actual writer before Monk tool choices");
    CampaignParty party(module());
    for (const auto *background :
            {"sage", "criminal", "acolyte", "soldier"
            })
    {
        auto d = draft("monk", background);
        d.name = std::string("Monk ") + background;
        d.training = {{"origin:languages", {"elvish", "dwarvish"}},
            {"class:monk", {"acrobatics", "insight"}}
        };
        party.add_pc(hero(d));
    }
    auto state = party.checkpoint();
    for (auto &member : state.roster)
    {
        member.vitals.hit_points -= 2;
        member.vitals.resources = "SRD1 0 0 0 0 0";
    }
    party.restore(state);
    const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures";
    std::ofstream campaign(root / "campaign-v11-monk-tools-before.ogs", std::ios::binary);
    campaign << encode_campaign(party, nullptr, "monk-tools");
    check(bool(campaign), "Write actual prior Monk campaign");
    std::vector<Participant> members;
    for (const auto &member : party.state().roster)
        members.push_back({member.id,
                           "campaign-character",
                           member.character.sheet().name,
                           0,
    {int(member.id), 1},
    rules->character_profile(member.character.sheet(), {}).data,
    member.vitals});
    members.push_back({99, "vanguard", "Enemy", 1, {6, 6}});
    std::ofstream combat(root / "combat-v14-monk-tools-before.save", std::ios::binary);
    combat << rules->create({{8, 8, std::vector<std::uint8_t>(64)}, members}, 13)->save();
    check(bool(combat), "Write actual prior Monk combat");
}

void freeze_bard_instruments()
{
    auto rules = module();
    check(rules->identity().version == "0.6.30",
          "Requires actual writer before Bard instrument choices");
    CampaignParty party(module());
    for (const auto *background :
            {"sage", "criminal", "acolyte", "soldier"
            })
    {
        auto d = draft("bard", background);
        d.name = std::string("Bard ") + background;
        d.training = {{"origin:languages", {"elvish", "dwarvish"}},
            {"class:bard", {"performance", "persuasion", "perception"}}
        };
        party.add_pc(hero(d));
    }
    auto state = party.checkpoint();
    for (auto &member : state.roster)
    {
        member.vitals.hit_points -= 2;
        member.vitals.resources = "SRD1 0 0 0 0 0";
    }
    party.restore(state);
    const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures";
    std::ofstream campaign(root / "campaign-v11-bard-instruments-before.ogs", std::ios::binary);
    campaign << encode_campaign(party, nullptr, "bard-instruments");
    check(bool(campaign), "Write actual prior Bard campaign");
    std::vector<Participant> members;
    for (const auto &member : party.state().roster)
        members.push_back({member.id,
                           "campaign-character",
                           member.character.sheet().name,
                           0,
    {int(member.id), 1},
    rules->character_profile(member.character.sheet(), {}).data,
    member.vitals});
    members.push_back({99, "vanguard", "Enemy", 1, {6, 6}});
    std::ofstream combat(root / "combat-v14-bard-instruments-before.save", std::ios::binary);
    combat << rules->create({{8, 8, std::vector<std::uint8_t>(64)}, members}, 13)->save();
    check(bool(combat), "Write actual prior Bard combat");
}

void monk_tool_prior_writer()
{
    auto rules = module();
    auto creation = srd5::character_rules();
    const auto saved = fixture("campaign-v11-monk-tools-before.ogs");
    CampaignParty party(module());
    party.restore(decode_campaign(saved, *creation, *rules, "monk-tools", nullptr).party);
    auto body = [](const auto & text)
    {
        return text.substr(text.find('\n', text.find('\n') + 1) + 1);
    };
    auto expected = body(saved);
    replace(expected, "0.6.31", rules->identity().version);
    expected = test::with_alert_grants(
                   test::with_sneak_attack_grants(test::with_tactical_mind_grants(expected)));
    check(body(encode_campaign(party, nullptr, "monk-tools")) == expected,
          "Prior Monk campaign retains all recorded data except module identity");
    check(party.state().roster.size() == 4, "Prior Monk fixture covers all four backgrounds");
    for (const auto &member : party.state().roster)
    {
        check(member.character.creation_data().training.at("class:monk") ==
              std::vector<std::string> {"acrobatics", "insight"},
              "Prior Monk skills retain selection order");
        check(member.vitals.hit_points == member.character.sheet().hit_points - 2,
              "Prior Monk wounds survive");
        check(member.vitals.resources == "SRD1 0 0 0 0 0", "Prior Monk resource state survives");
        check(!member.character.creation_data().training.contains("class:monk:tools"),
              "Prior Monk instrument selections are not invented");
        check(!member.character.sheet().training.complete,
              "Old Monk instrument choices remain pending");
    }
    const auto old = party.checkpoint();
    for (const auto &member : old.roster)
    {
        auto selected = member.character.creation_data().training;
        selected["class:monk:tools"] = {"smiths_tools"};
        if (member.character.creation_data().background == "soldier")
            selected["background:soldier:gaming_set"] = {"dice"};
        selected =
            with_advancement_training(member.character, *creation, *rules, std::move(selected));
        party.complete_training(member.id, *creation, selected);
        check(party.member(member.id).character.sheet().training.complete &&
              party.member(member.id).vitals == member.vitals,
              "Completing historical Monk choices preserves wounds and resources");
    }
    auto frozen = fixture("combat-v14-monk-tools-before.save");
    auto migrated = frozen;
    replace(migrated, "0.6.31", rules->identity().version);
    check(rules->restore(frozen)->save() == migrated,
          "Prior Monk combat retains profiles, HP, resources and RNG");
}

void bard_instrument_prior_writer()
{
    auto rules = module();
    auto creation = srd5::character_rules();
    const auto saved = fixture("campaign-v11-bard-instruments-before.ogs");
    CampaignParty party(module());
    party.restore(decode_campaign(saved, *creation, *rules, "bard-instruments", nullptr).party);
    auto body = [](const auto & text)
    {
        return text.substr(text.find('\n', text.find('\n') + 1) + 1);
    };
    auto expected = body(saved);
    replace(expected, "0.6.30", rules->identity().version);
    expected = test::with_alert_grants(
                   test::with_sneak_attack_grants(test::with_tactical_mind_grants(expected)));
    check(body(encode_campaign(party, nullptr, "bard-instruments")) == expected,
          "Prior Bard campaign retains all recorded data except module identity");
    check(party.state().roster.size() == 4, "Prior Bard fixture covers all four backgrounds");
    for (const auto &member : party.state().roster)
    {
        check(member.character.creation_data().training.at("class:bard") ==
              std::vector<std::string> {"performance", "persuasion", "perception"},
              "Prior Bard skills retain selection order");
        check(member.vitals.hit_points == member.character.sheet().hit_points - 2,
              "Prior Bard wounds survive");
        check(member.vitals.resources == "SRD1 0 0 0 0 0", "Prior Bard resource state survives");
        check(!member.character.creation_data().training.contains("class:bard:instruments"),
              "Prior Bard instrument selections are not invented");
        check(!member.character.sheet().training.complete,
              "Old Bard instrument choices remain pending");
    }
    const auto old = party.checkpoint();
    for (const auto &member : old.roster)
    {
        auto selected = member.character.creation_data().training;
        selected["class:bard:instruments"] = {"flute", "lute", "viol"};
        if (member.character.creation_data().background == "soldier")
            selected["background:soldier:gaming_set"] = {"dice"};
        selected =
            with_advancement_training(member.character, *creation, *rules, std::move(selected));
        party.complete_training(member.id, *creation, selected);
        check(party.member(member.id).character.sheet().training.complete &&
              party.member(member.id).vitals == member.vitals,
              "Completing historical Bard choices preserves wounds and resources");
    }
    auto frozen = fixture("combat-v14-bard-instruments-before.save");
    auto migrated = frozen;
    replace(migrated, "0.6.30", rules->identity().version);
    check(rules->restore(frozen)->save() == migrated,
          "Prior Bard combat retains profiles, HP, resources and RNG");
}

void freeze_soldier_gaming()
{
    auto rules = module();
    auto creation = srd5::character_rules();
    check(rules->identity().version == "0.6.33",
          "Requires actual writer before Soldier Gaming Set choices");
    CampaignParty party(module());
    for (const auto &klass : creation->choices(CreationField::character_class))
    {
        auto d = draft(klass.id, "soldier");
        d.training = {{"origin:languages", {"elvish", "dwarvish"}}};
        if (klass.id == "rogue")
        {
            d.training = choices();
            d.training["class:rogue:expertise"] = {"investigation", "perception"};
        }
        if (klass.id == "fighter")
            d.training["class:fighter:fighting_style"] = {"archery"};
        d.training["class:" + klass.id] = chosen_class_skills(klass.id);
        if (klass.id == "rogue")
            d.training["class:rogue:expertise"] = {d.training["class:rogue"][0],
                                                   d.training["class:rogue"][1]
                                                  };
        if (klass.id == "bard")
            d.training["class:bard:instruments"] = {"flute", "lute", "viol"};
        if (klass.id == "monk")
            d.training["class:monk:tools"] = {"smiths_tools"};
        const auto id = party.add_pc(hero(d));
        if (klass.id == "fighter" || klass.id == "cleric" || klass.id == "wizard")
        {
            party.award_experience(2700, "skills:" + klass.id);
            while (party.member(id).character.sheet().level < 4)
                party.advance(id, party.default_advancement(id));
        }
        party.remove(id);
    }
    auto state = party.checkpoint();
    for (auto &m : state.roster)
    {
        m.vitals.hit_points -= 1;
        const auto &klass = m.character.sheet().character_class;
        m.vitals.resources = klass == "Fighter"                       ? "SRD1 1 0 0 0 0"
                             : klass == "Wizard" || klass == "Cleric" ? "SRD2 0 1 1 0 0 0"
                             : "SRD1 0 0 0 0 0";
    }
    party.restore(state);
    const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures";
    std::ofstream out(root / "campaign-v11-soldier-gaming-before.ogs", std::ios::binary);
    out << encode_campaign(party, nullptr, "soldier-gaming");
    check(bool(out), "Write prior Soldier gaming campaign");
    std::vector<Participant> members;
    for (const auto &m : party.state().roster)
        members.push_back({m.id,
                           "campaign-character",
                           m.character.sheet().name,
                           0,
    {int(m.id), 1},
    rules->character_profile(m.character.sheet(), {}).data,
    m.vitals});
    members.push_back({99, "vanguard", "Enemy", 1, {14, 6}});
    std::ofstream combat(root / "combat-v14-soldier-gaming-before.save", std::ios::binary);
    combat << rules->create({{16, 8, std::vector<std::uint8_t>(128)}, members}, 13)->save();
    check(bool(combat), "Write prior Soldier gaming combat");
}

void freeze_class_skills()
{
    auto rules = module();
    auto creation = srd5::character_rules();
    check(rules->identity().version == "0.6.29",
          "Requires actual writer before all-class skill choices");
    CampaignParty party(module());
    for (const auto &klass : creation->choices(CreationField::character_class))
    {
        auto d = draft(klass.id, "sage");
        d.training = {{"origin:languages", {"elvish", "dwarvish"}}};
        if (klass.id == "rogue")
        {
            d.training = choices();
            d.training["class:rogue:expertise"] = {"investigation", "perception"};
        }
        if (klass.id == "fighter")
            d.training["class:fighter:fighting_style"] = {"archery"};
        const auto id = party.add_pc(hero(d));
        if (klass.id == "fighter" || klass.id == "cleric" || klass.id == "wizard")
        {
            party.award_experience(2700, "skills:" + klass.id);
            while (party.member(id).character.sheet().level < 4)
                party.advance(id, party.default_advancement(id));
        }
        party.remove(id);
    }
    auto state = party.checkpoint();
    for (auto &m : state.roster)
    {
        m.vitals.hit_points -= 1;
        const auto &klass = m.character.sheet().character_class;
        m.vitals.resources = klass == "Fighter"                       ? "SRD1 1 0 0 0 0"
                             : klass == "Wizard" || klass == "Cleric" ? "SRD2 0 1 1 0 0 0"
                             : "SRD1 0 0 0 0 0";
    }
    party.restore(state);
    const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures";
    std::ofstream out(root / "campaign-v11-class-skills-before.ogs", std::ios::binary);
    out << encode_campaign(party, nullptr, "class-skills");
    check(bool(out), "Write prior class-skill campaign");
    std::vector<Participant> members;
    for (const auto &m : party.state().roster)
        members.push_back({m.id,
                           "campaign-character",
                           m.character.sheet().name,
                           0,
    {int(m.id), 1},
    rules->character_profile(m.character.sheet(), {}).data,
    m.vitals});
    members.push_back({99, "vanguard", "Enemy", 1, {14, 6}});
    std::ofstream combat(root / "combat-v14-class-skills-before.save", std::ios::binary);
    combat << rules->create({{16, 8, std::vector<std::uint8_t>(128)}, members}, 13)->save();
    check(bool(combat), "Write prior class-skill combat");
}

void freeze_styles()
{
    auto rules = module();
    check(rules->identity().version == "0.6.28", "Requires actual writer before starting styles");
    CampaignParty party(module());
    for (int i = 0; i < 4; ++i)
    {
        auto d = draft("fighter", "sage");
        d.training = {{"origin:languages", {"elvish", "dwarvish"}}};
        party.add_pc(hero(d));
    }
    party.award_experience(2700, "style-migration");
    for (MemberId id :
            {
                2, 3, 4
            })
    {
        for (int i = 0; i < 2; ++i)
            party.advance(id, party.default_advancement(id));
        auto choice = party.default_advancement(id);
        if (id < 4)
        {
            choice.feat = id == 2 ? "defense" : "archery";
            choice.abilities = {};
        }
        else
        {
            choice.abilities = {};
            choice.abilities[2] = 2;
        }
        party.advance(id, choice);
    }
    auto state = party.checkpoint();
    for (auto &m : state.roster)
    {
        m.vitals.hit_points -= 2;
        m.vitals.resources = "SRD1 1 0 0 0 0";
    }
    party.restore(state);
    std::ofstream out(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "tests/fixtures/campaign-v11-styles-before.ogs",
                      std::ios::binary);
    out << encode_campaign(party, nullptr, "style-migration");
    check(bool(out), "Write actual prior style fixture");
    auto members = party.participants();
    for (unsigned i = 0; i < members.size(); ++i)
        members[i].cell = {int(i + 1), 1};
    members.push_back({99, "vanguard", "Enemy", 1, {6, 6}});
    std::ofstream combat(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                         "tests/fixtures/combat-v14-styles-before.save",
                         std::ios::binary);
    combat << rules->create({{8, 8, std::vector<std::uint8_t>(64)}, members}, 13)->save();
    check(bool(combat), "Write actual prior style combat");
}

void freeze_backgrounds()
{
    auto rules = module();
    check(rules->identity().version == "0.6.26", "Freeze requires actual pre-background writer");
    CampaignParty party(module());
    for (const auto background :
            {"acolyte", "soldier"
            })
    {
        auto d =
            draft(background == std::string_view("acolyte") ? "cleric" : "fighter", background);
        d.training = {{"origin:languages", {"elvish", "dwarvish"}}};
        party.add_pc(hero(d));
    }
    party.award_experience(1800, "backgrounds-migration");
    for (MemberId id :
            {
                1, 2
            })
        for (int i = 0; i < 2; ++i)
            party.advance(id, party.default_advancement(id));
    auto state = party.checkpoint();
    for (auto &m : state.roster)
    {
        m.vitals.hit_points -= 3;
        m.vitals.resources = m.id == 1 ? "SRD2 0 1 1 0 0 0" : "SRD1 1 0 0 0 0";
    }
    party.restore(std::move(state));
    std::ofstream out(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "tests/fixtures/campaign-v11-backgrounds-before.ogs",
                      std::ios::binary);
    out << encode_campaign(party, nullptr, "backgrounds-migration");
    check(bool(out), "Write actual prior background fixture");
}

void freeze_sage()
{
    auto rules = module();
    check(rules->identity().version == "0.6.25", "Freeze requires actual pre-Sage writer");
    auto d = draft("wizard", "sage");
    d.cantrips = std::vector<std::string> {"fire_bolt", "ray_of_frost"};
    d.training = {{"origin:languages", {"elvish", "dwarvish"}}};
    CampaignParty party(module());
    auto id = party.add_pc(hero(d));
    party.award_experience(900, "sage-migration");
    for (int i = 0; i < 2; ++i)
        party.advance(id, party.default_advancement(id));
    auto state = party.checkpoint();
    state.roster[0].vitals.hit_points -= 3;
    state.roster[0].vitals.resources = "SRD2 0 1 1 0 0 0";
    party.restore(std::move(state));
    std::ofstream out(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "tests/fixtures/campaign-v11-sage-before.ogs",
                      std::ios::binary);
    out << encode_campaign(party, nullptr, "sage-migration");
    check(bool(out), "Write actual prior-writer fixture");
}

#include "scholar_checks.h"
#include "cunning_checks.h"
#include "sneak_baseline.h"
#include "rogue_attack_checks.h"
#include "style_route_checks.h"
#include "light_attack_baseline.h"
#include "light_attack_checks.h"
#include "mastery_baseline.h"
#include "alert_checks.h"
#include "skilled_checks.h"
} // namespace

int main(int argc, char **argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--alert")
        {
            alert_checks::run();
            std::cout << "Alert acceptance passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--freeze-alert")
        {
            alert_checks::freeze();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--freeze-skilled")
        {
            skilled_checks::freeze();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--freeze-mastery")
        {
            mastery_baseline::freeze();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--mastery-baseline")
        {
            mastery_baseline::verify();
            std::cout << "Mastery prior-writer checks passed\n";
            return 0;
        }
        if (argc == 4 && std::string_view(argv[1]) == "--verify-light-style-ui")
        {
            style_route_checks::verify_ui(argv[2], argv[3], true);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--light-attacks")
        {
            light_attack_checks::run();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--freeze-hands")
        {
            light_attack_baseline::freeze_hands();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--hands-baseline")
        {
            light_attack_baseline::verify_hands_baseline();
            return 0;
        }
        if (argc == 3 && std::string_view(argv[1]) == "--verify-hands-ui")
        {
            light_attack_baseline::verify_hands_ui(argv[2]);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--hands-ui")
        {
            light_attack_baseline::write_hands_ui_fixture();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--freeze-light")
        {
            light_attack_baseline::freeze();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--light-baseline")
        {
            light_attack_baseline::verify();
            std::cout << "Light prior-writer checks passed\n";
            return 0;
        }
        if (argc == 4 && std::string_view(argv[1]) == "--verify-style-ui")
        {
            style_route_checks::verify_ui(argv[2], argv[3]);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--style-routes")
        {
            style_route_checks::run();
            std::cout << "Fighting Style route checks passed\n";
            return 0;
        }
        if (argc == 3 && std::string_view(argv[1]) == "--verify-rogue-ui")
        {
            rogue_attack_checks::verify_ui(argv[2]);
            return 0;
        }
        if (argc == 3 && std::string_view(argv[1]) == "--verify-scholar")
        {
            scholar_checks::verify_ui(argv[2]);
            return 0;
        }
        if (argc == 3 && std::string_view(argv[1]) == "--verify-review")
        {
            verify_review_result(argv[2]);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--rogue-attacks")
        {
            rogue_attack_checks::run();
            std::cout << "Rogue attacks checks passed\n";
            return 0;
        }
        if (argc == 2)
        {
            if (std::string_view(argv[1]) == "--freeze-sneak")
                sneak_baseline::freeze();
            else if (std::string_view(argv[1]) == "--freeze-cunning")
                freeze_cunning_action();
            else if (std::string_view(argv[1]) == "--freeze-soldier-gaming")
                freeze_soldier_gaming();
            else if (std::string_view(argv[1]) == "--freeze-druid-herbalism")
                freeze_druid_herbalism();
            else if (std::string_view(argv[1]) == "--freeze-monk-tools")
                freeze_monk_tools();
            else if (std::string_view(argv[1]) == "--freeze-bard-instruments")
                freeze_bard_instruments();
            else if (std::string_view(argv[1]) == "--freeze-class-skills")
                freeze_class_skills();
            else if (std::string_view(argv[1]) == "--freeze-styles")
                freeze_styles();
            else if (std::string_view(argv[1]) == "--freeze-backgrounds")
                freeze_backgrounds();
            else
                freeze_sage();
            return 0;
        }
        bool failed = false;
        auto run = [&](auto test, const char *name)
        {
            try
            {
                test();
            }
            catch (const std::exception &e)
            {
                std::cerr << name << ": " << e.what() << "\n";
                failed = true;
            }
        };
        run(alert_checks::run, "Alert");
        run(mastery_baseline::verify, "Mastery prior writer");
        run(light_attack_checks::run, "Light attacks");
        run(light_attack_baseline::verify_hands_baseline, "Hand baseline");
        run(light_attack_baseline::verify, "Light baseline");
        run(style_route_checks::run, "Fighting Style routes");
        run(rogue_attack_checks::run, "Rogue attacks");
        run(scholar_checks::run, "Scholar");
        run(sneak_baseline::verify, "Sneak baseline");
        run(cunning_checks::run, "Cunning Action");
        run(cunning_prior_writer, "Cunning prior writer");
        run(soldier_gaming, "Soldier gaming");
        run(druid_herbalism, "druid_herbalism");
        run(monk_tools, "monk_tools");
        run(monk_tool_prior_writer, "monk_tool_prior_writer");
        run(bard_instruments, "bard_instruments");
        run(bard_instrument_prior_writer, "bard_instrument_prior_writer");
        run(all_class_skills, "all_class_skills");
        run(sage_training, "sage_training");
        run(remaining_backgrounds, "remaining_backgrounds");
        run(starting_styles, "starting_styles");
        run(creation_controls, "creation_controls");
        run(preset_training, "preset_training");
        run(grants_and_checks, "grants_and_checks");
        run(invalid_choices, "invalid_choices");
        run(persistence, "persistence");
        run(complete_saved_training, "complete_saved_training");
        run(draft_review_editor, "draft_review_editor");
        if (failed)
            return 1;
        write_review_fixture();
        std::cout << "Training grant and creation tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
