extends TestCase
## The fighting rules: commands, timing, hit and block, damage, chains and run, throws,
## projectiles and juggles.

const H = preload("res://tests/sim_harness.gd")


func _started(h: SimHarness, who: int, move_id: String, since := 0) -> bool:
	for i in range(since, h.log.size()):
		var e: Dictionary = h.log[i]
		if e["type"] == "move" and e["who"] == who and e["move"] == move_id:
			return true
	return false


# --- Commands ------------------------------------------------------------------------------------

func test_motion_special_from_either_side() -> void:
	var h := SimHarness.new()
	h.place(3.0)
	h.motion(0, [H.D, H.D | H.R, H.R], H.HP)
	assert_true(_started(h, 0, "orb"), "D,F+HP facing right fires the projectile")
	# Fighter 2 faces left: forward is screen-left.
	h.place(3.0)
	var at := h.log.size()
	h.motion(1, [H.D, H.D | H.L, H.L], H.HP)
	assert_true(_started(h, 1, "orb", at), "D,F+HP facing left reads forward as screen-left")
	h.place(3.0)
	at = h.log.size()
	h.motion(1, [H.D, H.R], H.HP)
	assert_false(_started(h, 1, "orb", at), "the unmirrored motion does not fire for the left side")
	assert_true(_started(h, 1, "hp", at), "pressing HP while holding back still gives the high punch")


func test_motion_survives_crossover() -> void:
	var h := SimHarness.new()
	h.place(3.0)
	h.step(H.D)
	h.step(H.D)
	# The opponent is moved behind mid-motion: the fighter turns before forward is pressed.
	h.f(1).pos.x = h.f(0).pos.x - 1.5
	h.step(H.D | H.R)
	h.step(H.R, H.HP)
	assert_true(_started(h, 0, "orb"), "a motion begun before the crossover still comes out")


func test_motion_window_expires() -> void:
	var h := SimHarness.new()
	h.place(3.0)
	h.step(H.D)
	h.idle(Command.STEP_GAP + 6)
	h.step(H.R)
	h.step(H.R, H.HP)
	assert_false(_started(h, 0, "orb"), "a down tap long before forward is not a motion")


func test_held_direction_commands() -> void:
	var h := SimHarness.new()
	h.place(3.0)
	h.step(H.D, H.HP)
	assert_true(_started(h, 0, "uppercut"), "D+HP is the uppercut")
	h.place(3.0)
	var at := h.log.size()
	h.step(H.L, H.LK)
	assert_true(_started(h, 0, "sweep", at), "B+LK is the sweep")
	h.place(3.0)
	at = h.log.size()
	h.step(H.L | H.D, H.LK)
	assert_true(_started(h, 0, "crouch_lk", at), "down-back + LK is a crouching kick, not a sweep")


func test_press_is_buffered_during_recovery() -> void:
	var h := SimHarness.new()
	h.place(3.0)
	h.step(0, H.HK)
	var m := h.f(0).move
	assert_eq(m.id, "hk")
	for i in m.total - 3:
		h.step()
	var at := h.log.size()
	h.step(0, H.LK)
	h.idle(5)
	assert_true(_started(h, 0, "lk", at), "LK pressed three frames before recovery ends still comes out")


# --- Timing, hit and block -----------------------------------------------------------------------

func test_strike_connects_on_its_startup_frame() -> void:
	var h := SimHarness.new()
	h.place(0.9)
	h.step(0, H.HP)
	var hp := h.f(0).def.move("hp")
	for i in hp.startup - 2:
		h.step()
	assert_eq(h.count("hit"), 0, "no hit before the first active frame")
	h.step()
	assert_eq(h.count("hit"), 1, "the high punch hits on frame %d" % hp.startup)
	assert_eq(h.f(1).health, 1000 - hp.damage, "full damage on the first hit")
	h.idle(10)
	assert_eq(h.count("hit"), 1, "one active strike never hits twice")


