extends TestCase
## Round and match flow: knockouts, the dizzy finish and finishers, timeouts, double knockouts,
## rematch cleanup, and the special mechanics that ride on the round (parry, armour, teleport,
## the boss's health-gated storm).

const H = preload("res://tests/sim_harness.gd")


func _match(d1: FighterDef = null, d2: FighterDef = null) -> SimHarness:
	var h := SimHarness.new(d1, d2, false)
	return h


func _ko(h: SimHarness, loser: int) -> void:
	h.f(loser).health = 1
	h.place(0.85)
	if loser == 1:
		h.step(0, H.HP)
	else:
		h.step(0, 0, 0, H.HP)
	h.idle(10)


func test_ko_ends_round_and_starts_next() -> void:
	var h := _match()
	_ko(h, 1)
	assert_eq(h.sim.phase, FightSim.Phase.ROUND_END, "a knockout in the first round ends it")
	assert_eq(h.sim.wins[0], 1)
	assert_true(h.until(func(): return h.sim.round_num == 2, 600), "the next round begins")
	assert_eq(h.f(1).health, h.f(1).def.health, "health is restored for round two")
	assert_eq(h.sim.projectiles.size(), 0)


func test_deciding_ko_gives_finish_and_finisher_ends_match() -> void:
	var h := _match()
	h.sim.wins = [1, 0]
	_ko(h, 1)
	assert_eq(h.sim.phase, FightSim.Phase.FINISH, "the deciding knockout leaves the loser to be finished")
	assert_true(h.until(func(): return h.f(1).state == Fighter.S.DIZZY, 300), "the loser stands up dizzy")
	assert_eq(h.find("finish_prompt")["pronoun"], h.f(1).def.pronoun)
	# Enter the dummy finisher (F,F,D+HP up close).
	h.place(0.9)
	h.f(1).set_state(Fighter.S.DIZZY)
	h.f(1).health = 0
	h.motion(0, [H.R, 0, H.R, H.D], H.HP)
	assert_eq(h.sim.phase, FightSim.Phase.FINISHER, "the finisher begins")
	assert_true(h.until(func(): return h.sim.phase == FightSim.Phase.MATCH_END, 900), "and the match ends after it")
	var me := h.find("match_end")
	assert_eq(me["winner"], 0)
	assert_eq(me["finisher"], "fin")


func test_finisher_needs_its_range() -> void:
	var h := _match()
	h.sim.wins = [1, 0]
	_ko(h, 1)
	h.until(func(): return h.f(1).state == Fighter.S.DIZZY, 300)
	h.place(3.0)
	h.f(1).set_state(Fighter.S.DIZZY)
	h.motion(0, [H.R, 0, H.R, H.D], H.HP)
	assert_ne(h.sim.phase, FightSim.Phase.FINISHER, "a close-range finisher does nothing from across the stage")
	assert_eq(h.count("finisher_range"), 1, "and says why")


func test_finish_times_out_and_a_blow_also_ends_it() -> void:
	var h := _match()
	h.sim.wins = [1, 0]
	_ko(h, 1)
	h.until(func(): return h.f(1).state == Fighter.S.DIZZY, 300)
	assert_true(h.until(func(): return h.sim.phase == FightSim.Phase.ROUND_END, FightSim.FINISH_TICKS + 60), "the dizzy spell runs out")
	var h2 := _match()
	h2.sim.wins = [1, 0]
	_ko(h2, 1)
	h2.until(func(): return h2.f(1).state == Fighter.S.DIZZY, 300)
	h2.place(0.85)
	h2.f(1).set_state(Fighter.S.DIZZY)
	h2.step(0, H.HP)
	h2.idle(8)
	assert_eq(h2.sim.phase, FightSim.Phase.ROUND_END, "any blow on the dizzy fighter ends the match without a finisher")


func test_timeout_and_draw() -> void:
	var h := _match()
	h.f(1).health = 600
	h.sim.timer = 2
	h.idle(4)
	assert_eq(h.find("timeout")["winner"], 0, "the healthier fighter wins on time")
	var h2 := _match()
	h2.sim.timer = 2
	h2.idle(4)
	assert_eq(h2.find("timeout")["winner"], -1, "equal health is a draw")
	assert_eq(h2.sim.wins, [0, 0])


func test_double_ko() -> void:
	var h := _match()
	h.f(0).health = 1
	h.f(1).health = 1
	h.place(0.85)
	h.step(0, H.HP, 0, H.HP)
	h.idle(10)
	assert_eq(h.find("round_end")["how"], "double_ko")
	assert_eq(h.sim.wins, [0, 0], "nobody wins a double knockout")


func test_match_cannot_loop_forever() -> void:
	var h := _match()
	h.sim.round_num = FightSim.MAX_ROUNDS
	h.sim.timer = 2
	h.idle(4)
	assert_true(h.until(func(): return h.sim.phase == FightSim.Phase.MATCH_END, 900), "after the last round the match ends even without a winner")
	assert_eq(h.find("match_end")["winner"], -1)


