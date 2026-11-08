#ifndef OPENGOLDBOX_GAME_LOG_H
#define OPENGOLDBOX_GAME_LOG_H

#include <godot_cpp/classes/logger.hpp>
#include <godot_cpp/classes/script_backtrace.hpp>

// The game's own log (opengold/diagnostic_log.h): what the session runs on, every
// die rolled with what it was for, the player's actions, every line and move of
// each fight, the key decisions and the errors met, the engine's and scripts'
// included. Each session writes user://logs/opengold-<date>.log beside the
// engine's logs, and the newest five are kept. OPENGOLD_LOG names another file
// instead.
namespace game_log
{
// Opens the log and writes its head: the game's version and commit, the engine,
// the system, the game files' folder and the settings.
void open();

// Stops copying the engine's errors. The engine keeps its loggers past the
// game library's unloading, so one left behind crashes the engine's exit.
void close();
} // namespace game_log

// Copies the engine's errors and warnings, GDScript's included, into the game's
// log. The engine keeps writing them to its own log as well.
class EngineErrors : public godot::Logger
{
    GDCLASS(EngineErrors, godot::Logger)

  protected:
    static void _bind_methods()
    {
    }

  public:
    void _log_error(const godot::String &function, const godot::String &file, int32_t line,
                    const godot::String &code, const godot::String &rationale, bool editor_notify,
                    int32_t error_type,
                    const godot::TypedArray<godot::Ref<godot::ScriptBacktrace>> &script_backtraces)
        override;
    void _log_message(const godot::String &message, bool error) override;
};

#endif
