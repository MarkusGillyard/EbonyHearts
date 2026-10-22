class_name Hud
extends CanvasLayer
## The fight's heads-up display: health bars with names and round wins, the run meters, the round
## timer, combo counters, the big announcements (ROUND, FIGHT, FINISH, WINS, FLAWLESS VICTORY,
## SLOPTALITY), subtitles, notices and the pause menu. Training adds frame data, the dummy's mode
## and the input history.

signal pause_choice(choice: String)

const BAR_W := 720.0
const BAR_H := 34.0
const HEALTH := Color(0.62, 0.86, 0.18)
const HEALTH_LOW := Color(0.95, 0.62, 0.15)
const DRAIN := Color(0.72, 0.12, 0.08)
const RUN := Color(0.32, 0.62, 0.95)

var sim: FightSim
var mode := "cpu"
var config := {}
var dummy: TrainingDummy
var input_display := false
var show_frames := false
var show_states := false
var _root: Control
var _bars: Array[Dictionary] = []
var _timer: Label
var _announce: Label
var _announce_sub: Label
var _announce_t := 0.0
var _announce_life := 0.0
var _combo: Array[Label] = []
var _combo_t := [0.0, 0.0]
var _subtitle: Label
var _subtitle_t := 0.0
var _notice: Label
var _notice_t := 0.0
var _info: Label
var _inputs: Array[Label] = []
var _input_log := [[], []]
var _pause: Control
var _pause_buttons: Array[Button] = []
var _prompt: Label
var _shown_health := [1.0, 1.0]


func setup(s: FightSim, m: String, cfg: Dictionary) -> void:
	sim = s
	mode = m
	config = cfg
	layer = 10
	_root = Control.new()
	_root.set_anchors_preset(Control.PRESET_FULL_RECT)
	_root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_root.theme = UiKit.theme()
	add_child(_root)
	for i in 2:
		_bars.append(_make_bar(i))
	if mode == "stranger":
		# He has no health to show, and there is no clock.
		for n in _bars[1]["nodes"]:
			(n as CanvasItem).visible = false
	_timer = UiKit.title("90", 64, UiKit.TEXT)
	_timer.position = Vector2(900, 22)
	_timer.size = Vector2(120, 70)
	_root.add_child(_timer)
	_announce = UiKit.title("", 120, UiKit.TEXT)
	_announce.set_anchors_preset(Control.PRESET_FULL_RECT)
	_announce.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_announce.offset_top = -80
	_announce.pivot_offset = Vector2(960, 460)
	_root.add_child(_announce)
	_announce_sub = UiKit.title("", 52, UiKit.AMBER)
	_announce_sub.set_anchors_preset(Control.PRESET_FULL_RECT)
	_announce_sub.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_announce_sub.offset_top = 70
	_root.add_child(_announce_sub)
	for i in 2:
		var c := UiKit.title("", 40, UiKit.AMBER, HORIZONTAL_ALIGNMENT_LEFT if i == 0 else HORIZONTAL_ALIGNMENT_RIGHT)
		c.position = Vector2(60 if i == 0 else 1920 - 560, 250)
		c.size = Vector2(500, 120)
		_root.add_child(c)
		_combo.append(c)
	_subtitle = UiKit.label("", 34, UiKit.TEXT, HORIZONTAL_ALIGNMENT_CENTER, UiKit.body_font(true))
	_subtitle.position = Vector2(260, 1080 - 190)
	_subtitle.size = Vector2(1400, 60)
	_subtitle.add_theme_color_override("font_outline_color", Color.BLACK)
	_subtitle.add_theme_constant_override("outline_size", 8)
	_root.add_child(_subtitle)
	_prompt = UiKit.label("", 30, UiKit.TEXT, HORIZONTAL_ALIGNMENT_CENTER, UiKit.body_font(true))
	_prompt.position = Vector2(160, 1080 - 120)
	UiKit.wrap(_prompt, Vector2(1600, 90))
	_prompt.add_theme_color_override("font_outline_color", Color.BLACK)
	_prompt.add_theme_constant_override("outline_size", 8)
	_root.add_child(_prompt)
	_notice = UiKit.label("", 26, UiKit.AMBER, HORIZONTAL_ALIGNMENT_CENTER)
	_notice.position = Vector2(460, 150)
	_notice.size = Vector2(1000, 40)
	_notice.add_theme_color_override("font_outline_color", Color.BLACK)
	_notice.add_theme_constant_override("outline_size", 6)
	_root.add_child(_notice)
	_info = UiKit.label("", 20, UiKit.TEXT)
	_info.position = Vector2(60, 330)
	_info.size = Vector2(900, 400)
	_info.add_theme_color_override("font_outline_color", Color.BLACK)
	_info.add_theme_constant_override("outline_size", 5)
	_root.add_child(_info)
	for i in 2:
		var l := UiKit.label("", 22, UiKit.TEXT, HORIZONTAL_ALIGNMENT_LEFT if i == 0 else HORIZONTAL_ALIGNMENT_RIGHT)
		l.position = Vector2(60 if i == 0 else 1920 - 360, 560)
		l.size = Vector2(300, 480)
		l.add_theme_color_override("font_outline_color", Color.BLACK)
		l.add_theme_constant_override("outline_size", 5)
		_root.add_child(l)
		_inputs.append(l)
	_build_pause()


