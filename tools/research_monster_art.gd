extends SceneTree
# Research tool: extracts names, checks same-ID CPIC candidates and ID+128 pairs.
# It does not claim that matching IDs prove the game's runtime resource selection.
# Run: godot_console --headless --path demos/godot --script ../../tools/research_monster_art.gd
func _initialize() -> void:
    var folder := GameFinder.game_folder("POOLRAD")
    var output_dir := ProjectSettings.globalize_path("res://../../user-data")
    DirAccess.make_dir_recursive_absolute(output_dir)
    var loader := DaxSpriteLoader.new()
    var monsters: Array[Dictionary] = []
    var art: Array[Dictionary] = []
    for bank in range(1, 9):
        var archive := "MON%dCHA.DAX" % bank
        var path := folder.path_join(archive)
        var data := FileAccess.get_file_as_bytes(path)
        for id in loader.list_record_ids(path):
            var record := loader._extract_record(data, id)
            if record.size() != 285 or record[0] > 15:
                push_error("Unexpected monster record: %s/%d" % [archive, id])
                continue
            monsters.append({"archive":archive,"id":id,"name":record.slice(1,1+record[0]).get_string_from_ascii()})
        archive = "CPIC%d.DAX" % bank
        path = folder.path_join(archive)
        var ids := loader.list_record_ids(path)
        for id in ids:
            if id >= 128: continue
            var a := loader.load_frame(path, id, 0, true)
            var b := loader.load_frame(path, id + 128, 0, true)
            var valid := a != null and b != null
            if valid: valid = a.get_size() == b.get_size()
            art.append({"archive":archive,"id":id,"valid":valid})
    var csv := FileAccess.open(output_dir.path_join("monster-art-candidates.csv"),FileAccess.WRITE)
    csv.store_csv_line(PackedStringArray(["monster_archive","monster_id","name","role","art_archive","art_record","image_index","verification","evidence"]))
    var unresolved := FileAccess.open(output_dir.path_join("monster-art-unresolved.csv"),FileAccess.WRITE)
    unresolved.store_csv_line(PackedStringArray(["monster_archive","monster_id","name","reason"]))
    var matched := 0
    var base_ids := {}
    for monster in monsters:
        var found := false
        for entry in art:
            if entry.id != monster.id or not entry.valid: continue
            found = true
            base_ids[entry.id] = true
            for role in ["ready_candidate", "action_candidate"]:
                var pose: int = entry.id + (128 if role == "action_candidate" else 0)
                csv.store_csv_line(PackedStringArray([monster.archive,str(monster.id),monster.name,role,entry.archive,str(pose),"0","data_consistent_candidate","Name from length-prefixed MON record; matching CPIC ID and decodable equal-size ID+128 partner. Archive selection and gameplay not verified."]))
        if found: matched += 1
        else: unresolved.store_csv_line(PackedStringArray([monster.archive,str(monster.id),monster.name,"No same-ID CPIC pair in any bank; may use alias or assembled character icon. Not assigned."]))
    print("%d monster records; %d have same-ID candidates; %d unresolved; %d distinct matched artwork IDs; %d archive-local CPIC pairs." % [monsters.size(),matched,monsters.size()-matched,base_ids.size(),art.size()])
    print("Reports: ",output_dir)
    quit()
