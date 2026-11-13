#ifndef OPENGOLD_TREASURES_TEXTS_H
#define OPENGOLD_TREASURES_TEXTS_H

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// The tables and texts Treasures of the Savage Frontier keeps among its
// pictures, in BORD.DAX and LUCK.DAX (docs/games/treasure.md).
namespace opengold::treasures
{

// BORD.DAX record 7: the runs of 8x8 border tiles the screen frames are drawn
// from, each a list of tile numbers ended by FEh, where FFh leaves a cell
// alone. The runs come without their FEh; nullopt when the record does not end
// a run.
[[nodiscard]] std::optional<std::vector<std::vector<std::uint8_t>>>
decode_border_runs(std::span<const std::uint8_t> record);

// BORD.DAX records 9 (160 effect names of up to 37 letters) and 10 (255 item
// words of up to 20): strings of `capacity` letters after a length byte.
// nullopt when the record is not whole strings or a string is too long or
// holds something other than printable letters.
[[nodiscard]] std::optional<std::vector<std::string>>
decode_name_table(std::span<const std::uint8_t> record, std::size_t capacity);

// A LUCK.DAX record: the news a luck reading tells, a lucky paper's words or a
// romance scene, ended by the end-of-file mark 1Ah. Besides printable letters
// it holds line ends (13, 10), 12 to wait for a key, and 2 to 8 for the names
// and pronouns of the people in a romance scene. The text without its 1Ah;
// nullopt for anything else.
[[nodiscard]] std::optional<std::string> decode_luck_text(std::span<const std::uint8_t> record);

} // namespace opengold::treasures

#endif
