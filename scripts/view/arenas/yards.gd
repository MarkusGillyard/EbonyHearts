extends Arena
## The Rendering Yards of Tallowmarket: an open iron floor between the great vats where the Sump's
## slop is boiled down, under drizzle and furnace light. Gorm Tallow's home ground. The vat behind
## the centre of the floor takes part in the stage finisher.

const IRON := Color(0.2, 0.19, 0.18)
const RUST := Color(0.3, 0.17, 0.1)

func build() -> void:
	music = "music_yards"
	make_environment({"top": Color(0.01, 0.012, 0.015), "horizon": Color(0.12, 0.09, 0.07), "glow_color": Color(1.0, 0.42, 0.12),
		"glow_dir": Vector3(-0.5, 0.1, -1.0), "glow_size": 0.35, "glow_energy": 0.9, "clouds": 0.6},
		Color(0.16, 0.12, 0.09), 0.018, 0.25)
	make_key(Color(1.0, 0.74, 0.5), 1.7, Vector3(-40, -40, 0))
	make_fill(Color(0.75, 0.8, 0.85), 0.55)
	make_rim(Color(0.45, 0.6, 0.85), 0.9, Vector3(-20, 160, 0))

	var floor_mat := surface(3, Color(0.27, 0.25, 0.23), Color(0.16, 0.14, 0.13), 1.0, 0.7, 0.3, 0.5)
	box(Vector3(26, 0.4, 9), Vector3(0, -0.2, -1.0), floor_mat)
	# A raised kerb behind the fighting strip and a gutter of slop in front of the vats.
	var kerb := surface(1, Color(0.3, 0.29, 0.27), Color(0.18, 0.18, 0.17), 1.4, 0.8, 0.4)
	box(Vector3(26, 0.25, 0.5), Vector3(0, 0.12, -3.2), kerb)
	plane(Vector2(26, 1.4), Vector3(0, 0.02, -4.1), slop(Color(0.03, 0.05, 0.015), Color(0.4, 0.8, 0.1), 0.9, 0.18))
	box(Vector3(26, 0.6, 9), Vector3(0, -0.5, -9.0), kerb)

	var vat_mat := surface(3, Color(0.24, 0.17, 0.12), Color(0.12, 0.09, 0.07), 1.2, 0.55, 0.2, 0.7)
	var band := surface(0, Color(0.12, 0.11, 0.1), Color(0.06, 0.06, 0.05), 1.0, 0.45, 0.0, 0.9)
	var slop_mat := slop(Color(0.03, 0.06, 0.015), Color(0.5, 0.95, 0.15), 1.8, 0.3)
	for x in [-9.5, -4.6, 0.0, 4.6, 9.5]:
		var z := -6.6 if absf(x) > 1.0 else -6.2
		var r := 1.75 if absf(x) > 1.0 else 2.0
		cyl(r, 3.2, Vector3(x, 1.6, z), vat_mat, Vector3.ZERO, -1.0, 32)
		for y in [0.5, 1.6, 2.7]:
			cyl(r + 0.05, 0.14, Vector3(x, y, z), band, Vector3.ZERO, -1.0, 32)
		cyl(r + 0.15, 0.22, Vector3(x, 3.25, z), band, Vector3.ZERO, -1.0, 32)
		var top := MeshInstance3D.new()
		var disc := CylinderMesh.new()
		disc.top_radius = r - 0.08
		disc.bottom_radius = r - 0.08
		disc.height = 0.02
		disc.radial_segments = 32
		top.mesh = disc
		top.material_override = slop_mat
		top.position = Vector3(x, 3.18, z)
		add_child(top)
		var glow := omni(Vector3(x, 3.9, z + 0.6), Color(0.55, 1.0, 0.2), 1.6, 5.0)
		flicker(glow, 1.6, 0.12, 2.0)
		plume(Vector3(x, 3.6, z), Color(0.55, 0.65, 0.45, 0.5), 18, r * 0.5, 0.6, 1.6)
		# Spill down the side facing the floor.
		box(Vector3(0.5, 2.6, 0.05), Vector3(x + 0.3, 1.9, z + r - 0.02), slop_mat, Vector3(0, 0, 3))

	# Pipes along the back and a catwalk above the vats.
	var pipe := surface(0, Color(0.32, 0.2, 0.12), Color(0.18, 0.11, 0.07), 0.8, 0.5, 0.0, 0.8)
	cyl(0.22, 26.0, Vector3(0, 4.6, -7.8), pipe, Vector3(0, 0, 90))
	cyl(0.14, 26.0, Vector3(0, 5.1, -8.1), pipe, Vector3(0, 0, 90))
	var walk := surface(3, Color(0.2, 0.19, 0.18), Color(0.1, 0.1, 0.1), 0.6, 0.6, 0.1, 0.7)
	box(Vector3(26, 0.12, 1.4), Vector3(0, 4.0, -5.0), walk)
	for x in range(-12, 13, 2):
		cyl(0.04, 1.0, Vector3(x, 4.55, -4.35), band)
		if x % 4 == 0:
			cyl(0.08, 4.0, Vector3(x + 1.0, 2.0, -4.35), band)
	box(Vector3(26, 0.05, 0.05), Vector3(0, 5.05, -4.35), band)
	var chain_mat := Shapes.material(Color(0.22, 0.2, 0.18), 0.5, 0.85)
	for x in [-6.8, -2.2, 2.6, 6.6]:
		chain(Vector3(x, 3.95, -3.9), 2.2 + fmod(absf(x), 1.3), chain_mat)

	# The far wall with furnace mouths, and smokestacks against the glow.
	var brick := surface(4, Color(0.3, 0.15, 0.1), Color(0.16, 0.08, 0.06), 1.0, 0.9, 0.15)
	box(Vector3(34, 12, 1.0), Vector3(0, 5.5, -11.5), brick)
	for x in [-11.0, -5.5, 5.5, 11.0]:
		box(Vector3(2.2, 1.6, 0.2), Vector3(x, 1.3, -10.95), emissive(Color(1.0, 0.42, 0.1), 3.0))
		var fl := omni(Vector3(x, 1.6, -9.8), Color(1.0, 0.5, 0.18), 2.4, 6.0)
		flicker(fl, 2.4, 0.25, 7.0)
	for x in [-14.0, -7.0, 8.5, 15.0]:
		cyl(0.9, 14.0, Vector3(x, 7.0, -16.0), brick, Vector3.ZERO, 0.75)
		plume(Vector3(x, 14.2, -16.0), Color(0.2, 0.18, 0.16, 0.6), 14, 0.6, 1.0, 3.0)

	# The ends of the floor: stacked drums at the walls, so the boundary reads.
	var drum := surface(3, Color(0.35, 0.2, 0.1), Color(0.2, 0.12, 0.06), 0.7, 0.6, 0.3, 0.6)
	for side in [-1.0, 1.0]:
		var bx: float = side * (FightSim.STAGE_HALF + 0.7)
		box(Vector3(0.9, 3.6, 3.0), Vector3(bx + side * 0.25, 1.8, -1.2), surface(1, Color(0.25, 0.24, 0.22), Color(0.14, 0.13, 0.12), 1.0, 0.8, 0.3))
		for k in 3:
			cyl(0.32, 0.9, Vector3(bx + side * 0.2, 0.45, 1.2 + k * 0.7), drum)
		cyl(0.32, 0.9, Vector3(bx + side * 0.25, 1.35, 1.5), drum)
	# A low foreground pipe, far below the fight.
	cyl(0.16, 30.0, Vector3(0, 0.12, 3.4), pipe, Vector3(0, 0, 90))
	rain(1200)
	apply_quality(str(Settings.get_value("graphics")))
