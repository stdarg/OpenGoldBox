#ifndef OPENGOLD_GAME_CENSUS_H
#define OPENGOLD_GAME_CENSUS_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "opengold/gold_box_game.h"

// The reading census (docs/GAMES.md, phase 1): every file of a game's folder
// put through the decoders the game uses, to show what OpenGoldBox can read.
namespace opengold
{

// One kind of file, such as the scripts or the portraits' heads.
struct CensusRow
{
    std::string kind;
    std::vector<std::string> files;
    // The pieces examined (records, frames or files) and how many decoded.
    unsigned pieces{}, decoded{};
    // Where the first piece that did not decode is, and why.
    std::string first_failure;

    [[nodiscard]] bool complete() const noexcept
    {
        return decoded == pieces;
    }
};

struct GameCensus
{
    GoldBoxGame game{};
    std::vector<CensusRow> rows;
    // Files no decoder reads yet.
    std::vector<std::string> unread_files;
    // The original's programs and settings, which OpenGoldBox never reads.
    std::vector<std::string> program_files;

    [[nodiscard]] bool complete() const noexcept;
};

// The game whose files the folder holds, told apart by files only that game has.
[[nodiscard]] std::optional<GoldBoxGame> identify_game(const std::filesystem::path &folder);

[[nodiscard]] GameCensus take_census(const std::filesystem::path &folder, GoldBoxGame game);

} // namespace opengold

#endif
