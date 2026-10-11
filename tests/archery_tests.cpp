#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "combat_fixture.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>
using namespace opengold;
using namespace opengold::rules;

namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

template <class F> void rejects(F f)
{
    bool failed = false;
    try
    {
        f();
    }
    catch (const std::exception &)
    {
        failed = true;
    }
    check(failed, "Invalid feat operation must reject");
}

std::string read(const std::filesystem::path &p)
{
    std::ifstream in(p);
    check(bool(in), "Missing fixture");
    return {std::istreambuf_iterator<char>(in), {}};
}

auto module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

Character hero(std::string klass = "fighter", std::string background = "sage")
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = klass;
    d.background = background;
    d.alignment = "neutral_good";
    d.name = "Archer";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    return Character(*srd5::character_rules(), d, {});
}

void grow(CampaignParty &p, MemberId id, unsigned level)
{
    while (std::cmp_less(p.member(id).character.sheet().level, level))
        p.advance(id, p.default_advancement(id));
}

void replace(std::string &s, std::string_view a, std::string_view b)
{
    const auto at = s.find(a);
    check(at != s.npos, "Expected fixture field");
    s.replace(at, a.size(), b);
}

Command command(const CombatSession &c, std::string_view verb)
{
    for (const auto &a : c.legal_commands())
        if (a.verb == verb)
            return a;
    throw std::runtime_error("Missing command: " + std::string(verb));
}

Message result(const CombatSession &c)
{
    for (const auto &m : c.snapshot().log_messages())
        if (m.source.starts_with("{actor} -> {target}: d20"))
            return m;
    throw std::runtime_error("Missing attack log");
}

std::string arg(const Message &m, std::string_view key)
{
    for (const auto &a : m.arguments)
        if (a.name == key)
            return a.value;
    throw std::runtime_error("Missing attack argument");
}

Character leveled(std::string feat = "archery", std::string background = "sage")
{
    CampaignParty p(module());
    auto id = p.add_pc(hero("fighter", background));
    p.award_experience(2700, "archery");
    grow(p, id, 3);
    auto choice = p.default_advancement(id);
    choice.feat = feat;
    choice.abilities = {};
    p.advance(id, choice);
    return p.member(id).character;
}

auto battle(const RulesModule &rules, const Character &h, std::string weapon, Cell target = {3, 1},
unsigned seed = 89)
{
    const auto profile = rules.character_profile(h.sheet(), std::array<std::string, 1> {weapon});
    auto c = rules.create({{40, 8, std::vector<Terrain>(320)},
        {   {1, "campaign-character", "Archer", Side::party, {1, 1}, profile.data},
            {99, "target", "Target", Side::opposition, target}
        }},
    seed);
    check(c->snapshot().actor == 1, "Independent initiative seed starts with archer");
    return c;
}

void selection()
{
    auto rules = module();
    for (const auto &klass : srd5::character_rules()->choices(CreationField::character_class))
    {
        CampaignParty p(module());
        auto id = p.add_pc(hero(klass.id));
        p.award_experience(2700, "archery");
        grow(p, id, 3);
        auto choice = p.default_advancement(id);
        choice.feat = "archery";
        choice.abilities = {};
        const auto options = p.advancement_options(id);
        const auto option = std::find_if(options.feats.begin(), options.feats.end(),
                                         [](const auto & f)
        {
            return f.id == "archery";
        });
        check(option != options.feats.end() &&
              option->available ==
              (klass.id == "fighter" || klass.id == "paladin" || klass.id == "ranger"),
              "Existing feat selector requires Fighting Style");
        const auto before = encode_campaign(p, nullptr, "archery");
        if (klass.id != "fighter" && klass.id != "paladin" && klass.id != "ranger")
        {
            rejects(
                [&]
            {
                p.advance(id, choice);
            });
            check(encode_campaign(p, nullptr, "archery") == before,
                  "Missing entitlement rejects atomically");
            continue;
        }
        auto invalid = choice;
        invalid.abilities[Ability::strength] = 1;
        rejects(
            [&]
        {
            p.advance(id, invalid);
        });
        const auto preview = p.preview_advancement(id, choice);
        check(encode_campaign(p, nullptr, "archery") == before,
              "Preview and rejected allocation preserve campaign");
        p.advance(id, choice);
        check(p.member(id).character.sheet().grants == preview.character.sheet().grants,
              "Confirmation retains previewed grant");
        const auto &sheet = p.member(id).character.sheet();
        check(std::find(sheet.grants.begin(), sheet.grants.end(),
                        FeatureGrant{"feat:archery",
                                     "class:" + klass.id + ":ability_score_improvement",
                                     4,
                                     {}}) != sheet.grants.end(),
              "Feat records its real entitlement and acquisition level");
        auto bad = sheet;
        bad.grants.push_back({"feat:archery", "class:fighter:ability_score_improvement", 4, {}});
        rejects(
            [&]
        {
            (void)rules->character_profile(bad, {});
        });
        bad = sheet;
        std::erase_if(bad.grants,
                      [](const auto & g)
        {
            return g.id == "feature:fighting_style";
        });
        rejects(
            [&]
        {
            (void)rules->character_profile(bad, {});
        });
        auto before_four = hero().sheet();
        before_four.grants.push_back(
        {"feat:archery", "class:fighter:ability_score_improvement", 1, {}});
        rejects(
            [&]
        {
            (void)rules->character_profile(before_four, {});
        });
        const auto saved = encode_campaign(p, nullptr, "archery");
        CampaignParty restored(module());
        restored.restore(
            decode_campaign(saved, *srd5::character_rules(), *rules, "archery", nullptr).party);
        check(encode_campaign(restored, nullptr, "archery") == saved,
              "Selected Archery survives canonical campaign reload");
    }
}

