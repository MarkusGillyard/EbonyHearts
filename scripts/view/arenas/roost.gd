extends Arena
## The Slopbat's Roost, beneath the Sump: a broad ledge of rock and tar over a lake of glowing
## slop. The ribs of something colossal rise from the lake on the left, spines of bone-like rock on
## the right, beside the mast of a drowned ship; stalactites hang overhead and tar falls pour from
## the dark into the lake. The drowned hoard glints on the ledge's ends and on islands. The slop
## light pulses, and now and then green lightning flickers far back in the cavern: a distant
## light, never a full-screen flash, and only a slow swell when flashes are turned off.

const LAKE_Y := -2.2
const LEDGE_BACK := -2.9

var _rng := RandomNumberGenerator.new()
var _rock: ShaderMaterial
var _rocks := Batch.new()
var _bones := Batch.new()
var _gold := Batch.new()
var _coins: Array = []
var _glints: Array = []
var _storm: OmniLight3D
var _bolts: Array[MeshInstance3D] = []


func build() -> void:
	music = "music_roost"
	_rng.seed = 23
	make_environment({"top": Color(0.006, 0.01, 0.006), "horizon": Color(0.03, 0.05, 0.03), "glow_color": Color(0.35, 0.7, 0.15),
		"glow_dir": Vector3(0, -0.1, -1.0), "glow_size": 0.5, "glow_energy": 0.3, "clouds": 0.0},
		Color(0.035, 0.075, 0.04), 0.034, 0.22)
	environment.fog_height = LAKE_Y + 1.5
	environment.fog_height_density = 0.2
	environment.glow_intensity = 0.8
	environment.glow_bloom = 0.06
	make_key(Color(1.0, 0.88, 0.72), 1.2, Vector3(-40, -30, 0))
	make_fill(Color(0.75, 0.85, 0.8), 0.55)
	make_rim(Color(0.5, 0.95, 0.4), 1.7, Vector3(-22, 175, 0))
	_rock = surface(0, Color(0.085, 0.1, 0.09), Color(0.028, 0.034, 0.03), 1.2, 0.7, 0.4)
	_ledge()
	_lake()
	_ribs()
	_spires()
	_ship()
	_ceiling()
	_far_wall()
	_ends()
	_hoard()
	_storm_light()
	commit(_rocks, _rock)
	commit(_bones, surface(0, Color(0.5, 0.48, 0.4), Color(0.25, 0.24, 0.19), 0.5, 0.65, 0.15))
	var gold := Shapes.material(Color(1.0, 0.74, 0.3), 0.36, 0.85, 0.1)
	commit(_gold, gold)
	var coin := CylinderMesh.new()
	coin.top_radius = 0.035
	coin.bottom_radius = 0.035
	coin.height = 0.007
	coin.radial_segments = 8
	coin.rings = 1
	multi(coin, _coins, gold)
	sprites(_glints, Vector2(0.45, 0.45), Color(1.0, 0.85, 0.5), 4.0, 1, 0.02)
	motes(Vector3(0, LAKE_Y + 1.6, -9.0), Vector3(16, 1.6, 6), 120, Color(0.55, 1.0, 0.25, 0.9), 0.045, 0.25)
	mist(Vector2(70, 16), Vector3(0, LAKE_Y + 0.5, -10.0), Color(0.22, 0.42, 0.12), 0.3)
	mist(Vector2(120, 30), Vector3(0, LAKE_Y + 1.0, -32.0), Color(0.18, 0.36, 0.1), 0.45)
	finish()


## A lumpy boulder: a few faceted, squashed blobs.
func _boulder(c: Vector3, r: float) -> void:
	for k in 3:
		var o := Vector3(_rng.randf_range(-0.5, 0.5) * r, _rng.randf_range(-0.1, 0.3) * r, _rng.randf_range(-0.3, 0.3) * r)
		var m := SphereMesh.new()
		m.radius = r * _rng.randf_range(0.6, 1.0)
		m.height = m.radius * 2.0
		m.radial_segments = 7
		m.rings = 4
		_rocks.add(m, Transform3D(Basis.from_euler(Vector3(_rng.randf(), _rng.randf() * TAU, _rng.randf())).scaled(Vector3(_rng.randf_range(1.0, 1.5), 0.65, 1.0)), c + o))


