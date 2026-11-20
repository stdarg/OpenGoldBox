#ifndef OPENGOLD_DARK_QUEEN_CAMPAIGN_H
#define OPENGOLD_DARK_QUEEN_CAMPAIGN_H

#include "opengold/rolf_tour.h"
#include "opengold/ua_combat.h"

namespace opengold::por
{
// Immutable native resources shared by this campaign's districts. The root
// owns the districts; these resources do not retain the root.
struct DarkQueenResources
{
    explicit DarkQueenResources(const std::filesystem::path &folder)
        : files(folder, folder), icons(files)
    {
    }

    ua::DesignFiles files;
    ua::CombatIcons icons;
    std::vector<std::string> item_words;
    std::map<unsigned, Image> backdrops;
    std::map<unsigned, std::vector<AnimationFrame>> big_pictures;
};

[[nodiscard]] RolfTourSession load_dark_queen_campaign(const std::filesystem::path &folder);
[[nodiscard]] ua::FightField dark_queen_dungeon_field(const DarkQueenResources &resources,
                                                      const GeoMap &map, PartyPose pose,
                                                      unsigned set);
[[nodiscard]] ua::WildernessStyle dark_queen_wilderness_style(unsigned terrain, bool river);
} // namespace opengold::por

#endif
