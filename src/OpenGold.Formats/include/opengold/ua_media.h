#ifndef OPENGOLD_UA_MEDIA_H
#define OPENGOLD_UA_MEDIA_H

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "opengold/vga_picture.h"

// The common formats of Unlimited Adventures' files (docs/games/ua.md): the art
// templates in ART (IFF ILBM and PCX pictures), the sound effects (Creative
// Voice) and the music (Miles' extended MIDI).
namespace opengold::ua
{

// A whole-screen picture with its 256 colors, reduced to the VGA's 0..63.
struct ScreenPicture
{
    VgaImage image;
    VgaPaletteRange palette;
};

// An IFF ILBM picture of 8 bit planes, unpacked or ByteRun1-packed.
[[nodiscard]] std::optional<ScreenPicture> decode_ilbm(std::span<const std::uint8_t> bytes);

// A version 5 PCX picture of one 8-bit plane, run-length coded, with its 256
// colors at the end.
[[nodiscard]] std::optional<ScreenPicture> decode_pcx(std::span<const std::uint8_t> bytes);

// A run of sound in a Creative Voice file, and the marker before it: the game
// plays effect n from marker n.
struct VoiceSection
{
    std::optional<std::uint16_t> marker;
    std::uint32_t rate{};              // Samples a second.
    std::vector<std::uint8_t> samples; // 8-bit unsigned, 128 the rest level.
};

// Sound data may be 8-bit or Creative's 4-bit ADPCM; nullopt for other packings,
// an unknown block or a block that runs past the end.
[[nodiscard]] std::optional<std::vector<VoiceSection>>
decode_voice_file(std::span<const std::uint8_t> bytes);

// One event of an XMI sequence. Times are in the format's 120 ticks a second.
// A note-on carries its duration in place of a note-off.
struct XmiEvent
{
    std::uint32_t tick{};
    std::uint8_t status{}; // 80h to EFh a channel event, F0h or F7h system exclusive, FFh meta.
    std::uint8_t data1{};  // The note, controller or program; a meta event's type.
    std::uint8_t data2{};
    std::uint32_t duration{};
    std::vector<std::uint8_t> payload; // A meta or system exclusive event's bytes.
};

struct XmiSequence
{
    // The (patch, bank) timbres the sequence asks the driver to load.
    std::vector<std::array<std::uint8_t, 2>> timbres;
    std::vector<XmiEvent> events;
};

// Every sequence of an XMI file (FORM XDIR, then CAT XMID); nullopt when the
// chunks or an event do not add up or a sequence does not end.
[[nodiscard]] std::optional<std::vector<XmiSequence>>
decode_xmi(std::span<const std::uint8_t> bytes);

} // namespace opengold::ua

#endif
