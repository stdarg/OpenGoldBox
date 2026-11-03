extends SceneTree

# Plays the whole campaign in the game window with the game's controls: arrow
# keys and the Look, Camp and Continue controls in exploration, Q for Quick in
# combat. The route comes from tools/playtest.py campaign, which records it from
# the expedition test's whole-map run: moves, the story's answers, camps and
# purchases, cut into segments wherever that run changes the party by hand (its
# levels and healing). Each segment loads its save through the Load screen. A
# random encounter the recording did not meet is answered as a player would,
# a segment that goes wrong is played again from its save, and the report names
# every fault, stuck screen, lost fight and step off the route. Not a CTest check.
const STUCK_SECONDS := 120.0
const FIGHT_SECONDS := 600.0
const ATTEMPTS := 3
# A lost fight is a reload, not a fault; the finale on Quick is often lost.
const RELOADS := 8
const FINALE_RELOADS := 24
const EASY_AFTER := 12
const STANDOFF_ROUNDS := 4
const MAX_HUNTS := 3
const QUICK_PROMPTS := ["Keep initiative", "Decline reaction", "Keep current", "Continue"]

var route := ""
var out := ""
var first_stage := ""
var last_stage := ""
var first_segment := ""
var time_scale := 8.0
var report := PackedStringArray()
var shots := 0
var seen := {}
var area_names := {}
var fights := 0
var fight_rounds := 0
var spell_lines := {}
var problems := 0
var hunts := 0
var walked := {}
var fought := false
var losses := 0
var retry := 0
var need_knock := false
var difficulty := ""
var changed_difficulty := false
var difficulty_before := ""
var skipped := 0
var only := PackedStringArray()
var rests := 0

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--route="): route = arg.trim_prefix("--route=")
        if arg.begins_with("--playtest-out="): out = arg.trim_prefix("--playtest-out=")
        if arg.begins_with("--from="): first_stage = arg.trim_prefix("--from=")
        if arg.begins_with("--to="): last_stage = arg.trim_prefix("--to=")
        if arg.begins_with("--segment="): first_segment = arg.trim_prefix("--segment=")
        if arg.begins_with("--only="): only = arg.trim_prefix("--only=").split(",")
        if arg.begins_with("--difficulty="): difficulty = arg.trim_prefix("--difficulty=")
        if arg.begins_with("--time-scale="): time_scale = float(arg.trim_prefix("--time-scale="))
    call_deferred("run")

func say(line: String) -> void:
    report.append(line)
    var f := FileAccess.open(out.path_join("report.txt"), FileAccess.WRITE)
    f.store_string("\n".join(report) + "\n"); f.close()

func problem(line: String) -> void:
    problems += 1
    say("  PROBLEM: " + line)

func settle(frames := 3) -> void:
    for frame in range(frames): await process_frame

func capture(name: String) -> void:
    await settle()
    RenderingServer.force_draw(false)
    shots += 1
    var image := root.get_texture().get_image()
    image.resize(image.get_width() / 2, image.get_height() / 2)
    var file := "%05d-%s.jpg" % [shots, name.validate_filename().left(40)]
    image.save_jpg(out.path_join(file), 0.75)
    say("  [" + file + "]")

func key(code: Key) -> void:
    for down in [true, false]:
        var event := InputEventKey.new()
        event.keycode = code; event.pressed = down
        root.push_input(event)
    await settle()

func node(path: String) -> Node:
    return current_scene.get_node_or_null(path)

func has_town() -> bool:
    return current_scene.has_node("CampaignTown")

func town() -> Control:
    return current_scene.get_node("CampaignTown")

func in_combat() -> bool:
    return current_scene.has_node("CampaignCombat") and current_scene.get_node("CampaignCombat").is_visible_in_tree()

func combat() -> Control:
    return current_scene.get_node("CampaignCombat")

func press(path: String) -> bool:
    var b: Button = current_scene.get_node_or_null(path)
    if b == null or b.disabled or not b.is_visible_in_tree():
        problem("control not available: " + path)
        return false
    b.pressed.emit()
    await settle(4)
    return true

func dialogue() -> String:
    return (town().get_node("Dialogue") as RichTextLabel).get_parsed_text()

