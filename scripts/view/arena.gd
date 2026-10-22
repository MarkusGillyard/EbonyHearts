class_name Arena
extends Node3D
## Base for the stages: environment, lighting and building helpers. Each stage is a diorama around
## a flat fighting strip (x within FightSim.STAGE_HALF, z around 0) seen from +z. Background pieces
## sit behind z = -2; anything in front of z = 2 stays low so it never hides the fight.
## Settings "graphics" decides soft shadows, ambient occlusion and fog volumes.

const STAGES := {
	"rendering_yards": "res://scripts/view/arenas/yards.gd",
	"drowned_chapel": "res://scripts/view/arenas/chapel.gd",
	"stilt_market": "res://scripts/view/arenas/market.gd",
	"roost": "res://scripts/view/arenas/roost.gd",
}
const NAMES := {"rendering_yards": "The Rendering Yards", "drowned_chapel": "The Drowned Chapel",
	"stilt_market": "Lowmere Stilt Market", "roost": "The Slopbat's Roost"}

var id := ""
var music := "music_yards"
var environment: Environment
var key: DirectionalLight3D
var rim: DirectionalLight3D
var _animated: Array = []
var _time := 0.0


static func create(stage_id: String) -> Arena:
	var path: String = STAGES.get(stage_id, STAGES["rendering_yards"])
	if not ResourceLoader.exists(path):
		path = STAGES["rendering_yards"]
	var a: Arena = load(path).new()
	a.id = stage_id
	a.name = "Arena"
	return a


static func display_name(stage_id: String) -> String:
	return NAMES.get(stage_id, stage_id.capitalize())


## Builds the stage. Subclasses override.
func build() -> void:
	pass


## Per-frame animation (flicker, drifting props).
func step(delta: float) -> void:
	_time += delta
	for a in _animated:
		(a as Callable).call(_time, delta)


# --- Environment and light --------------------------------------------------------------------

func make_environment(sky_params: Dictionary, fog_color: Color, fog_density: float, ambient := 0.35) -> void:
	var we := WorldEnvironment.new()
	environment = Environment.new()
	var sky := Sky.new()
	var sm := ShaderMaterial.new()
	sm.shader = preload("res://assets/shaders/backdrop_sky.gdshader")
	for k in sky_params:
		sm.set_shader_parameter(k, sky_params[k])
	sky.sky_material = sm
	sky.radiance_size = Sky.RADIANCE_SIZE_128
	environment.background_mode = Environment.BG_SKY
	environment.sky = sky
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	environment.ambient_light_energy = ambient
	environment.reflected_light_source = Environment.REFLECTION_SOURCE_SKY
	environment.tonemap_mode = Environment.TONE_MAPPER_AGX
	environment.tonemap_exposure = 1.0
	environment.glow_enabled = true
	environment.glow_intensity = 0.5
	environment.glow_bloom = 0.04
	environment.glow_hdr_threshold = 1.2
	environment.glow_blend_mode = Environment.GLOW_BLEND_MODE_SOFTLIGHT
	environment.fog_enabled = true
	environment.fog_light_color = fog_color
	environment.fog_density = fog_density
	environment.fog_sky_affect = 0.6
	environment.adjustment_enabled = true
	environment.adjustment_contrast = 1.08
	environment.adjustment_saturation = 1.05
	we.environment = environment
	add_child(we)


func make_key(color: Color, energy: float, rot_deg: Vector3) -> DirectionalLight3D:
	key = DirectionalLight3D.new()
	key.light_color = color
	key.light_energy = energy
	key.shadow_enabled = true
	key.shadow_bias = 0.05
	key.shadow_normal_bias = 2.0
	key.shadow_blur = 1.5
	key.directional_shadow_mode = DirectionalLight3D.SHADOW_PARALLEL_2_SPLITS
	key.directional_shadow_max_distance = 30.0
	key.directional_shadow_split_1 = 0.3
	key.directional_shadow_blend_splits = true
	key.rotation_degrees = rot_deg
	add_child(key)
	return key


## A soft, shadowless light from the camera's side so the fighters always read against the
## stage, whatever the stage's own lighting does.
func make_fill(color: Color, energy: float) -> DirectionalLight3D:
	var fill := DirectionalLight3D.new()
	fill.light_color = color
	fill.light_energy = energy
	fill.light_specular = 0.25
	fill.shadow_enabled = false
	fill.rotation_degrees = Vector3(-12, 8, 0)
	add_child(fill)
	return fill


