#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/combat_demo.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <cstdlib>
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

// A Barbarian (Berserker) from level 5 to 10 (LEVELS-1): more Rages and Rage
// Damage, Fast Movement, Extra Attack, Reckless Attack on the turn's first
// attack, Instinctive Pounce, Brutal Strike and Retaliation.
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

CharacterDraft draft(std::string klass)
{
    CharacterDraft d;
    d.race = "dwarf";
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

// A Barbarian of the level, advanced through the rules' ordinary level-ups.
Character barbarian(unsigned level)
{
    Character hero(*srd5::character_rules(), draft("barbarian"), {});
    VitalState life;
    const auto rules = module();
    while (hero.sheet().level < static_cast<int>(level))
        check(hero.advance(*rules, life), "An ordinary level-up");
    return hero;
}

bool offered(const CombatSession &c, std::string_view verb)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(),
                       [&](const auto &command)
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

// A dummy, and a brute that always hits for 1d4 + 10.
std::unique_ptr<RulesModule> dummy_rules()
{
    std::ifstream in(root / "data/rules/srd-5.2.1/combat.rules");
    return srd5::parse_content(std::string{std::istreambuf_iterator<char>(in), {}} +
                               "\ncreature dummy 10 1000 -10 30 1 1 4 0 0 0 0 0 0 0 0 0 0 1 0\n"
                               "creature brute 10 1000 -10 30 30 1 4 10 0 0 0 0 0 0 0 0 0 1 0\n");
}

bool logged(const CombatSession &c, std::string_view text)
{
    const auto log = c.snapshot().log();
    return std::any_of(log.begin(), log.end(),
                       [&](const auto &line)
                       {
                           return line.find(text) != std::string::npos;
                       });
}

// A fight between the Barbarian and a dummy beside it, at the Barbarian's turn.
std::unique_ptr<CombatSession> duel(const RulesModule &rules, const Character &hero,
                                    std::uint64_t seed = 3, Cell foe = {2, 1},
                                    std::optional<VitalState> state = std::nullopt,
                                    std::string kind = "dummy")
{
    const auto profile = rules.character_profile(hero.sheet(), {}).data;
    auto c =
        rules.create({{8, 3, std::vector<Terrain>(24)},
                      {{1, "campaign-character", "Berserker", Side::party, {1, 1}, profile, state},
                       {98, std::move(kind), "Dummy", Side::opposition, foe}}},
                     seed);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
        check(submit(*c, "initiative_keep") || submit(*c, "end"), "Reach the Barbarian's turn");
    return c;
}

CombatantView unit(const CombatSession &c, EntityId id)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing combatant");
}

void progression_checks()
{
    {
        CampaignParty party(module());
        const auto id = party.add_pc(Character(*srd5::character_rules(), draft("barbarian"), {}));
        party.award_experience(165000, "levels-xp");
        while (party.can_advance(id))
            party.advance(id, party.default_advancement(id));
        check(party.member(id).character.sheet().level == 15,
              "In the campaign a Barbarian reaches the hall's level 15");
    }
    const auto rules = module();
    const auto rages = [&](unsigned level)
    {
        for (const auto &pool : rules->recovery_info(barbarian(level).sheet(), {}).resources)
            if (pool.id == "rage")
                return pool.capacity;
        return 0u;
    };
    check(rages(5) == 3 && rages(6) == 4 && rages(12) == 5, "A fourth Rage at 6, a fifth at 12");
    const auto ten = barbarian(10);
    check(ten.sheet().level == 10, "The rules advance a Barbarian to level 10");
    const auto profile = rules->character_profile(ten.sheet(), {});
    check(profile.movement_feet == 40, "Fast Movement: Speed 40 without Heavy armor");
    const auto fifteen = barbarian(15);
    check(fifteen.sheet().level == 15 && !rules->advancement_options(fifteen.sheet()).level,
          "Level 15 is the Barbarian's cap");
    const auto holds = [&](std::string_view id, int level)
    {
        return std::any_of(fifteen.sheet().grants.begin(), fifteen.sheet().grants.end(),
                           [&](const auto &g)
                           {
                               return g.id == id && g.level == level;
                           });
    };
    check(holds("feature:relentless_rage", 11) && holds("feature:improved_brutal_strike", 13) &&
              holds("feature:intimidating_presence", 14) && holds("feature:persistent_rage", 15),
          "The level 11-15 features arrive at their levels");
    unsigned found = 0;
    for (const auto &pool : rules->recovery_info(fifteen.sheet(), {}).resources)
        if ((pool.id == "relentless_rage" && pool.capacity == 5) ||
            (pool.id == "intimidating_presence" && pool.capacity == 1) ||
            (pool.id == "persistent_rage" && pool.capacity == 1))
            ++found;
    check(found == 3, "Relentless Rage, Intimidating Presence and Persistent Rage are resources");
}

