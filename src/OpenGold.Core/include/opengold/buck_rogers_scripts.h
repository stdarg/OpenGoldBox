#ifndef OPENGOLD_BUCK_ROGERS_SCRIPTS_H
#define OPENGOLD_BUCK_ROGERS_SCRIPTS_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "opengold/gold_box_game.h"

// What the Buck Rogers games' scripts ask of the engine, read from every
// command a script can reach (BUCK-ROGERS.md, step B1; docs/games/countdown.md
// and matrix-cubed.md, "Scripts"). Nothing runs: a value a command reads from
// memory is traced back to the literals the script saved there.
namespace opengold
{

struct ScriptPlace
{
    std::string archive; // ECL1.DAX ...
    unsigned script{};
    std::uint16_t address{};
};

// The literals a cell can hold where a command reads it, from the SAVEs on
// every path that reaches the command. complete is false when some path sets
// the cell otherwise, or reaches the command from a script's start.
struct TracedValues
{
    std::set<unsigned> values;
    bool complete{true};
};

// SKILL CHECK (22h), SKILL CHECK WITH TEXT (23h) or SKILL CHECK WITH DAMAGE
// (49h). The difficulty scales the skill: 0 four times, 1 twice, 2 as it is,
// 3 a half, 4 a quarter (charhlp.pas SelectSkillCharacter).
struct SkillCheckUse
{
    ScriptPlace place;
    std::uint8_t command{};
    TracedValues skill, difficulty;
    // Non-zero: the party's best member rolls; zero: the current character.
    TracedValues whole_party;
};

// Any other command the listing names, with its operands as the script
// gives them: a number, or a cell such as [4D1Eh].
struct ScriptCommandUse
{
    ScriptPlace place;
    std::string command;
    std::vector<std::string> operands;
};

// A NEW ECL and the disk (data set, cell 7F12h) it loads the script from.
struct ScriptLoad
{
    ScriptPlace place;
    TracedValues script, data_set;
};

struct BuckRogersScriptListing
{
    std::vector<SkillCheckUse> skill_checks;
    std::vector<ScriptCommandUse> ship_combats;
    // PICTURE MODE 2, or the area kind 70h written to cell 4BE6h.
    std::vector<ScriptCommandUse> solar_maps;
    std::vector<ScriptCommandUse> shops;
    // Every write to the party ship's fuel (cell 4D1Eh, TShip's fuel status).
    std::vector<ScriptCommandUse> fuel_writes;
    std::vector<ScriptCommandUse> programs;
    std::vector<ScriptLoad> script_loads;
    // Commands right after an IF that the original's skip (eclmenu.pas
    // SkipEclCmd) steps over by a length other than the command's own.
    std::vector<ScriptCommandUse> uneven_skips;
    // Reachable 3Bh and 3Ch, which never end.
    std::vector<ScriptCommandUse> stalls;
};

// Every script of a Buck Rogers game's folder. Throws std::runtime_error when
// an archive or script cannot be read.
[[nodiscard]] BuckRogersScriptListing list_buck_rogers_scripts(const std::filesystem::path &folder,
                                                               GoldBoxGame game);

} // namespace opengold

#endif