func make_rim(color: Color, energy: float, rot_deg: Vector3) -> DirectionalLight3D:
	rim = DirectionalLight3D.new()
	rim.light_color = color
	rim.light_energy = energy
	rim.light_specular = 0.9
	rim.rotation_degrees = rot_deg
	add_child(rim)
	return rim


func omni(pos: Vector3, color: Color, energy: float, rng: float, shadows := false) -> OmniLight3D:
	var l := OmniLight3D.new()
	l.position = pos
	l.light_color = color
	l.light_energy = energy
	l.omni_range = rng
	l.omni_attenuation = 1.4
	l.shadow_enabled = shadows
	add_child(l)
	return l


func spot(pos: Vector3, target: Vector3, color: Color, energy: float, rng: float, angle: float) -> SpotLight3D:
	var l := SpotLight3D.new()
	l.light_color = color
	l.light_energy = energy
	l.spot_range = rng
	l.spot_angle = angle
	l.spot_attenuation = 1.2
	add_child(l)
	l.global_transform = Transform3D(Basis.looking_at(target - pos, Vector3.UP if absf((target - pos).normalized().y) < 0.95 else Vector3.BACK), pos)
	return l


## A light that flickers like a flame or a failing lamp.
func flicker(l: Light3D, base: float, amount: float, speed := 9.0) -> void:
	var seed := randf() * 100.0
	_animated.append(func(t: float, _d: float):
		l.light_energy = base * (1.0 + amount * (sin(t * speed + seed) * 0.5 + sin(t * speed * 2.7 + seed * 1.3) * 0.3 + sin(t * speed * 6.1) * 0.2)))


func apply_quality(q: String) -> void:
	var high := q != "low"
	environment.ssao_enabled = high
	environment.ssao_radius = 1.0
	environment.ssao_intensity = 1.4
	environment.volumetric_fog_enabled = false
	if key:
		key.shadow_blur = 1.5 if high else 0.5
	RenderingServer.sub_surface_scattering_set_quality(RenderingServer.SUB_SURFACE_SCATTERING_QUALITY_MEDIUM if high else RenderingServer.SUB_SURFACE_SCATTERING_QUALITY_DISABLED)


# --- Building pieces ----------------------------------------------------------------------------

static func surface(pattern: int, a: Color, b: Color, scale := 1.0, rough := 0.85, wet := 0.0, metal := 0.0) -> ShaderMaterial:
	var m := ShaderMaterial.new()
	m.shader = preload("res://assets/shaders/surface.gdshader")
	m.set_shader_parameter("pattern", pattern)
	m.set_shader_parameter("color_a", a)
	m.set_shader_parameter("color_b", b)
	m.set_shader_parameter("scale", scale)
	m.set_shader_parameter("roughness", rough)
	m.set_shader_parameter("wet", wet)
	m.set_shader_parameter("metallic", metal)
	return m


static func slop(deep: Color, glow: Color, energy := 1.4, speed := 0.25) -> ShaderMaterial:
	var m := ShaderMaterial.new()
	m.shader = preload("res://assets/shaders/slop.gdshader")
	m.set_shader_parameter("deep", deep)
	m.set_shader_parameter("glow", glow)
	m.set_shader_parameter("glow_energy", energy)
	m.set_shader_parameter("speed", speed)
	return m


static func emissive(color: Color, energy: float) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = color * 0.3
	m.emission_enabled = true
	m.emission = color
	m.emission_energy_multiplier = energy
	return m


func box(size: Vector3, pos: Vector3, mat: Material, rot_deg := Vector3.ZERO, shadows := true) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var b := BoxMesh.new()
	b.size = size
	mi.mesh = b
	mi.material_override = mat
	mi.position = pos
	mi.rotation_degrees = rot_deg
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON if shadows else GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(mi)
	return mi


func cyl(radius: float, height: float, pos: Vector3, mat: Material, rot_deg := Vector3.ZERO, top := -1.0, segs := 24, shadows := true) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var c := CylinderMesh.new()
	c.top_radius = radius if top < 0.0 else top
	c.bottom_radius = radius
	c.height = height
	c.radial_segments = segs
	c.rings = 1
	mi.mesh = c
	mi.material_override = mat
	mi.position = pos
	mi.rotation_degrees = rot_deg
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON if shadows else GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(mi)
	return mi


func plane(size: Vector2, pos: Vector3, mat: Material, rot_deg := Vector3.ZERO) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var p := PlaneMesh.new()
	p.size = size
	mi.mesh = p
	mi.material_override = mat
	mi.position = pos
	mi.rotation_degrees = rot_deg
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(mi)
	return mi


