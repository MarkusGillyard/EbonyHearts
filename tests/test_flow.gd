extends TestCase
## Game flow and persistence: the arcade ladder, progress files (atomic, versioned, recovering
## from damage, test profiles kept apart) and settings.

var _saved := {}
var _saved_path := ""


func before_each() -> void:
	_saved = Progress.data.duplicate(true)
	_saved_path = Progress.path
	Progress.path = "user://progress_testcase.json"


func after_each() -> void:
	DirAccess.remove_absolute(ProjectSettings.globalize_path("user://progress_testcase.json"))
	DirAccess.remove_absolute(ProjectSettings.globalize_path("user://progress_testcase.json.bad"))
	Progress.path = _saved_path
	Progress.data = _saved


func test_ladders_have_rival_reflection_and_boss() -> void:
	for t in Game.TOWERS.size():
		for id in Content.roster():
			var rungs: Array = Game.plan_arcade(id, t, 7)
			var kinds := []
			var seen := {}
			for r in rungs:
				kinds.append(r["kind"])
				assert_true(Content.fighter(r["opponent"]) != null, "every opponent exists")
				assert_true(r["stage"] in Game.ARENAS or r["stage"] == Game.BOSS_ARENA, "every stage exists")
				if r["kind"] == "fight":
					assert_false(seen.has(r["opponent"]), "no ordinary opponent twice")
					assert_ne(r["opponent"], id, "you do not fight yourself before the Reflection")
					seen[r["opponent"]] = true
			assert_eq(kinds[kinds.size() - 1], "boss", "every tower ends at the boss")
			assert_true("rival" in kinds, "every tower has the rival")
			assert_eq("mirror" in kinds, t == 2, "only Master has the Reflection")


func test_arcade_advances_and_retries() -> void:
	var rungs: Array = Game.plan_arcade("ashka", 0, 3)
	Game.start_arcade("ashka", 0, rungs)
	assert_eq(Game.config["p2"], rungs[0]["opponent"])
	Game.arcade_retry()
	assert_eq(Game.arcade["continues"], 1)
	assert_eq(Game.config["p2"], rungs[0]["opponent"], "a retry fights the same opponent")
	for i in rungs.size() - 1:
		assert_true(Game.arcade_advance(false))
	assert_eq(Game.config["p2"], "slopbat")
	assert_eq(Game.config["rounds_to_win"], 1, "the boss is one long round")
	assert_eq(Game.config["p2_health"], Game.BOSS_HEALTH[0], "and is gentler in the Novice tower")
	assert_false(Game.arcade_advance(false), "beating the boss completes the run")
	Game.arcade = {}


func test_flawless_win_offers_the_stranger() -> void:
	var rungs: Array = Game.plan_arcade("gorm", 1, 3)
	Game.start_arcade("gorm", 1, rungs)
	Game.arcade_advance(true)
	assert_true(Game.stranger_pending)
	Game.start_stranger(true)
	assert_eq(Game.config["p2"], "daxion")
	Game.configure_rung()
	assert_eq(Game.config["mode"], "arcade", "the ladder resumes after the encounter")
	assert_eq(Game.config["p2"], rungs[1]["opponent"])
	Game.arcade = {}


func test_tutorial_blocking_steps_give_something_to_block() -> void:
	# Gorm walks in from the opening gap and swings; a player holding the block the prompt asks
	# for must be able to finish the step.
	for id in ["block", "block_low"]:
		var h := SimHarness.new(Content.fighter("ashka"), Content.fighter("gorm"))
		h.place(3.0)
		var tut: Node = load("res://scripts/ui/tutorial.gd").new()
		tut.sim = h.sim
		for i in tut.STEPS.size():
			if tut.STEPS[i]["id"] == id:
				tut.step = i
		var held := Vector3i(SimHarness.D if id == "block_low" else 0, SimHarness.BL, 0)
		for t in 600:
			h.sim.step(tut.filter_pads([held, Vector3i.ZERO]))
			h.log.append_array(h.sim.events)
		assert_true(h.count("block") >= (3 if id == "block" else 2), "Gorm's swings reach a held block in step " + id)
		assert_eq(h.count("hit"), 0, "and none of them gets through it")
		tut.free()


