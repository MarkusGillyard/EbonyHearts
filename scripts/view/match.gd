class_name Match
extends Node3D
## A fight on screen. Steps FightSim once per physics tick (60 Hz) from the players' devices or
## CPU brains, then draws: fighters interpolated between ticks, camera, effects and HUD. Losing
## window focus or a controller pauses the match and clears every held input.
## Debug keys: F1 boxes, F2 frame data, F3 states, F4 reset (training), F5 freeze, F6 step,
## F7 slow motion, F8 input display.

signal finished(result: Dictionary)

var sim: FightSim
var views: Array[FighterView] = []
var camera: FightCamera
var arena: Arena
var debug_draw: DebugDraw
var hud: Hud
var vfx: Vfx
var director: FinisherDirector
var pads: Array[PadReader] = [PadReader.new(0), PadReader.new(1)]
var brains: Array = [null, null]
var dummy: TrainingDummy
var stranger: StrangerRules
var tutorial: Node
var mode := "cpu"
var config := {}
var paused := false
var debug_pause := false
var slow := false
var _step_once := false
var _slow_count := 0
var _ko_slow := 0
var _ended := false
var _pending_pads := [Vector3i.ZERO, Vector3i.ZERO]
var _stepped := 0
var ticks_run := 0
var sim_usec := 0
## With --trace=path every tick's input, states and events are written as JSON lines.
var _trace: FileAccess
var _trace_live := false
## With --bench=seconds frame times are collected, printed and the game quits.
var _bench_secs := 0.0
var _bench_frames: PackedFloat32Array = PackedFloat32Array()
var _bench_t := 0.0
var _bench_anim_usec := 0


func start(cfg: Dictionary) -> void:
	config = cfg.duplicate()
	mode = str(config.get("mode", "cpu"))
	Settings.set_solo(not Game.two_players())
	sim = FightSim.new()
	sim.training = mode in ["training", "tutorial", "stranger"]
	sim.rounds_to_win = int(config.get("rounds_to_win", 2))
	sim.finishers_enabled = mode != "stranger"
	var stage := str(config.get("stage", "rendering_yards"))
	if Content.fighter(config["p1"]).move("stage_vat") and stage == "rendering_yards":
		sim.stage_finisher = "stage_vat"
	sim.intro = mode not in ["training", "tutorial", "stranger"]
	sim.setup(Content.fighter(config["p1"]), Content.fighter(config["p2"]))
	if mode == "stranger":
		# No announcer and no rounds: he is simply there.
		sim.phase = FightSim.Phase.FIGHT
		for f in sim.fighters:
			f.set_state(Fighter.S.IDLE)
	arena = Arena.create(stage)
	add_child(arena)
	arena.build()
	for f in sim.fighters:
		var v := FighterView.new()
		v.name = "Fighter%d" % (f.index + 1)
		add_child(v)
		v.setup(f, Content.common_poses)
		v.smoothing = Settings.get_value("motion_smoothing")
		views.append(v)
	if config.get("mirror_dark", false):
		views[1].set_tint(Color(0.06, 0.1, 0.05), 0.72, 0.6)
	elif config["p1"] == config["p2"]:
		# Mirror matches: the second player wears the alternate colours.
		views[1].set_costume(0.42, 1.05)
	for i in 2:
		var cos_key := "costume_p%d" % (i + 1)
		if config.get(cos_key, false) and not (i == 1 and config["p1"] == config["p2"]):
			views[i].set_costume(0.42, 1.05)
	camera = FightCamera.new()
	camera.sim = sim
	camera.shake_enabled = Settings.get_value("screen_shake")
	add_child(camera)
	camera.current = true
	camera.snap()
	debug_draw = DebugDraw.new()
	debug_draw.sim = sim
	debug_draw.visible = false
	add_child(debug_draw)
	vfx = Vfx.new()
	add_child(vfx)
	vfx.setup(sim, views, camera)
	director = FinisherDirector.new()
	add_child(director)
	director.setup(self)
	hud = Hud.new()
	add_child(hud)
	hud.setup(sim, mode, config)
	hud.input_display = Settings.get_value("input_display") or mode == "training"
	var diff := int(config.get("difficulty", Settings.get_value("difficulty")))
	for i in 2:
		if Game.is_cpu(i) and mode not in ["training", "tutorial", "stranger"]:
			if sim.fighters[i].def.boss:
				brains[i] = BossBrain.new(sim, i, diff)
			else:
				brains[i] = CpuBrain.new(sim, i, diff)
	if mode == "training":
		dummy = TrainingDummy.new(sim, 1)
		hud.dummy = dummy
		sim.refill_health = true
		sim.infinite_run = false
	if mode == "stranger":
		stranger = StrangerRules.new(sim, self)
		sim.hooks = stranger
	if mode == "tutorial":
		tutorial = load("res://scripts/ui/tutorial.gd").new()
		add_child(tutorial)
		tutorial.setup(self)
	Audio.play_music(arena.music)
	get_window().focus_exited.connect(_on_focus_lost)
	Input.joy_connection_changed.connect(_on_joy_changed)
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--trace="):
			_trace = FileAccess.open(arg.trim_prefix("--trace="), FileAccess.READ_WRITE if FileAccess.file_exists(arg.trim_prefix("--trace=")) else FileAccess.WRITE)
			_trace.seek_end()
		elif arg == "--trace-live":
			# Flush every tick, for tools that read the trace while the game runs.
			_trace_live = true
		elif arg.begins_with("--bench="):
			_bench_secs = float(arg.trim_prefix("--bench="))
	if _trace:
		# One header per match, so a long trace (a whole arcade run) says who fought where.
		_trace.store_line(JSON.stringify({"match": {"p1": config["p1"], "p2": config["p2"], "mode": mode, "stage": stage,
			"difficulty": config.get("difficulty", 1)}}))


