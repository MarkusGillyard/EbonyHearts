extends TestCase
## Every combo trial in data/trials.json can be performed as written: the steps are entered
## against the fighter's rival standing still, each at the first tick it can start (a chain
## link in its window, a special in a cancel window after contact, anything else once the
## fighter is free), and the listed hits land as one combo. The checker itself is tested too.


func test_checker_counts_hits_in_order_and_drops() -> void:
	var c := TrialCheck.new(["hp", "hp2", "ashka_elbow_chain"], 0)
	assert_eq(c.feed({"type": "hit", "by": 0, "move": "hp", "combo": 1}), "hit")
	assert_eq(c.feed({"type": "hit", "by": 1, "move": "hp2", "combo": 2}), "", "the other fighter's hits do not count")
	assert_eq(c.feed({"type": "hit", "by": 0, "move": "lp", "combo": 2}), "dropped", "the wrong move drops the attempt")
	assert_eq(c.progress, 0)
	assert_eq(c.feed({"type": "hit", "by": 0, "move": "hp", "combo": 1}), "hit")
	assert_eq(c.feed({"type": "hit", "by": 0, "move": "hp2", "combo": 2}), "hit")
	assert_eq(c.feed({"type": "combo_end", "who": 1}), "dropped", "the combo ending drops it")
	assert_eq(c.feed({"type": "hit", "by": 0, "move": "hp", "combo": 1}), "hit")
	assert_eq(c.feed({"type": "hit", "by": 0, "move": "hp2", "combo": 2}), "hit")
	assert_eq(c.feed({"type": "hit", "by": 0, "move": "ashka_elbow_chain", "combo": 3}), "done")
	assert_true(c.done)
	var one := TrialCheck.new(["vat_press"], 0)
	assert_eq(one.feed({"type": "hit", "by": 0, "move": "vat_press", "combo": 1, "throw": true}), "done", "a one-hit trial")


func test_combo_end_is_reported() -> void:
	var h := SimHarness.new()
	h.place(0.9)
	h.step(0, SimHarness.HP)
	h.idle(12)
	assert_eq(h.count("hit"), 1)
	h.idle(40)
	assert_eq(h.count("combo_end"), 1, "the combo ends when the hit fighter returns to neutral")


func test_every_trial_is_possible() -> void:
	var total := 0
	for id in Content.roster():
		var rival := str(Content.story.get("fighters", {}).get(id, {}).get("rival", "gorm"))
		for t in Content.trials.get(id, []):
			total += 1
			var ok := _perform(id, rival, t)
			assert_true(ok, "%s: %s (%s)" % [id, t["name"], ", ".join(t["steps"])])
	assert_true(total >= 24, "the roster has trials (%d)" % total)


func _perform(id: String, rival: String, t: Dictionary) -> bool:
	var h := SimHarness.new(Content.fighter(id), Content.fighter(rival))
	h.sim.infinite_run = true
	# From touching distance, where a player dials a combo.
	h.place(0.4)
	h.sim.step([Vector3i.ZERO, Vector3i.ZERO])
	var check := TrialCheck.new(t["moves"], 0)
	var intent := PadIntent.new()
	var steps: Array = t["steps"]
	var si := 0
	var waited := 0
	for tick in 700:
		var f := h.f(0)
		if not intent.busy() and si < steps.size():
			if _can_start(f, str(steps[si])):
				intent.command(str(steps[si]))
				si += 1
				waited = 0
			else:
				waited += 1
				if waited > 150:
					return false
		var pad: Vector3i = intent.next(f.facing) if intent.busy() else PadIntent.pad(5, f.facing)
		h.sim.step([pad, Vector3i.ZERO])
		for e in h.sim.events:
			if check.feed(e) == "done":
				return true
	return false


## Whether `notation` can begin now (or within the ticks its motion takes to enter).
static func _can_start(f: Fighter, notation: String) -> bool:
	if f.is_actionable():
		return true
	if f.state != Fighter.S.MOVE or f.move == null or f.hitstop > 0:
		return false
	var m := f.move
	# A link is dialled once the blow before it has landed (a player confirms the hit).
	if f.move_contact == "":
		return false
	for link in m.chain:
		if str(link["input"]) == notation and f.mf >= int(link["from"]) - 1 and f.mf <= int(link["to"]) - 1:
			return true
	if m.cancel != Vector2i.ZERO and f.move_contact != "":
		var c := Command.parse(notation)
		var lead := c.steps.size() + 2
		for cmd in f.def.commands:
			var target := f.def.move(cmd.move)
			if cmd.text == notation and target and target.kind == "special" and f.mf >= m.cancel.x - lead and f.mf <= m.cancel.y - 1:
				return true
	return false