func test_out_of_range_whiffs() -> void:
	var h := SimHarness.new()
	h.place(2.5)
	h.step(0, H.HP)
	h.idle(20)
	assert_eq(h.count("hit"), 0)


func test_high_misses_crouchers_and_low_beats_standing_block() -> void:
	var h := SimHarness.new()
	h.place(0.9)
	h.step(0, H.HP, H.D, 0)
	h.idle(12, 0, H.D)
	assert_eq(h.count("hit") + h.count("block"), 0, "a high punch passes over a crouching fighter")
	h.place(0.9)
	h.hold[1] = H.BL
	h.step(H.L, H.LK)
	h.idle(14)
	assert_eq(h.count("hit"), 1, "a sweep is not stopped by a standing block")
	h.place(0.9)
	var at := h.log.size()
	h.step(H.L, H.LK, H.D, 0)
	h.idle(14, 0, H.D)
	assert_eq(h.count("block", at), 1, "a crouching block stops the sweep")
	h.hold[1] = 0


func test_overhead_beats_crouching_block() -> void:
	var h := SimHarness.new()
	h.place(2.4)
	h.hold[1] = H.BL
	# A forward jump and an early kick come down on the crouching blocker.
	h.step(H.U | H.R, 0, H.D, 0)
	h.idle(3, 0, H.D)
	for i in 14:
		h.step(0, H.HK if i == 2 else 0, H.D, 0)
	assert_true(h.until(func(): return h.count("hit") + h.count("block") > 0, 40, 0, H.D), "the jump kick reaches")
	assert_eq(h.count("hit"), 1, "a jump kick is not stopped by a crouching block")
	h.hold[1] = 0


func test_block_chips_only_specials_and_pushes_back() -> void:
	var h := SimHarness.new()
	h.place(0.9)
	h.hold[1] = H.BL
	var x0 := h.f(1).pos.x
	h.step(0, H.HK)
	h.idle(16)
	assert_eq(h.count("block"), 1)
	assert_eq(h.f(1).health, 1000, "normals do no chip damage")
	assert_true(h.f(1).pos.x > x0 + 0.2, "blocking pushes the defender back")
	h.place(3.0)
	h.motion(0, [H.D, H.R], H.HP)
	h.until(func(): return h.count("block") >= 2, 80)
	assert_true(h.f(1).health < 1000, "a blocked special chips")
	h.hold[1] = 0


func test_trade_hits_both() -> void:
	var h := SimHarness.new()
	h.place(0.85)
	h.step(0, H.HP, 0, H.HP)
	h.idle(8)
	assert_eq(h.count("hit"), 2, "strikes landing on the same frame both connect")
	assert_true(h.f(0).health < 1000 and h.f(1).health < 1000)


# --- Damage, chains and run ---------------------------------------------------------------------

func test_chain_costs_run_meter_and_needs_it() -> void:
	var h := SimHarness.new()
	h.place(0.85)
	h.step(0, H.HP)
	h.idle(5)
	h.step(0, H.HP)
	h.until(func(): return h.count("chain") > 0, 12)
	assert_eq(h.f(0).move.id, "hp2", "HP, HP chains")
	assert_true(h.f(0).run_meter < Fighter.RUN_MAX, "the chain link spent run meter")
	h.place(3.0)
	h.f(0).run_meter = 0.0
	h.f(0).run_idle = 0
	h.step(0, H.HP)
	h.idle(5)
	h.step(0, H.HP)
	h.idle(2)
	assert_ne(h.f(0).move.id if h.f(0).move else "", "hp2", "no chain without run meter")


func test_combo_damage_scales() -> void:
	var h := SimHarness.new()
	h.place(0.85)
	h.f(1).combo_hits = 5
	h.f(1).set_state(Fighter.S.HITSTUN)
	h.f(1).stun = 30
	h.step(0, H.HP)
	h.idle(5)
	var e := h.find("hit")
	assert_eq(e["combo"], 6)
	assert_true(e["damage"] < h.f(0).def.move("hp").damage, "the sixth hit of a combo does reduced damage")


