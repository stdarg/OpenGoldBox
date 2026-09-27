extends SceneTree
func _initialize() -> void:
    call_deferred("start")
func start() -> void:
    # The application autoload applies saved settings during startup.
    for frame in range(5): await process_frame
    root.gui_embed_subwindows = true
    root.size = Vector2i(1120,800)
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--mastery-locale="): TranslationServer.set_locale(arg.trim_prefix("--mastery-locale="))
        if arg == "--mastery-large": root.size = Vector2i(1920,1080)
    for frame in range(5): await process_frame
    change_scene_to_file("res://scenes/rolf_tour.tscn")
