extends Arena
## Lowmere Stilt Market at night, Bram Ketch's beat: a plank boardwalk on stilts over the bog. Rope
## rails run behind the fighting strip under strings of amber paper lanterns; stalls with striped
## awnings close both ends and line a second pier across the water, joined by a footbridge; reeds
## and black water below, stilt houses with lit windows across the bog, a low moon and mist. On the
## right, a raised walkway juts out over the water: where the stranger in white is met.

const DECK_BACK := -2.55
const WATER_Y := -1.7
const PIER_Z := -12.0
const PIER_D := 2.6
const WALK_Y := 1.25
const WALK_Z := -15.5
const PAPER := [Color(1.0, 0.45, 0.12), Color(1.0, 0.5, 0.15), Color(1.0, 0.34, 0.08), Color(0.9, 0.16, 0.06), Color(1.0, 0.66, 0.3)]
const AWNINGS := [[Color(0.42, 0.13, 0.08), Color(0.62, 0.57, 0.46)], [Color(0.1, 0.28, 0.27), Color(0.6, 0.56, 0.45)],
	[Color(0.5, 0.35, 0.1), Color(0.3, 0.12, 0.08)], [Color(0.28, 0.12, 0.2), Color(0.58, 0.53, 0.42)], [Color(0.2, 0.26, 0.12), Color(0.55, 0.5, 0.38)]]

var _rng := RandomNumberGenerator.new()
var _lanterns: Array = []
var _wood: ShaderMaterial
var _dark: ShaderMaterial
var _rope: StandardMaterial3D
var _woods := Batch.new()
var _darks := Batch.new()
var _ropes := Batch.new()
var _cloth := {}
var _streak_pts: Array = []


func build() -> void:
	music = "music_market"
	_rng.seed = 11
	ssr = true
	make_environment({"top": Color(0.008, 0.013, 0.022), "horizon": Color(0.1, 0.13, 0.135), "glow_color": Color(0.55, 0.65, 0.62),
		"glow_dir": Vector3(0.35, 0.15, -1.0), "glow_size": 0.35, "glow_energy": 0.35, "clouds": 0.6,
		"moon_dir": Vector3(0.35, 0.15, -1.0), "moon_size": 0.021, "moon_color": Color(0.85, 0.9, 0.82), "stars": 0.7},
		Color(0.1, 0.125, 0.135), 0.018, 0.38)
	environment.fog_height = WATER_Y + 1.2
	environment.fog_height_density = 0.35
	environment.glow_intensity = 0.75
	make_key(Color(1.0, 0.84, 0.68), 1.4, Vector3(-36, -32, 0))
	make_fill(Color(0.75, 0.82, 0.9), 0.55)
	make_rim(Color(0.6, 0.76, 1.0), 1.6, Vector3(-28, 150, 0))
	_wood = surface(2, Color(0.31, 0.295, 0.27), Color(0.13, 0.12, 0.11), 1.0, 0.82, 0.2)
	_dark = surface(2, Color(0.14, 0.11, 0.085), Color(0.06, 0.05, 0.04), 0.6, 0.75, 0.4)
	_rope = Shapes.material(Color(0.32, 0.26, 0.18), 0.95)
	_water()
	_deck()
	_rail_and_strings()
	_pier()
	_stranger_walk()
	_ends()
	_houses()
	_reeds()
	commit(_woods, _wood)
	commit(_darks, _dark)
	commit(_ropes, _rope, false)
	for c in _cloth:
		commit(_cloth[c], Shapes.material(c, 0.92))
	_commit_lanterns()
	_commit_streaks()
	motes(Vector3(0, WATER_Y + 1.0, -14.0), Vector3(20, 1.0, 9), 40, Color(0.7, 1.0, 0.4, 0.9), 0.06)
	mist(Vector2(60, 14), Vector3(0, WATER_Y + 0.35, -8.0), Color(0.26, 0.31, 0.34), 0.35)
	mist(Vector2(90, 20), Vector3(0, WATER_Y + 0.5, -24.0), Color(0.24, 0.29, 0.32), 0.45)
	mist(Vector2(160, 30), Vector3(0, WATER_Y + 0.8, -50.0), Color(0.22, 0.27, 0.3), 0.55)
	finish()


func _water() -> void:
	var w := plane(Vector2(400, 300), Vector3(0, WATER_Y, -140.0), shader_mat("res://assets/shaders/stage_water.gdshader",
		{"deep": Color(0.008, 0.012, 0.013), "scum": Color(0.06, 0.07, 0.035), "scum_amount": 0.35, "swell": 0.35, "roughness": 0.05}))
	w.layers = FLOOR_LAYER
	# The moon's path across the water, and the far bank: low dark hills and dead trees.
	_streak_pts.append([Vector3(30.0, WATER_Y + 0.01, -90.0), Vector3(9.0, WATER_Y + 0.01, -22.0), 7.0, 2.0, Color(0.32, 0.36, 0.34)])
	var bank := Shapes.material(Color(0.02, 0.025, 0.025), 1.0)
	var b := Batch.new()
	for i in 14:
		var x := -110.0 + i * 17.0 + _rng.randf_range(-5, 5)
		b.sphere(_rng.randf_range(9, 16), Vector3(x, WATER_Y - 4.0, -95.0 - _rng.randf_range(0, 15)), Vector3(1.6, 0.42, 0.6), 12)
	commit(b, bank, false)
	var trees := Batch.new()
	for t in [[-34.0, -62.0], [-21.0, -70.0], [16.0, -58.0], [41.0, -66.0], [-48.0, -55.0], [27.0, -75.0]]:
		_dead_tree(trees, Vector3(t[0], WATER_Y, t[1]), _rng.randf_range(5.0, 8.0))
	commit(trees, bank, false)


