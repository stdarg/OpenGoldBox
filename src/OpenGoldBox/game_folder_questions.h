#ifndef OPENGOLDBOX_GAME_FOLDER_QUESTIONS_H
#define OPENGOLDBOX_GAME_FOLDER_QUESTIONS_H

#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/window.hpp>
#include <exception>
#include <functional>
#include <vector>

#include "opengold/gold_box_game.h"

// The questions that settle the folders a game plays from (docs/TECH.md,
// "Finding the games' folders"): a folder the search found, shown for the
// player to confirm, else the folder dialog, and an Unlimited Adventures
// design. Startup asks them before a game starts, and the game before it loads
// another game's save, from the same windows (scenes/game_folder_questions.tscn).
class GameFolderQuestions : public godot::Node
{
    GDCLASS(GameFolderQuestions, godot::Node)
  public:
    // A failed handler is already in the log, and the questions show their
    // own problems; called by presentation::run_guarded.
    void report_failure(const std::exception &) noexcept
    {
    }

    // What a game needs settled before it plays.
    struct Request
    {
        opengold::GoldBoxGame game{};
        // A new Unlimited Adventures game asks which design it plays; a save
        // names its own.
        bool design{};
        // A check's --game=UA plays the saved design while it is usable.
        bool saved_design_kept{};
    };

    using Outcome = std::function<void()>;
    // Asks for the request's game's folder and, unless the player chose it,
    // Pool of Radiance's (the party's portraits and combat art come from its
    // files, docs/GAMES.md), then its design; calls ready once every answer is
    // known, or back when the player closes a question.
    void ask(Request request, Outcome ready, Outcome back);
    // Whether one of the questions is on screen.
    [[nodiscard]] bool asking() const;
    // Whether the player chose game's folder and it still holds the game's
    // files: a folder the search found is asked about first.
    [[nodiscard]] static bool folder_chosen(opengold::GoldBoxGame game);

  protected:
    static void _bind_methods()
    {
    }

  private:
    void prepare();
    void connect_escape(godot::Window &dialog);
    void dialog_input(const godot::Ref<godot::InputEvent> &event);
    void next_folder();
    void close();
    void quit_game();
    void confirm_found_folder();
    void use_found_folder();
    void choose_another_folder();
    void ask_design();
    void show_path(const godot::String &message = {});
    void browse_path();
    void picked_path(const godot::String &directory);
    void path_edited(const godot::String &directory);
    void submitted_path(const godot::String &directory);
    void accept_path();
    void check_path();
    void continue_path();

    [[nodiscard]] bool asking_design() const
    {
        return folders_needed_.empty() && design_needed_;
    }

    bool prepared_{};
    // The games whose folders are still needed, the first being asked now.
    std::vector<opengold::GoldBoxGame> folders_needed_;
    bool design_needed_{};
    bool saved_design_kept_{};
    godot::String pending_path_;
    bool save_pending_path_{};
    Outcome ready_, back_;
};

#endif
