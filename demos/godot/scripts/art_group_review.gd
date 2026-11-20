extends Control

const STATE_PATH := "user://art-group-review.json"
const NAMES_PATH := "user://monster-art-names.json"
const LEGACY_PATH := "user://monster-art-decisions.json"
const INVENTORY_PATH := "user://art-group-inventory.json"
const CATEGORY_NAMES_PATH := "user://art-category-names.json"
const EVIDENCE_PATH := "user://art-script-evidence.json"
const ScriptEvidence = preload("res://scripts/art_script_evidence.gd")
var script_evidence = ScriptEvidence.new()
var evidence_text: RichTextLabel
const PLAYER_TARGETS := [
    {"key":"PLAYER:BODY", "name":"Customizable player character - body", "archive":"CBODY.DAX"},
    {"key":"PLAYER:HEAD", "name":"Customizable player character - head", "archive":"CHEAD.DAX"},
    {"key":"ART:ARROW", "name":"Arrow projectile", "archive":"COMSPR.DAX", "record":0},
    {"key":"ART:HATCHET", "name":"Hatchet projectile", "archive":"COMSPR.DAX", "record":3},
    {"key":"ART:FLASK", "name":"Flask projectile", "archive":""}
]
var loader := DaxSpriteLoader.new()
var groups: Array[Dictionary] = []
var monsters: Array[Dictionary] = []
var names: Dictionary = {}
var category_names: Dictionary = {}
var category_dialog: ConfirmationDialog
var category_name_field: LineEdit
var editing_category := ""
var legacy: Dictionary = {}
var state: Dictionary = {}
var visible_groups: Array[int] = []
var group_position := 0
var category: OptionButton
var move_category: OptionButton
var search: LineEdit
var choices: ItemList
var rename: LineEdit
var title: Label
var sources: RichTextLabel
var gallery: HBoxContainer
var message: Label
var assignments: VBoxContainer
var writable := true
var rename_dirty := false
var rename_key := ""

func _ready() -> void:
    _ui()
    names = _read(NAMES_PATH)
    legacy = _read(LEGACY_PATH)
    state = _read(STATE_PATH)
    category_names = _read(CATEGORY_NAMES_PATH)
    if not writable: return
    _refresh_category_names()
    var folder := GameFinder.game_folder("POOLRAD")
    var directory := DirAccess.open(folder)
    if directory == null:
        message.text = "Pool of Radiance's files were not found; set OPENGOLD_GAME_DIR to their folder."
        return
    var files := Array(directory.get_files())
    files.sort_custom(func(a, b): return str(a).naturalnocasecmp_to(str(b)) < 0)
    var dedup := {}
    for filename in files:
        var upper: String = filename.to_upper()
        var path: String = folder.path_join(filename)
        if upper.match("MON*CHA.DAX"):
            var data := FileAccess.get_file_as_bytes(path)
            for id in loader.list_record_ids(path):
                var r := loader._extract_record(data, id)
                if r.size() == 285 and r[0] <= 15:
                    monsters.append({"key":"%s:%d" % [upper,id],"name":r.slice(1,1+r[0]).get_string_from_ascii(),"id":id,"archive":filename})
            continue
        var kind := ""
        if upper.match("CPIC*.DAX"): kind = "Combat icons"
        elif upper.match("SPRIT*.DAX"): kind = "Encounter sprites"
        elif upper in ["CHEAD.DAX", "CBODY.DAX"]: kind = "Character components"
        elif upper == "COMSPR.DAX": kind = "Combat effects / miscellaneous"
        if kind.is_empty(): continue
        var combat := kind != "Encounter sprites"
        var ids := loader.list_record_ids(path)
        for id in ids:
            if combat and id >= 128 and ids.has(id - 128): continue
            var related: Array[int] = [id]
            if combat and id < 128 and ids.has(id + 128): related.append(id + 128)
            var images: Array[Image] = []
            var refs: Array[Dictionary] = []
            var fingerprints := PackedStringArray([kind])
            for record_id in related:
                for frame in range(loader.frame_count(path,record_id,combat)):
                    var img := loader.load_frame(path,record_id,frame,combat)
                    if img == null:
                        push_warning("Could not decode %s/%d/%d: %s" % [filename,record_id,frame,loader.error_message])
                        continue
                    images.append(img)
                    refs.append({"archive":filename,"record":record_id,"image":frame})
                    fingerprints.append("%dx%d:%s" % [img.get_width(),img.get_height(),img.get_data().hex_encode().sha256_text()])
            if images.is_empty(): continue
            var key := "|".join(fingerprints).sha256_text()
            if dedup.has(key):
                groups[dedup[key]].sources.append(refs)
            else:
                dedup[key] = groups.size()
                # Keep the original fingerprint category so reclassification retains saved decisions.
                var display_kind := "Projectiles" if upper == "COMSPR.DAX" and id in [0, 3] else kind
                groups.append({"key":key,"category":display_kind,"images":images,"sources":[refs]})
    script_evidence.build(folder)
    _write(EVIDENCE_PATH,script_evidence.export_data())
    _inventory()
    _filter()

