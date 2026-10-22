extends Node3D
## Authoring tool: renders poses, or frames of a move, into one contact-sheet PNG.
##   godot --path . res://tools/preview/pose_sheet.tscn -- --fighter=ashka --poses=stance,crouch --out=/tmp/sheets
##   godot --path . res://tools/preview/pose_sheet.tscn -- --fighter=ashka --move=hk --out=/tmp/sheets
##   godot --path . res://tools/preview/pose_sheet.tscn -- --fighter=ashka --cmds="HP;HP;LP;B+HK" --every=3 --out=/tmp/sheets
## Options: --angles=90,30 (camera yaw around the subject; 90 is the fight camera's side view),
## --cell=420, --frames=1,6,10, --opponent=id, --gap=1.0, --boxes (draw collision boxes).

var fighter_id := "ashka"
var opponent_id := ""
var poses: PackedStringArray = PackedStringArray()
var move_id := ""
var frames: Array[int] = []
var angles: Array[float] = [90.0, 35.0]
var cell := 420
var out_dir := "/tmp/slopbat_sheets"
var gap := 1.0
var cmds: PackedStringArray = PackedStringArray()
var every := 3
var focus := 1.0
var dist := 3.4
var boxes := false
var cam: Camera3D
var label: Label
var debug: DebugDraw


func _ready() -> void:
	for a in OS.get_cmdline_user_args():
		if a == "--boxes":
			boxes = true
		var kv := a.trim_prefix("--").split("=", true, 1)
		if kv.size() < 2:
			continue
		match kv[0]:
			"fighter":
				fighter_id = kv[1]
			"opponent":
				opponent_id = kv[1]
			"poses":
				poses = kv[1].split(",")
			"move":
				move_id = kv[1]
			"frames":
				for x in kv[1].split(","):
					frames.append(int(x))
			"angles":
				angles.clear()
				for x in kv[1].split(","):
					angles.append(float(x))
			"cell":
				cell = int(kv[1])
			"out":
				out_dir = kv[1]
			"gap":
				gap = float(kv[1])
			"focus":
				focus = float(kv[1])
			"dist":
				dist = float(kv[1])
			"cmds":
				cmds = kv[1].split(";")
			"every":
				every = int(kv[1])
	DirAccess.make_dir_recursive_absolute(out_dir)
	get_window().size = Vector2i(cell, cell)
	_run.call_deferred()


func _run() -> void:
	var arena := Arena.create("rendering_yards")
	add_child(arena)
	arena.build()
	cam = Camera3D.new()
	cam.fov = 34.0
	add_child(cam)
	cam.current = true
	var layer := CanvasLayer.new()
	add_child(layer)
	label = Label.new()
	label.position = Vector2(8, 6)
	label.add_theme_font_size_override("font_size", 15)
	label.add_theme_color_override("font_outline_color", Color.BLACK)
	label.add_theme_constant_override("outline_size", 4)
	layer.add_child(label)
	var def := Content.fighter(fighter_id)
	if def == null:
		push_error("unknown fighter " + fighter_id)
		get_tree().quit(1)
		return
	var opp := Content.fighter(opponent_id) if opponent_id != "" else def
	var sim := FightSim.new()
	sim.training = true
	sim.intro = false
	sim.setup(def, opp)
	sim.phase = FightSim.Phase.FIGHT
	for f in sim.fighters:
		f.set_state(Fighter.S.IDLE)
	var shots: Array[Image] = []
	if not cmds.is_empty():
		move_id = "cmds"
		shots = await _cmd_strip(sim)
	elif move_id != "":
		shots = await _move_strip(sim, def)
	else:
		shots = await _pose_grid(sim)
	_save_sheet(shots, "%s_%s" % [fighter_id, move_id if move_id != "" else "poses"])
	sim.dispose()
	get_tree().quit(0)


func _views(sim: FightSim) -> Array[FighterView]:
	var views: Array[FighterView] = []
	for f in sim.fighters:
		var v := FighterView.new()
		add_child(v)
		v.setup(f, Content.common_poses)
		views.append(v)
	if boxes:
		debug = DebugDraw.new()
		debug.sim = sim
		add_child(debug)
	return views


func _pose_grid(sim: FightSim) -> Array[Image]:
	var f := sim.fighters[0]
	f.pos = Vector2.ZERO
	f.facing = 1
	sim.fighters[1].pos = Vector2(40, 0)
	var view := FighterView.new()
	add_child(view)
	view.setup(f, Content.common_poses)
	var out: Array[Image] = []
	for p in poses:
		view.preview(p)
		view.reset_cloth()
		for i in 60:
			for c in view.cloths:
				c.step(1.0 / 60.0)
		for ang in angles:
			_aim(Vector3(0, focus, 0), ang, dist)
			label.text = "%s  %s  %d°" % [fighter_id, p, int(ang)]
			out.append(await _grab())
	if not view.lib.missing.is_empty():
		print("missing poses: ", view.lib.missing)
	return out


