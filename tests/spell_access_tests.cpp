#include "combat_fixture.h"
#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
using namespace opengold;
using namespace opengold::rules;

namespace
{
void check(bool ok, const char *why)
{
    if (!ok)
        throw std::runtime_error(why);
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
    check(caught, "Invalid spell grant/preparation must reject");
}

std::string read(const std::filesystem::path &path)
{
    std::ifstream in(path);
    check(bool(in), "Fixture exists");
    return {std::istreambuf_iterator<char>(in), {}};
}

const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

auto module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

Character hero(std::string klass = "wizard")
{
    CharacterDraft d;
    d.race = "orc";
    d.gender = "female";
    d.character_class = klass;
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Spellbook tester";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    return Character(*srd5::character_rules(), d, {});
}

std::vector<std::string> ids(const std::vector<LearnedSpell> &spells)
{
    std::vector<std::string> result;
    for (const auto &s : spells)
        result.push_back(s.id);
    return result;
}

bool has(const CombatSession &c, std::string_view verb)
{
    for (const auto &a : c.legal_commands())
        if (a.verb == verb)
            return true;
    return false;
}

Command command(const CombatSession &c, std::string_view verb)
{
    for (const auto &a : c.legal_commands())
        if (a.verb == verb)
            return a;
    throw std::runtime_error("Missing command");
}

CombatantView unit(const CombatSession &c)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == 1)
            return a;
    throw std::runtime_error("Missing actor");
}

auto battle(const RulesModule &rules, const CharacterSheet &sheet, VitalState state)
{
    const auto profile = rules.character_profile(sheet, {});
    return rules.create({{8, 8, std::vector<std::uint8_t>(64)},
        {   {1, "campaign-character", "Wizard", 0, {1, 1}, profile.data, state},
            {99, "vanguard", "Target", 1, {5, 1}}
        }},
    13);
}

std::string saved(const CampaignParty &p)
{
    return encode_campaign(p, nullptr, "spell-access");
}

void creation()
{
    auto rules = module();
    auto creation = srd5::character_rules();
    for (const auto &klass : creation->choices(CreationField::character_class))
    {
        const auto h = hero(klass.id);
        const auto access = rules->spell_access(h.sheet());
        if (klass.id != "wizard")
        {
            check(access.cantrips.empty() && access.spellbook.empty() && access.prepared.empty(),
                  "Other preparation policies are not invented");
            continue;
        }
        check(ids(access.cantrips) == std::vector<std::string> {"fire_bolt"} &&
              ids(access.spellbook) == std::vector<std::string> {"magic_missile"} &&
              access.prepared == std::vector<std::string> {"magic_missile"},
              "Existing playable Wizard preset has separate cantrip, book and preparation");
        check(
            access.cantrip_choices == 3 && access.spellbook_choices == 6 &&
            access.prepared_choices == 4,
            "Independent level-one SRD entitlements remain explicit beyond the implemented preset");
        for (const auto &s : access.cantrips)
            check(s.source_id == "class:wizard:spellcasting" && s.acquired_level == 1,
                  "Cantrip provenance is explicit");
        for (const auto &s : access.spellbook)
            check(s.source_id == "class:wizard:spellcasting" && s.acquired_level == 1,
                  "Book provenance is explicit");
    }
    const auto h = hero();
    auto s = h.sheet();
    s.prepared_spells.clear();
    auto c = battle(*rules, s, {s.hit_points, false, {}});
    check(
        has(*c, "fire_bolt") && !has(*c, "magic_missile"),
        "An empty preparation no longer silently re-prepares Magic Missile; known cantrip stays available");
    c = battle(*rules, h.sheet(), {s.hit_points, false, "SRD7 0 0 0 0 0 0 1 0 0 0 \"\" 2 FX1 1 0"});
    check(has(*c, "fire_bolt") && !has(*c, "magic_missile"),
          "Book/prepared access never grants a free leveled cast when slots are empty");
    const auto before = unit(*c).persistent;
    check(c->submit(command(*c, "fire_bolt")) && unit(*c).persistent == before,
          "Cantrip casting spends no spell slot or source use");
}

