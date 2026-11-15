#ifndef OPENGOLD_MUSIC_RENDER_H
#define OPENGOLD_MUSIC_RENDER_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "opengold/music_bank.h"

// The later games' music made audible: a song of a music bank played as the
// games' music driver plays it on an AdLib card, through FmSynth. How the
// driver plays a song: docs/games/later-sound.md, "Playing the music".
namespace opengold
{

// The bank a game hands its music driver as music `piece`: that record of
// SOUND.DAX in Secret of the Silver Blades, of MUSIC.DAX in the later games
// (the number a script's PLAY SOUND gives with bit 80h set). Throws
// std::runtime_error when the archive or the record cannot be read.
[[nodiscard]] MusicBank load_music_bank(const std::filesystem::path &game_directory,
                                        GoldBoxGame game, unsigned piece);

// An archive of a game's folder, read whole, by its name in either case.
// Throws std::runtime_error when it cannot be opened.
[[nodiscard]] std::vector<std::uint8_t> read_game_archive(const std::filesystem::path &directory,
                                                          const std::string &name);

// The piece a game plays over its title pictures; nullopt for the games
// without the music driver.
[[nodiscard]] std::optional<unsigned> title_music_piece(GoldBoxGame game) noexcept;

// One song of a bank as the driver plays it on an AdLib card, once through,
// with the instruments' last notes fading out after it: mono signed PCM at
// the given rate. A song number past the bank's songs gives no samples.
[[nodiscard]] std::vector<std::int16_t> render_music(const MusicBank &bank, std::size_t song,
                                                     unsigned sample_rate);

} // namespace opengold

#endif
