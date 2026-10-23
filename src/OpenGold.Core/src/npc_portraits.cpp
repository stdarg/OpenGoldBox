#include "opengold/npc_portraits.h"
#include "opengold/rolf_tour.h"
#include <algorithm>
#include <array>

namespace opengold
{
namespace
{
// Whether the party stands on one of a data/shops location's squares. The
// snapshot names the map by its record, which differs between the game's maps;
// New Phlan is record 0 of GEO3.DAX.
bool stands_on(const por::TourSnapshot &state, const std::string &archive, unsigned record,
               const std::vector<std::pair<unsigned, unsigned>> &markers)
{
    return record == state.area_id && (record != 0 || archive == "GEO3.DAX") &&
           std::ranges::find(markers, std::pair{state.pose.x, state.pose.y}) != markers.end();
}
} // namespace

// The shopkeeper of the New Phlan shop the party stands in, from its data/shops
// record. The original shop offer has a two-choice "THE SHOP..." dialogue
// before the host enters shopping; show the same person for that greeting and
// the entire shop.
std::string_view phlan_shopkeeper_portrait(const por::TourSnapshot &state,
                                           const std::vector<ShopSite> &shops)
{
    const bool greeting = state.phase == por::TourPhase::awaiting_continue &&
                          state.choices.size() == 2 && state.dialogue.starts_with("THE SHOP");
    if (state.area_id != 0 || !state.tour_finished ||
        (state.phase != por::TourPhase::shopping && !greeting))
    {
        return {};
    }
    for (const auto &shop : shops)
    {
        if (stands_on(state, shop.archive, shop.record, shop.markers))
        {
            return shop.portrait;
        }
    }
    return {};
}

// The priest of the temple the party stands in, from its data/shops record,
// for everything the temple says there: its greeting and its Heal list.
std::string_view temple_priest_portrait(const por::TourSnapshot &state,
                                        const std::vector<TempleSite> &temples)
{
    if (!state.tour_finished || state.phase != por::TourPhase::awaiting_continue)
    {
        return {};
    }
    for (const auto &temple : temples)
    {
        if (stands_on(state, temple.archive, temple.record, temple.markers))
        {
            return temple.portrait;
        }
    }
    return {};
}

// The complete NPC portrait (docs/PORTRAITS.md), relative to the installed
// portraits folder. Shopkeepers appear at their greeting and throughout
// shopping, and priests while their temple speaks. Story portraits use script-specific speaker
// cues, not the shared sprite ID: several characters use the same original sprite. Frame 0 is the
// encounter sprite's nearest pose; -1 means no active approach sprite.
std::string_view speaking_npc_portrait(const por::TourSnapshot &state,
                                       const std::vector<ShopSite> &shops,
                                       const std::vector<TempleSite> &temples)
{
    if (const auto shopkeeper = phlan_shopkeeper_portrait(state, shops); !shopkeeper.empty())
    {
        return shopkeeper;
    }
    if (const auto priest = temple_priest_portrait(state, temples); !priest.empty())
    {
        return priest;
    }
    if (state.dialogue.empty())
    {
        return {};
    }
    if (!state.tour_finished)
    {
        return state.sprite_frame == 0 ? "NPCs/rolf.png" : std::string_view{};
    }
    // Kuto's Well (ECL8:29) shows sprite 16 only for Norris the Gray's ambush.
    if (state.script_id == 29 && state.sprite_id == 16 && state.sprite_frame == 0)
    {
        return "NPCs/norris-the-gray.png";
    }
    if (state.sprite_frame > 0)
    {
        return {};
    }

    struct Cue
    {
        unsigned script;
        std::string_view opening, portrait;
    };

    // These are the exact opening words of dialogue in the supported original
    // ECL scripts, verified without consulting the old head/body artwork.
    constexpr std::array cues{
        Cue{0, "THE HARBOR MASTER TELLS", "NPCs/harbor-master.png"},
        Cue{0, "'BY ORDER OF THE CITY COUNCIL,' THE HARBOR MASTER", "NPCs/harbor-master.png"},
        Cue{0, "YOU ARE USHERED INTO THE BISHOP'S STUDY", "NPCs/bishop-braccio.png"},
        Cue{0, "'HE IS BOUND TO RECOVER THE TEMPLE", "NPCs/bishop-braccio.png"},
        Cue{0, "YOU ARE USHERED INTO THE STUDY WHERE DIRTEN WAITS", "NPCs/dirten.png"},
        Cue{8, "AT YOUR ENTRY, THE COUNCIL CLERK", "NPCs/council-clerk.png"},
        Cue{8, "THE CLERK SPEAKS", "NPCs/council-clerk.png"},
        Cue{8, "THE CLERK SHUFFLES", "NPCs/council-clerk.png"},
        Cue{8, "COUNCILMAN CADORNA CONFRONTS", "NPCs/councilman-cadorna.png"},
        Cue{8, "'WHEN OLD PHLAN WAS OVERRUN", "NPCs/councilman-cadorna.png"},
        Cue{8, "'THE FAITHFUL SERVANT SENT", "NPCs/councilman-cadorna.png"},
        Cue{8, "'IT IS IMPERATIVE THAT A MESSAGE", "NPCs/councilman-cadorna.png"},
        Cue{8, "'WHEN YOU GET TO ZHENTIAL KEEP", "NPCs/councilman-cadorna.png"},
        Cue{8, "IN THE COUNCIL CHAMBERS, THE ENTIRE CITY COUNCIL",
            "NPCs/mayor-ulrich-eberhard.png"},
        Cue{8, "'THANK YOU FOR COMING", "NPCs/mayor-ulrich-eberhard.png"},
        Cue{8, "'THE GATE IS TOO STRONG", "NPCs/mayor-ulrich-eberhard.png"},
        Cue{8, "'IT IS THE UNANIMOUS REQUEST", "NPCs/mayor-ulrich-eberhard.png"},
        Cue{20, "SEATED AT A TABLE IS A RAGGED OLD WOMAN", "NPCs/slums-fortune-teller.png"},
        Cue{20, "THE WOMAN'S HANDS MAKE MYSTIC PASSAGES", "NPCs/slums-fortune-teller.png"},
        Cue{20, "'I MAY HAVE A USE FOR YOU", "NPCs/ohlo.png"},
        Cue{20, "'YOU HAVE DONE WELL. GIVE ME THE POTION", "NPCs/ohlo.png"},
        Cue{20, "THE MAN TAKES THE POTION AND GULPS", "NPCs/ohlo.png"},
        Cue{20, "'YOU PRESUME TOO MUCH", "NPCs/ohlo.png"},
        Cue{20, "'I HAVE NO TIME FOR THE LIKES OF YOU", "NPCs/ohlo.png"},
        Cue{20, "'IT WAITS IN A BOOTH IN THE OLD ROPE GUILD", "NPCs/ohlo.png"},
        Cue{20, "'I AM SORRY,' THE MAN SAYS, 'I CANNOT HELP YOU", "NPCs/ohlos-potion-keeper.png"},
        Cue{20, "'WAIT A MOMENT,' THE MAN SAYS", "NPCs/ohlos-potion-keeper.png"},
        Cue{29, "A WIDE-EYED WOMAN IS SEATED ON A RUG", "NPCs/kutos-well-seer.png"},
    };
    const std::string_view dialogue{state.dialogue};
    for (const auto &cue : cues)
    {
        if (state.script_id == cue.script && dialogue.starts_with(cue.opening))
        {
            return cue.portrait;
        }
    }
    return {};
}
} // namespace opengold
