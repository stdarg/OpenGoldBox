extends RefCounted

# Exports the catalog's saved monster records into data/monsters, which the
# game ships (#274): one folder per creature, named like its catalog JSON, with
# monster.json and each weapon set's ready.png and action.png. The game reads
# only this export, never the catalog's own folders.

const MonsterMetadata = preload("res://scripts/monster_metadata.gd")
const FORMAT := "opengold-monster-v1"
const ART_SIZE := 128


static func game_folder() -> String:
    return ProjectSettings.globalize_path("res://../../../data/monsters").simplify_path()


# The original MONnITM slot an item came from, or -1 for gear added in the
# catalog.
static func _source_index(item: Dictionary) -> int:
    var id := str(item.get("id", ""))
    if not id.begins_with("item_") or not id.trim_prefix("item_").is_valid_int():
        return -1
    return int(id.trim_prefix("item_"))


static func _valid_art(path: String) -> bool:
    var image := Image.new()
    return FileAccess.file_exists(path) and image.load(path) == OK and image.get_width() == ART_SIZE and image.get_height() == ART_SIZE


# The record as the game reads it, with the art paths relative to its folder.
# A record flagged for regeneration keeps its data but exports no art.
static func game_record(record: Dictionary, root: String) -> Dictionary:
    var source: Dictionary = record.source
    var equipment: Array = []
    for item in record.equipment:
        equipment.append({"id": item.id, "name": item.name, "kind": item.kind,
                          "weapon_role": item.get("weapon_role", ""),
                          "readied": bool(item.get("readied", false)),
                          "source_type": int(item.get("source_type", -1)),
                          "source_index": _source_index(item)})
    var variants: Array = []
    if not bool(record.get("flag_for_regeneration", false)):
        for variant in record.variants:
            var ready := root.path_join(str(variant.ready))
            var action := root.path_join(str(variant.action))
            if _valid_art(ready) and _valid_art(action):
                variants.append({"id": variant.id, "weapon_id": variant.weapon_id,
                                 "shield_id": variant.shield_id,
                                 "ready": str(variant.id).path_join("ready.png"),
                                 "action": str(variant.id).path_join("action.png")})
    var result := {"format": FORMAT, "key": source.key,
                   "creature_archive": source.get("creature_archive", ""),
                   "creature_record": int(source.get("creature_record", -1)),
                   "name": record.name, "sprite_size": record.get("sprite_size", null),
                   "humanoid_size": record.get("humanoid_size", "na"),
                   "equipment": equipment, "variants": variants}
    if source.has("copied_from"):
        result["copied_from"] = source.copied_from
        result["origin_key"] = source.get("origin_key", source.copied_from)
    return result


# Whether the game can stage the record: a MONnCHA creature, or a copy of one.
static func _exported(record: Dictionary) -> bool:
    return str(record.source.get("creature_archive", "")).begins_with("MON") and \
        int(record.source.get("creature_record", -1)) >= 0


static func _remove_tree(path: String) -> void:
    for folder in DirAccess.get_directories_at(path):
        _remove_tree(path.path_join(folder))
    for name in DirAccess.get_files_at(path):
        DirAccess.remove_absolute(path.path_join(name))
    DirAccess.remove_absolute(path)


static func _write(path: String, text: String) -> String:
    var file := FileAccess.open(path, FileAccess.WRITE)
    if file == null:
        return "Cannot write " + path
    file.store_string(text)
    file.close()
    return ""


# Replaces the export with the saved records. Returns an error message, or "".
static func export_all(root: String = "", target: String = "") -> String:
    var tool := root if not root.is_empty() else MonsterMetadata.tool_root()
    var folder := target if not target.is_empty() else game_folder()
    var records := {}
    var json_folder := tool.path_join("json")
    for name in DirAccess.get_files_at(json_folder):
        if not name.ends_with(".json"):
            continue
        var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(json_folder.path_join(name)))
        if not parsed is Dictionary or not parsed.get("source") is Dictionary or \
                not parsed.get("equipment") is Array or not parsed.get("variants") is Array:
            return "Cannot read " + json_folder.path_join(name)
        if _exported(parsed):
            records[name.trim_suffix(".json")] = parsed
    if DirAccess.make_dir_recursive_absolute(folder) != OK:
        return "Cannot create " + folder
    for name in DirAccess.get_directories_at(folder):
        if not records.has(name):
            _remove_tree(folder.path_join(name))
    for id in records:
        var record: Dictionary = records[id]
        var exported := game_record(record, tool)
        var destination := folder.path_join(id)
        if DirAccess.dir_exists_absolute(destination):
            _remove_tree(destination)
        for variant in exported.variants:
            var variant_folder := destination.path_join(str(variant.id))
            if DirAccess.make_dir_recursive_absolute(variant_folder) != OK:
                return "Cannot create " + variant_folder
            for pose in ["ready", "action"]:
                var source: String = ""
                for saved in record.variants:
                    if saved.id == variant.id:
                        source = tool.path_join(str(saved[pose]))
                if DirAccess.copy_absolute(source, variant_folder.path_join(pose + ".png")) != OK:
                    return "Cannot export " + source
        if DirAccess.make_dir_recursive_absolute(destination) != OK:
            return "Cannot create " + destination
        var error := _write(destination.path_join("monster.json"), JSON.stringify(exported, "  ") + "\n")
        if not error.is_empty():
            return error
    return ""


# Whether data/monsters holds exactly what an export would write now.
static func up_to_date(root: String = "", target: String = "") -> bool:
    var tool := root if not root.is_empty() else MonsterMetadata.tool_root()
    var folder := target if not target.is_empty() else game_folder()
    var expected := {}
    var json_folder := tool.path_join("json")
    for name in DirAccess.get_files_at(json_folder):
        var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(json_folder.path_join(name))) \
            if name.ends_with(".json") else null
        if parsed is Dictionary and _exported(parsed):
            expected[name.trim_suffix(".json")] = parsed
    if Array(DirAccess.get_directories_at(folder)).size() != expected.size():
        return false
    for id in expected:
        var record: Dictionary = expected[id]
        var exported := game_record(record, tool)
        var written: Variant = JSON.parse_string(
            FileAccess.get_file_as_string(folder.path_join(id).path_join("monster.json")))
        if written != JSON.parse_string(JSON.stringify(exported)):
            return false
        for variant in exported.variants:
            for saved in record.variants:
                if saved.id != variant.id:
                    continue
                for pose in ["ready", "action"]:
                    if FileAccess.get_md5(tool.path_join(str(saved[pose]))) != \
                            FileAccess.get_md5(folder.path_join(id).path_join(str(variant[pose]))):
                        return false
    return true
