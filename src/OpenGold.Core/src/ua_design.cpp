#include "opengold/ua_design.h"

#include "archive_io.h"
#include "opengold/path_text.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

namespace opengold::ua
{
namespace
{

using Bytes = std::vector<std::uint8_t>;

// The kit's data folders, in the order its disks are asked for.
constexpr std::array<std::string_view, 3> kit_disks{"DISK1", "DISK2", "DISK3"};

// The file named `name` in `folder`, whatever the case of its letters.
std::optional<std::filesystem::path> file_in(const std::filesystem::path &folder,
                                             std::string_view name)
{
    std::error_code error;
    if (folder.empty() || !std::filesystem::is_directory(folder, error))
    {
        return std::nullopt;
    }
    const auto wanted = por::detail::upper_ascii(name);
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.is_regular_file() &&
            por::detail::upper_ascii(path_text(entry.path().filename())) == wanted)
        {
            return entry.path();
        }
    }
    return std::nullopt;
}

Bytes read_file(const std::filesystem::path &path)
{
    const auto name = path_text(path);
    return por::detail::read_archive_bytes<std::runtime_error>(
        path, name + " is too large", "Cannot open " + name, "Cannot read " + name);
}

} // namespace

DesignFiles::DesignFiles(std::filesystem::path kit, std::filesystem::path design)
    : kit_(std::move(kit)), design_(std::move(design))
{
}

std::optional<std::filesystem::path> DesignFiles::find(std::string_view file) const
{
    if (auto own = file_in(design_, file))
    {
        return own;
    }
    for (const auto disk : kit_disks)
    {
        if (auto found = file_in(kit_ / disk, file))
        {
            return found;
        }
    }
    return std::nullopt;
}

std::optional<Bytes> DesignFiles::data(std::string_view name, unsigned entry) const
{
    std::array<char, 16> number{};
    std::snprintf(number.data(), number.size(), "%03u", entry);
    if (const auto loose = file_in(design_, std::string(name) + number.data() + ".DAT"))
    {
        return read_file(*loose);
    }
    const auto library = this->library(std::string(name) + ".GLB");
    if (!library)
    {
        return std::nullopt;
    }
    const auto index = decode_index(*library);
    if (!index)
    {
        throw std::runtime_error(std::string(name) + ".GLB has no index");
    }
    for (const auto &found : *index)
    {
        if (found.number == entry)
        {
            return library->items[found.item];
        }
    }
    return std::nullopt;
}

std::optional<Library> DesignFiles::library(std::string_view file) const
{
    const auto path = find(file);
    if (!path)
    {
        return std::nullopt;
    }
    auto library = decode_library(read_file(*path));
    if (!library)
    {
        throw std::runtime_error(path_text(*path) + " is not a resource library");
    }
    return library;
}

bool is_kit_folder(const std::filesystem::path &folder)
{
    if (!file_in(folder, "CKIT.EXE"))
    {
        return false;
    }
    for (const auto disk : kit_disks)
    {
        std::error_code error;
        bool found = false;
        for (const auto &entry : std::filesystem::directory_iterator(folder, error))
        {
            found = found || (entry.is_directory() &&
                              por::detail::upper_ascii(path_text(entry.path().filename())) == disk);
        }
        if (!found)
        {
            return false;
        }
    }
    return true;
}

bool is_design_folder(const std::filesystem::path &folder)
{
    // "NAME.DSN/" names the same folder.
    const auto named = folder.has_filename() ? folder : folder.parent_path();
    std::error_code error;
    return std::filesystem::is_directory(named, error) &&
           por::detail::upper_ascii(path_text(named.extension())) == ".DSN";
}

} // namespace opengold::ua
