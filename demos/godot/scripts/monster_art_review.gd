extends Control

const SAVE_PATH := "user://monster-art-names.json"
const REVIEW_PATH := "user://monster-art-decisions.json"
const ASSOCIATIONS_PATH := "user://monster-art-associations.json"
var loader := DaxSpriteLoader.new()
var monsters: Array[Dictionary] = []
var archives: Array[Dictionary] = []
var overrides: Dictionary = {}
var decisions: Dictionary = {}
var selected := 0
var name_field: LineEdit
var identity: Label
var status: Label
var gallery: GridContainer
var folder := ""
var dirty := false

func _ready() -> void:
    _build_ui()
    if FileAccess.file_exists(SAVE_PATH):
        var parsed = JSON.parse_string(FileAccess.get_file_as_string(SAVE_PATH))
        if not parsed is Dictionary:
            status.text = "Cannot read saved names; repair or move %s before editing." % ProjectSettings.globalize_path(SAVE_PATH)
            name_field.editable = false
            return
        overrides = parsed
    if FileAccess.file_exists(REVIEW_PATH):
        var parsed = JSON.parse_string(FileAccess.get_file_as_string(REVIEW_PATH))
        if not parsed is Dictionary:
            status.text = "Cannot read saved art decisions; repair or move %s before reviewing." % ProjectSettings.globalize_path(REVIEW_PATH)
            name_field.editable = false
            return
        decisions = parsed
    folder = GameFinder.game_folder("POOLRAD")
    var directory := DirAccess.open(folder)
    if directory == null:
        status.text = "Pool of Radiance's files were not found; set OPENGOLD_GAME_DIR to their folder."
        return
    var files: Array[String] = []
    for filename in directory.get_files(): files.append(filename)
    files.sort_custom(func(a: String, b: String) -> bool: return a.naturalnocasecmp_to(b) < 0)
    for filename in files:
        var upper := filename.to_upper()
        var path := folder.path_join(filename)
        if upper.match("MON*CHA.DAX"):
            var data := FileAccess.get_file_as_bytes(path)
            for id in loader.list_record_ids(path):
                var record := loader._extract_record(data, id)
                if record.size() != 285 or record[0] > 15: continue
                monsters.append({"archive": filename, "id": id, "name": record.slice(1, 1 + record[0]).get_string_from_ascii(), "icon_head": record[189], "icon_body": record[190]})
        elif upper.match("CPIC*.DAX") or upper.match("SPRIT*.DAX") or upper in ["CHEAD.DAX", "CBODY.DAX"]:
            archives.append({"name":filename, "path":path, "ids":loader.list_record_ids(path), "combat":not upper.begins_with("SPRIT")})
    if monsters.is_empty():
        status.text = "No supported monster records found."
        return
    _show_monster()
    _save_associations()

func _build_ui() -> void:
    var background := ColorRect.new()
    background.color = Color(0.08, 0.09, 0.11)
    background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    add_child(background)
    var margin := MarginContainer.new()
    margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    for side in ["left", "right", "top", "bottom"]: margin.add_theme_constant_override("margin_" + side, 24)
    add_child(margin)
    var column := VBoxContainer.new()
    column.add_theme_constant_override("separation", 12)
    margin.add_child(column)
    var title := Label.new()
    title.text = "Monster Art Review"
    title.add_theme_font_size_override("font_size", 26)
    column.add_child(title)
    identity = Label.new()
    column.add_child(identity)
    var row := HBoxContainer.new()
    column.add_child(row)
    var previous := Button.new()
    previous.text = "< Previous"
    previous.pressed.connect(func(): _navigate(-1))
    row.add_child(previous)
    name_field = LineEdit.new()
    name_field.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    name_field.placeholder_text = "Monster name"
    name_field.text_changed.connect(func(_value: String): dirty = true; status.text = "Unsaved name. Save, press Enter, or navigate to save.")
    name_field.text_submitted.connect(func(_value: String):
        if _save_name(): name_field.release_focus())
    row.add_child(name_field)
    var save := Button.new()
    save.text = "Save name"
    save.pressed.connect(func():
        if _save_name(): name_field.release_focus())
    row.add_child(save)
    var next := Button.new()
    next.text = "Next >"
    next.pressed.connect(func(): _navigate(1))
    row.add_child(next)
    var help := Label.new()
    help.text = "Left / Right: browse monsters. While editing, arrows move the text cursor; Enter saves.\nNo smoothing; minimum 48 x 48. Each image explains its guess. Components are shown separately; bindings are not verified."
    column.add_child(help)
    var scroll := ScrollContainer.new()
    scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
    column.add_child(scroll)
    gallery = GridContainer.new()
    gallery.columns = 4
    gallery.add_theme_constant_override("h_separation", 16)
    gallery.add_theme_constant_override("v_separation", 16)
    scroll.add_child(gallery)
    status = Label.new()
    status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    column.add_child(status)

