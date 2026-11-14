#ifndef OPENGOLD_VACE_FILM_H
#define OPENGOLD_VACE_FILM_H

#include "opengold/vga_picture.h"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// The opening and ending films of Pools of Darkness, INTRO and FINAL, which
// the original plays with its separate program VACE.EXE: a control file
// (.ACF) that sets colors and paces the film, and the frames (.BMA) it draws
// one after the other. The layouts: docs/games/darkness.md, "The opening and ending films".
namespace opengold
{

inline constexpr std::uint16_t vace_film_width = 320;
inline constexpr std::uint16_t vace_film_height = 200;

// A song a film plays: a song file of the game's folder (P1.SMS) and the
// song's number in it.
struct VaceSong
{
    std::string file;
    std::uint8_t number{};
};

// One step of the film: the whole screen as it stands after the step's frame
// is drawn over the screen before it.
struct VaceFilmFrame
{
    // vace_film_width x vace_film_height color indices, row by row.
    std::vector<std::uint8_t> pixels;
    std::array<VgaColor, 256> palette{};
    // How long the frame stays before the next one, or before the screen
    // clears for the next scene, at the pace VACE keeps while music plays.
    std::uint32_t hold_milliseconds{};
    // A key while the frame shows ends the film.
    bool key_ends_film{};
    // The song the film starts as the frame is drawn.
    std::optional<VaceSong> starts_song;
};

// A film's control file and frames, every step of it whole. nullopt when the
// control file does not parse, the frame blocks do not match its list of
// them, a frame does not unpack inside the screen or the film asks for more
// frames than the .BMA holds.
[[nodiscard]] std::optional<std::vector<VaceFilmFrame>>
decode_vace_film(std::span<const std::uint8_t> control, std::span<const std::uint8_t> frames);

} // namespace opengold

#endif
