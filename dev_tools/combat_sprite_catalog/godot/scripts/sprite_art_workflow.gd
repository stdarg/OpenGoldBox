extends SceneTree

# Standalone art preparation/review; never loads original assets or writes live art.
const Metadata = preload("res://scripts/monster_metadata.gd")
const GENERATION := "res://../generation/"
const GUIDE := "res://../art/pose-guide/bipedal-v1.png"
const GUIDE_APPROVAL := "res://../art/pose-guide/approval.json"
const REVIEW_FORMAT := "opengold-sprite-review-v1"
const VISUAL_CHECKS := [
    "front_three_quarter_screen_right_chest_visible",
    "ready_weapon_raised_or_projectile_loaded",
    "action_late_strike_or_after_release",
    "correct_equipment_grip_and_handedness",
    "same_identity_clothing_palette_and_camera",
    "consistent_scale_and_grounded_foot_baseline_across_sets",
    "humanoid_height_tier_or_na_reviewed",
    "planted_feet_same_xy_no_whole_body_translation",
    "complete_silhouette_no_clipping_or_background",
    "pixel_art_readable_at_combat_size",
    "pair_reviewed_alternating_in_catalog",
    "only_independent_art_used_as_references",
]


static func read_json(path: String) -> Dictionary:
    if not FileAccess.file_exists(path): return {}
    var value: Variant = JSON.parse_string(FileAccess.get_file_as_string(path))
    return value if value is Dictionary else {}


static func workflow_hash() -> String:
    var text := ""
    for name in ["rules.txt", "ready.txt", "action.txt"]:
        text += FileAccess.get_file_as_string(GENERATION + name)
    return text.sha256_text()


static func write_text(path: String, text: String) -> bool:
    var file := FileAccess.open(path, FileAccess.WRITE)
    if file == null: return false
    file.store_string(text)
    return file.get_error() == OK


static func metadata_errors(record: Dictionary) -> Array[String]:
    if record.get("format") != "opengold-monster-art-v1" or \
            not record.get("equipment") is Array or not record.get("source") is Dictionary or \
            not record.source.get("key") is String or str(record.source.key).is_empty():
        return ["Invalid monster-art metadata."]
    if not record.has("humanoid_size") or not Metadata.valid_humanoid_size(record.humanoid_size):
        return ["Choose and save a valid humanoid height in the Combat Sprite Catalog."]
    var ids := {}
    for item in record.equipment:
        if not item is Dictionary or not item.has_all(["id", "name", "kind"]) or \
                not item.id is String or not Metadata._safe_item_id(item.id) or \
                ids.has(item.id) or str(item.name).strip_edges().is_empty():
            return ["Invalid or duplicate equipment entry."]
        ids[item.id] = true
        if item.kind not in ["weapon", "shield", "gear"]: return ["Invalid equipment kind."]
        if item.kind == "weapon" and item.get("weapon_role") not in ["one_hand", "two_hand", "ranged"]:
            return ["Weapon role is missing or invalid for " + str(item.name)]
    return []