func _read(path: String) -> Dictionary:
    if not FileAccess.file_exists(path): return {}
    var value = JSON.parse_string(FileAccess.get_file_as_string(path))
    if value is Dictionary: return value
    writable = false
    message.text = "Invalid review file; not overwriting: " + ProjectSettings.globalize_path(path)
    return {}

func _write(path: String, value: Dictionary) -> bool:
    if not writable: return false
    var f := FileAccess.open(path + ".tmp",FileAccess.WRITE)
    if f == null:
        message.text = "Unable to save " + path
        return false
    f.store_string(JSON.stringify(value,"  "))
    f.flush()
    var error := f.get_error()
    f.close()
    if error == OK: error = DirAccess.rename_absolute(ProjectSettings.globalize_path(path + ".tmp"),ProjectSettings.globalize_path(path))
    if error != OK:
        message.text = "Save failed: " + error_string(error)
        return false
    return true

func _ui() -> void:
    var bg := ColorRect.new()
    bg.color = Color(0.07,0.08,0.10)
    bg.set_anchors_and_offsets_preset(PRESET_FULL_RECT)
    add_child(bg)
    var margin := MarginContainer.new()
    margin.set_anchors_and_offsets_preset(PRESET_FULL_RECT)
    for side in ["left","right","top","bottom"]: margin.add_theme_constant_override("margin_"+side,20)
    add_child(margin)
    var column := VBoxContainer.new()
    column.add_theme_constant_override("separation",10)
    margin.add_child(column)
    title = Label.new()
    title.add_theme_font_size_override("font_size",24)
    column.add_child(title)
    var navigation := HBoxContainer.new()
    column.add_child(navigation)
    category = OptionButton.new()
    category.fit_to_longest_item = false
    category.custom_minimum_size.x = 220
    category.clip_text = true
    for text in ["All categories","Combat icons","Encounter sprites","Character components","Projectiles","Combat effects / miscellaneous"]:
        category.add_item(text)
        category.set_item_metadata(category.item_count - 1, text)
    category.item_selected.connect(func(_index): _filter())
    navigation.add_child(category)
    _button(navigation,"Add category",_open_add_category)
    _button(navigation,"Rename category",_open_category_rename)
    category_dialog = ConfirmationDialog.new()
    category_dialog.title = "Rename category"
    category_dialog.min_size = Vector2i(380, 110)
    category_name_field = LineEdit.new()
    category_name_field.placeholder_text = "Category name"
    category_dialog.add_child(category_name_field)
    category_dialog.confirmed.connect(_save_category_name)
    add_child(category_dialog)
    _button(navigation,"< Previous",func(): _navigate(-1))
    _button(navigation,"Next >",func(): _navigate(1))
    _button(navigation,"Next unreviewed",_next_unreviewed)
    _button(navigation,"Unknown / skip",func(): _decision("", "unknown"))
    var classification := HBoxContainer.new()
    column.add_child(classification)
    move_category = OptionButton.new()
    move_category.fit_to_longest_item = false
    move_category.custom_minimum_size.x = 220
    move_category.clip_text = true
    classification.add_child(move_category)
    _button(classification,"Move current group to category",_move_group)
    var split := HSplitContainer.new()
    split.size_flags_vertical = SIZE_EXPAND_FILL
    column.add_child(split)
    var left := VBoxContainer.new()
    left.custom_minimum_size.x = 660
    left.size_flags_horizontal = SIZE_EXPAND_FILL
    split.add_child(left)
    var scroll := ScrollContainer.new()
    scroll.custom_minimum_size.y = 240
    left.add_child(scroll)
    gallery = HBoxContainer.new()
    gallery.add_theme_constant_override("separation",24)
    scroll.add_child(gallery)
    var details := TabContainer.new()
    details.size_flags_vertical = SIZE_EXPAND_FILL
    left.add_child(details)
    evidence_text = RichTextLabel.new()
    evidence_text.name = "Script evidence"
    evidence_text.selection_enabled = true
    details.add_child(evidence_text)
    sources = RichTextLabel.new()
    sources.name = "Art sources"
    sources.selection_enabled = true
    details.add_child(sources)
    var right := VBoxContainer.new()
    right.custom_minimum_size.x = 420
    split.add_child(right)
    search = LineEdit.new()
    search.placeholder_text = "Search names / NPC roles / categories"
    search.text_changed.connect(func(_text): _choices())
    right.add_child(search)
    choices = ItemList.new()
    choices.custom_minimum_size.y = 150
    choices.item_selected.connect(_selected_monster)
    right.add_child(choices)
    rename = LineEdit.new()
    rename.placeholder_text = "Select an assignment to edit its name"
    rename.text_changed.connect(func(_text): rename_dirty = true)
    rename.text_submitted.connect(func(_text): _save_name())
    right.add_child(rename)
    _button(right,"Save assignment name (does not confirm art)",_save_name)
    var actions := HBoxContainer.new()
    right.add_child(actions)
    _button(actions,"Confirm selected",func(): _decision(_chosen(),"confirmed"))
    _button(actions,"Incorrect",func(): _decision(_chosen(),"rejected"))
    _button(actions,"Reset to suggested",func(): _decision(_chosen(),"suggested"))
    var assignment_scroll := ScrollContainer.new()
    assignment_scroll.size_flags_vertical = SIZE_EXPAND_FILL
    right.add_child(assignment_scroll)
    assignments = VBoxContainer.new()
    assignment_scroll.add_child(assignments)
    message = Label.new()
    message.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    column.add_child(message)

