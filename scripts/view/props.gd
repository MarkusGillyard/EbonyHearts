class_name Props
extends RefCounted
## Hand-held and worn props built from primitives: weapons, lanterns, tools. A prop is a Node3D
## attached to a rig joint; its spec comes from the fighter's look ("props"). Poses may show or
## hide a prop through the "prop" value (1 shown, the default when the spec says "always").


static func build(spec: Dictionary, rig: Rig) -> Node3D:
	var h := rig.h / 1.8
	var root := Node3D.new()
	root.name = "Prop_" + str(spec.get("kind", "prop"))
	var off: Array = spec.get("offset", [0, 0, 0])
	var rot: Array = spec.get("rot", [0, 0, 0])
	root.position = Vector3(float(off[0]), float(off[1]), float(off[2])) * h
	root.rotation_degrees = Vector3(float(rot[0]), float(rot[1]), float(rot[2]))
	var mats: Dictionary = rig.look.get("materials", {})
	match str(spec.get("kind", "")):
		"sickle":
			var wood := _mat(mats, "wood", "#3a2a1c", 0.7)
			var steel := _mat(mats, "steel", "#9aa3a8", 0.3, 0.9)
			_part(root, _cyl(0.014, 0.016, 0.34), wood, Vector3(0, -0.12, 0) * h, Vector3.ZERO)
			var blade := _blade(0.26 * h, 0.05 * h)
			_part(root, blade, steel, Vector3(0, -0.29, 0) * h, Vector3(0, 0, 0))
		"baton":
			var wood := _mat(mats, "baton", "#2b1d14", 0.55)
			var brass := _mat(mats, "brass", "#b08a3e", 0.3, 0.9)
			_part(root, _cyl(0.017, 0.022, 0.6), wood, Vector3(0, -0.24, 0) * h, Vector3.ZERO)
			_part(root, _cyl(0.024, 0.024, 0.03), brass, Vector3(0, 0.03, 0) * h, Vector3.ZERO)
			_part(root, _cyl(0.025, 0.025, 0.025), brass, Vector3(0, -0.53, 0) * h, Vector3.ZERO)
		"lantern":
			var iron := _mat(mats, "iron", "#2a2826", 0.5, 0.8)
			var glass := StandardMaterial3D.new()
			glass.albedo_color = Color(1.0, 0.75, 0.35)
			glass.emission_enabled = true
			glass.emission = Color(1.0, 0.62, 0.25)
			glass.emission_energy_multiplier = 2.2
			_part(root, _cyl(0.05, 0.05, 0.11), glass, Vector3(0, -0.12, 0) * h, Vector3.ZERO)
			_part(root, _cyl(0.035, 0.06, 0.03), iron, Vector3(0, -0.05, 0) * h, Vector3.ZERO)
			_part(root, _cyl(0.06, 0.06, 0.02), iron, Vector3(0, -0.185, 0) * h, Vector3.ZERO)
			var light := OmniLight3D.new()
			light.light_color = Color(1.0, 0.68, 0.32)
			light.light_energy = float(spec.get("light", 0.6))
			light.omni_range = 1.6
			light.position = Vector3(0, -0.12, 0) * h
			root.add_child(light)
		"harpoon":
			var brass := _mat(mats, "brass", "#a8823a", 0.35, 0.9)
			var iron := _mat(mats, "iron", "#3a3836", 0.5, 0.8)
			_part(root, _cyl(0.03, 0.035, 0.42), brass, Vector3(0, -0.1, 0.0) * h, Vector3(90, 0, 0))
			_part(root, _cyl(0.008, 0.008, 0.3), iron, Vector3(0, -0.1, -0.33) * h, Vector3(90, 0, 0))
			_part(root, _cyl(0.0, 0.025, 0.06), iron, Vector3(0, -0.1, -0.5) * h, Vector3(-90, 0, 0))
		"portholes":
			# Glass in a diving helmet's ports, lit faintly from inside; "ports" lists
			# [x, y, z, nx, ny, nz, radius] in the head joint's frame (1.8 m units).
			var glass := StandardMaterial3D.new()
			glass.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
			var gc := Color(str(mats.get("glass", {}).get("color", "#16382a")))
			glass.albedo_color = Color(gc.r, gc.g, gc.b, 0.55)
			glass.roughness = 0.04
			glass.metallic_specular = 0.9
			glass.emission_enabled = true
			glass.emission = Color(str(spec.get("glow", "#62e6a0")))
			glass.emission_energy_multiplier = float(spec.get("glass_glow", 0.3))
			for port: Array in spec.get("ports", []):
				var mi := MeshInstance3D.new()
				var r := float(port[6]) * h
				mi.mesh = _cyl(r, r, 0.004 * h)
				mi.material_override = glass
				var n := Vector3(float(port[3]), float(port[4]), float(port[5])).normalized()
				mi.transform = Transform3D(Basis(Quaternion(Vector3.UP, n)), Vector3(float(port[0]), float(port[1]), float(port[2])) * h)
				root.add_child(mi)
			var inner := OmniLight3D.new()
			inner.light_color = Color(str(spec.get("glow", "#62e6a0")))
			inner.light_energy = float(spec.get("light", 0.5))
			inner.omni_range = 0.32 * h
			inner.position = Vector3(0, 0.1, -0.06) * h
			root.add_child(inner)
		"wing":
			_build_wing(root, spec, mats, h)
		_:
			return null
	return root