void attacks()
{
    const auto content = read(root / "data/rules/srd-5.2.1/combat.rules");
    const auto target = "\ncreature target 1 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n";
    auto rules = srd5::parse_content(content + target);
    const auto archer = leveled(), baseline = leveled("defense");
    for (const auto weapon :
            {"dart", "light_crossbow", "shortbow", "sling", "blowgun",
             "hand_crossbow", "heavy_crossbow", "longbow", "musket", "pistol",
             "dagger", "handaxe", "javelin", "light_hammer", "spear", "trident"
            })
    {
        const bool ranged =
            std::string_view(weapon) == "dart" || std::string_view(weapon) == "light_crossbow" ||
            std::string_view(weapon) == "shortbow" || std::string_view(weapon) == "sling" ||
            std::string_view(weapon) == "blowgun" || std::string_view(weapon) == "hand_crossbow" ||
            std::string_view(weapon) == "heavy_crossbow" || std::string_view(weapon) == "longbow" ||
            std::string_view(weapon) == "musket" || std::string_view(weapon) == "pistol";
        for (unsigned seed :
                {
                    72u, 89u, 11u
                })
        {
            auto a = battle(*rules, archer, weapon, {3, 1}, seed),
                 b = battle(*rules, baseline, weapon, {3, 1}, seed);
            auto copy = rules->restore(a->save());
            const auto ticket = command(*a, "ranged");
            check(a->submit(ticket) && copy->submit(ticket) && a->save() == copy->save(),
                  "Archery attack resumes identically from saved profile");
            check(!a->submit(ticket) && a->save() == copy->save(),
                  "Stale attack cannot change action, RNG or state");
            check(b->submit(command(*b, "ranged")), "Control attack accepted");
            const auto actual = result(*a), control = result(*b);
            check(
                arg(actual, "bonus") == std::to_string(ranged ? 6 : 4) &&
                arg(control, "bonus") == "4",
                "Only Ranged weapon category gets Archery, including darts but excluding thrown melee weapons");
            check(arg(actual, "roll") == std::to_string(seed == 72   ? 20
                    : seed == 89 ? 17
                    : 1) &&
                  arg(actual, "roll") == arg(control, "roll"),
                  "Archery preserves independent natural rolls, criticals and natural-one misses");
            if (seed != 11)
                check(arg(actual, "damage") == arg(control, "damage"),
                      "Archery never modifies damage or consumes extra dice");
        }
    }
    auto soldier = battle(*rules, leveled("archery", "soldier"), "shortbow");
    check(soldier->submit(command(*soldier, "ranged")), "Archery plus Soldier attack starts");
    auto pending = rules->restore(soldier->save());
    check(soldier->save() == pending->save() && arg(result(*soldier), "bonus") == "7",
          "Restored Savage Attacker hit retains Archery's attack bonus");
    if (soldier->snapshot().free_movement)
        check(soldier->submit(command(*soldier, "end")),
              "Decline optional Champion movement before Surge");
    check(soldier->submit(command(*soldier, "action_surge")) &&
          soldier->submit(command(*soldier, "ranged")),
          "Archery remains usable on the additional non-Magic action");
    check(rules->restore(soldier->save())->save() == soldier->save(),
          "Spent Surge and Savage state round trip with Archery");
    auto a = battle(*rules, archer, "shortbow", {2, 1}),
         b = battle(*rules, baseline, "shortbow", {2, 1});
    check(a->submit(command(*a, "melee")) && b->submit(command(*b, "melee")),
          "Unarmed fallback is playable while holding bow");
    check(arg(result(*a), "bonus") == "4" && arg(result(*a), "bonus") == arg(result(*b), "bonus"),
          "Archery does not improve unarmed melee fallback");
    a = battle(*rules, archer, "shortbow", {20, 1});
    check(a->submit(command(*a, "ranged")), "Long-range shot allowed");
    check(arg(result(*a), "bonus") == "6" && arg(result(*a), "roll") == "8" &&
          arg(result(*a), "disadvantage") == " (disadvantage)",
          "Archery retains long-range Disadvantage and its +2 bonus");
    auto hard = srd5::parse_content(
                    content + "\ncreature target 23 1000 0 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
    a = battle(*hard, archer, "shortbow");
    b = battle(*hard, baseline, "shortbow");
    check(a->submit(command(*a, "ranged")) && b->submit(command(*b, "ranged")),
          "Boundary attacks accepted");
    check(result(*a).source.find("{damage}") != std::string::npos &&
          result(*b).source.find("misses") != std::string::npos,
          "Natural 17 plus Archery 6 hits AC23; baseline 4 misses");
}

void forged_profiles()
{
    auto rules = module();
    auto profile =
        rules->character_profile(leveled().sheet(), std::array<std::string, 1> {"shortbow"}).data;
    auto encounter = Encounter{{8, 8, std::vector<Terrain>(64)},
        {   {1, "campaign-character", "Archer", Side::party, {1, 1}, profile},
            {99, "vanguard", "Target", Side::opposition, {5, 1}}
        }};
    auto wrong_mask = profile;
    replace(wrong_mask, "PC42 4 4 ", "PC42 4 0 ");
    encounter.participants[0].character_profile = wrong_mask;
    rejects(
        [&]
    {
        (void)rules->create(encounter, 13);
    });
    replace(profile, "PC42", "PC16");
    encounter.participants[0].character_profile = profile;
    rejects(
        [&]
    {
        (void)rules->create(encounter, 13);
    });
}
} // namespace

int main()
{
    try
    {
        selection();
        attacks();
        forged_profiles();
        std::cout << "Archery checks passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