func _dead_tree(b: Batch, base: Vector3, h: float) -> void:
	b.cyl(0.25, h, base + Vector3(0, h * 0.5, 0), Vector3(0, 0, _rng.randf_range(-6, 6)), 0.06, 6)
	for k in 4:
		var y := h * (0.45 + k * 0.13)
		var side := 1.0 if k % 2 == 0 else -1.0
		var l := h * (0.35 - k * 0.05)
		b.cyl(0.08, l, base + Vector3(side * l * 0.4, y + l * 0.25, 0), Vector3(0, _rng.randf_range(-30, 30), -side * 55), 0.02, 5)


func _deck() -> void:
	var deck := box(Vector3(30, 0.22, 7.2), Vector3(0, -0.11, (DECK_BACK + 4.65) * 0.5), _wood)
	deck.layers = FLOOR_LAYER
	_darks.box(Vector3(30, 0.3, 0.16), Vector3(0, -0.3, DECK_BACK - 0.02))
	_darks.box(Vector3(30, 0.3, 0.16), Vector3(0, -0.3, 4.6))
	for i in 18:
		var x := -14.0 + i * 1.75
		for z in [DECK_BACK + 0.05, 4.55]:
			_darks.cyl(0.11, 2.2, Vector3(x, WATER_Y + 0.9, z), Vector3(_rng.randf_range(-2, 2), 0, _rng.randf_range(-2, 2)), -1.0, 8)
	# Cross-bracing between the back stilts.
	for i in 17:
		var x := -14.0 + i * 1.75 + 0.875
		_darks.box(Vector3(1.95, 0.08, 0.05), Vector3(x, WATER_Y + 0.8, DECK_BACK + 0.1), Vector3(0, 0, 28 if i % 2 == 0 else -28))
	# Clutter on the deck, low and towards the sides.
	_basket(Vector3(-4.6, 0, -1.95), 0.28, 0.35, true)
	_basket(Vector3(-4.05, 0, -2.1), 0.22, 0.28, false)
	_basket(Vector3(5.2, 0, -2.0), 0.3, 0.3, true)
	_coil(Vector3(3.3, 0.0, -2.05))
	_coil(Vector3(-5.6, 0.0, 2.4))
	for t in [[-2.9, -2.05, 15.0], [6.1, -1.85, -20.0]]:
		_trap(Vector3(t[0], 0, t[1]), t[2])
	_crate(Vector3(4.6, 0, 2.6), 0.42, 12.0)
	_basket(Vector3(-6.2, 0, 2.9), 0.25, 0.3, true)
	# A lantern set down on the boards at the front.
	_lanterns.append([Transform3D(Basis(), Vector3(-3.6, 0.42, 2.7)), PAPER[0]])
	_darks.cyl(0.12, 0.04, Vector3(-3.6, 0.02, 2.7), Vector3.ZERO, -1.0, 10)


func _rail_and_strings() -> void:
	var z := DECK_BACK + 0.12
	var poles := [-12.25, -7.0, -1.75, 3.5, 8.75, 14.0]
	for i in 17:
		var x := -14.0 + i * 1.75
		var tall := poles.has(x)
		_darks.box(Vector3(0.13, 3.45 if tall else 1.1, 0.13), Vector3(x, 1.72 if tall else 0.55, z))
		_darks.box(Vector3(0.18, 0.06, 0.18), Vector3(x, 1.12, z))
		if tall:
			_darks.box(Vector3(0.7, 0.08, 0.1), Vector3(x, 3.3, z))
		if i < 16:
			_rope_span(Vector3(x, 1.02, z), Vector3(x + 1.75, 1.02, z), 0.14)
			_rope_span(Vector3(x, 0.55, z), Vector3(x + 1.75, 0.55, z), 0.1)
	# Nets drying over the rail.
	for n in [[-9.0, 2.2], [4.2, 1.6]]:
		_net(Vector3(n[0], 0.62, z + 0.06), Vector2(n[1], 0.95), 0.0)
	for i in poles.size() - 1:
		var a := Vector3(poles[i], 3.25, z)
		var c := Vector3(poles[i + 1], 3.25, z)
		_lantern_string(a, c, 0.45)
	# Strings running back over the water to the stall pier.
	_lantern_string(Vector3(-7.0, 3.25, z), Vector3(-11.6, 2.7, PIER_Z + 0.2), 0.8)
	_lantern_string(Vector3(-1.75, 3.25, z), Vector3(-5.3, 2.7, PIER_Z + 0.2), 0.8)
	_lantern_string(Vector3(3.5, 3.25, z), Vector3(0.4, 2.7, PIER_Z + 0.2), 0.8)
	for x in [-7.0, -1.75, 3.5]:
		var l := omni(Vector3(x + 2.6, 2.75, z + 0.3), Color(1.0, 0.6, 0.3), 1.9, 6.5, false, 0.4)
		flicker(l, 1.9, 0.08, 3.0)


