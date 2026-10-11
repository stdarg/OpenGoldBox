extends Window


func place_training(supplemental: bool) -> void:
	var metrics := "OpenGoldMetrics"
	var label := get_node("AdvancementTrainingLabel") as Control
	var choice := get_node("AdvancementTraining") as Control
	label.position.y = label.get_theme_constant(
		"level_training_supplemental_label_y" if supplemental else "level_training_label_y",
		metrics)
	choice.position.y = choice.get_theme_constant(
		"level_training_supplemental_choice_y" if supplemental else "level_training_choice_y",
		metrics)
