class_name Vfx
extends Node3D
## Combat feedback: impact sparks and a brief light where blows land, guard flashes, dust where
## bodies meet the floor, blood (following the gore setting), and the look of every projectile in
## flight. Impact reads mostly from hit-stop, pose and sound; effects stay short.
##
## Everything is built in setup() and reused from pools, so a fight never creates materials or
## particle systems (and never compiles a shader) mid-round. Settings "effects" = "reduced" thins
## particles out and "flashes" off removes bright flashes.

const SPARK_POOL := 8
const BLOOD_POOL := 6
const PUFF_POOL := 4
const FLASH_POOL := 6
const PROJ_POOL := 6
## Projectile looks: core colour, light colour and energy, size, trail colour.
const KINDS := {
	"ember": {"core": Color(1.0, 0.55, 0.15), "light": 1.6, "size": 0.42, "trail": Color(1.0, 0.45, 0.1)},
	"orb": {"core": Color(0.8, 0.9, 1.0), "light": 1.0, "size": 0.35, "trail": Color(0.7, 0.8, 1.0)},
	"wave": {"core": Color(0.55, 0.8, 1.0), "light": 1.2, "size": 0.9, "trail": Color(0.5, 0.75, 1.0)},
	"flare": {"core": Color(1.0, 0.2, 0.12), "light": 1.8, "size": 0.26, "trail": Color(1.0, 0.5, 0.2)},
	"bubble": {"core": Color(0.55, 0.95, 0.85), "light": 0.5, "size": 0.6, "trail": Color(0.6, 1.0, 0.9)},
	"tar": {"core": Color(0.05, 0.08, 0.04), "light": 0.0, "size": 0.38, "trail": Color(0.25, 0.4, 0.1)},
	"sickle": {"core": Color(0.75, 0.8, 0.85), "light": 0.0, "size": 0.3, "trail": Color(0.7, 0.8, 0.6)},
	"harpoon": {"core": Color(0.8, 0.62, 0.3), "light": 0.0, "size": 0.25, "trail": Color(0.6, 0.6, 0.6)},
	"anchor": {"core": Color(0.25, 0.25, 0.27), "light": 0.0, "size": 0.5, "trail": Color(0.5, 0.6, 0.6)},
	"storm": {"core": Color(0.6, 1.0, 0.4), "light": 2.5, "size": 0.5, "trail": Color(0.6, 1.0, 0.4)},
	"quake": {"core": Color(0.55, 0.45, 0.32), "light": 0.0, "size": 0.5, "trail": Color(0.5, 0.42, 0.3)},
	"gust": {"core": Color(0.75, 0.82, 0.8), "light": 0.0, "size": 1.6, "trail": Color(0.8, 0.85, 0.85)},
}

var sim: FightSim
var views: Array[FighterView] = []
var camera: FightCamera
var reduced := false
var flashes_on := true
var gore := "full"
var _sparks: Array[GPUParticles3D] = []
var _blood: Array[GPUParticles3D] = []
var _puffs: Array[GPUParticles3D] = []
var _flashes: Array[Dictionary] = []
var _lights: Array[Dictionary] = []
## Projectile visuals: {"node", "core", "mat", "light", "trail", "chain", "proj"}
var _proj: Array[Dictionary] = []
var _shadows: Array[MeshInstance3D] = []
var _storm_light: OmniLight3D
var _bolts: Array[MeshInstance3D] = []
var _storm_fired := -1
var _next_spark := 0
var _next_blood := 0
var _next_puff := 0
var _next_flash := 0
var _dot_tex: Texture2D
var _time := 0.0


