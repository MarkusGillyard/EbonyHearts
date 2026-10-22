extends Arena
## The Drowned Chapel: Lowmere's sunken church, Mirela Sorrow's home. The fight crosses the nave
## near its west end; the camera looks down the flooded nave between two arcades of clustered
## piers towards the altar and a great rose window full of cold moonlight. A thin sheet of water
## lies over the flags (feet stay visible), broken pews stand in it, hundreds of candles burn on
## racks, ledges, the altar steps and little floating boards, and censers swing on long chains.
## Candle flames are one MultiMesh of sprites; a reflection probe renders the static nave once so
## the water mirrors the window and the candles.

const COL_X := 7.7
const PIERS := [-0.9, -5.3, -9.7, -14.1, -18.5, -22.9, -27.3, -31.7]
const BACK_Z := -31.8
const SPRING := 6.2
const WALL_TOP := 10.2
const AISLE_X := 11.6
const FOG := Color(0.06, 0.075, 0.1)
const ROSE_Y := 6.1

var _wax: Array = []
var _wicks: Array = []
var _flame_sizes: Array = []
var _stone: ShaderMaterial
var _rng := RandomNumberGenerator.new()


func build() -> void:
	music = "music_chapel"
	_rng.seed = 7
	ssr = true
	make_environment({"top": Color(0.02, 0.03, 0.045), "horizon": Color(0.07, 0.085, 0.11), "glow_color": Color(0.5, 0.65, 0.9),
		"glow_dir": Vector3(0, 0.3, -1.0), "glow_size": 0.3, "glow_energy": 0.4, "clouds": 0.0},
		FOG, 0.028, 0.28)
	environment.fog_height = 0.5
	environment.fog_height_density = 0.1
	environment.glow_intensity = 0.8
	environment.glow_bloom = 0.08
	make_key(Color(1.0, 0.8, 0.62), 1.25, Vector3(-38, -28, 0))
	make_fill(Color(0.72, 0.8, 0.92), 0.5)
	make_rim(Color(0.62, 0.76, 1.0), 1.5, Vector3(-24, 180, 0))
	_stone = surface(1, Color(0.3, 0.33, 0.38), Color(0.15, 0.165, 0.2), 1.5, 0.85, 0.0)
	_floor()
	_arcades()
	_aisles()
	_east_end()
	_pews()
	_racks()
	_floating_candles()
	_censers()
	_banners()
	_ends()
	_commit_candles()
	motes(Vector3(0, 3.0, -21.0), Vector3(3.0, 3.0, 8.0), 200, Color(0.75, 0.85, 1.0, 0.8), 0.035)
	motes(Vector3(0, 2.0, -6.0), Vector3(8.0, 1.8, 4.0), 60, Color(1.0, 0.8, 0.55, 0.5), 0.022)
	var rp := probe(Vector3(0, 4.5, -13.5), Vector3(24, 11, 40), Vector3(0, -2.8, 12.5))
	rp.cull_mask = STAGE_LAYER | FLOOR_LAYER
	finish()


func _floor() -> void:
	# The flags under the water are glossy, so they carry the reflections (SSR on high, the probe
	# on low); the water sheet above adds ripples and drip rings.
	var flags := surface(1, Color(0.27, 0.29, 0.31), Color(0.14, 0.15, 0.17), 1.3, 0.3, 0.0)
	flags.set_shader_parameter("grime", 0.15)
	flags.set_shader_parameter("mortar_dark", 0.4)
	box(Vector3(34, 0.4, 42), Vector3(0, -0.2, -13.0), flags).layers = FLOOR_LAYER
	var water := plane(Vector2(34, 42), Vector3(0, 0.035, -13.0), shader_mat("res://assets/shaders/stage_flood.gdshader",
		{"tint": Color(0.03, 0.05, 0.06), "clarity": 0.85, "murk": 0.25, "ripple": 0.3, "drips": 0.8, "roughness": 0.04}))
	water.layers = FLOOR_LAYER
	mist(Vector2(16, 16), Vector3(0, 0.35, -20.0), Color(0.42, 0.5, 0.62), 0.25)
	mist(Vector2(22, 8), Vector3(0, 0.45, -9.0), Color(0.4, 0.46, 0.56), 0.14)