func _key() -> String:
    var monster := monsters[selected]
    return "%s:%d" % [monster.archive.to_upper(), monster.id]

func _save_name() -> bool:
    if monsters.is_empty() or not name_field.editable: return false
    if not dirty: return true
    var value := name_field.text.strip_edges()
    if value.is_empty():
        status.text = "Enter a non-empty name before saving or navigating."
        return false
    var updated := overrides.duplicate()
    if value == monsters[selected].name: updated.erase(_key())
    else: updated[_key()] = value
    var temporary_path := SAVE_PATH + ".tmp"
    var file := FileAccess.open(temporary_path, FileAccess.WRITE)
    if file == null:
        status.text = "Name could not be saved: %s" % error_string(FileAccess.get_open_error())
        return false
    file.store_string(JSON.stringify(updated, "  "))
    file.flush()
    if file.get_error() != OK:
        status.text = "Failed to write name overrides."
        return false
    file.close()
    var result := DirAccess.rename_absolute(ProjectSettings.globalize_path(temporary_path), ProjectSettings.globalize_path(SAVE_PATH))
    if result != OK:
        status.text = "Could not replace saved names: %s" % error_string(result)
        return false
    overrides = updated
    dirty = false
    if not _save_associations(): return false
    status.text = "Saved local name override: " + ProjectSettings.globalize_path(SAVE_PATH)
    return true

func _navigate(direction: int) -> void:
    if monsters.is_empty() or not _save_name(): return
    selected = wrapi(selected + direction, 0, monsters.size())
    name_field.release_focus()
    _show_monster()

func _review_key(archive: String, record_id: int, frame: int) -> String:
    return "%s|%s:%d:%d" % [_key(), archive.to_upper(), record_id, frame]

func _candidate_records(monster: Dictionary, archive: Dictionary) -> Array[Dictionary]:
    var result: Array[Dictionary] = []
    var bases: Dictionary = {int(monster.id): "Matching record ID (provisional)"}
    for other in monsters:
        if other.name == monster.name and other.id != monster.id:
            bases[int(other.id)] = "Same original name, different ID (weak guess)"
    var component: bool = archive.name.to_upper() in ["CHEAD.DAX", "CBODY.DAX"]
    if component:
        # Do not mix character components into a monster with a fixed icon candidate.
        for source in archives:
            if not source.name.to_upper().begins_with("CPIC"): continue
            for base in bases:
                if source.ids.has(base): return result
        var field := "icon_head" if archive.name.to_upper() == "CHEAD.DAX" else "icon_body"
        bases = {int(monster[field]): "Character %s field (weak component guess)" % field}
    for base in bases:
        var ids: Array[int] = [base]
        if archive.combat: ids.append(base + 128)
        for id in ids:
            if not archive.ids.has(id): continue
            var role := "encounter_variant"
            if archive.combat:
                role = "ready_candidate" if id == base else "action_candidate"
            if component: role = "component_" + role
            result.append({"id": id, "role": role, "evidence": bases[base]})
    return result

func _set_incorrect(checked: bool, key: String, checkbox: CheckBox, review_label: Label) -> void:
    var updated := decisions.duplicate(true)
    if checked:
        updated[key] = {"status": "unresolved", "reason": "incorrect_association"}
    else:
        updated.erase(key)
    var temporary_path := REVIEW_PATH + ".tmp"
    var file := FileAccess.open(temporary_path, FileAccess.WRITE)
    var result := FileAccess.get_open_error()
    if file != null:
        file.store_string(JSON.stringify(updated, "  "))
        file.flush()
        result = file.get_error()
        file.close()
        if result == OK:
            result = DirAccess.rename_absolute(ProjectSettings.globalize_path(temporary_path), ProjectSettings.globalize_path(REVIEW_PATH))
    if result != OK:
        checkbox.set_pressed_no_signal(decisions.has(key))
        status.text = "Could not save art decision: %s" % error_string(result)
        return
    decisions = updated
    review_label.text = "Unresolved - incorrect association" if checked else "Candidate association"
    if not _save_associations(): return
    status.text = "Art decision saved. " + ("Marked unresolved." if checked else "Restored to candidate; not verified.")
    if dirty:
        status.text += " Name edit is still unsaved."

