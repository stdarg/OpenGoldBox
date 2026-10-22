extends SceneTree

const Workflow = preload("res://scripts/sprite_art_workflow.gd")
var failures: Array[String] = []
var fixture_dir := ""
var checks := 0


func expect(ok: bool, message: String) -> void:
    checks += 1
    if not ok: failures.append(message)


func save_json(path: String, value: Dictionary) -> void:
    expect(Workflow.write_text(path, JSON.stringify(value, "  ")), "Write fixture JSON")


func fixture_image(action := false) -> Image:
    var image := Image.create(128, 128, false, Image.FORMAT_RGBA8)
    image.fill(Color.TRANSPARENT)
    image.fill_rect(Rect2i(40, 25, 30, 90), Color.BLUE)
    image.fill_rect(Rect2i(70, 50 if action else 30, 20, 10), Color.WHITE)
    return image


func save_pair(directory: String) -> void:
    expect(fixture_image().save_png(directory.path_join("ready.png")) == OK, "Save ready fixture")
    expect(fixture_image(true).save_png(directory.path_join("action.png")) == OK, "Save action fixture")


func remove_tree(path: String) -> void:
    var directory := DirAccess.open(path)
    if directory == null: return
    for name in directory.get_files(): DirAccess.remove_absolute(path.path_join(name))
    for name in directory.get_directories(): remove_tree(path.path_join(name))
    DirAccess.remove_absolute(path)


