#include "opengold/encounter_budget.h"
#include "test_support.h"
#include <array>
#include <iostream>
#include <stdexcept>

using namespace opengold;

namespace
{
using opengold::test::check;
using opengold::test::throws;

// SRD 5.2.1 p. 202 Moderate budgets, per character, summed over the party.
void budgets()
{
    const std::array<unsigned, 6> six_level_two{2, 2, 2, 2, 2, 2};
    check(encounter_xp_budget(six_level_two, 100) == 900,
          "Challenge 100 is the SRD's Moderate budget");
    check(encounter_xp_budget(six_level_two, 50) == 450 &&
              encounter_xp_budget(six_level_two, 200) == 1800 &&
              encounter_xp_budget(six_level_two, 0) == 0,
          "The challenge scales the Moderate budget as a percentage");
    const std::array<unsigned, 2> mixed{1, 2};
    check(encounter_xp_budget(mixed, 100) == 75 + 150,
          "A mixed-level party sums each character's own budget");
    const std::array<unsigned, 1> twentieth{20};
    check(encounter_xp_budget(twentieth, 100) == 13200, "The table runs to level 20");
    check(throws<std::invalid_argument>(
              [&]
              {
                  (void)encounter_xp_budget(six_level_two, 201);
              }),
          "Challenge is at most 200");
    const std::array<unsigned, 1> invalid{21};
    check(throws<std::invalid_argument>(
              [&]
              {
                  (void)encounter_xp_budget(invalid, 100);
              }),
          "Levels outside the table are rejected");
}

void fitting()
{
    const std::array<EncounterGroup, 2> goblins{{{50, 4}, {50, 12}}};
    check((fit_encounter_to_budget(goblins, 800, 16) == std::vector<unsigned>{4, 12}),
          "An encounter within both limits is unchanged");
    check((fit_encounter_to_budget(goblins, 2000, 40) == std::vector<unsigned>{4, 12}),
          "Generous limits never add monsters");
    check((fit_encounter_to_budget(goblins, 400, 16) == std::vector<unsigned>{2, 6}),
          "Every group shrinks by the same factor to fit the XP budget");
    check((fit_encounter_to_budget(goblins, 800, 8) == std::vector<unsigned>{2, 6}),
          "Every group shrinks by the same factor to fit the creature limit");
    check((fit_encounter_to_budget(goblins, 600, 4) == std::vector<unsigned>{1, 3}),
          "The tighter limit decides the factor");
    const std::array<EncounterGroup, 2> bugbear_and_kobolds{{{200, 1}, {25, 10}}};
    check((fit_encounter_to_budget(bugbear_and_kobolds, 100, 11) == std::vector<unsigned>{1, 2}),
          "Each group keeps at least one monster");
}
} // namespace

// DIFF-2: Normal sizes as ever; the levels around it change the budget and the
// creatures allowed, never many more than Normal's, and above Normal a level
// comes on as the party grows.
void difficulty_sizes_fights()
{
    const std::vector<EncounterGroup> band{{50, 30}};
    const std::vector<unsigned> party(6, 3);
    const auto at = [&](Difficulty difficulty)
    {
        return fit_encounter_to_difficulty(band, party, 100, difficulty).front();
    };
    check(at(Difficulty::normal) ==
              fit_encounter_to_budget(band, encounter_xp_budget(party, 100), 6).front(),
          "Normal sizes a fight exactly as before");
    check(at(Difficulty::easy) == 5, "Easy meets one creature fewer than the party");
    check(at(Difficulty::hard) == 9, "Hard meets one and a half per member");
    check(at(Difficulty::nightmare) == 9 && at(Difficulty::infernal) == 10,
          "Above Hard the creatures Normal fields cap the two per member");
    // A large band of which Normal fields two: Infernal's budget would field four.
    const std::vector<EncounterGroup> orcs{{100, 12}};
    check(fit_encounter_to_difficulty(orcs, party, 15, Difficulty::normal).front() == 2 &&
              fit_encounter_to_difficulty(orcs, party, 15, Difficulty::infernal).front() == 3,
          "A small fight grows to at most 170% of its creatures");
    const auto strong = monster_strength(Difficulty::infernal, 2);
    check(strong.hit_points_percent == monster_strengths[4].hit_points_percent &&
              strong.attack_bonus == monster_strengths[4].attack_bonus,
          "From party level 2 a level has its full strength");
    const auto frail = monster_strength(Difficulty::infernal, 1);
    check(frail.hit_points_percent == 100 + (strong.hit_points_percent - 100) / 2 &&
              frail.attack_bonus == strong.attack_bonus / 2,
          "At party level 1 a level above Normal has half its extra strength");
    check(monster_strength(Difficulty::easy, 1).hit_points_percent ==
              monster_strengths[0].hit_points_percent,
          "Easy needs no easing in");
    check(average_party_level(party) == 3, "The party's level is its members' average");
}

int main()
{
    try
    {
        budgets();
        fitting();
        difficulty_sizes_fights();
        std::cout << "Encounter budget tests passed.\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
