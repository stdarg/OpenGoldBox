#ifndef OPENGOLD_TOOLS_CREATURE_DATA_H
#define OPENGOLD_TOOLS_CREATURE_DATA_H

#include "opengold/formats.h"
#include "opengold/png_decoder.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>

// What the Creature Stats Editor shows (dev_tools/creature_stats_editor): the
// repository's data/ and the player's install, read with the game's own
// loaders, and every value the game computes from them computed by the game's
// own code. Each query answers in JSON, which the editor reads as it is.
namespace opengold::creatures
{
class CreatureData
{
  public:
    // Throws std::runtime_error, with the game's own message, on data the game
    // would refuse.
    CreatureData(const std::filesystem::path &repository,
                 const std::filesystem::path &game_directory, por::PngDecoder decode);
    ~CreatureData();
    CreatureData(CreatureData &&) noexcept;
    CreatureData &operator=(CreatureData &&) noexcept;

    // Every original record an area converts: area, scripts, bank, record,
    // name, stat line, fight ratings, the original's Hit Points and AC, the
    // other records that share the stat line, and the entry as
    // conversions.json holds it.
    [[nodiscard]] std::string creatures() const;

    // The record of the area `script` belongs to as a fight fields it, at each
    // difficulty: AC, Hit Points, attacks, saves and spellcasting, from the
    // rules module; and each item it carries, with what a fight uses it for.
    [[nodiscard]] std::string creature(unsigned script, unsigned record) const;

    // The creature's combat art as the game's fights draw it, by the icon the
    // first fight of its area's scripts loads it with. None when no fight
    // loads it.
    [[nodiscard]] std::optional<Image> creature_art(unsigned script, unsigned record) const;

    // Every stat line in combat.rules: its key, Hit Points and spellcasting
    // profile.
    [[nodiscard]] std::string stat_lines() const;

    // Every LOAD MONSTER in the areas' scripts: its site, area, what the script
    // loads, the override, and the fight it is part of (the LOAD MONSTERs that
    // follow one another). A record or count the script reads from a variable
    // cannot be overridden.
    [[nodiscard]] std::string encounters() const;

    // What the game makes of the fight `site` is part of for a party of these
    // levels at the encounter challenge, at each difficulty: the XP budget and
    // each group's rating, loaded count and count met.
    [[nodiscard]] std::string fight(const std::string &site, std::span<const unsigned> levels,
                                    unsigned challenge) const;

    // Each companion built from companions.json as the game builds it, with the
    // entry as the file holds it.
    [[nodiscard]] std::string companions() const;

    // Every number the areas' scripts set an encounter chance with that an
    // entry of chances.json may change, with the entry that changes it.
    [[nodiscard]] std::string chances() const;

  private:
    struct Loaded;
    std::unique_ptr<const Loaded> loaded_;
};
} // namespace opengold::creatures
#endif