func setup(s: FightSim, v: Array[FighterView], cam: FightCamera) -> void:
	sim = s
	views = v
	camera = cam
	reduced = Settings.get_value("effects") == "reduced"
	flashes_on = Settings.get_value("flashes")
	gore = str(Settings.get_value("gore"))
	for i in SPARK_POOL:
		var p := _particles(26, 0.3, _glow_mesh(Vector2(0.014, 0.12)))
		var pm: ParticleProcessMaterial = p.process_material
		pm.particle_flag_align_y = true
		pm.direction = Vector3.UP
		pm.spread = 180.0
		pm.gravity = Vector3(0, -8.0, 0)
		pm.damping_min = 3.0
		pm.damping_max = 7.0
		pm.color_ramp = _fade_ramp()
		add_child(p)
		_sparks.append(p)
	for i in BLOOD_POOL:
		var p := _particles(32, 0.75, _drop_mesh())
		var pm: ParticleProcessMaterial = p.process_material
		pm.direction = Vector3(1, 0.6, 0)
		pm.spread = 38.0
		pm.gravity = Vector3(0, -11.0, 0)
		pm.scale_min = 0.012
		pm.scale_max = 0.035
		pm.particle_flag_align_y = true
		pm.color = Color(0.42, 0.02, 0.02)
		add_child(p)
		_blood.append(p)
	for i in PUFF_POOL:
		var p := _particles(12, 0.9, _dust_mesh())
		var pm: ParticleProcessMaterial = p.process_material
		pm.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_RING
		pm.emission_ring_axis = Vector3.UP
		pm.emission_ring_radius = 0.3
		pm.emission_ring_inner_radius = 0.05
		pm.emission_ring_height = 0.02
		pm.direction = Vector3(0, 0.3, 0)
		pm.spread = 80.0
		pm.initial_velocity_min = 0.4
		pm.initial_velocity_max = 1.3
		pm.gravity = Vector3(0, 0.15, 0)
		pm.damping_min = 1.5
		pm.damping_max = 2.5
		pm.scale_min = 0.3
		pm.scale_max = 0.6
		pm.color = Color(0.45, 0.47, 0.42, 0.35)
		pm.color_ramp = _fade_ramp()
		add_child(p)
		_puffs.append(p)
	for i in FLASH_POOL:
		var mi := MeshInstance3D.new()
		mi.mesh = QuadMesh.new()
		var m := StandardMaterial3D.new()
		m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		m.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
		m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		m.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
		m.albedo_texture = _soft_dot()
		m.no_depth_test = true
		mi.material_override = m
		mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		mi.visible = false
		add_child(mi)
		_flashes.append({"node": mi, "mat": m, "t": 1.0, "life": 1.0, "size": 0.3, "color": Color.WHITE})
	for i in 2:
		var l := OmniLight3D.new()
		l.omni_range = 2.8
		l.omni_attenuation = 1.6
		l.light_energy = 0.0
		l.visible = false
		add_child(l)
		_lights.append({"light": l, "t": 1.0, "life": 1.0, "energy": 0.0})
	for i in PROJ_POOL:
		_proj.append(_make_projectile_visual())
	# A soft blob shadow under each fighter: it shows where a jumper (or the Slopbat, high above
	# the screen during its dive) will come down.
	for i in 2:
		var sh := MeshInstance3D.new()
		var q := QuadMesh.new()
		q.size = Vector2(1, 1)
		q.orientation = PlaneMesh.FACE_Y
		sh.mesh = q
		var sm := StandardMaterial3D.new()
		sm.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		sm.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		sm.albedo_texture = _soft_dot()
		sm.albedo_color = Color(0, 0, 0, 0.55)
		sh.material_override = sm
		sh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		add_child(sh)
		_shadows.append(sh)
	_storm_light = OmniLight3D.new()
	_storm_light.light_color = Color(0.55, 1.0, 0.35)
	_storm_light.omni_range = 6.0
	_storm_light.visible = false
	add_child(_storm_light)
	for i in 6:
		var b := MeshInstance3D.new()
		var bq := QuadMesh.new()
		bq.size = Vector2(0.22, 6.0)
		b.mesh = bq
		var bm := StandardMaterial3D.new()
		bm.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		bm.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		bm.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
		bm.billboard_mode = BaseMaterial3D.BILLBOARD_FIXED_Y
		bm.albedo_color = Color(0.7, 1.0, 0.5, 0.9)
		bm.albedo_texture = _bolt_texture()
		b.material_override = bm
		b.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		b.visible = false
		add_child(b)
		_bolts.append(b)


func _particles(amount: int, lifetime: float, mesh: Mesh) -> GPUParticles3D:
	var p := GPUParticles3D.new()
	p.amount = amount
	p.lifetime = lifetime
	p.local_coords = false
	p.one_shot = true
	p.explosiveness = 1.0
	p.emitting = false
	p.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	p.process_material = ParticleProcessMaterial.new()
	p.draw_pass_1 = mesh
	return p


