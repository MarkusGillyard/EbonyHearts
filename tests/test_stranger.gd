extends TestCase
## The stranger in white: he cannot be hurt, he steps aside once, then answers; his memory moves
## forward across meetings and survives a damaged progress file being treated as new.

const H = preload("res://tests/sim_harness.gd")

var _saved := {}


func before_each() -> void:
	_saved = Progress.data.duplicate(true)
	Progress.data = Progress.fresh()


func after_each() -> void:
	Progress.data = _saved


func _encounter() -> Array:
	var h := SimHarness.new(Content.fighter("ashka"), Content.fighter("daxion"))
	var rules := StrangerRules.new(h.sim, null)
	h.sim.hooks = rules
	h.sim.training = true
	h.place(1.0)
	return [h, rules]


func _attack(h: SimHarness) -> void:
	h.place(0.9)
	h.step(0, H.HK)
	h.idle(30)


func test_first_meeting_evades_then_strikes() -> void:
	var e := _encounter()
	var h: SimHarness = e[0]
	assert_eq(int(Progress.data["stranger"]), 1, "meeting him is remembered")
	_attack(h)
	assert_eq(h.count("stranger_evade"), 1, "the first blow is stepped away from")
	assert_eq(h.f(1).health, h.f(1).def.health, "he takes no damage")
	assert_eq(int(Progress.data["stranger"]), 2)
	var said := false
	for ev in h.log:
		said = said or (ev["type"] == "line" and ev["text"] == "Careful, child.")
	assert_true(said, "with a warning")
	h.idle(20)
	_attack(h)
	assert_eq(h.count("stranger_strike"), 1, "the next blow is answered")
	assert_true(h.until(func(): return h.f(0).health <= 0, 120), "with one strike")
	assert_eq(int(Progress.data["stranger"]), 3)


func test_later_meetings_remember() -> void:
	Progress.data["stranger"] = 3
	var e := _encounter()
	var h: SimHarness = e[0]
	h.idle(80)
	var asked := false
	for ev in h.log:
		asked = asked or (ev["type"] == "line" and ev["text"] == "What have you learned, child?")
	assert_true(asked, "after being struck down: what have you learned")
	assert_eq(int(Progress.data["stranger"]), 4)
	_attack(h)
	assert_eq(h.count("stranger_crack"), 1, "then the whole of Find the Crack")
	assert_true(h.until(func(): return h.f(0).health <= 0, 200))
	assert_eq(int(Progress.data["stranger"]), 5)
	var e2 := _encounter()
	var h2: SimHarness = e2[0]
	h2.idle(400)
	var line := false
	for ev in h2.log:
		line = line or (ev["type"] == "line" and ev["text"] == "It seems I'm not the only immortal bastard around here.")
	assert_true(line, "and then he recognises a fellow immortal, in those words")
	assert_eq(int(Progress.data["stranger"]), 6)


func test_memory_only_moves_forward_and_bowing_out() -> void:
	Progress.data["stranger"] = 5
	Progress.set_stranger(2)
	assert_eq(int(Progress.data["stranger"]), 5, "memory never goes back")
	var e := _encounter()
	var h: SimHarness = e[0]
	h.hold[0] = H.BL
	h.idle(StrangerRules.BOW_TICKS + 5)
	h.hold[0] = 0
	assert_eq(h.find("match_end").get("winner", 99), -1, "holding block bows out and ends the encounter")


func test_projectiles_do_not_hurt_him() -> void:
	var e := _encounter()
	var h: SimHarness = e[0]
	h.place(3.0)
	h.motion(0, [H.D, H.R], H.HP)
	h.idle(60)
	assert_eq(h.f(1).health, h.f(1).def.health)
	assert_eq(h.count("stranger_evade"), 1)
