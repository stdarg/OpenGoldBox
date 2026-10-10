#include "opengold/campaign_party.h"
#include "opengold/character.h"
#include "opengold/combat_demo.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

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

std::string read(const std::filesystem::path &p)
{
    std::ifstream in(p);
    check(bool(in), "Read rules");
    return {std::istreambuf_iterator<char>(in), {}};
}

// A sturdy AC 1 target that hits back.
std::unique_ptr<RulesModule> rules()
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

// A Bard with Charisma 18, advanced to the given level.
Character bard(unsigned level = 1)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "bard";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Bard";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.rolls[5] = {{6, 6, 6, 1}, 3};
    d.training = {{"class:bard", {"performance", "persuasion", "history"}}};
    d.cantrips = {"vicious_mockery", "starry_wisp"};
    d.spells = SpellChoices{{},
        std::vector<std::string> {"dissonant_whispers", "faerie_fire", "healing_word", "charm_person"},
        {}, {}};
    CampaignParty party(srd5::load(root / "data/rules/srd-5.2.1/combat.rules"));
    const auto id = party.add_pc(Character(*srd5::character_rules(), d, {}));
    party.award_experience(2700, "bard-xp");
    for (unsigned n = 1; n < level; ++n)
        party.advance(id, party.default_advancement(id));
    return party.member(id).character;
}

CombatantView unit(const CombatSession &c, EntityId id)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing combatant");
}

bool submit(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && (!target || command.target == target))
            return c.submit(command);
    return false;
}

bool aim(CombatSession &c, Cell cell)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == "area_move" && command.destination == cell)
            return c.submit(command);
    return false;
}

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log();
    return std::any_of(log.begin(), log.end(), [&](const auto & line)
    {
        return line.find(text) != std::string::npos;
    });
}

bool has_condition(const CombatSession &c, EntityId id, std::string_view label)
{
    const auto conditions = unit(c, id).conditions;
    return std::any_of(conditions.begin(), conditions.end(), [&](const auto & condition)
    {
        return condition.source == label;
    });
}

bool has_grant(const Character &hero, std::string_view id)
{
    const auto &grants = hero.sheet().grants;
    return std::any_of(grants.begin(), grants.end(), [&](const auto & g)
    {
        return g.id == id;
    });
}

// The Bard (1), an ally (2) and an enemy (98) 15 feet away.
std::unique_ptr<CombatSession> battle(const RulesModule &module, const Character &hero,
                                      std::uint64_t seed = 5)
{
    const auto profile = module.character_profile(hero.sheet(), std::vector<std::string> {}).data;
    auto c = module.create({{12, 6, std::vector<std::uint8_t>(72)},
        {   {1, "campaign-character", "Bard", 0, {1, 1}, profile},
            {2, "target", "Ally", 0, {3, 2}},
            {98, "target", "Enemy", 1, {4, 1}}
        }},
    seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*c, "end"), "Reach the Bard's turn");
    check(c->snapshot().actor == 1, "The Bard acts");
    return c;
}

// Ends turns until the given creature acts.
void reach(CombatSession &c, EntityId id)
{
    check(submit(c, "end"), "End the turn");
    for (unsigned turns = 0; c.snapshot().actor != id && turns < 4; ++turns)
        check(submit(c, "end"), "End a turn");
}

void spell_checks()
{
    auto module = rules();
    {
        // Charisma is the Bard's spellcasting ability: 8 + 2 + 4 at Charisma 18.
        auto c = battle(*module, bard());
        check(submit(*c, "vicious_mockery", 98) && logged(*c, "vs DC 14"),
              "Bard spells use Charisma for their save DC");
    }
    bool mocked = false, fled = false, outlined = false;
    for (std::uint64_t seed = 1; seed < 64 && !(mocked && fled && outlined); ++seed)
    {
        {
            auto c = battle(*module, bard(), seed);
            check(submit(*c, "vicious_mockery", 98) && logged(*c, "Enemy Wisdom save"),
                  "Vicious Mockery calls for a Wisdom save");
            mocked |= logged(*c, "Enemy has Disadvantage on its next attack roll.");
        }
        {
            auto c = battle(*module, bard(), seed);
            check(submit(*c, "dissonant_whispers", 98) && logged(*c, "Psychic damage"),
                  "Dissonant Whispers deals Psychic damage either way");
            if (logged(*c, "Enemy flees from Bard."))
            {
                fled = true;
                check(unit(*c, 98).cell.x > 4 && !unit(*c, 98).reaction,
                      "A failed save spends the Reaction running away");
            }
        }
        {
            auto c = battle(*module, bard(), seed);
            check(submit(*c, "faerie_fire") && aim(*c, Cell{4, 1}) && submit(*c, "area_cast"),
                  "Faerie Fire on the enemy");
            if (!has_condition(*c, 98, "Outlined (Faerie Fire)"))
                continue;
            outlined = true;
            reach(*c, 2);
            check(submit(*c, "melee", 98) && logged(*c, "(advantage)"),
                  "Attacks against an outlined creature have Advantage");
        }
    }
    check(mocked && fled && outlined, "Some seeds fail each save");
    auto c = battle(*module, bard());
    check(submit(*c, "starry_wisp", 98) && has_condition(*c, 98, "Lit (Starry Wisp)"),
          "Starry Wisp keeps its target from being Invisible");
}