func test_progress_roundtrip_and_recovery() -> void:
	Progress.data = Progress.fresh()
	Progress.record_arcade("nix", 2, 312.0)
	Progress.set_stranger(3)
	assert_true(FileAccess.file_exists(Progress.path))
	Progress.data = {}
	Progress.load_progress()
	assert_true(Progress.cleared("nix", 2))
	assert_true(Progress.is_unlocked("costume:nix"))
	assert_true(Progress.is_unlocked("arena:roost"))
	assert_eq(int(Progress.data["stranger"]), 3)
	assert_true("ending:nix" in Progress.data["endings"])
	var f := FileAccess.open(Progress.path, FileAccess.WRITE)
	f.store_string("{ this is not json")
	f.close()
	Progress.load_progress()
	assert_eq(int(Progress.data["stranger"]), 0, "a damaged file loads as a fresh profile")
	assert_true(FileAccess.file_exists(Progress.path + ".bad"), "and is kept aside, not overwritten unseen")
	f = FileAccess.open(Progress.path, FileAccess.WRITE)
	f.store_string(JSON.stringify({"version": 1, "stranger": 99, "arcade": "wrong type"}))
	f.close()
	Progress.load_progress()
	assert_eq(int(Progress.data["stranger"]), 5, "out-of-range memory is clamped")
	assert_true(Progress.data["arcade"] is Dictionary, "wrong types fall back to defaults")


func test_save_is_atomic() -> void:
	Progress.data = Progress.fresh()
	assert_true(Progress.save())
	assert_false(FileAccess.file_exists(Progress.path + ".tmp"), "no temporary file is left behind")


func test_test_profile_is_separate() -> void:
	assert_ne(_saved_path, Progress.PATH, "tests never write the player's real progress file")
	assert_ne(Settings.path, Settings.PATH, "nor the player's real settings")


func test_settings_bindings_swap_and_persist() -> void:
	var before: Dictionary = Settings.keys.duplicate(true)
	var old_hp: int = Settings.keys["kb_left"]["hp"]
	var old_lp: int = Settings.keys["kb_left"]["lp"]
	Settings.rebind("kb_left", "hp", old_lp)
	assert_eq(Settings.keys["kb_left"]["hp"], old_lp)
	assert_eq(Settings.keys["kb_left"]["lp"], old_hp, "the displaced key moves to the other action")
	assert_true(InputMap.action_has_event("p1_hp", _key(old_lp)))
	var right_up: int = Settings.keys["kb_right"]["up"]
	var left_lk: int = Settings.keys["kb_left"]["lk"]
	Settings.rebind("kb_left", "lk", right_up)
	assert_eq(Settings.keys["kb_left"]["lk"], right_up)
	assert_eq(Settings.keys["kb_right"]["up"], left_lk, "a key taken from the other set swaps there too")
	Settings.keys = before
	Settings.save_settings()
	Settings.apply()


func test_solo_and_versus_device_mapping() -> void:
	Settings.set_solo(true)
	assert_true(InputMap.action_has_event("p1_left", _key(KEY_LEFT)), "in one-player modes the arrows also move player 1")
	Settings.set_solo(false)
	assert_false(InputMap.action_has_event("p1_left", _key(KEY_LEFT)), "in versus the arrows belong to player 2")
	assert_true(InputMap.action_has_event("p2_left", _key(KEY_LEFT)))
	Settings.set_solo(true)


static func _key(code: int) -> InputEventKey:
	var k := InputEventKey.new()
	k.physical_keycode = code
	return k


# --- Survival and scoring -------------------------------------------------------------------------

