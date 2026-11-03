#ifndef OPENGOLDBOX_GAME_LOG_H
#define OPENGOLDBOX_GAME_LOG_H

// The game's own log (opengold/diagnostic_log.h): every die rolled, with what it
// was for, every line of each fight's log, the key decisions and the errors met.
// Each session writes user://logs/opengold-<date>.log beside the engine's logs,
// and the newest five are kept. OPENGOLD_LOG names another file instead.
namespace game_log
{
void open();
} // namespace game_log

#endif