func test_run_drains_and_refills() -> void:
	var h := SimHarness.new()
	h.place(5.0)
	h.hold[0] = H.RN
	h.idle(30, H.R)
	assert_eq(h.f(0).state, Fighter.S.RUN)
	assert_true(h.f(0).run_meter < Fighter.RUN_MAX - 20.0, "running drains the meter")
	h.hold[0] = 0
	var low := h.f(0).run_meter
	h.idle(Fighter.RUN_REGEN_DELAY - 2)
	assert_eq(h.f(0).run_meter, low, "no refill right after running")
	h.idle(60)
	assert_true(h.f(0).run_meter >= minf(Fighter.RUN_MAX, low + 30.0), "the meter refills once left alone")


func test_run_stops_when_meter_empties() -> void:
	var h := SimHarness.new()
	h.place(5.6, -3.0)
	h.hold[0] = H.RN
	h.f(0).run_meter = 10.0
	h.f(0).run_idle = 0
	h.idle(12, H.R)
	h.idle(4, H.R)
	assert_ne(h.f(0).state, Fighter.S.RUN, "an empty meter ends the run")
	h.hold[0] = 0


# --- Throws --------------------------------------------------------------------------------------

func test_throw_and_break() -> void:
	var h := SimHarness.new()
	h.place(0.7)
	h.step(H.R, H.LP)
	h.idle(4)
	assert_eq(h.count("throw"), 1, "forward + LP up close throws")
	h.idle(60)
	assert_eq(h.f(1).health, 1000 - int(h.f(0).def.move("throw_f").throw_data["damage"]))
	h.place(0.7)
	var at := h.log.size()
	h.step(H.R, H.LP)
	h.idle(4)
	h.step(0, 0, 0, H.LP)
	h.idle(3)
	assert_eq(h.count("throw_break", at), 1, "pressing a punch just after being grabbed breaks free")
	h.place(0.7)
	at = h.log.size()
	h.step(H.R, H.LP, H.L, H.LP)
	h.idle(4)
	assert_eq(h.count("throw_break", at), 1, "two throws at once break apart")


func test_throw_needs_range_and_back_throw_swaps() -> void:
	var h := SimHarness.new()
	h.place(2.0)
	h.step(H.R, H.LP)
	h.idle(4)
	assert_eq(h.count("throw"), 0, "no throw from across the screen")
	assert_eq(h.f(0).move.id if h.f(0).move else "", "lp", "it is a low punch instead")
	h.place(0.7)
	h.step(H.L, H.LP)
	h.idle(70)
	assert_true(h.f(1).pos.x < h.f(0).pos.x, "a back throw lands the victim on the other side")


func test_command_grab_beats_block() -> void:
	var h := SimHarness.new()
	h.place(0.8)
	h.hold[1] = H.BL
	h.motion(0, [H.L, H.R], H.LP)
	h.idle(12)
	assert_eq(h.count("throw"), 1, "a command grab takes a blocking opponent")
	h.hold[1] = 0


# --- Projectiles ---------------------------------------------------------------------------------

func test_one_projectile_at_a_time_and_clash() -> void:
	var h := SimHarness.new()
	h.place(5.6)
	h.motion(0, [H.D, H.R], H.HP)
	h.idle(20, 0, H.D)
	assert_eq(h.sim.projectiles.size(), 1)
	h.idle(12, 0, H.D)
	var at := h.log.size()
	h.motion(0, [H.D, H.R], H.HP)
	h.idle(4)
	assert_false(_started(h, 0, "orb", at), "a second projectile waits for the first")
	h.place(5.0)
	h.motion(0, [H.D, H.R], H.HP)
	h.motion(1, [H.D, H.L], H.HP)
	h.until(func(): return h.count("clash") > 0, 60)
	assert_eq(h.count("clash"), 1, "projectiles meeting head-on destroy each other")
	h.idle(2)
	assert_eq(h.sim.projectiles.size(), 0)