func _ledge() -> void:
	var floor_mat := surface(0, Color(0.16, 0.175, 0.16), Color(0.03, 0.036, 0.032), 0.55, 0.8, 0.45)
	box(Vector3(32, 1.0, 7.2), Vector3(0, -0.5, (LEDGE_BACK + 4.3) * 0.5), floor_mat).layers = FLOOR_LAYER
	_rocks.box(Vector3(32, 2.4, 1.2), Vector3(0, -1.4, LEDGE_BACK - 0.3), Vector3(-12, 0, 0))
	# A ragged lip of boulders along the back edge, with gaps, and tar pooled behind it.
	var x := -15.5
	while x < 15.5:
		var r := _rng.randf_range(0.25, 0.65)
		if absf(x) < 2.0 and r > 0.4:
			r *= 0.6
		_boulder(Vector3(x, r * 0.2, LEDGE_BACK + _rng.randf_range(-0.15, 0.25)), r)
		x += r * 1.6 + _rng.randf_range(0.0, 0.8)
	var tar := slop(Color(0.012, 0.018, 0.01), Color(0.35, 0.75, 0.1), 0.35, 0.12)
	for p in [[-5.2, -2.3, 1.4], [3.8, -2.4, 1.1], [-11.0, -1.9, 1.0], [11.5, -2.2, 0.9]]:
		var mi := cyl(p[2], 0.02, Vector3(p[0], 0.01, p[1]), tar, Vector3.ZERO, -1.0, 20, false)
		mi.scale = Vector3(1.0, 1.0, 0.45)
		mi.layers = FLOOR_LAYER
	# Cracks in the ledge where the slop glows through.
	var cracks := Batch.new()
	for i in 12:
		_crack(cracks, Vector3(_rng.randf_range(-13.0, 13.0), 0.004, LEDGE_BACK + 0.25), _rng.randf_range(-0.7, 0.7), 0.035, _rng.randi_range(6, 14), true)
	commit(cracks, emissive(Color(0.45, 0.95, 0.15), 1.1), false, FLOOR_LAYER)
	# Scattered bones and stones at the front, low.
	for i in 9:
		var bp := Vector3(_rng.randf_range(-11, 11), 0.04, _rng.randf_range(1.8, 3.8))
		if absf(bp.x) < 1.5:
			continue
		_bones.cyl(0.04, _rng.randf_range(0.3, 0.6), bp, Vector3(90, _rng.randf() * 180.0, 0), 0.03, 6)
		_rocks.sphere(_rng.randf_range(0.08, 0.2), bp + Vector3(_rng.randf_range(-0.5, 0.5), 0, 0.3), Vector3(1.3, 0.6, 1), 8)
	_bones.sphere(0.13, Vector3(-6.4, 0.1, 2.9), Vector3(1, 0.85, 1.2), 12)


## A jagged crack wandering from `p` (heading 0 points to +z), thinning, with the odd fork.
func _crack(b: Batch, p: Vector3, heading: float, w: float, steps: int, fork: bool) -> void:
	for k in steps:
		var step := _rng.randf_range(0.12, 0.3)
		heading += _rng.randf_range(-0.9, 0.9)
		heading = clampf(heading, -1.3, 1.3)
		var q := p + Vector3(sin(heading), 0, cos(heading)) * step
		var m := BoxMesh.new()
		m.size = Vector3(w, 0.008, step + w * 0.5)
		b.add(m, Transform3D(Basis(Vector3.UP, heading), (p + q) * 0.5))
		p = q
		w *= 0.88
		if fork and k == steps / 2:
			_crack(b, p, heading + (0.9 if _rng.randf() < 0.5 else -0.9), w * 0.8, steps / 2, false)


