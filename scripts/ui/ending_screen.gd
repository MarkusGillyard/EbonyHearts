extends Screen
## A champion's ending: their figure in the light and their story in a few lines, then the
## credits. Enter skips ahead.

var fighter := "ashka"
var _lines: Array[Label] = []
var _t := 0.0
var _credits: Control
var _phase := 0


func configure(data: Dictionary) -> void:
	fighter = str(data.get("fighter", "ashka"))


func build() -> void:
	backdrop(UiKit.AMBER)
	var pv := FighterPreview.new(Vector2i(620, 860))
	pv.position = Vector2(110, 120)
	add_child(pv)
	pv.show_fighter(fighter, Progress.is_unlocked("costume:" + fighter) and false)
	pv.celebrate()
	var def := Content.fighter(fighter)
	var name := UiKit.title(def.name.to_upper(), 72, UiKit.ACCENT, HORIZONTAL_ALIGNMENT_LEFT)
	name.position = Vector2(820, 140)
	name.size = Vector2(1000, 100)
	add_child(name)
	var y := 300.0
	for line in Content.story.get("fighters", {}).get(fighter, {}).get("ending", []):
		var l := UiKit.label(str(line), 32, UiKit.TEXT)
		l.position = Vector2(820, y)
		l.size = Vector2(960, 160)
		l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		l.modulate.a = 0.0
		add_child(l)
		_lines.append(l)
		y += 190.0
	_credits = _build_credits()
	_credits.visible = false
	add_child(_credits)
	Audio.play_music("music_ending")
	hint("Enter to continue")


func _build_credits() -> Control:
	var c := Control.new()
	c.set_anchors_preset(Control.PRESET_FULL_RECT)
	var bg := ColorRect.new()
	bg.color = Color(0, 0, 0, 0.85)
	bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	c.add_child(bg)
	var lines := [["MORTAL SLOPBAT", 96, UiKit.ACCENT], ["A game by Markus Gillyard", 40, UiKit.TEXT], ["", 30, UiKit.TEXT],
		["Built with the Godot Engine", 30, UiKit.DIM], ["Type: New Rocker and Oswald (SIL Open Font License)", 30, UiKit.DIM],
		["Characters, stages, sound and music made for this game", 30, UiKit.DIM], ["", 30, UiKit.TEXT], ["THE END", 72, UiKit.TEXT]]
	var y := 220.0
	for l in lines:
		var lab := UiKit.title(l[0], l[1], l[2]) if l[1] >= 60 else UiKit.label(l[0], l[1], l[2], HORIZONTAL_ALIGNMENT_CENTER)
		lab.position = Vector2(0, y)
		lab.size = Vector2(1920, l[1] * 1.4)
		c.add_child(lab)
		y += l[1] * 1.6
	return c


func _process(delta: float) -> void:
	_t += delta
	for i in _lines.size():
		_lines[i].modulate.a = clampf((_t - 1.0 - i * 3.0) * 0.8, 0.0, 1.0)


func _unhandled_input(event: InputEvent) -> void:
	if not event.is_pressed() or event.is_echo() or not (event.is_action("ui_accept") or event.is_action("p1_hp") or event.is_action("p1_lp")):
		return
	get_viewport().set_input_as_handled()
	if _phase == 0:
		if _t < 1.0 + _lines.size() * 3.0:
			_t = 1.0 + _lines.size() * 3.0
			return
		_phase = 1
		_credits.visible = true
		Audio.play("ui_select")
	else:
		Game.arcade = {}
		Game.goto("menu")
