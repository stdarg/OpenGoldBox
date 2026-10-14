#ifndef OPENGOLD_TRAINING_HALL_H
#define OPENGOLD_TRAINING_HALL_H
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace opengold
{
// New Phlan's four training halls (docs/TRAINING-HALL.md). Each trains the SRD
// classes of one AD&D 2nd edition class group.
enum class TrainingHall
{
    fighters,
    clerics,
    magic_users,
    thieves
};

// The hall that trains a class, by its rules class ID ("fighter").
[[nodiscard]] std::optional<TrainingHall> training_hall_for_class(std::string_view class_id);

// The class IDs a hall trains, in the order the table lists them.
[[nodiscard]] std::vector<std::string_view> hall_classes(TrainingHall hall);

// The hall a script names in its "WE TRAIN ONLY ..." text ("FIGHTERS ").
[[nodiscard]] std::optional<TrainingHall> training_hall_named(std::string_view script_name);

// The highest level a hall's classes reach in the campaign: Phlan's Training
// Hall trains "8th level fighter, 9th level thief, and 6th level cleric and
// magic-user" (CAP-1, Pool of Radiance manual).
[[nodiscard]] unsigned hall_level_limit(TrainingHall hall);

// A session of experience costs 1,000 gp at level 1 and doubles at each level.
[[nodiscard]] std::uint32_t experience_session_fee(unsigned level);

// Why a hall turns a member away; none when it can serve them.
enum class TrainingDenial
{
    none,
    other_hall,      // The member's class trains at another hall.
    not_active,      // Only active, living members train.
    highest_level,   // There is no next level to train toward.
    ready_to_advance, // Enough experience already: advance first.
    already_trained, // One of this service per level.
    gold             // The party's gold cannot pay the fee.
};

// One member's experience session: its fee, the experience it grants and
// whether the hall can sell it now.
struct ExperienceOffer
{
    std::uint32_t fee{};
    unsigned experience{};
    TrainingDenial denial{TrainingDenial::none};
};

// Skill training: a new proficiency, or Expertise in one already held.
constexpr std::uint32_t proficiency_training_fee = 250;
constexpr std::uint32_t expertise_training_fee = 500;

// The fee for a lesson, by its grant ("skill:stealth" or "expertise:stealth").
[[nodiscard]] std::uint32_t skill_training_fee(std::string_view grant);
} // namespace opengold
#endif