func _button(parent: Node, text: String, action: Callable) -> void:
    var b := Button.new()
    b.text = text
    b.pressed.connect(action)
    parent.add_child(b)

func _clear(node: Node) -> void:
    for child in node.get_children():
        node.remove_child(child)
        child.queue_free()

func _group() -> Dictionary:
    return groups[visible_groups[group_position]]

func _category_of(group: Dictionary) -> String:
    return str(state.get(group.key, {}).get("category", group.category))

func _filter() -> void:
    if not _save_name(): return
    visible_groups.clear()
    for i in range(groups.size()):
        if category.selected == 0 or _category_of(groups[i]) == category.get_item_metadata(category.selected): visible_groups.append(i)
    group_position = 0
    _show()

func _navigate(direction: int) -> void:
    if visible_groups.is_empty() or not _save_name(): return
    group_position = wrapi(group_position+direction,0,visible_groups.size())
    _show()

func _next_unreviewed() -> void:
    if visible_groups.is_empty() or not _save_name(): return
    for offset in range(1,visible_groups.size()+1):
        var next := wrapi(group_position+offset,0,visible_groups.size())
        var review: Dictionary = state.get(groups[visible_groups[next]].key,{})
        if review.get("resolution", "unresolved") == "unresolved":
            group_position = next
            _show()
            return
    message.text = "No unreviewed groups in this category. Unknown groups are skipped; rejected-only groups remain unresolved."

func _links(group: Dictionary) -> Dictionary:
    var links: Dictionary = script_evidence.suggestions(group)
    for target in PLAYER_TARGETS:
        for refs in group.sources:
            for ref in refs:
                if str(ref.archive).to_upper() == target.archive and (not target.has("record") or ref.record == target.record):
                    links[target.key] = {"status":"suggested", "basis":"User-identified projectile: " + target.name if target.has("record") else "Character component archive: " + target.archive}
    if links.is_empty() and group.category in ["Combat icons","Encounter sprites"]:
        for monster in monsters:
            for refs in group.sources:
                if refs[0].record == monster.id:
                    links[monster.key] = {"status":"suggested","basis":"Matching record ID; not gameplay verified"}
    # Conservatively retain every legacy rejection for any source in this group.
    for monster in monsters:
        for refs in group.sources:
            for ref in refs:
                var key := "%s|%s:%d:%d" % [monster.key,str(ref.archive).to_upper(),ref.record,ref.image]
                if legacy.has(key): links[monster.key] = {"status":"rejected","basis":"Legacy Incorrect decision; source retained in legacy file"}
    var review: Dictionary = state.get(group.key,{})
    for key in review.get("links",{}): links[key] = review.links[key]
    return links

