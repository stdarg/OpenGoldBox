#ifndef OPENGOLD_ORIGINAL_HISTORY_ENTRIES_H
#define OPENGOLD_ORIGINAL_HISTORY_ENTRIES_H
#include "opengold/original_combat.h"
#include "opengold/original_rules.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// The entries the original rules write into a character's level history
// (AdvancementChoice::rules_data) and read back: what a training rolled, a
// level drain and its restoration, a raising, aging, the kit's Charisma rot
// and the Krynn calendar. Each decode_* answers nothing for an entry of
// another kind and throws for an unreadable one of its own.
namespace opengold::original::history
{
// What a training rolled, as a level's rules data: a version, the trained
// classes as bits by class slot, and the Hit Dice's total.
struct Training
{
    unsigned classes{};
    int rolled{};
};

inline constexpr int training_data_version = 1;

// A level drain a fight left (effhndlr.pas, the level drain), as a level
// history entry: "drain", the levels lost in each class slot and the Hit
// Points lost with them.
struct Drain
{
    ClassLevels levels{};
    int hit_points{};
};

inline constexpr std::string_view drain_word = "drain";

// Native PROGRAM 3 rolls the HP loss once and retains that result in the
// character's ordinary history, including its separate raw HP and XP losses.
using NativeDrain = NativeDrainReceipt;
inline constexpr std::string_view native_drain_word = "native_drain ";
inline constexpr std::string_view exact_native_drain_word = "native_drain2 ";

// What being raised cost (temple.pas, RaiseDead; Curse's spell, cast_raise),
// as a level history entry: "raised" and how, "temple" or "spell".
inline constexpr std::string_view raised_word = "raised";

inline constexpr std::string_view krynn_calendar_word = "krynn_days ";

// The years a character's age changed by after creation (Haste ages a year,
// Youth takes 1d4+1 off), as a level history entry: "age" and the years.
inline constexpr std::string_view age_word = "age";

// Treasures' refused proposal (its romance.pas, UpdateRelationship), as a
// level history entry.
inline constexpr std::string_view romance_refused_entry = "romance refused";

// The Charisma the kit's rot took (effects.c, fx_rotting), as a level
// history entry: "charisma" and the points lost.
inline constexpr std::string_view charisma_word = "charisma";

// A drained level Restoration gives back (magic.pas, SpellRestoration), as a
// level history entry: "restore", the class slot, and the Hit Points and
// Hit Points rolled given back with it.
struct Restoration
{
    ClassSlot slot{};
    int hit_points{}, rolled_hit_points{};
    std::optional<unsigned> native_experience;
};

inline constexpr std::string_view restore_word = "restore";
inline constexpr std::string_view native_restore_word = "native_restore";

[[nodiscard]] std::string encode_training(const Training &training);
[[nodiscard]] Training decode_training(std::string_view data);
[[nodiscard]] std::string encode_drain(const Drain &drain);
[[nodiscard]] std::string encode_native_drain(const NativeDrain &loss);
[[nodiscard]] std::optional<NativeDrain> decode_native_drain(std::string_view data);
[[nodiscard]] std::string encode_raised(rules::Revival by);
[[nodiscard]] std::optional<rules::Revival> decode_raised(std::string_view data);
[[nodiscard]] std::optional<std::uint64_t> decode_krynn_calendar(std::string_view data);
[[nodiscard]] std::string encode_age(int years);
[[nodiscard]] std::optional<int> decode_age(std::string_view data);
[[nodiscard]] std::string encode_charisma_loss(int points);
[[nodiscard]] std::optional<int> decode_charisma_loss(std::string_view data);
[[nodiscard]] std::optional<Drain> decode_drain(std::string_view data);
[[nodiscard]] std::string encode_restoration(const Restoration &restoration);
[[nodiscard]] std::optional<Restoration> decode_restoration(std::string_view data);
} // namespace opengold::original::history
#endif
