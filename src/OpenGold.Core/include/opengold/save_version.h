#ifndef OPENGOLD_SAVE_VERSION_H
#define OPENGOLD_SAVE_VERSION_H
#include <optional>
#include <string>
#include <string_view>

namespace opengold
{
// The one save version this build reads and writes, for every game
// (docs/SAVES.md). Any change to what a save holds raises it and commits the
// new layout as docs/save-schema/v<version>.txt (campaign_save_schema).
inline constexpr unsigned save_version = 56;
// The refusal of a save of any other version, older or newer: {saved} is the
// save's version and {version} save_version. Hosts translate it.
inline constexpr std::string_view save_version_message =
    "This save is version {saved}, and this version of OpenGoldBox reads only save version "
    "{version}.";
// The version a refusal of another save version names (save_version_message
// in English), or none when `message` is not that refusal.
[[nodiscard]] std::optional<std::string> refused_save_version(std::string_view message);
} // namespace opengold
#endif