void progression()
{
    auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(hero());
    party.award_experience(2700, "spells-xp");
    const int deficit = party.member(id).character.sheet().hit_points - 5;
    auto state = party.checkpoint();
    state.roster[0].vitals = {5, false, "SRD7 0 1 0 0 0 0 1 0 0 7 \"spell:fixture\" 1 FX1 1 0"};
    party.restore(state);
    for (unsigned level = 2; level <= 4; ++level)
    {
        auto choice = party.default_advancement(id);
        const auto before = saved(party);
        auto bad = choice;
        bad.spells = {"shield"};
        rejects(
            [&]
        {
            party.advance(id, bad);
        });
        check(saved(party) == before, "Rejected learning changes no state, RNG, XP or history");
        const auto preview = party.preview_advancement(id, choice);
        check(saved(party) == before, "Preview is isolated");
        party.advance(id, choice);
        const auto &member = party.member(id);
        const auto access = rules->spell_access(member.character.sheet());
        check(access.cantrip_choices == (level == 4 ? 4u : 3u) &&
              access.spellbook_choices == 6 + 2 * (level - 1) &&
              access.prepared_choices == level + 3,
              "Independent SRD level-two/three/four entitlement table");
        check(access.prepared == choice.spells &&
              access.spellbook == rules->spell_access(preview.character.sheet()).spellbook,
              "Confirmed learning/preparation matches preview");
        check(ids(access.spellbook) ==
              (level == 2
               ? std::vector<std::string> {"magic_missile"}
               : std::vector<std::string> {"magic_missile", "scorching_ray", "blindness"}),
              "Previously learned spells remain even when all are unprepared at different levels");
        if (level >= 3)
        {
            check(access.spellbook[1].acquired_level == 3 &&
                  access.spellbook[2].acquired_level == 3,
                  "New entries retain first learning level rather than latest preparation level");
        }
        check(member.vitals.hit_points == member.character.sheet().hit_points - deficit,
              "Advancement preserves wounds");
        const auto info = rules->recovery_info(member.character.sheet(), member.vitals);
        check(info.temporary_hp.amount == 7 &&
              std::any_of(info.resources.begin(), info.resources.end(),
                          [](const auto & r)
        {
            return r.id == "adrenaline_rush" && r.remaining == 1;
        }),
        "Spellbook changes preserve sourced Temporary HP and spent Adrenaline Rush");
        const auto bytes = saved(party);
        CampaignParty restored(module());
        restored.restore(
            decode_campaign(bytes, *srd5::character_rules(), *rules, "spell-access", nullptr)
            .party);
        check(
            saved(restored) == bytes,
            "Canonical grant/history reconstruction retains unprepared book entries and resource state");
        auto c = battle(*rules, restored.member(id).character.sheet(), restored.member(id).vitals);
        check(
            has(*c, "fire_bolt") && has(*c, "magic_missile") &&
            has(*c, "scorching_ray") == (level >= 3) && has(*c, "blindness") == (level >= 3),
            "Actual casting availability comes from known cantrips and current preparation, not all book entries");
        auto copy = rules->restore(c->save());
        const auto ticket = command(*c, level == 3 ? "scorching_ray" : "magic_missile");
        check(c->submit(ticket) && copy->submit(ticket) && c->save() == copy->save(),
              "Chosen spell continues exactly after checkpoint restore");
        const auto after = c->save();
        check(!c->submit(ticket) && c->save() == after,
              "Stale cast preserves slots, actions and RNG");
        check(!has(*c, "magic_missile") && !has(*c, "scorching_ray"),
              "One spell slot per turn remains enforced");
    }
}