func _lantern_string(a: Vector3, c: Vector3, sag: float) -> void:
	_rope_span(a, c, sag, 0.012, 14)
	var n := int(a.distance_to(c) / 0.9)
	for k in range(1, n):
		var t := float(k) / n
		var p := a.lerp(c, t) + Vector3(0, -sag * 4.0 * t * (1.0 - t) - 0.02, 0)
		_lanterns.append([Transform3D(Basis(), p), PAPER[_rng.randi() % PAPER.size()]])


## A rope sagging from `a` to `c` (a parabola), added to the rope batch; returns its points.
func _rope_span(a: Vector3, c: Vector3, sag: float, r := 0.016, segs := 8) -> Array:
	var pts := []
	for k in segs + 1:
		var t := float(k) / segs
		pts.append(a.lerp(c, t) + Vector3(0, -sag * 4.0 * t * (1.0 - t), 0))
	for k in segs:
		_cyl_between(_ropes, pts[k], pts[k + 1], r)
	return pts


func _cyl_between(b: Batch, p: Vector3, q: Vector3, r: float) -> void:
	var m := CylinderMesh.new()
	m.top_radius = r
	m.bottom_radius = r
	m.height = p.distance_to(q)
	m.radial_segments = 5
	m.rings = 1
	b.add(m, Transform3D(Basis(Quaternion(Vector3.UP, (q - p).normalized())), (p + q) * 0.5))


func _pier() -> void:
	var x0 := -22.0
	var x1 := 1.6
	var pier := box(Vector3(x1 - x0, 0.2, PIER_D), Vector3((x0 + x1) * 0.5, 0.0, PIER_Z - PIER_D * 0.5), _wood)
	pier.layers = FLOOR_LAYER
	var x := x0 + 0.6
	while x < x1:
		for zz in [PIER_Z + 0.05, PIER_Z - PIER_D + 0.05]:
			_darks.cyl(0.1, 2.0, Vector3(x, WATER_Y + 0.85, zz), Vector3.ZERO, -1.0, 8)
		x += 1.9
	# Stalls along the pier, facing the fight.
	var stalls := [[-19.6, 2.4, "jars"], [-16.6, 2.6, "fish"], [-13.5, 2.5, "produce"], [-10.4, 2.6, "cloth"], [-7.2, 2.4, "fish"], [-4.0, 2.7, "produce"], [-0.9, 2.2, "jars"]]
	for i in stalls.size():
		var s: Array = stalls[i]
		_stall(Vector3(s[0], 0.1, PIER_Z - 0.35), s[1], 1.6, AWNINGS[i % AWNINGS.size()], s[2])
	for lx in [-15.0, -8.0, -2.0]:
		var l := omni(Vector3(lx, 2.0, PIER_Z - 0.4), Color(1.0, 0.58, 0.28), 1.6, 5.5, false, 0.4)
		flicker(l, 1.6, 0.1, 4.0)
	# The footbridge from the boardwalk to the pier, on the left.
	var bx := -12.6
	var span := DECK_BACK - PIER_Z
	var bridge := box(Vector3(1.4, 0.12, span + 0.2), Vector3(bx, -0.05, (DECK_BACK + PIER_Z) * 0.5), _wood)
	bridge.layers = FLOOR_LAYER
	for side in [-0.7, 0.7]:
		for k in 4:
			var zz := DECK_BACK - k * span / 3.0
			_darks.box(Vector3(0.09, 1.0, 0.09), Vector3(bx + side, 0.45, zz))
			_darks.cyl(0.09, 2.0, Vector3(bx + side, WATER_Y + 0.8, zz), Vector3.ZERO, -1.0, 8)
		_rope_span(Vector3(bx + side, 0.92, DECK_BACK), Vector3(bx + side, 0.92, PIER_Z), 0.25)


