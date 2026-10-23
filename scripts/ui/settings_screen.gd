extends Screen
## Settings as one list: each row shows its value; Enter or Left/Right changes it. Volumes move in
## steps of ten percent. Key bindings for both keyboard halves are changed by selecting the row
## and pressing the new key (Escape cancels). Everything is saved as it changes.

const CHOICES := {
	"graphics": [["high", "High"], ["low", "Low"]],
	"gore": [["full", "Full"], ["low", "Low"], ["off", "Off"]],
	"effects": [["full", "Full"], ["reduced", "Reduced"]],
}

var _rows: Array[Button] = []
var _box: VBoxContainer
var _scroll: ScrollContainer
var _waiting: Dictionary = {}
var _status: Label


func configure(data: Dictionary) -> void:
	back_to = str(data.get("back", "menu"))


func build() -> void:
	backdrop()
	if back_to == "":
		back_to = "menu"
	heading("SETTINGS", 76, 30)
	_scroll = ScrollContainer.new()
	_scroll.position = Vector2(460, 150)
	_scroll.size = Vector2(1000, 820)
	_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	add_child(_scroll)
	_box = VBoxContainer.new()
	_box.custom_minimum_size = Vector2(980, 0)
	_box.add_theme_constant_override("separation", 6)
	_scroll.add_child(_box)
	_status = UiKit.label("", 24, UiKit.AMBER, HORIZONTAL_ALIGNMENT_CENTER)
	_status.position = Vector2(460, 990)
	_status.size = Vector2(1000, 36)
	add_child(_status)
	_section("VIDEO")
	_toggle("Fullscreen", "fullscreen")
	_toggle("Vertical sync", "vsync")
	_choice("Graphics", "graphics")
	_toggle("Vignette and grain", "post_fx")
	_toggle("Motion smoothing", "motion_smoothing")
	_section("SOUND")
	_volume("Master volume", "master_volume")
	_volume("Music volume", "music_volume")
	_volume("Effects volume", "sfx_volume")
	_section("FIGHTS")
	_choice("Gore", "gore")
	_choice("Effects", "effects")
	_toggle("Screen shake", "screen_shake")
	_toggle("Bright flashes", "flashes")
	_toggle("Input display", "input_display")
	_section("KEYBOARD, LEFT HALF  (player 1 by default)")
	for a in Settings.ACTIONS:
		_binding("kb_left", a)
	_section("KEYBOARD, RIGHT HALF  (player 2 by default)")
	for a in Settings.ACTIONS:
		_binding("kb_right", a)
	_action_row("Reset key bindings", func():
		Settings.reset_bindings()
		_refresh()
		_status.text = "Key bindings reset.")
	var pad := UiKit.label("Gamepads: Y high punch · X low punch · B high kick · A low kick · RB/RT block · LB/LT run · Start pause", 20, UiKit.DIM)
	pad.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	pad.custom_minimum_size = Vector2(980, 60)
	_box.add_child(pad)
	_section("DATA")
	_action_row("Erase progress (records, unlocks, endings, the stranger's memory)", func(): _confirm_erase())
	_action_row("Back", func(): go_back())
	UiKit.chain_focus(_rows, false)
	_focus_first = _rows[0]
	_refresh()
	hint("Up / Down to move   ·   Enter or Left / Right to change   ·   Backspace to go back")


func _section(text: String) -> void:
	var l := UiKit.label(text, 26, UiKit.ACCENT, HORIZONTAL_ALIGNMENT_LEFT, UiKit.body_font(true))
	l.custom_minimum_size = Vector2(980, 56)
	l.vertical_alignment = VERTICAL_ALIGNMENT_BOTTOM
	_box.add_child(l)


func _row(meta: Dictionary, on_press: Callable) -> Button:
	var b := UiKit.button("", on_press, 26)
	b.alignment = HORIZONTAL_ALIGNMENT_LEFT
	b.custom_minimum_size = Vector2(980, 50)
	b.set_meta("row", meta)
	b.focus_entered.connect(func(): _scroll.ensure_control_visible(b))
	_box.add_child(b)
	_rows.append(b)
	return b


