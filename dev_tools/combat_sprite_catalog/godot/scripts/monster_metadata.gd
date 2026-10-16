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


static func new_record(row: Dictionary, art: Dictionary) -> Dictionary:
    var equipment: Array = []
    for index in range(row.equipment.size()):
        var item: Dictionary = row.equipment[index]
        equipment.append({
            "id": "item_%03d" % index,
            "kind": source_kind(int(item.type)),
            "name": str(item.label),
            "source_type": int(item.type),
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
        },
        "description": "",
        "flag_for_regeneration": false,
        "equipment": equipment,
        "variants": [],
    }
    rebuild_variants(record)
    return record


static func rebuild_variants(record: Dictionary) -> void:
    var weapons: Array = []
    var shields: Array = []
    for entry in record.equipment:
        if not entry is Dictionary or str(entry.get("name", "")).strip_edges().is_empty():
            continue
        if entry.get("kind") == "weapon": weapons.append(entry)
        if entry.get("kind") == "shield": shields.append(entry)
    var variants: Array = []
    var key: String = record.source.key
    for weapon in weapons:
        for shield in shields:
            var variant_id := "%s__%s" % [weapon.id, shield.id]
            variants.append({
                "id": variant_id,
                "label": "%s + %s" % [weapon.name, shield.name],
                "weapon_id": weapon.id,
                "shield_id": shield.id,
                "ready": "art/%s/%s/ready.png" % [record_id(key), variant_id],
                "action": "art/%s/%s/action.png" % [record_id(key), variant_id],
            })
    if variants.is_empty():
        variants.append({
            "id": "default", "label": "Default (no weapon + shield pair)",
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
    var ids := {}
    for item in parsed.equipment:
        if not item is Dictionary or not (item.get("id") is String) or \
                not _safe_item_id(item.get("id", "")) or \
                not (item.get("kind") in ["weapon", "shield", "gear"]) or \
                not (item.get("name") is String) or ids.has(item.id):
            return {"data": new_record(row, art), "error": "Invalid equipment in %s" % path}
        ids[item.id] = true
    parsed["name"] = str(row.name)
    parsed["source"] = new_record(row, art).source
    parsed["description"] = str(parsed.get("description", ""))
    parsed["flag_for_regeneration"] = bool(parsed.get("flag_for_regeneration", false))
    rebuild_variants(parsed)
    return {"data": parsed, "error": ""}


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
