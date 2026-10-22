extends Node3D
## Authoring tool: builds a stage with two fighters and saves screenshots from the fight camera at
## representative framings, or times frames.
##   godot --path . res://tools/preview/stage_shot.tscn -- --stage=drowned_chapel --out=/tmp/stages
##   godot --path . res://tools/preview/stage_shot.tscn -- --stage=roost --bench=400
## Options: --fighter=ashka, --shots=centre,left,right,wide,air, --graphics=low, --warm=3.0
## (seconds of animation before the first shot, so particles fill in). Framings: centre (3 m
## apart), left / right (near a wall), wide (5.6 m), air (one fighter 2 m up). A contact sheet of
## all shots at half size is saved as <stage>_sheet.png. Vsync is off so --bench measures the
## real frame time; it prints mean, 95th percentile and the GPU render time. --burst=N saves N
## consecutive frames, --gap seconds apart (default 0.05), as one small contact sheet
## (<stage>_burst.png), for checking animation such as the roost's lightning.

const FRAMINGS := {
	"centre": [-1.5, 1.5, 0.0],
	"left": [-6.6, -3.6, 0.0],
	"right": [3.6, 6.6, 0.0],
	"wide": [-2.8, 2.8, 0.0],
	"air": [-1.2, 1.4, 2.0],
}

var stage_id := "rendering_yards"
var fighter_id := "ashka"
var out_dir := "/tmp/stages"
var shots: PackedStringArray = PackedStringArray(["centre", "left", "right", "wide", "air"])
var bench := 0
var warm := 3.0
var burst := 0
var gap := 0.05
var sim: FightSim
var arena: Arena
var cam: FightCamera
var views: Array[FighterView] = []


func _ready() -> void:
	for a in OS.get_cmdline_user_args():
		var kv := a.trim_prefix("--").split("=", true, 1)
		if kv.size() < 2:
			continue
		match kv[0]:
			"stage":
				stage_id = kv[1]
			"fighter":
				fighter_id = kv[1]
			"out":
				out_dir = kv[1]
			"shots":
				shots = kv[1].split(",")
			"bench":
				bench = int(kv[1])
			"warm":
				warm = float(kv[1])
			"burst":
				burst = int(kv[1])
			"gap":
				gap = float(kv[1])
	DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	get_window().size = Vector2i(1920, 1080) if bench > 0 else Vector2i(1600, 900)
	DirAccess.make_dir_recursive_absolute(out_dir)
	_run.call_deferred()


func _run() -> void:
	var t0 := Time.get_ticks_usec()
	# Bail out rather than hang if the stage script fails to load.
	get_tree().create_timer(120.0).timeout.connect(func(): get_tree().quit(2))
	arena = Arena.create(stage_id)
	if arena == null:
		get_tree().quit(1)
		return
	add_child(arena)
	arena.build()
	print("built %s in %.1f ms" % [stage_id, (Time.get_ticks_usec() - t0) / 1000.0])
	var def := Content.fighter(fighter_id)
	sim = FightSim.new()
	sim.training = true
	sim.intro = false
	sim.setup(def, def)
	sim.phase = FightSim.Phase.FIGHT
	for f in sim.fighters:
		var v := FighterView.new()
		add_child(v)
		v.setup(f, Content.common_poses)
		views.append(v)
	views[1].set_costume(0.42, 1.05)
	cam = FightCamera.new()
	cam.sim = sim
	add_child(cam)
	cam.current = true
	_place("centre")
	await _animate(warm)
	if bench > 0:
		await _bench()
	elif burst > 0:
		var frames: Array[Image] = []
		for i in burst:
			await _animate(gap)
			var img := get_viewport().get_texture().get_image()
			img.resize(400, 225, Image.INTERPOLATE_BILINEAR)
			frames.append(img)
		var cols := 6
		var sheet := Image.create(400 * cols, 225 * int(ceil(burst / float(cols))), false, frames[0].get_format())
		for i in burst:
			sheet.blit_rect(frames[i], Rect2i(0, 0, 400, 225), Vector2i((i % cols) * 400, (i / cols) * 225))
		var path := out_dir.path_join("%s_burst.png" % stage_id)
		sheet.save_png(path)
		print("saved ", path)
	else:
		var imgs: Array[Image] = []
		for s in shots:
			_place(s)
			await _animate(0.5)
			var img := get_viewport().get_texture().get_image()
			var path := out_dir.path_join("%s_%s.png" % [stage_id, s])
			img.save_png(path)
			print("saved ", path)
			imgs.append(img)
		_save_sheet(imgs)
	sim.dispose()
	get_tree().quit(0)


