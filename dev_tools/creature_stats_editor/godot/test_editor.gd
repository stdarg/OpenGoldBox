extends SceneTree
# Drives the editor's screen headless: the lists come from the game's code, an
# edit previews as a diff, and publishing writes it, into a scratch copy of the
# repository's files so the real ones stay as they are. Run by
# dev_tools/creature_stats_editor/check.sh.

var failures := 0


func check(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		printerr("FAILED: " + message)


# The first label under `node` whose text starts with `text`.
func find_text(node: Node, text: String) -> Label:
	for child in node.get_children():
		if child is Label and child.text.begins_with(text):
			return child
		var found := find_text(child, text)
		if found != null:
			return found
	return null


func copy_file(from_root: String, to_root: String, relative: String) -> void:
	DirAccess.make_dir_recursive_absolute(to_root.path_join(relative).get_base_dir())
	DirAccess.copy_absolute(from_root.path_join(relative), to_root.path_join(relative))


func copy_tree(from_root: String, to_root: String, relative: String) -> void:
	var from := from_root.path_join(relative)
	DirAccess.make_dir_recursive_absolute(to_root.path_join(relative))
	for file in DirAccess.get_files_at(from):
		copy_file(from_root, to_root, relative.path_join(file))
	for folder in DirAccess.get_directories_at(from):
		copy_tree(from_root, to_root, relative.path_join(folder))


# The files an import reads and writes.
func scratch_repository(repository: String) -> String:
	var scratch := OS.get_user_data_dir().path_join("scratch-repository")
	DirAccess.remove_absolute(scratch.path_join("edits.json")) # a previous run's
	for folder in ["data/rules", "data/encounters", "data/companions", "data/monsters"]:
		copy_tree(repository, scratch, folder)
	for file in ["src/OpenGold.Rules.Srd5/src/srd5.cpp", "docs/SAVES.md", "docs/DEVIATIONS.md"]:
		copy_file(repository, scratch, file)
	return scratch


func _initialize() -> void:
	OS.set_environment("OPENGOLD_EDITOR_BUILD", "") # never copy into the real game project
	var repository := OS.get_environment("OPENGOLD_REPOSITORY")
	var scratch := scratch_repository(repository)
	var editor = load("res://editor.tscn").instantiate()
	editor.repository = scratch
	editor.edits_path = scratch.path_join("edits.json")
	root.add_child(editor)
	await process_frame

	check(editor.creatures.size() > 150, "Every converted record is listed")
	check(editor.encounters.size() > 200, "Every LOAD MONSTER is listed")
	check(editor.chances.size() > 50, "The scripts' chances are listed")
	check(editor.companions.size() == 6, "The six companions are listed")

	editor.creature_search.text = "castle-knight"
	editor._show_creature_list()
	# Valjevo Castle's guards and a New Phlan record share the stat line.
	check(editor.creature_list.item_count == 2, "Searching by stat line finds every record built on it")
	var guards_row := 0
	for row in editor.creature_list.item_count:
		if editor.creature_list.get_item_text(row).contains("Valjevo Castle"):
			guards_row = row
	editor.creature_list.select(guards_row)
	editor._select_creature(guards_row)
	check(editor.creature_view.get_child_count() > 5, "The guards' details are shown")

	var guards := {}
	for site in editor.encounters:
		if site["site"] == "ECL5.DAX:7:0xA60D":
			guards = site
	editor._show_fight(guards)
	var sizing := find_text(editor.fight_view, "Normal")
	check(sizing != null and sizing.get_parent().get_child(sizing.get_index() + 2).text == "8",
		"Eight of the twelve guards meet the default party at Normal")
	editor._show_chance(editor.chances[0])
	check(editor.chance_view.get_child_count() > 3, "A chance is shown with its edit")
	editor._show_companion(editor.companions[0])
	check(find_text(editor.companion_view, "Built as the game builds") != null,
		"A companion is shown as the game builds it")

	editor.creature = editor.creature_list.get_item_metadata(guards_row)
	editor._conversion_edit({"fit_xp": 900}, "Rated higher")
	editor._conversion_edit({"award_xp": 800}, "Rated higher")
	check(editor.edits.size() == 1 and int(editor.edits[0]["to"]["fit_xp"]) == 900 and
		int(editor.edits[0]["to"]["award_xp"]) == 800 and int(editor.edits[0]["from"]["fit_xp"]) == 700,
		"A second change to a record's conversion builds on the pending one")
	editor.edits.clear()

	editor._add_edit({"change": "hit_points", "definition": "castle-knight", "from": 52, "to": 60,
		"reason": "Sturdier guards"})
	check(editor.edits.size() == 1 and FileAccess.file_exists(editor.edits_path),
		"A pending edit is kept in the edits file")
	editor._add_edit({"change": "hit_points", "definition": "castle-knight", "from": 52, "to": 58,
		"reason": "Sturdier guards"})
	check(editor.edits.size() == 1 and int(editor.edits[0]["to"]) == 58,
		"A second edit of the same value replaces the first")
	editor._preview()
	check(editor.report_view.text.contains("+ creature castle-knight 18 58 "),
		"Preview shows the stat line's new Hit Points in the diff")
	check(editor.report_view.text.contains("The rules version becomes"),
		"Preview says the rules version goes up")

	editor._publish()
	var rules := FileAccess.get_file_as_string(scratch.path_join("data/rules/srd-5.2.1/combat.rules"))
	check(rules.contains("creature castle-knight 18 58 "), "Publish writes the edit")
	check(editor.edits.is_empty(), "Publishing clears the pending edits")
	check(int(editor._stat_line("castle-knight")["hit_points"]) == 58,
		"After publishing the editor shows the game's new values")

	editor._add_edit({"change": "hit_points", "definition": "castle-knight", "from": 52, "to": 70,
		"reason": "Stale"})
	editor._preview()
	check(editor.report_view.text.begins_with("Refused"), "A stale edit is refused")

	editor.queue_free()
	await process_frame
	print("Creature Stats Editor checks: %s" % ("passed" if failures == 0 else "%d failed" % failures))
	quit(1 if failures else 0)
