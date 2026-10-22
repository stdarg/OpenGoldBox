extends RefCounted

# Paths are relative to the standalone tool, not to the Godot project inside it.
static func tool_root() -> String:
    return ProjectSettings.globalize_path("res://..").simplify_path()


static func record_id(key: String) -> String:
    return key.to_lower().replace(".", "_").replace(":", "_")


static func _safe_item_id(value: String) -> bool:
    if value.is_empty(): return false
    for index in range(value.length()):
        var code := value.unicode_at(index)
        if not (code >= 97 and code <= 122) and \
                not (code >= 65 and code <= 90) and \
                not (code >= 48 and code <= 57) and code != 95:
            return false
    return true


static func json_path(key: String, root: String = "") -> String:
    return (root if not root.is_empty() else tool_root()).path_join("json").path_join(record_id(key) + ".json")


static func art_path(key: String, variant_id: String, action: bool, root: String = "") -> String:
    var folder := (root if not root.is_empty() else tool_root()).path_join("art")
    return folder.path_join(record_id(key)).path_join(variant_id).path_join("action.png" if action else "ready.png")


static func source_kind(original_type: int) -> String:
    if original_type == 59:
        return "shield"
    if (original_type >= 1 and original_type <= 47 and original_type != 28) or original_type == 79:
        return "weapon"
    return "gear"


static func source_weapon_role(original_type: int, hands: int) -> String:
    # Bows, crossbows, sling, darts and wands have a ranged art setup. Other
    # throwable melee weapons can stay with a shield; the editor can override it.
    if original_type == 9 or (original_type >= 41 and original_type <= 47) or original_type == 79:
        return "ranged"
    return "two_hand" if hands >= 2 else "one_hand"


static func _gear_summary(equipment: Array) -> String:
    var counts := {}
    for item in equipment:
        var name := str(item.get("name", ""))
        counts[name] = int(counts.get(name, 0)) + 1
    var seen := {}
    var parts := PackedStringArray()
    for item in equipment:
        var name := str(item.get("name", ""))
        if seen.has(name): continue
        seen[name] = true
        parts.append("%d × %s" % [counts[name], name] if counts[name] > 1 else name)
    return ", ".join(parts) if not parts.is_empty() else "none"


static func _wealth_values(value: Dictionary) -> Dictionary:
    var result := {}
    for coin in ["copper", "silver", "electrum", "gold", "platinum", "gems", "jewelry"]:
        if value.has(coin): result[coin] = int(value[coin])
    return result


static func valid_sprite_size(value: Variant) -> bool:
    return value == null or (value is Dictionary and value.size() == 2 and \
            _valid_square_count(value.get("width_squares", null)) and \
            _valid_square_count(value.get("height_squares", null)))


static func _valid_square_count(value: Variant) -> bool:
    return typeof(value) in [TYPE_INT, TYPE_FLOAT] and float(value) in [1.0, 2.0, 3.0]


static func valid_humanoid_size(value: Variant) -> bool:
    return value is String and value in ["na", "small", "normal", "tall"]


static func valid_small_height(value: Variant) -> bool:
    return typeof(value) in [TYPE_INT, TYPE_FLOAT] and float(value) >= 2.0 and float(value) <= 4.0


static func new_record(row: Dictionary, art: Dictionary) -> Dictionary:
    var equipment: Array = []
    for index in range(row.equipment.size()):
        var item: Dictionary = row.equipment[index]
        var kind := source_kind(int(item.type))
        equipment.append({
            "id": "item_%03d" % index,
            "kind": kind,
            "weapon_role": source_weapon_role(int(item.type), int(item.get("hands", -1))) if kind == "weapon" else "",
            "name": str(item.label),
            "source_type": int(item.type),
            "source_hands": int(item.get("hands", -1)),
            "source_range": int(item.get("range", -1)),
            "readied": bool(item.readied),
        })
    var record := {
        "format": "opengold-monster-art-v1",
        "name": str(row.name),
        "source": {
            "key": str(row.key),
            "creature_archive": str(row.archive) if str(row.archive).begins_with("MON") else "",
            "creature_record": int(row.record) if str(row.archive).begins_with("MON") else -1,
            "art_archive": str(art.get("archive", "")),
            "art_record": int(art.get("record", -1)),
            "art_basis": str(art.get("basis", "Unresolved binding")),
            "linked_creature_keys": row.get("linked_keys", []),
        },
        "description": str(row.get("art_description", "")),
        "sprite_size": art.get("sprite_size", null) if valid_sprite_size(art.get("sprite_size", null)) else null,
        "humanoid_size": "na",
        "height_feet": null,
        "wealth": _wealth_values(row.get("wealth", {})),
        "flag_for_regeneration": false,
        "preview_fallback_variant": "",
        "equipment": equipment,
        "variants": [],
    }
    if row.has("identical_art_keys"):
        record.source["identical_art_keys"] = row.identical_art_keys.duplicate()
        record.source["shared_art_creature_keys"] = row.shared_art_user_keys.duplicate()
        record.source["direct_script_reference_found"] = row.direct_script_reference_found
    rebuild_variants(record)
    return record


