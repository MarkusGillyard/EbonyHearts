extends Node3D
## Renders each fighter's select-screen portrait to assets/portraits/<id>.png: head and shoulders
## in their stance, lit from the front with a coloured rim in their glow colour, on a dark
## gradient. Run after a model changes:
##   godot --path . res://tools/preview/portraits.tscn [-- --only=ashka]

const SIZE := Vector2i(420, 500)

var only := ""


func _ready() -> void:
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--only="):
			only = a.trim_prefix("--only=")
	get_window().size = SIZE
	_run.call_deferred()


func _run() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://assets/portraits"))
	var env := WorldEnvironment.new()
	var e := Environment.new()
	e.background_mode = Environment.BG_COLOR
	e.background_color = Color(0.03, 0.045, 0.04)
	e.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	e.ambient_light_color = Color(0.3, 0.33, 0.32)
	e.ambient_light_energy = 0.5
	e.tonemap_mode = Environment.TONE_MAPPER_AGX
	e.glow_enabled = true
	env.environment = e
	add_child(env)
	var key := DirectionalLight3D.new()
	key.light_color = Color(1.0, 0.88, 0.74)
	key.light_energy = 1.8
	key.rotation_degrees = Vector3(-25, 25, 0)
	add_child(key)
	var rim := DirectionalLight3D.new()
	rim.light_energy = 2.2
	rim.rotation_degrees = Vector3(-15, 205, 0)
	add_child(rim)
	var cam := Camera3D.new()
	cam.fov = 24.0
	add_child(cam)
	cam.current = true
	for id in Content.all_ids():
		if only != "" and not id in only.split(","):
			continue
		var def := Content.fighter(id)
		rim.light_color = Color(str(def.look.get("glow", "#a8dc3a")))
		var sim := FightSim.new()
		sim.training = true
		sim.intro = false
		sim.setup(def, def)
		var f := sim.fighters[0]
		f.pos = Vector2.ZERO
		f.facing = 1
		f.set_state(Fighter.S.IDLE)
		sim.fighters[1].pos = Vector2(40, 0)
		var v := FighterView.new()
		add_child(v)
		v.setup(f, Content.common_poses)
		v.preview("stance")
		for i in 90:
			for c in v.cloths:
				c.step(1.0 / 60.0)
		var head := v.rig.global_transform * v.rig.joint_pos("head")
		var target := head + Vector3(0, 0.02 * def.height, 0)
		var dir := Vector3(0.55, 0.05, 1.0).normalized()
		cam.global_transform = Transform3D(Basis.looking_at(-dir, Vector3.UP), target + dir * (1.15 * def.height / 1.8) + Vector3(0, -0.12, 0))
		await RenderingServer.frame_post_draw
		await RenderingServer.frame_post_draw
		await RenderingServer.frame_post_draw
		var img := get_viewport().get_texture().get_image()
		img.save_png(ProjectSettings.globalize_path("res://assets/portraits/%s.png" % id))
		print("portrait ", id)
		v.queue_free()
		sim.dispose()
		await get_tree().process_frame
	get_tree().quit(0)