func _lake() -> void:
	var lake := plane(Vector2(260, 140), Vector3(0, LAKE_Y, -72.0), slop(Color(0.02, 0.035, 0.012), Color(0.4, 0.85, 0.12), 0.85, 0.15))
	lake.material_override.set_shader_parameter("bubble_scale", 1.2)
	lake.layers = FLOOR_LAYER
	for x in [-8.0, 0.0, 8.0]:
		var l := omni(Vector3(x, LAKE_Y + 1.0, LEDGE_BACK - 1.6), Color(0.45, 1.0, 0.2), 2.2, 8.5, false, 0.25)
		flicker(l, 2.2, 0.18, 1.1)
	var deep := omni(Vector3(0, LAKE_Y + 2.5, -22.0), Color(0.45, 1.0, 0.2), 3.0, 28.0, false, 0.2)
	flicker(deep, 3.0, 0.2, 0.7)
	# Islands of rock in the lake.
	for s in [[-6.5, -13.0, 2.6, 1.6], [8.0, -20.0, 3.4, 2.2], [-16.0, -24.0, 3.0, 2.0], [18.0, -12.0, 2.2, 1.5]]:
		_rocks.sphere(s[2], Vector3(s[0], LAKE_Y - s[2] * 0.25, s[1]), Vector3(1.0, 0.45, s[3] / s[2]), 14)


func _ribs() -> void:
	# The ribcage of a colossal beast, half drowned: pairs of ribs curving towards each other.
	var xc := -6.0
	for i in 6:
		var z := -10.5 - i * 2.3
		var w := 3.4 - i * 0.15
		var h := 6.4 - i * 0.45
		for side in [-1.0, 1.0]:
			var prev := Vector3(xc + side * w, LAKE_Y - 0.3, z)
			for k in range(1, 11):
				var t := k / 10.0
				var a := t * PI * 0.5
				var p := Vector3(xc + side * (w * cos(a) * (1.0 - t * 0.15) + 0.35), LAKE_Y + h * sin(a) * (1.0 + t * 0.1), z + sin(a) * 0.4)
				var r := 0.26 * (1.0 - t * 0.7)
				_cyl_between(_bones, prev, p, r, r * 0.86)
				prev = p
	for i in 14:
		_bones.sphere(0.38, Vector3(xc + 0.0, LAKE_Y + 0.1, -9.6 - i * 1.0), Vector3(1.2, 0.9, 0.7), 10)


func _spires() -> void:
	# Spines of bone-like rock: stacked vertebrae, leaning a little.
	for s in [[6.0, -10.5, 7.0, 4.0], [10.5, -15.0, 9.0, -6.0], [3.0, -24.0, 8.0, 3.0], [-13.0, -15.5, 8.5, -5.0], [14.5, -26.0, 10.0, 6.0], [-2.0, -34.0, 9.0, -3.0]]:
		var base := Vector3(s[0], LAKE_Y - 0.3, s[1])
		var n := int(s[2] / 0.55)
		var lean := Vector3(sin(deg_to_rad(s[3])), 0, 0)
		var yaw := _rng.randf() * 180.0
		for k in n:
			var t := float(k) / n
			var r := 0.7 * (1.0 - t * 0.7)
			var c := base + Vector3(0, k * 0.55, 0) + lean * k * 0.55 * t
			_bones.sphere(r, c, Vector3(1.0, 0.32, 1.0), 10)
			_bones.cyl(r * 0.45, 0.4, c + Vector3(0, 0.27, 0), Vector3.ZERO, r * 0.4, 8)
			# Transverse processes and a spine curving down: what makes it read as vertebrae.
			_bones.box(Vector3(r * 3.0, 0.07, 0.14), c, Vector3(0, yaw, -8))
			_bones.cyl(0.07 * (1.0 - t * 0.5), r * 1.6, c + Basis(Vector3.UP, deg_to_rad(yaw + 90)) * Vector3(r * 0.9, -0.1, 0), Vector3(0, yaw, 60), 0.02, 6)
		_rocks.sphere(1.4, base + Vector3(0, 0.2, 0), Vector3(1.3, 0.5, 1.1), 10)


