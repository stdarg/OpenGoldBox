#include "old_user_folder.h"
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace
{
// Moves each entry of old_folder that new_folder lacks; a folder both have is
// merged the same way. A file both have stays where it is, so nothing is lost.
void merge_folder(const String &old_folder, const String &new_folder)
{
    DirAccess::make_dir_recursive_absolute(new_folder);
    for (const auto &name : DirAccess::get_directories_at(old_folder))
    {
        const auto old_path = old_folder.path_join(name);
        const auto new_path = new_folder.path_join(name);
        if (DirAccess::dir_exists_absolute(new_path))
        {
            merge_folder(old_path, new_path);
            DirAccess::remove_absolute(old_path); // only succeeds once empty
            continue;
        }

        if (DirAccess::rename_absolute(old_path, new_path) != OK)
        {
            UtilityFunctions::printerr("Cannot move ", old_path, " to ", new_path);
        }
    }

    for (const auto &name : DirAccess::get_files_at(old_folder))
    {
        const auto new_path = new_folder.path_join(name);
        if (FileAccess::file_exists(new_path))
        {
            continue;
        }

        if (DirAccess::rename_absolute(old_folder.path_join(name), new_path) != OK)
        {
            UtilityFunctions::printerr("Cannot move ", old_folder.path_join(name), " to ",
                                       new_path);
        }
    }
}
} // namespace

void move_old_user_folder()
{
    auto *os = OS::get_singleton(); // borrowed engine singleton
    if (os->get_name() != "Windows")
    {
        return;
    }

    // The new folder is <APPDATA>\OpenGold, so the old one sits beside it.
    const auto new_folder = os->get_user_data_dir();
    const auto app_userdata = new_folder.get_base_dir().path_join("Godot/app_userdata");
    const auto old_folder = app_userdata.path_join("OpenGold");
    if (!DirAccess::dir_exists_absolute(old_folder))
    {
        return;
    }

    merge_folder(old_folder, new_folder);
    // Each removal only succeeds on an empty folder; other Godot programs may
    // still keep theirs under Godot\app_userdata.
    DirAccess::remove_absolute(old_folder);
    DirAccess::remove_absolute(app_userdata);
    DirAccess::remove_absolute(app_userdata.get_base_dir());
}