func _stall(o: Vector3, w: float, d: float, colors: Array, goods: String) -> void:
	for sx in [-0.5, 0.5]:
		_darks.box(Vector3(0.1, 2.1, 0.1), o + Vector3(sx * w, 1.05, 0))
		_darks.box(Vector3(0.1, 2.55, 0.1), o + Vector3(sx * w, 1.27, -d))
		_woods.box(Vector3(0.05, 1.1, d), o + Vector3(sx * w, 0.55, -d * 0.5))
	_woods.box(Vector3(w, 2.5, 0.06), o + Vector3(0, 1.25, -d))
	_woods.box(Vector3(w + 0.1, 0.06, 0.56), o + Vector3(0, 0.88, -0.25))
	_woods.box(Vector3(w, 0.85, 0.05), o + Vector3(0, 0.42, -0.02))
	# Striped awning from the back wall out past the front posts, with a valance.
	var run := d + 0.55
	var drop := 0.55
	var ang := rad_to_deg(atan2(drop, run))
	var length := sqrt(run * run + drop * drop)
	var n := 7
	for k in n:
		var c: Color = colors[k % 2]
		var sx := -w * 0.5 + (k + 0.5) * w / n
		_cloth_box(c, Vector3(w / n + 0.002, 0.025, length), o + Vector3(sx, 2.55 - drop * 0.5, -d + run * 0.5), Vector3(ang, 0, 0))
		_cloth_box(c, Vector3(w / n + 0.002, 0.2, 0.02), o + Vector3(sx, 2.0 - 0.1, 0.55), Vector3.ZERO)
	_woods.box(Vector3(w * 0.55, 0.3, 0.04), o + Vector3(0, 2.75, -d + 0.05))
	_lanterns.append([Transform3D(Basis(), o + Vector3(0, 2.15, -d * 0.45)), PAPER[_rng.randi() % 3]])
	var top := o + Vector3(0, 0.91, -0.25)
	match goods:
		"fish":
			_cloth_box(Color(0.55, 0.6, 0.62), Vector3(w * 0.7, 0.1, 0.42), top + Vector3(0, 0.05, 0), Vector3.ZERO)
			for k in 7:
				_fish(top + Vector3(-w * 0.3 + k * w * 0.1, 0.13, _rng.randf_range(-0.1, 0.1)), Vector3(0, _rng.randf_range(-30, 30), 90), 0.9)
			for k in int(w / 0.28):
				var fx := -w * 0.5 + 0.2 + k * 0.28
				_cyl_between(_ropes, o + Vector3(fx, 1.98, 0.3), o + Vector3(fx, 1.72, 0.3), 0.006)
				_fish(o + Vector3(fx, 1.55, 0.3), Vector3(0, 0, 0), 1.2)
		"produce":
			for k in 3:
				var bp := top + Vector3(-w * 0.3 + k * w * 0.3, 0, 0)
				_basket(bp, 0.24, 0.18, false)
				var col: Color = [Color(0.45, 0.12, 0.22), Color(0.6, 0.38, 0.1), Color(0.5, 0.48, 0.3)][k]
				for j in 7:
					var a := TAU * j / 7.0
					_cloth_sphere(col, bp + Vector3(cos(a) * 0.12, 0.2, sin(a) * 0.12), 0.07)
			_crate(o + Vector3(w * 0.35, 0, 0.35), 0.4, 8.0)
		"jars":
			var glow := emissive(Color(0.5, 0.95, 0.2), 1.2)
			for k in 8:
				var jp := top + Vector3(-w * 0.38 + k * w * 0.11, 0.1, _rng.randf_range(-0.1, 0.1))
				if k % 3 == 1:
					cyl(0.055, 0.2, jp, glow, Vector3.ZERO, -1.0, 10, false)
				else:
					_cloth_cyl(Color(0.3, 0.32, 0.22), 0.055, 0.2, jp)
			_woods.box(Vector3(w * 0.9, 0.04, 0.3), o + Vector3(0, 1.5, -d + 0.18))
			for k in 6:
				_cloth_cyl(Color(0.25, 0.3, 0.2), 0.05, 0.22, o + Vector3(-w * 0.35 + k * w * 0.14, 1.63, -d + 0.18))
		"cloth":
			for k in 4:
				var col: Color = [Color(0.4, 0.1, 0.1), Color(0.15, 0.2, 0.35), Color(0.55, 0.5, 0.4), Color(0.2, 0.28, 0.15)][k]
				var bolt := CylinderMesh.new()
				bolt.top_radius = 0.09
				bolt.bottom_radius = 0.09
				bolt.height = 0.55
				bolt.radial_segments = 10
				_cloth_batch(col).add(bolt, Arena.xf(top + Vector3(-w * 0.3 + k * w * 0.2, 0.09, 0), Vector3(90, 15 * k, 0)))
			_net(o + Vector3(0, 1.4, -d + 0.06), Vector2(w * 0.8, 1.0), 0.0)


