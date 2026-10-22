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
		_:
			return null
	return root


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


## Shows or hides a prop from the pose ("prop" 1 shows held props whose spec says "posed").
static func update(node: Node3D, pose: Pose, _view: Node3D) -> void:
	var spec: Dictionary = node.get_meta("spec")
	if spec.get("posed", false):
		node.visible = pose.val["prop"] > 0.5


## The striking end of a prop in world space.
static func tip(node: Node3D) -> Vector3:
	var spec: Dictionary = node.get_meta("spec")
	var l := float(spec.get("tip", 0.5))
	return node.global_transform * Vector3(0, -l, 0)
