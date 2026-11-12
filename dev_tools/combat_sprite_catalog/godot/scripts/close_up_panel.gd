extends HBoxContainer

# The Close-ups tab: every close-up the original animates in the 3D view (a PIC
# record holding two or more frames on screen), one row per archive record. It
# plays the original with its timing, keeps a written description of it, and
# plays the new art drawn from that description in its place, as the game does.

const CloseUpStore = preload("res://scripts/close_up_store.gd")
# The 3D view's 5:6 shape: the game shows the original's 88 x 88 pictures 1.2
# times taller than wide, and new art is drawn at this size.
const PREVIEW_SIZE := Vector2(352, 422)
const TICKS_PER_SECOND := 18.2

var catalog: SpriteCatalog
var root := ""
var rows: Array = []
var visible_rows: Array[int] = []
var drafts: Dictionary = {}
var selected := -1
var elapsed := 0.0
var shown_frames: Array = []
var shown_delays := PackedInt32Array()
var filter: LineEdit
var list: ItemList
var count_label: Label
var heading: Label
var timing: Label
var copies: Label
var show_new: CheckButton
var preview: TextureRect
var preview_status: Label
var title_edit: LineEdit
var description_edit: TextEdit
var status: Label


func _init() -> void:
    name = "Close-ups"
    add_theme_constant_override("separation", 20)
    var sidebar := VBoxContainer.new()
    sidebar.custom_minimum_size.x = 420
    add_child(sidebar)
    var filter_label := Label.new()
    filter_label.text = "Filter by record or title"
    sidebar.add_child(filter_label)
    filter = LineEdit.new()
    filter.placeholder_text = "e.g. PIC2 or orc"
    filter.clear_button_enabled = true
    filter.text_changed.connect(func(_text: String): _rebuild_list())
    sidebar.add_child(filter)
    list = ItemList.new()
    list.size_flags_vertical = Control.SIZE_EXPAND_FILL
    list.item_selected.connect(_select)
    sidebar.add_child(list)
    count_label = Label.new()
    count_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    sidebar.add_child(count_label)

    var detail := VBoxContainer.new()
    detail.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    add_child(detail)
    heading = Label.new()
    heading.add_theme_font_size_override("font_size", 22)
    detail.add_child(heading)
    var columns := HBoxContainer.new()
    columns.size_flags_vertical = Control.SIZE_EXPAND_FILL
    columns.add_theme_constant_override("separation", 20)
    detail.add_child(columns)

    var left := VBoxContainer.new()
    left.custom_minimum_size.x = PREVIEW_SIZE.x
    columns.add_child(left)
    show_new = CheckButton.new()
    show_new.text = "Show new animation"
    show_new.tooltip_text = "Play the new art drawn from the description, as the game plays it, in place of the original."
    show_new.toggled.connect(func(_on: bool): _load_frames())
    left.add_child(show_new)
    var frame := PanelContainer.new()
    frame.custom_minimum_size = PREVIEW_SIZE
    left.add_child(frame)
    preview = TextureRect.new()
    preview.custom_minimum_size = PREVIEW_SIZE
    preview.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
    preview.stretch_mode = TextureRect.STRETCH_SCALE
    preview.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
    frame.add_child(preview)
    preview_status = Label.new()
    preview_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    preview_status.custom_minimum_size.x = PREVIEW_SIZE.x
    left.add_child(preview_status)
    timing = Label.new()
    timing.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    timing.custom_minimum_size.x = PREVIEW_SIZE.x
    left.add_child(timing)
    copies = Label.new()
    copies.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    copies.custom_minimum_size.x = PREVIEW_SIZE.x
    left.add_child(copies)

    var editor := VBoxContainer.new()
    editor.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    columns.add_child(editor)
    var title_label := Label.new()
    title_label.text = "Title"
    editor.add_child(title_label)
    title_edit = LineEdit.new()
    title_edit.text_changed.connect(func(_text: String): _store_draft())
    editor.add_child(title_edit)
    var description_label := Label.new()
    description_label.text = "Description (the only source for the new art)"
    editor.add_child(description_label)
    description_edit = TextEdit.new()
    description_edit.wrap_mode = TextEdit.LINE_WRAPPING_BOUNDARY
    description_edit.size_flags_vertical = Control.SIZE_EXPAND_FILL
    description_edit.text_changed.connect(_store_draft)
    editor.add_child(description_edit)
    var buttons := HBoxContainer.new()
    editor.add_child(buttons)
    var save := Button.new()
    save.name = "Save"
    save.text = "Save description"
    save.pressed.connect(_save)
    buttons.add_child(save)
    var export := Button.new()
    export.name = "Export"
    export.text = "Export to game"
    export.tooltip_text = "Copy the new close-up art to new-art-pack/closeups, which the game plays."
    export.pressed.connect(_export)
    buttons.add_child(export)
    status = Label.new()
    status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    editor.add_child(status)