void invalid()
{
    auto rules = module();
    const auto base = hero().sheet();
    const auto test = [&](CharacterSheet s)
    {
        rejects(
            [&]
        {
            (void)rules->character_profile(s, {});
        });
    };
    auto s = base;
    s.prepared_spells = {"fire_bolt"};
    test(s);
    s = base;
    s.prepared_spells = {"scorching_ray"};
    test(s);
    s = base;
    s.prepared_spells = {"magic_missile", "magic_missile"};
    test(s);
    const auto index = std::find_if(base.grants.begin(), base.grants.end(),
                                    [](const auto & g)
    {
        return g.id == "spell:magic_missile";
    }) -
    base.grants.begin();
    for (const auto source :
            {"class:cleric:spellcasting", "feat:magic_initiate", "class:wizard"
            })
    {
        s = base;
        s.grants[index].source_id = source;
        test(s);
    }
    for (unsigned level :
            {
                0u, 2u, 4u
            })
    {
        s = base;
        s.grants[index].level = level;
        test(s);
    }
    s = base;
    s.grants[index].choices = {{"access", "cantrip"}};
    test(s);
    s = base;
    s.grants[index].choices.emplace("free_cast", "1");
    test(s);
    s = base;
    s.grants.push_back(s.grants[index]);
    test(s);
    s = base;
    s.grants.erase(s.grants.begin() + index);
    test(s);
    s = base;
    s.grants.push_back(
    {"spell:scorching_ray", "class:wizard:spellcasting", 1, {{"access", "spellbook"}}});
    test(s);
    s = hero("fighter").sheet();
    s.grants.push_back(base.grants[index]);
    test(s);
    auto profile = rules->character_profile(base, {}).data;
    check(profile.starts_with("PC32 1 0 5 "), "New combat recipe carries sourced access");
    auto bad = profile;
    bad.replace(0, 4, "PC9");
    rejects(
        [&]
    {
        (void)rules->create({{8, 8, std::vector<std::uint8_t>(64)},
            {   {1, "campaign-character", "Wizard", 0, {1, 1}, bad},
                {99, "vanguard", "Target", 1, {5, 1}}
            }},
        13);
    });
    bad = profile;
    bad.replace(bad.find(" 0 5 ") + 3, 1, "4");
    rejects(
        [&]
    {
        (void)rules->create({{8, 8, std::vector<std::uint8_t>(64)},
            {   {1, "campaign-character", "Wizard", 0, {1, 1}, bad},
                {99, "vanguard", "Target", 1, {5, 1}}
            }},
        13);
    });
}

void legacy()
{
    auto rules = module();
    CampaignParty party(module());
    party.restore(decode_campaign(read(root / "tests/fixtures/campaign-v10-spells.ogs"),
                                  *srd5::character_rules(), *rules, "spells-fixture", nullptr)
                  .party);
    for (unsigned id = 1; id <= 3; ++id)
    {
        const auto &m = party.member(id);
        const auto access = rules->spell_access(m.character.sheet());
        check(m.character.sheet().level == (id == 1   ? 1
                                            : id == 2 ? 3
                                            : 4) &&
              m.vitals.hit_points == m.character.sheet().hit_points - 2,
              "Frozen legacy levels and wounds retained");
        check(
            ids(access.spellbook) ==
            (id == 1 ? std::vector<std::string> {"magic_missile"}
             : std::vector<std::string> {"magic_missile", "scorching_ray", "blindness"}),
            "Only history-backed book entries are recovered, including no-longer-prepared spells");
        check(access.prepared == (id == 1   ? std::vector<std::string> {"magic_missile"}
                                  : id == 2 ? std::vector<std::string> {"scorching_ray", "blindness"}
                                  : std::vector<std::string> {"blindness"}),
              "Current prepared list is preserved independently");
        if (id > 1)
            check(access.spellbook[1].acquired_level == 3 &&
                  access.spellbook[2].acquired_level == (id == 2 ? 3u : 4u),
                  "Legacy learning levels follow saved history rather than the current level");
        const std::string expected = "SRD7 0 1 " + std::to_string(id == 1 ? 0 : 1) + " 0 0 0 " +
                                     std::to_string(m.character.sheet().level) +
                                     " 0 0 7 \"spell:fixture\" " + std::to_string(id == 2 ? 1 : 0) +
                                     " FX1 1 0";
        check(m.vitals.resources == expected,
              "Prior-writer slots, Hit Dice, Temporary HP and source uses are never reset");
        check(m.character.inventory().find(1)->get().definition_id == "wand" &&
              m.equipped == std::vector<std::uint64_t> {1},
              "Equipment is retained");
    }
    check(party.state().time_minutes == 123 && party.state().subminute_milliseconds == 456,
          "Legacy campaign time retained");
    const auto bytes = encode_campaign(party, nullptr, "spells-fixture");
    CampaignParty again(module());
    again.restore(
        decode_campaign(bytes, *srd5::character_rules(), *rules, "spells-fixture", nullptr).party);
    check(encode_campaign(again, nullptr, "spells-fixture") == bytes,
          "Migration is canonical and does not re-learn spells on reload");
    const auto old = read(root / "tests/fixtures/combat-v12-spells.save");
    auto c = rules->restore(old);
    auto expected = old;
    expected.replace(expected.find("0.6.18"), 6, rules->identity().version);
    check(c->save() == test::with_savage_choice(expected),
          "Old combat preserves its recipe without inventing absent book history");
    check(c->submit(command(*c, "scorching_ray")), "Legacy prepared spell still casts");
    check(
        c->save() ==
        rules->restore(read(root / "tests/fixtures/combat-v12-spells-continued.save"))->save(),
        "Prior-writer spell damage, slots, action state, RNG and clock continue exactly");
}