func _stranger_walk() -> void:
	# A narrow walkway raised above the pier, jutting out over the water: kept empty and lit by
	# a single pale lamp, where a figure could stand.
	var x0 := 3.4
	var x1 := 15.5
	var walk := box(Vector3(x1 - x0, 0.16, 1.6), Vector3((x0 + x1) * 0.5, WALK_Y, WALK_Z), _wood)
	walk.layers = FLOOR_LAYER
	var x := x0 + 0.4
	while x < x1:
		for zz in [WALK_Z + 0.75, WALK_Z - 0.75]:
			_darks.cyl(0.1, WALK_Y - WATER_Y, Vector3(x, (WALK_Y + WATER_Y) * 0.5, zz), Vector3.ZERO, -1.0, 8)
		_darks.box(Vector3(0.09, 1.0, 0.09), Vector3(x, WALK_Y + 0.5, WALK_Z - 0.75))
		if x + 1.8 < x1:
			_rope_span(Vector3(x, WALK_Y + 0.95, WALK_Z - 0.75), Vector3(x + 1.8, WALK_Y + 0.95, WALK_Z - 0.75), 0.12)
		x += 1.8
	# Steps up from the end of the stall pier.
	for k in 6:
		_woods.box(Vector3(0.5, 0.08, 1.2), Vector3(1.8 + k * 0.3, 0.1 + k * WALK_Y / 6.0, WALK_Z + 0.2))
	_woods.box(Vector3(1.4, 0.12, 2.6), Vector3(1.6, 0.05, PIER_Z - PIER_D - 0.9))
	_darks.box(Vector3(0.12, 3.2, 0.12), Vector3(13.6, WALK_Y + 1.6, WALK_Z - 0.6))
	_darks.box(Vector3(0.6, 0.07, 0.07), Vector3(13.35, WALK_Y + 3.15, WALK_Z - 0.6))
	var lamp := sphere(0.14, Vector3(13.1, WALK_Y + 2.85, WALK_Z - 0.6), emissive(Color(0.75, 0.8, 1.0), 2.2), Vector3(1, 1.25, 1))
	lamp.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var cold := omni(Vector3(12.6, WALK_Y + 2.6, WALK_Z - 0.2), Color(0.72, 0.78, 1.0), 1.4, 5.0, false, 0.3)
	flicker(cold, 1.4, 0.04, 1.5)
	_streak_pts.append([Vector3(13.1, WATER_Y + 0.01, WALK_Z - 0.3), Vector3(12.4, WATER_Y + 0.01, WALK_Z + 7.0), 0.4, 0.9, Color(0.4, 0.44, 0.6)])


func _ends() -> void:
	# Left: a vegetable stall with crates stacked against its post. Right: a fishmonger's, barrels.
	_stall(Vector3(-9.5, 0.0, 0.5), 2.6, 1.6, AWNINGS[2], "produce")
	_stall(Vector3(9.5, 0.0, 0.5), 2.6, 1.6, AWNINGS[1], "fish")
	for c in [[-7.65, 0.0, -1.0, 0.56, 5.0], [-7.7, 0.0, -0.35, 0.5, -8.0], [-7.75, 0.56, -0.7, 0.5, 14.0], [-7.6, 0.0, 0.3, 0.46, 20.0], [-7.7, 1.06, -0.75, 0.42, -4.0]]:
		_crate(Vector3(c[0], c[1], c[2]), c[3], c[4])
	var barrel := surface(2, Color(0.3, 0.2, 0.12), Color(0.14, 0.09, 0.06), 0.4, 0.75, 0.2)
	var bb := Batch.new()
	for b in [[7.75, -1.0, 0.0], [7.8, -0.25, 0.0], [7.7, 0.45, 0.0], [7.75, -0.6, 0.86]]:
		bb.cyl(0.3, 0.86, Vector3(b[0], b[2] + 0.43, b[1]), Vector3.ZERO, -1.0, 16)
		_darks.cyl(0.315, 0.05, Vector3(b[0], b[2] + 0.15, b[1]), Vector3.ZERO, -1.0, 16)
		_darks.cyl(0.315, 0.05, Vector3(b[0], b[2] + 0.71, b[1]), Vector3.ZERO, -1.0, 16)
	commit(bb, barrel)
	_net(Vector3(8.9, 1.25, -1.05), Vector2(1.4, 1.5), 0.0)
	for side in [-1.0, 1.0]:
		var l := omni(Vector3(side * 9.5, 1.9, -0.2), Color(1.0, 0.58, 0.28), 1.5, 4.5, false, 0.3)
		flicker(l, 1.5, 0.1, 4.0)