## A wall with a pointed (equilateral) arch opening, in local XY: `bay` wide, the opening `span`
## wide springing at v = 0, the wall rising to `top`, `thick` deep along z.
static func arch_wall(bay: float, span: float, top: float, thick: float) -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var half := span * 0.5
	var pts: Array[Vector2] = []
	var n := 10
	for i in n + 1:
		var a := deg_to_rad(180.0 - 60.0 * i / n)
		pts.append(Vector2(half + span * cos(a), span * sin(a)))
	for i in range(n - 1, -1, -1):
		pts.append(Vector2(-pts[i].x, pts[i].y))
	var zf := thick * 0.5
	for side in [1.0, -1.0]:
		var nrm := Vector3(0, 0, side)
		var z: float = zf * side
		for i in pts.size() - 1:
			var a := pts[i]
			var b := pts[i + 1]
			_quad(st, Vector3(a.x, a.y, z), Vector3(b.x, b.y, z), Vector3(b.x, top, z), Vector3(a.x, top, z), nrm)
		for e in [[-bay * 0.5, -half], [half, bay * 0.5]]:
			_quad(st, Vector3(e[0], 0, z), Vector3(e[1], 0, z), Vector3(e[1], top, z), Vector3(e[0], top, z), nrm)
	for i in pts.size() - 1:
		var d := pts[i + 1] - pts[i]
		var nrm := Vector3(d.y, -d.x, 0).normalized()
		_quad(st, Vector3(pts[i].x, pts[i].y, zf), Vector3(pts[i + 1].x, pts[i + 1].y, zf),
			Vector3(pts[i + 1].x, pts[i + 1].y, -zf), Vector3(pts[i].x, pts[i].y, -zf), nrm)
	return st.commit()


## Two triangles wound so they face along `nrm` (Godot treats clockwise as front-facing).
static func _quad(st: SurfaceTool, a: Vector3, b: Vector3, c: Vector3, d: Vector3, nrm: Vector3) -> void:
	var tris := [[a, b, c], [a, c, d]]
	for t in tris:
		var p0: Vector3 = t[0]
		var p1: Vector3 = t[1]
		var p2: Vector3 = t[2]
		if (p1 - p0).cross(p2 - p0).dot(nrm) > 0.0:
			var tmp := p1
			p1 = p2
			p2 = tmp
		for p in [p0, p1, p2]:
			st.set_normal(nrm)
			st.add_vertex(p)


func _arcades() -> void:
	var piers := Batch.new()
	var walls := Batch.new()
	for side in [-1.0, 1.0]:
		var x: float = side * COL_X
		for z in PIERS:
			piers.box(Vector3(1.2, 0.5, 1.2), Vector3(x, 0.25, z))
			piers.cyl(0.56, 0.18, Vector3(x, 0.59, z), Vector3.ZERO, -1.0, 16)
			piers.cyl(0.36, SPRING - 0.55, Vector3(x, (SPRING + 0.55) * 0.5, z), Vector3.ZERO, -1.0, 16)
			for o in [Vector2(0.36, 0), Vector2(-0.36, 0), Vector2(0, 0.36), Vector2(0, -0.36)]:
				piers.cyl(0.13, SPRING - 0.55, Vector3(x + o.x, (SPRING + 0.55) * 0.5, z + o.y), Vector3.ZERO, -1.0, 10)
			piers.cyl(0.52, 0.12, Vector3(x, SPRING - 0.35, z), Vector3.ZERO, -1.0, 16)
			piers.box(Vector3(1.15, 0.32, 1.15), Vector3(x, SPRING - 0.12, z))
		for i in PIERS.size() - 1:
			var z0: float = PIERS[i]
			var z1: float = PIERS[i + 1]
			var bay := absf(z1 - z0)
			walls.add(arch_wall(bay, bay - 1.0, WALL_TOP - SPRING, 0.7), Transform3D(Basis(Vector3.UP, PI * 0.5), Vector3(x, SPRING, (z0 + z1) * 0.5)))
		walls.box(Vector3(0.95, 0.3, absf(PIERS[-1] - PIERS[0]) + 1.0), Vector3(x, WALL_TOP + 0.15, (PIERS[0] + PIERS[-1]) * 0.5))
		walls.box(Vector3(0.7, 4.0, absf(PIERS[-1] - PIERS[0]) + 1.0), Vector3(x, WALL_TOP + 2.3, (PIERS[0] + PIERS[-1]) * 0.5))
	commit(piers, _stone)
	commit(walls, _stone)