func _ship() -> void:
	# The mast and a spar of a drowned ship, part of the hoard's history.
	var wood := surface(2, Color(0.12, 0.09, 0.06), Color(0.05, 0.04, 0.03), 0.5, 0.7, 0.4)
	var b := Batch.new()
	b.cyl(0.2, 11.0, Vector3(12.0, LAKE_Y + 4.0, -31.0), Vector3(0, 0, -14), 0.13, 10)
	b.cyl(0.1, 5.0, Vector3(13.3, LAKE_Y + 7.5, -31.0), Vector3(20, 0, 72), -1.0, 8)
	b.box(Vector3(6.0, 1.6, 2.0), Vector3(10.5, LAKE_Y + 0.2, -31.5), Vector3(-8, 20, 14))
	commit(b, wood)


func _ceiling() -> void:
	for i in 34:
		var p := Vector3(_rng.randf_range(-26, 26), 13.0, _rng.randf_range(-7.0, -40.0))
		var drop := _rng.randf_range(3.0, 8.5) * (0.8 if p.z > -12.0 else 1.0)
		var r := drop * _rng.randf_range(0.1, 0.16)
		_rocks.cyl(r, drop, p - Vector3(0, drop * 0.5, 0), Vector3(_rng.randf_range(-4, 4), 0, _rng.randf_range(-4, 4)), 0.02, 8)
	_rocks.box(Vector3(80, 1.0, 50), Vector3(0, 13.4, -25.0))


func _far_wall() -> void:
	_rocks.box(Vector3(200, 50, 2.0), Vector3(0, 20.0, -68.0))
	for i in 9:
		var x := -48.0 + i * 12.0 + _rng.randf_range(-3, 3)
		_rocks.cyl(_rng.randf_range(2.5, 5.0), 40.0, Vector3(x, 18.0, -64.0 + _rng.randf_range(-4, 3)), Vector3.ZERO, _rng.randf_range(1.5, 3.0), 10)
	for x in [-34.0, 34.0]:
		_rocks.box(Vector3(10, 40, 70), Vector3(x + signf(x) * 5.0, 15.0, -32.0), Vector3(0, -signf(x) * 8.0, 0))
	# Tar falls pouring from the dark into the lake, mist rising where they land.
	var fall := shader_mat("res://assets/shaders/stage_fall.gdshader", {"albedo": Color(0.02, 0.025, 0.02),
		"glow": Color(0.4, 0.9, 0.15), "glow_energy": 0.7, "speed": 2.2, "roughness": 0.15, "fray": 0.3})
	for f in [[-30.0, -60.0, 3.5], [-11.0, -61.0, 2.6], [9.0, -60.5, 3.0], [27.0, -61.0, 2.4], [-19.0, -27.0, 1.5], [18.0, -36.0, 1.8]]:
		var q := QuadMesh.new()
		q.size = Vector2(f[2], 18.0)
		var mi := MeshInstance3D.new()
		mi.mesh = q
		mi.material_override = fall
		mi.position = Vector3(f[0], LAKE_Y + 8.8, f[1])
		mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		add_child(mi)
		plume(Vector3(f[0], LAKE_Y + 0.4, f[1] + 0.5), Color(0.35, 0.6, 0.25, 0.4), 12, f[2] * 0.4, 0.7, f[2] * 1.2)


func _ends() -> void:
	# Great spires of rock close the ledge at both ends.
	for side in [-1.0, 1.0]:
		var x: float = side * (FightSim.STAGE_HALF + 1.2)
		_rocks.cyl(1.0, 9.0, Vector3(x, 4.5, -1.6), Vector3(0, 0, -side * 3.0), 0.35, 12)
		_rocks.cyl(0.6, 5.0, Vector3(x + side * 1.2, 2.5, -0.4), Vector3(8, 0, side * 6.0), 0.15, 10)
		_boulder(Vector3(x - side * 0.1, 0.3, -1.3), 1.1)
		_boulder(Vector3(x + side * 0.5, 0.1, 0.9), 0.6)
		var l := omni(Vector3(side * 9.3, 1.6, 0.9), Color(1.0, 0.72, 0.35), 2.0, 4.0, false, 0.8)
		flicker(l, 2.0, 0.06, 2.0)


