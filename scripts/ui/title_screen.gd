extends Screen
## The title: the name in dripping green, the premise fading in line by line, and a prompt.

var _press: Label
var _lines: Array[Label] = []
var _t := 0.0


func build() -> void:
	backdrop()
	var title := UiKit.title("MORTAL SLOPBAT", 168, UiKit.ACCENT)
	title.position = Vector2(0, 230)
	title.size = Vector2(1920, 240)
	title.add_theme_color_override("font_outline_color", Color(0.03, 0.06, 0.02))
	title.add_theme_constant_override("outline_size", 22)
	add_child(title)
	var y := 520.0
	for line in Content.story.get("premise", []):
		var l := UiKit.label(str(line), 28, UiKit.TEXT, HORIZONTAL_ALIGNMENT_CENTER)
		l.position = Vector2(260, y)
		l.size = Vector2(1400, 40)
		l.modulate.a = 0.0
		add_child(l)
		_lines.append(l)
		y += 46.0
	_press = UiKit.title("PRESS ENTER", 44, UiKit.AMBER)
	_press.position = Vector2(0, 840)
	_press.size = Vector2(1920, 60)
	add_child(_press)
	var by := UiKit.label("© 2026 Markus Gillyard", 20, UiKit.DIM, HORIZONTAL_ALIGNMENT_CENTER)
	by.position = Vector2(0, 1030)
	by.size = Vector2(1920, 30)
	add_child(by)
	Audio.play_music("music_menu")


func _process(delta: float) -> void:
	_t += delta
	for i in _lines.size():
		_lines[i].modulate.a = clampf((_t - 0.6 - i * 1.1) * 0.9, 0.0, 1.0)
	_press.modulate.a = 0.55 + 0.45 * sin(_t * 3.0)


func _unhandled_input(event: InputEvent) -> void:
	var pressed := (event is InputEventKey or event is InputEventJoypadButton) and event.is_pressed() and not event.is_echo()
	if pressed and (event.is_action("ui_accept") or event.is_action("pause") or event is InputEventJoypadButton):
		Audio.play("ui_select")
		Game.goto("menu")
		get_viewport().set_input_as_handled()
