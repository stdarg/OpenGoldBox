#ifndef OPENGOLDBOX_VITAL_FIXTURES_H
#define OPENGOLDBOX_VITAL_FIXTURES_H
// Acceptance checks author rules-owned vital state directly. This writes the
// SRD module's only vital format with every class pool full, so a check states
// just the spent and mortality fields it exercises.
#include "opengold/character.h"
#include <algorithm>
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
    std::string_view effects = "FX8 1 0 0";
};

// Reads the race and class by stable ID; their labels are presentation only.
inline std::string srd_vitals(const opengold::Character &character, const VitalFixture &fixture)
{
    const auto &sheet = character.sheet();
    const auto &race = character.creation_data().race;
    const auto &class_id = character.creation_data().character_class;
    const int rushes = race == "orc" ? 2 + (sheet.level - 1) / 4 : 0;
    const int surges = class_id == "fighter" && sheet.level >= 2 ? 1 : 0;
    const int arcane = class_id == "wizard" ? 1 : 0;
    const int lay_on_hands = class_id == "paladin" ? 5 * sheet.level : 0;
    std::ostringstream out;
    const int free_casts = class_id == "paladin" && sheet.level >= 2 ? 1
                           : class_id == "ranger"                    ? 2
                                                                     : 0;
    // The free cast of each Magic Initiate feat that has chosen its spell, unspent.
    const auto initiates =
        std::count_if(sheet.grants.begin(), sheet.grants.end(),
                      [](const auto &grant)
                      {
                          const auto access = grant.choices.find("access");
                          return grant.source_id.starts_with("feat:magic_initiate_") &&
                                 access != grant.choices.end() &&
                                 access->second == "always_prepared";
                      });
    out << "SRD13 " << fixture.winds << ' ' << fixture.slots << ' ' << fixture.slots2
        << " 0 0 0 0 0 0 0 " << fixture.successes << ' ' << fixture.failures << ' '
        << fixture.stable << ' ' << fixture.hit_dice << ' ' << fixture.death_save_ms << ' '
        << fixture.stable_recovery_ms << " 0 \"\" " << rushes << ' ' << surges << ' ' << arcane
        << ' ' << lay_on_hands << ' ' << free_casts << " 0 " << (1 << initiates) - 1 << ' '
        << fixture.effects;
    return out.str();
}
} // namespace presentation
#endif