func test_restart_clears_everything() -> void:
	var h := SimHarness.new()
	h.place(3.0)
	h.motion(0, [H.D, H.R], H.HP)
	h.idle(14)
	h.step(0, H.LP)
	assert_true(h.sim.projectiles.size() > 0)
	h.f(1).health = 300
	h.f(0).run_meter = 10.0
	h.f(0).buff = "armor"
	h.f(0).buff_ticks = 100
	h.sim.restart_round()
	assert_eq(h.sim.projectiles.size(), 0, "no projectile survives a restart")
	for f in h.sim.fighters:
		assert_eq(f.health, f.def.health)
		assert_eq(f.run_meter, Fighter.RUN_MAX)
		assert_eq(f.input.press_age(InputBuffer.ALL, 60), -1, "input history is cleared")
		assert_eq(f.move, null)
		assert_eq(f.buff, "")
		assert_eq(f.projectiles_out, 0)


func test_parry_counters_and_buff_armour_absorbs() -> void:
	var bram := Content.fighter("bram")
	var h := SimHarness.new(SimHarness.dummy_def("a"), bram)
	h.place(0.85)
	h.f(1).start_move(bram.move("stand_down"))
	h.idle(2)
	h.step(0, H.HP)
	h.idle(4)
	assert_eq(h.count("parry"), 1, "Stand Down catches a high punch")
	assert_eq(h.f(1).move.id if h.f(1).move else "", "stand_down_strike", "and answers with the baton")
	h.idle(20)
	assert_true(h.f(0).health < 1000, "the counter lands")
	var gorm := Content.fighter("gorm")
	var g := SimHarness.new(SimHarness.dummy_def("a"), gorm)
	g.place(3.0)
	g.f(1).start_move(gorm.move("tallow_hide"))
	g.idle(40)
	assert_eq(g.f(1).buff, "armor", "Tallow Hide hardens Gorm")
	g.place(0.95)
	g.f(1).buff = "armor"
	g.f(1).buff_ticks = 200
	g.f(1).buff_hits = 2
	g.step(0, H.HP)
	g.idle(8)
	assert_eq(g.count("armor"), 1, "the first blow is absorbed")
	assert_ne(g.f(1).state, Fighter.S.HITSTUN, "without flinching")


func test_sinkstep_reappears_behind() -> void:
	var nix := Content.fighter("nix")
	var h := SimHarness.new(nix, SimHarness.dummy_def("b"))
	h.place(2.0)
	h.f(0).start_move(nix.move("sinkstep"))
	h.until(func(): return h.count("teleport") > 0, 60)
	assert_true(h.f(0).pos.x > h.f(1).pos.x, "Nix comes up on the far side")
	assert_eq(h.f(0).facing, -1, "facing the opponent")


func test_storm_needs_half_health() -> void:
	var bat := Content.fighter("slopbat")
	var h := SimHarness.new(SimHarness.dummy_def("a"), bat)
	var storm := bat.move("storm_shroud")
	assert_false(h.f(1).can_use(storm), "no storm at full health")
	h.f(1).health = int(bat.health * 0.4)
	assert_true(h.f(1).can_use(storm), "the storm comes below half health")
	h.place(3.0)
	h.f(1).start_move(storm)
	h.idle(10)
	h.step(0, H.LP)
	h.f(0).pos.x = h.f(1).pos.x - 0.9
	h.step(0, H.HP)
	h.idle(10)
	assert_ne(h.f(1).move.id if h.f(1).move else "", "storm_shroud", "a blow during the charge breaks it")


func test_boss_cannot_be_thrown() -> void:
	var bat := Content.fighter("slopbat")
	var h := SimHarness.new(SimHarness.dummy_def("a"), bat)
	h.place(0.9)
	h.step(H.R, H.LP)
	h.idle(5)
	assert_eq(h.count("throw"), 0)


func test_deciding_ko_clears_the_losers_projectiles() -> void:
	var h := _match()
	h.sim.wins = [1, 0]
	h.place(5.0)
	h.motion(1, [H.D, H.L], H.HP)
	h.idle(14)
	assert_eq(h.sim.projectiles.size(), 1, "the loser-to-be has a shot in flight")
	h.f(1).health = 0
	h.idle(1)
	assert_eq(h.sim.phase, FightSim.Phase.FINISH)
	h.idle(4)
	assert_eq(h.sim.projectiles.size(), 0, "it dies with the round")
	assert_eq(h.f(0).health, h.f(0).max_health(), "and the winner is untouched")


func test_flawless_match_means_no_damage_all_match() -> void:
	var h := _match()
	h.f(0).health -= 50
	h.f(0).damage_taken += 50
	_ko(h, 1)
	assert_true(h.until(func(): return h.sim.round_num == 2 and h.sim.phase == FightSim.Phase.FIGHT, 900))
	_ko(h, 1)
	assert_eq(h.sim.phase, FightSim.Phase.FINISH, "the second knockout decides it")
	assert_true(h.until(func(): return h.f(1).state == Fighter.S.DIZZY, 400), "the loser stands dizzy")
	h.place(0.85)
	h.f(1).set_state(Fighter.S.DIZZY)
	h.step(0, H.HP)
	assert_true(h.until(func(): return h.sim.phase == FightSim.Phase.MATCH_END, 1500), "a blow ends the match")
	var me := h.find("match_end")
	assert_eq(me["winner"], 0)
	assert_false(me["flawless"], "a last round without a scratch is not a flawless match")