func test_high_projectile_ducked_low_projectile_blocked_crouching() -> void:
	var h := SimHarness.new()
	h.place(4.0)
	h.motion(0, [H.D, H.R], H.HP)
	h.idle(60, 0, H.D)
	assert_eq(h.count("hit") + h.count("block"), 0, "crouching under a high projectile")
	h.place(4.0)
	h.hold[1] = H.BL
	h.motion(0, [H.D, H.L], H.LP)
	h.idle(60, 0, H.D)
	assert_eq(h.count("block"), 1, "a low projectile is blocked crouching")
	h.hold[1] = 0


# --- Juggles -------------------------------------------------------------------------------------

func test_uppercut_launches_and_juggle_limit_holds() -> void:
	var h := SimHarness.new()
	h.place(0.8)
	h.step(H.D, H.HP)
	h.idle(10)
	assert_eq(h.f(1).state, Fighter.S.JUGGLE, "the uppercut launches")
	assert_true(h.until(func(): return h.f(1).pos.y > 1.5, 40), "high into the air")
	h.f(1).juggle_points = Fighter.JUGGLE_LIMIT
	assert_true(h.f(1).is_invulnerable(), "a body past the juggle limit cannot be hit again")
	h.until(func(): return h.f(1).state == Fighter.S.DOWN, 200)
	assert_eq(h.f(1).state, Fighter.S.DOWN, "it lands knocked down")
	assert_true(h.until(func(): return h.f(1).is_actionable(), 120), "and gets back up")


func test_determinism() -> void:
	var traces := []
	for run in 2:
		var h := SimHarness.new()
		h.place(2.0)
		var rng := RandomNumberGenerator.new()
		rng.seed = 1234
		var out := []
		for i in 900:
			var d1: int = [0, H.R, H.L, H.D, H.U, H.D | H.R][rng.randi_range(0, 5)]
			var d2: int = [0, H.R, H.L, H.D][rng.randi_range(0, 3)]
			var b1: int = [0, 0, 0, H.HP, H.LK, H.HK, H.LP][rng.randi_range(0, 6)]
			var b2: int = [0, 0, H.BL, H.LP][rng.randi_range(0, 3)]
			h.step(d1, b1, d2, b2)
			out.append([h.f(0).pos, h.f(1).pos, h.f(0).health, h.f(1).health, h.f(0).state, h.f(1).state])
		traces.append(out)
	assert_eq(traces[0], traces[1], "the same inputs give the same match")


func test_thrower_hit_mid_throw_lets_go() -> void:
	var h := SimHarness.new()
	h.place(0.7)
	h.step(H.R, H.LP)
	h.idle(8)
	assert_eq(h.f(1).state, Fighter.S.THROWN, "the throw has hold of its victim")
	# Something else hits the thrower (a returning sickle, a falling anchor).
	h.f(0).take_stun(20, "hit")
	h.idle(2)
	assert_eq(h.f(0).throw_victim, null, "the thrower lets go")
	assert_eq(h.f(1).thrown_by, null)
	assert_ne(h.f(1).state, Fighter.S.THROWN, "and the victim is free")
	assert_true(h.until(func(): return h.f(1).is_actionable(), 400), "and back on their feet")


func test_harmless_hits_stay_harmless() -> void:
	var d := SimHarness.dummy_def("pusher", {"moves": {"gust": {"kind": "special", "startup": 3, "active": 3,
		"recovery": 10, "level": "mid", "damage": 0, "hitstun": 10, "hitboxes": [{"rect": [0.2, 0.5, 1.0, 1.0]}]}},
		"commands": [["F,F+HK", "gust"]]})
	var h := SimHarness.new(d)
	h.place(1.0)
	h.motion(0, [H.R, 0, H.R], H.HK)
	h.idle(10)
	assert_eq(h.count("hit"), 1, "the gust connects")
	assert_eq(h.f(1).health, 1000, "and does no damage, even as the first hit of a combo")
