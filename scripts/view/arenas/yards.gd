extends Arena
## The Rendering Yards of Tallowmarket: an open iron floor between the great vats where the Sump's
## slop is boiled down over fireboxes, a furnace house behind, smokestacks against a sky lit by the
## furnaces, drizzle. Gorm Tallow's home ground. The vat behind the centre of the floor (x 0,
## z -6.2, radius 2, lip at 3.3 m) takes part in the stage finisher.

const VATS := [[0.0, -6.2, 2.0, 2.55], [-5.5, -7.6, 1.6, 2.3], [5.6, -7.4, 1.65, 2.45], [-10.8, -7.0, 1.9, 2.7], [10.9, -7.2, 1.85, 2.6]]

func build() -> void:
	music = "music_yards"
	make_environment({"top": Color(0.012, 0.012, 0.016), "horizon": Color(0.13, 0.095, 0.08), "glow_color": Color(1.0, 0.42, 0.14),
		"glow_dir": Vector3(-0.3, 0.12, -1.0), "glow_size": 0.4, "glow_energy": 0.8, "clouds": 0.7},
		Color(0.08, 0.09, 0.1), 0.02, 0.3)
	environment.adjustment_contrast = 1.1
	environment.adjustment_saturation = 1.0
	make_key(Color(1.0, 0.9, 0.8), 2.0, Vector3(-42, -35, 0))
	make_fill(Color(0.78, 0.84, 0.92), 0.55)
	make_rim(Color(0.5, 0.7, 1.0), 1.3, Vector3(-36, 165, 0))
	_floor()
	_vats()
	_furnace_house()
	_catwalk()
	_ends()
	rain(1200, Color(0.62, 0.68, 0.72, 0.28))
	finish()


func _floor() -> void:
	var floor_mat := surface(3, Color(0.31, 0.315, 0.32), Color(0.16, 0.17, 0.18), 1.0, 0.72, 0.18, 0.45)
	box(Vector3(30, 0.4, 10), Vector3(0, -0.2, -0.5), floor_mat).layers = FLOOR_LAYER
	var kerb := surface(1, Color(0.3, 0.29, 0.27), Color(0.17, 0.17, 0.16), 1.4, 0.82, 0.3)
	box(Vector3(30, 0.22, 0.45), Vector3(0, 0.11, -3.15), kerb)
	plane(Vector2(30, 1.3), Vector3(0, 0.03, -4.05), slop(Color(0.03, 0.05, 0.015), Color(0.4, 0.8, 0.1), 0.8, 0.18)).layers = FLOOR_LAYER
	var yard := surface(1, Color(0.2, 0.19, 0.18), Color(0.1, 0.1, 0.1), 1.6, 0.75, 0.5)
	box(Vector3(40, 0.6, 10), Vector3(0, -0.5, -9.7), yard).layers = FLOOR_LAYER
	# Drain grates and a few loose plates break up the floor.
	var grate := surface(3, Color(0.08, 0.075, 0.07), Color(0.04, 0.04, 0.04), 0.25, 0.6, 0.0, 0.7)
	var b := Batch.new()
	for g in [[-3.6, 1.1], [3.9, -1.9], [-9.5, -1.5], [9.2, 1.4]]:
		b.box(Vector3(0.9, 0.02, 0.6), Vector3(g[0], 0.005, g[1]))
	commit(b, grate, false, FLOOR_LAYER)


