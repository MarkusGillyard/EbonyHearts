extends Node
## Preferences, key bindings and which device controls which side, stored in user://settings.cfg
## and applied on load. A damaged file is ignored key by key: anything unreadable keeps its default.

const PATH := "user://settings.cfg"
const ACTIONS := ["up", "down", "left", "right", "hp", "lp", "hk", "lk", "bl", "rn"]
const ACTION_LABELS := {"up": "Up (jump)", "down": "Down (crouch)", "left": "Left", "right": "Right",
	"hp": "High Punch", "lp": "Low Punch", "hk": "High Kick", "lk": "Low Kick", "bl": "Block", "rn": "Run"}
## The two keyboard sets mirror the arcade panel: HP BL HK on the top row, LP RN LK below.
const DEFAULT_KEYS := {
	"kb_left": {"up": KEY_W, "down": KEY_S, "left": KEY_A, "right": KEY_D, "hp": KEY_U, "bl": KEY_I, "hk": KEY_O,
		"lp": KEY_J, "rn": KEY_K, "lk": KEY_L},
	"kb_right": {"up": KEY_UP, "down": KEY_DOWN, "left": KEY_LEFT, "right": KEY_RIGHT, "hp": KEY_KP_7, "bl": KEY_KP_8,
		"hk": KEY_KP_9, "lp": KEY_KP_4, "rn": KEY_KP_5, "lk": KEY_KP_6},
}
## Standard (Xbox-style) layout: Y high punch, X low punch, B high kick, A low kick, right
## shoulder or trigger block, left shoulder or trigger run.
const PAD_BUTTONS := {"up": JOY_BUTTON_DPAD_UP, "down": JOY_BUTTON_DPAD_DOWN, "left": JOY_BUTTON_DPAD_LEFT,
	"right": JOY_BUTTON_DPAD_RIGHT, "hp": JOY_BUTTON_Y, "lp": JOY_BUTTON_X, "hk": JOY_BUTTON_B, "lk": JOY_BUTTON_A,
	"bl": JOY_BUTTON_RIGHT_SHOULDER, "rn": JOY_BUTTON_LEFT_SHOULDER}
const DEVICES := ["kb_left", "kb_right", "pad0", "pad1"]

var values := {
	"fullscreen": false,
	"vsync": true,
	## "high" or "low": soft shadows, ambient occlusion, fog volumes.
	"graphics": "high",
	"master_volume": 0.8,
	"music_volume": 0.5,
	"sfx_volume": 0.9,
	"screen_shake": true,
	"flashes": true,
	## "full" or "reduced": particle counts and lingering effects.
	"effects": "full",
	## "full", "low" or "off".
	"gore": "full",
	"input_display": false,
	"motion_smoothing": true,
	"difficulty": 1,
}
var keys := {"kb_left": DEFAULT_KEYS["kb_left"].duplicate(), "kb_right": DEFAULT_KEYS["kb_right"].duplicate()}
## Which side (0 or 1, or -1 for none) each device controls in two-player modes.
var assign := {"kb_left": 0, "kb_right": 1, "pad0": 0, "pad1": 1}
## In one-player modes every device drives player 1.
var solo := true


var path := PATH


func _ready() -> void:
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--profile="):
			# Test and development runs keep their settings apart from the player's.
			path = "user://settings_%s.cfg" % a.trim_prefix("--profile=").validate_filename()
	load_settings()
	apply()


func get_value(k: String) -> Variant:
	if k == "graphics":
		for a in OS.get_cmdline_user_args():
			if a.begins_with("--graphics="):
				return a.trim_prefix("--graphics=")
	return values.get(k)


func set_value(k: String, v: Variant) -> void:
	values[k] = v
	apply()
	save_settings()


func load_settings() -> void:
	var cf := ConfigFile.new()
	if cf.load(path) != OK:
		return
	for k in values:
		var v: Variant = cf.get_value("prefs", k, values[k])
		if typeof(v) == typeof(values[k]) or (typeof(values[k]) == TYPE_FLOAT and typeof(v) == TYPE_INT):
			values[k] = v
	for dev in keys:
		for a in ACTIONS:
			var code: Variant = cf.get_value("keys_" + dev, a, keys[dev][a])
			if code is int and code > 0:
				keys[dev][a] = code
	for dev in DEVICES:
		var side: Variant = cf.get_value("devices", dev, assign[dev])
		if side is int and side >= -1 and side <= 1:
			assign[dev] = side


func save_settings() -> void:
	var cf := ConfigFile.new()
	for k in values:
		cf.set_value("prefs", k, values[k])
	for dev in keys:
		for a in ACTIONS:
			cf.set_value("keys_" + dev, a, keys[dev][a])
	for dev in DEVICES:
		cf.set_value("devices", dev, assign[dev])
	cf.save(path)


func rebind(dev: String, action: String, keycode: int) -> void:
	# A key used elsewhere in the same set swaps places, so no action is left unbound.
	for a in ACTIONS:
		if keys[dev][a] == keycode:
			keys[dev][a] = keys[dev][action]
	keys[dev][action] = keycode
	_apply_bindings()
	save_settings()


func reset_bindings() -> void:
	keys = {"kb_left": DEFAULT_KEYS["kb_left"].duplicate(), "kb_right": DEFAULT_KEYS["kb_right"].duplicate()}
	_apply_bindings()
	save_settings()


func set_solo(on: bool) -> void:
	if solo != on:
		solo = on
		_apply_bindings()