func slot_path(slot: String) -> String:
    return ProjectSettings.globalize_path("user://saves/" + slot.to_utf8_buffer().hex_encode() + ".ogs")

func load_segment(file: String) -> bool:
    change_scene_to_file("res://scenes/character_creation.tscn")
    await settle(20)
    var slot := "Route " + file.get_basename()
    var path := slot_path(slot)
    DirAccess.make_dir_recursive_absolute(path.get_base_dir())
    var f := FileAccess.open(path, FileAccess.WRITE)
    f.store_buffer(FileAccess.get_file_as_bytes(route.path_join(file))); f.close()
    await press("Party")
    await press("PartyPanel/Load")
    var list: ItemList = current_scene.get_node("SaveSlots/Slots")
    for i in range(list.item_count):
        if list.get_item_text(i) == slot:
            list.select(i); list.item_selected.emit(i)
    await press("SaveSlots/Action"); await press("SaveSlots/Action")
    if has_town() and not town().visible:
        await press("PartyPanel/ActionRow2/Explore")
    if not has_town() or not town().visible:
        problem("save " + file + " did not load")
        await capture("load-failed")
        return false
    return true

# --- Combat ---

func press_any(parent: Node, texts: Array) -> bool:
    for child in parent.get_children():
        if child is Button and child.is_visible_in_tree() and not child.disabled and texts.has(child.text):
            child.pressed.emit()
            await settle(4)
            return true
        if await press_any(child, texts): return true
    return false

func fight_turn_text() -> String:
    return combat().get_node("StatusStack/Turn").text if has_combat() else ""

func has_combat() -> bool:
    return current_scene.has_node("CampaignCombat")

# The defeat screen stays up until a save is loaded.
func defeated() -> bool:
    return has_combat() and fight_turn_text().contains("defeat")

# Remembers each spell the log names.
func note_spells() -> void:
    for line in (combat().get_node("LogStack/Log") as RichTextLabel).get_parsed_text().split("\n"):
        if line.contains(" casts ") and not spell_lines.has(line):
            spell_lines[line] = true

func round_number() -> int:
    return int(fight_turn_text().trim_prefix("Round ").get_slice(" ", 0))

func prompt() -> String:
    return combat().get_node("Prompt").text

func log_text() -> String:
    return (combat().get_node("LogStack/Log") as RichTextLabel).get_parsed_text()

# A log line other than a member's turn starting: something happened.
func last_event() -> String:
    var lines := log_text().split("\n", false)
    for i in range(lines.size() - 1, -1, -1):
        if not lines[i].ends_with(" acts."): return lines[i]
    return ""

func click(cell: Vector2i) -> void:
    var canvas: Control = combat().get_node("BattlefieldScroll/Canvas")
    var tile: float = combat().cell_pixels()
    var point := canvas.get_global_transform_with_canvas() * ((Vector2(cell) + Vector2(0.5, 0.5)) * tile)
    # A square scrolled out of view would land on whatever control is there.
    if not (combat().get_node("BattlefieldScroll") as Control).get_global_rect().has_point(point): return
    for down in [true, false]:
        var event := InputEventMouseButton.new()
        event.button_index = MOUSE_BUTTON_LEFT; event.pressed = down; event.position = point
        root.push_input(event, true)
    await settle(4)

func enemy_distance(cell: Vector2i, enemies: Array) -> int:
    var best := 1 << 30
    for enemy in enemies:
        best = mini(best, maxi(absi(enemy.x - cell.x), absi(enemy.y - cell.y)))
    return best

# Cycles the actions with A and clicks an enemy, as a player does.
func try_attack() -> bool:
    var first := ""
    for i in range(30):
        await key(KEY_A)
        if not has_combat(): return true
        var shown := prompt()
        if shown == first: return false
        if first.is_empty(): first = shown
        if not shown.contains("Click a highlighted creature"): continue
        for cell in combat().enemy_cells():
            var before := log_text()
            await click(cell)
            if not has_combat() or log_text() != before: return true
    return false