func _vats() -> void:
	var tank := surface(3, Color(0.19, 0.185, 0.18), Color(0.085, 0.08, 0.075), 0.8, 0.42, 0.15, 0.75)
	var band := surface(0, Color(0.26, 0.15, 0.09), Color(0.1, 0.07, 0.05), 0.5, 0.5, 0.0, 0.7)
	var brick := surface(4, Color(0.2, 0.13, 0.1), Color(0.09, 0.065, 0.055), 0.7, 0.9, 0.2)
	var slop_mat := slop(Color(0.03, 0.06, 0.015), Color(0.5, 0.95, 0.15), 1.9, 0.3)
	var spill := shader_mat("res://assets/shaders/stage_fall.gdshader", {"albedo": Color(0.06, 0.1, 0.02),
		"glow": Color(0.45, 0.9, 0.12), "glow_energy": 1.15, "glow_floor": 0.8, "speed": 0.7, "fray": 0.5})
	var fire := emissive(Color(1.0, 0.42, 0.1), 2.6)
	var bands := Batch.new()
	var bricks := Batch.new()
	var tanks := Batch.new()
	for v in VATS:
		var x: float = v[0]
		var z: float = v[1]
		var r: float = v[2]
		var h: float = v[3]
		var base := 0.75
		var lip := base + h
		bricks.cyl(r + 0.25, base, Vector3(x, base * 0.5, z), Vector3.ZERO, -1.0, 24)
		box(Vector3(0.7, 0.3, 0.06), Vector3(x, 0.3, z + r + 0.22), fire, Vector3.ZERO, false)
		for k in 5:
			bands.cyl(0.02, 0.32, Vector3(x - 0.28 + k * 0.14, 0.3, z + r + 0.27), Vector3.ZERO, -1.0, 6)
		tanks.cyl(r, h, Vector3(x, base + h * 0.5, z), Vector3.ZERO, -1.0, 40)
		tanks.cyl(r + 0.06, h * 0.45, Vector3(x, base + h * 0.42, z), Vector3.ZERO, -1.0, 40)
		for y in [base + 0.18, base + h * 0.72]:
			bands.cyl(r + 0.08, 0.12, Vector3(x, y, z), Vector3.ZERO, -1.0, 40)
		bands.cyl(r + 0.16, 0.2, Vector3(x, lip, z), Vector3.ZERO, -1.0, 40)
		var top := MeshInstance3D.new()
		var disc := CylinderMesh.new()
		disc.top_radius = r - 0.05
		disc.bottom_radius = r - 0.05
		disc.height = 0.02
		disc.radial_segments = 40
		top.mesh = disc
		top.material_override = slop_mat
		top.position = Vector3(x, lip - 0.06, z)
		add_child(top)
		plume(Vector3(x, lip + 0.3, z), Color(0.5, 0.56, 0.48, 0.35), 14, r * 0.5, 0.6, 1.6)
		var q := QuadMesh.new()
		q.size = Vector2(0.32 + r * 0.08, h + 0.1)
		var s := MeshInstance3D.new()
		s.mesh = q
		s.material_override = spill
		s.position = Vector3(x + r * 0.25, base + h * 0.5 + 0.05, z + r * 0.97 + 0.17)
		s.rotation_degrees = Vector3(0, 14.5, 0)
		s.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		add_child(s)
		# Green light spilling over the lip and down the vat's face.
		var g := omni(Vector3(x, lip + 0.25, z + r + 0.5), Color(0.5, 1.0, 0.18), 2.4, 3.4 + r)
		flicker(g, 2.4, 0.12, 1.7)
		# A feed pipe from each vat back to the furnace house.
		bands.cyl(0.13, 12.5 + z, Vector3(x - r * 0.4, base + h - 0.5, (z - 12.5) * 0.5), Vector3(90, 0, 0), -1.0, 12)
	commit(bricks, brick)
	commit(tanks, tank)
	commit(bands, band)
	# A ladder up the side of the right-hand vat gives the scale.
	var lad := Batch.new()
	for rx in [4.35, 4.75]:
		lad.cyl(0.025, 2.9, Vector3(rx, 2.2, -5.72), Vector3.ZERO, -1.0, 6)
	for k in 9:
		lad.cyl(0.018, 0.4, Vector3(4.55, 1.0 + k * 0.3, -5.72), Vector3(0, 0, 90), -1.0, 6)
	commit(lad, band, false)


func _furnace_house() -> void:
	var brick := surface(4, Color(0.19, 0.12, 0.095), Color(0.085, 0.06, 0.05), 1.0, 0.9, 0.15)
	var iron := surface(3, Color(0.1, 0.095, 0.09), Color(0.05, 0.05, 0.045), 0.6, 0.55, 0.0, 0.7)
	box(Vector3(46, 6.6, 1.0), Vector3(0, 3.3, -12.6), brick)
	box(Vector3(46, 0.4, 1.6), Vector3(0, 6.7, -12.5), iron)
	var bars := Batch.new()
	var frames := Batch.new()
	var window := emissive(Color(0.9, 0.5, 0.2), 0.7)
	var mouth := emissive(Color(1.0, 0.33, 0.06), 2.0)
	for x in [-13.5, -8.2, -2.7, 2.9, 8.3, 13.6]:
		box(Vector3(1.9, 1.3, 0.1), Vector3(x, 1.05, -12.08), mouth, Vector3.ZERO, false)
		frames.box(Vector3(2.3, 0.22, 0.3), Vector3(x, 1.8, -11.98))
		frames.box(Vector3(2.3, 0.18, 0.3), Vector3(x, 0.3, -11.98))
		for k in 7:
			bars.cyl(0.03, 1.3, Vector3(x - 0.84 + k * 0.28, 1.05, -11.95), Vector3.ZERO, -1.0, 6)
		# Soot above each mouth: a darker hood; a high window above that.
		frames.box(Vector3(2.7, 0.9, 0.5), Vector3(x, 2.35, -11.85), Vector3(-12, 0, 0))
		box(Vector3(1.1, 1.3, 0.08), Vector3(x, 4.4, -12.08), window, Vector3.ZERO, false)
		frames.box(Vector3(1.4, 0.15, 0.25), Vector3(x, 3.7, -12.0))
		frames.box(Vector3(0.08, 1.3, 0.14), Vector3(x, 4.4, -12.0))
		frames.box(Vector3(1.1, 0.06, 0.14), Vector3(x, 4.5, -12.0))
	commit(bars, iron, false)
	commit(frames, iron)
	for x in [-8.2, 8.3]:
		var fl := omni(Vector3(x, 1.4, -10.6), Color(1.0, 0.5, 0.18), 2.6, 7.0)
		flicker(fl, 2.6, 0.28, 7.0)
	motes(Vector3(0, 2.2, -11.4), Vector3(15, 1.8, 0.5), 70, Color(1.0, 0.55, 0.2, 1.0), 0.035, 0.35).process_material.gravity = Vector3(0.1, 0.25, 0)
	# Stacks and the furnace glow beyond the roofline.
	var stack := surface(4, Color(0.16, 0.1, 0.08), Color(0.07, 0.05, 0.04), 1.4, 0.9, 0.0)
	for s in [[-15.0, -20.0, 1.0], [-6.5, -24.0, 1.2], [4.5, -21.0, 0.9], [13.0, -23.0, 1.1], [21.0, -26.0, 1.0]]:
		cyl(s[2], 16.0, Vector3(s[0], 8.0, s[1]), stack, Vector3.ZERO, s[2] * 0.75, 16)
		plume(Vector3(s[0], 16.4, s[1]), Color(0.22, 0.18, 0.16, 0.6), 12, 0.6, 1.0, 3.0)
	box(Vector3(60, 9.0, 1.0), Vector3(0, 4.5, -30.0), brick)


