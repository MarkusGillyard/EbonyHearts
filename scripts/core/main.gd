extends Node
## Root scene: owns whichever screen is showing and swaps it when Game asks. Development flags
## (after `--`): --match=p1,p2,mode[,stage[,difficulty]] jumps straight into a fight and
## --screen=name opens a screen directly.

const SCREENS := {
	"title": "res://scripts/ui/title_screen.gd",
	"menu": "res://scripts/ui/main_menu.gd",
	"select": "res://scripts/ui/select_screen.gd",
	"stage": "res://scripts/ui/stage_screen.gd",
	"devices": "res://scripts/ui/devices_screen.gd",
	"tower": "res://scripts/ui/tower_screen.gd",
	"versus": "res://scripts/ui/versus_screen.gd",
	"results": "res://scripts/ui/results_screen.gd",
	"ending": "res://scripts/ui/ending_screen.gd",
	"moves": "res://scripts/ui/move_list_screen.gd",
	"settings": "res://scripts/ui/settings_screen.gd",
	"extras": "res://scripts/ui/extras_screen.gd",
	"stranger_offer": "res://scripts/ui/stranger_offer_screen.gd",
}

var current: Node
var match_node: Match
var _overlay: Control


func _ready() -> void:
	Game.screen_requested.connect(_show)
	var start := "title"
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--window-title="):
			# Playtest tools find the window by a title no other instance shares.
			get_window().title = a.trim_prefix("--window-title=")
		if a.begins_with("--screen="):
			start = a.trim_prefix("--screen=")
		elif a.begins_with("--match="):
			var parts := a.trim_prefix("--match=").split(",")
			Game.config["p1"] = parts[0]
			Game.config["p2"] = parts[1] if parts.size() > 1 else parts[0]
			Game.config["mode"] = parts[2] if parts.size() > 2 else "training"
			if parts.size() > 3:
				Game.config["stage"] = parts[3]
			if parts.size() > 4:
				Game.config["difficulty"] = int(parts[4])
			if Game.config["mode"] == "stranger":
				Game.start_stranger(false)
				Game.config["p1"] = parts[0]
			start = "match"
	_show.call_deferred(start, {})


func _show(screen: String, data: Dictionary) -> void:
	close_overlay()
	if screen == "moves" and match_node:
		# Over a paused match, on a layer above the HUD (and its pause menu) so nothing covers it.
		var layer := CanvasLayer.new()
		layer.layer = 20
		layer.process_mode = Node.PROCESS_MODE_ALWAYS
		add_child(layer)
		_overlay = _make_screen(screen, data)
		layer.add_child(_overlay)
		return
	if current:
		current.queue_free()
		current = null
		match_node = null
	if screen == "match":
		_start_match()
		return
	if screen in ["menu", "title"]:
		Audio.play_music("music_menu")
	current = _make_screen(screen, data)
	add_child(current)


func _make_screen(screen: String, data: Dictionary) -> Control:
	var path: String = SCREENS.get(screen, SCREENS["menu"])
	var c: Control = load(path).new()
	c.set_anchors_preset(Control.PRESET_FULL_RECT)
	c.theme = UiKit.theme()
	if c.has_method("configure"):
		c.configure(data)
	return c


func _start_match() -> void:
	match_node = Match.new()
	current = match_node
	add_child(match_node)
	match_node.start(Game.config)
	match_node.finished.connect(_on_match_finished)
	match_node.hud.pause_choice.connect(_on_pause_choice)


func _on_match_finished(result: Dictionary) -> void:
	await get_tree().create_timer(1.2).timeout
	if current == match_node:
		Game.goto("results", result)


func _on_pause_choice(choice: String) -> void:
	match choice:
		"resume":
			match_node.toggle_pause()
		"moves":
			Game.goto("moves", {"fighter": Game.config["p1"], "overlay": true})
		"restart":
			Game.goto("match")
		"select":
			Game.goto("select", {"mode": Game.config["mode"]})
		"menu":
			Game.arcade = {}
			Game.goto("menu")


func close_overlay() -> void:
	if _overlay:
		_overlay.get_parent().queue_free()
		_overlay = null