func _initialize() -> void:
    fixture_dir = ProjectSettings.globalize_path("user://sprite-workflow-tests-%d" % OS.get_process_id())
    expect(DirAccess.make_dir_recursive_absolute(fixture_dir) == OK, "Create isolated fixture directory")
    var metadata_path := fixture_dir.path_join("monster.json")
    var record := {
        "format": "opengold-monster-art-v1", "name": "Test biped", "description": "An independently designed warrior.",
        "humanoid_size": "normal", "sprite_size": {"width_squares": 1, "height_squares": 1},
        "flag_for_regeneration": true, "source": {"key": "TEST:1"},
        "equipment": [
            {"id": "sword", "name": "Sword", "kind": "weapon", "weapon_role": "one_hand"},
            {"id": "shield", "name": "Shield", "kind": "shield"},
            {"id": "greatsword", "name": "Greatsword", "kind": "weapon", "weapon_role": "two_hand"},
            {"id": "bow", "name": "Bow", "kind": "weapon", "weapon_role": "ranged"},
            {"id": "armor", "name": "Mail", "kind": "gear"},
        ], "variants": [],
    }
    save_json(metadata_path, record)
    var original_hash := FileAccess.get_sha256(metadata_path)
    var stage := fixture_dir.path_join("sword-pair")
    expect(Workflow.prepare(metadata_path, "sword__shield", stage).is_empty(), "Prepare sword/shield pair")
    expect(not Workflow.prepare(metadata_path, "sword__one_hand", fixture_dir.path_join("wrong")).is_empty(),
        "Do not offer sword alone when shield is present")
    expect(not Workflow.prepare(metadata_path, "sword__shield", stage).is_empty(), "Never overwrite an existing packet")
    var ready_text := FileAccess.get_file_as_string(stage.path_join("ready-prompt.txt"))
    var action_text := FileAccess.get_file_as_string(stage.path_join("action-prompt.txt"))
    var rules := FileAccess.get_file_as_string(Workflow.GENERATION + "rules.txt")
    expect(ready_text.begins_with(rules) and action_text.begins_with(rules), "Both prompts include complete shared rules")
    expect('"selected_set": "Sword + Shield"' in ready_text and '"name": "Mail"' in ready_text,
        "Prompt uses selected setup and other gear")
    expect('"humanoid_size": "normal"' in ready_text and '"humanoid_size": "normal"' in action_text,
        "Both prompts retain the saved humanoid height")
    expect('"name": "Bow"' not in ready_text and '"name": "Greatsword"' not in ready_text,
        "Prompt does not equip other weapon sets")
    var blank_review := Workflow.read_json(stage.path_join("review.json"))
    expect(blank_review.reviewer.is_empty() and blank_review.ready_sha256.is_empty() and
        blank_review.visual_checks.values().all(func(value: Variant) -> bool: return value is bool and not value),
        "Preparation leaves reviewer, image approval and all visual checks incomplete")
    for set_id in ["greatsword__two_hand", "bow__ranged"]:
        var alternate := fixture_dir.path_join(set_id)
        expect(Workflow.prepare(metadata_path, set_id, alternate).is_empty(), "Prepare standalone weapon set")
        var prompt := FileAccess.get_file_as_string(alternate.path_join("ready-prompt.txt"))
        expect('"name": "Shield"' not in prompt, "No shield added to two-handed/ranged set")
    expect(FileAccess.get_sha256(metadata_path) == original_hash, "Preparation preserves live monster JSON/regen flag")

    expect(not Workflow.check_pair(stage).errors.is_empty(), "Missing pair rejected")
    save_pair(stage)
    var report := Workflow.check_pair(stage)
    expect(report.errors.is_empty(), "Distinct 128x128 transparent pair passes technical check")
    var review := Workflow.read_json(stage.path_join("review.json"))
    var approval := {"format": "opengold-pose-guide-approval-v1", "specification": "bipedal-v1",
        "status": "approved", "approved_by": "TEST FIXTURE ONLY", "approved_at": "2026-10-17T00:00:00Z",
        "image_sha256": FileAccess.get_sha256(Workflow.GUIDE)}
    expect(not Workflow.review_errors(review, report.sha256, metadata_path, approval).is_empty(),
        "Technical pass does not satisfy visual review")
    review.reviewer = "TEST FIXTURE ONLY"
    review.reviewed_at = "2026-10-17T00:00:00Z"
    review.ready_sha256 = report.sha256.ready
    review.action_sha256 = report.sha256.action
    for key in Workflow.VISUAL_CHECKS: review.visual_checks[key] = true
    expect(Workflow.review_errors(review, report.sha256, metadata_path, approval).is_empty(), "Complete matching review accepted")
    var pending := approval.duplicate()
    pending.status = "pending_user_approval"
    expect(not Workflow.review_errors(review, report.sha256, metadata_path, pending).is_empty(), "Pending guide blocks acceptance")
    pending.status = "approved"
    pending.image_sha256 = "different guide"
    expect(not Workflow.review_errors(review, report.sha256, metadata_path, pending).is_empty(), "Changed guide invalidates approval")
    for key in Workflow.VISUAL_CHECKS:
        review.visual_checks[key] = false
        expect(not Workflow.review_errors(review, report.sha256, metadata_path, approval).is_empty(), "Every visual attestation is required: " + key)
        review.visual_checks[key] = true
    var changed := review.duplicate(true)
    changed.workflow_sha256 = "old rules"
    expect(not Workflow.review_errors(changed, report.sha256, metadata_path, approval).is_empty(), "Changed rules invalidate review")
    changed = review.duplicate(true)
    changed.install_paths.ready = "art/wrong-monster/ready.png"
    expect(not Workflow.review_errors(changed, report.sha256, metadata_path, approval).is_empty(), "Wrong installation target rejected")
    var image := fixture_image(true)
    image.set_pixel(50, 50, Color.RED)
    image.save_png(stage.path_join("action.png"))
    expect(not Workflow.review_errors(review, Workflow.check_pair(stage).sha256, metadata_path, approval).is_empty(),
        "Changing pixels invalidates visual review")
    record.description = "A revised description."
    save_json(metadata_path, record)
    expect(not Workflow.review_errors(review, report.sha256, metadata_path, approval).is_empty(), "Changed JSON invalidates review")

    var ready_path := stage.path_join("ready.png")
    var bad_images: Array[Image] = []
    image = Image.create(64, 64, false, Image.FORMAT_RGBA8)
    image.fill(Color.BLUE)
    bad_images.append(image)
    image = fixture_image()
    image.fill(Color.BLUE)
    bad_images.append(image)
    image = fixture_image()
    image.fill(Color.TRANSPARENT)
    bad_images.append(image)
    image = fixture_image()
    image.set_pixel(0, 60, Color.BLUE)
    bad_images.append(image)
    for bad_image in bad_images:
        save_pair(stage)
        bad_image.save_png(ready_path)
        expect(not Workflow.check_pair(stage).errors.is_empty(), "Bad size/opaque/empty/clipped PNG rejected")
    save_pair(stage)
    fixture_image().save_png(stage.path_join("action.png"))
    expect(not Workflow.check_pair(stage).errors.is_empty(), "Identical frames rejected")
    Workflow.write_text(ready_path, "Not a PNG")
    expect(not Workflow.check_pair(stage).errors.is_empty(), "Disguised non-PNG rejected")

    record.equipment = []
    save_json(metadata_path, record)
    expect(Workflow.prepare(metadata_path, "default", fixture_dir.path_join("unarmed")).is_empty(), "No-gear default pair supported")
    record.humanoid_size = "small"
    record.height_feet = 3.5
    record.height_reference = {"min_feet": 3, "max_feet": 4, "url": "test source", "note": "Goblin range"}
    save_json(metadata_path, record)
    var small_stage := fixture_dir.path_join("small-height")
    expect(Workflow.prepare(metadata_path, "default", small_stage).is_empty(), "Small art height is valid")
    var small_prompt := FileAccess.get_file_as_string(small_stage.path_join("ready-prompt.txt"))
    expect('"height_feet": 3.5' in small_prompt and '"min_feet": 3' in small_prompt,
        "Prompt includes the Small art target and source range")
    record.erase("height_feet")
    save_json(metadata_path, record)
    expect(not Workflow.prepare(metadata_path, "default", fixture_dir.path_join("missing-small-height")).is_empty(),
        "Small art preparation requires saved foot height")
    record.erase("humanoid_size")
    save_json(metadata_path, record)
    expect(not Workflow.prepare(metadata_path, "default", fixture_dir.path_join("missing-height")).is_empty(),
        "Art preparation requires a saved humanoid height")
    record.humanoid_size = "normal"
    record.equipment = [42]
    save_json(metadata_path, record)
    expect(not Workflow.prepare(metadata_path, "default", fixture_dir.path_join("invalid")).is_empty(), "Malformed equipment fails cleanly")
    expect(not Workflow.review_errors(review, report.sha256, metadata_path, approval).is_empty(), "Malformed metadata rejects acceptance cleanly")
    remove_tree(fixture_dir)
    expect(not DirAccess.dir_exists_absolute(fixture_dir), "Fixture cleanup complete")
    for failure in failures: printerr(failure)
    if failures.is_empty(): print("Combat sprite workflow checks passed: %d assertions" % checks)
    quit(0 if failures.is_empty() else 1)
