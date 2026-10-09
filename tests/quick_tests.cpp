// Quick combat, as in the original (QUICK-1): the campaign party keeps who the
// computer plays and whether it casts spells between fights and in saves, and
// with magic off the computer casts no spell for a Quick member.
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/combat_demo.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
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

std::unique_ptr<RulesModule> module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

// A level-one Druid with Produce Flame, which the computer lights at once.
Character druid()
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "druid";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Druid";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    d.rolls[4] = {{6, 6, 6, 1}, 3};
    d.training = {{"class:druid", {"nature", "medicine"}}, {"class:druid:primal_order", {"warden"}}};
    d.cantrips = {"produce_flame", "shillelagh"};
    d.spells = SpellChoices{{},
        std::vector<std::string> {"cure_wounds", "entangle", "faerie_fire", "thunderwave"},
        {}, {}};
    return Character(*srd5::character_rules(), d, {});
}

bool casts(const CombatSession &fight, const Command &command)
{
    const auto state = fight.snapshot();
    for (const auto &a : state.combatants)
        if (a.id == state.actor)
            return std::any_of(a.spells.begin(), a.spells.end(), [&](const auto & spell)
        {
            return command.verb == spell || command.verb.starts_with(spell + "_");
        });
    return false;
}

void party_keeps_quick()
{
    CampaignParty party(module());
    const auto id = party.add_pc(druid());
    check(!party.member(id).quick && !party.state().quick_magic, "Nobody starts on Quick");
    party.set_quick(id);
    party.set_quick_magic(true);
    const auto saved = encode_campaign(party, nullptr, "quick");
    check(saved.starts_with("OPENGOLD-CAMPAIGN 25\n"), "Campaign saves are format 25");
    CampaignParty loaded(module());
    loaded.restore(decode_campaign(saved, *srd5::character_rules(), *module(), "quick", nullptr).party);
    check(loaded.member(id).quick && loaded.state().quick_magic,
          "Quick and Quick magic survive a save, for the next fight");
    loaded.take_control();
    check(!loaded.member(id).quick && loaded.state().quick_magic,
          "Taking control returns every member; Quick magic stays as set");
}

void magic_off_casts_nothing()
{
    const auto rules = module();
    const auto profile = rules->character_profile(druid().sheet(), {}).data;
    auto fight = rules->create({{12, 6, std::vector<std::uint8_t>(72)},
        {   {1, "campaign-character", "Druid", 0, {1, 1}, profile},
            {98, "bandit", "Enemy", 1, {6, 1}}
        }},
    5);
    for (unsigned turns = 0; fight->snapshot().actor != 1 && turns < 4; ++turns)
        for (const auto &command : fight->legal_commands())
            if (command.verb == "end" || command.verb == "initiative_keep")
            {
                check(fight->submit(command), "Reach the Druid's turn");
                break;
            }
    check(fight->snapshot().actor == 1, "The Druid acts");
    check(casts(*fight, choose_quick_command(*fight, true)),
          "With Quick magic on, the computer casts for the Druid");
    for (unsigned n = 0; n < 6 && fight->snapshot().actor == 1; ++n)
    {
        const auto command = choose_quick_command(*fight, false);
        check(!casts(*fight, command), "With Quick magic off, the computer casts no spell");
        check(fight->submit(command), "The computer's command is legal");
    }
}
} // namespace

int main()
{
    try
    {
        party_keeps_quick();
        magic_off_casts_nothing();
        std::cout << "Quick combat tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
