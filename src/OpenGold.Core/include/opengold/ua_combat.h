#ifndef OPENGOLD_UA_COMBAT_H
#define OPENGOLD_UA_COMBAT_H

#include "opengold/dungeon_battlefield.h"
#include "opengold/formats.h"
#include "opengold/ua_design.h"

#include <array>
#include <functional>
#include <vector>

// What a fight of an Unlimited Adventures design looks like (docs/games/ua.md,
// Fights): the kit's battlefields and its monsters' combat icons, for the
// view's CampaignEncounter.
namespace opengold::ua
{

// A battlefield, its tiles' pictures by tile number, and the colors the fight
// is drawn in (the combat icons' too).
struct FightField
{
    por::DungeonBattlefield field;
    std::vector<Image> terrain_art;
    std::array<VgaColor, 256> palette{};
};

[[nodiscard]] std::array<VgaColor, 256> combat_palette(const DesignFiles &files, unsigned set,
                                                       bool dungeon);

// A dungeon fight's field (cmbsetup.c, dungeon_map): the walls around the
// party's square (x, y) as DUNGCOM tiles of the area's dungeon combat set
// (header +0Ch; 0 is the first). Throws when the set is missing or does not
// decode.
[[nodiscard]] FightField dungeon_field(const DesignFiles &files, const Area &area, unsigned x,
                                       unsigned y);
// The shared library field layout with an engine's own map-edge decoder.
[[nodiscard]] FightField dungeon_field(const DesignFiles &files, unsigned set,
                                       const por::FieldEdges &edges);

// An outdoor fight's field (wild_map) of WILDCOM set `set` (0 is the first):
// open ground with trees, brush and stones scattered by `roll`, which gives 1
// to its argument. Throws when the set is missing or does not decode.
[[nodiscard]] FightField wilderness_field(const DesignFiles &files, unsigned set,
                                          const std::function<unsigned(unsigned)> &roll);

struct WildernessStyle
{
    unsigned tree_chance{5};
    bool trees{true}, river{};
    // Scree, brush, the unused tall-stone threshold, grass and small stones.
    std::array<unsigned, 5> scatter{0, 2, 5, 10, 5};
    unsigned art_tiles{38};
};

[[nodiscard]] FightField wilderness_field(const DesignFiles &files, unsigned set,
                                          const std::function<unsigned(unsigned)> &roll,
                                          const WildernessStyle &style);

// A monster's combat icon (CPIC, by its MONST number) in the fight's colors:
// its two poses and the squares it fills, from the icon's shape as the kit
// sizes it (evcmds.c, monster_add).
struct CombatIcon
{
    Image ready, action;
    rules::Footprint footprint;
};

// The kit's combat icons (CPIC.TLB, read once) and a design's own
// (CPIC1nnn.TLB in its folder, which take their place).
class CombatIcons
{
  public:
    // Throws when CPIC.TLB does not decode.
    explicit CombatIcons(DesignFiles files);

    // Icon `number`; nullopt when there is none. Throws when a design's own
    // does not decode.
    [[nodiscard]] std::optional<CombatIcon> icon(unsigned number,
                                                 const std::array<VgaColor, 256> &palette) const;
    // The squares icon `number` fills; one when there is none.
    [[nodiscard]] rules::Footprint footprint(unsigned number) const;

  private:
    struct Poses
    {
        Glyph ready, action;
    };

    [[nodiscard]] std::optional<Poses> poses(unsigned number) const;

    DesignFiles files_;
    std::optional<TileLibrary> kit_;
};

// The kit's code for an icon's squares (its record's +82h): 1 one square,
// 2 tall, 3 wide, 4 two by two.
[[nodiscard]] std::uint8_t icon_size_code(rules::Footprint footprint) noexcept;

} // namespace opengold::ua

#endif
