// Quick combat, as in the original (QUICK-1): the campaign party keeps who the
// computer plays and whether it casts spells in saves, a fight's end returns
// every member to the player unless Quick is kept, and with magic off the
// computer casts no spell for a Quick member.
#include "opengold/campaign_encounter.h"
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
    {
        throw std::runtime_error(message);
    }
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
    {
        r = {{6, 5, 4, 1}, 3};
    }
    d.rolls[4] = {{6, 6, 6, 1}, 3};
    d.training = {{"class:druid", {"nature", "medicine"}},
                  {"class:druid:primal_order", {"warden"}}};
    d.cantrips = {"produce_flame", "shillelagh"};
    d.spells = SpellChoices{
        {},
        std::vector<std::string>{"cure_wounds", "entangle", "faerie_fire", "thunderwave"},
        {},
        {}};
    return Character(*srd5::character_rules(), d, {});
}

bool casts(const CombatSession &fight, const Command &command)
{
    const auto state = fight.snapshot();
    for (const auto &a : state.combatants)
    {
        if (a.id == state.actor)
        {
            return std::any_of(a.spells.begin(), a.spells.end(),
                               [&](const auto &spell)
                               {
                                   return command.verb == spell ||
                                          command.verb.starts_with(spell + "_");
                               });
        }
    }
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
    check(saved.starts_with("OPENGOLD-CAMPAIGN 46\n"), "Campaign saves are format 33");
    CampaignParty loaded(module());
    loaded.restore(
        decode_campaign(saved, *srd5::character_rules(), *module(), "quick", nullptr).party);
    check(loaded.member(id).quick && loaded.state().quick_magic,
          "Quick and Quick magic survive a save, for the next fight");
    loaded.take_control();
    check(!loaded.member(id).quick && loaded.state().quick_magic,
          "Taking control returns every member; Quick magic stays as set");
}

// When a fight ends every member is back under the player's control, unless
// the Options screen keeps Quick for the next fight.
void fight_ends_quick()
{
    for (const bool keep : {false, true})
    {
        auto party = std::make_shared<CampaignParty>(module());
        const auto id = party->add_pc(druid());
        party->set_quick(id);
        party->set_quick_magic(true);
        CombatDemo fight(module());
        fight.campaign_party(party);
        fight.keep_quick(keep);
        CampaignEncounter encounter;
        encounter.field.geometry = {12, 9, std::vector<Terrain>(108)};
        encounter.enemies = {{1000, "bandit", "Bandit", Side::opposition, {9, 4}}};
        fight.encounter(std::move(encounter), 42);
        for (unsigned n = 0; n < 2000 && fight.combat().snapshot().outcome == Outcome::ongoing; ++n)
        {
            check(party->member(id).quick, "The member stays on Quick while the fight lasts");
            check(fight.submit(choose_demo_command(fight.combat(), Searching::party)),
                  "The computer's command is legal");
        }
        check(fight.combat().snapshot().outcome != Outcome::ongoing, "The fight ends");
        check(party->member(id).quick == keep,
              keep ? "Keep Quick leaves the member on Quick for the next fight"
                   : "The fight's end returns the member to the player");
        check(party->state().quick_magic, "Quick magic stays as set");
    }
}

int unit_hit_points(const rules::Snapshot &state, EntityId id)
{
    const auto found = std::find_if(state.combatants.begin(), state.combatants.end(),
                                    [&](const auto &unit)
                                    {
                                        return unit.id == id;
                                    });
    return found->hit_points;
}

// DIFF-2: on Easy a won fight leaves no member down; otherwise the member stays so.
void easy_spares_the_winners()
{
    const auto play = [](bool spared)
    {
        const auto rules = module();
        const auto profile = rules->character_profile(druid().sheet(), {}).data;
        rules::VitalState down;
        down.hit_points = 0;
        rules::VitalState frail;
        frail.hit_points = 1;
        Encounter encounter{
            {12, 6, std::vector<Terrain>(72)},
            {{1, "campaign-character", "Standing", Side::party, {1, 1}, profile},
             {2, "campaign-character", "Fallen", Side::party, {1, 3}, profile, down},
             {98, "bandit", "Enemy", Side::opposition, {2, 1}, {}, frail}}};
        encounter.spares_the_party = spared;
        auto fight = rules->create(std::move(encounter), 5);
        for (unsigned n = 0; n < 400 && fight->snapshot().outcome == rules::Outcome::ongoing; ++n)
        {
            check(fight->submit(choose_demo_command(*fight)), "The command is legal");
        }
        const auto state = fight->snapshot();
        check(state.outcome == rules::Outcome::victory, "The party wins");
        const auto log = state.log();
        return std::pair{unit_hit_points(state, 2),
                         std::find(log.begin(), log.end(), "Fallen gets back up.") != log.end()};
    };
    const auto [spared_hp, spared_said] = play(true);
    check(spared_hp == 1 && spared_said, "On Easy a downed member gets back up after a win");
    const auto [normal_hp, normal_said] = play(false);
    check(normal_hp == 0 && !normal_said, "Otherwise a downed member stays down");
}

