#include "game_log.h"
#include "application_settings.h"
#include "godot_path.h"
#include "opengold/diagnostic_log.h"
#include "opengoldbox_commit.h"
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <cstdlib>
#include <string>

using namespace godot;

namespace
{
constexpr auto logs_folder = "user://logs";
constexpr int kept_logs = 5;

std::string utf8(const String &text)
{
    return text.utf8().get_data();
}

void log_session(const String &what)
{
    opengold::log_diagnostic(opengold::LogPriority::notice, "session", utf8(what));
}

// What this session runs on, so a report can be reproduced.
void log_head()
{
    auto *os = OS::get_singleton(); // borrowed engine singleton
    const Dictionary engine = Engine::get_singleton()->get_version_info();
    log_session(String("OpenGoldBox ") + OPENGOLDBOX_VERSION + ", commit " + OPENGOLDBOX_COMMIT);
    log_session("engine " + String(engine["string"]) + "; " + os->get_name() + " " +
                os->get_version() + ", " + os->get_model_name() + ", " + os->get_processor_name());
    log_session("game files " + settings::game_path() + "; language " + settings::language());
    String abilities;
    for (const auto &ability : settings::monster_abilities_off())
    {
        abilities += (abilities.is_empty() ? "" : ", ") + String::utf8(ability.c_str());
    }
    log_session(
        "settings: difficulty " +
        String(
            opengold::difficulty_names[static_cast<std::size_t>(settings::difficulty())].data()) +
        ", encounter challenge " + String::num_uint64(settings::encounter_challenge()) +
        ", combat zoom " + String::num_int64(settings::combat_zoom_percent()) +
        "%, original wall art " + (settings::original_wall_art() ? "on" : "off") +
        ", original combat art " + (settings::original_combat_art() ? "on" : "off") +
        ", auto-equip " + (settings::auto_equip() ? "on" : "off") + ", unidentified magic " +
        (settings::skip_unidentified_magic() ? "left packed" : "equipped") +
        ", monster abilities off: " + (abilities.is_empty() ? "none" : abilities));
}

void remove_oldest_logs()
{
    const auto names = DirAccess::get_files_at(logs_folder);
    PackedStringArray ours;
    for (int64_t index = 0; index < names.size(); ++index)
    {
        if (names[index].begins_with("opengold-"))
        {
            ours.push_back(names[index]);
        }
    }
    ours.sort();
    for (int64_t index = 0; index < ours.size() - kept_logs; ++index)
    {
        DirAccess::remove_absolute(String(logs_folder).path_join(ours[index]));
    }
}
} // namespace

namespace game_log
{
void open()
{
    // The log reads OPENGOLD_LOG itself, when it is first used.
    if (const char *chosen = std::getenv("OPENGOLD_LOG"); !chosen || !*chosen)
    {
        DirAccess::make_dir_recursive_absolute(logs_folder);
        const auto now = Time::get_singleton()->get_datetime_string_from_system().replace(":", "-");
        const auto file = String(logs_folder).path_join("opengold-" + now + ".log");
        opengold::open_diagnostic_log(
            presentation::path_from_godot(ProjectSettings::get_singleton()->globalize_path(file)));
        remove_oldest_logs();
    }
    log_head();
    OS::get_singleton()->add_logger(memnew(EngineErrors));
}
} // namespace game_log

void EngineErrors::_log_error(const String &function, const String &file, int32_t line,
                              const String &code, const String &rationale, bool,
                              int32_t error_type, const TypedArray<Ref<ScriptBacktrace>> &)
{
    // Error types: 0 error, 1 warning, 2 script, 3 shader.
    const auto priority =
        error_type == 1 ? opengold::LogPriority::warning : opengold::LogPriority::error;
    const auto where = file + String(":") + String::num_int64(line) + String(" in ") + function;
    opengold::log_diagnostic(priority, error_type == 2 ? "script error" : "engine",
                             utf8((rationale.is_empty() ? code : rationale) + String(" (") + where + String(")")));
}

void EngineErrors::_log_message(const String &message, bool error)
{
    // Ordinary prints stay in the engine's log; printed errors come here too.
    if (error)
    {
        opengold::log_diagnostic(opengold::LogPriority::error, "engine",
                                 utf8(message.strip_edges()));
    }
}