func _exit_tree() -> void:
	if Input.joy_connection_changed.is_connected(_on_joy_changed):
		Input.joy_connection_changed.disconnect(_on_joy_changed)
	if sim:
		sim.dispose()


func _on_focus_lost() -> void:
	for p in pads:
		p.reset()
	if not paused and not _ended and mode != "watch" and _bench_secs <= 0.0:
		toggle_pause()


func _on_joy_changed(device: int, connected: bool) -> void:
	for p in pads:
		p.reset()
	hud.notice("Controller %d %s" % [device + 1, "connected" if connected else "disconnected"])
	if not connected and not paused and not _ended:
		toggle_pause()


func _unhandled_input(event: InputEvent) -> void:
	if paused:
		return
	for p in pads:
		p.feed(event)
	if event.is_action_pressed("pause"):
		if sim.phase == FightSim.Phase.FINISHER:
			# Skipping a finisher goes straight to the result; it never traps the player.
			sim.skip_finisher()
		else:
			toggle_pause()
		get_viewport().set_input_as_handled()
	elif not OS.is_debug_build() and not mode in ["training", "tutorial"] and not "--debug-keys" in OS.get_cmdline_user_args():
		return
	elif event.is_action_pressed("debug_boxes"):
		debug_draw.visible = not debug_draw.visible
	elif event.is_action_pressed("debug_frames"):
		hud.show_frames = not hud.show_frames
	elif event.is_action_pressed("debug_states"):
		hud.show_states = not hud.show_states
	elif event.is_action_pressed("debug_pause"):
		debug_pause = not debug_pause
	elif event.is_action_pressed("debug_step"):
		_step_once = true
	elif event.is_action_pressed("debug_slow"):
		slow = not slow
	elif event.is_action_pressed("input_display"):
		hud.input_display = not hud.input_display
	elif event.is_action_pressed("training_reset") and mode == "training":
		reset_positions()
	elif event.is_action_pressed("training_dummy") and dummy:
		dummy.next_mode()


func toggle_pause() -> void:
	if _ended:
		return
	paused = not paused
	for p in pads:
		p.reset()
	hud.show_pause(paused)
	Audio.pause_music(paused)


func reset_positions() -> void:
	sim.restart_round()
	sim.phase = FightSim.Phase.FIGHT
	for f in sim.fighters:
		f.set_state(Fighter.S.IDLE)
	for v in views:
		v.sync_from_sim(true)
	camera.snap()


