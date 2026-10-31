extends SceneTree

# Every spell's look draws without an error and reaches the screen, and a real
# Magic Missile sends its three glowing darts across the battlefield.
const SPELL_EFFECTS := preload("res://scenes/spell_effects.gd")
const COLUMNS := 15
const ROWS_ON_PAGE := 10
const CELL := Vector2(128.0, 108.0)
const TILE := 44.0
const BACKGROUND := Color(0.13, 0.19, 0.23)

var fixtures := ""
var originals := {}
var save_path := ""
var rows: Array = []
var drawings: RefCounted = SPELL_EFFECTS.new()
var sheet: Control
var page := 0
var progress := 0.5
var travelling := false

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--component-fixtures="): fixtures = arg.trim_prefix("--component-fixtures=")
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

func settle(frames := 4) -> void:
    for frame in range(frames): await process_frame

func key(code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new()
        event.keycode = code; event.pressed = down
        root.push_input(event)
    await settle()

func click_enemy(combat: Control) -> void:
    var visuals: Dictionary = combat.battlefield_visuals()
    var size: float = combat.get_node("CombatLayout").base_tile * visuals["zoom"]
    var canvas: Control = combat.get_node("BattlefieldScroll/Canvas")
    for overlay in visuals["overlays"]:
        if overlay["kind"] in ["target", "effect_focus"]:
            var cell := Vector2(overlay["cell"])
            var at: Vector2 = canvas.get_global_transform_with_canvas() * ((cell + Vector2.ONE * 0.5) * size)
            for down in [true, false]:
                var event := InputEventMouseButton.new()
                event.position = at
                event.button_index = MOUSE_BUTTON_LEFT
                event.pressed = down
                root.push_input(event)
            await settle()
            return
    require(false, "A creature can be aimed at: " + str(visuals["overlays"]))

func screen() -> Image:
    RenderingServer.force_draw(false)
    return root.get_texture().get_image()

func cell_origin(index: int) -> Vector2:
    return Vector2(index % COLUMNS, index / COLUMNS) * CELL

func paint_sheet() -> void:
    sheet.draw_rect(Rect2(Vector2.ZERO, sheet.size), BACKGROUND)
    var shown := shown_rows()
    for index in range(mini(shown.size() - page * COLUMNS * ROWS_ON_PAGE, COLUMNS * ROWS_ON_PAGE)):
        var row: Dictionary = shown[page * COLUMNS * ROWS_ON_PAGE + index]
        var center := (cell_origin(index) + CELL * 0.5) / TILE
        var seconds: float = row["seconds"]
        drawings.draw_effect(sheet, {
            "travel": row["travel"] if travelling else "", "arrive": row["arrive"],
            "color": row["color"], "accent": row["accent"], "amount": row["amount"],
            "size": row["size"], "seed": float(index), "seconds": seconds,
            "time": progress if travelling else 1.0 + seconds * progress,
            "travel_seconds": 1.0, "from": center - Vector2(0.9, 0.0), "to": center + Vector2(0.9, 0.0),
            "arrivals": PackedVector3Array([Vector3(center.x, center.y, 0.0)]), "missed": false,
        }, TILE, 1.0)

func shown_rows() -> Array:
    return rows.filter(func(row): return row["travel"] != "") if travelling else rows

func differs_from_background(image: Image, area: Rect2i) -> bool:
    for y in range(area.position.y, area.end.y, 2):
        for x in range(area.position.x, area.end.x, 2):
            var pixel := image.get_pixel(x, y)
            if absf(pixel.r - BACKGROUND.r) + absf(pixel.g - BACKGROUND.g) + absf(pixel.b - BACKGROUND.b) > 0.08:
                return true
    return false

# Each spell, part way through, leaves a mark in its square of the sheet.
func check_every_look_shows() -> void:
    sheet = Control.new()
    sheet.size = Vector2(1920.0, 1080.0)
    sheet.draw.connect(paint_sheet)
    root.add_child(sheet)
    for mode in [false, true]:
        travelling = mode
        var shown := shown_rows()
        var on_page := COLUMNS * ROWS_ON_PAGE
        for number in range(ceili(float(shown.size()) / on_page)):
            page = number
            for step in [0.2, 0.5, 0.8]:
                progress = step
                sheet.queue_redraw()
                await settle(2)
                var image := screen()
                if is_equal_approx(step, 0.5):
                    for index in range(mini(shown.size() - number * on_page, on_page)):
                        var area := Rect2i(cell_origin(index), CELL)
                        require(differs_from_background(image, area), "A spell shows: " + str(shown[number * on_page + index]["spell"]))
    sheet.queue_free()
    await settle()

func purple_pixels(image: Image, untouched: Image) -> int:
    var found := 0
    for y in range(0, image.get_height(), 2):
        for x in range(0, image.get_width(), 2):
            var pixel := image.get_pixel(x, y)
            var earlier := untouched.get_pixel(x, y)
            var changed := absf(pixel.r - earlier.r) + absf(pixel.g - earlier.g) + absf(pixel.b - earlier.b) > 0.2
            if changed and pixel.b > 0.5 and pixel.b > pixel.g + 0.08 and pixel.r > pixel.g:
                found += 1
    return found

func check_magic_missile(combat: Control) -> void:
    save_path = ProjectSettings.globalize_path("user://checks/combat.save")
    DirAccess.make_dir_recursive_absolute(save_path.get_base_dir())
    for suffix in ["", ".bak"]:
        originals[save_path + suffix] = FileAccess.get_file_as_bytes(save_path + suffix) if FileAccess.file_exists(save_path + suffix) else null
    var file := FileAccess.open(save_path, FileAccess.WRITE)
    file.store_buffer(FileAccess.get_file_as_bytes(fixtures.path_join("wizard-free.save"))); file.close()
    combat.get_node("Load").pressed.emit()
    await settle()
    var untouched := screen()
    combat.get_node("MagicMissile").pressed.emit()
    await settle()
    # A dart can be aimed at the same creature again, and all three are aimed first.
    for dart in range(3):
        await click_enemy(combat)
    var in_play: PackedStringArray = combat.spell_effects_in_play()
    require(in_play == PackedStringArray(["magic_missile", "magic_missile", "magic_missile"]), "Magic Missile sends three darts: " + str(in_play))
    var seen := 0
    while seen < 12 and not combat.spell_effects_in_play().is_empty():
        await settle(1)
        seen = maxi(seen, purple_pixels(screen(), untouched))
    require(seen >= 12, "The glowing darts reach the screen: " + str(seen))
    # In seconds, not frames: the darts fly in real time however fast frames come.
    var give_up_at := Time.get_ticks_msec() + 10000
    while Time.get_ticks_msec() < give_up_at and not combat.spell_effects_in_play().is_empty():
        await settle(1)
    require(combat.spell_effects_in_play().is_empty(), "The darts land and the effect ends")

func run_checks() -> void:
    require(not fixtures.is_empty(), "--component-fixtures required")
    root.size = Vector2i(1920, 1080)
    change_scene_to_file("res://scenes/combat_demo.tscn")
    await settle()
    var combat := current_scene
    rows = combat.spell_effect_rows()
    for row in rows:
        require(drawings.has_method("m_" + str(row["arrive"])), "A drawing exists: " + str(row["arrive"]))
        require(row["travel"] == "" or drawings.has_method("t_" + str(row["travel"])), "A drawing exists: " + str(row["travel"]))
    await check_magic_missile(combat)
    combat.hide()
    await check_every_look_shows()
    restore_files()
    print("Spell effect view checks passed")
    quit(0)
