#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/combat_demo.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

using namespace opengold;
using namespace opengold::rules;

// A Rogue (Thief) from level 5 to 10 (LEVELS-1): Sneak Attack dice, feats at
// 8 and 10, Expertise at 6, Cunning Strike, Uncanny Dodge, Evasion, Reliable
// Talent and Supreme Sneak.
namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const std::string &message)
{
    if (!ok)
        throw std::runtime_error(message);
}

std::unique_ptr<RulesModule> module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

CharacterDraft draft(std::string klass, std::string race = "dwarf")
{
    CharacterDraft d;
    d.race = std::move(race);
    d.gender = "female";
    d.character_class = std::move(klass);
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Veteran";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.rolls[0] = {{6, 6, 6, 1}, 3};
    const auto rules = srd5::character_rules();
    for (bool filled = true; filled;)
    {
        filled = false;
        for (const auto &group : rules->training_options(d))
            if (!d.training.contains(group.id))
            {
                for (unsigned i = 0; i < group.count; ++i)
                    d.training[group.id].push_back(group.options.at(i).id);
                filled = true;
                break;
            }
    }
    return d;
}



// A Rogue of the level, advanced through the rules' ordinary level-ups.
Character rogue(unsigned level, std::string race = "dwarf")
{
    // Stealth among its skills, for Reliable Talent's hiding.
    auto d = draft("rogue", std::move(race));
    d.training["class:rogue"] = {"acrobatics", "deception", "perception", "stealth"};
    d.training["class:rogue:expertise"] = {"perception", "acrobatics"};
    Character hero(*srd5::character_rules(), d, {});
    VitalState life;
    const auto rules = module();
    while (hero.sheet().level < static_cast<int>(level))
        check(hero.advance(*rules, life), "An ordinary level-up");
    return hero;
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

std::unique_ptr<RulesModule> dummy_rules()
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    // A dummy that hits hard enough to matter, and a dull one that barely notices.
    return srd5::parse_content(std::string{std::istreambuf_iterator<char>(in), {}} +
                               "\ncreature dummy 10 1000 -10 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "creature dull 10 1000 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "perception dull 4\n");
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

// The Rogue with a dagger at (1, 1), the dummy at (2, 1) and an ally beside
// the dummy, so Sneak Attack applies; at the Rogue's turn.
std::unique_ptr<CombatSession> fight(const RulesModule &rules, const Character &hero,
                                     std::uint64_t seed, bool ally = true)
{
    const auto profile = rules.character_profile(hero.sheet(), std::vector<std::string> {"dagger"}).data;
    std::vector<Participant> setup{
        {1, "campaign-character", "Rogue", Side::party, {1, 1}, profile},
        {98, "dummy", "Dummy", Side::opposition, {2, 1}}};
    if (ally)
        setup.push_back({2, "dull", "Ally", Side::party, {3, 1}});
    auto c = rules.create({{8, 3, std::vector<Terrain>(24)}, setup}, seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
        check(submit(*c, "initiative_keep") || submit(*c, "end"), "Reach the Rogue's turn");
    return c;
}

void progression_checks()
{
    {
        CampaignParty party(module());
        const auto id = party.add_pc(Character(*srd5::character_rules(), draft("rogue"), {}));
        party.award_experience(165000, "levels-xp");
        while (party.can_advance(id))
            party.advance(id, party.default_advancement(id));
        check(party.member(id).character.sheet().level == 15,
              "In the campaign a Rogue reaches the hall's level 15");
    }
    const auto ten = rogue(10);
    const auto &grants = ten.sheet().grants;
    check(std::count_if(grants.begin(), grants.end(), [](const auto & g)
    {
        return g.source_id == "class:rogue:ability_score_improvement";
    }) == 3, "Feats or ability points at levels 4, 8 and 10");
    check(std::count_if(grants.begin(), grants.end(), [](const auto & g)
    {
        return g.source_id == "class:rogue:expertise:6" && g.id.starts_with("expertise:");
    }) == 2, "Expertise in two more skills at level 6");
    CampaignParty party(module());
    party.add_pc(ten);
    const auto bytes = encode_campaign(party, nullptr, "rogue-levels");
    CampaignParty next(module());
    next.restore(decode_campaign(bytes, *srd5::character_rules(), party.rule_module(),
                                 "rogue-levels", nullptr).party);
    check(encode_campaign(next, nullptr, "rogue-levels") == bytes,
          "A level-10 Rogue saves and reloads exactly");

    const auto fifteen = rogue(15);
    const auto rules = module();
    check(fifteen.sheet().level == 15 && !rules->advancement_options(fifteen.sheet()).level,
          "Level 15 is the Rogue's cap");
    const auto holds = [&](std::string_view id, int level)
    {
        return std::any_of(fifteen.sheet().grants.begin(), fifteen.sheet().grants.end(),
                           [&](const auto & g)
        {
            return g.id == id && g.level == level;
        });
    };
    check(holds("feature:improved_cunning_strike", 11) && holds("feature:use_magic_device", 13) &&
          holds("feature:devious_strikes", 14) && holds("feature:slippery_mind", 15),
          "The level 11-15 features arrive at their levels");
    unsigned proficient = 0;
    for (const auto ability : all_abilities)
        proficient += fifteen.sheet().save_proficiencies[ability] ? 1 : 0;
    check(proficient == 4 && fifteen.sheet().save_proficiencies[Ability::wisdom] &&
          fifteen.sheet().save_proficiencies[Ability::charisma],
          "Slippery Mind: Wisdom and Charisma saves at level 15");
}

// Improved Cunning Strike: two effects on one Sneak Attack from level 11.
void improved_cunning_checks()
{
    const auto rules = dummy_rules();
    check(!offered(*fight(*rules, rogue(10), 1), "melee_trip+withdraw"), "Not before level 11");
    for (std::uint64_t seed = 1; seed < 60; ++seed)
    {
        auto c = fight(*rules, rogue(11), seed);
        check(offered(*c, "melee_trip+withdraw") && !offered(*c, "melee_daze"),
              "Pairs of effects; no Devious Strikes before level 14");
        check(submit(*c, "melee_trip+withdraw", 98), "A Trip and Withdraw attack");
        if (!logged(*c, "adds Sneak Attack: 4d6"))
            continue;
        check(logged(*c, "Rogue may withdraw"), "Both effects: the withdrawal follows");
        return;
    }
    throw std::runtime_error("No seed's paired Cunning Strike hits");
}

// Devious Strikes: Daze, Knock Out and Obscure at level 14.
void devious_checks()
{
    const auto rules = dummy_rules();
    bool knocked = false, blinded = false, dazed = false;
    for (std::uint64_t seed = 1; seed < 80 && !(knocked && blinded && dazed); ++seed)
        for (const auto *verb : {"melee_knock_out", "melee_obscure", "melee_daze"})
        {
            auto c = fight(*rules, rogue(14), seed);
            check(offered(*c, verb) && offered(*c, "melee_trip+knock_out") &&
                  !offered(*c, "melee_withdraw+knock_out+trip"), "Devious Strikes are offered");
            check(submit(*c, verb, 98), "A Devious Strike");
            const auto conditions = unit(*c, 98).conditions;
            const auto shows = [&](std::string_view text)
            {
                return std::any_of(conditions.begin(), conditions.end(), [&](const auto & m)
                {
                    return m.source == text;
                });
            };
            const auto saved = c->save();
            check(rules->restore(saved)->save() == saved, "A checkpoint keeps the effect");
            if (std::string_view(verb) == "melee_knock_out" && logged(*c, "Dummy is knocked out."))
                knocked = shows("Unconscious (Knock Out)");
            if (std::string_view(verb) == "melee_obscure" && logged(*c, "Dummy is Blinded."))
                blinded = shows("Blinded");
            if (std::string_view(verb) != "melee_daze" || !logged(*c, "Dummy is dazed."))
                continue;
            // On its turn the dazed dummy moves, and then may not attack.
            check(submit(*c, "end"), "The Rogue ends its turn");
            for (unsigned steps = 0; steps < 6 && c->snapshot().actor != 98; ++steps)
                check(submit(*c, "end") || c->submit(choose_demo_command(*c)), "Reach the dummy");
            if (c->snapshot().actor != 98)
                continue;
            bool moved = false;
            for (const auto &command : c->legal_commands())
                if (!moved && command.verb == "move" && command.actor == 98)
                    moved = c->submit(command);
            check(moved && !offered(*c, "melee"), "Having moved, the dazed dummy cannot attack");
            dazed = true;
        }
    check(knocked && blinded && dazed, "Each Devious Strike lands on some seed");
}

// Cunning Strike: Trip and Withdraw give up a Sneak Attack die.
void cunning_checks()
{
    const auto rules = dummy_rules();
    bool tripped = false, withdrew = false;
    for (std::uint64_t seed = 1; seed < 60 && !(tripped && withdrew); ++seed)
        for (const auto *verb : {"melee_trip", "melee_withdraw"})
        {
            auto c = fight(*rules, rogue(5), seed);
            check(offered(*c, verb) && !offered(*c, "melee_stealth"),
                  "Cunning Strike is offered with Sneak Attack; Stealth Attack waits for level 9");
            check(submit(*c, verb, 98), "A Cunning Strike attack");
            if (!logged(*c, "adds Sneak Attack: 2d6"))
                continue;
            if (std::string_view(verb) == "melee_trip")
                tripped |= logged(*c, "Dummy is tripped and falls Prone.");
            else
            {
                const auto free = c->snapshot().free_movement;
                withdrew = logged(*c, "Rogue may withdraw 15 feet") && free &&
                           free->remaining_feet == 15;
                const auto saved = c->save();
                check(rules->restore(saved)->save() == saved, "A checkpoint keeps the withdrawal");
            }
        }
    check(tripped && withdrew, "Trip and Withdraw both work on some seed");
    auto alone = fight(*rules, rogue(5), 1, false);
    check(!offered(*alone, "melee_trip"), "No Cunning Strike without Sneak Attack");
}

// Uncanny Dodge: the Rogue's Reaction halves a hit's damage.
void uncanny_dodge_checks()
{
    const auto rules = dummy_rules();
    for (std::uint64_t seed = 1; seed < 60; ++seed)
    {
        auto c = fight(*rules, rogue(5), seed, false);
        check(submit(*c, "end"), "The Rogue waits");
        for (unsigned steps = 0; steps < 4 && c->snapshot().actor == 98 &&
                !offered(*c, "uncanny_dodge"); ++steps)
            check(c->submit(choose_demo_command(*c)), "The dummy attacks");
        if (!offered(*c, "uncanny_dodge"))
            continue;
        check(submit(*c, "uncanny_dodge") && logged(*c, "Rogue uses Uncanny Dodge") &&
              !unit(*c, 1).reaction, "Uncanny Dodge halves the hit for the Reaction");
        return;
    }
    throw std::runtime_error("No seed's dummy hits the Rogue");
}

// Evasion: a Dragonborn Rogue's breath (Dexterity save, half on a success)
// leaves another level-7 Rogue unhurt when it saves.
void evasion_checks()
{
    const auto rules = dummy_rules();
    const auto breather = rogue(7, "dragonborn");
    const auto profile = rules->character_profile(breather.sheet(), {}).data;
    const auto foe = rules->character_profile(rogue(7).sheet(), {}).data;
    for (std::uint64_t seed = 1; seed < 80; ++seed)
    {
        auto c = rules->create({{8, 3, std::vector<Terrain>(24)},
            {   {1, "campaign-character", "Breather", Side::party, {1, 1}, profile},
                {98, "campaign-character", "Evader", Side::opposition, {2, 1}, foe}
            }},
        seed);
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
            check(submit(*c, "initiative_keep") || submit(*c, "end"), "Reach the breather's turn");
        if (c->snapshot().actor != 1 || !submit(*c, "breath_weapon_line"))
            continue;
        for (const auto &command : c->legal_commands())
            if (command.verb == "area_move" && command.destination == Cell{2, 1})
            {
                check(c->submit(command), "Aim the breath at the Evader");
                break;
            }
        check(submit(*c, "area_cast"), "The breath is exhaled");
        if (!logged(*c, "Evader takes"))
            continue;
        if (logged(*c, "Evader takes 0 "))
            return;
    }
    throw std::runtime_error("No seed's save leaves the evading Rogue unhurt");
}

// Reliable Talent: a hiding level-7 Rogue's low d20 counts as 10.
void reliable_checks()
{
    const auto rules = dummy_rules();
    const auto profile = rules->character_profile(rogue(7).sheet(), {}).data;
    std::vector<Terrain> walled(72);
    for (int y = 0; y <= 2; ++y)
        walled[y * 12 + 4] = Terrain::obstacle;
    for (std::uint64_t seed = 1; seed < 80; ++seed)
    {
        auto c = rules->create({{12, 6, walled},
            {   {1, "campaign-character", "Rogue", Side::party, {3, 1}, profile},
                {98, "dull", "Foe", Side::opposition, {8, 1}}
            }},
        seed);
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
            check(submit(*c, "initiative_keep") || submit(*c, "end"), "Reach the Rogue's turn");
        check(submit(*c, "cunning_hide"), "The Rogue hides");
        if (logged(*c, "Reliable Talent: Rogue's d20 of "))
            return;
    }
    throw std::runtime_error("No seed rolls a low d20 for the hiding Rogue");
}

// Supreme Sneak: a hidden level-9 Rogue's Stealth Attack that deals Sneak
// Attack damage leaves it hidden.
void supreme_sneak_checks()
{
    const auto rules = dummy_rules();
    const auto profile =
        rules->character_profile(rogue(9).sheet(), std::vector<std::string> {"dagger"}).data;
    std::vector<Terrain> walled(72);
    for (int y = 0; y <= 2; ++y)
        walled[y * 12 + 4] = Terrain::obstacle;
    const auto step = [](CombatSession & c, Cell cell)
    {
        for (const auto &command : c.legal_commands())
            if (command.verb == "move" && command.destination == cell)
                return c.submit(command);
        return false;
    };
    for (std::uint64_t seed = 1; seed < 120; ++seed)
    {
        auto c = rules->create({{12, 6, walled},
            {   {1, "campaign-character", "Rogue", Side::party, {3, 1}, profile},
                {98, "dull", "Foe", Side::opposition, {6, 1}}
            }},
        seed);
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
            check(submit(*c, "initiative_keep") || submit(*c, "end"), "Reach the Rogue's turn");
        if (!submit(*c, "cunning_hide") || !unit(*c, 1).hidden)
            continue;
        if (!step(*c, {3, 2}) || !step(*c, {3, 3}) || !step(*c, {4, 3}) || !step(*c, {5, 2}) ||
                !unit(*c, 1).hidden)
            continue;
        check(offered(*c, "melee_stealth"), "A hidden Rogue is offered Stealth Attack");
        check(submit(*c, "melee_stealth", 98), "The Stealth Attack");
        if (!logged(*c, "adds Sneak Attack: 4d6"))
            continue;
        check(logged(*c, "Rogue stays hidden (Stealth Attack).") && unit(*c, 1).hidden,
              "Dealing Sneak Attack damage, the Rogue stays hidden");
        return;
    }
    throw std::runtime_error("No seed's Stealth Attack hits from hiding");
}
} // namespace

int main()
{
    try
    {
        progression_checks();
        improved_cunning_checks();
        devious_checks();
        cunning_checks();
        uncanny_dodge_checks();
        evasion_checks();
        reliable_checks();
        supreme_sneak_checks();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Rogue level tests passed\n";
}
