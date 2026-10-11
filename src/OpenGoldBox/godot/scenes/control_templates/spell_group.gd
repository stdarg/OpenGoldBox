extends VBoxContainer


func set_level_up(active: bool) -> void:
    theme_type_variation = &"LevelUpSpellSection" if active else &"SpellRows"
    get_node("Pending").theme_type_variation = &"LevelUpPending" if active else &"TrainingPending"