# In Move mode, clicks the highlighted square nearest an enemy.
func approach() -> void:
    for i in range(30):
        if prompt().contains("Click a highlighted square"): break
        await key(KEY_A)
    if not prompt().contains("Click a highlighted square"): return
    var me: Vector2i = combat().selected_character_cell()
    var enemies: Array = combat().enemy_cells()
    if enemies.is_empty(): return
    # Toward the enemy, but around walls: a square already stood on counts as
    # further away, so a walk stopped by a wall turns along it.
    var score := func(cell: Vector2i) -> int:
        return enemy_distance(cell, enemies) + 4 * int(walked.get(cell, 0))
    walked[me] = int(walked.get(me, 0)) + 1
    var squares := []
    for dx in range(-6, 7):
        for dy in range(-6, 7):
            if dx != 0 or dy != 0: squares.append(me + Vector2i(dx, dy))
    squares.sort_custom(func(a, b): return score.call(a) < score.call(b))
    var acting: int = combat().selected_character_id()
    for cell in squares:
        await click(cell)
        if not has_combat(): return
        if combat().selected_character_id() != acting:
            await click(me)
            continue
        if combat().selected_character_cell() != me: return

# Quick holds its ground while no one sees an enemy (docs/AI.md): the player
# takes the party back and goes after it until something happens.
func hunt() -> void:
    say("  standoff: %s; going after the enemy" % fight_turn_text().get_slice("\n", 0))
    var event := last_event()
    for turns in range(240):
        if not has_combat() or defeated() or last_event() != event: break
        await press_any(combat(), QUICK_PROMPTS + ["Take control"])
        if not has_combat(): break
        var end: Button = combat().get_node("End")
        if not end.is_visible_in_tree() or end.disabled:
            await settle(4)
            continue
        var near: bool = enemy_distance(combat().selected_character_cell(), combat().enemy_cells()) <= 1
        # Beside an enemy around a wall's corner neither can strike: step round it.
        if not near or not await try_attack():
            await approach()
        if has_combat() and end.is_visible_in_tree() and not end.disabled:
            await key(KEY_ENTER)
    if has_combat() and not defeated(): await key(KEY_Q)

# Plays one fight on Quick. Returns false when it was lost or never ended.
func play_fight() -> bool:
    fights += 1
    var name := "fight-%d" % fights
    await capture(name + "-start")
    var started := Time.get_ticks_msec()
    var quick_pressed := false
    var last_log := ""
    var last_change := started
    var last_event_seen := ""
    var event_round := 0
    var fight_hunts := 0
    var held_back := 0
    walked.clear()
    while has_combat() and not defeated():
        if not quick_pressed and (combat().get_node("End") as Button).is_visible_in_tree():
            # On Easy the same moves after a reload lose the same way: a retry
            # holds members back at first.
            if held_back < retry % 6:
                held_back += 1
                await key(KEY_ENTER)
                continue
            await key(KEY_Q)
            quick_pressed = true
            # The party's casters use their spells, as a player would have them.
            var want_magic := retry % 12 < 6
            if has_combat() and (combat().get_node("QuickMagic") as Button).text.ends_with("Off") == want_magic:
                await key(KEY_M)
        if not has_combat(): break
        await press_any(combat(), QUICK_PROMPTS)
        if not has_combat(): break
        note_spells()
        var text := log_text()
        if text != last_log:
            last_log = text
            last_change = Time.get_ticks_msec()
        var event := last_event()
        if event != last_event_seen:
            last_event_seen = event
            event_round = round_number()
        elif round_number() - event_round >= STANDOFF_ROUNDS:
            hunts += 1
            fight_hunts += 1
            if fight_hunts > MAX_HUNTS:
                problem("no way to the enemy after %d chases: %s; fleeing" % [MAX_HUNTS, fight_turn_text().get_slice("\n", 0)])
                say("  enemies at %s; selected %s: %s" % [combat().enemy_cells(), combat().selected_character_cell(), prompt()])
                await capture(name + "-flee")
                for wait in range(600):
                    if not has_combat() or defeated(): break
                    await press_any(combat(), ["Take control"] + QUICK_PROMPTS)
                    if has_combat() and await press_any(combat(), ["Flee"]): break
                    await settle(2)
                event_round = round_number() + 1000
                continue
            await hunt()
            if not has_combat(): break
            last_event_seen = last_event()
            event_round = round_number()
            continue
        var now := Time.get_ticks_msec()
        if now - last_change > STUCK_SECONDS * 1000.0:
            problem("fight stuck: nothing happened for %d s; %s / prompt: %s" % [STUCK_SECONDS, fight_turn_text().get_slice("\n", 0), combat().get_node("Prompt").text])
            await capture(name + "-stuck")
            return false
        if now - started > FIGHT_SECONDS * 1000.0:
            problem("fight never ended: " + fight_turn_text().get_slice("\n", 0))
            await capture(name + "-long")
            return false
        await settle(2)
    if has_combat():
        await capture(name + "-lost")
        losses += 1
        say("  lost " + name + ": " + fight_turn_text().get_slice("\n", 0))
        return false
    fought = true
    # The opening rolls, to compare fights across reloads.
    var rolls := PackedStringArray()
    for line in last_log.split("\n", false):
        if line.contains(": d20 ") and rolls.size() < 6:
            rolls.append(line.get_slice(": d20 ", 1).get_slice(" ", 0))
    say("  opening rolls: " + " ".join(rolls))
    say("  won " + name + " in %.0f s" % ((Time.get_ticks_msec() - started) / 1000.0))
    return true