func _step(sim: FightSim, views: Array[FighterView], pads := [Vector3i.ZERO, Vector3i.ZERO]) -> void:
	sim.step(pads)
	for v in views:
		v.sync_from_sim()
		v.update_visual(1.0, 1.0 / 60.0)


func _move_strip(sim: FightSim, def: FighterDef) -> Array[Image]:
	var m := def.move(move_id)
	if m == null:
		push_error("unknown move " + move_id)
		return []
	var a := sim.fighters[0]
	var b := sim.fighters[1]
	a.pos = Vector2(-gap * 0.5, 0)
	b.pos = Vector2(gap * 0.5, 0)
	var views := _views(sim)
	for i in 30:
		_step(sim, views)
	if frames.is_empty():
		frames = [1, maxi(2, m.startup - 3), m.startup, m.last_active(), m.last_active() + m.recovery / 2, m.total]
	a.start_move(m)
	var out: Array[Image] = []
	var shown := {}
	var guard := 0
	while guard < m.total + 60 and shown.size() < frames.size():
		guard += 1
		var fr := a.mf if a.state == Fighter.S.MOVE and a.move == m else -1
		for v in views:
			v.sync_from_sim()
			v.update_visual(1.0, 1.0 / 60.0)
		if fr in frames and not shown.has(fr):
			shown[fr] = true
			if debug:
				debug.redraw()
			for ang in angles:
				_aim(Vector3((a.pos.x + b.pos.x) * 0.5, focus, 0), ang, dist)
				label.text = "%s %s  f%d/%d  %s" % [fighter_id, move_id, fr, m.total, a.state_name()]
				out.append(await _grab())
		sim.step([Vector3i.ZERO, Vector3i.ZERO])
		if a.state != Fighter.S.MOVE and shown.size() > 0 and fr < 0:
			break
	return out


## Plays a list of commands (arcade notation, one per entry, a few ticks apart) and captures
## every `every` ticks from the side.
func _cmd_strip(sim: FightSim) -> Array[Image]:
	var a := sim.fighters[0]
	var b := sim.fighters[1]
	a.pos = Vector2(-gap * 0.5, 0)
	b.pos = Vector2(gap * 0.5, 0)
	var views := _views(sim)
	var intent := PadIntent.new()
	for c in cmds:
		if c.begins_with("wait"):
			intent.hold(5, int(c.trim_prefix("wait")))
		else:
			intent.command(c)
			intent.hold(5, 5)
	intent.hold(5, 40)
	var out: Array[Image] = []
	var t := 0
	while intent.busy():
		t += 1
		_step(sim, views, [intent.next(a.facing), Vector3i.ZERO])
		if t % every == 0:
			if debug:
				debug.redraw()
			for ang in angles:
				_aim(Vector3((a.pos.x + b.pos.x) * 0.5, focus, 0), ang, dist)
				label.text = "%s t%d %s %s f%d | %s" % [fighter_id, t, a.state_name(), a.move.id if a.move else "", a.mf, b.state_name()]
				out.append(await _grab())
	return out


## Camera on a circle around `target`: yaw 90 looks from +z (the fight camera's side).
func _aim(target: Vector3, yaw_deg: float, d: float) -> void:
	var y := deg_to_rad(yaw_deg)
	var p := target + Vector3(cos(y), 0.12, sin(y)) * d
	cam.global_transform = Transform3D(Basis.looking_at(target - p, Vector3.UP), p)


func _grab() -> Image:
	await RenderingServer.frame_post_draw
	await RenderingServer.frame_post_draw
	return get_viewport().get_texture().get_image()


func _save_sheet(shots: Array[Image], name: String) -> void:
	if shots.is_empty():
		return
	var per_row := mini(shots.size(), 6 if move_id != "" else maxi(angles.size() * 2, 4))
	var rows := int(ceil(float(shots.size()) / per_row))
	var w := shots[0].get_width()
	var h := shots[0].get_height()
	var sheet := Image.create(w * per_row, h * rows, false, shots[0].get_format())
	for i in shots.size():
		sheet.blit_rect(shots[i], Rect2i(0, 0, w, h), Vector2i((i % per_row) * w, (i / per_row) * h))
	var path := out_dir.path_join(name + ".png")
	sheet.save_png(path)
	print("saved ", path)
