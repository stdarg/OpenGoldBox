extends SceneTree

# The game ships data/monsters, the Combat Sprite Catalog's export. It must hold
# exactly what an export of the catalog's records and art would write now, so
# catalog changes reach the game. So must data/art/closeups, the export of its
# new close-up art. Run with --path dev_tools/combat_sprite_catalog/godot.

const MonsterExport = preload("res://scripts/monster_export.gd")
const CloseUpStore = preload("res://scripts/close_up_store.gd")


func _init() -> void:
    if not MonsterExport.up_to_date():
        printerr("data/monsters is out of date: run the Combat Sprite Catalog with --export-to-game"
            + " (dev_tools/combat_sprite_catalog/README.md) and commit data/monsters")
        quit(1)
        return
    if not CloseUpStore.up_to_date():
        printerr("data/art/closeups is out of date: run the Combat Sprite Catalog with --export-to-game"
            + " and commit data/art/closeups")
        quit(1)
        return
    print("Monster export checks passed: data/monsters and data/art/closeups match the catalog")
    quit()
