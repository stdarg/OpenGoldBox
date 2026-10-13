#include "opengold/campaign_party.h"
#include "opengold/character.h"
#include "opengold/srd5.h"
#include "status_effects.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;
namespace fx = opengold::srd5::detail;

// Species traits in play (SRD 5.2.1 pp. 83-86).
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

bool submit(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &command : c.legal_commands())
        if (command.verb == verb && (!target || command.target == target))
            return c.submit(command);
    return false;
}

// A level-one Fighter of the species, every roll the same.
Character fighter(std::string race)
{
    CharacterDraft d;
    d.race = std::move(race);
    d.gender = "female";
    d.character_class = "fighter";
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Target";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.training = {{"class:fighter", {"acrobatics", "history"}},
        {"class:fighter:fighting_style", {"defense"}}
    };
    return Character(*srd5::character_rules(), d, {});
}

// An enemy Cleric who commands the target to halt; returns the natural roll
// of its Wisdom save.
int wisdom_save(const RulesModule &rules, const Character &target, std::uint64_t seed)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "cleric";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Foe";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.cantrips = {"sacred_flame"};
    d.spells = SpellChoices{{}, std::vector<std::string> {"command"}, {}, {}};
    d.training = {{"class:cleric", {"medicine", "persuasion"}},
        {"class:cleric:divine_order", {"protector"}}
    };
    const Character foe(*srd5::character_rules(), d, {});
    auto c = rules.create({{12, 6, std::vector<Terrain>(72)},
        {   {1, "campaign-character", "Target", Side::party, {1, 1},
                rules.character_profile(target.sheet(), {}).data},
            {98, "campaign-character", "Foe", Side::opposition, {6, 1},
             rules.character_profile(foe.sheet(), {}).data}
        }},
    seed);
    for (unsigned turns = 0; c->snapshot().actor != 98 && turns < 4; ++turns)
        check(submit(*c, "end"), "Reach the Cleric's turn");
    check(submit(*c, "command_halt", 1), "The Cleric commands the target to halt");
    for (const auto &line : c->snapshot().log())
        if (const auto at = line.find("Target Wisdom save: d20 "); at != std::string::npos)
            return std::stoi(line.substr(at + 24));
    throw std::runtime_error("No Wisdom save in the log");
}

// Gnomish Cunning: Advantage on Intelligence, Wisdom and Charisma saves. With
// the same rolls, Advantage keeps the first die or a higher second one.
void gnome_checks()
{
    const auto rules = module();
    const auto gnome = fighter("gnome"), human = fighter("human");
    bool higher = false;
    for (std::uint64_t seed = 1; seed <= 24; ++seed)
    {
        const int plain = wisdom_save(*rules, human, seed),
                  cunning = wisdom_save(*rules, gnome, seed);
        check(cunning >= plain, "Gnomish Cunning never lowers a Wisdom save");
        higher |= cunning > plain;
    }
    check(higher, "Gnomish Cunning gives Advantage on Wisdom saves");

    // The repeated saves at the end of a turn, such as against Hold Person.
    fx::EffectState effects;
    fx::EffectSubject subject{1, effects, {}, false, false, false, true};
    check(fx::mental_save_modifiers(fx::Ability::wisdom, subject).advantage &&
          fx::mental_save_modifiers(fx::Ability::charisma, subject).advantage &&
          !fx::mental_save_modifiers(fx::Ability::constitution, subject).advantage,
          "Repeated mental saves have Advantage too; others do not");
}
CombatantView unit(const CombatSession &c, EntityId id)
{
    for (const auto &a : c.snapshot().combatants)
        if (a.id == id)
            return a;
    throw std::runtime_error("Missing combatant");
}

bool logged(const CombatSession &c, std::string_view text)
{
    for (const auto &line : c.snapshot().log())
        if (line.find(text) != std::string::npos)
            return true;
    return false;
}

std::size_t count_logged(const CombatSession &c, std::string_view text)
{
    std::size_t count = 0;
    for (const auto &line : c.snapshot().log())
        count += line.find(text) != std::string::npos;
    return count;
}

// The bandit attacks the Orc until a blow lands; false if none does.
bool hit_orc(CombatSession &c)
{
    for (unsigned attempt = 0; attempt < 12; ++attempt)
    {
        const auto can_attack = [&]
        {
            const auto commands = c.legal_commands();
            return c.snapshot().actor == 98 &&
                   std::any_of(commands.begin(), commands.end(), [](const auto & command)
            {
                return command.verb == "melee" && command.target == 1;
            });
        };
        for (unsigned turns = 0; !can_attack() && turns < 6; ++turns)
            check(submit(c, "end"), "Reach the bandit's next attack");
        const int before = unit(c, 1).hit_points;
        const auto endured = count_logged(c, "Relentless Endurance");
        if (!submit(c, "melee", 1))
            throw std::runtime_error("The bandit cannot attack the Orc (HP " +
                                     std::to_string(before) + ", actor " +
                                     std::to_string(c.snapshot().actor) + ")");
        if (unit(c, 1).hit_points < before || count_logged(c, "Relentless Endurance") > endured)
            return true;
        check(submit(c, "end"), "The bandit missed; end its turn");
    }
    return false;
}

// Relentless Endurance: an Orc dropped to 0 Hit Points drops to 1 instead,
// once per Long Rest.
void orc_checks()
{
    const auto rules = module();
    CampaignParty party(module());
    const auto id = party.add_pc(fighter("orc"));
    auto state = party.checkpoint();
    state.roster.front().vitals.hit_points = 1;
    party.restore(std::move(state));
    const auto encounter = [&]
    {
        Encounter e{{12, 6, std::vector<Terrain>(72)}, party.participants()};
        e.participants.front().cell = {1, 1};
        e.participants.push_back({98, "bandit", "Bandit", Side::opposition, {2, 1}});
        return e;
    };
    auto c = rules->create(encounter(), 3);
    check(hit_orc(*c) && unit(*c, 1).hit_points == 1 && logged(*c, "Relentless Endurance"),
          "The first blow to drop the Orc leaves it at 1 Hit Point");
    check(rules->restore(c->save())->save() == c->save(), "The spent trait survives a checkpoint");
    // The fight so far comes back to the party: the Orc stands at 1 Hit Point.
    party.begin_combat();
    party.apply_combat(c->snapshot());
    party.end_combat();
    check(hit_orc(*c) && unit(*c, 1).hit_points == 0, "The next blow drops it: once per Long Rest");

    // Before a Long Rest the trait stays spent; after one it is back.
    auto before_rest = rules->create(encounter(), 3);
    check(hit_orc(*before_rest) && unit(*before_rest, 1).hit_points == 0,
          "Without a Long Rest the trait does not return");
    party.advance_time(std::chrono::hours{24});
    check(bool(party.rest(RestKind::long_rest)), "The Orc takes a Long Rest");
    state = party.checkpoint();
    state.roster.front().vitals.hit_points = 1;
    party.restore(std::move(state));
    auto again = rules->create(encounter(), 3);
    check(hit_orc(*again) && unit(*again, 1).hit_points == 1,
          "After a Long Rest the Orc endures again");
    (void)id;
}

} // namespace

int main()
{
    try
    {
        gnome_checks();
        orc_checks();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Species tests passed\n";
}
