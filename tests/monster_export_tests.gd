extends SceneTree

# The game ships data/monsters, the Combat Sprite Catalog's export. It must hold
# exactly what an export of the catalog's records and art would write now, so
# catalog changes reach the game. Run with --path dev_tools/combat_sprite_catalog/godot.

const MonsterExport = preload("res://scripts/monster_export.gd")


func _init() -> void:
    if not MonsterExport.up_to_date():
        printerr("data/monsters is out of date: run the Combat Sprite Catalog with --export-to-game"
            + " (dev_tools/combat_sprite_catalog/README.md) and commit data/monsters")
        quit(1)
        return
    print("Monster export checks passed: data/monsters matches the catalog")
    quit()