func _catwalk() -> void:
	var band := surface(0, Color(0.12, 0.11, 0.1), Color(0.06, 0.06, 0.05), 1.0, 0.45, 0.0, 0.9)
	var walk := surface(3, Color(0.2, 0.19, 0.18), Color(0.1, 0.1, 0.1), 0.6, 0.6, 0.1, 0.7)
	box(Vector3(30, 0.12, 1.4), Vector3(0, 4.1, -4.9), walk)
	var b := Batch.new()
	for x in range(-14, 15, 2):
		b.cyl(0.035, 1.0, Vector3(x, 4.65, -4.25), Vector3.ZERO, -1.0, 6)
		if x % 4 == 0:
			b.box(Vector3(0.16, 4.1, 0.16), Vector3(x + 1.0, 2.05, -4.3))
	b.box(Vector3(30, 0.05, 0.05), Vector3(0, 5.15, -4.25))
	b.box(Vector3(30, 0.04, 0.04), Vector3(0, 4.65, -4.25))
	commit(b, band)
	var chain_mat := Shapes.material(Color(0.22, 0.2, 0.18), 0.5, 0.85)
	for x in [-6.6, -2.3, 2.7, 6.4]:
		var c := chain(Vector3(x, 4.05, -3.95), 1.8 + fmod(absf(x), 1.1), chain_mat)
		var hook := MeshInstance3D.new()
		var tm := TorusMesh.new()
		tm.inner_radius = 0.06
		tm.outer_radius = 0.1
		hook.mesh = tm
		hook.material_override = chain_mat
		hook.position = Vector3(0, -1.85 - fmod(absf(x), 1.1), 0)
		hook.rotation_degrees = Vector3(90, 0, 0)
		c.add_child(hook)


func _ends() -> void:
	# Gantry legs and stacked drums close the floor at both ends, so the boundary reads.
	var leg := surface(3, Color(0.24, 0.22, 0.2), Color(0.12, 0.11, 0.1), 0.7, 0.55, 0.2, 0.65)
	var drum := surface(3, Color(0.36, 0.2, 0.1), Color(0.18, 0.1, 0.06), 0.7, 0.6, 0.25, 0.55)
	var drums := Batch.new()
	var legs := Batch.new()
	for side in [-1.0, 1.0]:
		var bx: float = side * (FightSim.STAGE_HALF + 0.8)
		legs.box(Vector3(0.75, 8.0, 0.75), Vector3(bx + side * 0.3, 4.0, -1.7))
		legs.box(Vector3(0.95, 0.3, 0.95), Vector3(bx + side * 0.3, 0.15, -1.7))
		legs.box(Vector3(0.18, 5.5, 0.18), Vector3(bx + side * 0.1, 2.5, -1.2), Vector3(0, 0, side * 22))
		for k in 3:
			drums.cyl(0.33, 0.9, Vector3(bx + side * 0.1 + k * side * 0.05, 0.45, -0.4 + k * 0.72), Vector3(0, k * 40, 0), -1.0, 20)
		drums.cyl(0.33, 0.9, Vector3(bx + side * 0.2, 1.35, 0.0), Vector3(0, 20, 0), -1.0, 20)
		drums.cyl(0.33, 0.9, Vector3(bx + side * 0.75, 0.33, 1.1), Vector3(90, side * 30, 0), -1.0, 20)
	commit(legs, leg)
	commit(drums, drum)
	# Low clutter in front, far below the fight.
	var pipe := surface(0, Color(0.16, 0.1, 0.07), Color(0.08, 0.05, 0.04), 0.8, 0.5, 0.0, 0.8)
	cyl(0.07, 34.0, Vector3(0, 0.07, 3.2), pipe, Vector3(0, 0, 90), -1.0, 12)