func _make_bar(i: int) -> Dictionary:
	var right := i == 1
	var x := 60.0 if not right else 1920.0 - 60.0 - BAR_W
	var frame := Panel.new()
	var fs := StyleBoxFlat.new()
	fs.bg_color = Color(0.02, 0.02, 0.02, 0.85)
	fs.border_color = Color(0.75, 0.7, 0.58)
	fs.set_border_width_all(3)
	frame.add_theme_stylebox_override("panel", fs)
	frame.position = Vector2(x - 4, 36)
	frame.size = Vector2(BAR_W + 8, BAR_H + 8)
	_root.add_child(frame)
	var drain := ColorRect.new()
	drain.color = DRAIN
	drain.position = Vector2(x, 40)
	drain.size = Vector2(BAR_W, BAR_H)
	_root.add_child(drain)
	var fill := ColorRect.new()
	fill.color = HEALTH
	fill.position = Vector2(x, 40)
	fill.size = Vector2(BAR_W, BAR_H)
	_root.add_child(fill)
	var sheen := ColorRect.new()
	sheen.color = Color(1, 1, 1, 0.12)
	sheen.position = Vector2(x, 40)
	sheen.size = Vector2(BAR_W, BAR_H * 0.4)
	sheen.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_root.add_child(sheen)
	var shown_name := sim.fighters[i].def.name.to_upper()
	if sim.fighters[i].def.id == "daxion":
		shown_name = "STRANGER IN WHITE"
	# Light with a dark outline: readable over the full bar and over the emptied part alike.
	var name := UiKit.label(shown_name, 26, UiKit.TEXT, HORIZONTAL_ALIGNMENT_RIGHT if right else HORIZONTAL_ALIGNMENT_LEFT, UiKit.body_font(true))
	name.position = Vector2(x + 12, 38)
	name.size = Vector2(BAR_W - 24, BAR_H)
	name.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	name.add_theme_color_override("font_outline_color", Color(0.02, 0.02, 0.02))
	name.add_theme_constant_override("outline_size", 7)
	_root.add_child(name)
	var run_bg := ColorRect.new()
	run_bg.color = Color(0.02, 0.02, 0.03, 0.8)
	run_bg.position = Vector2(x + (BAR_W - 300 if right else 0), 84)
	run_bg.size = Vector2(300, 12)
	_root.add_child(run_bg)
	var run := ColorRect.new()
	run.color = RUN
	run.position = run_bg.position + Vector2(2, 2)
	run.size = Vector2(296, 8)
	_root.add_child(run)
	var run_label := UiKit.label("RUN", 16, RUN.lightened(0.3), HORIZONTAL_ALIGNMENT_RIGHT if right else HORIZONTAL_ALIGNMENT_LEFT, UiKit.body_font(true))
	run_label.position = Vector2(x + (BAR_W - 300 - 54 if right else 306), 79)
	run_label.size = Vector2(48, 20)
	_root.add_child(run_label)
	var marks: Array[Control] = []
	for k in 2:
		var m := Panel.new()
		var ms := StyleBoxFlat.new()
		ms.bg_color = Color(0.08, 0.08, 0.07)
		ms.border_color = Color(0.75, 0.7, 0.58)
		ms.set_border_width_all(2)
		ms.set_corner_radius_all(11)
		m.add_theme_stylebox_override("panel", ms)
		m.size = Vector2(22, 22)
		var mx := (x + BAR_W - 26 - k * 30) if not right else (x + 4 + k * 30)
		m.position = Vector2(mx, 104)
		_root.add_child(m)
		marks.append(m)
	var nodes: Array = [frame, drain, fill, sheen, run_bg, run, run_label]
	nodes.append_array(marks)
	return {"x": x, "fill": fill, "drain": drain, "run": run, "marks": marks, "right": right, "drain_v": 1.0, "nodes": nodes}


