#include "opengold/training_hall.h"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace opengold
{
namespace
{
constexpr std::array<std::pair<std::string_view, TrainingHall>, 12> class_halls{
    {{"fighter", TrainingHall::fighters},
     {"barbarian", TrainingHall::fighters},
     {"paladin", TrainingHall::fighters},
     {"ranger", TrainingHall::fighters},
     {"cleric", TrainingHall::clerics},
     {"druid", TrainingHall::clerics},
     {"wizard", TrainingHall::magic_users},
     {"sorcerer", TrainingHall::magic_users},
     {"warlock", TrainingHall::magic_users},
     {"rogue", TrainingHall::thieves},
     {"bard", TrainingHall::thieves},
     {"monk", TrainingHall::thieves}}};

// The words the original hall script stores before "HERE. DO YOU WANT TO TRAIN?".
constexpr std::array<std::pair<std::string_view, TrainingHall>, 4> script_halls{
    {{"FIGHTERS ", TrainingHall::fighters},
     {"CLERICS ", TrainingHall::clerics},
     {"MAGIC USERS ", TrainingHall::magic_users},
     {"THIEVES ", TrainingHall::thieves}}};
} // namespace

std::optional<TrainingHall> training_hall_for_class(std::string_view class_id)
{
    for (const auto &[id, hall] : class_halls)
    {
        if (id == class_id)
        {
            return hall;
        }
    }
    return {};
}

bool trains_any(TrainingHall hall, std::span<const std::string> class_ids)
{
    return std::any_of(class_ids.begin(), class_ids.end(),
                       [&](const std::string &id)
                       {
                           const auto trained_by = training_hall_for_class(id);
                           return trained_by &&
                                  (hall == TrainingHall::every_class || trained_by == hall);
                       });
}

std::vector<std::string_view> hall_classes(TrainingHall hall)
{
    std::vector<std::string_view> result;
    for (const auto &[id, trained_by] : class_halls)
    {
        if (hall == TrainingHall::every_class || trained_by == hall)
        {
            result.push_back(id);
        }
    }
    return result;
}

std::optional<TrainingHall> training_hall_named(std::string_view script_name)
{
    for (const auto &[name, hall] : script_halls)
    {
        if (name == script_name)
        {
            return hall;
        }
    }
    return {};
}

std::uint32_t experience_session_fee(unsigned level)
{
    // Level 20 would cost 524,288,000 gp; a level beyond the SRD's has no fee.
    if (level < 1 || level > 19)
    {
        throw std::out_of_range("Experience sessions are for levels 1 to 19");
    }
    return 1000u << (level - 1);
}

std::uint32_t skill_training_fee(std::string_view grant)
{
    return grant.starts_with("expertise:") ? expertise_training_fee : proficiency_training_fee;
}

std::vector<std::string_view> original_hall_classes(TrainingHall hall)
{
    return original_mask_classes(original_class_mask(hall));
}

std::vector<std::string_view> original_mask_classes(unsigned class_mask, GoldBoxGame game)
{
    if (game == GoldBoxGame::death_knights_of_krynn || game == GoldBoxGame::dark_queen_of_krynn)
    {
        static constexpr std::array<std::pair<std::string_view, unsigned>, 7> death_knights_bits{
            {{"magic_user", 1},
             {"cleric", 2},
             {"thief", 4},
             {"fighter", 8},
             {"knight", 16},
             {"paladin", 64},
             {"ranger", 128}}};
        // Native training uses class_bit, not its separate class_mask_bits:
        // knights are 40h and rangers 20h here.
        static constexpr std::array<std::pair<std::string_view, unsigned>, 7> dark_queen_bits{
            {{"magic_user", 1},
             {"cleric", 2},
             {"thief", 4},
             {"fighter", 8},
             {"knight", 64},
             {"paladin", 16},
             {"ranger", 32}}};
        const auto &krynn_bits =
            game == GoldBoxGame::dark_queen_of_krynn ? dark_queen_bits : death_knights_bits;
        std::vector<std::string_view> classes;
        for (const auto &[klass, bit] : krynn_bits)
        {
            if ((class_mask & bit) != 0)
            {
                classes.push_back(klass);
            }
        }
        return classes;
    }
    static constexpr std::array<std::pair<std::string_view, unsigned>, 8> class_bits{{
        {"cleric", 0x02},
        {"druid", 0x02},
        {"fighter", 0x08},
        {"paladin", 0x10},
        {"ranger", 0x20},
        {"magic_user", 0x01},
        {"thief", 0x04},
        {"monk", 0x04},
    }};
    std::vector<std::string_view> classes;
    for (const auto &[klass, bit] : class_bits)
    {
        if ((class_mask & bit) != 0)
        {
            if (game == GoldBoxGame::champions_of_krynn && klass == "monk")
            {
                continue;
            }
            classes.push_back(
                game == GoldBoxGame::champions_of_krynn && klass == "paladin" ? "knight" : klass);
        }
    }
    return classes;
}

unsigned original_class_mask(TrainingHall hall)
{
    switch (hall)
    {
    case TrainingHall::fighters:
        return 0x08 | 0x10 | 0x20;
    case TrainingHall::clerics:
        return 0x02;
    case TrainingHall::magic_users:
        return 0x01;
    case TrainingHall::thieves:
        return 0x04;
    case TrainingHall::every_class:
        return 127;
    }
    return 0;
}

} // namespace opengold
