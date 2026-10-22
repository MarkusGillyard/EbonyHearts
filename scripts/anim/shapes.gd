class_name Shapes
extends RefCounted
## Procedural meshes and materials for props. Fighters themselves are sculpted models
## (CharacterModel).


## Umbrella canopy: a shallow dome of `radius` opening downward from the origin, stretched over
## `ribs` ribs with the paper sagging a little between them. `open_amt` 0..1 unfolds it from a
## slim furled cone `furled_len` long, the panels folding into pleats. UV.x runs round the
## canopy (one unit per panel), UV.y from the tip (0) to the rim (1).
static func canopy(radius: float, ribs: int, open_amt: float, furled_len: float) -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var rings := 10
	var per := 6
	var seg := ribs * per
	var spread := lerpf(0.07, 1.0, open_amt)
	var drop := lerpf(furled_len, radius * 0.36, open_amt)
	for i in rings + 1:
		var t := float(i) / rings
		for j in seg + 1:
			var a := TAU * j / seg
			var panel := float(j % per) / per
			var between := sin(panel * PI)
			# Open, the paper sags between ribs; furled, it folds outward into pleats.
			var sag := lerpf(0.5, -0.04, open_amt) * between * t
			var r := radius * t * spread * (1.0 - 0.035 * between * t * open_amt) + sag * radius * 0.06 * (1.0 - open_amt)
			var y := -drop * t * t - sag * radius * 0.05 * open_amt
			st.set_uv(Vector2(float(j) / per, t))
			st.add_vertex(Vector3(cos(a) * r, y, sin(a) * r))
	for i in rings:
		for j in seg:
			var a := i * (seg + 1) + j
			var b := a + 1
			var c := a + seg + 1
			var d := c + 1
			st.add_index(a)
			st.add_index(b)
			st.add_index(c)
			st.add_index(b)
			st.add_index(d)
			st.add_index(c)
	st.generate_normals()
	return st.commit()


## Material with a soft rim, for props.
static func material(color: Color, roughness := 0.7, metallic := 0.0, emission := 0.0, rim := 0.35) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = color
	m.roughness = roughness
	m.metallic = metallic
	m.rim_enabled = rim > 0.0
	m.rim = rim
	m.rim_tint = 0.6
	# Emission stays enabled (at zero energy when unused) so effects such as frost only change
	# parameters; toggling the feature at run time would compile a new shader mid-fight.
	m.emission_enabled = true
	m.emission = color if emission > 0.0 else Color.BLACK
	m.emission_energy_multiplier = emission
	return m