func _show() -> void:
    _clear(gallery)
    _clear(assignments)
    if visible_groups.is_empty():
        title.text = "Art Review - no groups"
        sources.text = ""
        evidence_text.text = ""
        choices.clear()
        message.text = "No supported art in this category."
        return
    var group := _group()
    var review: Dictionary = state.get(group.key,{})
    var display_category := _category_of(group)
    title.text = "%s | group %d / %d | %s" % [category_names.get(display_category,display_category),group_position+1,visible_groups.size(),review.get("resolution","unresolved")]
    for i in range(group.images.size()):
        var img: Image = group.images[i]
        var box := VBoxContainer.new()
        gallery.add_child(box)
        var label := Label.new()
        label.text = "Image %d - %dx%d" % [i+1,img.get_width(),img.get_height()]
        box.add_child(label)
        var t := TextureRect.new()
        t.texture = ImageTexture.create_from_image(img)
        t.texture_filter = TEXTURE_FILTER_NEAREST
        t.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
        t.size_flags_horizontal = SIZE_SHRINK_BEGIN
        t.size_flags_vertical = SIZE_SHRINK_BEGIN
        t.custom_minimum_size = Vector2(img.get_size()*maxi(1,ceili(48.0/mini(img.get_width(),img.get_height()))))
        box.add_child(t)
    sources.text = "All source references (%d copies of this group):\n" % group.sources.size()
    for refs in group.sources:
        for ref in refs: sources.text += "%s / record %d / image %d\n" % [ref.archive,ref.record,ref.image]
        sources.text += "\n"
    var links := _links(group)
    _show_evidence(group)
    for key in links:
        var label := Label.new()
        label.custom_minimum_size.x = 395
        label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
        label.text = "%s [%s]\n%s\n%s" % [_name(key),key,links[key].status,links[key].basis]
        assignments.add_child(label)
    _choices()
    message.text = "Left / Right: groups. Confirm any number of identities. Incorrect rejects only the selected association. Unknown skips the group. All changes save locally."

func _name(key: String) -> String:
    if key.begins_with("CUSTOM:"): return str(category_names.get(key, key))
    for target in script_evidence.targets:
        if target.key == key: return str(names.get(key,target.name))
    for target in PLAYER_TARGETS:
        if target.key == key: return str(names.get(key,target.name))
    for monster in monsters:
        if monster.key == key: return str(names.get(key,monster.name))
    return key

func _refresh_category_names() -> void:
    var existing := []
    for i in range(category.item_count): existing.append(category.get_item_metadata(i))
    for key in category_names:
        if str(key).begins_with("CUSTOM:") and not existing.has(key):
            category.add_item(str(category_names[key]))
            category.set_item_metadata(category.item_count - 1, key)
    var previous = move_category.get_item_metadata(move_category.selected) if move_category.selected >= 0 else ""
    move_category.clear()
    for i in range(1, category.item_count):
        var key: String = category.get_item_metadata(i)
        category.set_item_text(i, str(category_names.get(key, key)))
        move_category.add_item(category.get_item_text(i))
        move_category.set_item_metadata(i - 1, key)
        if key == previous: move_category.select(i - 1)

func _open_add_category() -> void:
    editing_category = ""
    category_dialog.title = "Add category"
    category_name_field.text = ""
    category_dialog.popup_centered()
    category_name_field.grab_focus()

func _move_group() -> void:
    if visible_groups.is_empty() or move_category.selected < 0 or not _save_name(): return
    var updated := state.duplicate(true)
    var key: String = _group().key
    var review: Dictionary = updated.get(key, {"links":{}})
    review.category = move_category.get_item_metadata(move_category.selected)
    updated[key] = review
    if not _write(STATE_PATH, updated): return
    state = updated
    _inventory()
    _filter()
    message.text = "Group category saved; existing assignments and decisions preserved."

func _open_category_rename() -> void:
    if category.selected == 0:
        message.text = "Choose a specific category to rename."
        return
    editing_category = str(category.get_item_metadata(category.selected))
    category_dialog.title = "Rename category"
    category_name_field.text = str(category_names.get(editing_category, editing_category))
    category_dialog.popup_centered()
    category_name_field.grab_focus()
    category_name_field.select_all()

