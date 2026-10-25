extends SceneTree

# Writes the Godot engine's license, the copyright of every third-party
# component compiled into it and those licenses' texts, as the engine itself
# reports them, so the package's notices match the engine it ships.
# Usage: godot --headless --script write_engine_notices.gd -- <output file>

func _init() -> void:
    var arguments := OS.get_cmdline_user_args()
    if arguments.size() != 1:
        printerr("Usage: -- <output file>")
        quit(1)
        return
    var file := FileAccess.open(arguments[0], FileAccess.WRITE)
    if file == null:
        printerr("Cannot write " + arguments[0])
        quit(1)
        return
    var version: Dictionary = Engine.get_version_info()
    file.store_string("Godot Engine %s\n\n%s\n" % [version.string, Engine.get_license_text()])
    file.store_string("\nThird-party components compiled into the Godot Engine\n")
    for component: Dictionary in Engine.get_copyright_info():
        file.store_string("\n- %s\n" % component.name)
        for part: Dictionary in component.parts:
            for holder: String in part.copyright:
                file.store_string("  Copyright %s\n" % holder)
            file.store_string("  License: %s\n" % part.license)
    var licenses: Dictionary = Engine.get_license_info()
    for name: String in licenses:
        file.store_string("\n\nLicense: %s\n\n%s\n" % [name, licenses[name]])
    file.close()
    quit()
