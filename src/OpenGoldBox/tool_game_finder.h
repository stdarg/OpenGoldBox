#ifndef OPENGOLDBOX_TOOL_GAME_FINDER_H
#define OPENGOLDBOX_TOOL_GAME_FINDER_H

#include "opengold/gold_box_game.h"
#include <filesystem>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/string.hpp>

// The game finder (opengold/game_finder.h) for the demos' and dev tools' Godot
// projects, which run from the source checkout without the game's extension:
// they look where the game looks, the environment's folders and then upward
// from the checkout. The game itself asks settings::Settings::game_folder.
namespace presentation
{
// The game's folder, or empty when no folder holds its files.
[[nodiscard]] std::filesystem::path tool_game_folder(opengold::GoldBoxGame game);
} // namespace presentation

// GDScript's way in: GameFinder.game_folder("POOLRAD"), the name as
// opengold::game_folder gives it, like StartupView.game_folder in the game;
// "" when no folder holds the game's files.
class GameFinder : public godot::Object
{
    GDCLASS(GameFinder, godot::Object)

  public:
    [[nodiscard]] static godot::String game_folder(const godot::String &name);

  protected:
    static void _bind_methods();
};

#endif