func _physics_process(_delta: float) -> void:
	if sim == null or paused:
		return
	var p := [pads[0].poll(), pads[1].poll()]
	if debug_pause and not _step_once:
		_pending_pads = [_merge(_pending_pads[0], p[0]), _merge(_pending_pads[1], p[1])]
		return
	if debug_pause:
		p = [_merge(p[0], _pending_pads[0]), _merge(p[1], _pending_pads[1])]
		_pending_pads = [Vector3i.ZERO, Vector3i.ZERO]
		_stepped += 1
	_step_once = false
	if slow or _ko_slow > 0:
		_slow_count += 1
		if _ko_slow > 0:
			_ko_slow -= 1
		if _slow_count % (4 if slow else 3) != 0:
			return
	if dummy:
		p[1] = dummy.think()
	if stranger:
		p[1] = stranger.think()
	for i in 2:
		if brains[i]:
			p[i] = brains[i].think()
	if tutorial:
		p = tutorial.filter_pads(p)
	var t0 := Time.get_ticks_usec()
	sim.step(p)
	sim_usec += Time.get_ticks_usec() - t0
	ticks_run += 1
	for v in views:
		v.sync_from_sim()
	for e in sim.events:
		_on_event(e)
	hud.on_tick(p)
	if _trace:
		_write_trace(p)


func _write_trace(p: Array) -> void:
	var fs := []
	for f in sim.fighters:
		fs.append([f.state_name(), f.move.id if f.move else "", f.mf, f.health, snappedf(f.pos.x, 0.001), snappedf(f.pos.y, 0.001), f.input.dir()])
	var ev := []
	for e in sim.events:
		var short := {"type": e["type"]}
		for k in ["who", "by", "move", "damage", "combo", "winner", "how"]:
			if e.has(k):
				short[k] = e[k]
		ev.append(short)
	var pr := []
	for q in sim.projectiles:
		pr.append([q.owner, snappedf(q.pos.x, 0.001), snappedf(q.pos.y, 0.001), snappedf(q.vel.x, 0.01), q.level == MoveDef.Level.LOW])
	_trace.store_line(JSON.stringify({"t": sim.tick_count, "phase": sim.phase, "pads": [[p[0].x, p[0].z], [p[1].x, p[1].z]], "f": fs, "ev": ev,
		"facing": [sim.fighters[0].facing, sim.fighters[1].facing], "stun": [sim.fighters[0].stun_kind, sim.fighters[1].stun_kind], "proj": pr}))
	if sim.tick_count % 60 == 0 or _trace_live:
		_trace.flush()


static func _merge(a: Vector3i, b: Vector3i) -> Vector3i:
	return Vector3i(a.x | b.x, a.y | b.y, a.z | b.z)


func _process(delta: float) -> void:
	if sim == null:
		return
	var frac := Engine.get_physics_interpolation_fraction()
	if paused or debug_pause:
		frac = 1.0
	var d := 0.0 if paused or debug_pause else delta
	if debug_pause and not paused:
		d = _stepped / 60.0
		_stepped = 0
	var t_anim := Time.get_ticks_usec()
	for v in views:
		v.update_visual(frac, d)
	director.step(d)
	if _bench_secs > 0.0:
		_bench_step(delta, Time.get_ticks_usec() - t_anim)
	camera.step(delta if not paused else 0.0)
	vfx.step(d)
	arena.step(d)
	if debug_draw.visible:
		debug_draw.redraw()
	hud.refresh(delta)


