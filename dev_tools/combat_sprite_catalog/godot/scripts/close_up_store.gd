extends RefCounted

# The Close-ups tab's files. Each animated close-up of the original (a PIC
# record) has a description in closeups/json/<folder>.json, and its new art,
# drawn only from that description, in closeups/art/<folder>/frame_<n>.png.
# The game never reads these: Export to game copies the art to
# data/art/closeups, where the game finds it.

const FORMAT := "opengold-closeup-v1"


static func tool_root() -> String:
    return ProjectSettings.globalize_path("res://../closeups").simplify_path()


static func game_folder() -> String:
    return ProjectSettings.globalize_path("res://../../../data/art/closeups").simplify_path()


static func json_path(folder: String, root: String = "") -> String:
    return (root if not root.is_empty() else tool_root()).path_join("json").path_join(folder + ".json")


static func art_folder(folder: String, root: String = "") -> String:
    return (root if not root.is_empty() else tool_root()).path_join("art").path_join(folder)


# The saved title and description, or empty ones for a record not yet saved.
static func load_record(row: Dictionary, root: String = "") -> Dictionary:
    var path := json_path(str(row.folder), root)
    if not FileAccess.file_exists(path):
        return {"title": "", "description": ""}
    var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(path))
    if not parsed is Dictionary or parsed.get("key") != row.key:
        return {"title": "", "description": "", "error": "Cannot read " + path}
    return {"title": str(parsed.get("title", "")), "description": str(parsed.get("description", ""))}


static func save_record(row: Dictionary, title: String, description: String, root: String = "") -> String:
    var path := json_path(str(row.folder), root)
    if DirAccess.make_dir_recursive_absolute(path.get_base_dir()) != OK:
        return "Cannot create " + path.get_base_dir()
    var record := {"format": FORMAT, "key": row.key, "title": title, "description": description}
    var file := FileAccess.open(path, FileAccess.WRITE)
    if file == null:
        return "Cannot write " + path
    file.store_string(JSON.stringify(record, "  ") + "\n")
    file.close()
    return ""


static func _art_sets(root: String) -> Array:
    var art := root.path_join("art")
    var sets: Array = []
    if not DirAccess.dir_exists_absolute(art):
        return sets
    for folder in DirAccess.get_directories_at(art):
        var frames := Array(DirAccess.get_files_at(art.path_join(folder))).filter(
            func(name): return name.begins_with("frame_") and name.ends_with(".png"))
        if not frames.is_empty():
            frames.sort()
            sets.append({"folder": folder, "frames": frames})
    return sets


static func _remove_tree(path: String) -> void:
    for folder in DirAccess.get_directories_at(path):
        _remove_tree(path.path_join(folder))
    for name in DirAccess.get_files_at(path):
        DirAccess.remove_absolute(path.path_join(name))
    DirAccess.remove_absolute(path)


# Replaces data/art/closeups's folders with the art there is now. The game
# checks each set as it plays it and shows the original for one it cannot use;
# the tab shows why. Returns an error message, or "".
static func export_all(root: String = "", target: String = "") -> String:
    var tool := root if not root.is_empty() else tool_root()
    var destination := target if not target.is_empty() else game_folder()
    if DirAccess.make_dir_recursive_absolute(destination) != OK:
        return "Cannot create " + destination
    for folder in DirAccess.get_directories_at(destination):
        _remove_tree(destination.path_join(folder))
    for art_set in _art_sets(tool):
        var to := destination.path_join(art_set.folder)
        if DirAccess.make_dir_recursive_absolute(to) != OK:
            return "Cannot create " + to
        for name in art_set.frames:
            var from := art_folder(art_set.folder, tool).path_join(name)
            if DirAccess.copy_absolute(from, to.path_join(name)) != OK:
                return "Cannot export " + from
    return ""


# Whether data/art/closeups holds exactly what an export would write now.
static func up_to_date(root: String = "", target: String = "") -> bool:
    var tool := root if not root.is_empty() else tool_root()
    var destination := target if not target.is_empty() else game_folder()
    var sets := _art_sets(tool)
    if Array(DirAccess.get_directories_at(destination)).size() != sets.size():
        return false
    for art_set in sets:
        var to := destination.path_join(art_set.folder)
        var written := Array(DirAccess.get_files_at(to))
        written.sort()
        if written != art_set.frames:
            return false
        for name in art_set.frames:
            if FileAccess.get_md5(art_folder(art_set.folder, tool).path_join(name)) != FileAccess.get_md5(to.path_join(name)):
                return false
    return true