func test_survival_waves_boss_health_and_difficulty() -> void:
	Game.start_survival("ashka", 11)
	assert_eq(Game.config["mode"], "survival")
	assert_eq(Game.config["rounds_to_win"], 1, "one round a fight")
	assert_eq(int(Game.config["p1_health"]), 0, "the first wave starts with full health")
	assert_eq(int(Game.config["difficulty"]), 0, "the first waves are Novice")
	var last := ""
	for wave in 10:
		var boss := (wave + 1) % Game.SURVIVAL_BOSS_EVERY == 0
		assert_eq(Game.config["p2"] == "slopbat", boss, "the Slopbat on wave %d" % (wave + 1))
		if boss:
			assert_eq(Game.config["stage"], Game.BOSS_ARENA)
			assert_true(Game.config.has("p2_health"), "the boss's health is set")
		else:
			assert_true(Game.config["p2"] in Content.roster(), "an ordinary opponent")
			assert_ne(Game.config["p2"], last, "never the same opponent twice running")
			assert_true(Game.config["stage"] in Game.ARENAS)
			assert_false(Game.config.has("p2_health"))
		assert_eq(int(Game.config["difficulty"]), Game.survival_difficulty(wave))
		last = Game.config["p2"]
		Game.survival_advance(400, false)
	assert_eq(Game.survival_waves(), 10)
	assert_eq(Game.survival_difficulty(0), 0)
	assert_eq(Game.survival_difficulty(Game.SURVIVAL_WAVES_PER_LEVEL), 1)
	assert_eq(Game.survival_difficulty(99), 2, "Master for good after the first levels")
	Game.survival = {}
	Game.config["mode"] = "cpu"
	Game.config.erase("p1_health")


func test_survival_carries_health_with_a_refill() -> void:
	Game.start_survival("gorm", 3)
	var full := Content.fighter("gorm").health
	Game.survival_advance(100, false)
	assert_eq(int(Game.config["p1_health"]), 100 + int(full * Game.SURVIVAL_REFILL), "a share of full health comes back")
	Game.survival_advance(full - 10, true)
	assert_eq(int(Game.config["p1_health"]), full, "never above full")
	Game.survival_advance(0, true)
	assert_eq(int(Game.config["p1_health"]), 1 + int(full * (Game.SURVIVAL_REFILL + Game.SURVIVAL_FLAWLESS_REFILL)), "a flawless win gives more back")
	assert_true(Progress.record_survival("gorm", 3), "a first result is a record")
	assert_false(Progress.record_survival("gorm", 2), "fewer waves are not")
	assert_eq(Progress.best_survival("gorm"), 3)
	Game.survival = {}
	Game.config["mode"] = "cpu"
	Game.config.erase("p1_health")


func test_fight_score() -> void:
	var plain := Game.fight_score({"health": [0, 0]})
	assert_eq(plain, int(Game.SCORE["win"]), "a bare win scores the base")
	var full := Game.fight_score({"health": [1000, 0], "flawless": true, "finisher": "last_light", "time_left": 50})
	assert_eq(full, int(Game.SCORE["win"]) + 1000 * int(Game.SCORE["health"]) + int(Game.SCORE["flawless"]) + int(Game.SCORE["finisher"]) + 50 * int(Game.SCORE["second"]))
	assert_true(Game.fight_score({"health": [500, 0], "finisher": "stage_vat"}) < Game.fight_score({"health": [500, 0], "finisher": "last_light"}), "a stage finisher is worth a little less")
	Progress.record_arcade("nix", 1, 200.0, 50000)
	Progress.record_arcade("nix", 1, 300.0, 30000)
	var a: Dictionary = Progress.data["arcade"]["nix:1"]
	assert_eq(int(a["score"]), 50000, "the best score is kept")
	assert_eq(float(a["best"]), 200.0, "and the best time")
	assert_eq(UiKit.commas(1234567), "1,234,567")
	assert_eq(UiKit.commas(999), "999")