func _glow_mesh(size: Vector2) -> QuadMesh:
	var q := QuadMesh.new()
	q.size = size
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	m.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	m.billboard_keep_scale = true
	m.vertex_color_use_as_albedo = true
	m.albedo_texture = _soft_dot()
	m.albedo_color = Color(2.2, 2.2, 2.2)
	q.material = m
	return q


func _drop_mesh() -> SphereMesh:
	# Stretched along the velocity, so droplets streak instead of hanging like beads.
	var s := SphereMesh.new()
	s.radius = 0.5
	s.height = 3.0
	s.radial_segments = 6
	s.rings = 3
	var m := StandardMaterial3D.new()
	m.vertex_color_use_as_albedo = true
	m.roughness = 0.25
	m.metallic_specular = 0.7
	s.material = m
	return s


func _dust_mesh() -> QuadMesh:
	var q := QuadMesh.new()
	var m := StandardMaterial3D.new()
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	m.vertex_color_use_as_albedo = true
	m.albedo_texture = _soft_dot()
	m.roughness = 1.0
	q.material = m
	return q


func _fade_ramp() -> GradientTexture1D:
	var g := Gradient.new()
	g.set_color(0, Color(1, 1, 1, 1))
	g.set_color(1, Color(1, 1, 1, 0))
	g.add_point(0.6, Color(1, 1, 1, 0.75))
	var t := GradientTexture1D.new()
	t.gradient = g
	return t


## A jagged vertical streak for lightning bolts.
func _bolt_texture() -> Texture2D:
	var img := Image.create(32, 256, false, Image.FORMAT_RGBA8)
	var rng := RandomNumberGenerator.new()
	rng.seed = 7
	var x := 16.0
	for y in 256:
		x = clampf(x + rng.randf_range(-2.5, 2.5), 6.0, 26.0)
		for xx in 32:
			var d := absf(xx - x)
			var a := clampf(1.0 - d / 5.0, 0.0, 1.0)
			img.set_pixel(xx, y, Color(1, 1, 1, a * a))
	return ImageTexture.create_from_image(img)


func _soft_dot() -> Texture2D:
	if _dot_tex:
		return _dot_tex
	var img := Image.create(64, 64, false, Image.FORMAT_RGBA8)
	for y in 64:
		for x in 64:
			var d := Vector2(x - 31.5, y - 31.5).length() / 32.0
			var a := clampf(1.0 - d, 0.0, 1.0)
			img.set_pixel(x, y, Color(1, 1, 1, a * a))
	_dot_tex = ImageTexture.create_from_image(img)
	return _dot_tex


func _make_projectile_visual() -> Dictionary:
	var node := Node3D.new()
	node.visible = false
	add_child(node)
	var core := MeshInstance3D.new()
	var q := QuadMesh.new()
	core.mesh = q
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	m.albedo_texture = _soft_dot()
	core.material_override = m
	core.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	node.add_child(core)
	var solid := MeshInstance3D.new()
	node.add_child(solid)
	var light := OmniLight3D.new()
	light.omni_range = 2.4
	node.add_child(light)
	var trail := _particles(40, 0.45, _glow_mesh(Vector2(0.08, 0.08)))
	trail.one_shot = false
	trail.explosiveness = 0.0
	var pm: ParticleProcessMaterial = trail.process_material
	pm.spread = 180.0
	pm.initial_velocity_min = 0.1
	pm.initial_velocity_max = 0.5
	pm.gravity = Vector3(0, 0.6, 0)
	pm.scale_min = 0.6
	pm.scale_max = 1.6
	pm.color_ramp = _fade_ramp()
	add_child(trail)
	var chain := MeshInstance3D.new()
	var cm := CylinderMesh.new()
	cm.top_radius = 0.008
	cm.bottom_radius = 0.008
	cm.height = 1.0
	cm.radial_segments = 6
	chain.mesh = cm
	chain.material_override = Shapes.material(Color(0.35, 0.35, 0.33), 0.5, 0.8)
	chain.visible = false
	add_child(chain)
	return {"node": node, "core": core, "mat": m, "solid": solid, "light": light, "trail": trail, "chain": chain, "proj": null}


