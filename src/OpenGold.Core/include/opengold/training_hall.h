#ifndef OPENGOLD_TRAINING_HALL_H
#define OPENGOLD_TRAINING_HALL_H
#include "opengold/gold_box_game.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace opengold
{
// New Phlan's four training halls (docs/TRAINING-HALL.md). Each trains the SRD
// classes of one AD&D 2nd edition class group. Curse's halls train every
// class (their scripts' class mask is 127).
enum class TrainingHall
{
    fighters,
    clerics,
    magic_users,
    thieves,
    every_class
};

// The hall that trains a class, by its rules class ID ("fighter").
[[nodiscard]] std::optional<TrainingHall> training_hall_for_class(std::string_view class_id);

// The class IDs a hall trains, in the order the table lists them.
[[nodiscard]] std::vector<std::string_view> hall_classes(TrainingHall hall);

// Whether the hall trains any of a character's classes, given by stable ID: a
// multiclass character may use the hall of any of them (MULTI-1).
[[nodiscard]] bool trains_any(TrainingHall hall, std::span<const std::string> class_ids);

// The hall a script names in its "WE TRAIN ONLY ..." text ("FIGHTERS ").
[[nodiscard]] std::optional<TrainingHall> training_hall_named(std::string_view script_name);

// The highest level the Training Hall trains any class to, the game's cap
// (LEVELS-2), in place of the original's "8th level fighter, 9th level thief,
// and 6th level cleric and magic-user" (CAP-1).
inline constexpr unsigned hall_level_limit = 15;

// A session of experience costs 1,000 gp at level 1 and doubles at each level.
[[nodiscard]] std::uint32_t experience_session_fee(unsigned level);

// Why a hall turns a member away; none when it can serve them.
enum class TrainingDenial
{
    none,
    other_hall,       // The member's class trains at another hall.
    not_active,       // Only active, living members train.
    highest_level,    // There is no next level to train toward.
    ready_to_advance, // Enough experience already: advance first.
    already_trained,  // One of this service per level.
    gold,             // The party's gold cannot pay the fee.
    not_conscious,    // The original trains only conscious people.
    experience        // Not enough experience for any class this hall trains.
};

// The original's classes a hall trains, by their stable IDs, as its script's
// class mask has them (chargen.pas, TrainingMenu): the fighters' hall
// fighters, paladins and rangers, the clerics' clerics and druids.
[[nodiscard]] std::vector<std::string_view> original_hall_classes(TrainingHall hall);

// What the original's training costs (chargen.pas, TrainingMenu): 1,000
// gold, and nothing in Secret of the Silver Blades and Pools of Darkness,
// whose TrainingMenu takes no money. Treasures of the Savage Frontier asks
// 200 platinum for each class trained, one a visit: 1,000 gold's worth.
[[nodiscard]] constexpr std::uint32_t original_training_fee(GoldBoxGame game) noexcept
{
    return game == GoldBoxGame::secret_of_the_silver_blades ||
                   game == GoldBoxGame::pools_of_darkness
               ? 0
               : 1000;
}

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