func _sound(e: Dictionary) -> void:
	match e["type"]:
		"move":
			var m := sim.fighters[e["who"]].def.move(e["move"])
			if m:
				var sw := str(m.sfx.get("swing", ""))
				match sw:
					"light":
						Audio.play("whoosh_light", -7.0, randf_range(0.94, 1.08))
					"medium":
						Audio.play("whoosh_medium", -6.0, randf_range(0.94, 1.06))
					"heavy":
						Audio.play("whoosh_heavy", -5.0, randf_range(0.94, 1.04))
					"":
						pass
					_:
						Audio.play(sw, -3.0)
		"hit":
			var dmg: int = e["damage"]
			if e.get("throw", false):
				Audio.play("slam", 0.0)
			else:
				Audio.play("hit_light" if dmg < 55 else ("hit_medium" if dmg < 95 else "hit_heavy"), 0.0, randf_range(0.95, 1.05))
			if e.get("proj", false):
				Audio.play("burst", -4.0)
		"block":
			Audio.play("block", -2.0, randf_range(0.95, 1.05))
		"armor":
			Audio.play("armor", -2.0)
		"throw":
			Audio.play("grab", -2.0)
		"throw_break":
			Audio.play("throw_break")
		"land":
			Audio.play("land" if not e.get("soft", false) else "step", -4.0 if not e.get("soft", false) else -12.0)
		"projectile":
			Audio.play("cast_" + str(e.get("kind", "orb")), -4.0)
		"clash":
			Audio.play("burst", -2.0)
		"jump":
			Audio.play("step", -10.0, 1.1)
		"chain":
			pass
		"ko":
			Audio.play("ko_hit")
		"round_start":
			Audio.play("gong", -4.0)
		"fight":
			Audio.play("fight", -2.0)
		"finish_prompt":
			Audio.play("finish", -1.0)
		"round_end":
			if e.get("how", "") == "timeout":
				Audio.play("gong", -6.0, 0.8)


func _bench_step(delta: float, anim_usec: int) -> void:
	_bench_t += delta
	if _bench_t < 2.0:
		return
	_bench_frames.append(delta * 1000.0)
	_bench_anim_usec += anim_usec
	if _bench_t < 2.0 + _bench_secs:
		return
	var sorted := _bench_frames.duplicate()
	sorted.sort()
	var n := sorted.size()
	var total := 0.0
	for x in sorted:
		total += x
	var avg_ms := total / n
	print("BENCH frames=%d avg_fps=%.1f avg_ms=%.2f p50=%.2f p95=%.2f p99=%.2f p999=%.2f max=%.2f sim_us_per_tick=%.0f anim_us_per_frame=%.0f window=%s renderer=%s" % [
		n, 1000.0 / avg_ms, avg_ms, sorted[n / 2], sorted[int(n * 0.95)], sorted[int(n * 0.99)], sorted[mini(n - 1, int(n * 0.999))], sorted[n - 1],
		float(sim_usec) / maxi(1, ticks_run), float(_bench_anim_usec) / n, get_window().size, RenderingServer.get_current_rendering_method()])
	_bench_secs = 0.0
	Game.quit()


func _on_event(e: Dictionary) -> void:
	vfx.on_event(e)
	hud.on_event(e)
	director.on_event(e)
	if stranger:
		stranger.on_event(e)
	if tutorial:
		tutorial.on_event(e)
	_sound(e)
	match e["type"]:
		"hit":
			var v := views[e["who"]]
			var att := sim.fighters[e["by"]]
			var strength := clampf(float(e["damage"]) / 100.0, 0.15, 1.3)
			v.impact(signf(sim.fighters[e["who"]].pos.x - att.pos.x), strength, int(e["level"]), false)
			if e["damage"] >= 95:
				camera.add_shake(0.02 + strength * 0.02)
		"block":
			views[e["who"]].impact(signf(sim.fighters[e["who"]].pos.x - sim.fighters[e["by"]].pos.x), 0.35, MoveDef.Level.MID, true)
		"ko":
			_ko_slow = 50 if not e.get("finish", false) else 30
			camera.add_shake(0.05)
		"round_start":
			camera.release_zoom()
			for v in views:
				v.sync_from_sim(true)
			camera.snap()
		"match_end":
			_ended = true
			var w: int = e["winner"]
			finished.emit({"winner": w, "wins": sim.wins.duplicate(), "p1": sim.fighters[0].def.id, "p2": sim.fighters[1].def.id,
				"finisher": e.get("finisher", ""), "flawless": e.get("flawless", false), "mode": mode,
				"stranger_line": e.get("stranger_line", "")})
