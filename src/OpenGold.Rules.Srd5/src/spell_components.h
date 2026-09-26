#ifndef OPENGOLD_SRD5_SPELL_COMPONENTS_H
#define OPENGOLD_SRD5_SPELL_COMPONENTS_H
#include "spell_table.h"
#include <array>
#include <cstddef>
#include <string_view>
namespace opengold::srd5::detail {
struct SpellComponents {std::string_view id;bool verbal{},somatic{};};
// SRD 5.2.1 spell descriptions, carried on the spell table. These spells have
// no Material component; component substitution and speech-blocking sources
// are separate increments.
inline const SpellComponents* spell_components(std::string_view command){
    const auto* spell=find_spell(command);
    if(!spell)return nullptr;
    // Callers keep the returned pointer, so it must outlive the call. The table
    // is constexpr and fixed, so a parallel array indexed the same way is stable.
    static const auto views=[]{
        std::array<SpellComponents,spell_table.size()> out{};
        for(std::size_t i=0;i<spell_table.size();++i)
            out[i]={spell_table[i].id,spell_table[i].verbal,spell_table[i].somatic};
        return out;
    }();
    return &views[static_cast<std::size_t>(spell-spell_table.data())];
}
}
#endif