func _hoard() -> void:
	for m in [[-9.4, -0.3, 1.5, 0.65, 1.1, 0.0], [9.6, -0.1, 1.7, 0.75, 1.2, 0.0], [-11.4, 1.6, 0.9, 0.4, 0.7, 0.0],
			[10.9, 1.9, 0.7, 0.3, 0.6, 0.0], [-6.5, -13.0, 1.6, 0.7, 1.0, LAKE_Y + 0.55], [8.0, -20.0, 2.2, 0.9, 1.4, LAKE_Y + 0.9]]:
		var c := Vector3(m[0], m[5], m[1])
		var rad := Vector3(m[2], m[3], m[4])
		_gold.sphere(1.0, c, rad * Vector3(1.0, 1.0, 1.0), 16)
		var count := int(rad.x * rad.z * 140.0)
		for i in count:
			var th := _rng.randf() * TAU
			var ph := sqrt(_rng.randf()) * PI * 0.5
			var n := Vector3(sin(ph) * cos(th), cos(ph), sin(ph) * sin(th))
			var p := c + n * rad * 0.995
			var up := (n / rad).normalized()
			var basis := Basis(Quaternion(Vector3.UP, up)) * Basis(Vector3.UP, _rng.randf() * TAU)
			_coins.append(Transform3D(basis.rotated(Vector3.RIGHT, _rng.randf_range(-0.4, 0.4)), p))
			if i % 14 == 0:
				_glints.append(p + up * 0.03)
		# Coins spilled on the floor round the pile.
		for i in int(rad.x * 30.0):
			var a := _rng.randf() * TAU
			var d := _rng.randf_range(1.0, 1.6)
			_coins.append(Transform3D(Basis(Vector3.UP, a).rotated(Vector3.RIGHT, _rng.randf_range(-0.1, 0.1)), c + Vector3(cos(a) * rad.x * d, 0.004, sin(a) * rad.z * d)))
	# Treasures in the piles: goblets, a crown, swords, a shield, a chest spilling over.
	for g in [[-9.0, 0.55, 0.2], [9.2, 0.62, 0.35], [-10.2, 0.35, -0.5], [10.6, 0.4, 0.5]]:
		var p := Vector3(g[0], g[1], g[2])
		_gold.cyl(0.02, 0.14, p + Vector3(0, 0.07, 0), Vector3.ZERO, -1.0, 6)
		_gold.cyl(0.06, 0.12, p + Vector3(0, 0.2, 0), Vector3.ZERO, 0.075, 10)
		_gold.cyl(0.05, 0.015, p, Vector3.ZERO, -1.0, 10)
		_glints.append(p + Vector3(0, 0.27, 0.05))
	var crown := TorusMesh.new()
	crown.inner_radius = 0.1
	crown.outer_radius = 0.13
	crown.rings = 16
	crown.ring_segments = 6
	_gold.add(crown, Transform3D(Basis(Vector3.RIGHT, 0.4), Vector3(9.9, 0.74, -0.4)))
	for k in 6:
		var a := TAU * k / 6.0
		_gold.cyl(0.015, 0.09, Vector3(9.9 + cos(a) * 0.115, 0.8 + sin(a) * 0.04, -0.4 + sin(a) * 0.105), Vector3(23, 0, 0), 0.003, 4)
	for s in [[-8.7, 0.6, -0.8, 25.0], [9.0, 0.7, -0.9, -30.0], [-9.8, 0.5, 0.4, -12.0]]:
		_gold.box(Vector3(0.05, 0.9, 0.015), Vector3(s[0], s[1] + 0.3, s[2]), Vector3(0, 0, s[3]))
		_gold.box(Vector3(0.24, 0.035, 0.035), Vector3(s[0] - sin(deg_to_rad(s[3])) * 0.12, s[1] - 0.12, s[2]), Vector3(0, 0, s[3]))
	_gold.cyl(0.36, 0.04, Vector3(10.3, 0.55, -0.9), Vector3(70, 20, 0), -1.0, 16)
	var chest := surface(2, Color(0.2, 0.12, 0.07), Color(0.08, 0.05, 0.03), 0.3, 0.7, 0.2)
	var cb := Batch.new()
	cb.box(Vector3(0.8, 0.45, 0.5), Vector3(-8.3, 0.22, 0.6), Vector3(0, 18, 0))
	cb.box(Vector3(0.82, 0.08, 0.5), Vector3(-8.25, 0.62, 0.4), Vector3(-55, 18, 0))
	commit(cb, chest)
	_gold.sphere(0.36, Vector3(-8.3, 0.45, 0.6), Vector3(1.0, 0.35, 0.6), 12)
	for i in 3:
		_glints.append(Vector3(-8.3 + i * 0.15, 0.6, 0.62))


