#include "opengold/campaign_party.h"
#include "opengold/character.h"
#include "opengold/combat_demo.h"
#include "opengold/srd5.h"
#include "status_effects.h"
#include <algorithm>
#include <cstdio>
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

// A sturdy AC 1 target that hits back, and a guard of AC 20 that attacks at +4
// and so often misses, or hits narrowly.
std::unique_ptr<RulesModule> rules()
{
    return srd5::parse_content(read(root / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature target 1 1000 0 30 20 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "creature guard 20 1000 0 30 4 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n");
}

// A level-one Bard draft with Charisma 18 that prepares the given spells.
CharacterDraft bard_draft(std::vector<std::string> prepared)
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
    d.spells = SpellChoices{{}, std::move(prepared), {}, {}};
    return d;
}

// A Bard with Charisma 18, advanced to the given level.
Character bard(unsigned level = 1, std::string background = "sage")
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "bard";
    d.background = std::move(background);
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

// The Bard (1), an ally (2) and an enemy (98) 15 feet away, both `creature`.
std::unique_ptr<CombatSession> battle(const RulesModule &module, const Character &hero,
                                      std::uint64_t seed = 5, const std::string &creature = "target")
{
    const auto profile = module.character_profile(hero.sheet(), std::vector<std::string> {}).data;
    auto c = module.create({{12, 6, std::vector<Terrain>(72)},
        {   {1, "campaign-character", "Bard", Side::party, {1, 1}, profile},
            {2, creature, "Ally", Side::party, {3, 2}},
            {98, creature, "Enemy", Side::opposition, {4, 1}}
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

// The d20 total and AC of the logged attack line starting `prefix`.
struct AttackRoll
{
    int total{}, ac{};
};

AttackRoll attack_roll(const CombatSession &c, std::string_view prefix)
{
    for (const auto &line : c.snapshot().log())
        if (line.starts_with(prefix))
        {
            int natural = 0, bonus = 0, ac = 0;
            std::sscanf(line.c_str() + prefix.size(), ": d20 %d + %d vs AC %d", &natural, &bonus, &ac);
            return {natural == 1 ? -100 : natural + bonus, ac};
        }
    throw std::runtime_error("No attack line");
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
    // The guard (+4 against AC 20) misses often. Its die is added without
    // asking, but only to a miss it could turn into a hit.
    bool used = false, kept = false;
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(*module, bard(), seed, "guard");
        check(submit(*c, "bardic_inspiration", 2), "Inspire the ally");
        reach(*c, 2);
        check(submit(*c, "melee", 98) && !c->snapshot().reaction_pending,
              "The ally attacks; nobody is asked about the die");
        if (logged(*c, "Bardic Inspiration adds"))
        {
            used = true;
            check(!has_condition(*c, 2, "Inspired"), "The used die is gone");
            continue;
        }
        const auto miss = attack_roll(*c, "Ally -> Enemy");
        if (miss.total < miss.ac)
        {
            kept = true;
            check(miss.ac - miss.total > 6 && has_condition(*c, 2, "Inspired"),
                  "Only a miss the d6 cannot rescue keeps the die");
        }
    }
    check(used && kept, "Some seeds use the die, others keep it");
    // An enemy's hit on the ally that a d6 could undo draws the Lore Bard's
    // Cutting Words, without asking.
    bool cut = false;
    for (std::uint64_t seed = 1; seed < 64; ++seed)
    {
        auto c = battle(*module, bard(3), seed, "guard");
        reach(*c, 98);
        check(submit(*c, "melee", 2) && !c->snapshot().reaction_pending,
              "The enemy attacks the ally; the Bard is not asked");
        if (!logged(*c, "Bard uses Cutting Words: -"))
            continue;
        cut = true;
        check(!unit(*c, 1).reaction && inspiration_left(*c) == 3,
              "Cutting Words spends the Bard's Reaction and a use");
        check(logged(*c, "turning the hit into a miss.") != logged(*c, "the attack still hits."),
              "The log says whether the attack still hits");
    }
    check(cut, "Some seed hits the ally");
}

// Bardic Inspiration on the saves that are rolled outside a command: the
// repeated save at the end of a turn and the Concentration save.
void inspired_save_checks()
{
    namespace fx = opengold::srd5::detail;
    fx::EffectState effects;
    fx::apply_spell_benefit(effects, {.scope = 1, .actor = 1}, "Bard", fx::EffectKind::inspired, 6);
    std::uint64_t rng = 7;
    const fx::SaveResult near{fx::Ability::wisdom, 9, 2, 15, 0, false},
          far{fx::Ability::wisdom, 2, 2, 15, 0, false};
    check(fx::inspiration_die(effects.active, far, rng) == 0 && rng == 7,
          "A save 11 short keeps the die and rolls nothing");
    const int die = fx::inspiration_die(effects.active, near, rng);
    check(die >= 1 && die <= 6, "A save 4 short rolls the d6");
    fx::spend_inspiration(effects);
    check(!fx::has_effect(effects, fx::EffectKind::inspired), "The die is spent");
}

// The Bard's Initiative total in a fixed fight: its d20 is the same whatever
// the Bard's level, so totals differ only by their bonuses.
int initiative(const RulesModule &module, const Character &hero)
{
    const auto profile = module.character_profile(hero.sheet(), std::vector<std::string> {}).data;
    const auto c = module.create({{12, 6, std::vector<Terrain>(72)},
        {   {1, "campaign-character", "Bard", Side::party, {1, 1}, profile},
            {98, "target", "Enemy", Side::opposition, {4, 1}}
        }},
    5);
    return unit(*c, 1).initiative;
}

// Jack of All Trades adds half the Proficiency Bonus to Initiative from level
// two (SRD-DECISIONS keeps it for that), unless Alert already adds all of it.
void jack_of_all_trades_checks()
{
    auto module = rules();
    check(initiative(*module, bard(2)) == initiative(*module, bard(1)) + 1,
          "Jack of All Trades adds +1 to Initiative at level two");
    // The Criminal's +2 Dexterity raises the modifier by 1; Alert adds +2.
    check(initiative(*module, bard(1, "criminal")) == initiative(*module, bard(1)) + 3,
          "A level-one Criminal Bard adds Alert's Proficiency Bonus");
    check(initiative(*module, bard(2, "criminal")) == initiative(*module, bard(1, "criminal")),
          "Jack of All Trades does not add to Alert's whole Proficiency Bonus");
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

// A level-one Bard knows two cantrips and prepares four level-one Bard spells;
// creation refuses anything else.
void creation_checks()
{
    auto module = srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
    const auto creation = srd5::character_rules();
    const auto draft =
        bard_draft({"dissonant_whispers", "faerie_fire", "healing_word", "charm_person"});
    const Character hero(*creation, draft, {});
    const auto access = module->spell_access(hero.sheet());
    check(access.cantrip_choices == 2 && access.prepared_choices == 4 &&
          creation->cantrip_options(draft).count == 2 &&
          creation->spell_choice_options(draft).prepared_count == 4,
          "A level-one Bard knows two cantrips and prepares four spells");
    for (const auto &bad : std::vector<std::vector<std::string>>
{
    {"dissonant_whispers", "faerie_fire", "healing_word", "magic_missile"},
        {"dissonant_whispers", "faerie_fire", "healing_word", "healing_word"},
        {"dissonant_whispers", "faerie_fire", "healing_word", "heat_metal"},
        {"dissonant_whispers", "faerie_fire", "healing_word", "charm_person", "heroism"}
    })
    {
        bool refused = false;
        try
        {
            (void)creation->evaluate(bard_draft(bad), NameRequirement::required);
        }
        catch (const std::exception &)
        {
            refused = true;
        }
        check(refused, "Creation refuses a foreign, repeated, level-two or fifth spell");
    }
}

} // namespace

int main()
{
    try
    {
        creation_checks();
        policy_checks();
        spell_checks();
        advancement_checks();
        inspiration_checks();
        inspired_save_checks();
        jack_of_all_trades_checks();
        std::cout << "Bard tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