func sphere(radius: float, pos: Vector3, mat: Material, squash := Vector3.ONE) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var s := SphereMesh.new()
	s.radius = radius
	s.height = radius * 2.0
	mi.mesh = s
	mi.material_override = mat
	mi.position = pos
	mi.scale = squash
	add_child(mi)
	return mi


## A hanging chain of `links` from `top` straight down, swaying gently.
func chain(top: Vector3, length: float, mat: Material) -> Node3D:
	var root := Node3D.new()
	root.position = top
	add_child(root)
	var n := int(length / 0.09)
	for i in n:
		var t := MeshInstance3D.new()
		var tm := TorusMesh.new()
		tm.inner_radius = 0.022
		tm.outer_radius = 0.04
		tm.rings = 8
		tm.ring_segments = 6
		t.mesh = tm
		t.material_override = mat
		t.position = Vector3(0, -i * 0.075, 0)
		t.rotation_degrees = Vector3(90, 90 * (i % 2), 0)
		t.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		root.add_child(t)
	var seed := randf() * 10.0
	_animated.append(func(t: float, _d: float): root.rotation = Vector3(sin(t * 0.7 + seed) * 0.05, 0, sin(t * 0.9 + seed * 2.0) * 0.06))
	return root


## Falling rain over the whole stage (streaks lit by whatever is around).
func rain(amount: int, color := Color(0.6, 0.7, 0.75, 0.35)) -> GPUParticles3D:
	var p := GPUParticles3D.new()
	p.amount = amount if Settings.get_value("effects") != "reduced" else amount / 3
	p.lifetime = 1.1
	p.visibility_aabb = AABB(Vector3(-14, -1, -10), Vector3(28, 14, 16))
	var pm := ParticleProcessMaterial.new()
	pm.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_BOX
	pm.emission_box_extents = Vector3(13, 0.2, 6)
	pm.direction = Vector3(0.12, -1, 0)
	pm.spread = 3.0
	pm.initial_velocity_min = 11.0
	pm.initial_velocity_max = 14.0
	pm.gravity = Vector3.ZERO
	pm.particle_flag_align_y = true
	pm.color = color
	p.process_material = pm
	var q := QuadMesh.new()
	q.size = Vector2(0.012, 0.42)
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	m.vertex_color_use_as_albedo = true
	q.material = m
	p.draw_pass_1 = q
	p.position = Vector3(0, 11, -2)
	p.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(p)
	return p


## Rising steam or smoke from a point.
func plume(pos: Vector3, color: Color, amount := 24, spread := 0.6, rise := 0.8, size := 1.2) -> GPUParticles3D:
	var p := GPUParticles3D.new()
	p.amount = amount if Settings.get_value("effects") != "reduced" else maxi(4, amount / 3)
	p.lifetime = 4.0
	p.visibility_aabb = AABB(Vector3(-4, -1, -4), Vector3(8, 12, 8))
	var pm := ParticleProcessMaterial.new()
	pm.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_SPHERE
	pm.emission_sphere_radius = spread
	pm.direction = Vector3.UP
	pm.spread = 15.0
	pm.initial_velocity_min = rise * 0.6
	pm.initial_velocity_max = rise
	pm.gravity = Vector3(0.05, 0.05, 0)
	pm.scale_min = size * 0.6
	pm.scale_max = size * 1.4
	var grow := Curve.new()
	grow.add_point(Vector2(0, 0.3))
	grow.add_point(Vector2(1, 1.0))
	var gt := CurveTexture.new()
	gt.curve = grow
	pm.scale_curve = gt
	pm.color = color
	var g := Gradient.new()
	g.set_color(0, Color(1, 1, 1, 0))
	g.set_color(1, Color(1, 1, 1, 0))
	g.add_point(0.2, Color(1, 1, 1, 1))
	var ramp := GradientTexture1D.new()
	ramp.gradient = g
	pm.color_ramp = ramp
	p.process_material = pm
	var q := QuadMesh.new()
	var m := StandardMaterial3D.new()
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	m.vertex_color_use_as_albedo = true
	m.roughness = 1.0
	var img := Image.create(64, 64, false, Image.FORMAT_RGBA8)
	for y in 64:
		for x in 64:
			var d := Vector2(x - 31.5, y - 31.5).length() / 32.0
			img.set_pixel(x, y, Color(1, 1, 1, clampf(1.0 - d, 0.0, 1.0) ** 2 * 0.5))
	m.albedo_texture = ImageTexture.create_from_image(img)
	q.material = m
	p.draw_pass_1 = q
	p.position = pos
	p.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(p)
	return p
