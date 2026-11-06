extends RefCounted

# The Close-ups tab's part of --catalog-check. It needs the original game files.
# Returns an error message, or "".

const CloseUpStore = preload("res://scripts/close_up_store.gd")
const CloseUpPanel = preload("res://scripts/close_up_panel.gd")


static func _remove_tree(path: String) -> void:
    for folder in DirAccess.get_directories_at(path):
        _remove_tree(path.path_join(folder))
    for name in DirAccess.get_files_at(path):
        DirAccess.remove_absolute(path.path_join(name))
    DirAccess.remove_absolute(path)


static func _write_frame(path: String, size: Vector2i) -> void:
    DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    var image := Image.create(size.x, size.y, false, Image.FORMAT_RGBA8)
    image.fill(Color(0.2, 0.4, 0.6))
    image.save_png(path)


static func _pick(panel, key: String) -> bool:
    panel.filter.text = key
    panel._rebuild_list()
    for n in range(panel.visible_rows.size()):
        if panel.rows[panel.visible_rows[n]].key == key:
            panel.list.select(n)
            panel._select(n)
            return true
    return false


static func run(catalog: SpriteCatalog) -> String:
    var rows: Array = catalog.close_ups()
    if rows.is_empty():
        return "No animated close-ups were found"
    for row in rows:
        if row.held.size() < 2 or row.frames.size() != row.delays.size():
            return "%s is listed but does not animate" % row.key
        var saved: Dictionary = CloseUpStore.load_record(row)
        if saved.title.is_empty() or saved.description.length() < 80:
            return "%s has no saved title and description" % row.key
        var art: Dictionary = catalog.close_up_art(CloseUpStore.art_folder(row.folder), row.delays)
        if not art.problem.is_empty():
            return "%s's new art cannot be used: %s" % [row.key, art.problem]
    var orc: Array = rows.filter(func(row): return row.key == "PIC2.DAX:4")
    if orc.is_empty() or Array(orc[0].held) != [0, 2]:
        return "The Slums orc (PIC2.DAX:4) holds its two poses"
    if catalog.close_up_frame_at(orc[0].delays, 0.5) != 0 or catalog.close_up_frame_at(orc[0].delays, 16 / 18.2) != 2:
        return "The orc's close-up plays with the game's timing"
    if not CloseUpStore.up_to_date():
        return "data/art/closeups is out of date: run --export-to-game"

    var temp := ProjectSettings.globalize_path("user://close-up-check-%d" % Time.get_ticks_usec())
    var panel = CloseUpPanel.new()
    panel.set_catalog(catalog, temp)
    var error := _check_panel(panel, temp)
    panel.free()
    _remove_tree(temp)
    return error


static func _check_panel(panel, temp: String) -> String:
    if not _pick(panel, "PIC2.DAX:4"):
        return "The filter finds PIC2.DAX:4"
    if panel.shown_frames.size() != 4 or panel.preview_status.text.contains("New art is ready"):
        return "The original orc plays all its frames"
    panel.show_new.button_pressed = true
    if not panel.shown_frames.is_empty() or not panel.preview_status.text.begins_with("No new animation yet"):
        return "With no new art the toggle says so: " + panel.preview_status.text
    var folder := CloseUpStore.art_folder("pic2_dax_4", temp)
    _write_frame(folder.path_join("frame_0.png"), Vector2i(352, 422))
    panel._load_frames()
    if panel.preview_status.text != "The new art cannot be used: frame_1.png is missing.":
        return "A missing frame is named: " + panel.preview_status.text
    _write_frame(folder.path_join("frame_1.png"), Vector2i(352, 352))
    panel._load_frames()
    if panel.preview_status.text != "The new art cannot be used: frame_1.png is not 352 x 422.":
        return "A frame of the wrong size is named: " + panel.preview_status.text
    _write_frame(folder.path_join("frame_1.png"), Vector2i(352, 422))
    panel._load_frames()
    if panel.shown_frames.size() != 2 or Array(panel.shown_delays) != [15, 7]:
        return "The new orc plays its two frames with the original's timing"
    panel.show_new.button_pressed = false
    if panel.shown_frames.size() != 4 or not panel.preview_status.text.contains("New art is ready"):
        return "Turning the toggle off plays the original again"

    panel.title_edit.text = "Check title"
    panel.description_edit.text = "Check description"
    panel._store_draft()
    panel._save()
    var saved: Dictionary = CloseUpStore.load_record(panel.rows[panel.selected], temp)
    if saved.title != "Check title" or saved.description != "Check description":
        return "A saved description reads back"
    var target := temp.path_join("export")
    if not CloseUpStore.export_all(temp, target).is_empty() or not CloseUpStore.up_to_date(temp, target):
        return "Export copies the new art"
    if Array(DirAccess.get_files_at(target.path_join("pic2_dax_4"))) != ["frame_0.png", "frame_1.png"]:
        return "Export writes the record's frames"
    return ""