# --- Bat wing -------------------------------------------------------------------------------------
# A wing in the prop's frame (x outward on its "side", y up, z toward the back): an arm from the
# root to the wrist, four finger spars fanning from the wrist, and a membrane between the spars
# and down to the body. "open" 0 folds it at the back with the wrist above the shoulder and the
# fingers hanging to the hips; 1 spreads it wide and up. Lengths are in 1.8 m units.

const WING_FOLD := {"elbow": Vector3(0.1, -0.02, 0.18), "wrist": Vector3(0.12, 0.4, 0.14),
	"fingers": [Vector3(0.06, -1.0, 0.22), Vector3(0.12, -1.0, 0.18), Vector3(0.17, -1.0, 0.14), Vector3(0.22, -1.0, 0.1)]}
const WING_SPREAD := {"elbow": Vector3(0.42, 0.14, 0.12), "wrist": Vector3(0.84, 0.4, 0.06),
	"fingers": [Vector3(0.55, 0.8, 0.02), Vector3(0.92, 0.25, 0.05), Vector3(0.85, -0.45, 0.08), Vector3(0.35, -0.95, 0.08)]}
const WING_LENGTHS := [0.6, 0.78, 0.74, 0.62]
const WING_BODY := Vector3(0.04, -0.42, 0.07)


static func _build_wing(root: Node3D, spec: Dictionary, mats: Dictionary, h: float) -> void:
	var membrane := _mat(mats, "membrane", "#1b2217", 0.42)
	membrane.cull_mode = BaseMaterial3D.CULL_DISABLED
	membrane.backlight_enabled = true
	membrane.backlight = Color(str(mats.get("membrane", {}).get("backlight", "#1f3a12")))
	var bone := _mat(mats, "spar", "#121810", 0.3)
	var claw := _mat(mats, "claw", "#2a2620", 0.25)
	var skin := MeshInstance3D.new()
	skin.name = "Membrane"
	skin.material_override = membrane
	root.add_child(skin)
	# Spars: humerus, forearm, four fingers; unit-length cylinders stretched each pose.
	for i in 6:
		var r0 := 0.03 if i < 2 else 0.014
		var mi := _part(root, _cyl(r0 * 0.6 * h, r0 * h, 1.0), bone, Vector3.ZERO, Vector3.ZERO)
		mi.name = "Spar%d" % i
	var hook := _part(root, _cyl(0.0, 0.016 * h, 0.12 * h), claw, Vector3.ZERO, Vector3.ZERO)
	hook.name = "Claw"
	root.set_meta("wing_side", signf(float(spec.get("side", 1.0))))
	root.set_meta("wing_h", h)
	_pose_wing(root, 0.0)


static func _pose_wing(root: Node3D, u: float) -> void:
	var side: float = root.get_meta("wing_side")
	var h: float = root.get_meta("wing_h")
	var e := smoothstep(0.0, 1.0, u)
	var flip := Vector3(side, 1.0, 1.0)
	var elbow: Vector3 = (WING_FOLD["elbow"] as Vector3).lerp(WING_SPREAD["elbow"], e) * flip * h
	var wrist: Vector3 = (WING_FOLD["wrist"] as Vector3).lerp(WING_SPREAD["wrist"], e) * flip * h
	var tips: Array[Vector3] = []
	for i in 4:
		var d: Vector3 = (WING_FOLD["fingers"][i] as Vector3).normalized().lerp((WING_SPREAD["fingers"][i] as Vector3).normalized(), e).normalized()
		tips.append(wrist + d * flip * float(WING_LENGTHS[i]) * h)
	var body := WING_BODY * flip * h
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	# Panels between neighbouring fingers, fanning from the wrist, with scalloped trailing edges.
	for i in 3:
		_wing_patch(st, func(a: float, b: float) -> Vector3:
			var edge: Vector3 = tips[i].lerp(tips[i + 1], a)
			edge = edge.lerp(wrist, 0.28 * sin(PI * a))
			return wrist.lerp(edge, b), 6, 5)
	# The main membrane: from the arm and the last finger down to the body.
	var top := func(a: float) -> Vector3:
		return Vector3.ZERO.lerp(elbow, a * 2.0) if a < 0.5 else elbow.lerp(wrist, a * 2.0 - 1.0)
	_wing_patch(st, func(a: float, b: float) -> Vector3:
		var t: Vector3 = top.call(a)
		var bottom := body.lerp(tips[3], a)
		bottom = bottom.lerp((Vector3.ZERO + wrist) * 0.5, 0.22 * sin(PI * a))
		var inner := Vector3.ZERO.lerp(body, b)
		var outer := wrist.lerp(tips[3], b)
		return t * (1.0 - b) + bottom * b + inner * (1.0 - a) + outer * a \
			- (Vector3.ZERO * (1.0 - a) * (1.0 - b) + wrist * a * (1.0 - b) + body * (1.0 - a) * b + tips[3] * a * b), 8, 7)
	st.generate_normals()
	(root.get_node("Membrane") as MeshInstance3D).mesh = st.commit()
	var segs := [[Vector3.ZERO, elbow], [elbow, wrist]]
	for i in 4:
		segs.append([wrist, tips[i]])
	for i in 6:
		var a: Vector3 = segs[i][0]
		var b: Vector3 = segs[i][1]
		var mi: MeshInstance3D = root.get_node("Spar%d" % i)
		mi.transform = Transform3D(Basis(Quaternion(Vector3.UP, (b - a).normalized())).scaled(Vector3(1.0, (b - a).length(), 1.0)), (a + b) * 0.5)
	var hook: MeshInstance3D = root.get_node("Claw")
	var up := (Vector3(0.0, 1.0, -0.6) + Vector3(side * 0.2, 0, 0)).normalized()
	hook.transform = Transform3D(Basis(Quaternion(Vector3.UP, up)), wrist + up * 0.05 * h)
	root.set_meta("wing_open", u)


