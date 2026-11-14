#ifndef OPENGOLD_UA_DESIGN_H
#define OPENGOLD_UA_DESIGN_H

#include "opengold/ua_formats.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

// An adventure made with Unlimited Adventures' construction kit: the files of
// its design folder (NAME.DSN), over the kit's own (docs/games/ua.md, Designs).
namespace opengold::ua
{

// Where a design's data comes from. The kit reads a numbered record from the
// design folder's loose file first ("GEO003.DAT", area 3), and from the
// kit's library otherwise ("GEO.GLB", whose index names the record for area
// 3); a library of pictures likewise from the design folder first, then from
// the kit's DISK1 to DISK3. Names are matched without regard to case.
class DesignFiles
{
  public:
    // `design` may be empty: the kit's own templates then make the design.
    DesignFiles(std::filesystem::path kit, std::filesystem::path design);

    // Record `entry` of the data `name` ("GEO", "GAME", "MONST"); nullopt
    // when neither the design nor the kit has it. Throws when a file exists
    // but cannot be read.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> data(std::string_view name,
                                                                unsigned entry) const;

    // The library `file` ("8X8DB.TLB"); nullopt when there is none. Throws
    // when it exists but does not decode.
    [[nodiscard]] std::optional<Library> library(std::string_view file) const;

    [[nodiscard]] const std::filesystem::path &kit() const noexcept
    {
        return kit_;
    }

    [[nodiscard]] const std::filesystem::path &design() const noexcept
    {
        return design_;
    }

  private:
    [[nodiscard]] std::optional<std::filesystem::path> find(std::string_view file) const;

    std::filesystem::path kit_, design_;
};

// The kit's folder holds CKIT.EXE and DISK1 to DISK3.
[[nodiscard]] bool is_kit_folder(const std::filesystem::path &folder);
// A design's folder, as the kit names it: NAME.DSN.
[[nodiscard]] bool is_design_folder(const std::filesystem::path &folder);

} // namespace opengold::ua

#endif