# --- Exploration ---

func cleaned(text: String) -> String:
    var result := ""
    for c in text.to_lower():
        if c >= "a" and c <= "z" or c >= "0" and c <= "9": result += c
    return result

func same_dialogue(shown: String, recorded: String) -> bool:
    var a := cleaned(shown)
    var b := cleaned(recorded)
    return a == b or (not b.is_empty() and (a.contains(b) or b.contains(a)))

# The next recorded answer to what is on screen, before the next move.
func find_answer(items: Array, at: int, kind: String) -> int:
    var i := at
    while i < items.size():
        var item: Dictionary = items[i]
        if item.t == "explore" or item.t == "camp" or item.t == "segment": return -1
        if item.t == kind and same_dialogue(dialogue(), item.dialogue): return i
        i += 1
    return -1

func answer_choice(items: Array, at: int) -> int:
    var list: ItemList = town().get_node("Choices")
    var found := find_answer(items, at, "choose")
    var selection := 0
    if found >= 0:
        selection = int(items[found].index)
        at = found + 1
    elif list.is_visible_in_tree() and list.item_count > 1:
        # Off the recording: the route's last answer to this question in the
        # segment, or else what a player would pick. A door relocks when its
        # area is loaded, so one the route opened earlier may need opening again.
        var known := ""
        for i in range(items.size() - 1, -1, -1):
            var item: Dictionary = items[i]
            if item.t == "choose" and same_dialogue(dialogue(), item.dialogue):
                known = str(item.choice).to_lower()
                break
        # A wizard lock holds against force: without Knock the party leaves it
        # and camps until the spell is back.
        var wanted := [known, "knock", "pick", "bash", "fight", "combat", "attack"]
        if dialogue().begins_with("Wizard locked."):
            wanted = ["knock", "exit"]
            need_knock = true
        var best := wanted.size()
        for i in range(list.item_count):
            var rank := wanted.find(list.get_item_text(i).to_lower())
            if rank >= 0 and rank < best:
                best = rank
                selection = i
        var shown := dialogue().replace("\n", " ").left(120)
        if not seen.has(shown):
            seen[shown] = true
            say("  off the recording: " + shown + " -> " + list.get_item_text(selection))
    if list.is_visible_in_tree() and selection < list.item_count:
        list.select(selection)
    await note_screen()
    await press("CampaignTown/Continue")
    return at

func note_screen() -> void:
    var shown := dialogue()
    if shown.contains("TYRANTHRAXUS") and not seen.has("ending" + shown.left(30)):
        seen["ending" + shown.left(30)] = true
        say("  ending text: " + shown.replace("\n", " ").left(140))
    var word := cleaned(shown).left(24)
    if not shown.is_empty() and not seen.has("shot" + word):
        seen["shot" + word] = true
        await capture(word)

func rest_dialog() -> Window:
    return town().get_node("RestDialog")

func window_shown(name: String) -> Window:
    var w: Window = town().get_node_or_null(name)
    return w if w != null and w.visible else null

