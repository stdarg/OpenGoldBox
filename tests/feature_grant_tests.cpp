#include "opengold/campaign_save.h"
#include "opengold/rolf_tour.h"
#include "opengold/srd5.h"
#include "../src/OpenGold.Rules.Srd5/src/feature_grants.h"
#include "../src/OpenGold.Rules.Srd5/src/weapon_mastery.h"
#include <algorithm>
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
                  "PC42 1 2 "),
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
    for (const auto &klass :
            {"fighter", "cleric", "wizard"
            })
        for (const auto &background :
                {"soldier", "sage"
                })
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
            choice.abilities[Ability::constitution] = 1;
            choice.abilities[Ability::wisdom] = 1;
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
            for (const auto &bad :
                    {"0", "3", "-1", "01", "two"
                    })
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

// A sheet stores race and background labels. Their grant sources are the
// creation tables' stable IDs, looked up by label rather than spelt from it,
// so a relabelled or translated choice keeps its grants and an unknown label
// is rejected.
void origin_ids()
{
    namespace detail = srd5::detail;
    const auto rules = srd5::character_rules();
    for (const auto &race : rules->choices(CreationField::race))
        check(detail::race_id(race.label) == race.id, "Every race label names its ID");
    for (const auto &background : rules->choices(CreationField::background))
        check(detail::background_id(background.label) == background.id,
              "Every background label names its ID");
    rejects([]
    {
        (void)detail::race_id("Humann");
    });
    rejects([]
    {
        (void)detail::background_id("sage");
    });
}

void profiles()
{
    auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(hero());
    party.award_experience(2700, "grant-xp");
    for (unsigned level = 2; level <= 3; ++level)
        party.advance(id, party.default_advancement(id));
    auto choice = party.default_advancement(id);
    choice.abilities = {};
    choice.abilities[Ability::constitution] = 2;
    party.advance(id, choice);
    const auto bytes = saved(party);
    for (const auto &bad :
            {
                mutate_save(bytes, "\"background:soldier\" 1", "\"background:soldier\" 4"),
                mutate_save(bytes, "\"feat:savage_attacker\"", "\"feat:unknown\""),
                mutate_save(bytes, "\"constitution\" \"2\"", "\"strength\" \"2\"")
            })
        rejects(
            [&]
    {
        (void)decode_campaign(bad, *srd5::character_rules(), *rules, "grant-fixture",
        nullptr);
    });
    const auto encounter = [&](std::string profile)
    {
        return Encounter{{8, 8, std::vector<Terrain>(64)},
            {   {1, "campaign-character", "Hero", Side::party, {1, 1}, profile},
                {2, "vanguard", "Target", Side::opposition, {2, 1}}
            }};
    };
    auto profile = party.profile(id).data;
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
    replace(wrong, "PC42 4 2 ", "PC42 4 0 ");
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
}

#include "mastery_grant_checks.h"
#include "mastery_rest_checks.h"
} // namespace

int main()
{
    try
    {
        mastery_grant_checks::run();
        mastery_rest_checks::run();
        creation();
        advancement();
        profiles();
        origin_ids();
        std::cout << "Feature grant tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