func _toggle(label: String, key: String) -> void:
	_row({"kind": "toggle", "label": label, "key": key}, func():
		Settings.set_value(key, not Settings.values[key])
		_refresh())


func _choice(label: String, key: String) -> void:
	_row({"kind": "choice", "label": label, "key": key}, func(): _cycle(key, 1))


func _volume(label: String, key: String) -> void:
	_row({"kind": "volume", "label": label, "key": key}, func(): _step(key, 0.1, true))


func _binding(dev: String, action: String) -> void:
	_row({"kind": "bind", "label": Settings.ACTION_LABELS[action], "dev": dev, "action": action}, func():
		_waiting = {"dev": dev, "action": action}
		_status.text = "Press the new key for %s (Escape to cancel)" % Settings.ACTION_LABELS[action]
		_refresh())


func _action_row(label: String, on_press: Callable) -> void:
	_row({"kind": "action", "label": label}, on_press)


func _cycle(key: String, dir: int) -> void:
	var opts: Array = CHOICES[key]
	var i := 0
	for k in opts.size():
		if opts[k][0] == Settings.values[key]:
			i = k
	Settings.set_value(key, opts[(i + dir + opts.size()) % opts.size()][0])
	_refresh()


func _step(key: String, amount: float, wrap := false) -> void:
	var v: float = Settings.values[key] + amount
	if wrap and v > 1.001:
		v = 0.0
	# Tenths by division, so the saved value is 0.3 and not 0.30000000000000004.
	Settings.set_value(key, roundf(clampf(v, 0.0, 1.0) * 10.0) / 10.0)
	Audio.play("ui_move")
	_refresh()


func _refresh() -> void:
	for b in _rows:
		var m: Dictionary = b.get_meta("row")
		match m["kind"]:
			"toggle":
				b.text = "%s:  %s" % [m["label"], "On" if Settings.values[m["key"]] else "Off"]
			"choice":
				var label := str(Settings.values[m["key"]])
				for o in CHOICES[m["key"]]:
					if o[0] == Settings.values[m["key"]]:
						label = o[1]
				b.text = "%s:  %s" % [m["label"], label]
			"volume":
				b.text = "%s:  %d%%" % [m["label"], int(round(float(Settings.values[m["key"]]) * 100.0))]
			"bind":
				var waiting: bool = _waiting.get("dev", "") == m["dev"] and _waiting.get("action", "") == m["action"]
				b.text = "%s:  %s" % [m["label"], "…" if waiting else Settings.key_label(m["dev"], m["action"])]
			_:
				b.text = m["label"]


func _confirm_erase() -> void:
	if _status.text.begins_with("Press Enter again"):
		Progress.reset_all()
		_status.text = "Progress erased."
	else:
		_status.text = "Press Enter again on that line to erase all progress."


func _input(event: InputEvent) -> void:
	if _waiting.is_empty() or not (event is InputEventKey) or not event.pressed or event.echo:
		return
	get_viewport().set_input_as_handled()
	if event.physical_keycode == KEY_ESCAPE:
		_status.text = "Unchanged."
	else:
		Settings.rebind(_waiting["dev"], _waiting["action"], event.physical_keycode)
		_status.text = "%s is now %s." % [Settings.ACTION_LABELS[_waiting["action"]], OS.get_keycode_string(event.physical_keycode)]
	_waiting = {}
	_refresh()


func _unhandled_input(event: InputEvent) -> void:
	if not _waiting.is_empty() or not event.is_pressed():
		return
	var f := get_viewport().gui_get_focus_owner()
	if f is Button and f.has_meta("row") and (event.is_action_pressed("ui_left") or event.is_action_pressed("ui_right")):
		var m: Dictionary = f.get_meta("row")
		var dir := -1 if event.is_action_pressed("ui_left") else 1
		match m["kind"]:
			"toggle":
				Settings.set_value(m["key"], not Settings.values[m["key"]])
				_refresh()
			"choice":
				_cycle(m["key"], dir)
			"volume":
				_step(m["key"], 0.1 * dir)
		get_viewport().set_input_as_handled()
		return
	super._unhandled_input(event)