# Spends what a Short Rest offers, and keeps what a rest's choices offer.
func answer_rest_windows() -> bool:
    var spells := window_shown("RestSpells")
    if spells:
        (spells.find_child("Cancel", true, false) as Button).pressed.emit()
        await settle(4)
        return true
    var training := window_shown("RestTraining")
    if training:
        (training.find_child("Cancel", true, false) as Button).pressed.emit()
        await settle(4)
        return true
    var rest := window_shown("RestDialog")
    if not rest: return false
    var heal: Button = rest.find_child("Heal", true, false)
    var finish: Button = rest.find_child("Finish", true, false)
    var start: Button = rest.find_child("Start", true, false)
    # Before a rest the Finish button reads Cancel; Start shows only then.
    if start.is_visible_in_tree() and not start.disabled:
        start.pressed.emit()
        await settle(4)
        return true
    if not start.is_visible_in_tree() and finish.is_visible_in_tree() and not finish.disabled:
        var members: ItemList = rest.find_child("Members", true, false)
        for i in range(members.item_count):
            members.select(i); members.item_selected.emit(i)
            await settle(2)
            for spend in range(20):
                if heal.disabled or not heal.is_visible_in_tree(): break
                heal.pressed.emit()
                await settle(2)
        finish.pressed.emit()
        await settle(4)
        return true
    say("  camp dialog offers nothing: " + (rest.find_child("Result", true, false) as Label).text)
    rest.hide()
    await settle(4)
    return true

func idle() -> bool:
    return has_town() and town().is_visible_in_tree() and not (town().get_node("Left") as Button).disabled \
        and not window_shown("RestDialog") and not window_shown("RestSpells") and not window_shown("RestTraining")

# Plays whatever the last command started until exploration resumes. Returns
# the route position, or -1 when the segment has to start again.
func play_on(items: Array, at: int) -> int:
    var last_change := Time.get_ticks_msec()
    var last_state := ""
    while true:
        if has_combat():
            if not await play_fight(): return -1
            last_change = Time.get_ticks_msec()
            continue
        if not has_town():
            problem("the town screen is gone")
            return -1
        var speaker: String = (town().get_node("Speaker") as Label).text
        if speaker == "Unable to continue":
            problem("script stopped: " + dialogue().replace("\n", " "))
            await capture("fault")
            return -1
        if await answer_rest_windows():
            last_change = Time.get_ticks_msec()
            continue
        if idle(): return at
        var go: Button = town().get_node("Continue")
        var state := dialogue() + str(go.disabled)
        if state != last_state:
            last_state = state
            last_change = Time.get_ticks_msec()
        if Time.get_ticks_msec() - last_change > STUCK_SECONDS * 1000.0:
            problem("stuck: " + dialogue().replace("\n", " "))
            await capture("stuck")
            return -1
        if go.is_visible_in_tree() and not go.disabled:
            if speaker.begins_with("Shop"):
                at = await shop(items, at)
            elif (town().get_node("Answer") as Control).is_visible_in_tree():
                at = await answer_input(items, at)
            else:
                at = await answer_choice(items, at)
            continue
        await settle(2)
    return at

func answer_input(items: Array, at: int) -> int:
    var found := find_answer(items, at, "input")
    var value := "0"
    if found >= 0:
        value = items[found].value
        at = found + 1
    (town().get_node("Answer") as LineEdit).text = value
    await note_screen()
    await press("CampaignTown/Continue")
    return at

func shop(items: Array, at: int) -> int:
    await note_screen()
    while at < items.size() and items[at].t == "buy":
        await key(KEY_1 + int(items[at].slot))
        var list: ItemList = town().get_node("Choices")
        list.select(int(items[at].item))
        await press("CampaignTown/Continue")
        at += 1
    if at < items.size() and items[at].t == "leave_shop": at += 1
    await press("CampaignTown/LeaveShop")
    return at

func pose() -> Vector2i:
    var text: String = (town().get_node("Coordinates") as Label).text
    var numbers := text.get_slice("(", 1).get_slice(")", 0).split(",")
    return Vector2i(int(numbers[0]), int(numbers[1]))

func facing() -> int:
    return int(town().call("tour_visuals").get("facing", 0))

const STEP_X := [0, 1, 0, -1]
const STEP_Y := [-1, 0, 1, 0]