// Relentless Rage: dropped to 0 Hit Points while raging, a Constitution save
// leaves twice the level in Hit Points.
void relentless_checks()
{
    const auto rules = dummy_rules();
    bool tried = false, saved = false;
    for (std::uint64_t seed = 1; seed < 40 && !saved; ++seed)
    {
        auto c = duel(*rules, barbarian(11), seed, {2, 1}, VitalState{1, false, {}, {}}, "brute");
        check(submit(*c, "rage") && submit(*c, "end"), "Rage, then wait");
        // Retaliation is declined.
        for (unsigned steps = 0; steps < 6; ++steps)
        {
            const auto now = c->snapshot();
            if (now.outcome != Outcome::ongoing || (now.actor != 98 && !now.reaction_pending))
                break;
            check(submit(*c, "decline") || c->submit(choose_demo_command(*c)), "The brute acts");
        }
        if (!logged(*c, "Berserker tries Relentless Rage."))
            continue;
        tried = true;
        saved = logged(*c, "Berserker fights on with 22 Hit Points (Relentless Rage).") &&
                unit(*c, 1).hit_points == 22;
        const auto checkpoint = c->save();
        check(rules->restore(checkpoint)->save() == checkpoint, "A checkpoint keeps the tries");
    }
    check(tried && saved, "Some seed's brute drops the Barbarian and the save passes");
}

// Extra Attack, and Reckless Attack only with the turn's first attack roll.
void attack_checks()
{
    const auto rules = dummy_rules();
    auto c = duel(*rules, barbarian(5));
    check(offered(*c, "reckless") && submit(*c, "reckless", 98) && logged(*c, "attacks recklessly"),
          "The first attack may be reckless");
    check(offered(*c, "melee") && !offered(*c, "reckless"),
          "Extra Attack's second attack, which cannot start Reckless Attack");
}

// Instinctive Pounce: half Speed of movement with the Rage, gone once
// anything else is done.
void pounce_checks()
{
    const auto rules = dummy_rules();
    auto c = duel(*rules, barbarian(7), 3, {6, 1});
    const auto before = unit(*c, 1).movement_feet;
    check(submit(*c, "rage") && logged(*c, "may move 20 feet as part of entering the Rage") &&
              unit(*c, 1).movement_feet == before + 20,
          "Rage adds 20 feet of Instinctive Pounce");
    const auto saved = c->save();
    check(rules->restore(saved)->save() == saved, "A checkpoint keeps the pounce");
    check(submit(*c, "dodge") && unit(*c, 1).movement_feet == before,
          "Another action gives up the pounce's movement");
}