func _build_pause() -> void:
	_pause = Control.new()
	_pause.set_anchors_preset(Control.PRESET_FULL_RECT)
	_pause.visible = false
	_pause.process_mode = Node.PROCESS_MODE_ALWAYS
	_root.add_child(_pause)
	var dim := ColorRect.new()
	dim.color = Color(0, 0, 0, 0.62)
	dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	_pause.add_child(dim)
	var box := VBoxContainer.new()
	box.position = Vector2(740, 280)
	box.size = Vector2(440, 520)
	box.add_theme_constant_override("separation", 12)
	_pause.add_child(box)
	box.add_child(UiKit.title("PAUSED", 72))
	var items := [["Resume", "resume"], ["Move List", "moves"]]
	if mode in ["versus", "cpu", "training", "watch"]:
		items.append(["Restart Match", "restart"])
		items.append(["Character Select", "select"])
	if mode == "arcade":
		items.append(["Quit Arcade", "menu"])
	else:
		items.append(["Main Menu", "menu"])
	for it in items:
		var choice: String = it[1]
		var b := UiKit.button(it[0], func(): pause_choice.emit(choice), 30)
		b.process_mode = Node.PROCESS_MODE_ALWAYS
		box.add_child(b)
		_pause_buttons.append(b)
	UiKit.chain_focus(_pause_buttons)


func show_pause(on: bool) -> void:
	_pause.visible = on
	if on and not _pause_buttons.is_empty():
		_pause_buttons[0].grab_focus.call_deferred()


func _unhandled_input(event: InputEvent) -> void:
	if _pause.visible and (event.is_action_pressed("pause") or event.is_action_pressed("ui_cancel")):
		pause_choice.emit("resume")
		get_viewport().set_input_as_handled()


func announce(text: String, sub := "", life := 1.6, color := UiKit.TEXT, size := 120) -> void:
	_announce.text = text
	_announce.add_theme_color_override("font_color", color)
	_announce.add_theme_font_size_override("font_size", size)
	_announce_sub.text = sub
	_announce_t = 0.0
	_announce_life = life


func subtitle(text: String, life := 3.0) -> void:
	_subtitle.text = text
	_subtitle_t = life


func notice(text: String, life := 3.0) -> void:
	_notice.text = text
	_notice_t = life


func prompt(text: String) -> void:
	_prompt.text = text


func on_event(e: Dictionary) -> void:
	if mode == "stranger" and e["type"] in ["round_start", "fight", "win_pose", "round_end", "timeout"]:
		return
	match e["type"]:
		"line":
			if e.get("hint", false):
				prompt(str(e["text"]))
			else:
				subtitle("“%s”" % str(e["text"]), 3.4)
		"round_start":
			var r: int = e["round"]
			var last: bool = sim.wins[0] == sim.rounds_to_win - 1 and sim.wins[1] == sim.rounds_to_win - 1
			announce("FINAL ROUND" if last and r > 1 else "ROUND %d" % r, "", 1.5, UiKit.TEXT, 120)
		"fight":
			announce("FIGHT!", "", 0.9, UiKit.AMBER, 150)
		"finish_prompt":
			announce("FINISH %s" % str(e.get("pronoun", "him")).to_upper(), "", 2.2, UiKit.RUST.lightened(0.15), 130)
		"finisher":
			announce("", "")
		"finisher_end":
			var fid := str(e["move"])
			var fin := "SLOPTALITY" if not fid.begins_with("stage_") else "STAGE SLOPTALITY"
			announce(fin, "", 3.0, UiKit.ACCENT, 140)
		"timeout":
			announce("TIME", "", 1.2, UiKit.AMBER)
		"round_end":
			if e["how"] == "double_ko":
				announce("DOUBLE KO", "", 2.0, UiKit.AMBER)
			elif e["winner"] < 0:
				announce("DRAW", "", 2.0, UiKit.AMBER)
		"win_pose":
			var w := sim.fighters[e["who"]]
			var name := w.def.name.to_upper()
			if config.get("mirror_dark", false) and e["who"] == 1:
				name = "THE REFLECTION"
			if _announce.text != "SLOPTALITY" and _announce.text != "STAGE SLOPTALITY":
				announce("%s WINS" % name, "FLAWLESS VICTORY" if e.get("flawless", false) else "", 2.6, UiKit.TEXT, 104)
			else:
				_announce_sub.text = "%s WINS" % name
		"hit":
			var by: int = e["by"]
			var n: int = e["combo"]
			if n >= 2:
				var v := sim.fighters[e["who"]]
				_combo[by].text = "%d HIT COMBO\n%d%%" % [n, int(round(100.0 * v.combo_damage / v.def.health))]
				_combo_t[by] = 1.6
		"finisher_range":
			notice("Wrong distance: %s range" % str(e.get("want", "")))


