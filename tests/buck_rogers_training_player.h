#ifndef OPENGOLD_TESTS_BUCK_ROGERS_TRAINING_PLAYER_H
#define OPENGOLD_TESTS_BUCK_ROGERS_TRAINING_PLAYER_H

#include "opengold/buck_rogers_characters.h"
#include "opengold/buck_rogers_combat.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/rolf_tour.h"
#include "test_support.h"
#include <algorithm>
#include <iostream>
#include <span>
#include <vector>

namespace opengold::test
{
// The skills in `order`, the ones the character has most in first (a player
// builds on what a member already does), equals in the rules' order.
inline std::vector<original::Skill> strongest_first(const buck_rogers::Creature &character,
                                                    std::vector<original::Skill> order)
{
    std::ranges::stable_sort(order,
                             [&](original::Skill left, original::Skill right)
                             {
                                 return original::level_of(character.skills, left) >
                                        original::level_of(character.skills, right);
                             });
    return order;
}

// The training pages as the earned routes fill them, through the session's
// public training commands, as a player does: a Warrior specializes in its
// readied gun while that takes another, else in the first weapon the menu
// offers that does; career points go to the career's skills and general
// points to the general skills the member already knows, strongest first,
// each as far as the rules take it; then the pages are left.
inline void choose_buck_rogers_training(por::RolfTourSession &session, const CampaignParty &party,
                                        std::span<const buck_rogers::Item> templates)
{
    const auto *training = session.buck_rogers_training();
    check(training != nullptr, "The training waits for its choices");
    const auto record = original::buck_rogers_record_of(party.game_record(training->member));
    check(record.has_value(), "The trainee has its record");
    if (training->choices.specializations_owed() > 0)
    {
        std::vector<std::uint8_t> weapons;
        if (const auto gun = original::readied_weapon(record->items, record->types))
        {
            weapons.push_back(record->items[*gun].type);
        }
        for (const auto type : original::specializable_weapon_types(templates))
        {
            weapons.push_back(type);
        }
        const auto chosen = std::ranges::find_if(weapons,
                                                 [&](std::uint8_t type)
                                                 {
                                                     return session.specialize_in_training(type);
                                                 });
        check(chosen != weapons.end(), "The Warrior's specialization is chosen");
        std::cout << "Training specialization in item type " << unsigned(*chosen) << ".\n";
    }
    const auto &character = training->choices.character();
    const auto career = original::career_from_record(character.career);
    check(career.has_value(), "The trainee has a career");
    const auto career_skills = original::career_skills(*career);
    std::vector<original::Skill> known_general;
    for (const auto skill : original::general_skills(*career))
    {
        if (original::level_of(character.skills, skill) > 0)
        {
            known_general.push_back(skill);
        }
    }
    auto order = strongest_first(character, {career_skills.begin(), career_skills.end()});
    const auto general = strongest_first(character, known_general);
    order.insert(order.end(), general.begin(), general.end());
    for (const auto skill : order)
    {
        int points = 0;
        while (session.raise_training_skill(skill) == original::SkillRefusal::none)
        {
            ++points;
        }
        if (points > 0)
        {
            std::cout << "Training skill " << original::skill_definition(skill).name << " +"
                      << points << ".\n";
        }
    }
    const auto &spending = session.buck_rogers_training()->choices.spending();
    std::cout << "Training leaves " << spending.career_points() << " career and "
              << spending.general_points() << " general points unspent.\n";
    session.finish_buck_rogers_training();
}
} // namespace opengold::test
#endif