func _aisles() -> void:
	var b := Batch.new()
	var ledges := Batch.new()
	var glass := _window_mat(1, 1.3)
	for side in [-1.0, 1.0]:
		var x: float = side * AISLE_X
		b.box(Vector3(0.8, 8.0, 38.0), Vector3(x + side * 0.4, 4.0, -14.0))
		b.box(Vector3(4.6, 0.4, 38.0), Vector3(side * (COL_X + 2.1), 7.6, -14.0))
		ledges.box(Vector3(0.35, 0.12, 33.0), Vector3(x - side * 0.17, 1.3, -15.5))
		for i in PIERS.size() - 1:
			var z: float = (PIERS[i] + PIERS[i + 1]) * 0.5
			_quad_mi(Vector2(1.0, 3.0), Vector3(x - side * 0.02, 4.0, z), -90.0 * side, glass)
			ledges.box(Vector3(0.3, 0.12, 1.1), Vector3(x - side * 0.1, 2.45, z))
			# Candles along the ledge below each window, unevenly spaced, some burned out.
			var zz := z - 2.0
			while zz < z + 2.0:
				_candle(Vector3(x - side * 0.2 + _rng.randf_range(-0.06, 0.06), 1.36, zz), _rng.randf_range(0.06, 0.3), _rng.randf_range(0.02, 0.035), _rng.randf() < 0.88)
				zz += _rng.randf_range(0.18, 0.45)
		var l := omni(Vector3(x - side * 1.0, 2.0, -8.0), Color(1.0, 0.62, 0.3), 1.6, 9.0, false, 0.1)
		flicker(l, 1.6, 0.12, 6.0)
	commit(b, _stone)
	commit(ledges, _stone)


