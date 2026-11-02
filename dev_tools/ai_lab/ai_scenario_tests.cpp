// A scenario pack keeps a fight as the game built it, and replaying one is
// repeatable from its seed, whichever levels play it (dev_tools/docs/ai-lab.md).
#include "ai_scenario.h"
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/combat_demo.h"
#include "opengold/srd5.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

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

Character fighter()
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = "druid";
    d.background = "sage";
    d.alignment = "neutral_good";
    d.name = "Tester";
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

CampaignEncounter open_field_fight()
{
    CampaignEncounter fight;
    fight.field.geometry = {12, 8, std::vector<Terrain>(96)};
    fight.field.geometry.terrain[20] = Terrain::obstacle;
    fight.field.geometry.plants.assign(96, false);
    fight.field.geometry.plants[3] = true;
    fight.field.walled[1][2][3] = true;
    fight.facing = por::MapDirection::east;
    fight.surprise = Surprise::monsters;
    fight.distance = 1;
    fight.morale = 80;
    fight.daylight = true;
    Participant bandit{1000, "bandit", "BANDIT 1", Side::opposition, {6, 3}};
    bandit.melee_weapon = "longsword";
    bandit.ranged_weapon = "longbow";
    bandit.morale = 40;
    bandit.intelligence = 7;
    bandit.footprint = {2, 1};
    bandit.inventory.push_back({9, "potion_of_healing", 2, 1});
    fight.enemies.push_back(bandit);
    fight.allies.push_back({2000, "bandit", "HELPER", Side::party, {1, 1}});
    return fight;
}

void encounter_round_trip()
{
    const auto fight = open_field_fight();
    const auto decoded = decode_ai_encounter(encode_ai_encounter(fight));
    check(decoded.field.geometry.width == 12 && decoded.field.geometry.height == 8 &&
              decoded.field.geometry.terrain == fight.field.geometry.terrain &&
              decoded.field.geometry.plants == fight.field.geometry.plants &&
              decoded.field.walled == fight.field.walled,
          "The field survives");
    check(decoded.facing == fight.facing && decoded.surprise == fight.surprise &&
              decoded.distance == 1 && decoded.morale == 80 && decoded.daylight,
          "The encounter's settings survive");
    check(decoded.enemies.size() == 1 && decoded.allies.size() == 1, "Both sides survive");
    const auto &bandit = decoded.enemies.front();
    check(bandit.id == 1000 && bandit.definition == "bandit" && bandit.name == "BANDIT 1" &&
              bandit.melee_weapon == "longsword" && bandit.ranged_weapon == "longbow" &&
              bandit.morale == 40 && bandit.intelligence == 7 && bandit.footprint.width == 2,
          "A creature's gear and numbers survive");
    check(bandit.inventory.size() == 1 && bandit.inventory[0].definition == "potion_of_healing" &&
              bandit.inventory[0].quantity == 2,
          "Its items survive");
    auto wounded = fight;
    wounded.enemies[0].character_profile = "a character";
    bool refused = false;
    try
    {
        static_cast<void>(encode_ai_encounter(wounded));
    }
    catch (const std::runtime_error &)
    {
        refused = true;
    }
    check(refused, "A fight it cannot keep faithfully is refused");
}

// DIFF-2: replaying a level sizes the band and strengthens the monsters as the game
// does; those on the party's side stay as they are.
void level_sets_the_fight()
{
    auto encounter = open_field_fight();
    encounter.allies = {encounter.enemies.front()};
    encounter.allies.front().id = 500;
    encounter.band.clear();
    for (unsigned n = 0; n < 30; ++n)
    {
        auto creature = encounter.enemies.front();
        creature.name = "Goblin";
        encounter.band.push_back({creature, 7, 10});
    }
    encounter.morale = 60;
    const auto text = encode_ai_encounter(encounter);
    const auto decoded = decode_ai_encounter(text);
    check(decoded.band.size() == 30 && decoded.band.front().xp == 10 &&
              decoded.band.front().record == 7 && encode_ai_encounter(decoded) == text,
          "A pack keeps the whole band");
    const std::vector<unsigned> party(6, 3);
    const auto normal = encounter_at_difficulty(decoded, party, Difficulty::normal);
    const auto hard = encounter_at_difficulty(decoded, party, Difficulty::hard);
    const auto infernal = encounter_at_difficulty(decoded, party, Difficulty::infernal);
    check(normal.enemies.size() == 6 && hard.enemies.size() == 9 && infernal.enemies.size() > 9,
          "The level sizes the band");
    check(normal.enemies.front().hit_points_percent == 100 &&
              normal.enemies.front().attack_bonus == 0 && normal.morale == 60,
          "Normal leaves the monsters as they are");
    check(infernal.enemies.front().hit_points_percent == monster_strengths[4].hit_points_percent &&
              infernal.enemies.front().attack_bonus == monster_strengths[4].attack_bonus &&
              infernal.morale == 90 && infernal.allies.front().hit_points_percent == 100,
          "Infernal strengthens the enemies alone");
    const auto easy = encounter_at_difficulty(decoded, party, Difficulty::easy);
    check(easy.spares_the_party && easy.morale == 30 && !normal.spares_the_party,
          "Easy spares the party and breaks monsters sooner");
    auto never_breaks = encounter;
    never_breaks.morale = 100;
    check(encounter_at_difficulty(never_breaks, party, Difficulty::easy).morale == 100,
          "Morale that never breaks stays so");
}

void replay_is_repeatable(const std::filesystem::path &pack)
{
    auto party = std::make_shared<CampaignParty>(module());
    party->add_pc(fighter());
    write_ai_scenario(pack, "0001", *party, open_field_fight());
    const auto scenarios = load_ai_scenarios(pack);
    check(scenarios.size() == 1 && scenarios[0].name == "0001", "The pack holds the fight");
    const auto saved = decode_campaign(scenarios[0].party_save, *srd5::character_rules(), *module(),
                                       ai_scenario_assets, nullptr)
                           .party;
    const auto play = [&](Difficulty party_level, std::uint64_t seed)
    {
        return play_ai_fight(module, saved, scenarios[0].encounter, party_level, Difficulty::normal,
                             seed);
    };
    const auto first = play(Difficulty::infernal, 5), again = play(Difficulty::infernal, 5);
    check(first.commands > 0 && !first.stalled, "The fight is played to its end");
    check(first.commands == again.commands && first.won == again.won &&
              first.hit_points_left == again.hit_points_left,
          "The same seed plays the same fight");
    bool dice_differ = false;
    for (std::uint64_t seed = 6; seed < 30 && !dice_differ; ++seed)
    {
        const auto other = play(Difficulty::infernal, seed);
        dice_differ =
            other.commands != first.commands || other.hit_points_left != first.hit_points_left;
    }
    check(dice_differ, "Other seeds roll other dice");
    check(party->state().slots == saved.slots, "Replaying does not change the party");
}
} // namespace

int main(int argc, char **argv)
{
    try
    {
        const std::filesystem::path pack =
            argc > 1 ? argv[1] : std::filesystem::temp_directory_path() / "opengold-ai-lab-check";
        std::filesystem::remove_all(pack);
        encounter_round_trip();
        level_sets_the_fight();
        replay_is_repeatable(pack);
        std::cout << "AI scenario tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
