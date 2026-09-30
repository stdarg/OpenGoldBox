#ifndef OPENGOLDBOX_VITAL_FIXTURES_H
#define OPENGOLDBOX_VITAL_FIXTURES_H
// Acceptance checks author rules-owned vital state directly. This writes the
// SRD module's only vital format with every class pool full, so a check states
// just the spent and mortality fields it exercises.
#include "opengold/character_rules.h"
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace presentation
{
struct VitalFixture
{
    int winds{}, slots{}, slots2{}, successes{}, failures{};
    bool stable{};
    int hit_dice{}, death_save_ms{}, stable_recovery_ms{};
    std::string_view effects = "FX7 1 0 0 0";
};

inline std::string srd_vitals(const opengold::rules::CharacterSheet &sheet,
                              const VitalFixture &fixture)
{
    const int rushes = sheet.race == "Orc" ? 2 + (sheet.level - 1) / 4 : 0;
    const int surges = sheet.character_class == "Fighter" && sheet.level >= 2 ? 1 : 0;
    const int arcane = sheet.character_class == "Wizard" ? 1 : 0;
    std::ostringstream out;
    out << "SRD9 " << fixture.winds << ' ' << fixture.slots << ' ' << fixture.slots2 << ' '
        << fixture.successes << ' ' << fixture.failures << ' ' << fixture.stable << ' '
        << fixture.hit_dice << ' ' << fixture.death_save_ms << ' ' << fixture.stable_recovery_ms
        << " 0 \"\" " << rushes << ' ' << surges << ' ' << arcane << ' ' << fixture.effects;
    return out.str();
}
} // namespace presentation
#endif