func _houses() -> void:
	var walls := Batch.new()
	var roofs := Batch.new()
	var lit := Batch.new()
	var dim := Batch.new()
	var specs := [[-44.0, -52.0], [-33.0, -40.0], [-25.0, -47.0], [-16.0, -36.0], [-9.0, -50.0], [-1.0, -42.0], [7.0, -54.0],
		[12.0, -38.0], [20.0, -46.0], [28.0, -36.0], [35.0, -50.0], [46.0, -42.0], [-52.0, -38.0], [55.0, -56.0]]
	for s in specs:
		var x: float = s[0] + _rng.randf_range(-2, 2)
		var z: float = s[1]
		var w := _rng.randf_range(3.0, 5.5)
		var h := _rng.randf_range(2.2, 3.4)
		var d := _rng.randf_range(2.6, 3.6)
		var legs := _rng.randf_range(1.6, 2.6)
		var base := WATER_Y + legs
		var tilt := _rng.randf_range(-3.0, 3.0)
		for lx in [-0.42, 0.42]:
			for lz in [-0.4, 0.4]:
				_darks.cyl(0.12, legs + 0.3, Vector3(x + lx * w, WATER_Y + legs * 0.5, z + lz * d), Vector3.ZERO, -1.0, 6)
		walls.box(Vector3(w + 0.8, 0.15, d + 0.8), Vector3(x, base, z))
		walls.box(Vector3(w, h, d), Vector3(x, base + h * 0.5, z), Vector3(0, 0, tilt))
		var r := maxf(w, d) * 0.62
		if _rng.randf() < 0.5:
			roofs.box(Vector3(w + 0.5, r, r), Vector3(x, base + h, z), Vector3(45, 0, tilt))
		else:
			roofs.box(Vector3(r, r, d + 0.5), Vector3(x, base + h, z), Vector3(0, 0, 45 + tilt))
		roofs.box(Vector3(0.4, 1.4, 0.4), Vector3(x + w * 0.3, base + h + 0.7, z - 0.3))
		var nwin := _rng.randi_range(1, 3)
		for k in nwin:
			var wx := x - w * 0.35 + k * w * 0.35
			var wy := base + h * 0.5 + _rng.randf_range(-0.2, 0.3)
			var on := _rng.randf() < 0.7
			(lit if on else dim).box(Vector3(0.5, 0.65, 0.06), Vector3(wx, wy, z + d * 0.5 + 0.02))
			if on:
				_streak_pts.append([Vector3(wx, WATER_Y + 0.01, z + d * 0.5 + 0.3), Vector3(wx + x * 0.04, WATER_Y + 0.01, z + d * 0.5 + 9.0), 0.5, 1.4, Color(0.5, 0.31, 0.15)])
		if _rng.randf() < 0.4:
			_lanterns.append([Transform3D(Basis.from_scale(Vector3.ONE * 1.4), Vector3(x + w * 0.5 + 0.2, base + 1.8, z + d * 0.5)), PAPER[_rng.randi() % 3]])
	var plaster := surface(2, Color(0.2, 0.19, 0.17), Color(0.1, 0.095, 0.085), 0.7, 0.85, 0.2)
	commit(walls, plaster)
	commit(roofs, surface(0, Color(0.09, 0.085, 0.08), Color(0.04, 0.04, 0.035), 1.0, 0.9, 0.3))
	commit(lit, emissive(Color(1.0, 0.62, 0.28), 2.6), false)
	commit(dim, Shapes.material(Color(0.03, 0.03, 0.03), 0.4), false)


func _reeds() -> void:
	var clump := _reed_clump()
	var xforms := []
	for i in 90:
		var p := Vector3(_rng.randf_range(-16, 16), WATER_Y, _rng.randf_range(DECK_BACK - 0.4, PIER_Z + 0.3))
		if absf(p.x + 12.6) < 1.2:
			continue
		xforms.append(Arena.xf(p, Vector3(0, _rng.randf() * 360.0, 0), Vector3.ONE * _rng.randf_range(0.7, 1.15)))
	for i in 40:
		var p := Vector3(_rng.randf_range(-24, 24), WATER_Y, _rng.randf_range(PIER_Z - PIER_D - 0.3, -22.0))
		xforms.append(Arena.xf(p, Vector3(0, _rng.randf() * 360.0, 0), Vector3.ONE * _rng.randf_range(0.8, 1.3)))
	for i in 50:
		var p := Vector3(_rng.randf_range(-60, 60), WATER_Y, _rng.randf_range(-24.0, -60.0))
		xforms.append(Arena.xf(p, Vector3(0, _rng.randf() * 360.0, 0), Vector3.ONE * _rng.randf_range(1.0, 1.8)))
	for i in 14:
		var p := Vector3(_rng.randf_range(-12, 12), WATER_Y, _rng.randf_range(5.0, 7.0))
		xforms.append(Arena.xf(p, Vector3(0, _rng.randf() * 360.0, 0), Vector3.ONE * _rng.randf_range(0.6, 0.85)))
	var m := shader_mat("res://assets/shaders/stage_sway.gdshader", {"albedo": Color(0.2, 0.22, 0.1), "hanging": false,
		"sway": 0.035, "speed": 0.9, "roughness": 0.85})
	multi(clump, xforms, m)


