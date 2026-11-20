extends SceneTree
var fixture_dir := ""
var original_game_dir := OS.get_environment("OPENGOLD_GAME_DIR")
func _initialize() -> void:
    call_deferred("run_checks")
func require(ok: bool, message: String) -> void:
    if not ok:
        push_error(message)
        quit(1)
        assert(ok, message)

func settle() -> void:
    for frame in range(4):
        await process_frame

# Wholly authored indexed pixels exercise real DAX decoding without original art.
func picture(width: int, height: int, frames: int, seed: int, shape := 0) -> PackedByteArray:
    var record := PackedByteArray()
    record.resize(17)
    record.encode_u16(0, height)
    record.encode_u16(2, width / 8)
    record[8] = frames
    for frame in range(frames):
        for y in range(height):
            for x in range(0, width, 2):
                var pair := 0
                for dx in range(2):
                    var pixel := (x + dx + y + seed + frame) % 15 + 1
                    if shape > 0:
                        var inset := 7 if shape == 1 else 2
                        if x + dx < inset or x + dx >= width - inset or y < (3 if shape == 1 else 0):
                            pixel = 0
                    pair = (pair << 4) | pixel
                record.append(pair)
    return record

func write_archive(name: String, records: Dictionary) -> void:
    var header := PackedByteArray()
    header.resize(2 + records.size() * 9)
    header.encode_u16(0, records.size() * 9)
    var payload := PackedByteArray()
    var entry := 2
    for id in records:
        var raw: PackedByteArray = records[id]
        var encoded := PackedByteArray()
        for offset in range(0, raw.size(), 128):
            var length := mini(128, raw.size() - offset)
            encoded.append(length - 1)
            encoded.append_array(raw.slice(offset, offset + length))
        header[entry] = id
        header.encode_u32(entry + 1, payload.size())
        header.encode_u16(entry + 5, raw.size())
        header.encode_u16(entry + 7, encoded.size())
        payload.append_array(encoded)
        entry += 9
    var file := FileAccess.open(fixture_dir.path_join(name), FileAccess.WRITE)
    require(file != null, "Fixture archive opens")
    file.store_buffer(header)
    file.store_buffer(payload)
    file.close()

func create_fixtures() -> void:
    fixture_dir = ProjectSettings.globalize_path("res://../../build/sprite-fixtures/%s" % OS.get_process_id())
    require(DirAccess.make_dir_recursive_absolute(fixture_dir) == OK, "Fixture directory created")
    for disk in range(1, 9):
        write_archive("HEAD%d.DAX" % disk, {1: picture(88, 40, 1, 0)})
        write_archive("BODY%d.DAX" % disk, {1: picture(88, 48, 1, 0)})
    for part in ["CHEAD", "CBODY"]:
        var records := {}
        for bank in [0, 64, 128, 192]:
            for id in range(14 if part == "CHEAD" else 32):
                records[bank + id] = picture(24, 8 if part == "CHEAD" else 24, 1,
                    id + (2 if bank >= 128 else 0), 2 if bank % 128 == 64 else 1)
        write_archive(part + ".DAX", records)
    write_archive("DUNGCOM.DAX", {1: picture(24, 24, 25, 0)})
    var monsters := {}
    for id in [0, 2, 4, 26, 31]:
        for pose in [0, 128]:
            monsters[id + pose] = picture(48 if id == 26 else 24, 48 if id == 31 else 24, 1, id + pose, 2)
    write_archive("CPIC2.DAX", monsters)
    # The file only Pool of Radiance has, so the game finder takes this folder for its.
    write_archive("SQRPACI.DAX", {})
    OS.set_environment("OPENGOLD_GAME_DIR", fixture_dir)

func cleanup() -> void:
    OS.set_environment("OPENGOLD_GAME_DIR", original_game_dir)
    if not fixture_dir.is_empty():
        for name in DirAccess.get_files_at(fixture_dir):
            DirAccess.remove_absolute(fixture_dir.path_join(name))
        DirAccess.remove_absolute(fixture_dir)