// Brutal Strike after Reckless Attack: a Hamstring Blow, or a Forceful Blow
// the Barbarian may follow.
void brutal_checks()
{
    const auto rules = dummy_rules();
    bool hamstrung = false, forceful = false;
    for (std::uint64_t seed = 1; seed < 40 && !(hamstrung && forceful); ++seed)
        for (const bool push : {false, true})
        {
            auto c = duel(*rules, barbarian(9), seed);
            check(!offered(*c, "brutal_hamstring"), "Brutal Strike needs Reckless Attack first");
            check(submit(*c, "reckless", 98), "A reckless first attack");
            check(offered(*c, "brutal_forceful") && offered(*c, "brutal_hamstring"),
                  "Then Brutal Strike is offered");
            check(submit(*c, push ? "brutal_forceful" : "brutal_hamstring", 98),
                  "The Brutal Strike attack");
            if (!logged(*c, "Brutal Strike adds "))
                continue;
            if (!push)
            {
                hamstrung = logged(*c, "Dummy's Speed drops by 15 feet (Hamstring Blow).");
                continue;
            }
            const auto free = c->snapshot().free_movement;
            forceful = logged(*c, "Dummy is pushed 15 feet.") && free && free->remaining_feet == 20;
            const auto saved = c->save();
            check(rules->restore(saved)->save() == saved, "A checkpoint keeps the follow-up move");
            for (const auto &command : c->legal_commands())
                if (command.verb == "move")
                    check(command.destination.x > 1,
                          "The follow-up move only closes on the target");
        }
    check(hamstrung && forceful, "Both blows land on some seed");
}

// Improved Brutal Strike: Staggering Blow and Sundering Blow at level 13.
void improved_brutal_checks()
{
    const auto rules = dummy_rules();
    auto early = duel(*rules, barbarian(12));
    check(submit(*early, "reckless", 98) && offered(*early, "brutal_hamstring") &&
              !offered(*early, "brutal_staggering"),
          "Not before level 13");
    bool staggered = false;
    for (std::uint64_t seed = 1; seed < 40 && !staggered; ++seed)
    {
        auto c = duel(*rules, barbarian(13), seed);
        check(submit(*c, "reckless", 98) && offered(*c, "brutal_sundering") &&
                  submit(*c, "brutal_staggering", 98),
              "A Staggering Blow");
        if (!logged(*c, "Dummy staggers (Staggering Blow)."))
            continue;
        staggered = true;
        const auto conditions = unit(*c, 98).conditions;
        check(std::any_of(conditions.begin(), conditions.end(),
                          [](const auto &m)
                          {
                              return m.source == "Staggered (Staggering Blow)";
                          }),
              "The dummy shows the blow");
        const auto checkpoint = c->save();
        check(rules->restore(checkpoint)->save() == checkpoint, "A checkpoint keeps the blow");
    }
    check(staggered, "A Staggering Blow lands on some seed");

    // Sundering Blow: +5 to an ally's next attack roll, not the Barbarian's own.
    const auto ally = rules->character_profile(barbarian(5).sheet(), {}).data;
    const auto berserker = rules->character_profile(barbarian(13).sheet(), {}).data;
    for (std::uint64_t seed = 1; seed < 80; ++seed)
    {
        auto c =
            rules->create({{8, 3, std::vector<Terrain>(24)},
                           {{1, "campaign-character", "Berserker", Side::party, {1, 1}, berserker},
                            {2, "campaign-character", "Ally", Side::party, {3, 1}, ally},
                            {98, "dummy", "Dummy", Side::opposition, {2, 1}}}},
                          seed);
        for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 6; ++turns)
            check(submit(*c, "initiative_keep") || submit(*c, "end"), "Reach the Barbarian's turn");
        check(submit(*c, "reckless", 98) && submit(*c, "brutal_sundering", 98), "A Sundering Blow");
        if (!logged(*c, "Dummy is sundered (Sundering Blow)."))
            continue;
        if (offered(*c, "melee"))
            check(submit(*c, "melee", 98), "The Barbarian's second attack");
        check(!logged(*c, "Sundering Blow adds 5"), "Not to the Barbarian's own attack");
        check(submit(*c, "end"), "The Barbarian ends its turn");
        for (unsigned steps = 0; steps < 6 && c->snapshot().actor != 2; ++steps)
            check(c->submit(choose_demo_command(*c)), "The dummy acts");
        if (c->snapshot().actor != 2)
            continue;
        check(submit(*c, "melee", 98) && logged(*c, "Sundering Blow adds 5 to the attack roll."),
              "The ally's attack roll gains +5");
        return;
    }
    throw std::runtime_error("No seed's Sundering Blow reaches the ally's turn");
}

