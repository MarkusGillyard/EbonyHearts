extends Screen
## The main menu. The panel on the right describes whichever item has focus.

const ITEMS := [
	["Arcade", "arcade", "Climb a tower of the Slopbat's chosen to the roost itself. Your choice of tower sets the difficulty."],
	["Survival", "survival", "An endless ladder: one round a fight, your health carried on with a little given back after each win, and the Slopbat waiting every fifth wave. How far can you climb?"],
	["Versus", "versus", "Two players on one machine. Any fighter, any stage, mirror matches welcome."],
	["Training", "training", "Practise against a dummy that stands, blocks, jumps or fights back. Move lists, frame data and hitboxes."],
	["Tutorial", "tutorial", "Learn to move, block, strike, combo, throw and finish, one step at a time."],
	["Extras", "extras", "Endings you have earned, your records, and anything else you have found."],
	["Settings", "settings", "Video, sound, gore, effects and controls."],
	["Quit", "quit", "Leave the Sump."],
]

var _desc: Label


func build() -> void:
	backdrop()
	var t := UiKit.title("MORTAL SLOPBAT", 96, UiKit.ACCENT)
	t.position = Vector2(120, 90)
	t.size = Vector2(1100, 130)
	t.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT
	add_child(t)
	var items := []
	for it in ITEMS:
		var id: String = it[1]
		items.append([it[0], func(): _choose(id)])
	var buttons := menu(items, Vector2(140, 300), 460, 32)
	for i in buttons.size():
		var text: String = ITEMS[i][2]
		buttons[i].focus_entered.connect(func(): _desc.text = text)
	_focus_first = buttons[Game.menu_choice]
	var panel := PanelContainer.new()
	panel.position = Vector2(760, 330)
	panel.size = Vector2(900, 220)
	add_child(panel)
	_desc = UiKit.label(ITEMS[Game.menu_choice][2], 30, UiKit.TEXT)
	_desc.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_desc.custom_minimum_size = Vector2(860, 180)
	panel.add_child(_desc)
	if not Progress.data.get("tutorial_done", false):
		var tip := UiKit.label("New to Mortal Slopbat? The tutorial takes about five minutes.", 24, UiKit.AMBER)
		tip.position = Vector2(780, 580)
		tip.size = Vector2(900, 40)
		add_child(tip)
	hint("Arrows or W/S to choose   ·   Enter or J to select")
	Audio.play_music("music_menu")


func _choose(id: String) -> void:
	for i in ITEMS.size():
		if ITEMS[i][1] == id:
			Game.menu_choice = i
	match id:
		"arcade":
			Game.config["mode"] = "arcade"
			Game.goto("select", {"mode": "arcade"})
		"survival":
			Game.config["mode"] = "survival"
			Game.goto("select", {"mode": "survival"})
		"versus":
			Game.config["mode"] = "versus"
			Game.goto("devices", {})
		"training":
			Game.config["mode"] = "training"
			Game.goto("select", {"mode": "training"})
		"tutorial":
			Game.config = Game.config.merged({"mode": "tutorial", "p1": "ashka", "p2": "gorm", "stage": "rendering_yards", "rounds_to_win": 1, "mirror_dark": false}, true)
			Game.goto("match")
		"extras":
			Game.goto("extras", {})
		"settings":
			Game.goto("settings", {"back": "menu"})
		"quit":
			Game.quit()