func _assign_projectile(p: Projectile) -> void:
	for v in _proj:
		if v["proj"] == null:
			v["proj"] = p
			var k: Dictionary = KINDS.get(p.kind, KINDS["orb"])
			var size := float(k["size"])
			((v["core"] as MeshInstance3D).mesh as QuadMesh).size = Vector2(size, size)
			(v["mat"] as StandardMaterial3D).albedo_color = (k["core"] as Color) * 1.6
			var light: OmniLight3D = v["light"]
			light.light_color = k["core"]
			light.light_energy = float(k["light"]) * (0.6 if reduced else 1.0)
			light.visible = float(k["light"]) > 0.0
			var solid: MeshInstance3D = v["solid"]
			solid.mesh = _solid_mesh(p.kind)
			solid.material_override = _solid_material(p.kind)
			(v["core"] as MeshInstance3D).visible = not p.kind in ["sickle", "harpoon", "anchor"]
			var trail: GPUParticles3D = v["trail"]
			(trail.process_material as ParticleProcessMaterial).color = k["trail"]
			trail.amount_ratio = 0.4 if reduced else 1.0
			trail.global_position = Vector3(p.pos.x, p.pos.y, 0.0)
			trail.emitting = true
			(v["chain"] as MeshInstance3D).visible = p.tether or p.kind == "sickle"
			(v["node"] as Node3D).visible = true
			return


func _solid_mesh(kind: String) -> Mesh:
	match kind:
		"sickle":
			return Props._blade(0.3, 0.06)
		"harpoon":
			var c := CylinderMesh.new()
			c.top_radius = 0.0
			c.bottom_radius = 0.035
			c.height = 0.3
			return c
		"anchor":
			var t := TorusMesh.new()
			t.inner_radius = 0.16
			t.outer_radius = 0.24
			return t
		"bubble":
			var s := SphereMesh.new()
			s.radius = 0.28
			s.height = 0.56
			return s
		"tar":
			var s := SphereMesh.new()
			s.radius = 0.16
			s.height = 0.3
			return s
	return null


func _solid_material(kind: String) -> Material:
	match kind:
		"bubble":
			var m := StandardMaterial3D.new()
			m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
			m.albedo_color = Color(0.6, 0.95, 0.9, 0.18)
			m.roughness = 0.05
			m.rim_enabled = true
			m.rim = 1.0
			m.metallic_specular = 1.0
			return m
		"tar":
			return Shapes.material(Color(0.04, 0.05, 0.03), 0.15, 0.0)
	return Shapes.material(Color(0.6, 0.62, 0.65), 0.35, 0.9)


func _burst(pos: Vector3, color: Color, count: int, speed: float, life := 0.3) -> void:
	var p := _sparks[_next_spark]
	_next_spark = (_next_spark + 1) % SPARK_POOL
	var pm: ParticleProcessMaterial = p.process_material
	pm.color = color
	pm.initial_velocity_min = speed * 0.4
	pm.initial_velocity_max = speed
	p.amount_ratio = clampf(float(count) / p.amount * (0.5 if reduced else 1.0), 0.05, 1.0)
	p.lifetime = life
	p.global_position = pos
	p.restart()


## A spray of blood from `pos` heading `dir` (+1 / -1 along x), scaled by the gore setting.
func blood(pos: Vector3, dir: float, amount: float) -> void:
	if gore == "off":
		return
	var p := _blood[_next_blood]
	_next_blood = (_next_blood + 1) % BLOOD_POOL
	var pm: ParticleProcessMaterial = p.process_material
	pm.direction = Vector3(dir, 0.7, 0.15)
	pm.initial_velocity_min = 1.2 + amount
	pm.initial_velocity_max = 2.6 + amount * 2.0
	var k := (1.0 if gore == "full" else 0.35) * (0.6 if reduced else 1.0)
	p.amount_ratio = clampf(amount * k, 0.05, 1.0)
	p.global_position = pos
	p.restart()


func dust(pos: Vector3) -> void:
	var p := _puffs[_next_puff]
	_next_puff = (_next_puff + 1) % PUFF_POOL
	p.global_position = pos
	p.amount_ratio = 0.5 if reduced else 1.0
	p.restart()