static func rebuild_variants(record: Dictionary) -> void:
    var one_hand: Array = []
    var standalone: Array = []
    var shields: Array = []
    for entry in record.equipment:
        if not entry is Dictionary or str(entry.get("name", "")).strip_edges().is_empty():
            continue
        if entry.get("kind") == "weapon":
            if entry.get("weapon_role", "one_hand") == "one_hand":
                one_hand.append(entry)
            else:
                standalone.append(entry)
        if entry.get("kind") == "shield": shields.append(entry)
    var variants: Array = []
    var key: String = record.source.key
    for weapon in one_hand:
        # A one-handed weapon and an available shield are one complete setup.
        # Only offer the weapon alone when this creature has no shield at all.
        var partners: Array = shields if not shields.is_empty() else [{}]
        for shield in partners:
            var shield_id := str(shield.get("id", ""))
            var variant_id := "%s__%s" % [weapon.id, shield_id if not shield_id.is_empty() else "one_hand"]
            variants.append({
                "id": variant_id,
                "label": "%s + %s" % [weapon.name, shield.name] if not shield_id.is_empty() else str(weapon.name),
                "weapon_role": "one_hand",
                "weapon_id": weapon.id,
                "shield_id": shield_id,
                "ready": "art/%s/%s/ready.png" % [record_id(key), variant_id],
                "action": "art/%s/%s/action.png" % [record_id(key), variant_id],
            })
    for weapon in standalone:
        var role: String = weapon.get("weapon_role", "two_hand")
        var variant_id := "%s__%s" % [weapon.id, role]
        variants.append({
            "id": variant_id,
            "label": str(weapon.name),
            "weapon_role": role,
            "weapon_id": weapon.id,
            "shield_id": "",
            "ready": "art/%s/%s/ready.png" % [record_id(key), variant_id],
            "action": "art/%s/%s/action.png" % [record_id(key), variant_id],
        })
    if variants.is_empty():
        variants.append({
            "id": "default", "label": "Default (no selectable weapon setup)",
            "weapon_role": "default",
            "weapon_id": "", "shield_id": "",
            "ready": "art/%s/default/ready.png" % record_id(key),
            "action": "art/%s/default/action.png" % record_id(key),
        })
    record.variants = variants


