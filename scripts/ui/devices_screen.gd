extends Screen
## Versus controller setup: which side each keyboard half and each gamepad controls. Up and down
## pick a device, left and right move it between Player 1, nobody and Player 2.

const NAMES := {"kb_left": "Keyboard (WASD · U I O / J K L)", "kb_right": "Keyboard (arrows · numpad 7 8 9 / 4 5 6)",
	"pad0": "Gamepad 1", "pad1": "Gamepad 2"}

var _rows: Array[Dictionary] = []
var _sel := 0
var _continue: Button


func build() -> void:
	backdrop()
	back_to = "menu"
	heading("CONTROLLERS", 76, 40)
	Settings.set_solo(false)
	var y := 280.0
	for dev in Settings.DEVICES:
		var name := UiKit.label(NAMES[dev], 28, UiKit.TEXT)
		name.position = Vector2(240, y)
		name.size = Vector2(760, 50)
		add_child(name)
		var state := UiKit.label("", 22, UiKit.DIM)
		state.position = Vector2(240, y + 40)
		state.size = Vector2(760, 30)
		add_child(state)
		var side := UiKit.title("", 40, UiKit.TEXT)
		side.position = Vector2(1060, y)
		side.size = Vector2(620, 60)
		add_child(side)
		_rows.append({"dev": dev, "name": name, "state": state, "side": side})
		y += 120.0
	_continue = UiKit.button("Continue to character select", func(): _go(), 30)
	_continue.position = Vector2(660, 800)
	_continue.size = Vector2(600, 64)
	_continue.focus_mode = Control.FOCUS_NONE
	add_child(_continue)
	_refresh()
	hint("Up / Down: device   ·   Left / Right: move to a side   ·   Enter: continue   ·   Gamepads can be plugged in now")
	Input.joy_connection_changed.connect(func(_d, _c): _refresh())


func _refresh() -> void:
	var pads := Input.get_connected_joypads()
	for i in _rows.size():
		var r := _rows[i]
		var dev: String = r["dev"]
		var side: int = Settings.assign[dev]
		var label := "◀  PLAYER 1" if side == 0 else ("PLAYER 2  ▶" if side == 1 else "—  nobody  —")
		(r["side"] as Label).text = label
		(r["side"] as Label).add_theme_color_override("font_color", UiKit.ACCENT if side == 0 else (UiKit.AMBER if side == 1 else UiKit.DIM))
		(r["side"] as Label).horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT if side == 0 else (HORIZONTAL_ALIGNMENT_RIGHT if side == 1 else HORIZONTAL_ALIGNMENT_CENTER)
		var connected := true
		if dev.begins_with("pad"):
			var n := int(dev.trim_prefix("pad"))
			connected = n in pads
			(r["state"] as Label).text = (Input.get_joy_name(n) + "  ·  connected") if connected else "not connected"
		else:
			(r["state"] as Label).text = "always available"
		(r["name"] as Label).add_theme_color_override("font_color", UiKit.ACCENT if i == _sel else UiKit.TEXT)
		(r["name"] as Label).modulate.a = 1.0 if connected else 0.5
	var sides := [false, false]
	for r in _rows:
		var s: int = Settings.assign[r["dev"]]
		if s >= 0:
			sides[s] = true
	_continue.disabled = not (sides[0] and sides[1])
	_continue.text = "Continue to character select" if not _continue.disabled else "Each player needs a device"


func _unhandled_input(event: InputEvent) -> void:
	if not event.is_pressed() or event.is_echo():
		return
	if event.is_action_pressed("ui_up"):
		_sel = (_sel + _rows.size() - 1) % _rows.size()
	elif event.is_action_pressed("ui_down"):
		_sel = (_sel + 1) % _rows.size()
	elif event.is_action_pressed("ui_left") or event.is_action_pressed("ui_right"):
		# Left to right the slots are Player 1, nobody, Player 2.
		var order := [0, -1, 1]
		var dev: String = _rows[_sel]["dev"]
		var at := order.find(int(Settings.assign[dev]))
		at = clampi(at + (-1 if event.is_action_pressed("ui_left") else 1), 0, 2)
		Settings.set_assign(dev, order[at])
	elif event.is_action_pressed("ui_accept"):
		_go()
	else:
		super._unhandled_input(event)
		return
	Audio.play("ui_move", -6.0)
	_refresh()
	get_viewport().set_input_as_handled()


func _go() -> void:
	if _continue.disabled:
		Audio.play("ui_locked", -4.0)
		return
	Game.config["mode"] = "versus"
	Game.goto("select", {"mode": "versus"})