func _save_category_name() -> void:
    var value := category_name_field.text.strip_edges()
    if value.is_empty():
        message.text = "Category name cannot be empty."
        return
    for i in range(category.item_count):
        if category.get_item_metadata(i) != editing_category and category.get_item_text(i).to_lower() == value.to_lower():
            message.text = "A category with that name already exists."
            return
    if editing_category.is_empty():
        editing_category = "CUSTOM:" + Crypto.new().generate_random_bytes(16).hex_encode()
    var updated := category_names.duplicate()
    updated[editing_category] = value
    if not _write(CATEGORY_NAMES_PATH, updated): return
    category_names = updated
    _refresh_category_names()
    search.text = ""
    _choices()
    for i in range(choices.item_count):
        if choices.get_item_metadata(i) == editing_category:
            choices.select(i)
            _selected_monster(i)
            choices.ensure_current_is_visible()
    for i in range(move_category.item_count):
        if move_category.get_item_metadata(i) == editing_category: move_category.select(i)
    _inventory()
    if not visible_groups.is_empty():
        var group := _group()
        var display_category := _category_of(group)
        title.text = "%s | group %d / %d | %s" % [category_names.get(display_category,display_category),group_position+1,visible_groups.size(),state.get(group.key,{}).get("resolution","unresolved")]
    message.text = "Category name saved."

func _choices() -> void:
    if not _save_name(): return
    choices.clear()
    rename.text = ""
    rename_dirty = false
    if visible_groups.is_empty(): return
    var links := _links(_group())
    var custom_targets: Array = []
    for key in category_names:
        if str(key).begins_with("CUSTOM:"): custom_targets.append({"key":key})
    # Suggestions first; search can select any monster even if no automatic association exists.
    for suggested in [true,false]:
        for monster in PLAYER_TARGETS + custom_targets + script_evidence.targets + monsters:
            if links.has(monster.key) != suggested: continue
            var text := "%s | %s" % [_name(monster.key),monster.key]
            if not search.text.is_empty() and not search.text.to_lower() in text.to_lower(): continue
            choices.add_item(text)
            choices.set_item_metadata(choices.item_count-1,monster.key)
            choices.set_item_tooltip(choices.item_count-1,text)
    _select_suggested_choice(links)

func _select_suggested_choice(links: Dictionary) -> void:
    if state.get(_group().key, {}).get("resolution", "unresolved") == "confirmed": return
    for link in links.values():
        if link.status == "confirmed": return
    var best_index := -1
    var best_score := 0.5
    for suggested_key in links:
        if links[suggested_key].status != "suggested": continue
        var suggested_name := _name(suggested_key).strip_edges().to_lower()
        if suggested_name.is_empty(): continue
        for i in range(choices.item_count):
            var key: String = choices.get_item_metadata(i)
            if links.get(key, {}).get("status", "") == "rejected": continue
            var candidate := _name(key).strip_edges().to_lower()
            var score := suggested_name.similarity(candidate)
            # Prefer a user-created category when its name matches equally well.
            var custom_tie := score == best_score and key.begins_with("CUSTOM:") and best_index >= 0 and not str(choices.get_item_metadata(best_index)).begins_with("CUSTOM:")
            if score > best_score or custom_tie:
                best_score = score
                best_index = i
    if best_index >= 0:
        choices.select(best_index)
        _selected_monster(best_index)
        choices.ensure_current_is_visible()

func _chosen() -> String:
    var selected := choices.get_selected_items()
    return "" if selected.is_empty() else str(choices.get_item_metadata(selected[0]))

func _selected_monster(_index: int) -> void:
    if not _save_name(): return
    rename_key = _chosen()
    rename.text = _name(_chosen())
    rename_dirty = false

func _save_name() -> bool:
    if not rename_dirty: return true
    if rename_key.is_empty() or rename.text.strip_edges().is_empty():
        message.text = "Select an assignment and enter a nonempty name."
        return false
    if rename_key.begins_with("CUSTOM:"):
        var label := rename.text.strip_edges()
        for i in range(category.item_count):
            if category.get_item_metadata(i) != rename_key and category.get_item_text(i).to_lower() == label.to_lower():
                message.text = "A category with that name already exists."
                return false
        var updated_categories := category_names.duplicate()
        updated_categories[rename_key] = label
        if not _write(CATEGORY_NAMES_PATH, updated_categories): return false
        category_names = updated_categories
        rename_dirty = false
        _refresh_category_names()
        _inventory()
        return true
    var updated := names.duplicate()
    updated[rename_key] = rename.text.strip_edges()
    if not _write(NAMES_PATH,updated): return false
    names = updated
    rename_dirty = false
    _inventory()
    message.text = "Name saved. Art associations were not confirmed."
    return true