func flash(pos: Vector3, color: Color, size: float, life := 0.09, light := 0.0, light_owner := 0) -> void:
	if light > 0.0:
		var l: Dictionary = _lights[light_owner]
		var ol: OmniLight3D = l["light"]
		ol.global_position = pos
		ol.light_color = color
		ol.visible = true
		l["t"] = 0.0
		l["life"] = life * 1.6
		l["energy"] = light * (0.6 if reduced else 1.0)
	if not flashes_on:
		return
	var fl := _flashes[_next_flash]
	_next_flash = (_next_flash + 1) % FLASH_POOL
	fl["t"] = 0.0
	fl["life"] = life
	fl["size"] = size
	fl["color"] = color
	var n: MeshInstance3D = fl["node"]
	n.global_position = pos
	n.visible = true


func burst(pos: Vector3, color: Color, count: int, speed: float, life := 0.3) -> void:
	_burst(pos, color, count, speed, life)


## The Storm Shroud: green crackling while it gathers, then bolts all around the caster.
func _storm_step() -> void:
	var any := false
	for f in sim.fighters:
		if f.state != Fighter.S.MOVE or f.move == null or not "storm" in f.move.tags:
			continue
		any = true
		var c := Vector3(f.pos.x, 1.4, 0.0)
		_storm_light.global_position = c + Vector3(0, 0.6, 0.6)
		_storm_light.visible = true
		if f.mf < f.move.startup:
			var k := float(f.mf) / f.move.startup
			_storm_light.light_energy = (0.6 + k * 2.5) * (0.6 + 0.4 * sin(_time * 40.0)) * (0.6 if reduced else 1.0)
			if randf() < 0.35 + k * 0.4:
				_burst(c + Vector3(randf_range(-0.8, 0.8), randf_range(-0.6, 1.2), 0.2), Color(0.6, 1.0, 0.4), 6, 2.5, 0.25)
			for b in _bolts:
				b.visible = false
		elif f.move.in_active(f.mf):
			if _storm_fired != f.sim.tick_count - f.mf:
				_storm_fired = f.sim.tick_count - f.mf
				Audio.play("thunder", 2.0)
				flash(c, Color(0.7, 1.0, 0.5), 3.0, 0.25, 0.0, f.index)
			_storm_light.light_energy = 4.0 * (0.6 if reduced else 1.0)
			for i in _bolts.size():
				var b := _bolts[i]
				b.visible = flashes_on or i % 2 == 0
				var ang := (float(i) / _bolts.size()) * TAU + _time * 3.0
				var r := 0.9 + fmod(float(i) * 0.53 + _time * 2.0, 1.6)
				b.global_position = Vector3(f.pos.x + cos(ang) * r, 3.0, sin(ang) * 0.3)
		else:
			for b in _bolts:
				b.visible = false
	if not any:
		_storm_light.visible = false
		for b in _bolts:
			b.visible = false


static func _v3(p: Vector2, z := 0.0) -> Vector3:
	return Vector3(p.x, p.y, z)


func on_event(e: Dictionary) -> void:
	match e["type"]:
		"hit":
			var pos := _v3(e["point"], 0.15)
			var dmg: int = e["damage"]
			var strong := dmg >= 90
			var col := Color(1.0, 0.86, 0.62)
			if e.get("proj", false):
				var k: Dictionary = KINDS.get(str(e.get("kind", "orb")), KINDS["orb"])
				col = k["core"]
				_burst(pos, col, 22, 4.0, 0.45)
			flash(pos, col, 0.35 + (0.3 if strong else 0.0), 0.09, 1.8 if strong else 1.0, int(e["by"]))
			_burst(pos, col, 10 if not strong else 22, 3.0 if not strong else 4.6)
			if not e.get("throw", false) and dmg > 0:
				var dir := signf(sim.fighters[e["who"]].pos.x - sim.fighters[e["by"]].pos.x)
				blood(pos, dir, clampf(dmg / 110.0, 0.15, 1.0))
		"block":
			flash(_v3(e["point"], 0.15), Color(0.55, 0.8, 1.0), 0.32, 0.08, 0.8, 0)
		"armor":
			flash(_v3(e["point"], 0.15), Color(1.0, 0.95, 0.75), 0.5, 0.14, 1.5, int(e["who"]))
		"clash":
			flash(_v3(e["pos"], 0.1), Color(1, 1, 1), 0.8, 0.15, 2.0, 0)
			_burst(_v3(e["pos"], 0.1), Color(1.0, 0.85, 0.6), 26, 4.0, 0.4)
		"throw_break":
			var mid := (sim.fighters[0].pos + sim.fighters[1].pos) * 0.5 + Vector2(0, 1.3)
			flash(_v3(mid, 0.1), Color(1, 1, 1), 0.5, 0.12, 1.2, 0)
		"land":
			if not e.get("soft", false):
				var f := sim.fighters[e["who"]]
				dust(Vector3(f.pos.x, 0.03, 0.0))
		"projectile":
			for p in sim.projectiles:
				var assigned := false
				for v in _proj:
					if v["proj"] == p:
						assigned = true
				if not assigned:
					_assign_projectile(p)
		"projectile_end":
			var k: Dictionary = KINDS.get(str(e.get("kind", "orb")), KINDS["orb"])
			_burst(_v3(e["pos"], 0.05), k["trail"], 10, 2.0, 0.3)