static func load_record(row: Dictionary, art: Dictionary, root: String = "") -> Dictionary:
    var path := json_path(str(row.key), root)
    if not FileAccess.file_exists(path):
        return {"data": new_record(row, art), "error": ""}
    var file := FileAccess.open(path, FileAccess.READ)
    if file == null:
        return {"data": new_record(row, art), "error": "Cannot read %s" % path}
    var parsed: Variant = JSON.parse_string(file.get_as_text())
    if not parsed is Dictionary or parsed.get("format") != "opengold-monster-art-v1" or \
            not parsed.get("source") is Dictionary or parsed.source.get("key") != row.key or \
            not parsed.get("equipment") is Array or not parsed.get("variants") is Array:
        return {"data": new_record(row, art), "error": "Invalid monster metadata: %s" % path}
    var original_summary := _gear_summary(parsed.equipment)
    var ids := {}
    var legacy_names := {}
    for item in parsed.equipment:
        if not item is Dictionary or not (item.get("id") is String) or \
                not _safe_item_id(item.get("id", "")) or \
                not (item.get("kind") in ["weapon", "shield", "gear"]) or \
                not (item.get("name") is String) or ids.has(item.id):
            return {"data": new_record(row, art), "error": "Invalid equipment in %s" % path}
        ids[item.id] = true
        var original_type := int(item.get("source_type", -1))
        var hands := int(item.get("source_hands", -1))
        var item_range := int(item.get("source_range", -1))
        if hands < 0:
            for source_item in row.equipment:
                if int(source_item.type) == original_type:
                    hands = int(source_item.get("hands", -1))
                    item_range = int(source_item.get("range", -1))
                    break
        item["source_hands"] = hands
        item["source_range"] = item_range
        # Saved catalog entries predate per-item names decoded from START.EXE.
        # Match the original slot and type so edited or newly added gear keeps its name.
        var old_name := str(item.name)
        var legacy_prefix := "Item type %d" % original_type
        if old_name == legacy_prefix or old_name.begins_with(legacy_prefix + " +") or \
                old_name.begins_with(legacy_prefix + " -"):
            for index in range(row.equipment.size()):
                var source_item: Dictionary = row.equipment[index]
                if str(item.id) == "item_%03d" % index and int(source_item.type) == original_type:
                    var current_name := str(source_item.label)
                    if not current_name.begins_with("Item type "):
                        item.name = current_name
                        if legacy_names.has(old_name) and legacy_names[old_name] != current_name:
                            legacy_names[old_name] = ""
                        elif not legacy_names.has(old_name):
                            legacy_names[old_name] = current_name
                    break
        if item.kind == "weapon":
            if not (item.get("weapon_role") in ["one_hand", "two_hand", "ranged"]):
                item["weapon_role"] = source_weapon_role(original_type, hands)
        else:
            item["weapon_role"] = ""
    if str(parsed.get("name", "")) == "Unidentified combat sprite" and \
            (row.has("linked_name") or row.has("identified_name")):
        parsed["name"] = row.get("identified_name", row.get("linked_name", row.name))
    else:
        parsed["name"] = str(parsed.get("name", row.name)).strip_edges()
    if parsed.name.is_empty(): parsed.name = str(row.name)
    if row.has("linked_name") and parsed.equipment.is_empty() and not row.equipment.is_empty():
        parsed.equipment = new_record(row, art).equipment
    # Original wealth is authoritative; copied monsters retain their independent
    # snapshot because they have no MON character record to reload.
    parsed["wealth"] = _wealth_values(parsed.get("wealth", {}) if str(row.key).begins_with("CUSTOM:") \
            else row.get("wealth", {}))
    if not str(row.key).begins_with("CUSTOM:"):
        parsed["source"] = new_record(row, art).source
    parsed["description"] = str(parsed.get("description", ""))
    var sprite_size: Variant = parsed.get("sprite_size", art.get("sprite_size", null))
    if not valid_sprite_size(sprite_size):
        return {"data": new_record(row, art), "error": "Invalid sprite size in %s" % path}
    parsed["sprite_size"] = sprite_size
    var humanoid_size: Variant = parsed.get("humanoid_size", "na")
    if not valid_humanoid_size(humanoid_size):
        return {"data": new_record(row, art), "error": "Invalid humanoid size in %s" % path}
    parsed["humanoid_size"] = humanoid_size
    var height_feet: Variant = parsed.get("height_feet", null)
    if height_feet != null and not valid_small_height(height_feet):
        return {"data": new_record(row, art), "error": "Invalid small humanoid height in %s" % path}
    parsed["height_feet"] = height_feet
    if parsed.description.ends_with(original_summary + ".") and \
            (parsed.description.contains(". Gear: ") or parsed.description.contains(". Original gear: ")):
        parsed["description"] = parsed.description.trim_suffix(original_summary + ".") + \
                _gear_summary(parsed.equipment) + "."
    else:
        for old_name in legacy_names:
            if not str(legacy_names[old_name]).is_empty():
                parsed["description"] = parsed.description.replace(old_name, legacy_names[old_name])
    parsed["flag_for_regeneration"] = bool(parsed.get("flag_for_regeneration", false))
    var fallback := str(parsed.get("preview_fallback_variant", ""))
    parsed["preview_fallback_variant"] = fallback if fallback.is_empty() or _safe_item_id(fallback) else ""
    rebuild_variants(parsed)
    return {"data": parsed, "error": ""}


