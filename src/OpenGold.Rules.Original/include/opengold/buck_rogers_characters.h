#ifndef OPENGOLD_BUCK_ROGERS_CHARACTERS_H
#define OPENGOLD_BUCK_ROGERS_CHARACTERS_H
#include "opengold/buck_rogers_combat.h"
#include "opengold/character_rules.h"

#include <optional>
#include <string>
#include <string_view>

// A Buck Rogers character as the campaign keeps it (docs/games/countdown.md,
// "The team in the campaign"): the game's own record, whole, with its items
// and the ITEMS rows they name (GroundRecord). Its creation draft holds the
// record as it was made; a fight, a shop or training gives back the record
// as it is now, which the party keeps in the member's vitals
// (rules::VitalState::resources), as the combat session leaves it there. The
// sheet the party's screens read is drawn from the record.
namespace opengold::original
{
// The draft of a character whose record this is: its name, race, gender and
// career, and the record whole as the draft's rules data.
[[nodiscard]] rules::CharacterDraft buck_rogers_draft(const GroundRecord &record);
// The record a draft or a vitals' resources hold, if they hold one.
[[nodiscard]] std::optional<GroundRecord> buck_rogers_record_of(std::string_view rules_data);
[[nodiscard]] bool holds_buck_rogers_record(std::string_view rules_data) noexcept;
// The sheet of a draft holding a record: its name, race, career, the first
// six abilities, its level and Hit Points, and the record's text in
// CharacterSheet::Original::game_record.
[[nodiscard]] rules::CharacterSheet buck_rogers_sheet(const rules::CharacterDraft &draft,
                                                      const GroundRecord &record,
                                                      rules::Identity identity);

// The names the game shows: the six races offered and the NPCs' two, and the
// careers (gdconsts.pas raceNames, careerNames).
[[nodiscard]] std::string_view buck_rogers_race_name(Race race);
[[nodiscard]] std::string_view buck_rogers_career_name(Profession career);
} // namespace opengold::original

#endif
