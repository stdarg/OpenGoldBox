extends SceneTree
const Decoder = preload("res://scripts/por_ecl_decoder.gd")
const Evidence = preload("res://scripts/art_script_evidence.gd")

func _initialize() -> void:
    var folder := GameFinder.game_folder("POOLRAD")
    var loader := DaxSpriteLoader.new()
    var scripts: Array = []
    var directory := DirAccess.open(folder)
    if directory == null:
        push_error("Game directory unavailable")
        quit(1)
        return
    for archive in directory.get_files():
        if not archive.to_upper().match("ECL*.DAX"): continue
        var path := folder.path_join(archive)
        var bytes := FileAccess.get_file_as_bytes(path)
        for id in loader.list_record_ids(path):
            var record := loader._extract_record(bytes,id)
            var result = Decoder.new().decode(record)
            result.archive = archive
            result.record = id
            scripts.append(result)
            print("%s/%d: %d instructions, %d diagnostics %s" % [archive,id,result.nodes.size(),result.diagnostics.size(),result.diagnostics])
    var output := ProjectSettings.globalize_path("res://../../user-data/ecl-art-research.json")
    DirAccess.make_dir_recursive_absolute(output.get_base_dir())
    var file := FileAccess.open(output,FileAccess.WRITE)
    file.store_string(JSON.stringify({"scripts":scripts},"  "))
    file.close()
    print(output)
    var evidence = Evidence.new()
    evidence.build(folder)
    var evidence_file := FileAccess.open("res://../../user-data/art-script-evidence.json",FileAccess.WRITE)
    evidence_file.store_string(JSON.stringify(evidence.export_data(),"  "))
    evidence_file.close()
    print("Evidence: ",evidence.summary)
    var report := FileAccess.open("res://../../user-data/unresolved-art-evidence.md",FileAccess.WRITE)
    report.store_line("# Unresolved art evidence\n\nStatic candidates; resource banks and gameplay remain unverified.\n")
    var inventory_path := "user://art-group-inventory.json"
    if FileAccess.file_exists(inventory_path):
        var inventory = JSON.parse_string(FileAccess.get_file_as_string(inventory_path))
        if inventory is Dictionary:
            for group in inventory.get("groups",[]):
                if group.get("resolution","unresolved") != "unresolved": continue
                var source: Dictionary = group.sources[0][0]
                report.store_line("## %s / %d\n\nGroup `%s`\n" % [source.archive,source.record,group.key])
                var entries: Array = evidence.for_group(group)
                if entries.is_empty(): report.store_line("No resolved script reference.\n")
                for entry in entries:
                    report.store_line("- %s/%d @0x%04X: %s. %s." % [entry.script_archive,entry.script_record,entry.address,entry.command,entry.archive_basis])
                    for candidate in entry.candidates:
                        report.store_line("  - Candidate: %s [%s]. %s." % [candidate.name,candidate.key,candidate.basis])
                    if entry.has("table_lookup"): report.store_line("  - Table lookup: " + JSON.stringify(entry.table_lookup))
                    for text in entry.dialogue:
                        report.store_line("  - Dialogue @0x%04X: %s" % [text.address,text.text])
                report.store_line("")
    else:
        report.store_line("Run the art review tool once to create the current group inventory.")
    report.close()
    quit()
