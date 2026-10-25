#ifndef OPENGOLD_DOS_PROBE_PNG_H
#define OPENGOLD_DOS_PROBE_PNG_H

#include <cstdint>
#include <filesystem>
#include <vector>

namespace opengold::probe
{
// Writes 8-bit RGB pixels as an uncompressed PNG.
void write_png(const std::filesystem::path &path, unsigned width, unsigned height,
               const std::vector<std::uint8_t> &rgb);
} // namespace opengold::probe

#endif