func _save_associations() -> bool:
    var entries: Array[Dictionary] = []
    for monster in monsters:
        var monster_key := "%s:%d" % [monster.archive.to_upper(), monster.id]
        var related: Array[Dictionary] = []
        for archive in archives:
            for candidate in _candidate_records(monster, archive):
                var id: int = candidate.id
                if not archive.ids.has(id): continue
                for frame in range(loader.frame_count(archive.path, id, archive.combat)):
                    var key := "%s|%s:%d:%d" % [monster_key, archive.name.to_upper(), id, frame]
                    var incorrect := decisions.has(key)
                    related.append({
                        "art_archive": archive.name, "art_record": id, "image_index": frame,
                        "category": "combat" if archive.combat else "encounter",
                        "role": candidate.role, "guess_basis": candidate.evidence,
                        "status": "unresolved" if incorrect else "candidate",
                        "incorrect": incorrect,
                        "evidence": "user_rejected_association" if incorrect else candidate.evidence
                    })
        entries.append({"monster_archive": monster.archive, "monster_id": monster.id,
            "original_name": monster.name, "name": str(overrides.get(monster_key, monster.name)),
            "art": related})
    var temporary_path := ASSOCIATIONS_PATH + ".tmp"
    var file := FileAccess.open(temporary_path, FileAccess.WRITE)
    var result := FileAccess.get_open_error()
    if file != null:
        file.store_string(JSON.stringify({"schema_version": 1, "monsters": entries}, "  "))
        file.flush()
        result = file.get_error()
        file.close()
        if result == OK:
            result = DirAccess.rename_absolute(ProjectSettings.globalize_path(temporary_path), ProjectSettings.globalize_path(ASSOCIATIONS_PATH))
    if result != OK:
        status.text = "Could not save association list: %s. Name/decision files are retained; reopen to regenerate." % error_string(result)
        return false
    return true

func _input(event: InputEvent) -> void:
    if name_field.has_focus() or not event is InputEventKey or not event.pressed or event.echo: return
    if event.keycode == KEY_LEFT or event.keycode == KEY_RIGHT:
        _navigate(-1 if event.keycode == KEY_LEFT else 1)
        get_viewport().set_input_as_handled()

func _show_monster() -> void:
    for child in gallery.get_children():
        gallery.remove_child(child)
        child.queue_free()
    var monster := monsters[selected]
    identity.text = "%d / %d | %s | record %d | Original name: %s" % [selected + 1, monsters.size(), monster.archive, monster.id, monster.name]
    name_field.text = str(overrides.get(_key(), monster.name))
    dirty = false
    var image_count := 0
    for archive in archives:
        for candidate in _candidate_records(monster, archive):
            var id: int = candidate.id
            if not archive.ids.has(id): continue
            var count := loader.frame_count(archive.path, id, archive.combat)
            for frame in range(count):
                var img := loader.load_frame(archive.path, id, frame, archive.combat)
                if img == null: continue
                var box := VBoxContainer.new()
                box.custom_minimum_size = Vector2(270, 150)
                gallery.add_child(box)
                var label := Label.new()
                var role: String = candidate.role.replace("_", " ")
                var scale_factor := maxi(1, ceili(48.0 / mini(img.get_width(), img.get_height())))
                label.text = "%s\n%s\n%s / %d / image %d\n%d x %d (%dx display)\n%s" % [str(overrides.get(_key(), monster.name)), role, archive.name, id, frame + 1, img.get_width(), img.get_height(), scale_factor, candidate.evidence]
                label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
                box.add_child(label)
                var texture := TextureRect.new()
                texture.texture = ImageTexture.create_from_image(img)
                texture.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
                texture.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
                texture.stretch_mode = TextureRect.STRETCH_SCALE
                texture.custom_minimum_size = Vector2(img.get_size() * scale_factor)
                texture.size_flags_horizontal = Control.SIZE_SHRINK_BEGIN
                box.add_child(texture)
                var review_key := _review_key(archive.name, id, frame)
                var incorrect := CheckBox.new()
                incorrect.text = "Incorrect"
                incorrect.button_pressed = decisions.has(review_key)
                box.add_child(incorrect)
                var review_label := Label.new()
                review_label.text = "Unresolved - incorrect association" if incorrect.button_pressed else "Candidate association"
                box.add_child(review_label)
                incorrect.toggled.connect(_set_incorrect.bind(review_key, incorrect, review_label))
                image_count += 1
    status.text = "%d images. Check Incorrect to mark an association unresolved; decisions save immediately. Original DAX files are unchanged." % image_count
    if image_count == 0:
        status.text = "No same-ID art found. This monster may use an alias or assembled icon; its artwork binding is unresolved."
