extends Screen
## The face-off before a fight: both fighters, VS, the stage, and the rival's words or the
## Reflection's or the boss's line. A few seconds, or skip with Enter.

var _t := 0.0
var _left := 3.4


func build() -> void:
	backdrop(UiKit.RUST)
	var cfg := Game.config
	for side in 2:
		var id: String = cfg["p1"] if side == 0 else cfg["p2"]
		var pv := FighterPreview.new(Vector2i(640, 820))
		pv.facing = 1 if side == 0 else -1
		pv.position = Vector2(120 if side == 0 else 1920 - 760, 120)
		add_child(pv)
		var costume: bool = cfg.get("costume_p%d" % (side + 1), false) or (side == 1 and cfg["p1"] == cfg["p2"] and not cfg.get("mirror_dark", false))
		pv.show_fighter(id, costume)
		var def := Content.fighter(id)
		var name := def.name.to_upper()
		if side == 1 and cfg.get("mirror_dark", false):
			name = "THE REFLECTION"
		var n := UiKit.title(name, 60, UiKit.ACCENT if side == 0 else UiKit.AMBER)
		n.position = Vector2(120 if side == 0 else 1920 - 760, 930)
		n.size = Vector2(640, 80)
		add_child(n)
	var vs := UiKit.title("VS", 200, UiKit.RUST.lightened(0.2))
	vs.position = Vector2(760, 310)
	vs.size = Vector2(400, 260)
	add_child(vs)
	var stage := UiKit.label(Arena.display_name(str(cfg.get("stage", ""))), 30, UiKit.DIM, HORIZONTAL_ALIGNMENT_CENTER, UiKit.body_font(true))
	stage.position = Vector2(560, 580)
	stage.size = Vector2(800, 50)
	add_child(stage)
	var lines: Array = []
	var rung := Game.current_rung() if cfg.get("mode", "") == "arcade" else {}
	match str(rung.get("kind", "")):
		"rival":
			var rl: Array = Content.story.get("fighters", {}).get(cfg["p1"], {}).get("rival_lines", [])
			if rl.size() >= 2:
				lines = ["%s: “%s”" % [Content.fighter(cfg["p1"]).name, rl[0]], "%s: “%s”" % [Content.fighter(cfg["p2"]).name, rl[1]]]
		"mirror":
			lines = [str(Content.story.get("reflection", ""))]
		"boss":
			lines = [str(Content.story.get("boss_intro", ""))]
			Audio.play("screech", -6.0)
	if not lines.is_empty():
		_left = 5.2
	var y := 640.0
	for l in lines:
		var lab := UiKit.label(str(l), 28, UiKit.TEXT, HORIZONTAL_ALIGNMENT_CENTER)
		lab.position = Vector2(400, y)
		UiKit.wrap(lab, Vector2(1120, 44))
		add_child(lab)
		y += 56.0
	if cfg.get("mode", "") == "arcade":
		var r: int = int(Game.arcade.get("rung", 0))
		var total: int = Game.arcade.get("rungs", []).size()
		var prog := UiKit.label("Fight %d of %d" % [r + 1, total], 24, UiKit.DIM, HORIZONTAL_ALIGNMENT_CENTER)
		prog.position = Vector2(660, 70)
		prog.size = Vector2(600, 40)
		add_child(prog)
	elif cfg.get("mode", "") == "survival":
		var wave := Game.survival_waves() + 1
		var boss := cfg["p2"] == "slopbat"
		var prog := UiKit.title("WAVE %d" % wave, 54, UiKit.RUST.lightened(0.2) if boss else UiKit.AMBER)
		prog.position = Vector2(560, 50)
		prog.size = Vector2(800, 70)
		add_child(prog)
		var lvl := UiKit.label(Game.DIFFICULTIES[int(cfg.get("difficulty", 0))], 24, UiKit.DIM, HORIZONTAL_ALIGNMENT_CENTER)
		lvl.position = Vector2(660, 125)
		lvl.size = Vector2(600, 40)
		add_child(lvl)
		if boss:
			var bl := UiKit.label(str(Content.story.get("boss_intro", "")), 28, UiKit.TEXT, HORIZONTAL_ALIGNMENT_CENTER)
			bl.position = Vector2(400, 640)
			UiKit.wrap(bl, Vector2(1120, 44))
			add_child(bl)
			Audio.play("screech", -6.0)
			_left = 4.6
	Audio.play("gong", -6.0, 0.8)
	Audio.play_music("music_select")


func _process(delta: float) -> void:
	_t += delta
	if _t >= _left:
		_start()


func _start() -> void:
	set_process(false)
	set_process_unhandled_input(false)
	Game.goto("match")


func _unhandled_input(event: InputEvent) -> void:
	if event.is_pressed() and not event.is_echo() and _t > 0.4 and (event.is_action("ui_accept") or event.is_action("p1_hp") or event.is_action("p1_lp")):
		_start()
		get_viewport().set_input_as_handled()
