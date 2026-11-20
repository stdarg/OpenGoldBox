#ifndef OPENGOLD_BUCK_ROGERS_CREATION_H
#define OPENGOLD_BUCK_ROGERS_CREATION_H
#include "opengold/buck_rogers_characters.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/character_rules.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Creation in the two Buck Rogers games through the creation pages every
// game shares (docs/CHARACTER-CREATION.md, "The Buck Rogers games"): the
// draft names the race, gender and career, and once rolled holds the
// character's record whole (buck_rogers_draft), which the later pages, the
// skills, a Warrior's specializations and the body icon, change.
namespace opengold::original
{
// What the pages list: the six races and the five careers in their menus'
// order (chargen.pas CreateCharacterMenu), the two genders; no alignment and
// no background.
[[nodiscard]] std::vector<rules::CreationChoice>
buck_rogers_creation_choices(rules::CreationField field);
// Whether the draft's race allows the career (professions_for).
[[nodiscard]] bool buck_rogers_creation_allows(const rules::CharacterDraft &draft,
                                               rules::CreationField field, std::string_view id,
                                               GoldBoxGame game);

[[nodiscard]] std::optional<Race> buck_rogers_race_named(std::string_view id);
[[nodiscard]] std::optional<Profession> buck_rogers_career_named(std::string_view id);

struct BuckRogersDraftChoices
{
    Race race{};
    Gender gender{};
    Profession career{};
};

// The race, gender and career a draft names. Throws std::invalid_argument
// for ones the menus do not offer together.
[[nodiscard]] BuckRogersDraftChoices buck_rogers_draft_choices(const rules::CharacterDraft &draft,
                                                               GoldBoxGame game);
// The roll (CreateCharacterMenu): a new character of the draft's choices,
// trained free to the starting level, with the draft's name and its default
// body icon (SetDefaultIcon), as its record. Rolling again keeps the age, as
// the original's "Reroll stats?" does, and starts the skills and
// specializations over.
void roll_buck_rogers_draft(rules::CharacterDraft &draft, GoldBoxGame game, RandomState &random);
// The draft with the name, in its record too. Throws std::invalid_argument
// for a name the record cannot hold (15 letters).
[[nodiscard]] rules::CharacterDraft named_buck_rogers_draft(const rules::CharacterDraft &draft,
                                                            std::string name);
// The pages these games ask after the roll: no alignment or portrait, the
// skill screen, and a new Warrior's specializations (one in Countdown, three
// in Matrix Cubed).
[[nodiscard]] rules::InPlacePages buck_rogers_creation_pages(const rules::CharacterDraft &draft,
                                                             GoldBoxGame game);
} // namespace opengold::original
#endif