func _storm_light() -> void:
	# Lights the far cavern (spines, falls, stalactites) from behind; never the ledge or the lake.
	_storm = omni(Vector3(0, 8.0, -30.0), Color(0.6, 1.0, 0.45), 0.0, 30.0, false, 0.3)
	_storm.omni_attenuation = 0.6
	_storm.light_cull_mask = STAGE_LAYER
	var flashes: bool = Settings.get_value("flashes") != false
	if flashes:
		var bolt_mat := emissive(Color(0.7, 1.0, 0.55), 4.0)
		bolt_mat.disable_fog = true
		for i in 3:
			var mi := MeshInstance3D.new()
			mi.mesh = _bolt_mesh(18.0)
			mi.material_override = bolt_mat
			mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
			mi.visible = false
			add_child(mi)
			_bolts.append(mi)
	var state := {"next": _rng.randf_range(3.0, 6.0), "start": -10.0, "bolt": null}
	_animated.append(func(t: float, _d: float):
		if t >= state["next"]:
			state["start"] = t
			state["next"] = t + _rng.randf_range(5.0, 11.0)
			var x := _rng.randf_range(-16.0, 16.0)
			_storm.position = Vector3(x, 8.0, -30.0)
			if flashes:
				var b: MeshInstance3D = _bolts[_rng.randi() % _bolts.size()]
				b.position = Vector3(x, LAKE_Y + 16.0, -38.0)
				b.scale = Vector3(1.0 if _rng.randf() < 0.5 else -1.0, 1.0, 1.0)
				state["bolt"] = b
		var e: float = t - state["start"]
		var energy := 0.0
		if flashes:
			# Two quick pulses and a short tail: bright at the back of the cave, never on screen.
			if e < 0.07:
				energy = 7.0
			elif e < 0.12:
				energy = 1.5
			elif e < 0.2:
				energy = 5.0
			elif e < 0.55:
				energy = 5.0 * (1.0 - (e - 0.2) / 0.35)
			if state["bolt"]:
				(state["bolt"] as MeshInstance3D).visible = e < 0.2 and not (e > 0.07 and e < 0.12)
		elif e < 1.2:
			energy = 2.0 * sin(e / 1.2 * PI)
		_storm.light_energy = energy)


## A jagged bolt hanging down from the origin, with two forks; flat quads facing +z.
func _bolt_mesh(h: float) -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var main := [Vector3.ZERO]
	var p := Vector3.ZERO
	for k in 14:
		p += Vector3(_rng.randf_range(-0.9, 0.9), -h / 14.0, 0)
		main.append(p)
	_bolt_strip(st, main, 0.16)
	for f in 2:
		var at: int = 3 + f * 5
		var fork := [main[at]]
		var q: Vector3 = main[at]
		var dir := 1.0 if f == 0 else -1.0
		for k in 5:
			q += Vector3(dir * _rng.randf_range(0.3, 1.0), -h / 18.0, 0)
			fork.append(q)
		_bolt_strip(st, fork, 0.08)
	return st.commit()


func _bolt_strip(st: SurfaceTool, pts: Array, w: float) -> void:
	for k in pts.size() - 1:
		var a: Vector3 = pts[k]
		var b: Vector3 = pts[k + 1]
		var side := (b - a).cross(Vector3.BACK).normalized() * w * 0.5
		for v in [a - side, a + side, b + side, a - side, b + side, b - side]:
			st.set_normal(Vector3.BACK)
			st.add_vertex(v)


func _cyl_between(b: Batch, p: Vector3, q: Vector3, r_bottom: float, r_top: float) -> void:
	var m := CylinderMesh.new()
	m.top_radius = r_top
	m.bottom_radius = r_bottom
	m.height = p.distance_to(q)
	m.radial_segments = 8
	m.rings = 1
	b.add(m, Transform3D(Basis(Quaternion(Vector3.UP, (q - p).normalized())), (p + q) * 0.5))