func _east_end() -> void:
	box(Vector3(26, 16, 1.0), Vector3(0, 8.0, BACK_Z - 0.5), _stone)
	_quad_mi(Vector2(6.2, 6.2), Vector3(0, ROSE_Y, BACK_Z + 0.04), 0.0, _window_mat(0, 3.4))
	var ring := MeshInstance3D.new()
	var tm := TorusMesh.new()
	tm.inner_radius = 3.05
	tm.outer_radius = 3.6
	tm.rings = 48
	tm.ring_segments = 8
	ring.mesh = tm
	ring.material_override = _stone
	ring.position = Vector3(0, ROSE_Y, BACK_Z + 0.05)
	ring.rotation_degrees = Vector3(90, 0, 0)
	ring.scale = Vector3(1, 0.6, 1)
	add_child(ring)
	# Two lancets flank the altar.
	var glass := _window_mat(1, 2.0)
	var trim := Batch.new()
	for x in [-4.2, 4.2]:
		_quad_mi(Vector2(1.3, 3.9), Vector3(x, 3.4, BACK_Z + 0.03), 0.0, glass)
		trim.box(Vector3(1.6, 0.16, 0.3), Vector3(x, 1.4, BACK_Z + 0.1))
	# The altar on a two-step dais, an ivory cloth over it.
	trim.box(Vector3(8.0, 0.2, 3.6), Vector3(0, 0.1, BACK_Z + 1.8))
	trim.box(Vector3(5.6, 0.2, 2.4), Vector3(0, 0.3, BACK_Z + 1.2))
	trim.box(Vector3(2.8, 1.0, 1.0), Vector3(0, 0.9, BACK_Z + 0.9))
	commit(trim, _stone)
	var cloth := Shapes.material(Color(0.72, 0.68, 0.58), 0.9)
	box(Vector3(2.9, 0.04, 1.1), Vector3(0, 1.42, BACK_Z + 0.9), cloth)
	box(Vector3(2.0, 0.7, 0.03), Vector3(0, 1.08, BACK_Z + 1.42), cloth)
	for i in 7:
		var x := -1.2 + i * 0.4
		_candle(Vector3(x, 1.44, BACK_Z + 0.75), 0.32 + 0.12 * (1.0 - absf(i - 3) / 3.0), 0.04, true)
	for row in [[BACK_Z + 3.4, 0.2, 7.4, 34], [BACK_Z + 2.3, 0.4, 5.2, 26]]:
		for i in row[3]:
			var x: float = -row[2] * 0.5 + (i + _rng.randf_range(0.1, 0.9)) * row[2] / row[3]
			_candle(Vector3(x, row[1], row[0] + _rng.randf_range(-0.12, 0.12)), _rng.randf_range(0.05, 0.18), _rng.randf_range(0.02, 0.032), _rng.randf() < 0.9)
	var iron := Shapes.material(Color(0.07, 0.065, 0.06), 0.5, 0.8)
	var stands := Batch.new()
	for x in [-2.7, 2.7]:
		stands.cyl(0.03, 1.7, Vector3(x, 0.2 + 0.85, BACK_Z + 2.8), Vector3.ZERO, -1.0, 8)
		stands.cyl(0.2, 0.06, Vector3(x, 0.23, BACK_Z + 2.8), Vector3.ZERO, -1.0, 12)
		stands.box(Vector3(0.9, 0.04, 0.04), Vector3(x, 1.85, BACK_Z + 2.8))
		for k in 5:
			_candle(Vector3(x - 0.4 + k * 0.2, 1.87 + (0.12 if k == 2 else 0.0), BACK_Z + 2.8), 0.22, 0.03, true)
	commit(stands, iron)
	var altar := omni(Vector3(0, 1.8, BACK_Z + 3.6), Color(1.0, 0.62, 0.3), 2.2, 6.0, false, 0.2)
	flicker(altar, 2.6, 0.1, 5.0)
	# Moonlight through the rose: a pool on the water mid-nave and a soft shaft.
	spot(Vector3(0, ROSE_Y + 1.0, BACK_Z + 1.0), Vector3(0, 0, -14.0), Color(0.6, 0.74, 1.0), 9.0, 26.0, 17.0)
	shaft(Vector3(0, ROSE_Y, BACK_Z + 0.3), Vector3(0, 0.0, -15.0), 5.0, 7.5, Color(0.55, 0.7, 1.0), 0.5)


func _pews() -> void:
	var wood := surface(2, Color(0.2, 0.13, 0.08), Color(0.09, 0.06, 0.04), 0.5, 0.7, 0.25)
	var b := Batch.new()
	var z := -4.4
	var row := 0
	while z > -25.5:
		for side in [-1.0, 1.0]:
			var kind := _rng.randi() % 7
			var at := Vector3(side * 3.8 + _rng.randf_range(-0.2, 0.2), 0.0, z + _rng.randf_range(-0.15, 0.15))
			var yaw := _rng.randf_range(-4.0, 4.0)
			match kind:
				0:
					pass
				1:
					_pew(b, at + Vector3(0, 0.2, 0.3), yaw + 6.0, Vector3(-78, 0, 0), 3.4)
				2:
					_pew(b, at + Vector3(-side * 0.95, -0.05, 0), yaw - 9.0, Vector3(0, 0, side * 7.0), 1.6)
					_pew(b, at + Vector3(side * 1.0, -0.12, 0.2), yaw + 14.0, Vector3(4, 0, -side * 11.0), 1.5)
				3:
					_pew(b, at + Vector3(0, -0.1, 0), yaw, Vector3(0, 0, side * 5.0), 3.4)
				_:
					_pew(b, at, yaw, Vector3.ZERO, 3.4)
		z -= 1.55
		row += 1
	commit(b, wood)


func _pew(b: Batch, at: Vector3, yaw: float, tilt: Vector3, length: float) -> void:
	var basis := Basis.from_euler(Vector3(deg_to_rad(tilt.x), deg_to_rad(yaw), deg_to_rad(tilt.z)))
	var parts := [[Vector3(length, 0.06, 0.42), Vector3(0, 0.45, 0)], [Vector3(length, 0.46, 0.05), Vector3(0, 0.76, 0.2)],
		[Vector3(length + 0.04, 0.05, 0.1), Vector3(0, 1.0, 0.2)], [Vector3(0.07, 1.0, 0.52), Vector3(-length * 0.5, 0.5, 0.02)],
		[Vector3(0.07, 1.0, 0.52), Vector3(length * 0.5, 0.5, 0.02)], [Vector3(length, 0.05, 0.12), Vector3(0, 0.16, -0.3)]]
	for p in parts:
		var m := BoxMesh.new()
		m.size = p[0]
		b.add(m, Transform3D(basis, at) * Transform3D(Basis(), p[1]))


