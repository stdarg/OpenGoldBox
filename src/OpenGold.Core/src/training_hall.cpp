#include "opengold/training_hall.h"
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
        if (id == class_id)
            return hall;
    return {};
}

std::vector<std::string_view> hall_classes(TrainingHall hall)
{
    std::vector<std::string_view> result;
    for (const auto &[id, trained_by] : class_halls)
        if (trained_by == hall)
            result.push_back(id);
    return result;
}

std::optional<TrainingHall> training_hall_named(std::string_view script_name)
{
    for (const auto &[name, hall] : script_halls)
        if (name == script_name)
            return hall;
    return {};
}

std::uint32_t experience_session_fee(unsigned level)
{
    // Level 20 would cost 524,288,000 gp; a level beyond the SRD's has no fee.
    if (level < 1 || level > 19)
        throw std::out_of_range("Experience sessions are for levels 1 to 19");
    return 1000u << (level - 1);
}

std::uint32_t skill_training_fee(std::string_view grant)
{
    return grant.starts_with("expertise:") ? expertise_training_fee : proficiency_training_fee;
}
} // namespace opengold