func _decision(key: String, value: String) -> void:
    if visible_groups.is_empty() or not _save_name(): return
    if key.is_empty() and value != "unknown":
        message.text = "Select an assignment first."
        return
    var updated := state.duplicate(true)
    var group := _group()
    var review: Dictionary = updated.get(group.key,{"links":{}})
    if value == "unknown":
        for link in review.links.values():
            if link.status == "confirmed":
                link.status = "suggested"
                link.basis = "Confirmation cleared by explicit Unknown decision"
        review.resolution = "unknown"
    else:
        review.links[key] = {"status":value,"basis":"Explicit user review"}
        if key.begins_with("CUSTOM:") and value == "confirmed": review.category = key
        var any_confirmed := false
        for link in review.links.values():
            if link.status == "confirmed": any_confirmed = true
        review.resolution = "confirmed" if any_confirmed else "unresolved"
    updated[group.key] = review
    if not _write(STATE_PATH,updated): return
    state = updated
    _inventory()
    if value == "confirmed":
        _show()
        _next_unreviewed()
        return
    _show()

func _inventory() -> void:
    var inventory := []
    for group in groups:
        var evidence_refs: Array = []
        for entry in script_evidence.for_group(group):
            evidence_refs.append({"archive":entry.script_archive,"record":entry.script_record,"address":entry.address})
        inventory.append({"key":group.key,"category":_category_of(group),"source_category":group.category,"sources":group.sources,"links":_links(group),"script_evidence":evidence_refs,"resolution":state.get(group.key,{}).get("resolution","unresolved")})
    _write(INVENTORY_PATH,{"schema_version":2,"groups":inventory,"names":names,"category_names":category_names,"player_targets":PLAYER_TARGETS,"dialogue_targets":script_evidence.targets,"evidence_file":EVIDENCE_PATH,"scope":"CPIC, SPRIT, CHEAD, CBODY, COMSPR; other asset families not decoded by this tool"})

func _show_evidence(group: Dictionary) -> void:
    var entries: Array = script_evidence.for_group(group)
    var lines := PackedStringArray()
    lines.append("%d script references. Banks are inferred; these are suggestions, not gameplay verification.\n" % entries.size())
    if entries.is_empty():
        lines.append("No constant art selection found for this group. Dynamic references, unused art, or a different resource bank may account for it.")
    for entry in entries:
        lines.append("%s / area-script %d / 0x%04X (record offset %d)" % [entry.script_archive,entry.script_record,entry.address,entry.record_offset])
        var operands := PackedStringArray()
        for arg in entry.operands: operands.append(ScriptEvidence.Decoder.operand_label(arg))
        lines.append("%s: %s" % [entry.command,", ".join(operands)])
        lines.append("%s. Art: %s / %d" % [entry.archive_basis,", ".join(entry.art_archives),entry.art_id])
        if entry.has("table_lookup"):
            var table: Dictionary = entry.table_lookup
            lines.append("Parallel tables: character 0x%04X, icon 0x%04X; possible index %d -> character %d, icon %d" % [table.monster_table,table.icon_table,table.index,table.monster_id,table.icon_id])
        for candidate in entry.candidates:
            lines.append("Candidate: %s (%s) @0x%04X — %s" % [_name(candidate.key),candidate.key,candidate.address,candidate.basis])
        if not entry.dialogue.is_empty(): lines.append("Dialogue on possible paths (branches may describe different participants):")
        for text in entry.dialogue:
            lines.append("0x%04X: %s" % [text.address,text.text])
        if entry.context_truncated: lines.append("Context search reached its limit; further paths remain.")
        lines.append("")
    lines.append("Index: %d scripts, %d instructions, %d dynamic art references, %d decoding diagnostics. Full index: %s" % [script_evidence.summary.get("scripts",0),script_evidence.summary.get("instructions",0),script_evidence.summary.get("dynamic_art_references",0),script_evidence.summary.get("diagnostics",[]).size(),ProjectSettings.globalize_path(EVIDENCE_PATH)])
    evidence_text.text = "\n".join(lines)
    evidence_text.scroll_to_line(0)

func _input(event: InputEvent) -> void:
    if search.has_focus() or rename.has_focus() or category.get_popup().visible or move_category.get_popup().visible or category_dialog.visible: return
    if event is InputEventKey and event.pressed and not event.echo:
        if event.keycode in [KEY_LEFT,KEY_RIGHT]:
            _navigate(-1 if event.keycode == KEY_LEFT else 1)
            get_viewport().set_input_as_handled()