func reviewer(path: String) -> Control:
    var view = load("res://scenes/combat_body_review.tscn").instantiate()
    view.catalog_path = path
    root.add_child(view)
    return view

func run_checks() -> void:
    if OS.get_cmdline_user_args().has("--visual"):
        root.size = Vector2i(1280, 720)
        var view = reviewer("res://../../data/art/combat-body-looks.tsv")
        await settle()
        view._step(1)
        view.filter_box.text = "short bow"
        view._fill_list()
        await settle()
        require(view.status.get_global_rect().end.y <= root.size.y, "Save status fits launcher window")
        for preview in view.previews:
            require(preview.get_global_rect().end.y <= root.size.y, "All previews fit launcher window")
        await RenderingServer.frame_post_draw
        require(root.get_texture().get_image().save_png("res://../../build/combat-body-checklist.png") == OK, "Visual capture saved")
        view._review_body(32)
        view.filter_box.text = "unarmed"
        view._fill_list()
        await settle()
        await RenderingServer.frame_post_draw
        require(root.get_texture().get_image().save_png("res://../../build/combat-body-unarmed-shield.png") == OK, "Derived body visual capture saved")
        for id in [7, 24, 33, 34]:
            view._review_body(id)
            await settle()
            await RenderingServer.frame_post_draw
            require(root.get_texture().get_image().save_png("res://../../build/combat-body-%d.png" % id) == OK, "Dagger comparison capture saved")
        for tab in range(1, 4):
            view.tabs.current_tab = tab
            await settle()
            await RenderingServer.frame_post_draw
            require(root.get_texture().get_image().save_png("res://../../build/combat-body-tab-%d.png" % tab) == OK, "Tab capture saved")
        view.queue_free()
        await settle()
        quit(0)
        return
    create_fixtures()
    var path := fixture_dir.path_join("assignments.tsv")
    var file := FileAccess.open(path, FileAccess.WRITE)
    for id in range(33):
        file.store_line("%d\t%s" % [id, "silver_23_shield,type_23_shield" if id == 4 else "type_43" if id == 1 else "unreviewed"])
    file.close()
    var view = reviewer(path)
    await settle()
    require(view.loaded, "Legacy catalog loads")
    require(view.assignments.size() == 35, "Derived body is in the reviewer")
    for id in [33, 34]:
        view._review_body(id)
        var key := "type_8" if id == 33 else "type_8_shield"
        view._toggle(key, true)
        require(view.assignments[id].has(key), "Dagger bodies accept assignments")
        require(FileAccess.get_file_as_string(path).contains("%d\t%s" % [id, key]), "Dagger assignment is saved")
        view._toggle(key, false)
        for bank in [0, 64, 128, 192]:
            var source: PackedByteArray = view.loader._extract_record(view.body_data, (7 if id == 33 else 24) + bank)
            var saved := source.duplicate()
            var dagger: PackedByteArray = view._with_dagger(source, bank)
            require(source == saved, "Original sword data stays intact")
            var changed := 0
            for pixel in range(576):
                var offset := 17 + (pixel >> 1)
                var before := source[offset] >> 4 if pixel % 2 == 0 else source[offset] & 15
                var after := dagger[offset] >> 4 if pixel % 2 == 0 else dagger[offset] & 15
                if before != after:
                    changed += 1
                    require(before in [7, 15] and after in [0, 15], "Only blade pixels change")
            require(changed > 0, "Blade is shortened in every size and pose")
    view._step(1)
    require(view.body_id == 0, "Navigation wraps after both dagger bodies")

    require(view.assignments[4] == ["type_23_shield"], "Silver normalized and deduplicated")
    view._step(1)
    view.filter_box.text = "short bow"
    view._fill_list()
    require(view.list.get_child_count() == 4, "Filter narrows visible checklist")
    var checkbox: CheckBox = view.list.get_child(2)
    require(checkbox.text == "Short Bow", "Filtered checkbox is identifiable")
    checkbox.button_pressed = true
    require(view.assignments[1].has("type_43") and view.assignments[1].has("type_44"), "Click adds shared bow association")
    require(view.summary.text.contains("Long Bow") and view.summary.text.contains("Short Bow"), "Summary includes hidden assignment")
    view._step(1)
    view._step(-1)
    require(view.list.get_child(2).button_pressed, "Navigation and filtering preserve checks")
    view.list.get_child(2).button_pressed = false
    require(view.assignments[1] == ["type_43"], "Unchecking short bow preserves long bow")
    view._toggle("type_44", true)
    view.queue_free()
    await settle()
    view = reviewer(path)
    await settle()
    require(view.assignments[1] == ["type_43", "type_44"], "Autosave survives reopening")
    require(view.assignments[4] == ["type_23_shield"], "Migration survives save and reopening")
    require(not FileAccess.get_file_as_string(path).contains("silver_"), "Saved catalog uses ordinary IDs")
    view._step(1)
    view._toggle("type_43", false)
    view._toggle("type_44", false)
    require(view.assignment.text.contains("Unreviewed"), "Empty body visibly unreviewed")
    require(view.tabs.get_tab_count() == 4, "Four reviewer tabs")
    view._toggle("type_38_shield", true)
    view._toggle("type_38", true)
    view._step(1)
    view._toggle("type_38_shield", true)
    view.report_filters[0].text = "Two-Handed"
    view.report_filters[1].text = "Two-Handed"
    view.report_filters[2].text = "Two-Handed"
    view._refresh_reports()
    require(view.report_counts[0].text == "0 of 93 combinations", "Assigned combinations disappear from unassigned list")
    require(view.report_lists[1].get_child_count() == 1, "Only duplicate combination is listed")
    var row: HBoxContainer = view.report_lists[1].get_child(0)
    require(row.get_meta("combination") == "type_38_shield", "Duplicate report identifies complete combination")
    require(row.get_child(1).get_child_count() == 2, "Both matching bodies have review buttons")
    view.tabs.current_tab = 2
    row.get_child(1).get_child(0).pressed.emit()
    require(view.tabs.current_tab == 0 and view.body_id == 1, "Body link opens correct review previews")
    view.tabs.current_tab = 3
    view.report_lists[2].get_child(1).get_child(0).pressed.emit()
    require(view.deleted.has("type_38_shield"), "Delete button removes combination")
    require(view._bodies_for("type_38_shield").is_empty(), "Deletion removes every association")
    require(view.assignments[1].has("type_38"), "Plain weapon survives shield combination deletion")
    require(view.report_counts[1].text.begins_with("0 of 0"), "Duplicate report refreshes after deletion")
    require(view.report_lists[2].get_child_count() == 1, "Deleted combination removed from management list")
    view.filter_box.text = "Two-Handed"
    view._fill_list()
    require(view.list.get_child_count() == 1, "Deleted combination removed from assignment checklist")
    view.queue_free()
    await settle()
    view = reviewer(path)
    await settle()
    require(view.deleted == ["type_38_shield"], "Deleted combination persists after reopening")
    require(view._bodies_for("type_38_shield").is_empty() and view.assignments[1].has("type_38"), "Deletion and surviving assignments persist together")
    view._toggle("type_38_shield", true)
    require(view._bodies_for("type_38_shield").is_empty(), "Deleted combination cannot be reassigned")
    var persisted := FileAccess.get_file_as_string(path)
    view.catalog_path = fixture_dir.path_join("missing-directory/assignments.tsv")
    view._delete_combination("type_38")
    require(not view.deleted.has("type_38") and view.assignments[1].has("type_38"), "Failed deletion save preserves current assignments and options")
    require(FileAccess.get_file_as_string(path) == persisted, "Failed deletion save preserves disk catalog")
    view.catalog_path = path
    view.queue_free()
    await settle()
    cleanup()
    print("Combat body reviewer checks passed")
    quit(0)
