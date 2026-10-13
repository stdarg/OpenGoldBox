#include "opengold/campaign_party.h"
#include "opengold/character.h"
#include "opengold/srd5.h"
#include "status_effects.h"
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
} // namespace

int main()
{
    try
    {
        gnome_checks();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Species tests passed\n";
}
