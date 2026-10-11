extends SceneTree

var saved_files := {}
var fixtures := ""
var captures := ""
var spanish := false

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--adrenaline-fixtures="): fixtures = arg.trim_prefix("--adrenaline-fixtures=")
        if arg.begins_with("--adrenaline-capture="): captures = arg.trim_prefix("--adrenaline-capture=")
    spanish = OS.get_environment("OPENGOLD_LANG") == "es"
    call_deferred("run_checks")

func restore_files() -> void:
    for path in saved_files:
        if saved_files[path] == null:
            if FileAccess.file_exists(path): DirAccess.remove_absolute(path)
        else:
            var file := FileAccess.open(path, FileAccess.WRITE)
            if file: file.store_buffer(saved_files[path]); file.close()
    saved_files.clear()

func require(ok: bool, message: String) -> void:
    if not ok:
        restore_files()
        push_error(message)
        quit(1)
        assert(ok, message)

func settle() -> void:
    for frame in range(4): await process_frame

func capture(name: String, modal: Window = null) -> void:
    if captures.is_empty(): return
    DirAccess.make_dir_recursive_absolute(captures)
    await RenderingServer.frame_post_draw
    var viewport: Viewport = modal if modal != null else root
    require(viewport.get_texture().get_image().save_png(captures.path_join(name + ".png")) == OK, "Capture rendered controls")

func run_checks() -> void:
    var path := ProjectSettings.globalize_path("user://checks/combat.save")
    for suffix in ["", ".bak"]:
        saved_files[path + suffix] = FileAccess.get_file_as_bytes(path + suffix) if FileAccess.file_exists(path + suffix) else null
    DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    change_scene_to_file("res://scenes/combat_demo.tscn")
    await settle()
    var combat := current_scene
    combat.set_process(false)
    var rush: Button = combat.get_node("AdrenalineRush")
    var dash: Button = combat.get_node("Dash")
    var modal: Window = combat.get_node("TemporaryHP")
    require(not combat.get_node("Save").visible and not combat.get_node("Load").visible, "Player save controls stay unavailable during combat")
    for fixture in ["initial", "pending"]:
        var bytes := FileAccess.get_file_as_bytes(fixtures.path_join("adrenaline-" + fixture + ".save"))
        require(not bytes.is_empty(), "Normally created Orc fixture is available")
        var file := FileAccess.open(path, FileAccess.WRITE)
        file.store_buffer(bytes); file.close()
        combat.get_node("Load").pressed.emit()
        await settle()
        require(rush.visible and dash.visible, "Approved feature and Dash controls are visible")
        require(rush.position.y == dash.position.y and rush.position.x > dash.position.x + dash.size.x, "Adrenaline Rush is beside Dash")
        require(rush.focus_mode == Control.FOCUS_ALL and not rush.tooltip_text.is_empty(), "Labeled feature is keyboard accessible and explains costs/recharge")
        if fixture == "initial":
            require(not rush.disabled and rush.text.contains("2/2"), "Ordinary Orc starts with its PB use pool")
            rush.grab_focus()
            await settle()
            var press := InputEventKey.new()
            press.keycode = KEY_SPACE; press.pressed = true
            root.push_input(press)
            var release := InputEventKey.new()
            release.keycode = KEY_SPACE
            root.push_input(release)
            await settle()
            require(rush.disabled and rush.text.contains("1/2") and not dash.disabled, "Button spends Bonus Action/use while retaining Action Dash")
            require(not modal.visible, "First pool needs no replacement decision")
            var roster: String = combat.get_node("StatusStack/Roster").text
            require(roster.contains("#f3d55b") and roster.contains("#80d99a") and roster.contains("[hint=") and roster.contains("Adrenaline" if not spanish else "adrenalina"), "Wounded HP is yellow; separate Temporary HP is green and names its source")
            for dimensions in [Vector2i(1920, 1080), Vector2i(1120, 800)]:
                root.size = dimensions
                await settle()
                require(rush.get_rect().end.y < combat.size.y and rush.get_rect().end.x < combat.size.x, "Controls fit the supported window sizes")
                await capture("rush-" + str(dimensions.x))
        else:
            require(modal.visible and rush.disabled and dash.disabled, "Pending replacement blocks other actions")
            require(modal.get_node("Text").text.contains("7") and modal.get_node("Text").text.contains("spell:fixture"), "Dialog shows current amount and source")
            require(modal.get_node("Text").text.contains("2") and modal.get_node("Text").text.contains("Orc"), "Dialog shows offered amount and source")
            require(modal.get_node("Keep").has_focus(), "Keyboard focus enters the replacement dialog")
            await capture("replacement", modal)
            combat.get_node("Save").pressed.emit()
            var pending := FileAccess.get_file_as_string(path)
            require(pending == bytes.get_string_from_utf8(), "Saving pending choice does not refund or reroll")
            combat.get_node("Load").pressed.emit()
            require(modal.visible and rush.text.contains("1/2"), "Reload restores the pending decision and spent use")
            modal.get_node("Use").grab_focus()
            await settle()
            var down := InputEventKey.new()
            down.keycode = KEY_ENTER; down.pressed = true
            modal.push_input(down)
            var up := InputEventKey.new()
            up.keycode = KEY_ENTER
            modal.push_input(up)
            await settle()
            require(not modal.visible and rush.text.contains("1/2") and not dash.disabled, "Keyboard Use new resolves without refunding costs")
            require(not combat.get_node("StatusStack/Roster").text.contains("spell:fixture"), "Smaller new pool replaces old source")
            combat.get_node("Load").pressed.emit()
            require(modal.visible, "Pending checkpoint can be restored again")
            modal.get_node("Keep").pressed.emit()
            require(not modal.visible and combat.get_node("StatusStack/Roster").text.contains("spell:fixture"), "Keep current preserves existing source and amount")
    restore_files()
    print("Adrenaline view checks passed: real buttons, keyboard replacement, HP presentation, checkpoint continuation")
    quit(0)
