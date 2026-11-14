extends SceneTree
# The original rules' actions on the combat screen (docs/ORIGINAL-RULES.md):
# a fight saved under the original rules loads under them, the Items list
# offers what a spell asks as it goes off (Fire Shield's flames), and using
# one casts it. Run with --original-fixtures=<folder of fire_shield.save>.
var fixtures := ""
var originals := {}
var save_path := ""

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--original-fixtures="): fixtures = arg.trim_prefix("--original-fixtures=")
    call_deferred("run_checks")

func restore_files() -> void:
    for path in originals:
        if originals[path] == null:
            if FileAccess.file_exists(path): DirAccess.remove_absolute(path)
        else:
            var file := FileAccess.open(path, FileAccess.WRITE)
            file.store_buffer(originals[path]); file.close()
    originals.clear()

func require(ok: bool, message: String) -> void:
    if not ok:
        restore_files()
        push_error(message)
        quit(1)
        assert(ok, message)

func settle() -> void:
    for frame in range(4): await process_frame

func item_index(label: String) -> int:
    var items: OptionButton = current_scene.get_node("ItemAction")
    for i in range(items.item_count):
        if items.get_item_text(i) == label: return i
    return -1

func use(label: String) -> void:
    var items: OptionButton = current_scene.get_node("ItemAction")
    var index := item_index(label)
    require(index >= 0, label + " is listed under Items")
    items.select(index); items.item_selected.emit(index); await settle()
    current_scene.get_node("UseItemAction").pressed.emit(); await settle()

func run_checks() -> void:
    save_path = ProjectSettings.globalize_path("user://checks/combat.save")
    for suffix in ["", ".bak"]:
        originals[save_path + suffix] = FileAccess.get_file_as_bytes(save_path + suffix) if FileAccess.file_exists(save_path + suffix) else null
    DirAccess.make_dir_recursive_absolute(save_path.get_base_dir())
    for locale in ["en", "es"]:
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn")
        await settle()
        var combat := current_scene
        combat.set_process(false)
        root.size = Vector2i(1120, 800)
        await settle()
        var bytes := FileAccess.get_file_as_bytes(fixtures.path_join("fire_shield.save"))
        require(not bytes.is_empty(), "The native fixture exists")
        var file := FileAccess.open(save_path, FileAccess.WRITE)
        file.store_buffer(bytes); file.close()
        combat.get_node("Load").pressed.emit()
        await settle()
        var hot := "Hot flames" if locale == "en" else "Llamas calientes"
        var cold := "Cold flames" if locale == "en" else "Llamas frías"
        require(item_index(hot) >= 0 and item_index(cold) >= 0, "Fire Shield's flames are offered: " + locale)
        await use(cold)
        var log_text: String = combat.get_node("LogStack/Log").get_parsed_text()
        var spell := "Fire Shield" if locale == "en" else "Escudo de fuego"
        require(log_text.contains(spell), "Choosing the flames casts Fire Shield: " + locale)
        require(item_index(hot) < 0, "The flames are no longer asked for: " + locale)
    TranslationServer.set_locale("en")
    restore_files()
    print("Original combat view checks passed")
    quit(0)