## Nine blades with a few bulrush heads, standing on the origin (vertex colour darkens the heads).
func _reed_clump() -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var rng := RandomNumberGenerator.new()
	rng.seed = 5
	for k in 9:
		var a := rng.randf() * TAU
		var base := Vector3(cos(a), 0, sin(a)) * rng.randf_range(0.0, 0.25)
		var h := rng.randf_range(1.2, 2.4)
		var lean := Vector3(cos(a), 0, sin(a)) * rng.randf_range(0.1, 0.45)
		var side := Vector3(-sin(a), 0, cos(a)) * 0.025
		var prev_l := base - side
		var prev_r := base + side
		for s in range(1, 5):
			var t := s / 4.0
			var c := base + Vector3(0, h * t, 0) + lean * t * t
			var wv := side * (1.0 - t * 0.9)
			var l := c - wv
			var r := c + wv
			for v in [prev_l, prev_r, r, prev_l, r, l]:
				st.set_color(Color(0.9, 1.0, 0.8))
				st.set_normal(Vector3(cos(a), 0.3, sin(a)).normalized())
				st.add_vertex(v)
			prev_l = l
			prev_r = r
		if k % 3 == 0:
			var top := base + Vector3(0, h * 0.8, 0) + lean * 0.64
			var cm := CylinderMesh.new()
			cm.top_radius = 0.035
			cm.bottom_radius = 0.035
			cm.height = 0.22
			cm.radial_segments = 6
			cm.rings = 1
			var arr := cm.get_mesh_arrays()
			var verts: PackedVector3Array = arr[Mesh.ARRAY_VERTEX]
			var norms: PackedVector3Array = arr[Mesh.ARRAY_NORMAL]
			var idx: PackedInt32Array = arr[Mesh.ARRAY_INDEX]
			for i in idx:
				st.set_color(Color(0.55, 0.32, 0.18))
				st.set_normal(norms[i])
				st.add_vertex(verts[i] + top)
	return st.commit()


func _commit_lanterns() -> void:
	var paper := SphereMesh.new()
	paper.radius = 0.13
	paper.height = 0.3
	paper.radial_segments = 12
	paper.rings = 8
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	st.append_from(paper, 0, Transform3D(Basis(), Vector3(0, -0.28, 0)))
	var body := st.commit()
	var caps := Batch.new()
	caps.cyl(0.065, 0.045, Vector3(0, -0.13, 0), Vector3.ZERO, -1.0, 8)
	caps.cyl(0.065, 0.045, Vector3(0, -0.43, 0), Vector3.ZERO, -1.0, 8)
	caps.cyl(0.006, 0.12, Vector3(0, -0.06, 0), Vector3.ZERO, -1.0, 4)
	caps.cyl(0.012, 0.1, Vector3(0, -0.5, 0), Vector3.ZERO, -1.0, 4)
	var cap_mesh := caps.st.commit()
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.use_colors = true
	mm.mesh = body
	mm.instance_count = _lanterns.size()
	var xforms := []
	var halos := []
	for i in _lanterns.size():
		var xf: Transform3D = _lanterns[i][0]
		mm.set_instance_transform(i, xf)
		mm.set_instance_color(i, (_lanterns[i][1] as Color).srgb_to_linear())
		xforms.append(xf)
		halos.append(xf.origin + Vector3(0, -0.28, 0) * xf.basis.get_scale().y)
	var mi := MultiMeshInstance3D.new()
	mi.multimesh = mm
	mi.material_override = shader_mat("res://assets/shaders/stage_sway.gdshader", {"albedo": Color(0.1, 0.07, 0.05),
		"emission": Color(1, 1, 1), "emission_energy": 1.0, "ribs": 24.0, "sway": 0.05, "speed": 1.1, "roughness": 0.9})
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	mi.extra_cull_margin = 1.0
	add_child(mi)
	multi(cap_mesh, xforms, shader_mat("res://assets/shaders/stage_sway.gdshader", {"albedo": Color(0.08, 0.06, 0.05),
		"sway": 0.05, "speed": 1.1, "roughness": 0.6}))
	sprites(halos, Vector2(1.1, 1.1), Color(1.0, 0.42, 0.12), 0.2, 2, 0.02)


func _commit_streaks() -> void:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	for s in _streak_pts:
		var from: Vector3 = s[0]
		var to: Vector3 = s[1]
		var dir := (to - from).normalized()
		var side := Vector3.UP.cross(dir).normalized()
		var corners := [[from - side * s[2] * 0.5, Vector2(0, 0)], [from + side * s[2] * 0.5, Vector2(1, 0)],
			[to + side * s[3] * 0.5, Vector2(1, 1)], [to - side * s[3] * 0.5, Vector2(0, 1)]]
		for i in [0, 1, 2, 0, 2, 3]:
			st.set_color((s[4] as Color).srgb_to_linear())
			st.set_uv(corners[i][1])
			st.set_normal(Vector3.UP)
			st.add_vertex(corners[i][0])
	var mi := MeshInstance3D.new()
	mi.mesh = st.commit()
	mi.material_override = shader_mat("res://assets/shaders/stage_shaft.gdshader", {"color": Color(1, 1, 1),
		"energy": 1.8, "ripple": 1.0, "softness": 0.001, "fade": 0.012})
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(mi)


# --- Props -------------------------------------------------------------------------------------

func _cloth_batch(c: Color) -> Batch:
	if not _cloth.has(c):
		_cloth[c] = Batch.new()
	return _cloth[c]


func _cloth_box(c: Color, size: Vector3, pos: Vector3, rot: Vector3) -> void:
	_cloth_batch(c).box(size, pos, rot)