func set_catalog(value: SpriteCatalog, files_root: String = "") -> void:
    catalog = value
    root = files_root
    rows = catalog.close_ups()
    drafts.clear()
    for row in rows:
        var saved: Dictionary = CloseUpStore.load_record(row, root)
        drafts[row.key] = {"title": saved.title, "description": saved.description}
    _rebuild_list()
    if not visible_rows.is_empty():
        list.select(0)
        _select(0)


func _process(delta: float) -> void:
    if shown_frames.is_empty():
        return
    elapsed += delta
    preview.texture = shown_frames[catalog.close_up_frame_at(shown_delays, elapsed)]


func _label(index: int) -> String:
    var title: String = drafts[rows[index].key].title
    return rows[index].key + ("  " + title if not title.is_empty() else "")


func _rebuild_list() -> void:
    var needle := filter.text.strip_edges().to_lower()
    var current: String = rows[selected].key if selected >= 0 else ""
    list.clear()
    visible_rows.clear()
    var with_art := 0
    for index in range(rows.size()):
        if DirAccess.dir_exists_absolute(CloseUpStore.art_folder(rows[index].folder, root)):
            with_art += 1
        if not needle.is_empty() and not _label(index).to_lower().contains(needle):
            continue
        visible_rows.append(index)
        list.add_item(_label(index))
        list.set_item_tooltip(list.item_count - 1, _label(index))
        if rows[index].key == current:
            list.select(list.item_count - 1)
    count_label.text = "%d animated close-ups, %d with new art." % [rows.size(), with_art]


func _select(list_index: int) -> void:
    selected = visible_rows[list_index]
    var row: Dictionary = rows[selected]
    var draft: Dictionary = drafts[row.key]
    heading.text = row.key + ("  " + draft.title if not draft.title.is_empty() else "")
    title_edit.set_block_signals(true)
    title_edit.text = draft.title
    title_edit.set_block_signals(false)
    description_edit.set_block_signals(true)
    description_edit.text = draft.description
    description_edit.set_block_signals(false)
    timing.text = _timing_text(row)
    copies.text = "Same picture as " + ", ".join(row.copies) if not row.copies.is_empty() else ""
    status.text = ""
    _load_frames()


# What the new art must hold: one frame per frame the original holds, in order.
func _timing_text(row: Dictionary) -> String:
    var parts: Array[String] = []
    var held: PackedInt32Array = row.held
    for n in range(held.size()):
        var ticks: int = row.delays[held[n]]
        parts.append("frame_%d.png %d tick%s (%.2f s)" % [n, ticks, "" if ticks == 1 else "s", ticks / TICKS_PER_SECOND])
    return "New art: %d frames of 352 × 422, played in a loop: %s." % [held.size(), ", ".join(parts)]


func _load_frames() -> void:
    elapsed = 0.0
    # A new list: the one shown may be a row's own frames.
    shown_frames = []
    preview.texture = null
    if selected < 0:
        return
    var row: Dictionary = rows[selected]
    var folder := CloseUpStore.art_folder(row.folder, root)
    var art: Dictionary = catalog.close_up_art(folder, row.delays)
    if not show_new.button_pressed:
        shown_frames = row.frames
        shown_delays = row.delays
        preview_status.text = "Original %s." % row.key + (" New art is ready." if not art.frames.is_empty() else "")
        return
    if art.frames.is_empty():
        preview_status.text = "No new animation yet: put its frames in closeups/art/%s." % row.folder \
            if not art.found else "The new art cannot be used: %s." % art.problem
        return
    shown_frames = art.frames
    shown_delays = art.delays
    preview_status.text = "New animation from closeups/art/%s, with the original's timing." % row.folder


func _store_draft() -> void:
    if selected < 0:
        return
    drafts[rows[selected].key] = {"title": title_edit.text.strip_edges(), "description": description_edit.text}


func _save() -> void:
    if selected < 0:
        return
    _store_draft()
    var row: Dictionary = rows[selected]
    var draft: Dictionary = drafts[row.key]
    var error := CloseUpStore.save_record(row, draft.title, draft.description, root)
    status.text = "Saved " + CloseUpStore.json_path(row.folder, root).get_file() if error.is_empty() else error
    heading.text = row.key + ("  " + draft.title if not draft.title.is_empty() else "")
    _rebuild_list()


func _export() -> void:
    var error := CloseUpStore.export_all(root)
    status.text = "Exported the new close-up art to new-art-pack/closeups" if error.is_empty() else error
