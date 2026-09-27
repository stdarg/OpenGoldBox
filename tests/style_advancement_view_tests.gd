extends SceneTree
var light := false
var fixture := ""
var klass := "fighter"
var captures := ""
var output := ""
var originals := {}
var locales := ["en", "es"]
const SLOT = "Fighting Style advancement test"
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--style-class="): klass = arg.trim_prefix("--style-class=")
        if arg == "--style-light": light = true
        if arg == "--style-demo": locales = ["en"]
        if arg.begins_with("--style-fixture="): fixture = arg.trim_prefix("--style-fixture=")
        if arg.begins_with("--style-captures="): captures = arg.trim_prefix("--style-captures=")
        if arg.begins_with("--style-output="): output = arg.trim_prefix("--style-output=")
    call_deferred("run_checks")
func restore_files() -> void:
    for path in originals:
        if originals[path] == null:
            if FileAccess.file_exists(path): DirAccess.remove_absolute(path)
        else:
            var f = FileAccess.open(path, FileAccess.WRITE); f.store_buffer(originals[path]); f.close()
    originals.clear()
func require(ok: bool, message: String) -> void:
    if not ok:
        restore_files(); push_error(message); quit(1); assert(ok, message)
func settle() -> void:
    for frame in range(5): await process_frame
func press(path: String) -> void:
    var b: Button = current_scene.get_node(path)
    require(not b.disabled, "Disabled control: " + path)
    b.pressed.emit(); await settle()
func key(window: Window, code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new(); event.keycode = code; event.pressed = down
        if code == KEY_ESCAPE: window.window_input.emit(event)
        else: window.push_input(event, true)
    await settle()
func load_slot() -> void:
    await press("PartyPanel/Load")
    var list: ItemList = current_scene.get_node("SaveSlots/Slots")
    var index := -1
    for i in range(list.item_count):
        if list.get_item_text(i) == SLOT: index = i
    require(index >= 0, "Fixture slot found")
    list.select(index); list.item_selected.emit(index)
    await press("SaveSlots/Action"); await press("SaveSlots/Action")
    require(not current_scene.get_node("SaveSlots").visible, "Fixture loaded")
func capture(name: String, window: Window) -> void:
    for size in [Vector2i(1120,800), Vector2i(1920,1080)]:
        root.size = size; await settle(); window.popup_centered(); await settle()
        if not captures.is_empty():
            DirAccess.make_dir_recursive_absolute(captures); await RenderingServer.frame_post_draw
            require(root.get_texture().get_image().save_png(captures.path_join(name + "-" + str(size.x) + ".png")) == OK, "Capture layout")
func run_checks() -> void:
    require(not fixture.is_empty(), "Fixture required")
    var slot := ProjectSettings.globalize_path("user://saves/" + SLOT.to_utf8_buffer().hex_encode() + ".ogs")
    DirAccess.make_dir_recursive_absolute(slot.get_base_dir())
    for suffix in ["", ".bak"]: originals[slot + suffix] = FileAccess.get_file_as_bytes(slot + suffix) if FileAccess.file_exists(slot + suffix) else null
    var source := FileAccess.get_file_as_bytes(fixture)
    root.gui_embed_subwindows = true
    for locale in locales:
        TranslationServer.set_locale(locale)
        var f = FileAccess.open(slot, FileAccess.WRITE); f.store_buffer(source); f.close()
        change_scene_to_file("res://scenes/character_creation.tscn"); await settle(); await press("Party"); await load_slot()
        for attained in [2,3,4]:
            await press("PartyPanel/Roster/Advance1")
            var level: Window = current_scene.get_node("LevelUp")
            require(level.visible and str(attained) in level.get_node("Title").text, "Ordinary level-up opens for next Fighting Style level")
            var style: OptionButton = level.get_node("FightingStyle")
            require(style.visible == (klass == "fighter" or attained == 2), "Style entitlement controls match actual class/level")
            if style.visible:
                require(style.focus_mode == Control.FOCUS_ALL, "Style selector supports keyboard focus")
                var selection := (4 if klass == "fighter" else 3) if light else (3 if klass == "fighter" else 2)
                if klass == "fighter" and attained == 3: selection = 2
                if klass == "fighter" and attained == 4: selection = 4 if light else 1
                require(not style.is_item_disabled(selection), "Required replacement available")
                style.select(selection); style.item_selected.emit(selection); await settle()
            if klass == "fighter" and attained == 4:
                var feat: OptionButton = level.get_node("Feat")
                require(not feat.is_item_disabled(5), "Replacing Archery frees independent Archery feat")
                feat.select(5); feat.item_selected.emit(5); await settle()
            if klass != "fighter":
                require("unavailable" in level.get_node("Note").text if locale == "en" else "Sin conjuros, trucos alternativos, maestría" in level.get_node("Note").text, "Remaining class scope explicit")
            require(level.get_node("Note").get_rect().end.y <= level.get_node("Error").position.y, "Class note fits above validation errors: level=" + str(attained) + " locale=" + locale + " note=" + str(level.get_node("Note").get_rect()) + " error=" + str(level.get_node("Error").position) + " lines=" + str(level.get_node("Note").get_line_count()))
            if klass == "fighter" and attained == 4:
                level.get_node("AdvancementTraining").select(1); level.get_node("AdvancementTraining").item_selected.emit(1); await settle()
            await capture("style-" + klass + "-level-" + str(attained) + "-" + locale, level)
            var before: String = current_scene.get_node("PartyPanel/Sheet").text
            await press("LevelUp/Cancel")
            require(not level.visible and current_scene.get_node("PartyPanel/Sheet").text == before, "Cancel preserves character")
            await press("PartyPanel/Roster/Advance1")
            if style.visible:
                var selection := (4 if klass == "fighter" else 3) if light else (3 if klass == "fighter" else 2)
                if klass == "fighter" and attained == 3: selection = 2
                if klass == "fighter" and attained == 4: selection = 4 if light else 1
                style.select(selection); style.item_selected.emit(selection); await settle()
            if klass == "fighter" and attained == 4:
                level.get_node("Feat").select(5); level.get_node("Feat").item_selected.emit(5); await settle()
            if klass == "fighter" and attained == 4:
                level.get_node("AdvancementTraining").select(1); level.get_node("AdvancementTraining").item_selected.emit(1); await settle()
            level.get_node("Confirm").grab_focus(); await key(level, KEY_ENTER)
            require(not level.visible, "Keyboard confirms complete advancement")
        await press("PartyPanel/Save"); current_scene.get_node("SaveSlots/Name").text = SLOT
        await press("SaveSlots/Action"); await press("SaveSlots/Action")
        require(not current_scene.get_node("SaveSlots").visible, "Advanced Fighting Style saves through ordinary party flow")
        if not output.is_empty():
            f = FileAccess.open(output, FileAccess.WRITE); f.store_buffer(FileAccess.get_file_as_bytes(slot)); f.close()
        await load_slot()
        require(not current_scene.get_node("PartyPanel/Roster/Advance1").visible, "Reload retains level four and does not offer unimplemented level five")
    restore_files(); print("Fighting Style advancement UI checks passed"); quit(0)
