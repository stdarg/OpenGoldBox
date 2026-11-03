#include "game_log.h"
#include "godot_path.h"
#include "opengold/diagnostic_log.h"
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <cstdlib>

using namespace godot;

namespace
{
constexpr auto logs_folder = "user://logs";
constexpr int kept_logs = 5;

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
    if (const char *chosen = std::getenv("OPENGOLD_LOG"); chosen && *chosen)
    {
        return;
    }
    DirAccess::make_dir_recursive_absolute(logs_folder);
    const auto now = Time::get_singleton()->get_datetime_string_from_system().replace(":", "-");
    const auto file = String(logs_folder).path_join("opengold-" + now + ".log");
    opengold::open_diagnostic_log(
        presentation::path_from_godot(ProjectSettings::get_singleton()->globalize_path(file)));
    remove_oldest_logs();
}
} // namespace game_log
