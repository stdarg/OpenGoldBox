#include "combat_fixture.h"
#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
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
    bool caught = false;
    try
    {
        f();
    }
    catch (const std::exception &)
    {
        caught = true;
    }
    check(caught, "Invalid choice must reject");
}
auto module()
{
    return srd5::load(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "data/rules/srd-5.2.1/combat.rules");
}
Character character(std::string klass)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = klass;
    d.alignment = "neutral_good";
    d.background = "sage";
    d.name = "Advancement " + klass;
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    return Character(*srd5::character_rules(), d, {});
}
std::string saved(const CampaignParty &p)
{
    return encode_campaign(p, nullptr, "advancement-fixture");
}
bool offers(const CombatSession &s, std::string_view verb)
{
    const auto commands = s.legal_commands();
    return std::any_of(commands.begin(), commands.end(),
                       [&](const auto &c)
                       {
                           return c.verb == verb;
                       });
}
Command command(const CombatSession &s, std::string_view verb)
{
    for (const auto &c : s.legal_commands())
        if (c.verb == verb)
            return c;
    throw std::runtime_error("Missing command: " + std::string(verb));
}
CombatantView unit(const CombatSession &s, EntityId id)
{
    for (const auto &c : s.snapshot().combatants)
        if (c.id == id)
            return c;
    throw std::runtime_error("Missing actor");
}
auto duel(const RulesModule &rules, const CampaignParty &party)
{
    auto participants = party.participants();
    participants[0].cell = {1, 1};
    participants.push_back({99, "vanguard", "Target", 1, {8, 1}});
    Encounter encounter{{30, 9, std::vector<std::uint8_t>(270)}, participants};
    for (unsigned seed = 0; seed < 100; ++seed)
    {
        auto combat = rules.create(encounter, seed);
        if (combat->snapshot().actor == participants[0].id)
            return combat;
    }
    throw std::runtime_error("No first initiative seed");
}
void dwarf_class_sources()
{
    for (const auto &[klass, die] : std::vector<std::pair<std::string, int>>{{"barbarian", 12},
                                                                             {"bard", 8},
                                                                             {"cleric", 8},
                                                                             {"druid", 8},
                                                                             {"fighter", 10},
                                                                             {"monk", 8},
                                                                             {"paladin", 10},
                                                                             {"ranger", 10},
                                                                             {"rogue", 8},
                                                                             {"sorcerer", 6},
                                                                             {"warlock", 8},
                                                                             {"wizard", 6}})
    {
        auto draft = character(klass).creation_data();
        draft.race = "dwarf";
        draft.rolls[2] = {{6, 4, 4, 1}, 3};
        const auto adjustments = srd5::character_rules()->adjustments("sage");
        for (unsigned i = 0; i < adjustments.size(); ++i)
            if (adjustments[i].bonuses[2] == 0)
            {
                draft.adjustment = i;
                break;
            }
        CampaignParty party(module());
        auto id = party.add_pc(Character(*srd5::character_rules(), draft, {}));
        party.award_experience(2700, "dwarf-source");
        const unsigned maximum =
            klass == "fighter" || klass == "cleric" || klass == "wizard" ? 4 : 1;
        for (unsigned level = 1; level <= maximum; ++level)
        {
            if (level > 1)
            {
                auto choice = party.default_advancement(id);
                if (level == 4)
                {
                    choice.abilities = {};
                    choice.abilities[2] = 2;
                }
                party.advance(id, choice);
            }
            const auto &sheet = party.member(id).character.sheet();
            const int expected = die + 3 + (level - 1) * (die / 2 + 4) + (level == 4 ? 4 : 0);
            check(
                sheet.hit_points == expected,
                "All starting classes and supported advancement have independent Dwarf HP totals");
            check(sheet.racial_messages.front().source == "Dwarven Toughness: +{hp} maximum HP." &&
                      sheet.racial_messages.front().arguments[0].value == std::to_string(level),
                  "All class paths expose the attained Toughness contribution");
            auto bytes = saved(party);
            CampaignParty restored(module());
            restored.restore(decode_campaign(bytes, *srd5::character_rules(), *module(),
                                             "advancement-fixture", nullptr)
                                 .party);
            check(restored.member(id).character.sheet().racial_modifiers ==
                          sheet.racial_modifiers &&
                      saved(restored) == bytes,
                  "Reload preserves source explanation and campaign bytes");
        }
    }
}