func face(direction: int) -> void:
    for turns in range(4):
        if facing() == direction: return
        await key(KEY_RIGHT)
        await settle(4)
        if not idle(): return

# Camps once: a Long Rest when anyone may take one, otherwise a Short Rest.
func rest_once(items: Array, at: int) -> int:
    await key(KEY_C)
    await settle(4)
    var rest := window_shown("RestDialog")
    if not rest:
        say("  Camp opened no dialog: idle %s, Camp button disabled %s, Movement '%s', Speaker '%s'" % [idle(), (town().get_node("Camp") as Button).disabled, (town().get_node("Movement") as Label).text, (town().get_node("Speaker") as Label).text])
        return at
    var kind: OptionButton = rest.find_child("Kind", true, false)
    var start: Button = rest.find_child("Start", true, false)
    if not kind.disabled:
        kind.select(1); kind.item_selected.emit(1)
        await settle(4)
        if start.disabled:
            kind.select(0); kind.item_selected.emit(0)
            await settle(4)
    return await play_on(items, at)

# A disorientation square sends the party to another at random, so the dice may
# not land where the recording's did. As in the expedition test, it steps off and
# back on until they do.
func disorient_until(items: Array, at: int, target: Vector2i) -> int:
    for tries in range(60):
        if pose() == target or not dialogue().contains("SUDDEN DISORIENTATION"): return at
        var square := pose()
        var way_off := -1
        for d in range(4):
            await face(d)
            if idle():
                await key(KEY_UP)
                await settle(6)
            at = await play_on(items, at)
            if at < 0: return -1
            if pose() != square:
                way_off = d
                break
        if way_off < 0: return at
        await face((way_off + 2) % 4)
        if idle():
            await key(KEY_UP)
            await settle(6)
        at = await play_on(items, at)
        if at < 0: return -1
    say("  60 tries did not land on %s" % target)
    return at

# Walks back to a recorded square when a fight or an interruption moved the
# party, with the map the player has: walls and doors seen so far.
func walk_back(items: Array, at: int, target: Vector2i) -> int:
    var knock_camps := 0
    for move in range(200):
        if pose() == target: return at
        # A fight on the way back leaves the party as hurt as any other.
        at = await camp_if_hurt(items, at)
        if at < 0: return -1
        var cells: Array = town().call("tour_visuals").get("cells", [])
        if cells.size() != 256:
            say("  no map to walk by")
            return -1
        var from := pose()
        var previous := {from: from}
        var queue := [from]
        while not queue.is_empty():
            var cell: Vector2i = queue.pop_front()
            var flags: int = cells[cell.y * 16 + cell.x]
            for d in range(4):
                var next := cell + Vector2i(STEP_X[d], STEP_Y[d])
                if next.x < 0 or next.y < 0 or next.x > 15 or next.y > 15 or previous.has(next): continue
                if flags >= 0 and flags & (1 << (1 + d)) and not flags & (1 << (5 + d)): continue
                previous[next] = cell
                queue.append(next)
        if not previous.has(target):
            say("  no way known from %s to %s" % [from, target])
            return -1
        var step := target
        while previous[step] != from: step = previous[step]
        var offset := step - from
        for d in range(4):
            if offset == Vector2i(STEP_X[d], STEP_Y[d]):
                await face(d)
        if idle():
            await key(KEY_UP)
            await settle(6)
        at = await play_on(items, at)
        if at < 0: return -1
        if need_knock:
            need_knock = false
            knock_camps += 1
            if knock_camps > 2:
                say("  no Knock came back after camping twice")
                return -1
            var nights := 0
            for night in range(30):
                nights += 1
                at = await rest_once(items, at)
                if at < 0: return -1
                if dialogue().contains("Long rest complete"): break
            say("  camped %d times for Knock: %s" % [nights, dialogue().replace("\n", " ").left(90)])
    say("  200 moves did not reach %s from %s" % [target, pose()])
    return -1

const COMMAND_KEYS := {0: KEY_LEFT, 1: KEY_RIGHT, 2: KEY_DOWN, 3: KEY_UP, 4: KEY_L,
    6: KEY_UP, 7: KEY_PAGEUP, 8: KEY_RIGHT, 9: KEY_PAGEDOWN, 10: KEY_DOWN, 11: KEY_END, 12: KEY_LEFT, 13: KEY_HOME}