void capture_wizard_choices()
{
    auto rules = module();
    check(rules->identity().version == "0.6.50", "Capture requires actual 0.6.50 writer");
    const auto write = [](const std::string & name, const std::string & bytes)
    {
        std::ofstream out(root / "tests/fixtures" / name, std::ios::binary);
        out << bytes;
        check(bool(out), "Write previous-writer spell-choice fixture");
    };
    for (unsigned level = 1; level <= 4; ++level)
    {
        CampaignParty party(module());
        auto draft = hero().creation_data();
        draft.cantrips = std::vector<std::string> {"fire_bolt", "ray_of_frost", "chill_touch"};
        draft.training = {{"origin:languages", {"elvish", "dwarvish"}},
            {"class:wizard", {"medicine", "nature"}}
        };
        const auto id = party.add_pc(Character(*srd5::character_rules(), draft, {}));
        party.award_experience(2700, "choice-baseline");
        for (unsigned n = 2; n <= level; ++n)
        {
            auto choice = party.default_advancement(id);
            if (n == 3)
                choice.spells = {"scorching_ray", "blindness"};
            if (n == 4)
                choice.spells = {"magic_missile"};
            party.advance(id, choice);
        }
        auto state = party.checkpoint();
        state.roster[0].vitals.hit_points -= 2;
        party.restore(std::move(state));
        auto combat = battle(*rules, party.member(id).character.sheet(), party.member(id).vitals);
        const auto spell = level == 3 ? "scorching_ray" : "magic_missile";
        check(combat->submit(command(*combat, spell)), "Baseline casts a prepared leveled spell");
        party.begin_combat();
        party.apply_combat(combat->snapshot());
        party.end_combat();
        (void)party.rest(RestKind::short_rest);
        (void)party.recover_rest_choice(party.state().short_rest->ticket, id,
                                        level == 3 ? "arcane_recovery:0:1" : "arcane_recovery:1:0");
        party.finish_short_rest(party.state().short_rest->ticket);
        combat = battle(*rules, party.member(id).character.sheet(), party.member(id).vitals);
        check(combat->submit(command(*combat, spell)),
              "Baseline spends a slot after Arcane Recovery");
        party.begin_combat();
        party.apply_combat(combat->snapshot());
        party.end_combat();
        write("campaign-wizard-choices-level" + std::to_string(level) + ".ogs",
              encode_campaign(party, nullptr, "wizard-choices-baseline"));
        if (level == 4)
        {
            write("combat-wizard-choices-before.save", combat->save());
            check(combat->submit(command(*combat, "end")), "Baseline ends turn");
            write("combat-wizard-choices-continued.save", combat->save());
        }
    }
}

#include "wizard_choices_checks.h"
} // namespace

int main(int argc, char **argv)
{
    try
    {
        if (argc == 3 && std::string_view(argv[1]) == "--verify-wizard-ui")
        {
            verify_wizard_ui(argv[2]);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--capture-wizard-choices")
        {
            capture_wizard_choices();
            return 0;
        }
        check(argc == 1, "Unexpected argument");
        creation();
        progression();
        invalid();
        legacy();
        wizard_choices_checks();
        write_wizard_ui_fixture();
        std::cout << "Spell access tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
