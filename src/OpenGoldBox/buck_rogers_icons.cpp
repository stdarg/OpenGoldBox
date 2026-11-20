#include "buck_rogers_icons.h"
#include "application_settings.h"
#include "godot_images.h"
#include "godot_path.h"
#include "opengold/creature_art.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <vector>

namespace presentation
{
namespace
{
// The folder's CHARS.DAX, whatever the case of its name; empty without it.
std::vector<std::uint8_t> read_body_icons(const std::filesystem::path &folder)
{
    std::error_code error;
    for (const auto &entry : std::filesystem::directory_iterator(folder, error))
    {
        auto name = entry.path().filename().string();
        std::ranges::transform(name, name.begin(),
                               [](unsigned char c)
                               {
                                   return static_cast<char>(std::toupper(c));
                               });
        if (name == "CHARS.DAX")
        {
            std::ifstream in(entry.path(), std::ios::binary);
            return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        }
    }
    return {};
}
} // namespace

BodyIcon buck_rogers_body_icon(unsigned number)
{
    const auto game = settings::chosen_game();
    const auto folder = path_from_godot(settings::game_folder(game));
    // The pictures are kept, not their textures: a Godot resource must not
    // outlive the engine, as a static one would.
    static std::map<std::filesystem::path, std::vector<std::uint8_t>> archives;
    static std::map<std::pair<std::filesystem::path, unsigned>, std::optional<opengold::CombatArt>>
        icons;
    auto found = icons.find({folder, number});
    if (found == icons.end())
    {
        auto archive = archives.find(folder);
        if (archive == archives.end())
        {
            archive = archives.emplace(folder, read_body_icons(folder)).first;
        }
        found = icons
                    .emplace(std::pair{folder, number},
                             archive->second.empty()
                                 ? std::nullopt
                                 : opengold::por::creature_art(archive->second, number, nullptr,
                                                               false, game))
                    .first;
    }
    BodyIcon icon;
    if (const auto &art = found->second)
    {
        icon.ready = image_texture(art->image);
        if (art->action)
        {
            icon.action = image_texture(*art->action);
        }
    }
    return icon;
}
} // namespace presentation
