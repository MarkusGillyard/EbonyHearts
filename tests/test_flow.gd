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