static func prepare(metadata_path: String, variant_id: String, directory: String) -> Array[String]:
    if not metadata_path.is_absolute_path() or not directory.is_absolute_path():
        return ["Use absolute paths for the metadata JSON and staging directory."]
    var record := read_json(metadata_path)
    var errors := metadata_errors(record)
    if not errors.is_empty(): return errors
    # Use the catalog's existing setup rules, including mandatory weapon+shield pairs.
    Metadata.rebuild_variants(record)
    var variant: Dictionary = {}
    for candidate in record.variants:
        if candidate.id == variant_id: variant = candidate
    if variant.is_empty():
        var choices: Array[String] = []
        for candidate in record.variants: choices.append(candidate.id)
        return ["Unknown/currently unavailable weapon set '%s'. Available: %s" % [variant_id, ", ".join(choices)]]
    if DirAccess.dir_exists_absolute(directory) or FileAccess.file_exists(directory):
        return ["Staging path already exists; use a new directory to preserve previous work."]
    for name in ["rules.txt", "ready.txt", "action.txt"]:
        if not FileAccess.file_exists(GENERATION + name): return ["Missing generation specification: " + name]
    if not FileAccess.file_exists(GUIDE): return ["Missing pose guide."]
    var selected: Array = []
    var other_gear: Array = []
    for item in record.equipment:
        if item.id in [variant.weapon_id, variant.shield_id]: selected.append(item)
        elif item.kind not in ["weapon", "shield"]: other_gear.append(item)
    var context := {
        "name": record.get("name", ""), "description": record.get("description", ""),
        "source_key": record.source.key, "selected_set": variant.label,
        "humanoid_size": record.humanoid_size, "sprite_size": record.get("sprite_size", null),
        "weapon_role": variant.weapon_role, "selected_equipment": selected,
        "other_gear": other_gear,
    }
    var prefix := FileAccess.get_file_as_string(GENERATION + "rules.txt") + \
        "\nMONSTER AND SELECTED SET (current catalog JSON):\n" + JSON.stringify(context, "  ") + "\n\n"
    var visual := {}
    for key in VISUAL_CHECKS: visual[key] = false
    var review := {
        "format": REVIEW_FORMAT, "specification": "bipedal-v1",
        "source_key": record.source.key, "variant_id": variant.id,
        "metadata_sha256": FileAccess.get_sha256(metadata_path),
        "workflow_sha256": workflow_hash(), "pose_guide_sha256": FileAccess.get_sha256(GUIDE),
        "reviewer": "", "reviewed_at": "", "ready_sha256": "", "action_sha256": "",
        "visual_checks": visual, "notes": "",
        "install_paths": {"ready": variant.ready, "action": variant.action},
    }
    if DirAccess.make_dir_recursive_absolute(directory) != OK: return ["Cannot create staging directory."]
    for pose in ["ready", "action"]:
        var prompt := prefix + FileAccess.get_file_as_string(GENERATION + pose + ".txt")
        if not write_text(directory.path_join(pose + "-prompt.txt"), prompt):
            return ["Cannot write prompt; incomplete staging directory: " + directory]
    if not write_text(directory.path_join("review.json"), JSON.stringify(review, "  ") + "\n"):
        return ["Cannot write review; incomplete staging directory: " + directory]
    return []


static func check_pair(directory: String) -> Dictionary:
    var errors: Array[String] = []
    var hashes := {}
    var pixels: Array[PackedByteArray] = []
    for pose in ["ready", "action"]:
        var path := directory.path_join(pose + ".png")
        if not FileAccess.file_exists(path):
            errors.append("Missing " + path)
            continue
        var bytes := FileAccess.get_file_as_bytes(path)
        if bytes.size() < 8 or bytes.slice(0, 8) != PackedByteArray([137, 80, 78, 71, 13, 10, 26, 10]):
            errors.append(pose + ": file is not a PNG.")
            continue
        var image := Image.new()
        if image.load_png_from_buffer(bytes) != OK:
            errors.append(pose + ": PNG cannot be decoded.")
            continue
        hashes[pose] = FileAccess.get_sha256(path)
        if image.get_size() != Vector2i(128, 128):
            errors.append(pose + ": must be 128 x 128 pixels.")
            continue
        if image.detect_alpha() == Image.ALPHA_NONE:
            errors.append(pose + ": must contain genuine transparent pixels.")
        image.convert(Image.FORMAT_RGBA8)
        var bounds := image.get_used_rect()
        if not bounds.has_area(): errors.append(pose + ": image is empty/fully transparent.")
        elif bounds.position.x <= 0 or bounds.position.y <= 0 or bounds.end.x >= 128 or bounds.end.y >= 128:
            errors.append(pose + ": nontransparent pixels touch the border; check for clipping.")
        pixels.append(image.get_data())
    if pixels.size() == 2 and pixels[0] == pixels[1]:
        errors.append("Ready and action contain identical pixels; review the missing pose change.")
    return {"errors": errors, "sha256": hashes}