static func list_custom_rows(root: String = "") -> Array:
    var result: Array = []
    var folder := (root if not root.is_empty() else tool_root()).path_join("json")
    var directory := DirAccess.open(folder)
    if directory == null: return result
    for filename in directory.get_files():
        if not filename.begins_with("custom_") or not filename.ends_with(".json"): continue
        var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(folder.path_join(filename)))
        if not parsed is Dictionary or not parsed.get("source") is Dictionary: continue
        var source: Dictionary = parsed.source
        var key := str(source.get("key", ""))
        if not key.begins_with("CUSTOM:") or json_path(key, root).get_file() != filename: continue
        result.append({"key":key,"name":str(parsed.get("name", "Unnamed copy")),
            "archive":str(source.get("creature_archive", "")),
            "record":int(source.get("creature_record", -1)),"npc":false,
            "equipment":[],"wealth":_wealth_values(parsed.get("wealth", {})),
            "art_basis":str(source.get("art_basis", "Copied monster art")),
            "origin_key":str(source.get("origin_key", "")),"native_index":-1})
    result.sort_custom(func(a: Dictionary, b: Dictionary): return str(a.name) < str(b.name))
    return result


static func _copy_art_folder(source: String, target: String) -> String:
    var directory := DirAccess.open(source)
    if directory == null: return ""
    var error := DirAccess.make_dir_recursive_absolute(target)
    if error != OK: return "Cannot create %s (error %d)" % [target, error]
    for folder in directory.get_directories():
        var failure := _copy_art_folder(source.path_join(folder), target.path_join(folder))
        if not failure.is_empty(): return failure
    for filename in directory.get_files():
        if not filename.ends_with(".png"): continue
        error = DirAccess.copy_absolute(source.path_join(filename), target.path_join(filename))
        if error != OK: return "Cannot copy %s (error %d)" % [filename, error]
    return ""


static func _remove_art_folder(path: String) -> void:
    var directory := DirAccess.open(path)
    if directory == null: return
    for folder in directory.get_directories():
        _remove_art_folder(path.path_join(folder))
    for filename in directory.get_files():
        DirAccess.remove_absolute(path.path_join(filename))
    DirAccess.remove_absolute(path)


static func copy_record(record: Dictionary, name: String, root: String = "") -> Dictionary:
    var clean_name := name.strip_edges()
    if clean_name.is_empty(): return {"error":"Enter a name for the new monster."}
    var base := root if not root.is_empty() else tool_root()
    var key := ""
    var allocated := false
    for attempt in range(100):
        key = "CUSTOM:%d_%d" % [Time.get_ticks_usec(), attempt]
        if not FileAccess.file_exists(json_path(key, root)) and \
                not DirAccess.dir_exists_absolute(base.path_join("art").path_join(record_id(key))):
            allocated = true
            break
    if not allocated:
        return {"error":"Could not allocate a unique monster ID."}
    var copied: Dictionary = record.duplicate(true)
    var old_key := str(record.source.key)
    copied.name = clean_name
    copied.source.key = key
    copied.source.copied_from = old_key
    copied.source.origin_key = str(record.source.get("origin_key", old_key))
    rebuild_variants(copied)
    var source_art := base.path_join("art").path_join(record_id(old_key))
    var target_art := base.path_join("art").path_join(record_id(key))
    var failure := _copy_art_folder(source_art, target_art)
    if not failure.is_empty():
        _remove_art_folder(target_art)
        return {"error":failure}
    failure = save_record(copied, root)
    if not failure.is_empty():
        _remove_art_folder(target_art)
        return {"error":failure}
    return {"data":copied,"error":""}


static func save_record(record: Dictionary, root: String = "") -> String:
    rebuild_variants(record)
    var path := json_path(str(record.source.key), root)
    var directory := path.get_base_dir()
    var error := DirAccess.make_dir_recursive_absolute(directory)
    if error != OK:
        return "Cannot create %s (error %d)" % [directory, error]
    var temporary := path + ".tmp"
    var file := FileAccess.open(temporary, FileAccess.WRITE)
    if file == null:
        return "Cannot write %s" % temporary
    file.store_string(JSON.stringify(record, "  ") + "\n")
    file.flush()
    var write_error := file.get_error()
    file.close()
    if write_error != OK:
        DirAccess.remove_absolute(temporary)
        return "Cannot save %s (error %d)" % [path, write_error]
    error = DirAccess.rename_absolute(temporary, path)
    if error != OK:
        DirAccess.remove_absolute(temporary)
        return "Cannot replace %s (error %d)" % [path, error]
    return ""
