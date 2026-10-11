extends SceneTree

# Items row (GEAR-1): the gear actions a character can take now are listed in a
# dropdown with a Use button, and the prompt says how to finish each action.
# Loads saves written by opengold_playtest_fixtures.
var fixtures := ""
var path := ""
var original: Variant = null

func _init() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--playtest-fixtures="): fixtures = arg.trim_prefix("--playtest-fixtures=")
    call_deferred("run_checks")

func cleanup() -> void:
    if original == null:
        if FileAccess.file_exists(path): DirAccess.remove_absolute(path)
    else:
        var file := FileAccess.open(path, FileAccess.WRITE); file.store_buffer(original)

func require(ok: bool, message: String) -> void:
    if not ok:
        cleanup(); push_error(message); quit(1); assert(ok, message)

func settle() -> void:
    for frame in range(8): await process_frame

func key(code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new(); event.keycode = code; event.pressed = down; root.push_input(event)
    await settle()

func load_fixture(name: String) -> void:
    var bytes := FileAccess.get_file_as_bytes(fixtures.path_join(name + ".save"))
    require(not bytes.is_empty(), "Play-test fixture " + name + " exists")
    var file := FileAccess.open(path, FileAccess.WRITE); file.store_buffer(bytes); file.close()
    current_scene.get_node("Load").pressed.emit(); await settle()

func prompt() -> String:
    return current_scene.get_node("Prompt").text

func item_index(label: String) -> int:
    var items: OptionButton = current_scene.get_node("ItemAction")
    for i in range(items.item_count):
        if items.get_item_text(i).contains(tr(label)): return i
    return -1

func use(label: String) -> void:
    var items: OptionButton = current_scene.get_node("ItemAction")
    var index := item_index(label)
    require(index >= 0, label + " is listed under Items")
    items.select(index); items.item_selected.emit(index); await settle()
    current_scene.get_node("UseItemAction").pressed.emit(); await settle()

func click(cell: Vector2i) -> void:
    var canvas: Control = current_scene.get_node("BattlefieldScroll/Canvas")
    var point: Vector2 = canvas.get_global_transform_with_canvas() * ((Vector2(cell) + Vector2(0.5, 0.5)) * current_scene.cell_pixels())
    for down in [true, false]:
        var event := InputEventMouseButton.new(); event.button_index = MOUSE_BUTTON_LEFT
        event.position = point; event.pressed = down; root.push_input(event, true)
    await settle()

func run_checks() -> void:
    path = ProjectSettings.globalize_path("user://checks/combat.save")
    original = FileAccess.get_file_as_bytes(path) if FileAccess.file_exists(path) else null
    DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    for locale in ["en", "es"]:
        TranslationServer.set_locale(locale)
        change_scene_to_file("res://scenes/combat_demo.tscn"); await settle()
        current_scene.set_process(false)
        for size in [Vector2i(1120, 800), Vector2i(1920, 1080)]:
            root.size = size; await settle()
            await load_fixture("gear-torch")
            var items: OptionButton = current_scene.get_node("ItemAction")
            var button: Button = current_scene.get_node("UseItemAction")
            require(items.visible and button.visible and current_scene.get_node("ItemActionLabel").visible, "Items row shows when gear can be used")
            require(item_index("Torch attack") >= 0, "Torch attack is listed beside an adjacent troll")
            var oil := items.get_item_text(item_index("Throw Oil"))
            require(oil == tr("{action} ({count} left)").format({"action": tr("Throw Oil"), "count": 1}), "Flasks show how many are left: " + oil)
            require(prompt() == tr("Selected: {action}. Click a highlighted square.").format({"action": tr("Move")}), "Move asks for a square: " + prompt())
            require(button.get_global_rect().end.y <= root.size.y and items.get_global_rect().end.x <= button.get_global_rect().position.x, "Items controls fit without overlap")
            require(not items.get_global_rect().intersects(current_scene.get_node("ThrownWeapon").get_global_rect()), "Items row sits apart from the thrown-weapon row")
            require(current_scene.get_node("LogStack/Log").get_global_rect().position.y >= items.get_rect().end.y, "Combat log moves below the Items row")
            require(current_scene.get_node("LogStack/Log").get_global_rect().end.y <= current_scene.get_node("Footer").position.y, "Combat log remains above footer")
            require(button.get_rect().end.x <= current_scene.get_node("BattlefieldScroll").get_rect().end.x, "Items row stays inside combat column")
            await use("Torch attack")
            require(prompt() == tr("Selected: {action}. Click a highlighted creature.").format({"action": tr("Torch attack")}), "A targeted item asks for a creature: " + prompt())
            await key(KEY_ESCAPE)
            for i in range(40):
                if prompt().contains(tr("Dodge")): break
                await key(KEY_A)
            require(prompt() == tr("Selected: {action}. Press Space to use it.").format({"action": tr("Dodge")}), "An untargeted action asks for Space: " + prompt())
            await load_fixture("gear-ally")
            await click(Vector2i(1, 3))
            require(prompt() == tr("It is not {name}'s turn.").format({"name": "Ally"}), "Clicking an ally shows it: " + prompt())
            await use("Torch attack")
            await click(Vector2i(2, 1))
            require(current_scene.get_node("LogStack/Log").get_parsed_text().contains("Hero -> Troll"), "After looking at an ally, a chosen action still strikes the clicked target")
            await click(Vector2i(1, 3))
            current_scene.get_node("End").pressed.emit(); await settle()
            require(current_scene.get_node("StatusStack/Turn").text.contains(tr("Troll")), "The troll acts next: " + current_scene.get_node("StatusStack/Turn").text)
            require(prompt() == tr("Enemy turn"), "An enemy's turn says so even with an ally selected: " + prompt())
            await load_fixture("gear-bow")
            require(item_index("Shoot") < 0, "A shield keeps the bow from being shot")
            await use("Take off shield")
            # Taking off a shield spends the Action, so Shoot waits for the next turn.
            require(item_index("Take off shield") < 0 and current_scene.get_node("LogStack/Log").get_parsed_text().contains(tr("{name} takes off a shield to use a bow.").format({"name": "Hero"})), "Use takes the shield off at once")
    cleanup(); print("Items controls passed"); quit()