# The next recorded move the party can make from where it stands, when play has
# run ahead of the recording or left it: the same place, or -1. The skip is
# reported, as those moves were not played.
func catch_up(items: Array, at: int) -> int:
    var location: String = (town().get_node("Location") as Label).text.get_slice(" / ", 0)
    var here := pose()
    for i in range(at, items.size()):
        var item: Dictionary = items[i]
        if item.t != "explore": continue
        # Out on the wilderness map already, ahead of the recording's steps to it.
        if item.overland and location == "Wilderness":
            skipped += i - at
            say("  ahead of the recording: on the wilderness map %d items early" % (i - at))
            return i
        if item.overland or area_names.get(int(item.area), "") != location: continue
        if Vector2i(int(item.x), int(item.y)) == here:
            skipped += i - at
            say("  ahead of the recording: went on %d items later, %s" % [i - at, location])
            return i
    return -1

func explore(items: Array, at: int) -> int:
    var item: Dictionary = items[at]
    var area_id := int(item.area)
    var location: String = (town().get_node("Location") as Label).text.get_slice(" / ", 0)
    # The wilderness map is not learned like an area: a first overland step met
    # before the party left the last area would name the map after that area.
    if not item.overland and not area_names.has(area_id):
        area_names[area_id] = location
    var expected: String = "Wilderness" if item.overland else area_names[area_id]
    if expected != location:
        var later := catch_up(items, at)
        if later < 0:
            # Nothing later happens here: this segment's route is done with this place.
            problem("left the route in %s with %d items unplayed" % [location, items.size() - at])
            skipped += items.size() - at
            return items.size()
        return later
    at += 1
    if not item.overland:
        var target := Vector2i(int(item.x), int(item.y))
        if pose() != target:
            say("  off the route at %s, walking back to %s" % [pose(), target])
            at = await disorient_until(items, at, target)
            if at < 0: return -1
            var back := await walk_back(items, at, target)
            if back < 0:
                if not has_town() or has_combat(): return -1
                var later := catch_up(items, at)
                if later < 0:
                    problem("cannot walk back to %s" % target)
                    await capture("lost")
                    return -1
                return later
            at = back
        if facing() != int(item.facing):
            await face(int(item.facing))
    await key(COMMAND_KEYS[int(item.command)])
    await settle(4)
    return await play_on(items, at)

func camp(items: Array, at: int) -> int:
    var item: Dictionary = items[at]
    at += 1
    await key(KEY_C)
    await settle(4)
    var rest := window_shown("RestDialog")
    if not rest:
        problem("Camp did not open its dialog")
        return at
    var kind: OptionButton = rest.find_child("Kind", true, false)
    var wanted := 1 if item.kind == "long" else 0
    if not kind.disabled and kind.selected != wanted:
        kind.select(wanted); kind.item_selected.emit(wanted)
        await settle(4)
    return await play_on(items, at)

# The party's rows show each member's Hit Points.
func badly_hurt() -> bool:
    for row in (town().get_node("PartyList/Rows") as Node).get_children():
        if not (row is Button) or not row.is_visible_in_tree(): continue
        var text: String = row.text if not row.text.is_empty() else str(row.get("tooltip_text"))
        var at_hp := text.find("HP ")
        if at_hp < 0: continue
        var numbers := text.substr(at_hp + 3).split("/")
        if numbers.size() >= 2 and int(numbers[0]) * 2 < int(numbers[1]): return true
    return false

# A player does not walk on with the party at half strength.
func camp_if_hurt(items: Array, at: int) -> int:
    if not fought: return at
    fought = false
    # A Long Rest comes once a day, so recovering can take a day of hour-long camps.
    for tries in range(26):
        if at < 0 or not idle() or not badly_hurt(): return at
        rests += 1
        at = await rest_once(items, at)
    return at