func _cloth_sphere(c: Color, pos: Vector3, r: float) -> void:
	_cloth_batch(c).sphere(r, pos, Vector3.ONE, 8)


func _cloth_cyl(c: Color, r: float, h: float, pos: Vector3) -> void:
	_cloth_batch(c).cyl(r, h, pos, Vector3.ZERO, -1.0, 10)


func _fish(pos: Vector3, rot: Vector3, s: float) -> void:
	var silver := Color(0.42, 0.46, 0.48)
	var b := _cloth_batch(silver)
	var body := SphereMesh.new()
	body.radius = 0.05 * s
	body.height = 0.1 * s
	body.radial_segments = 8
	body.rings = 4
	var basis := Basis.from_euler(rot * (PI / 180.0))
	b.add(body, Transform3D(basis * Basis.from_scale(Vector3(0.45, 2.6, 1.0)), pos))
	var tail := BoxMesh.new()
	tail.size = Vector3(0.012, 0.06, 0.08) * s
	b.add(tail, Transform3D(basis, pos + basis * Vector3(0, -0.15 * s, 0)))


func _basket(pos: Vector3, r: float, h: float, lid: bool) -> void:
	var wicker := Color(0.36, 0.27, 0.15)
	_cloth_batch(wicker).cyl(r * 0.85, h, pos + Vector3(0, h * 0.5, 0), Vector3.ZERO, r, 12)
	_darks.cyl(r + 0.01, 0.03, pos + Vector3(0, h, 0), Vector3.ZERO, -1.0, 12)
	if lid:
		_cloth_batch(wicker).sphere(r * 0.95, pos + Vector3(0, h, 0), Vector3(1, 0.3, 1), 10)


func _coil(pos: Vector3) -> void:
	for k in 3:
		var t := TorusMesh.new()
		t.inner_radius = 0.16 - k * 0.02
		t.outer_radius = 0.2 - k * 0.02
		t.rings = 16
		t.ring_segments = 6
		_ropes.add(t, Transform3D(Basis(), pos + Vector3(0, 0.02 + k * 0.035, 0)))


func _trap(pos: Vector3, yaw: float) -> void:
	var wicker := Color(0.3, 0.23, 0.13)
	_cloth_batch(wicker).cyl(0.2, 0.7, pos + Vector3(0, 0.2, 0), Vector3(90, yaw, 0), 0.12, 10)
	var basis := Basis.from_euler(Vector3(PI * 0.5, deg_to_rad(yaw), 0))
	for k in 3:
		var t := TorusMesh.new()
		t.inner_radius = 0.13 + k * 0.03
		t.outer_radius = 0.15 + k * 0.03
		t.rings = 12
		t.ring_segments = 4
		_darks.add(t, Transform3D(basis * Basis(Vector3.RIGHT, PI * 0.5), pos + Vector3(0, 0.2, 0) + basis * Vector3(0, -0.25 + k * 0.25, 0)))


func _crate(pos: Vector3, s: float, yaw: float) -> void:
	_woods.box(Vector3(s, s, s), pos + Vector3(0, s * 0.5, 0), Vector3(0, yaw, 0))
	var basis := Basis(Vector3.UP, deg_to_rad(yaw))
	for e in [-0.5, 0.5]:
		var m := BoxMesh.new()
		m.size = Vector3(s + 0.02, 0.05, s + 0.02)
		_darks.add(m, Transform3D(basis, pos + Vector3(0, s * 0.5 + e * s * 0.8, 0)))


func _net(pos: Vector3, size: Vector2, yaw: float) -> void:
	var m := StandardMaterial3D.new()
	m.albedo_color = Color(0.2, 0.18, 0.15)
	m.albedo_texture = _net_texture()
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR
	m.alpha_scissor_threshold = 0.5
	m.alpha_antialiasing_mode = BaseMaterial3D.ALPHA_ANTIALIASING_ALPHA_TO_COVERAGE
	m.cull_mode = BaseMaterial3D.CULL_DISABLED
	m.uv1_scale = Vector3(size.x * 6.0, size.y * 6.0, 1)
	var mi := MeshInstance3D.new()
	var q := QuadMesh.new()
	q.size = size
	mi.mesh = q
	mi.material_override = m
	mi.position = pos
	mi.rotation_degrees = Vector3(0, yaw, 0)
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(mi)
	for k in 3:
		_cloth_sphere(Color(0.5, 0.36, 0.18), pos + Vector3(-size.x * 0.35 + k * size.x * 0.35, size.y * 0.5 - 0.05, 0.02), 0.045)


static func _net_texture() -> ImageTexture:
	var img := Image.create(32, 32, false, Image.FORMAT_RGBA8)
	for y in 32:
		for x in 32:
			var a := 1.0 if absf(fmod(x + y, 16.0) - 8.0) > 7.0 or absf(fmod(x - y + 32.0, 16.0) - 8.0) > 7.0 else 0.0
			img.set_pixel(x, y, Color(1, 1, 1, a))
	img.generate_mipmaps()
	return ImageTexture.create_from_image(img)