func _place(framing: String) -> void:
	var f: Array = FRAMINGS.get(framing, FRAMINGS["centre"])
	for i in 2:
		var ft := sim.fighters[i]
		ft.set_state(Fighter.S.IDLE)
		ft.pos = Vector2(f[i], 0.0)
		ft.vel = Vector2.ZERO
		ft.facing = 1 if i == 0 else -1
	if f[2] > 0.0:
		sim.fighters[0].set_state(Fighter.S.AIR)
		sim.fighters[0].pos.y = f[2]
	for v in views:
		v.sync_from_sim(true)
	cam.snap()


## Runs the views, stage and camera for `secs` of real frames without stepping the simulation, so
## fighters hold their placed positions.
func _animate(secs: float) -> void:
	var t := 0.0
	while t < secs:
		var d := 1.0 / 60.0
		for v in views:
			v.update_visual(1.0, d)
		arena.step(d)
		cam.step(d)
		await RenderingServer.frame_post_draw
		t += d


func _bench() -> void:
	var vp := get_viewport().get_viewport_rid()
	RenderingServer.viewport_set_measure_render_time(vp, true)
	var times := PackedFloat32Array()
	var gpu := 0.0
	var order := ["centre", "left", "right", "wide", "air"]
	var last := Time.get_ticks_usec()
	for i in bench:
		if i % 80 == 0:
			_place(order[(i / 80) % order.size()])
		sim.step([Vector3i.ZERO, Vector3i.ZERO])
		for v in views:
			v.sync_from_sim()
			v.update_visual(1.0, 1.0 / 60.0)
		arena.step(1.0 / 60.0)
		cam.step(1.0 / 60.0)
		await RenderingServer.frame_post_draw
		var now := Time.get_ticks_usec()
		if i >= 20:
			times.append((now - last) / 1000.0)
			gpu += RenderingServer.viewport_get_measured_render_time_gpu(vp)
		last = now
	var sorted := times.duplicate()
	sorted.sort()
	var mean := 0.0
	for x in times:
		mean += x
	mean /= times.size()
	print("BENCH %s graphics=%s: mean %.2f ms (%.0f fps), p95 %.2f ms, gpu %.2f ms over %d frames at %s" % [stage_id,
		Settings.get_value("graphics"), mean, 1000.0 / mean, sorted[int(sorted.size() * 0.95)], gpu / times.size(),
		times.size(), get_window().size])


func _save_sheet(imgs: Array[Image]) -> void:
	if imgs.is_empty():
		return
	var w := imgs[0].get_width() / 2
	var h := imgs[0].get_height() / 2
	var cols := 2
	var rows := int(ceil(imgs.size() / float(cols)))
	var sheet := Image.create(w * cols, h * rows, false, imgs[0].get_format())
	for i in imgs.size():
		var small := imgs[i].duplicate() as Image
		small.resize(w, h, Image.INTERPOLATE_BILINEAR)
		sheet.blit_rect(small, Rect2i(0, 0, w, h), Vector2i((i % cols) * w, (i / cols) * h))
	var path := out_dir.path_join("%s_sheet.png" % stage_id)
	sheet.save_png(path)
	print("saved ", path)
