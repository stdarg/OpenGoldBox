extends SceneTree
# A level-6 Lore Bard replaces a Magical Discoveries spell through the level-up
# window's Replace page, and the next level's page lists the new one.
var fixture := ""
var captures := ""
var originals := {}
const SLOT = "Discovery swap test"
func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--discovery-fixture="): fixture = arg.trim_prefix("--discovery-fixture=")
        if arg.begins_with("--discovery-captures="): captures = arg.trim_prefix("--discovery-captures=")
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
# Presses Next until the Replace page shows.
func to_swaps(level: Window) -> void:
    for page in range(4):
        if level.get_node("SwapPage").visible: return
        await press("LevelUp/Confirm")
    require(level.get_node("SwapPage").visible, "The Replace page comes before the spells")
func items(control: OptionButton) -> Array:
    var result := []
    for n in range(control.item_count): result.append(str(control.get_item_metadata(n)))
    return result
func run_checks() -> void:
    require(not fixture.is_empty(), "Fixture required")
    var slot := ProjectSettings.globalize_path("user://saves/" + SLOT.to_utf8_buffer().hex_encode() + ".ogs")
    DirAccess.make_dir_recursive_absolute(slot.get_base_dir())
    for suffix in ["", ".bak"]: originals[slot + suffix] = FileAccess.get_file_as_bytes(slot + suffix) if FileAccess.file_exists(slot + suffix) else null
    var f = FileAccess.open(slot, FileAccess.WRITE); f.store_buffer(FileAccess.get_file_as_bytes(fixture)); f.close()
    root.gui_embed_subwindows = true
    for locale in ["en", "es"]:
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/character_creation.tscn"); await settle(); await press("Party"); await load_slot()
        await press("PartyPanel/Roster/Advance1")
        var level: Window = current_scene.get_node("LevelUp")
        require(level.visible and "7" in level.get_node("Title").text, "Level 7 opens")
        await to_swaps(level)
        var discovery: OptionButton = level.get_node("SwapPage/SwapDiscovery")
        var with: OptionButton = level.get_node("SwapPage/SwapDiscoveryWith")
        require(discovery.visible and discovery.item_count == 3 and with.disabled, "Either discovery may be replaced, nothing until chosen")
        var old: String = items(discovery)[1]
        discovery.select(1); discovery.item_selected.emit(1); await settle()
        var ice := items(with).find("ice_storm")
        require(not with.disabled and ice > 0 and items(with).find("cone_of_cold") < 0, "Spells of the levels the Bard has slots for")
        with.select(ice); with.item_selected.emit(ice); await settle()
        await capture("discovery-swap-" + locale, level)
        for page in range(4):
            if not level.visible: break
            await press("LevelUp/Confirm")
        require(not level.visible, "The level-up completes")
        if locale == "en":
            # Level 8's Replace page lists the new discovery, not the old one.
            await press("PartyPanel/Roster/Advance1")
            await to_swaps(level)
            var now := items(level.get_node("SwapPage/SwapDiscovery"))
            require("ice_storm" in now and not old in now, "The new discovery is known: " + str(now))
            await press("LevelUp/Cancel")
            f = FileAccess.open(slot, FileAccess.WRITE); f.store_buffer(FileAccess.get_file_as_bytes(fixture)); f.close()
    restore_files(); print("Discovery swap view checks passed"); quit(0)