void advancement_checks()
{
    const auto third = bard(3), fourth = bard(4);
    check(has_grant(bard(2), "feature:jack_of_all_trades"), "Level two brings Jack of All Trades");
    const auto &grants = third.sheet().grants;
    check(has_grant(third, "subclass:lore") && has_grant(third, "feature:bonus_proficiencies") &&
          std::count_if(grants.begin(), grants.end(), [](const auto & g)
    {
        return g.source_id == "subclass:bard:lore" && g.id.starts_with("skill:");
    }) == 3,
    "Level three brings the College of Lore and three skills");
    check(fourth.sheet().level == 4 && fourth.sheet().prepared_spells.size() == 7,
          "A level-four Bard prepares seven spells");
}

unsigned inspiration_left(const CombatSession &c)
{
    for (const auto &pool : unit(c, 1).resources)
        if (pool.id == "bardic_inspiration")
            return pool.remaining;
    throw std::runtime_error("No Bardic Inspiration pool");
}

void inspiration_checks()
{
    auto module = rules();
    {
        auto c = battle(*module, bard());
        check(inspiration_left(*c) == 4 && submit(*c, "bardic_inspiration", 2) &&
              logged(*c, "Bard inspires Ally.") && has_condition(*c, 2, "Inspired") &&
              inspiration_left(*c) == 3 && unit(*c, 1).action,
              "Bardic Inspiration is a Bonus Action; Charisma 18 gives four uses");
    }
    // The ally (AC 20 against the target's +4) misses often; the die is asked about.
    bool asked = false;
    for (std::uint64_t seed = 1; seed < 64 && !asked; ++seed)
    {
        auto c = battle(*module, bard(), seed);
        check(submit(*c, "bardic_inspiration", 2), "Inspire the ally");
        reach(*c, 2);
        check(submit(*c, "melee", 98), "The ally attacks");
        if (!c->snapshot().reaction_pending)
            continue;
        check(c->snapshot().actor == 2 && submit(*c, "inspire") &&
              logged(*c, "Bardic Inspiration adds") && !has_condition(*c, 2, "Inspired"),
              "A miss asks the inspired ally, who adds the die");
        asked = true;
    }
    check(asked, "Some seed misses with the inspired ally");
    // The enemy's hit on the ally asks the Lore Bard about Cutting Words.
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(*module, bard(3), seed);
        reach(*c, 98);
        check(submit(*c, "melee", 2), "The enemy attacks the ally");
        if (!c->snapshot().reaction_pending)
            continue;
        check(c->snapshot().actor == 1 && submit(*c, "cutting") &&
              logged(*c, "Bard uses Cutting Words: -") && !unit(*c, 1).reaction,
              "Cutting Words spends the Bard's Reaction and a use");
        return;
    }
    throw std::runtime_error("No seed asks about Cutting Words");
}

// The automated policy inspires an ally before acting.
void policy_checks()
{
    auto module = rules();
    auto c = battle(*module, bard());
    const auto inspire = choose_demo_command(*c);
    check(inspire.verb == "bardic_inspiration" && inspire.target == 2,
          "The policy gives the ally Bardic Inspiration first");
}

} // namespace

int main()
{
    try
    {
        policy_checks();
        spell_checks();
        advancement_checks();
        inspiration_checks();
        std::cout << "Bard tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