func _racks() -> void:
	var iron := Shapes.material(Color(0.07, 0.065, 0.06), 0.5, 0.8)
	var b := Batch.new()
	for r in [[-6.0, -3.5, 8.0, 1], [6.1, -4.3, -10.0, 1], [-5.6, -12.2, 5.0, 1], [5.9, -11.0, -6.0, 1], [-6.0, -21.0, 12.0, 0], [5.8, -20.0, -4.0, 0]]:
		var basis := Basis(Vector3.UP, deg_to_rad(r[2]))
		var o := Vector3(r[0], 0, r[1])
		for lx in [-0.7, 0.7]:
			b.add(_cyl_mesh(0.025, 1.3), Transform3D(basis, o + basis * Vector3(lx, 0.65, 0)))
		for t in 3:
			var y := 0.82 + t * 0.2
			var tz := 0.2 - t * 0.2
			var bar := BoxMesh.new()
			bar.size = Vector3(1.5, 0.03, 0.12)
			b.add(bar, Transform3D(basis, o + basis * Vector3(0, y, tz)))
			for k in 10:
				var lx := -0.66 + k * 0.146 + _rng.randf_range(-0.03, 0.03)
				_candle(o + basis * Vector3(lx, y + 0.015, tz + _rng.randf_range(-0.03, 0.03)), _rng.randf_range(0.04, 0.2), _rng.randf_range(0.018, 0.028), _rng.randf() < 0.85)
		if r[3] == 1:
			var l := omni(o + Vector3(0, 1.5, 0.5), Color(1.0, 0.58, 0.26), 2.6, 6.0, false, 0.08)
			flicker(l, 2.6, 0.15, 7.0)
	# Melted clusters on the pier plinths.
	for side in [-1.0, 1.0]:
		for z in [-5.3, -9.7, -14.1, -18.5, -22.9]:
			for k in _rng.randi_range(3, 7):
				var a := _rng.randf() * TAU
				_candle(Vector3(side * COL_X - side * 0.45 + cos(a) * 0.12, 0.5, z + 0.45 + sin(a) * 0.12), _rng.randf_range(0.08, 0.35), _rng.randf_range(0.025, 0.045), _rng.randf() < 0.8)
	commit(b, iron)


func _floating_candles() -> void:
	var board := surface(2, Color(0.2, 0.14, 0.09), Color(0.1, 0.07, 0.05), 0.3, 0.8)
	var b := Batch.new()
	var spots: Array = []
	for i in 60:
		var p := Vector3(_rng.randf_range(-6.5, 6.5), 0.035, _rng.randf_range(-24.0, -2.6))
		if i < 12:
			p = Vector3(_rng.randf_range(-7.0, 7.0), 0.035, _rng.randf_range(1.6, 3.4))
		spots.append(p)
	for p in spots:
		b.cyl(0.06, 0.025, p, Vector3(_rng.randf_range(-3, 3), 0, _rng.randf_range(-3, 3)), -1.0, 8)
		_candle(p + Vector3(0, 0.012, 0), _rng.randf_range(0.03, 0.06), 0.024, true)
	commit(b, board, false)
	# Funeral lilies drifting among the candles near the front.
	var petal := Shapes.material(Color(0.62, 0.6, 0.55), 0.7)
	var lb := Batch.new()
	for i in 9:
		var c := Vector3(_rng.randf_range(-6.5, 6.5), 0.04, _rng.randf_range(1.4, 3.6))
		for k in 6:
			var a := TAU * k / 6.0 + _rng.randf() * 0.3
			lb.add(_petal_mesh(), Transform3D(Basis(Vector3.UP, -a) * Basis(Vector3.BACK, 0.35), c + Vector3(cos(a) * 0.03, 0.0, sin(a) * 0.03)))
	commit(lb, petal, false)