// Intimidating Presence: enemies within 30 feet save or are Frightened, which
// keeps them from coming closer.
void intimidating_checks()
{
    const auto rules = dummy_rules();
    for (std::uint64_t seed = 1; seed < 40; ++seed)
    {
        auto c = duel(*rules, barbarian(14), seed, {4, 1});
        check(submit(*c, "intimidating_presence") &&
                  logged(*c, "Berserker uses Intimidating Presence."),
              "Intimidating Presence");
        if (!logged(*c, "Dummy is Frightened."))
            continue;
        check(!offered(*c, "intimidating_presence"), "One use, and the Bonus Action is spent");
        const auto checkpoint = c->save();
        check(rules->restore(checkpoint)->save() == checkpoint, "A checkpoint keeps the fear");
        const auto reach = c->movement_reach(98);
        check(!reach.empty(), "The frightened dummy can still move");
        for (const auto cell : reach)
            check(std::max(std::abs(cell.x - 1), std::abs(cell.y - 1)) >= 3,
                  "The frightened dummy cannot come closer");
        return;
    }
    throw std::runtime_error("No seed's dummy fails the Wisdom save");
}

// Persistent Rage: when Initiative is rolled, every Rage use comes back, once
// per Long Rest; the Rage needs no extending.
void persistent_checks()
{
    const auto rules = dummy_rules();
    auto first = duel(*rules, barbarian(15));
    check(submit(*first, "rage") && !offered(*first, "extend_rage"), "Rage");
    const auto spent = unit(*first, 1).persistent;
    const auto profile = rules->character_profile(barbarian(15).sheet(), {}).data;
    auto c =
        rules->create({{8, 3, std::vector<Terrain>(24)},
                       {{1, "campaign-character", "Berserker", Side::party, {1, 1}, profile, spent},
                        {98, "dummy", "Dummy", Side::opposition, {2, 1}}}},
                      3);
    check(submit(*c, "persistent_rage") &&
              logged(*c, "Berserker regains every Rage use (Persistent Rage)."),
          "Persistent Rage is offered at Initiative");
    bool full = false;
    for (const auto &pool : unit(*c, 1).resources)
        if (pool.id == "rage")
            full = pool.remaining == pool.capacity;
    check(full, "Every Rage use is back");
}

// Retaliation: damaged by the creature beside it, a level-10 Berserker may
// strike back with its Reaction.
void retaliation_checks()
{
    const auto rules = dummy_rules();
    for (std::uint64_t seed = 1; seed < 80; ++seed)
    {
        auto c = duel(*rules, barbarian(10), seed);
        check(submit(*c, "end"), "The Barbarian waits");
        for (unsigned steps = 0;
             steps < 6 && c->snapshot().actor == 98 && !offered(*c, "retaliate"); ++steps)
            check(c->submit(choose_demo_command(*c)), "The dummy acts");
        if (!offered(*c, "retaliate"))
            continue;
        check(submit(*c, "retaliate") && logged(*c, "Berserker retaliates.") &&
                  logged(*c, "Berserker -> Dummy: d20") && !unit(*c, 1).reaction,
              "Retaliation's melee attack spends the Reaction");
        return;
    }
    throw std::runtime_error("No seed's dummy hits the Barbarian");
}
} // namespace

int main()
{
    try
    {
        progression_checks();
        attack_checks();
        pounce_checks();
        brutal_checks();
        retaliation_checks();
        relentless_checks();
        improved_brutal_checks();
        intimidating_checks();
        persistent_checks();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Barbarian level tests passed\n";
}