static func review_errors(review: Dictionary, hashes: Dictionary, metadata_path: String, approval: Dictionary) -> Array[String]:
    var errors: Array[String] = []
    if review.get("format") != REVIEW_FORMAT or review.get("specification") != "bipedal-v1":
        return ["Missing/unsupported review.json; prepare a review packet first."]
    if not FileAccess.file_exists(GUIDE) or approval.get("format") != "opengold-pose-guide-approval-v1" or \
            approval.get("specification") != "bipedal-v1" or approval.get("status") != "approved" or \
            str(approval.get("approved_by", "")).strip_edges().is_empty() or \
            str(approval.get("approved_at", "")).strip_edges().is_empty() or \
            approval.get("image_sha256") != FileAccess.get_sha256(GUIDE):
        errors.append("Pose guide needs explicit user approval for this exact image.")
    if review.get("workflow_sha256") != workflow_hash() or \
            review.get("pose_guide_sha256") != FileAccess.get_sha256(GUIDE):
        errors.append("Specification or pose guide changed; prepare and review again.")
    if not FileAccess.file_exists(metadata_path) or \
            review.get("metadata_sha256") != FileAccess.get_sha256(metadata_path):
        errors.append("Monster metadata changed or is missing; prepare from the current JSON and review again.")
    var current := read_json(metadata_path)
    if not current.get("source") is Dictionary or review.get("source_key") != current.source.get("key"):
        errors.append("Review does not identify this monster.")
    var invalid_metadata := metadata_errors(current)
    errors.append_array(invalid_metadata)
    if invalid_metadata.is_empty():
        Metadata.rebuild_variants(current)
        var matches := false
        for variant in current.variants:
            if variant.id == review.get("variant_id") and \
                    review.get("install_paths") == {"ready": variant.ready, "action": variant.action}:
                matches = true
        if not matches: errors.append("Review weapon set/install paths do not match current metadata.")
    if str(review.get("reviewer", "")).strip_edges().is_empty() or \
            str(review.get("reviewed_at", "")).strip_edges().is_empty():
        errors.append("Record the visual reviewer and review date/time.")
    for pose in ["ready", "action"]:
        if not hashes.has(pose) or review.get(pose + "_sha256", "") != hashes.get(pose, ""):
            errors.append(pose + ": record a visual review of the exact current PNG hash.")
    var checks: Variant = review.get("visual_checks", {})
    for key in VISUAL_CHECKS:
        if not checks is Dictionary or not checks.get(key) is bool or checks.get(key) != true:
            errors.append("Visual review incomplete: " + key)
    return errors


func _initialize() -> void:
    var args := OS.get_cmdline_user_args()
    var errors: Array[String] = []
    if args.size() == 4 and args[0] == "prepare":
        errors = prepare(args[1], args[2], args[3])
        if errors.is_empty():
            print("Prepared prompts and review.json in " + args[3])
            print("Pose guide approval: " + str(read_json(GUIDE_APPROVAL).get("status", "missing")))
    elif args.size() in [2, 3] and args[0] in ["check", "accept"]:
        if args[0] == "accept" and args.size() != 3:
            errors.append("accept requires the current monster JSON path.")
        else:
            var report := check_pair(args[1])
            errors.assign(report.errors)
            print(JSON.stringify(report, "  "))
            if args[0] == "accept":
                errors.append_array(review_errors(read_json(args[1].path_join("review.json")), \
                    report.sha256, args[2], read_json(GUIDE_APPROVAL)))
            if errors.is_empty():
                print("Pair accepted for installation; no files were installed." if args[0] == "accept" else \
                    "Technical checks passed. Visual review is still required.")
    else:
        errors.append("Usage after --: prepare ABS_METADATA_JSON VARIANT_ID ABS_NEW_DIRECTORY | check DIRECTORY | accept DIRECTORY CURRENT_METADATA_JSON")
    for error in errors: printerr(error)
    quit(0 if errors.is_empty() else 1)