func _censers() -> void:
	var brass := Shapes.material(Color(0.5, 0.36, 0.16), 0.38, 0.9)
	var chain_mat := Shapes.material(Color(0.25, 0.2, 0.12), 0.45, 0.9)
	var ember := emissive(Color(1.0, 0.45, 0.12), 3.0)
	for c in [[-3.3, 3.8, -6.2], [3.9, 4.3, -9.0], [-0.8, 4.6, -13.6], [2.4, 5.0, -19.5]]:
		var drop: float = 10.0 - c[1]
		var root := chain(Vector3(c[0], 10.0, c[2]), drop, chain_mat, 0.8)
		var b := Batch.new()
		b.sphere(0.17, Vector3(0, -drop - 0.05, 0), Vector3(1, 0.7, 1), 16)
		b.sphere(0.15, Vector3(0, -drop + 0.06, 0), Vector3(1, 0.9, 1), 16)
		b.cyl(0.03, 0.12, Vector3(0, -drop + 0.2, 0), Vector3.ZERO, 0.01, 8)
		b.cyl(0.08, 0.06, Vector3(0, -drop - 0.17, 0), Vector3.ZERO, -1.0, 12)
		var body := MeshInstance3D.new()
		body.mesh = b.st.commit()
		body.material_override = brass
		root.add_child(body)
		var glow := MeshInstance3D.new()
		var band := CylinderMesh.new()
		band.top_radius = 0.172
		band.bottom_radius = 0.172
		band.height = 0.025
		band.radial_segments = 16
		glow.mesh = band
		glow.material_override = ember
		glow.position = Vector3(0, -drop, 0)
		root.add_child(glow)
		var smoke := plume(Vector3.ZERO, Color(0.55, 0.6, 0.7, 0.28), 14, 0.05, 0.35, 0.5)
		smoke.reparent(root, false)
		smoke.position = Vector3(0, -drop + 0.25, 0)


func _banners() -> void:
	var tex := _tatter_texture()
	for bnr in [[-1.0, -7.5, Color(0.62, 0.58, 0.5)], [1.0, -12.0, Color(0.1, 0.09, 0.09)], [-1.0, -16.3, Color(0.1, 0.09, 0.09)], [1.0, -3.1, Color(0.6, 0.56, 0.48)], [1.0, -25.1, Color(0.6, 0.56, 0.48)], [-1.0, -29.5, Color(0.1, 0.09, 0.09)]]:
		var m := StandardMaterial3D.new()
		m.albedo_color = bnr[2]
		m.albedo_texture = tex
		m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR
		m.alpha_scissor_threshold = 0.5
		m.cull_mode = BaseMaterial3D.CULL_DISABLED
		m.roughness = 0.95
		var pivot := Node3D.new()
		pivot.position = Vector3(bnr[0] * (COL_X - 0.45), 9.2, bnr[1])
		add_child(pivot)
		var q := MeshInstance3D.new()
		var qm := QuadMesh.new()
		qm.size = Vector2(1.1, 4.0)
		q.mesh = qm
		q.material_override = m
		q.position = Vector3(0, -2.0, 0)
		q.rotation_degrees = Vector3(0, -65.0 * bnr[0], 0)
		pivot.add_child(q)
		var seed := _rng.randf() * 10.0
		_animated.append(func(t: float, _d: float): pivot.rotation = Vector3(sin(t * 0.5 + seed) * 0.025, 0, sin(t * 0.37 + seed) * 0.02))


## A long cloth hanging, its lower edge torn into a few ragged tongues, with a darker hem band.
static func _tatter_texture() -> ImageTexture:
	var img := Image.create(64, 256, false, Image.FORMAT_RGBA8)
	var rng := RandomNumberGenerator.new()
	rng.seed = 3
	var cut := PackedFloat32Array()
	var tongues := [rng.randf_range(-20, 30), rng.randf_range(-20, 30), rng.randf_range(-20, 30), rng.randf_range(-20, 30)]
	for x in 64:
		var t: float = tongues[mini(x / 16, 3)]
		cut.append(205.0 + t - absf(fmod(x, 16.0) - 8.0) * 2.5 + rng.randf_range(-3.0, 3.0))
	for y in 256:
		for x in 64:
			var a := 1.0 if y < cut[x] else 0.0
			var shade := 0.85 + rng.randf() * 0.08 - (0.35 if y > 20 and y < 28 else 0.0) - (0.25 if x < 3 or x > 60 else 0.0)
			img.set_pixel(x, y, Color(shade, shade, shade, a))
	return ImageTexture.create_from_image(img)


