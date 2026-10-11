extends Node


func _place(path: String, rect: Rect2) -> void:
	var control := get_parent().get_node(path) as Control
	control.position = rect.position
	control.size = rect.size


func apply(race: bool, attributes: bool, sheet: bool) -> void:
	var view := get_parent() as Control
	var w := view.size.x
	var h := view.size.y
	var page := (view.get_node("PageBounds") as Control).get_rect()
	var preview := (view.get_node("PreviewBounds") as Control).get_rect()
	var x := page.position.x
	var y := page.position.y
	var pw := page.size.x
	var ph := page.size.y
	_place("Title", Rect2(24, 20, w - 48, 36))
	_place("Subtitle", Rect2(24, 64, w - 48, 26))
	_place("Steps", Rect2(24, 128, 180, h - 252))
	_place("Restart", Rect2(24, h - 110, 166, 36))
	_place("PageTitle", Rect2(x + 20, y + 18, pw - 40, 36))
	_place("Instructions", Rect2(x + 20, y + 60, pw - 40, 48))
	_place("Choices", Rect2(x + 20, y + 116, pw - 40, ph - (320 if race else 272)))
	_place("Description", Rect2(x + 20, y + ph - 140, pw - 40, 120))
	_place("GenderLabel", Rect2(x + 20, y + ph - 196, 106, 36))
	_place("Gender", Rect2(x + 126, y + ph - 196, pw - 146, 36))
	(view.get_node("Choices") as ItemList).fixed_column_width = (pw - 160) / 2.0
	_place("BackgroundLabel", Rect2(x + 20, y + 118, 106, 32))
	_place("Background", Rect2(x + 126, y + 114, pw - 146, 36))
	_place("BonusLabel", Rect2(x + 20, y + 164, 106, 32))
	_place("Bonus", Rect2(x + 126, y + 160, pw - 146, 36))
	_place("Columns", Rect2(x + 20, y + 208, pw - 40, 24))
	_place("DiceHeader", Rect2(x + 152, y + 208, 86, 32))
	_place("DiceHint", Rect2(x + 246, y + 208, pw - 266, 34))
	_place("BaseHeader", Rect2(x + 144, y + 208, 50, 24))
	_place("BonusHeader", Rect2(x + 198, y + 208, 48, 24))
	_place("TotalHeader", Rect2(x + 250, y + 208, 50, 24))
	for i in range(6):
		_place("Ability%d" % i, Rect2(x + 20, y + 244 + i * 47, 116, 37))
		_place("Dice%d" % i, Rect2(x + pw - 72, y + 244 + i * 47, 52, 37))
		_place("Score%d" % i, Rect2(x + 144, y + 244 + i * 47, 64, 37))
		_place("BonusScore%d" % i, Rect2(x + 220, y + 244 + i * 47, pw - 304, 37))
		_place("TotalScore%d" % i, Rect2(x + 250, y + 248 + i * 47, 50, 37))
	_place("Roll", Rect2(x + 20, y + ph - 58, 170, 36))
	_place("SwapHint", Rect2(x + 202, y + ph - 62, pw - 222, 46))
	if attributes:
		_place("BackgroundLabel", Rect2(x + 20, y + 62, 106, 32))
		_place("Background", Rect2(x + 126, y + 58, pw - 146, 36))
		_place("BonusLabel", Rect2(x + 20, y + 104, 106, 32))
		_place("Bonus", Rect2(x + 126, y + 100, pw - 146, 36))
		_place("Columns", Rect2(x + 20, y + 142, 110, 24))
		_place("DiceHeader", Rect2(x + 192, y + 142, 86, 24))
		_place("DiceHint", Rect2(x + 20, y + 166, 254, 30))
		(view.get_node("DiceHint") as Label).theme_type_variation = "LabelText12"
		_place("TargetsTitle", Rect2(x + 282, y + 142, pw - 302, 28))
		_place("Targets", Rect2(x + 282, y + 178, pw - 302, ph - 336))
		_place("TargetHint", Rect2(x + 282, y + ph - 150, pw - 302, 90))
		for i in range(6):
			var row := y + 200 + i * 58
			_place("Ability%d" % i, Rect2(x + 20, row, 98, 32))
			(view.get_node("Ability%d" % i) as Button).theme_type_variation = "ButtonText13"
			_place("Score%d" % i, Rect2(x + 126, row, 60, 32))
			_place("Dice%d" % i, Rect2(x + 202, row, 44, 32))
			_place("BonusScore%d" % i, Rect2(x + 20, row + 34, 100, 24))
			(view.get_node("BonusScore%d" % i) as Label).theme_type_variation = "LabelText11"
			_place("Warning%d" % i, Rect2(x + 126, row + 32, 144, 26))
		_place("SwapHint", Rect2(x + 202, y + ph - 56, pw - 222, 42))
		(view.get_node("SwapHint") as Label).theme_type_variation = "LabelText13"
	_place("TrainingFixed", Rect2(x + 20, y + 116, pw - 40, 126))
	_place("Training", Rect2(x + 20, y + 250, pw - 40, ph - 270))
	_place("SpellChoices", Rect2(x + 20, y + 132, pw - 40, ph - 152))
	_place("Name", Rect2(x + 20, y + 138, pw - 40, 46))
	for stem in ["CombatHead", "Weapon"]:
		var row := 0 if stem == "CombatHead" else 1
		_place(stem + "Previous", Rect2(x + 20, y + 126 + row * 48, 110, 36))
		_place(stem + "Label", Rect2(x + 144, y + 130 + row * 48, pw - 290, 30))
		_place(stem + "Next", Rect2(x + pw - 130, y + 126 + row * 48, 110, 36))
	_place("Size", Rect2(x + 20, y + 222, 150, 34))
	_place("ColorTitle", Rect2(x + 20, y + 264, pw - 40, 26))
	var colorw := (pw - 166) / 2.0
	_place("Color1Title", Rect2(x + 130, y + 264, colorw, 26))
	_place("Color2Title", Rect2(x + 138 + colorw, y + 264, colorw, 26))
	for part in range(6):
		_place("Part%d" % part, Rect2(x + 20, y + 300 + part * 35, 105, 28))
		for bank in range(2):
			_place("Color%d_%d" % [bank, part],
				Rect2(x + 130 + bank * (colorw + 8), y + 294 + part * 35, colorw, 30))
	_place("PaletteHint", Rect2(x + 20, y + ph - 106, pw - 40, 24))
	var swatch := (pw - 40 - 7 * 6) / 8.0
	for i in range(16):
		_place("Palette%d" % i,
			Rect2(x + 20 + (i % 8) * (swatch + 6), y + ph - 76 + (i / 8) * 30, swatch, 25))
	var px := preview.position.x
	var py := preview.position.y
	_place("PreviewTitle", Rect2(px + 18, py + 18, 282, 24))
	_place("PreviewName", Rect2(px + 18, py + 50, 282, 36))
	_place("PortraitPrevious", Rect2(px + 18, py + 374, 36, 32))
	_place("PortraitSelect", Rect2(px + 60, py + 374, 198, 32))
	_place("PortraitNext", Rect2(px + 264, py + 374, 36, 32))
	_place("PortraitGender", Rect2(px + 18, py + 414, 136, 32))
	_place("PortraitClass", Rect2(px + 164, py + 414, 136, 32))
	_place("PortraitRace", Rect2(px + 18, py + 454, 282, 32))
	_place("ReadyLabel", Rect2(px + 26, py + 488, 120, 28))
	_place("ActionLabel", Rect2(px + 172, py + 488, 120, 28))
	(view.get_node("PreviewSummary") as Control).visible = ph >= 656
	_place("PreviewSummary", Rect2(px + 18, py + 596, 282, maxf(1.0, ph - 604)))
	_place("Back", Rect2(x, h - 60, 150, 38))
	_place("Next", Rect2(x + pw - 190, h - 60, 190, 38))
	_place("Status", Rect2(x + 160, h - 62, maxf(1.0, pw - 360), 44))
	_place("Footer", Rect2(24, h - 25, w - 48, 22))
	_place("Modifiers", Rect2(x + 20, y + ph - 60, 150, 36))
	_place("SavingThrows", Rect2(x + 180, y + ph - 60, 160, 36))
	var mw := minf(780.0, w - 100)
	var mh := h - 120
	(view.get_node("ModifiersModal") as Window).size = Vector2i(mw, mh)
	_place("ModifiersModal/Background", Rect2(0, 0, mw, mh))
	_place("ModifiersModal/Title", Rect2(24, 18, mw - 48, 36))
	_place("ModifiersModal/Text", Rect2(24, 70, mw - 48, mh - 140))
	_place("ModifiersModal/Close", Rect2(mw - 154, mh - 52, 130, 36))
	(view.get_node("SavingThrowsModal") as Window).size = Vector2i(mw, mh)
	_place("SavingThrowsModal/Background", Rect2(0, 0, mw, mh))
	_place("SavingThrowsModal/Title", Rect2(24, 18, mw - 48, 36))
	_place("SavingThrowsModal/DCLabel", Rect2(24, 66, 110, 36))
	_place("SavingThrowsModal/DC", Rect2(144, 66, 90, 36))
	_place("SavingThrowsModal/Text", Rect2(24, 118, mw - 48, mh - 188))
	_place("SavingThrowsModal/Close", Rect2(mw - 154, mh - 52, 130, 36))
	if sheet:
		_place("Description", Rect2(x + 20, y + 124, pw - 40, ph - 194))