void progression()
{
    for (const char *klass : {"fighter", "cleric", "wizard"})
    {
        CampaignParty party(module());
        const auto id = party.add_pc(character(klass));
        const auto level1 = saved(party);
        rejects(
            [&]
            {
                party.advance(id, {});
            });
        check(saved(party) == level1, "Insufficient XP is atomic");
        party.award_experience(2700, "fourth-level");
        check(party.member(id).character.sheet().level == 1, "XP does not auto-advance");
        auto wounded = party.checkpoint();
        wounded.roster[0].vitals.hit_points -= 3;
        wounded.roster[0].vitals.resources =
            std::string_view(klass) == "fighter" ? "SRD1 1 0 0 0 0" : "SRD1 0 1 0 0 0";
        party.restore(std::move(wounded));
        party.begin_combat();
        check(!party.can_advance(id), "Advancement unavailable during combat");
        rejects(
            [&]
            {
                party.advance(id, {});
            });
        party.end_combat();
        for (unsigned level = 2; level <= 4; ++level)
        {
            const auto before = saved(party);
            const auto old = party.member(id);
            auto choice = party.default_advancement(id);
            if (level == 4)
            {
                choice.abilities = {};
                choice.abilities[2] = 2;
            }
            auto bad = choice;
            bad.spells.push_back("unimplemented");
            rejects(
                [&]
                {
                    party.advance(id, bad);
                });
            check(saved(party) == before, "Rejected spell cannot change state or choices");
            if (level == 4)
            {
                bad = choice;
                bad.abilities[2] = 1;
                rejects(
                    [&]
                    {
                        party.advance(id, bad);
                    });
                check(saved(party) == before, "Incomplete ability allocation is atomic");
            }
            const auto preview = party.preview_advancement(id, choice);
            check(saved(party) == before, "Preview is entirely read only");
            party.advance(id, choice);
            const auto &now = party.member(id);
            check(now.character.sheet().level == level && now.vitals == preview.vitals,
                  "Confirmation matches HP and resource preview");
            check(now.character.sheet().hit_points - now.vitals.hit_points == 3,
                  "Level-up preserves pre-existing HP deficit");
            check(now.character.advancements().size() == level - 1,
                  "Each confirmed choice is retained");
            if (level == 4)
            {
                check(now.character.sheet().modifiers[2] == old.character.sheet().modifiers[2] + 1,
                      "Constitution ability points update modifier");
                const int expected =
                    old.character.sheet().hit_die / 2 + 1 + old.character.sheet().modifiers[2] + 4;
                check(now.character.sheet().hit_points - old.character.sheet().hit_points ==
                          expected,
                      "Constitution growth applies retroactively to all four levels");
            }
            const auto bytes = saved(party);
            auto loaded = decode_campaign(bytes, *srd5::character_rules(), *module(),
                                          "advancement-fixture", nullptr);
            CampaignParty restored(module());
            restored.restore(std::move(loaded.party));
            check(saved(restored) == bytes,
                  "Level and choice history survive full save reconstruction");
        }
        check(!party.can_advance(id), "Supported progression stops at level four");
        check(party.member(id).vitals.resources ==
                  (std::string_view(klass) == "fighter" ? "SRD1 2 0 0 0 0" : "SRD2 0 3 3 0 0 0"),
              "Only new resource capacity is added across three advancements");
    }
    CampaignParty capped(module());
    auto high = character("fighter").creation_data();
    for (auto &roll : high.rolls)
        roll = {{6, 6, 6, 1}, 3};
    const auto cap = capped.add_pc(Character(*srd5::character_rules(), high, {}));
    capped.award_experience(2700, "cap");
    for (unsigned level = 2; level <= 3; ++level)
        capped.advance(cap, capped.default_advancement(cap));
    auto invalid = capped.default_advancement(cap);
    invalid.abilities = {};
    invalid.abilities[2] = 2;
    const auto unchanged = saved(capped);
    rejects(
        [&]
        {
            capped.advance(cap, invalid);
        });
    check(saved(capped) == unchanged, "Ability cap rejects without partially applying level or HP");
    CampaignParty unsupported(module());
    const auto id = unsupported.add_pc(character("bard"));
    unsupported.award_experience(2700, "xp");
    check(!unsupported.can_advance(id), "Unsupported classes cannot select partial advancement");
}
void hp_history()
{
    struct Example
    {
        int constitution;
        unsigned increase;
        std::array<int, 4> hp;
    };
    // SRD p. 23: gain HP using the old modifier, then apply the feat's
    // Constitution modifier increase once per attained level.
    for (const auto example : {Example{3, 2, {2, 3, 4, 9}}, Example{3, 1, {2, 3, 4, 9}},
                               Example{4, 1, {3, 4, 5, 6}}, Example{4, 2, {3, 4, 5, 10}},
                               Example{5, 2, {3, 4, 5, 10}}, Example{14, 2, {8, 14, 20, 30}}})
        for (const bool dwarf : {false, true})
            for (const bool unconscious : {false, true})
            {
                auto draft = character("wizard").creation_data();
                draft.race = dwarf ? "dwarf" : "human";
                // Authored legal dice, with the fourth (lowest) die discarded.
                draft.rolls[2] = example.constitution == 14
                                     ? AbilityRoll{{6, 4, 4, 1}, 3}
                                     : AbilityRoll{{example.constitution - 2, 1, 1, 1}, 3};
                const auto adjustments = srd5::character_rules()->adjustments("sage");
                for (unsigned i = 0; i < adjustments.size(); ++i)
                    if (adjustments[i].bonuses[2] == 0)
                    {
                        draft.adjustment = i;
                        break;
                    }
                CampaignParty party(module());
                const auto id = party.add_pc(Character(*srd5::character_rules(), draft, {}));
                party.award_experience(2700, "hp-history");
                for (unsigned level = 1; level <= 4; ++level)
                {
                    if (level > 1)
                    {
                        auto choice = party.default_advancement(id);
                        if (level == 4)
                        {
                            auto state = party.checkpoint();
                            auto &vital = state.roster[0].vitals;
                            vital.hit_points =
                                unconscious ? 0 : party.member(id).character.sheet().hit_points - 2;
                            vital.resources = unconscious ? "SRD2 0 1 1 1 2 0" : "SRD2 0 1 1 0 0 0";
                            party.restore(std::move(state));
                            choice.abilities = {};
                            choice.abilities[2] = example.increase;
                            choice.abilities[3] = 2 - example.increase;
                        }
                        party.advance(id, choice);
                    }
                    const auto expected = example.hp[level - 1] + (dwarf ? int(level) : 0);
                    check(
                        party.member(id).character.sheet().hit_points == expected,
                        "HP history follows the SRD sequence even when earlier gains were clamped");
                    check(party.profile(id).hit_points == expected,
                          "Combat profile independently reconstructs the HP history");
                    const auto correct_source = [&](const Message &m)
                    {
                        return m.source == "Dwarven Toughness: +{hp} maximum HP." &&
                               m.arguments.size() == 1 && m.arguments[0].name == "hp" &&
                               m.arguments[0].value == std::to_string(level);
                    };
                    const auto &sheet = party.member(id).character.sheet();
                    check(std::count_if(sheet.racial_messages.begin(), sheet.racial_messages.end(),
                                        correct_source) == int(dwarf),
                          "Species explanation gives total Toughness at the attained level");
                    if (dwarf)
                        check(sheet.racial_modifiers.starts_with(
                                  "Dwarven Toughness: +" + std::to_string(level) + " maximum HP."),
                              "Plain-text source agrees with localized source");
                    const auto bytes = saved(party);
                    auto loaded = decode_campaign(bytes, *srd5::character_rules(), *module(),
                                                  "advancement-fixture", nullptr);
                    CampaignParty restored(module());
                    restored.restore(std::move(loaded.party));
                    check(saved(restored) == bytes,
                          "Each level reconstructs exactly from saved advancement choices");
                    const auto &restored_sources =
                        restored.member(id).character.sheet().racial_messages;
                    check(
                        std::count_if(restored_sources.begin(), restored_sources.end(),
                                      correct_source) == int(dwarf),
                        "Reload reconstructs current racial contribution without stale level-one text");
                }
                const auto &member = party.member(id);
                const int maximum = example.hp[3] + (dwarf ? 4 : 0);
                check(
                    member.vitals.hit_points == (unconscious ? 0 : maximum - 2),
                    "Constitution advancement preserves wounds and does not wake an unconscious character");
                check(member.vitals.resources ==
                          (unconscious ? "SRD5 0 1 2 1 2 0 4 6000 0 FX1 1 0" : "SRD2 0 1 2 0 0 0"),
                      "HP growth preserves death saves and existing spell expenditure");
                auto malformed = member.character.sheet();
                ++malformed.hit_points;
                rejects(
                    [&]
                    {
                        (void)module()->character_profile(malformed, {});
                    });
                malformed = member.character.sheet();
                malformed.hit_point_modifiers.pop_back();
                rejects(
                    [&]
                    {
                        (void)module()->character_profile(malformed, {});
                    });
                malformed = member.character.sheet();
                malformed.hit_point_modifiers[1] = 6;
                rejects(
                    [&]
                    {
                        (void)module()->character_profile(malformed, {});
                    });
                malformed = member.character.sheet();
                --malformed.hit_point_modifiers.back();
                rejects(
                    [&]
                    {
                        (void)module()->character_profile(malformed, {});
                    });
                if (!unconscious)
                {
                    auto rules = module();
                    auto combat = duel(*rules, party);
                    check(unit(*combat, id).max_hit_points == maximum &&
                              unit(*combat, id).hit_points == maximum - 2,
                          "Combat uses corrected maximum and wounded current HP");
                    check(rules->restore(combat->save())->save() == combat->save(),
                          "HP history survives a combat checkpoint");
                }
            }
}
void ability_sources()
{
    for (const unsigned ability : {0u, 4u})
    {
        auto draft = character("fighter").creation_data();
        draft.background = "soldier";
        CampaignParty party(module());
        const auto id = party.add_pc(Character(*srd5::character_rules(), draft, {}));
        const auto verify = [&](const CharacterSheet &sheet, unsigned count)
        {
            check(sheet.ability_adjustments.size() == count,
                  "Only acquired ability grants appear as sources");
            const auto &background = sheet.ability_adjustments.front();
            check(background.source_id == "background:soldier" && background.level == 1 &&
                      background.label == "Soldier background",
                  "Background source has its own stable identity and acquisition level");
            check(background.bonuses == std::array<int, 6>{2, 1, 0, 0, 0, 0},
                  "Soldier bonuses remain the original +2 Strength and +1 Dexterity");
            if (count == 2)
            {
                const auto &feat = sheet.ability_adjustments.back();
                std::array<int, 6> expected{};
                expected[ability] = 2;
                check(
                    feat.source_id == "feat:ability_score_improvement" && feat.level == 4 &&
                        feat.bonuses == expected,
                    "Level-four feat points retain their source, including abilities outside the background");
                check(feat.label == "Level 4 Ability Score Improvement" &&
                          feat.label_message.source == "Level {level} Ability Score Improvement",
                      "The feat explanation identifies its acquisition level");
            }
            for (unsigned i = 0; i < 6; ++i)
            {
                int total = 0;
                for (const auto &source : sheet.ability_adjustments)
                    total += source.bonuses[i];
                check(total == sheet.bonuses[i] && sheet.base[i] + total == sheet.scores[i],
                      "Separate sources sum exactly to the displayed final scores");
            }
        };
        verify(party.member(id).character.sheet(), 1);
        party.award_experience(2700, "ability-sources");
        for (unsigned level = 2; level <= 3; ++level)
        {
            party.advance(id, party.default_advancement(id));
            verify(party.member(id).character.sheet(), 1);
        }
        auto choice = party.default_advancement(id);
        choice.abilities = {};
        choice.abilities[ability] = 2;
        auto invalid = choice;
        invalid.abilities[ability] = 1;
        rejects(
            [&]
            {
                party.advance(id, invalid);
            });
        verify(party.member(id).character.sheet(), 1);
        const auto before = saved(party);
        const auto preview = party.preview_advancement(id, choice);
        verify(preview.character.sheet(), 2);
        check(saved(party) == before, "Preview leaves saved source choices untouched");
        party.advance(id, choice);
        verify(party.member(id).character.sheet(), 2);
        const auto bytes = saved(party);
        auto loaded = decode_campaign(bytes, *srd5::character_rules(), *module(),
                                      "advancement-fixture", nullptr);
        CampaignParty restored(module());
        restored.restore(std::move(loaded.party));
        verify(restored.member(id).character.sheet(), 2);
        check(saved(restored) == bytes,
              "Source records reconstruct from saved choices without changing campaign bytes");
    }
}
void feats()
{
    for (const char *feat : {"defense", "savage_attacker"})
    {
        CampaignParty party(module());
        const auto id = party.add_pc(character("fighter"));
        party.award_experience(2700, "xp");
        for (unsigned level = 2; level <= 3; ++level)
            party.advance(id, party.default_advancement(id));
        party.set_wealth(id, {0, 0, 0, 100, 0, 0, 0});
        for (unsigned type : {36, 55})
        {
            por::Equipment e;
            e.stored.type = type;
            e.stored.stack_size = 1;
            e.stored.value = 1;
            party.purchase(id, e);
            party.equip(id, party.member(id).character.inventory().items().back().id);
        }
        const auto ac = party.profile(id).armor_class;
        auto choice = party.default_advancement(id);
        choice.feat = feat;
        choice.abilities = {};
        party.advance(id, choice);
        check(party.member(id).character.sheet().ability_adjustments.size() == 1,
              "A feat without ability points does not invent an ability adjustment");
        if (std::string_view(feat) == "defense")
        {
            check(party.profile(id).armor_class == ac + 1, "Defense adds AC in armor");
            party.unequip(id, 2);
            check(party.profile(id).armor_class ==
                      10 + party.member(id).character.sheet().modifiers[1],
                  "Defense does not grant unarmored AC");
        }
        else
        {
            auto rules = module();
            auto participants = party.participants();
            participants[0].cell = {1, 1};
            participants.push_back({99, "vanguard", "Target", 1, {2, 1}});
            Encounter e{{12, 9, std::vector<std::uint8_t>(108)}, participants};
            bool hit = false;
            for (unsigned seed = 0; seed < 100 && !hit; ++seed)
            {
                auto combat = rules->create(e, seed);
                if (combat->snapshot().actor != id)
                    continue;
                combat->submit(command(*combat, "melee"));
                test::choose_savage_damage(*combat);
                for (const auto &log : combat->snapshot().log)
                    hit |= log.find("Savage Attacker") != std::string::npos;
                if (hit)
                    check(rules->restore(combat->save())->save() == combat->save(),
                          "Spent Savage Attacker state survives checkpoint");
            }
            check(hit, "Selected Savage Attacker modifies actual weapon damage");
        }
    }
}
void spells()
{
    for (const char *klass : {"wizard", "cleric"})
    {
        CampaignParty party(module());
        const auto id = party.add_pc(character(klass));
        party.award_experience(2700, "xp");
        for (unsigned level = 2; level <= 4; ++level)
        {
            auto choice = party.default_advancement(id);
            if (std::string_view(klass) == "cleric")
                choice.spells = {"cure_wounds", "healing_word"};
            else if (level >= 3)
                choice.spells = {"magic_missile", "scorching_ray", "blindness"};
            party.advance(id, choice);
        }
        auto state = party.checkpoint();
        state.roster[0].vitals.hit_points -= 10;
        party.restore(std::move(state));
        auto rules = module();
        auto combat = duel(*rules, party);
        if (std::string_view(klass) == "wizard")
        {
            check(offers(*combat, "scorching_ray") && offers(*combat, "magic_missile_2"),
                  "Prepared wizard has second-level actions");
            combat->submit(command(*combat, "scorching_ray"));
            check(unit(*combat, id).persistent.resources.starts_with("SRD2 0 4 2 "),
                  "Scorching Ray spends exactly one level-two slot");
            const auto bytes = combat->save();
            auto restored = rules->restore(bytes);
            check(restored->save() == bytes, "New spell resources persist exactly");
        }
        else
        {
            check(offers(*combat, "healing_word") && offers(*combat, "healing_word_2"),
                  "Cleric can select either healing slot level");
            const auto hp = unit(*combat, id).hit_points;
            combat->submit(command(*combat, "healing_word_2"));
            check(unit(*combat, id).hit_points > hp && unit(*combat, id).action,
                  "Healing Word heals and preserves action");
            check(!offers(*combat, "cure_wounds") && !offers(*combat, "healing_word"),
                  "A second spell slot cannot be spent in the same turn");
            const auto bytes = combat->save();
            auto restored = rules->restore(bytes);
            check(restored->save() == bytes && !offers(*restored, "cure_wounds"),
                  "Slot-per-turn restriction survives checkpoint");
        }
        party.begin_combat();
        party.apply_combat(combat->snapshot());
        party.end_combat();
        const auto bytes = saved(party);
        auto loaded = decode_campaign(bytes, *srd5::character_rules(), *rules,
                                      "advancement-fixture", nullptr);
        CampaignParty restored(module());
        restored.restore(std::move(loaded.party));
        check(saved(restored) == bytes, "Spent second-level slots survive campaign reload");
    }
}
} // namespace
int main()
{
    try
    {
        dwarf_class_sources();
        progression();
        hp_history();
        ability_sources();
        feats();
        spells();
        std::cout << "Manual advancement tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
