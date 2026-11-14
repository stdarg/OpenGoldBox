#ifndef OPENGOLD_SAVE_SLOTS_H
#define OPENGOLD_SAVE_SLOTS_H
#include <godot_cpp/classes/window.hpp>
#include <exception>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "opengold/gold_box_game.h"

// The folder the game keeps a game's campaign saves in, and the name a player
// gave a save file there (empty for a file that is not one). Each game has its
// own, as each original had its SAVE folder; Pool of Radiance's stays where
// its saves always were.
[[nodiscard]] std::filesystem::path campaign_save_directory(opengold::GoldBoxGame game);
[[nodiscard]] std::string campaign_save_name(const std::filesystem::path &file);

// Scene-owned shared save UI. Callbacks remain in the campaign host.
class SaveSlots : public godot::Window
{
    GDCLASS(SaveSlots, godot::Window)
  public:
    void _ready() override;
    // Lists the saves of the game being played, or the review characters.
    void open(bool saving);
    // Shows a failed handler's error; called by presentation::run_guarded.
    void report_failure(const std::exception &failure);
    using FileAction = std::function<void(const std::filesystem::path &)>;
    // The campaign host's save and load, set once; both are required.
    void connect_host(FileAction save, FileAction load);
    // Keep review characters separate from normal campaign slots.
    void use_character_directory(std::filesystem::path directory);

  protected:
    static void _bind_methods()
    {
    }

  private:
    void prepare_dialog();
    // Private, so nothing else can replace or call them (Effective C++
    // Item 22).
    FileAction save_, load_;
    bool saving_{}, confirmed_{};
    bool character_saves_{};
    std::filesystem::path directory_, pending_;
    std::vector<std::filesystem::path> paths_;
    void select(std::int64_t index);
    void changed(godot::String text);
    void act();
    void close();
};
#endif
