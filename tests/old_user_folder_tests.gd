extends SceneTree

# Run by old_user_folder.cmake: the game has started, so the old folder's files
# are already in the new one; this only confirms which folder Godot uses.
func _initialize() -> void:
    var folder := OS.get_user_data_dir()
    if not folder.ends_with("/OpenGold") or folder.contains("app_userdata"):
        push_error("User data folder is %s" % folder)
        quit(1)
        return
    print("Old user folder checks passed")
    quit(0)
