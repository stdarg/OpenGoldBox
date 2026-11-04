#ifndef OPENGOLD_TEST_MEMBER_CONDITIONS_H
#define OPENGOLD_TEST_MEMBER_CONDITIONS_H
#include "opengold/campaign_party.h"
#include "status_effects.h"
#include <functional>
#include <sstream>
#include <stdexcept>

namespace opengold::test
{
// Gives a member a complete vital record at `hit_points`, or dead, with the
// effects `change` adds (Blinded, Poisoned, a disease, a curse, Petrified,
// the mark of the recently dead): a member a temple can help. Needs the
// SRD module's internal headers (status_effects.h).
inline void set_condition(const rules::RulesModule &rules, PartyMember &member, int hit_points,
                          bool dead,
                          const std::function<void(srd5::detail::EffectState &)> &change = {})
{
    member.vitals = {member.character.sheet().hit_points, false, {}};
    rules.recover(member.vitals, member.character.sheet());
    member.vitals.hit_points = hit_points;
    member.vitals.dead = dead;
    if (!change)
    {
        return;
    }
    const auto at = member.vitals.resources.find("FX9");
    if (at == std::string::npos)
    {
        throw std::runtime_error("A vital record holds its effects");
    }
    std::istringstream in(member.vitals.resources.substr(at));
    auto effects = srd5::detail::read_effects(in);
    change(effects);
    std::ostringstream out;
    srd5::detail::write_effects(out, effects);
    member.vitals.resources.replace(at, std::string::npos, out.str());
}
} // namespace opengold::test
#endif