func set_assign(dev: String, side: int) -> void:
	assign[dev] = side
	_apply_bindings()
	save_settings()


func apply() -> void:
	if DisplayServer.get_name() != "headless":
		var mode := DisplayServer.WINDOW_MODE_FULLSCREEN if values["fullscreen"] else DisplayServer.WINDOW_MODE_WINDOWED
		if DisplayServer.window_get_mode() != mode:
			DisplayServer.window_set_mode(mode)
		var vsync: bool = values["vsync"] and not "--uncapped" in OS.get_cmdline_user_args()
		DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_ENABLED if vsync else DisplayServer.VSYNC_DISABLED)
	_apply_bindings()
	if has_node("/root/Audio"):
		get_node("/root/Audio").apply_volumes()


static func action_name(side: int, action: String) -> String:
	return "p%d_%s" % [side + 1, action]


func side_of(dev: String) -> int:
	return 0 if solo and assign[dev] >= 0 else int(assign[dev])


func _apply_bindings() -> void:
	for side in 2:
		for a in ACTIONS:
			var name := action_name(side, a)
			if not InputMap.has_action(name):
				InputMap.add_action(name, 0.45)
			InputMap.action_erase_events(name)
	for dev in DEVICES:
		var side := side_of(dev)
		if side < 0:
			continue
		for a in ACTIONS:
			var name := action_name(side, a)
			if dev.begins_with("kb"):
				var k := InputEventKey.new()
				k.physical_keycode = keys[dev][a]
				InputMap.action_add_event(name, k)
			else:
				var pad := int(dev.trim_prefix("pad"))
				var jb := InputEventJoypadButton.new()
				jb.device = pad
				jb.button_index = PAD_BUTTONS[a]
				InputMap.action_add_event(name, jb)
				if a in ["up", "down", "left", "right"]:
					var jm := InputEventJoypadMotion.new()
					jm.device = pad
					jm.axis = JOY_AXIS_LEFT_X if a in ["left", "right"] else JOY_AXIS_LEFT_Y
					jm.axis_value = -1.0 if a in ["left", "up"] else 1.0
					InputMap.action_add_event(name, jm)
				elif a in ["bl", "rn"]:
					var tr := InputEventJoypadMotion.new()
					tr.device = pad
					tr.axis = JOY_AXIS_TRIGGER_RIGHT if a == "bl" else JOY_AXIS_TRIGGER_LEFT
					tr.axis_value = 1.0
					InputMap.action_add_event(name, tr)
	_extra_actions()


func _extra_actions() -> void:
	# Menu keys: the left keyboard set and its punch keys work in menus as well as the arrows.
	for pair in [["ui_up", KEY_W], ["ui_down", KEY_S], ["ui_left", KEY_A], ["ui_right", KEY_D], ["ui_accept", KEY_J], ["ui_accept", KEY_U], ["ui_cancel", KEY_BACKSPACE]]:
		var k := InputEventKey.new()
		k.physical_keycode = pair[1]
		if not InputMap.action_has_event(pair[0], k):
			InputMap.action_add_event(pair[0], k)
	# Any gamepad can drive the menus.
	for pair in [["ui_accept", JOY_BUTTON_A], ["ui_cancel", JOY_BUTTON_B], ["ui_up", JOY_BUTTON_DPAD_UP], ["ui_down", JOY_BUTTON_DPAD_DOWN],
			["ui_left", JOY_BUTTON_DPAD_LEFT], ["ui_right", JOY_BUTTON_DPAD_RIGHT]]:
		var jb := InputEventJoypadButton.new()
		jb.button_index = pair[1]
		jb.device = -1
		if not InputMap.action_has_event(pair[0], jb):
			InputMap.action_add_event(pair[0], jb)
	for pair in [["ui_up", JOY_AXIS_LEFT_Y, -1.0], ["ui_down", JOY_AXIS_LEFT_Y, 1.0], ["ui_left", JOY_AXIS_LEFT_X, -1.0], ["ui_right", JOY_AXIS_LEFT_X, 1.0]]:
		var jm := InputEventJoypadMotion.new()
		jm.axis = pair[1]
		jm.axis_value = pair[2]
		jm.device = -1
		if not InputMap.action_has_event(pair[0], jm):
			InputMap.action_add_event(pair[0], jm)
	for extra in [["pause", KEY_ESCAPE, JOY_BUTTON_START], ["debug_boxes", KEY_F1, -1], ["debug_frames", KEY_F2, -1],
			["debug_states", KEY_F3, -1], ["training_reset", KEY_F4, JOY_BUTTON_BACK], ["debug_pause", KEY_F5, -1],
			["debug_step", KEY_F6, -1], ["debug_slow", KEY_F7, -1], ["input_display", KEY_F8, -1],
			["training_dummy", KEY_TAB, -1]]:
		if InputMap.has_action(extra[0]):
			continue
		InputMap.add_action(extra[0])
		var k := InputEventKey.new()
		k.physical_keycode = extra[1]
		InputMap.action_add_event(extra[0], k)
		if extra[2] >= 0:
			var jb := InputEventJoypadButton.new()
			jb.button_index = extra[2]
			jb.device = -1
			InputMap.action_add_event(extra[0], jb)


func key_label(dev: String, action: String) -> String:
	return OS.get_keycode_string(keys[dev][action])


## The key or button to show in prompts for an action, for player 1.
func prompt(action: String) -> String:
	for dev in ["kb_left", "kb_right"]:
		if side_of(dev) == 0:
			return key_label(dev, action)
	return action.to_upper()