func play_items(items: Array, stage: String) -> bool:
    var at := 0
    while at < items.size():
        at = await camp_if_hurt(items, at)
        if at < 0: return false
        var item: Dictionary = items[at]
        match item.t:
            "explore": at = await explore(items, at)
            "camp": at = await camp(items, at)
            "search":
                if (town().get_node("Search") as Button).button_pressed != item.on:
                    await key(KEY_S)
                at += 1
            _:
                # An answer whose question never came up: a random event the
                # recording met and this run did not.
                if item.t in ["choose", "input"] and not seen.has("skip" + str(item.dialogue)):
                    seen["skip" + str(item.dialogue)] = true
                    say("  not met here: " + str(item.dialogue).replace("\n", " ").left(100))
                at += 1
        if at < 0: return false
        if not has_town(): return false
    return true

# A run from the editor's executable keeps its settings in the project folder,
# which other runs share: the first change remembers the setting to put back.
func set_difficulty(value: String) -> void:
    var path := "res://settings.cfg" if OS.has_feature("editor") else "user://settings.cfg"
    var settings := ConfigFile.new()
    settings.load(path)
    if not changed_difficulty:
        changed_difficulty = true
        difficulty_before = settings.get_value("game", "difficulty", "")
    settings.set_value("game", "difficulty", value)
    settings.save(path)

func restore_difficulty() -> void:
    if not changed_difficulty: return
    var path := "res://settings.cfg" if OS.has_feature("editor") else "user://settings.cfg"
    var settings := ConfigFile.new()
    settings.load(path)
    if difficulty_before.is_empty():
        settings.erase_section_key("game", "difficulty")
    else:
        settings.set_value("game", "difficulty", difficulty_before)
    settings.save(path)

func read_route() -> Array:
    var segments := []
    var stage := ""
    var current: Dictionary = {}
    for line in FileAccess.get_file_as_string(route.path_join("route.jsonl")).split("\n", false):
        var item: Dictionary = JSON.parse_string(line)
        if item.t == "stage":
            stage = item.name
        elif item.t == "segment":
            current = {"save": item.save, "stage": stage, "items": []}
            segments.append(current)
        else:
            current["items"].append(item)
    return segments

func run() -> void:
    DirAccess.make_dir_recursive_absolute(out)
    root.gui_embed_subwindows = true
    root.size = Vector2i(1600, 1000)
    TranslationServer.set_locale("en")
    Engine.time_scale = time_scale
    if not difficulty.is_empty(): set_difficulty(difficulty)
    var playing := first_stage.is_empty() and first_segment.is_empty()
    var shown_stage := ""
    var done := 0
    var failed := PackedStringArray()
    for segment in read_route():
        if segment.stage == first_stage or segment.save == first_segment + ".ogs": playing = true
        if not playing: continue
        if not only.is_empty() and not only.has(segment.save.get_basename().trim_prefix("segment-")): continue
        if not last_stage.is_empty() and shown_stage == last_stage and segment.stage != last_stage: break
        if segment.stage != shown_stage:
            shown_stage = segment.stage
            say("== " + shown_stage)
        var won := false
        var failures := 0
        for attempt in range(FINALE_RELOADS if segment.stage == "castle" else RELOADS):
            say(" %s, attempt %d" % [segment.save, attempt + 1])
            retry = attempt
            # Twelve different plays have lost the finale: the difficulty a
            # player would turn down in Options, as the report says.
            if attempt == EASY_AFTER and segment.stage == "castle":
                say("  turning the difficulty down to Easy in the settings")
                set_difficulty("easy")
            if not await load_segment(segment.save): break
            if attempt == 0: await capture(segment.stage + "-start")
            var lost_before := losses
            if await play_items(segment["items"], segment.stage):
                won = true
                break
            if losses == lost_before:
                failures += 1
                if failures >= ATTEMPTS: break
        if won:
            done += 1
        else:
            failed.append(segment.save)
            problem("segment " + segment.save + " (" + segment.stage + ") not finished")
    say("== Summary")
    say("segments finished: %d, not finished: %s" % [done, ", ".join(failed)])
    say("fights: %d, lost: %d, standoffs broken by hand: %d, extra camps: %d, recorded items skipped: %d, problems: %d" % [fights, losses, hunts, rests, skipped, problems])
    say("spell lines:")
    for line in spell_lines: say("  " + line)
    restore_difficulty()
    quit(0 if failed.is_empty() and problems == 0 else 1)
