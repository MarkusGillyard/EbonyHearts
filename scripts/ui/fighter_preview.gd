class_name FighterPreview
extends SubViewportContainer
## A live 3D view of one fighter for menus: the sculpted model in its stance, breathing, lit like
## a stage, on its own small world. show_fighter() swaps who is shown; celebrate() plays the win
## pose for a moment.

var viewport: SubViewport
var _world: Node3D
var _sim: FightSim
var _view: FighterView
var _cam: Camera3D
var _id := ""
var _costume := false
var _celebrate := 0.0
var _turn := 0.0
var facing := 1


func _init(size_px := Vector2i(560, 760)) -> void:
	stretch = true
	custom_minimum_size = size_px
	size = size_px
	viewport = SubViewport.new()
	viewport.size = size_px
	viewport.own_world_3d = true
	viewport.transparent_bg = true
	viewport.msaa_3d = Viewport.MSAA_4X
	add_child(viewport)
	_world = Node3D.new()
	viewport.add_child(_world)
	var env := WorldEnvironment.new()
	var e := Environment.new()
	e.background_mode = Environment.BG_CLEAR_COLOR
	e.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	e.ambient_light_color = Color(0.32, 0.36, 0.34)
	e.ambient_light_energy = 0.6
	e.tonemap_mode = Environment.TONE_MAPPER_AGX
	env.environment = e
	_world.add_child(env)
	var key := DirectionalLight3D.new()
	key.light_color = Color(1.0, 0.86, 0.7)
	key.light_energy = 1.6
	key.rotation_degrees = Vector3(-30, 35, 0)
	_world.add_child(key)
	var rim := DirectionalLight3D.new()
	rim.light_color = Color(0.5, 0.85, 0.4)
	rim.light_energy = 1.4
	rim.rotation_degrees = Vector3(-10, 200, 0)
	_world.add_child(rim)
	var fill := DirectionalLight3D.new()
	fill.light_color = Color(0.7, 0.75, 0.85)
	fill.light_energy = 0.5
	fill.rotation_degrees = Vector3(-15, -60, 0)
	_world.add_child(fill)
	_cam = Camera3D.new()
	_cam.fov = 30.0
	_world.add_child(_cam)
	_cam.current = true


func show_fighter(id: String, costume := false) -> void:
	if id == _id and costume == _costume:
		return
	_id = id
	_costume = costume
	if _view:
		_view.queue_free()
		_view = null
	if _sim:
		_sim.dispose()
		_sim = null
	var def := Content.fighter(id)
	if def == null:
		return
	_sim = FightSim.new()
	_sim.training = true
	_sim.intro = false
	_sim.setup(def, def)
	_sim.phase = FightSim.Phase.FIGHT
	var f := _sim.fighters[0]
	f.pos = Vector2.ZERO
	f.facing = facing
	f.set_state(Fighter.S.IDLE)
	_sim.fighters[1].pos = Vector2(30.0 * facing, 0)
	_view = FighterView.new()
	_world.add_child(_view)
	_view.setup(f, Content.common_poses)
	if costume:
		_view.set_costume(0.42, 1.05)
	var h := def.height
	var dist := 2.6 + h * 1.1
	_cam.position = Vector3(0.0, h * 0.55, dist)
	_cam.look_at(Vector3(0.0, h * 0.5, 0.0))


func celebrate() -> void:
	_celebrate = 1.6
	if _sim:
		_sim.fighters[0].set_state(Fighter.S.WIN)


func _process(delta: float) -> void:
	if _view == null:
		return
	if _celebrate > 0.0:
		_celebrate -= delta
		if _celebrate <= 0.0:
			_sim.fighters[0].set_state(Fighter.S.IDLE)
	_view.sync_from_sim()
	_view.update_visual(1.0, delta)
	_turn += delta
	var h := _view.def.height
	var dist := 2.6 + h * 1.1
	var a := sin(_turn * 0.35) * 0.18 + 0.25 * facing
	_cam.position = Vector3(sin(a) * dist, h * 0.55, cos(a) * dist)
	_cam.look_at(Vector3(0.0, h * 0.5, 0.0))


func _exit_tree() -> void:
	if _sim:
		_sim.dispose()
