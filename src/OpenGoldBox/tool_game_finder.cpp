#include "tool_game_finder.h"
#include "godot_game_search.h"
#include "godot_path.h"
#include "opengold/game_finder.h"
#include "opengold/path_text.h"
#include <godot_cpp/classes/project_settings.hpp>
#include <string>

using namespace godot;

namespace presentation
{
std::filesystem::path tool_game_folder(opengold::GoldBoxGame game)
{
    const auto project =
        path_from_godot(ProjectSettings::get_singleton()->globalize_path("res://"));
    const auto search = godot_game_search(opengold::checkout_folder(project));
    return opengold::find_game_folder(game, search).value_or(std::filesystem::path());
}
} // namespace presentation

void GameFinder::_bind_methods()
{
    ClassDB::bind_static_method("GameFinder", D_METHOD("game_folder", "name"),
                                &GameFinder::game_folder);
}

String GameFinder::game_folder(const String &name)
{
    for (const auto game : opengold::gold_box_games)
    {
        if (name == String(std::string(opengold::game_folder(game)).c_str()))
        {
            return String::utf8(opengold::path_text(presentation::tool_game_folder(game)).c_str());
        }
    }
    return {};
}