// The Slums trolls' room: the ogre stands on the one square beside the last
// member standing that the troll could reach, with members down around them.
// A way around the creatures longer than a turn's move takes the troll to her
// other side; with none, it does not charge on the spot.
void blocked_monster_goes_around()
{
    const auto play = [](bool way_around)
    {
        const auto rules = module();
        const auto profile = rules->character_profile(druid().sheet(), {}).data;
        std::vector<Terrain> terrain(56, Terrain::obstacle);
        std::vector<std::pair<int, int>> open{{1, 1}, {2, 1}, {3, 1}, {2, 2},
                                              {3, 2}, {4, 2}, {5, 2}, {4, 3}};
        if (way_around)
        {
            // Eleven squares, more than a charging troll covers in a turn.
            open.insert(
                open.end(),
                {{1, 2}, {1, 3}, {1, 4}, {1, 5}, {1, 6}, {2, 6}, {3, 6}, {4, 6}, {4, 5}, {4, 4}});
        }
        for (const auto &[x, y] : open)
        {
            terrain[y * 7 + x] = Terrain::open;
        }
        rules::VitalState down;
        down.hit_points = 0;
        Encounter encounter{
            {7, 8, terrain},
            {{1, "campaign-character", "Last", Side::party, {4, 2}, profile},
             {2, "campaign-character", "Down 1", Side::party, {1, 1}, profile, down},
             {3, "campaign-character", "Down 2", Side::party, {2, 2}, profile, down},
             {4, "campaign-character", "Down 3", Side::party, {3, 2}, profile, down},
             {98, "troll", "Troll", Side::opposition, {2, 1}},
             {99, "ogre", "Ogre", Side::opposition, {3, 1}}}};
        auto fight = rules->create(std::move(encounter), 5);
        bool struck = false;
        for (unsigned n = 0; n < 120 && !struck && fight->snapshot().outcome == Outcome::ongoing;
             ++n)
        {
            const auto state = fight->snapshot();
            auto command = choose_demo_command(*fight);
            if (state.actor != 98 && state.actor != 99)
            {
                const auto legal = fight->legal_commands();
                command = *std::find_if(legal.begin(), legal.end(),
                                        [](const Command &c)
                                        {
                                            return c.verb == "end";
                                        });
            }
            struck = state.actor == 98 && command.target == 1 && command.verb != "move";
            check(fight->submit(command), "The command is legal");
        }
        const auto log = fight->snapshot().log();
        return std::pair{struck, std::find(log.begin(), log.end(), "Troll charges.") != log.end()};
    };
    check(play(true).first, "A troll goes around the creatures to strike the last one standing");
    const auto [struck, charged] = play(false);
    check(!struck && !charged, "With no way around, the troll does not charge on the spot");
}

void magic_off_casts_nothing()
{
    const auto rules = module();
    const auto profile = rules->character_profile(druid().sheet(), {}).data;
    auto fight = rules->create({{12, 6, std::vector<Terrain>(72)},
                                {{1, "campaign-character", "Druid", Side::party, {1, 1}, profile},
                                 {98, "bandit", "Enemy", Side::opposition, {6, 1}}}},
                               5);
    for (unsigned turns = 0; fight->snapshot().actor != 1 && turns < 4; ++turns)
    {
        for (const auto &command : fight->legal_commands())
        {
            if (command.verb == "end" || command.verb == "initiative_keep")
            {
                check(fight->submit(command), "Reach the Druid's turn");
                break;
            }
        }
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

// The difficulty setting makes the party's enemies harder, not its own Quick
// members worse: over a whole fight they play as the highest level does.
void quick_plays_at_the_highest_level()
{
    const auto rules = module();
    const auto profile = rules->character_profile(druid().sheet(), {}).data;
    auto fight = rules->create({{12, 6, std::vector<Terrain>(72)},
                                {{1, "campaign-character", "Druid", Side::party, {1, 1}, profile},
                                 {98, "bandit", "Enemy", Side::opposition, {6, 1}},
                                 {99, "bandit", "Other", Side::opposition, {6, 3}}}},
                               5);
    bool differs_from_easy = false;
    for (unsigned n = 0; n < 200 && fight->snapshot().outcome == Outcome::ongoing; ++n)
    {
        const auto highest = choose_demo_command(*fight, Searching::by_sight, Difficulty::infernal);
        if (fight->snapshot().actor == 1)
        {
            const auto quick = choose_quick_command(*fight, true);
            check(quick.verb == highest.verb && quick.target == highest.target &&
                      quick.destination == highest.destination,
                  "A Quick member plays as the highest difficulty does");
            const auto easy = choose_demo_command(*fight, Searching::by_sight, Difficulty::easy);
            differs_from_easy = differs_from_easy || easy.verb != highest.verb ||
                                easy.target != highest.target ||
                                easy.destination != highest.destination;
        }
        check(fight->submit(highest), "The computer's command is legal");
    }
    check(differs_from_easy, "The highest level plays differently from Easy");
}
} // namespace

int main()
{
    try
    {
        party_keeps_quick();
        fight_ends_quick();
        easy_spares_the_winners();
        blocked_monster_goes_around();
        magic_off_casts_nothing();
        quick_plays_at_the_highest_level();
        std::cout << "Quick combat tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