func step(delta: float) -> void:
	_time += delta
	for i in 2:
		var f := sim.fighters[i]
		var sh := _shadows[i]
		var hgt := maxf(f.pos.y, 0.0)
		var w := f.def.push_w * 2.6 * (1.0 + hgt * 0.12)
		sh.global_position = Vector3(f.pos.x, 0.015, 0.0)
		sh.scale = Vector3(w, 1.0, w * 0.55)
		(sh.material_override as StandardMaterial3D).albedo_color = Color(0, 0, 0, clampf(0.5 - hgt * 0.03, 0.22, 0.5))
		sh.visible = views[i].visible and f.state != Fighter.S.DOWN and f.state != Fighter.S.KO
	_storm_step()
	for fl in _flashes:
		var n: MeshInstance3D = fl["node"]
		if not n.visible:
			continue
		fl["t"] += delta
		var t: float = fl["t"] / fl["life"]
		if t >= 1.0:
			n.visible = false
			continue
		n.scale = Vector3.ONE * float(fl["size"]) * (1.0 + t * 0.8)
		var c: Color = fl["color"]
		(fl["mat"] as StandardMaterial3D).albedo_color = Color(c.r, c.g, c.b, 1.0 - t)
	for l in _lights:
		var ol: OmniLight3D = l["light"]
		if not ol.visible:
			continue
		l["t"] += delta
		var k: float = l["t"] / l["life"]
		ol.light_energy = float(l["energy"]) * maxf(1.0 - k, 0.0) ** 2
		if k >= 1.0:
			ol.visible = false
	for v in _proj:
		var p: Projectile = v["proj"]
		if p == null:
			continue
		if p.dead or not p in sim.projectiles:
			v["proj"] = null
			(v["node"] as Node3D).visible = false
			(v["trail"] as GPUParticles3D).emitting = false
			(v["chain"] as MeshInstance3D).visible = false
			continue
		var pos := _v3(p.pos, 0.0)
		var node: Node3D = v["node"]
		node.global_position = pos
		(v["trail"] as GPUParticles3D).global_position = pos
		var solid: MeshInstance3D = v["solid"]
		match p.kind:
			"sickle":
				solid.rotation = Vector3(0, 0, _time * 22.0 * -p.facing)
			"harpoon":
				solid.rotation = Vector3(0, 0, -PI * 0.5 * p.facing)
			"anchor":
				solid.rotation = Vector3(0, 0, 0)
			"bubble":
				solid.scale = Vector3.ONE * (1.0 + sin(_time * 9.0) * 0.06)
			"wave":
				(v["core"] as MeshInstance3D).scale = Vector3(1.0 + sin(_time * 14.0) * 0.12, 1.0, 1.0)
		var chain: MeshInstance3D = v["chain"]
		if chain.visible:
			var owner := views[p.owner]
			var hand := owner.strike_point("hand_r")
			var to := pos - hand
			var len := to.length()
			if len > 0.05:
				var y := to / len
				var x := y.cross(Vector3.BACK).normalized()
				chain.global_transform = Transform3D(Basis(x, y, x.cross(y)).scaled_local(Vector3(1, len, 1)), hand + to * 0.5)
