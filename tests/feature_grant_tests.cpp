#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "combat_fixture.h"
#include <algorithm>
#include <fstream>
#include <iostream>
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
    check(caught, "Invalid grant must reject");
}

auto module()
{
    return srd5::load(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "data/rules/srd-5.2.1/combat.rules");
}

Character hero(std::string klass = "fighter", std::string background = "soldier",
               std::string race = "human")
{
    CharacterDraft draft;
    draft.race = race;
    draft.gender = "female";
    draft.character_class = klass;
    draft.background = background;
    if (klass == "wizard")
        draft.training["class:wizard"] = {"medicine", "nature"};
    draft.alignment = "neutral_good";
    draft.name = "Grant tester";
    draft.rolled = true;
    for (auto &roll : draft.rolls)
        roll = {{6, 5, 4, 1}, 3};
    return Character(*srd5::character_rules(), draft, {});
}

std::string saved(const CampaignParty &party)
{
    return encode_campaign(party, nullptr, "grant-fixture");
}

std::string fixture(const char *name)
{
    std::ifstream in(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures" / name);
    check(bool(in), "Frozen fixture exists");
    return {std::istreambuf_iterator<char>(in), {}};
}

bool has(const CharacterSheet &sheet, const FeatureGrant &grant)
{
    return std::find(sheet.grants.begin(), sheet.grants.end(), grant) != sheet.grants.end();
}

void replace(std::string &text, std::string_view from, std::string_view to)
{
    const auto at = text.find(from);
    check(at != text.npos, "Fixture field exists");
    text.replace(at, from.size(), to);
}

std::string mutate_save(std::string bytes, std::string_view from, std::string_view to)
{
    const auto begin = bytes.find('\n', bytes.find('\n') + 1) + 1;
    auto body = bytes.substr(begin);
    replace(body, from, to);
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : body)
    {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    return bytes.substr(0, bytes.find('\n') + 1) + std::to_string(hash) + '\n' + body;
}

void creation()
{
    auto rules = module();
    auto creation = srd5::character_rules();
    for (const auto &klass : creation->choices(CreationField::character_class))
    {
        const auto sheet = hero(klass.id).sheet();
        check(has(sheet, {"feat:savage_attacker", "background:soldier", 1, {}}),
              "Every Soldier acquires the feat at creation, independent of class and advancement");
        check(rules->character_profile(sheet, {}).data.starts_with(
                  klass.id == "wizard"  ? "PC32 1 2 "
                  : klass.id == "rogue" ? "PC35 1 2 "
                                        : "PC28 1 2 "),
              "Creation grant supplies Savage Attacker to combat without a level-four feat");
        auto invalid = sheet;
        invalid.grants.push_back(sheet.grants.front());
        rejects(
            [&]
            {
                (void)rules->character_profile(invalid, {});
            });
        invalid = sheet;
        invalid.grants.erase(invalid.grants.begin());
        rejects(
            [&]
            {
                (void)rules->character_profile(invalid, {});
            });
        invalid = sheet;
        invalid.grants.front().source_id = "background:sage";
        rejects(
            [&]
            {
                (void)rules->character_profile(invalid, {});
            });
        invalid = sheet;
        invalid.grants.front().level = 4;
        rejects(
            [&]
            {
                (void)rules->character_profile(invalid, {});
            });
        invalid = sheet;
        invalid.grants.front().choices = {{"free", "yes"}};
        rejects(
            [&]
            {
                (void)rules->character_profile(invalid, {});
            });
    }
    check(has(hero().sheet(), {"feature:fighting_style", "class:fighter", 1, {}}) &&
              has(hero().sheet(), {"feature:second_wind", "class:fighter", 1, {}}),
          "Existing Fighter entitlement and recovery have class provenance");
    check(has(hero("wizard", "sage", "dwarf").sheet(),
              {"trait:dwarven_toughness", "species:dwarf", 1, {}}),
          "Existing HP trait records its species source");
    check(has(hero("cleric", "sage").sheet(), {"feature:spellcasting", "class:cleric", 1, {}}),
          "Existing Spellcasting records its class source");
    auto invalid = hero("wizard", "sage").sheet();
    invalid.grants.push_back({"feat:defense", "class:wizard:ability_score_improvement", 1, {}});
    rejects(
        [&]
        {
            (void)rules->character_profile(invalid, {});
        });
    invalid = hero("fighter", "sage").sheet();
    invalid.grants.push_back({"feat:ability_score_improvement",
                              "class:fighter:ability_score_improvement",
                              1,
                              {{"strength", "2"}}});
    rejects(
        [&]
        {
            (void)rules->character_profile(invalid, {});
        });
}

void advancement()
{
    for (const auto &klass : {"fighter", "cleric", "wizard"})
        for (const auto &background : {"soldier", "sage"})
        {
            CampaignParty party(module());
            const auto id = party.add_pc(hero(klass, background));
            party.award_experience(2700, "grant-xp");
            for (unsigned level = 2; level <= 3; ++level)
                party.advance(id, party.default_advancement(id));
            const auto before = saved(party);
            const auto sheet = party.member(id).character.sheet();
            const auto options = module()->advancement_options(sheet);
            const auto available = [&](const char *feat)
            {
                for (const auto &option : options.feats)
                    if (option.id == feat)
                        return option.available;
                return false;
            };
            check(available("savage_attacker") == (std::string_view(background) != "soldier"),
                  "Origin grants control duplicate feat availability");
            check(available("defense") == (std::string_view(klass) == "fighter"),
                  "Defense requires the acquired Fighting Style feature");
            auto choice = party.default_advancement(id);
            choice.feat = "savage_attacker";
            choice.abilities = {};
            if (std::string_view(background) == "soldier")
            {
                rejects(
                    [&]
                    {
                        party.advance(id, choice);
                    });
                check(saved(party) == before,
                      "Duplicate feat rejection preserves history and all party state");
            }
            choice.feat = "defense";
            if (std::string_view(klass) != "fighter")
            {
                rejects(
                    [&]
                    {
                        party.advance(id, choice);
                    });
                check(saved(party) == before, "Missing prerequisite rejection is atomic");
            }
            choice = party.default_advancement(id);
            choice.abilities = {};
            choice.abilities[2] = 1;
            choice.abilities[4] = 1;
            const auto preview = party.preview_advancement(id, choice);
            const FeatureGrant expected{"feat:ability_score_improvement",
                                        std::string("class:") + klass +
                                            ":ability_score_improvement",
                                        4,
                                        {{"constitution", "1"}, {"wisdom", "1"}}};
            check(has(preview.character.sheet(), expected) && saved(party) == before,
                  "Preview records both choices without mutating the campaign");
            party.advance(id, choice);
            check(has(party.member(id).character.sheet(), expected),
                  "Confirmed feat retains its entitlement, level and individual choices");
            const auto bytes = saved(party);
            auto loaded = decode_campaign(bytes, *srd5::character_rules(), *module(),
                                          "grant-fixture", nullptr);
            CampaignParty restored(module());
            restored.restore(std::move(loaded.party));
            check(saved(restored) == bytes,
                  "Explicit grants and choices survive campaign reconstruction exactly");
            check(restored.member(id).character.sheet().grants ==
                      party.member(id).character.sheet().grants,
                  "Reconstructed provenance matches acquired grants");
            for (const auto &bad : {"0", "3", "-1", "01", "two"})
            {
                auto invalid = party.member(id).character.sheet();
                invalid.grants.back().choices["constitution"] = bad;
                rejects(
                    [&]
                    {
                        (void)module()->character_profile(invalid, {});
                    });
            }
            auto invalid = party.member(id).character.sheet();
            invalid.grants.push_back(invalid.grants.back());
            rejects(
                [&]
                {
                    (void)module()->character_profile(invalid, {});
                });
            invalid = party.member(id).character.sheet();
            invalid.grants.back().choices = {{"strength", "2"}};
            rejects(
                [&]
                {
                    (void)module()->character_profile(invalid, {});
                });
            const auto tampered =
                mutate_save(bytes, "\"constitution\" \"1\"", "\"constitution\" \"2\"");
            rejects(
                [&]
                {
                    (void)decode_campaign(tampered, *srd5::character_rules(), *module(),
                                          "grant-fixture", nullptr);
                });
            check(saved(party) == bytes, "Malformed saved choices do not affect the live campaign");
        }
}

void profiles_and_migration()
{
    auto rules = module();
    const auto old = fixture("campaign-v7-grants.ogs");
    auto loaded = decode_campaign(old, *srd5::character_rules(), *rules, "grant-fixture", nullptr);
    CampaignParty party(module());
    party.restore(std::move(loaded.party));
    const std::array<int, 6> maxima{12, 40, 40, 40, 38, 24};
    const std::array<int, 6> armor{16, 16, 17, 16, 12, 16};
    for (unsigned n = 0; n < 6; ++n)
    {
        const auto &member = party.member(n + 1);
        const auto profile = party.profile(n + 1);
        check(profile.hit_points == maxima[n] && member.vitals.hit_points == maxima[n] - 3 &&
                  profile.armor_class == armor[n],
              "Legacy feat/HP/armor totals survive source migration");
        check(member.vitals.resources == (n < 4 ? "SRD1 1 0 0 0 0" : "SRD2 0 1 1 0 0 0"),
              "Legacy spent resources are preserved");
    }
    check(has(party.member(1).character.sheet(),
              {"feat:savage_attacker", "background:soldier", 1, {}}),
          "Old implicit Soldier feat gains explicit background provenance");
    check(has(party.member(2).character.sheet(), {"feat:ability_score_improvement",
                                                  "class:fighter:ability_score_improvement",
                                                  4,
                                                  {{"constitution", "2"}}}),
          "Old ability choice is migrated without collapsing the Soldier grant");
    check(has(party.member(3).character.sheet(),
              {"feat:defense", "class:fighter:ability_score_improvement", 4, {}}),
          "Old Defense retains the actual level-four acquisition source");
    check(has(party.member(4).character.sheet(),
              {"feat:savage_attacker", "class:fighter:ability_score_improvement", 4, {}}),
          "Chosen Savage Attacker remains distinct from a background grant");
    const auto bytes = saved(party);
    auto again = decode_campaign(bytes, *srd5::character_rules(), *rules, "grant-fixture", nullptr);
    CampaignParty twice(module());
    twice.restore(std::move(again.party));
    check(saved(twice) == bytes, "Grant migration happens once and saves canonically");
    for (const auto &bad :
         {mutate_save(bytes, "\"background:soldier\" 1", "\"background:soldier\" 4"),
          mutate_save(bytes, "\"feat:defense\"", "\"feat:unknown\""),
          mutate_save(bytes, "\"constitution\" \"2\"", "\"strength\" \"2\"")})
        rejects(
            [&]
            {
                (void)decode_campaign(bad, *srd5::character_rules(), *rules, "grant-fixture",
                                      nullptr);
            });
    const auto encounter = [&](std::string profile)
    {
        return Encounter{{8, 8, std::vector<std::uint8_t>(64)},
                         {{1, "campaign-character", "Hero", 0, {1, 1}, profile},
                          {2, "vanguard", "Target", 1, {2, 1}}}};
    };
    auto profile = party.profile(2).data;
    check(profile.find("background:soldier") != profile.npos &&
              profile.find("constitution") != profile.npos,
          "Combat recipe persists full provenance and selected abilities");
    auto combat = rules->create(encounter(profile), 13);
    check(rules->restore(combat->save())->save() == combat->save(),
          "Combat checkpoint retains all grant records");
    auto wrong = profile;
    replace(wrong, "background:soldier", "background:sage");
    rejects(
        [&]
        {
            (void)rules->create(encounter(wrong), 13);
        });
    wrong = profile;
    replace(wrong, "PC31 4 2 ", "PC31 4 0 ");
    rejects(
        [&]
        {
            (void)rules->create(encounter(wrong), 13);
        });
    wrong = profile;
    replace(wrong, "\"constitution\" \"2\"", "\"strength\" \"2\"");
    rejects(
        [&]
        {
            (void)rules->create(encounter(wrong), 13);
        });
    auto legacy = rules->restore(fixture("combat-v8-grants.save"));
    auto continued = rules->restore(legacy->save());
    // Older combat recipes lack a background/history; preserve their effects
    // without fabricating whether Savage Attacker came from origin or leveling.
    unsigned decision_commands{};
    for (unsigned n = 0; n < 12; ++n)
    {
        check(legacy->save() == continued->save(),
              "Legacy combat effects, resources and RNG continue deterministically");
        const auto commands = legacy->legal_commands();
        if (commands.empty())
            break;
        auto command = std::find_if(commands.begin(), commands.end(),
                                    [](const auto &c)
                                    {
                                        return c.verb == "melee";
                                    });
        if (command == commands.end())
            command = std::find_if(commands.begin(), commands.end(),
                                   [](const auto &c)
                                   {
                                       return c.verb == "end";
                                   });
        check(command != commands.end(), "Legacy encounter can continue");
        check(legacy->submit(*command) && continued->submit(*command),
              "Both continuations accept identical commands");
        const auto choices = test::choose_savage_damage(*legacy);
        decision_commands += choices;
        check(test::choose_savage_damage(*continued) == choices, "Restored decisions match");
    }
    auto reference =
        test::with_hit_dice(fixture("combat-v8-grants-continued.save"), rules->identity(),
                            {{1, 1}, {2, 4}, {3, 4}, {4, 4}, {5, 4}, {6, 3}, {99, 0}}, {{1, 5143}});
    // The old writer resolved Savage Attacker inside the attack command. Only
    // revision gains the new decision tickets; the remaining oracle is frozen.
    std::size_t state = 0;
    for (unsigned row = 0; row < 3; ++row)
        state = reference.find('\n', state) + 1;
    const auto revision = reference.find(' ', state) + 1, end = reference.find(' ', revision);
    reference.replace(revision, end - revision,
                      std::to_string(std::stoull(reference.substr(revision, end - revision)) +
                                     decision_commands));
    // Actor 1 falls during this continuation. Preserve the frozen oracle except
    // for its newly explicit Prone condition; all damage, budgets and RNG stay exact.
    const auto first_effect = reference.find("\nFX1 1 0\n");
    check(first_effect != reference.npos, "Frozen effect block exists");
    reference.replace(first_effect + 9, 7, "FX4 1 0 0 1");
    // This continuation now drops actor 1's longsword. Extend the old oracle
    // explicitly with the new ledger/budgets; never regenerate its damage/RNG.
    std::istringstream frozen_rows(reference);
    std::vector<std::string> rows;
    for (std::string row; std::getline(frozen_rows, row);)
        rows.push_back(row);
    rows[0].replace(0, 11, "OGCOMBAT 16");
    for (unsigned i = 4; i < 11; ++i)
    {
        if (rows[i].starts_with("1 "))
        {
            auto pos = rows[i].size();
            for (unsigned n = 0; n < 8; ++n)
                pos = rows[i].rfind(' ', pos - 1);
            check(rows[i].substr(pos + 1, 1) == "1", "Old writer recorded the held longsword");
            rows[i].replace(pos + 1, 1, "0");
        }
        rows[i] += " 0 0 0 0"; // Surge pools/allowance and Dash count, absent in format 13.
    }
    reference.clear();
    for (const auto &row : rows)
        reference += row + '\n';
    reference += "6\n1 0 1 1\n2 2 0 0\n3 3 0 0\n4 4 0 0\n5 5 0 0\n6 6 0 0\n1 1 1 1 1 1 1 \n";
    check(
        legacy->save() == reference,
        "Continuation matches the previous writer exactly, including damage, spent feats, turn budgets and RNG");
}
} // namespace

int main()
{
    try
    {
        creation();
        advancement();
        profiles_and_migration();
        std::cout << "Feature grant tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