static func _wing_patch(st: SurfaceTool, f: Callable, nu: int, nv: int) -> void:
	for j in nv:
		for i in nu:
			var a0 := float(i) / nu
			var a1 := float(i + 1) / nu
			var b0 := float(j) / nv
			var b1 := float(j + 1) / nv
			var p00: Vector3 = f.call(a0, b0)
			var p10: Vector3 = f.call(a1, b0)
			var p01: Vector3 = f.call(a0, b1)
			var p11: Vector3 = f.call(a1, b1)
			st.add_vertex(p00); st.add_vertex(p10); st.add_vertex(p01)
			st.add_vertex(p10); st.add_vertex(p11); st.add_vertex(p01)


static func _mat(mats: Dictionary, key: String, fallback: String, rough: float, metal := 0.0) -> StandardMaterial3D:
	var spec: Dictionary = mats.get(key, {})
	return Shapes.material(Color(str(spec.get("color", fallback))), float(spec.get("roughness", rough)), float(spec.get("metallic", metal)))


static func _cyl(top: float, bottom: float, height: float) -> CylinderMesh:
	var c := CylinderMesh.new()
	c.top_radius = top
	c.bottom_radius = bottom
	c.height = height
	c.radial_segments = 16
	return c


## A curved blade in the local xy plane: a crescent swept from the haft.
static func _blade(length: float, width: float) -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var n := 12
	var pts_out := []
	var pts_in := []
	for i in n + 1:
		var t := float(i) / n
		var a := t * PI * 0.62
		var r := length * 0.55
		var c := Vector3(-r * 0.6, 0, 0)
		pts_out.append(c + Vector3(cos(a) * r, -sin(a) * r * 0.9, 0))
		var w := width * (1.0 - t * 0.92)
		pts_in.append(c + Vector3(cos(a) * (r - w), -sin(a) * (r - w) * 0.9, 0))
	for side in [1.0, -1.0]:
		for i in n:
			var o0: Vector3 = pts_out[i] + Vector3(0, 0, 0.003 * side)
			var o1: Vector3 = pts_out[i + 1] + Vector3(0, 0, 0.003 * side)
			var i0: Vector3 = pts_in[i] + Vector3(0, 0, 0.003 * side)
			var i1: Vector3 = pts_in[i + 1] + Vector3(0, 0, 0.003 * side)
			if side > 0.0:
				st.add_vertex(o0); st.add_vertex(i0); st.add_vertex(o1)
				st.add_vertex(o1); st.add_vertex(i0); st.add_vertex(i1)
			else:
				st.add_vertex(o0); st.add_vertex(o1); st.add_vertex(i0)
				st.add_vertex(o1); st.add_vertex(i1); st.add_vertex(i0)
	st.generate_normals()
	return st.commit()


static func _part(parent: Node3D, mesh: Mesh, mat: Material, pos: Vector3, rot_deg: Vector3) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.mesh = mesh
	mi.material_override = mat
	mi.position = pos
	mi.rotation_degrees = rot_deg
	parent.add_child(mi)
	return mi


## Shows or hides a prop from the pose ("prop" 1 shows held props whose spec says "posed"); a
## wing reads "prop" as how far it is spread (the animator's crossfade makes it change smoothly).
static func update(node: Node3D, pose: Pose, _view: Node3D) -> void:
	var spec: Dictionary = node.get_meta("spec")
	if str(spec.get("kind", "")) == "wing":
		var u := clampf(pose.val["prop"], 0.0, 1.0)
		if absf(u - float(node.get_meta("wing_open"))) > 0.004:
			_pose_wing(node, u)
	elif spec.get("posed", false):
		node.visible = pose.val["prop"] > 0.5


## The striking end of a prop in world space.
static func tip(node: Node3D) -> Vector3:
	var spec: Dictionary = node.get_meta("spec")
	var l := float(spec.get("tip", 0.5))
	return node.global_transform * Vector3(0, -l, 0)
