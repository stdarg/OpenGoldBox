#ifndef OPENGOLD_AIL_FILES_H
#define OPENGOLD_AIL_FILES_H

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// The music files Matrix Cubed plays through John Miles' Audio Interface
// Library (docs/games/matrix-cubed.md, "Music"): its drivers, recognized and
// never run, its global timbre library, and the AdLib instrument bank and
// timbre catalog the library was built from, which the game never opens.
namespace opengold::ail
{

// A driver (*.ADV): its first word names the table of the functions it
// offers, (function number, offset) pairs ended by FFFFh; John Miles'
// copyright notice follows the word. Nothing is run.
struct Driver
{
    std::vector<std::array<std::uint16_t, 2>> functions;
};

[[nodiscard]] std::optional<Driver> decode_driver(std::span<const std::uint8_t> bytes);

// One timbre of a global timbre library (FATV9.AD): the index gives its patch,
// bank and offset, and the timbre starts with its size, that word included
// (xmidi.pas LoadGlobalTimbre).
struct GlobalTimbre
{
    std::uint8_t patch{}, bank{};
    std::vector<std::uint8_t> bytes;
};

// nullopt when the index does not end with bank FFh or a timbre runs past the end.
[[nodiscard]] std::optional<std::vector<GlobalTimbre>>
decode_global_timbre_library(std::span<const std::uint8_t> bytes);

// An AdLib instrument bank (FATV9.BNK): a header with "ADLIB-", a list of
// 12-byte names, each naming one of the 30-byte instruments that follow.
struct BankInstrument
{
    std::string name;
    std::uint16_t data_index{};
};

[[nodiscard]] std::optional<std::vector<BankInstrument>>
decode_instrument_bank(std::span<const std::uint8_t> bytes);

// A timbre catalog (FATV9.CAT), the text the library was built from: each
// line timbre(bank,patch) = BNK_inst("name",...) names a bank's instrument.
struct CatalogTimbre
{
    std::uint8_t bank{}, patch{};
    std::string instrument;
};

// nullopt when a timbre line cannot be read or the text is not plain ASCII.
[[nodiscard]] std::optional<std::vector<CatalogTimbre>>
decode_timbre_catalog(std::span<const std::uint8_t> bytes);

} // namespace opengold::ail

#endif