func on_tick(pads: Array) -> void:
	if input_display:
		for i in 2:
			var p: Vector3i = pads[i]
			var f := sim.fighters[i]
			var d := InputBuffer.rel(InputBuffer.screen_from(p.x & 1, p.x & 2, p.x & 4, p.x & 8), f.facing)
			var arrows := {1: "↙", 2: "↓", 3: "↘", 4: "←", 5: "·", 6: "→", 7: "↖", 8: "↑", 9: "↗"}
			var btn := ""
			for pair in [[InputBuffer.HP, "HP"], [InputBuffer.LP, "LP"], [InputBuffer.HK, "HK"], [InputBuffer.LK, "LK"], [InputBuffer.BL, "BL"], [InputBuffer.RN, "RN"]]:
				if p.z & pair[0]:
					btn += " " + pair[1]
			var line := "%s%s" % [arrows[d], btn]
			var log: Array = _input_log[i]
			if log.is_empty() or log[0][0] != line:
				log.push_front([line, 1])
			else:
				log[0][1] += 1
			while log.size() > 18:
				log.pop_back()


func refresh(delta: float) -> void:
	for i in 2:
		var f := sim.fighters[i]
		var b := _bars[i]
		var h := clampf(float(f.health) / f.def.health, 0.0, 1.0)
		var fill: ColorRect = b["fill"]
		var drain: ColorRect = b["drain"]
		var w := BAR_W * h
		fill.size.x = w
		fill.color = HEALTH if h > 0.25 else HEALTH_LOW
		if b["right"]:
			fill.position.x = float(b["x"]) + BAR_W - w
		# The red drain follows the health down after a moment, so damage reads.
		if f.combo_hits == 0 or f.state in [Fighter.S.DOWN, Fighter.S.KO]:
			b["drain_v"] = move_toward(b["drain_v"], h, delta * 0.6)
		b["drain_v"] = maxf(b["drain_v"], h)
		var dw := BAR_W * float(b["drain_v"])
		drain.size.x = dw
		if b["right"]:
			drain.position.x = float(b["x"]) + BAR_W - dw
		var run: ColorRect = b["run"]
		var rk := f.run_meter / Fighter.RUN_MAX
		run.size.x = 296.0 * rk
		if b["right"]:
			run.position.x = float(b["x"]) + BAR_W - 300 + 2 + 296.0 * (1.0 - rk)
		var marks: Array = b["marks"]
		for k in marks.size():
			var ms: StyleBoxFlat = (marks[k] as Panel).get_theme_stylebox("panel")
			ms.bg_color = UiKit.ACCENT if sim.wins[i] > k else Color(0.08, 0.08, 0.07)
		_combo_t[i] = maxf(0.0, _combo_t[i] - delta)
		_combo[i].modulate.a = clampf(_combo_t[i] * 2.0, 0.0, 1.0)
	_timer.text = "%d" % sim.timer_seconds() if not sim.training else ("" if mode == "stranger" else "∞")
	_timer.add_theme_color_override("font_color", UiKit.TEXT if sim.timer_seconds() > 10 else UiKit.RUST.lightened(0.2))
	if _announce_life > 0.0:
		_announce_t += delta
		var k := _announce_t / _announce_life
		var pop := 1.0 + 0.25 * maxf(0.0, 1.0 - _announce_t * 6.0)
		_announce.scale = Vector2(pop, pop)
		_announce.modulate.a = clampf((1.0 - k) * 4.0, 0.0, 1.0)
		_announce_sub.modulate.a = _announce.modulate.a
		if k >= 1.0:
			_announce_life = 0.0
			_announce.text = ""
			_announce_sub.text = ""
	_subtitle_t = maxf(0.0, _subtitle_t - delta)
	_subtitle.modulate.a = clampf(_subtitle_t * 2.0, 0.0, 1.0)
	_notice_t = maxf(0.0, _notice_t - delta)
	_notice.modulate.a = clampf(_notice_t * 2.0, 0.0, 1.0)
	var info := ""
	if dummy:
		info += "Dummy: %s   (Tab to change, F4 to reset)\n" % dummy.mode_name()
	if show_frames or show_states:
		for f in sim.fighters:
			var m := f.move
			info += "P%d %s %s f%d  hp %d  run %d  stun %d  combo %d\n" % [f.index + 1, f.state_name(), m.id if m else "", f.mf, f.health, int(f.run_meter), f.stun, f.combo_hits]
			if show_frames and m:
				info += "    %s  startup %d  active %d  recovery %d  on block %+d  dmg %d\n" % [m.name, m.startup, m.active, m.recovery, m.block_advantage(), m.damage]
	_info.text = info
	for i in 2:
		if input_display:
			var lines := []
			for entry in _input_log[i]:
				lines.append("%s  %d" % [entry[0], mini(entry[1], 99)])
			_inputs[i].text = "\n".join(lines)
		else:
			_inputs[i].text = ""