static func _petal_mesh() -> SphereMesh:
	var m := SphereMesh.new()
	m.radius = 0.03
	m.height = 0.012
	m.radial_segments = 8
	m.rings = 4
	return m


func _ends() -> void:
	# Rubble against the first piers, so the ends of the floor read as walls.
	var b := Batch.new()
	for side in [-1.0, 1.0]:
		var x: float = side * (COL_X + 0.4)
		b.cyl(0.42, 1.6, Vector3(x + side * 0.2, 0.42, 0.9), Vector3(90, side * 20.0, 0), -1.0, 16)
		b.box(Vector3(0.9, 0.7, 0.8), Vector3(x + side * 0.1, 0.35, -2.2), Vector3(0, side * 15.0, 4))
		b.box(Vector3(0.5, 0.4, 0.6), Vector3(x - side * 0.3, 0.2, 2.2), Vector3(10, 30, 0))
	commit(b, _stone)
	var wood := surface(2, Color(0.2, 0.13, 0.08), Color(0.09, 0.06, 0.04), 0.5, 0.7, 0.25)
	var pb := Batch.new()
	_pew(pb, Vector3(-COL_X - 0.3, 0.6, -1.9), 80.0, Vector3(0, 0, 32), 2.4)
	_pew(pb, Vector3(COL_X + 0.4, 0.0, -2.0), 95.0, Vector3(0, 0, -6), 2.4)
	commit(pb, wood)


static func _cyl_mesh(r: float, h: float) -> CylinderMesh:
	var c := CylinderMesh.new()
	c.top_radius = r
	c.bottom_radius = r
	c.height = h
	c.radial_segments = 8
	c.rings = 1
	return c


func _candle(base: Vector3, h: float, r: float, lit: bool) -> void:
	_wax.append(Transform3D(Basis.from_scale(Vector3(r, h, r)), base + Vector3(0, h * 0.5, 0)))
	if lit:
		_wicks.append(base + Vector3(0, h + 0.004, 0))
		_flame_sizes.append(clampf(r / 0.028, 0.75, 1.5) * _rng.randf_range(0.85, 1.15))


func _commit_candles() -> void:
	var wax := StandardMaterial3D.new()
	wax.albedo_color = Color(0.5, 0.43, 0.33)
	wax.roughness = 0.5
	wax.emission_enabled = true
	wax.emission = Color(1.0, 0.6, 0.3)
	wax.emission_energy_multiplier = 0.3
	var c := CylinderMesh.new()
	c.top_radius = 1.0
	c.bottom_radius = 1.0
	c.height = 1.0
	c.radial_segments = 8
	c.rings = 1
	multi(c, _wax, wax)
	sprites(_wicks, Vector2(0.055, 0.13), Color(1.0, 0.5, 0.16), 6.0, 0, 0.025, _flame_sizes)
	# A faint warm halo round every flame, so the racks and ledges glow as a whole.
	var halo := []
	for w in _wicks:
		halo.append(w + Vector3(0, 0.04, 0))
	sprites(halo, Vector2(0.5, 0.5), Color(1.0, 0.55, 0.22), 0.1, 2, 0.03)


func _window_mat(shape: int, energy: float) -> ShaderMaterial:
	return shader_mat("res://assets/shaders/stage_rose.gdshader", {"shape": shape, "energy": energy,
		"stone": Color(0.25, 0.26, 0.29), "fog_color": FOG, "fog_density": 0.028})


func _quad_mi(size: Vector2, pos: Vector3, yaw: float, mat: Material) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var q := QuadMesh.new()
	q.size = size
	mi.mesh = q
	mi.material_override = mat
	mi.position = pos
	mi.rotation_degrees = Vector3(0, yaw, 0)
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(mi)
	return mi
