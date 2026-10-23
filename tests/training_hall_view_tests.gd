extends SceneTree

# Original-data integration: OPENGOLD_GAME_DIR must point at the installed game the
# fixture was written for. The fixture is a created party of Fighters at the
# Fighters' training hall door, (8,1) of the training area (OPENGOLD_HALL_FIXTURE
# from opengold_expedition_tests). Answering YES lists the hall's services in
# the dialogue, as the temple does; a refused session changes nothing, a session
# and a lesson are bought, then the party leaves. --shots=DIR also saves screenshots of each step.
const SLOT = "Training hall fixture"
var fixture := ""
var shots := ""
var originals := {}

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--hall-fixture="): fixture = arg.trim_prefix("--hall-fixture=")
        if arg.begins_with("--shots="): shots = arg.trim_prefix("--shots=")
    call_deferred("run_checks")

func slot_path(slot: String) -> String:
    return ProjectSettings.globalize_path("user://saves/" + slot.to_utf8_buffer().hex_encode() + ".ogs")

func restore_files() -> void:
    for path in originals:
        if originals[path] == null:
            if FileAccess.file_exists(path): DirAccess.remove_absolute(path)
        else:
            var f := FileAccess.open(path, FileAccess.WRITE); f.store_buffer(originals[path]); f.close()
    originals.clear()

func require(ok: bool, message: String) -> void:
    if not ok:
        restore_files(); push_error(message); quit(1); assert(ok, message)

func settle() -> void:
    for frame in range(5): await process_frame

func capture(name: String) -> void:
    if shots.is_empty(): return
    # Only the town is drawn: the creation screen behind it stays visible
    # under this script, which the game's own window never shows.
    var hidden := []
    for child in current_scene.get_children():
        if child is CanvasItem and child.name != "CampaignTown" and child.visible:
            child.visible = false; hidden.append(child)
    for frame in range(30): await process_frame
    RenderingServer.force_draw()
    root.get_texture().get_image().save_png(shots.path_join(name + ".png"))
    for child in hidden: child.visible = true

func press(path: String) -> void:
    var b: Button = current_scene.get_node(path)
    require(not b.disabled, "Disabled control: " + path)
    b.pressed.emit(); await settle()

func town() -> Control:
    return current_scene.get_node("CampaignTown")

func dialogue() -> String:
    return town().get_node("Dialogue").get_parsed_text()

func choices() -> ItemList:
    return town().get_node("Choices")

func choice_index(text: String) -> int:
    for i in range(choices().item_count):
        if choices().get_item_text(i) == text: return i
    return -1

func choose(text: String) -> void:
    var index := choice_index(text)
    require(index >= 0, "Choice listed: " + text + " in " + str(range(choices().item_count).map(func(i): return choices().get_item_text(i))))
    choices().select(index)
    await press("CampaignTown/Continue")
    for frame in range(20):
        await process_frame

func open_slot(slot: String) -> void:
    var list: ItemList = current_scene.get_node("SaveSlots/Slots")
    var index := -1
    for i in range(list.item_count):
        if list.get_item_text(i) == slot: index = i
    require(index >= 0, "Save slot listed: " + slot)
    list.select(index); list.item_selected.emit(index)
    await press("SaveSlots/Action"); await press("SaveSlots/Action")
    require(not current_scene.get_node("SaveSlots").visible, "Save loaded: " + slot)

# Step through the door and wait for the hall's YES/NO question.
func ask_to_train(yes: String) -> void:
    await press("CampaignTown/Forward")
    for prompt in range(40):
        if choice_index(yes) >= 0: return
        if town().get_node("Continue").disabled:
            for frame in range(10): await process_frame
            continue
        await press("CampaignTown/Continue")
    require(false, "The hall asks whether to train: at " + town().get_node("Coordinates").text + " / " + dialogue())

func run_checks() -> void:
    require(not fixture.is_empty(), "--hall-fixture required")
    var path := slot_path(SLOT)
    DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    for suffix in ["", ".bak"]:
        originals[path + suffix] = FileAccess.get_file_as_bytes(path + suffix) if FileAccess.file_exists(path + suffix) else null
    root.gui_embed_subwindows = true
    for locale in ["en", "es"]:
        var f := FileAccess.open(path, FileAccess.WRITE); f.store_buffer(FileAccess.get_file_as_bytes(fixture)); f.close()
        TranslationServer.set_locale(locale)
        var en: bool = locale == "en"
        change_scene_to_file("res://scenes/character_creation.tscn"); await settle()
        await press("Party")
        await press("PartyPanel/Load"); await open_slot(SLOT)
        await press("PartyPanel/ActionRow2/Explore")
        await ask_to_train("YES")
        await capture("hall-%s-1-question" % locale)
        await choose("YES")
        var session := "Arden: session of experience, 150 XP (1,000 gp)" if en else "Arden: sesión de experiencia, 150 PX (1.000 po)"
        require(choice_index(session) >= 0, "Arden's session is offered")
        var bryn := "Bryn: session of experience (advance a level first)" if en else "Bryn: sesión de experiencia (primero sube de nivel)"
        require(choice_index(bryn) >= 0, "Bryn must advance first")
        require(choice_index("Cora: session of experience (bought at this level)" if en else "Cora: sesión de experiencia (ya comprada en este nivel)") >= 0,
                "Cora bought this level's session")
        require(dialogue().contains("Fighters' hall" if en else "escuela de guerreros"), "The hall introduces itself")
        await capture("hall-%s-2-services" % locale)
        await choose(bryn)
        require(dialogue().contains("advance from the party roster first" if en else "primero sube de nivel desde la lista del grupo"),
                "Choosing Bryn's session explains the refusal")
        require(choice_index(session) >= 0, "The refusal changes nothing: Arden's session is still offered")
        await choose(session)
        require(dialogue().contains("Arden gains 150 experience." if en else "Arden gana 150 de experiencia."), "The session is bought")
        require(choice_index("Arden: session of experience (bought at this level)" if en else "Arden: sesión de experiencia (ya comprada en este nivel)") >= 0,
                "Arden's session now shows as bought at this level")
        await capture("hall-%s-3-session-bought" % locale)
        await choose("Darin: skill lesson" if en else "Darin: lección de habilidad")
        require(choice_index("Stealth (250 gp)" if en else "Sigilo (250 po)") >= 0, "A Ranger's Stealth is taught here")
        await capture("hall-%s-4-lessons" % locale)
        await choose("Stealth (250 gp)" if en else "Sigilo (250 po)")
        require(dialogue().contains("Darin learns Stealth." if en else "Darin aprende Sigilo."), "The lesson is learned")
        await capture("hall-%s-5-lesson-learned" % locale)
        await choose("Leave" if en else "Salir")
        for frame in range(60): await process_frame
        require(not choices().visible or choice_index("Leave" if en else "Salir") < 0, "The party leaves the hall")
        await capture("hall-%s-6-left" % locale)
    restore_files()
    print("Training hall view checks passed")
    quit(0)
