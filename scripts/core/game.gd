extends Node
## Screen flow and the configuration of the next match, including the arcade ladder. Screens
## ask for transitions through goto(); Main owns the actual nodes.

signal screen_requested(screen: String, data: Dictionary)

## The main menu item chosen last, so coming back (from Settings, a match, Extras) lands there.
var menu_choice := 0

const DIFFICULTIES := ["Novice", "Warrior", "Master"]
## Arcade towers: how many ordinary opponents precede the rival, and whether the Reflection
## (a mirror match against a slop-dark copy) stands before the boss.
const TOWERS := [{"name": "Novice", "fights": 3, "mirror": false},
	{"name": "Warrior", "fights": 4, "mirror": false},
	{"name": "Master", "fights": 4, "mirror": true}]
const ARENAS := ["rendering_yards", "drowned_chapel", "stilt_market"]
const BOSS_ARENA := "roost"
## The Slopbat's health in each tower: Novice to Master.
const BOSS_HEALTH := [1400, 1650, 1900]
## Survival: the Slopbat waits on every wave that is a multiple of this.
const SURVIVAL_BOSS_EVERY := 5
## Survival: the share of full health given back after a win, and the extra for a flawless one.
const SURVIVAL_REFILL := 0.3
const SURVIVAL_FLAWLESS_REFILL := 0.15
## Survival: waves per difficulty step (Novice, then Warrior, then Master for good).
const SURVIVAL_WAVES_PER_LEVEL := 3
## Arcade points: for a win, per point of health left, for a flawless victory, for a Sloptality,
## for a stage finisher, and per second left on the clock.
const SCORE := {"win": 10000, "health": 20, "flawless": 25000, "finisher": 15000, "stage_finisher": 10000, "second": 100}

## mode: "arcade", "versus" (two players), "cpu" (player 1 against the CPU), "training",
## "tutorial", "stranger" (the optional encounter), "watch" (CPU against CPU), "survival" (an
## endless ladder, health carried from fight to fight).
var config := {
	"mode": "cpu",
	"p1": "ashka",
	"p2": "gorm",
	"stage": "rendering_yards",
	"difficulty": 1,
	"rounds_to_win": 2,
	"mirror_dark": false,
}
var arcade: Dictionary = {}
## The survival run in progress: fighter, wave (0-based, the one being fought), health carried
## in (0 for full), the last opponent, the seed of the order.
var survival: Dictionary = {}
## Set when the last arcade fight was won flawlessly: the stranger may be waiting.
var stranger_pending := false


func _ready() -> void:
	get_tree().auto_accept_quit = false
	process_mode = Node.PROCESS_MODE_ALWAYS


func _notification(what: int) -> void:
	if what == NOTIFICATION_WM_CLOSE_REQUEST:
		quit()


## Every exit goes through here so the audio stops before the engine shuts down.
func quit() -> void:
	Audio.close()
	await get_tree().create_timer(0.1, true, false, true).timeout
	get_tree().quit()


func goto(screen: String, data := {}) -> void:
	screen_requested.emit(screen, data)


func is_cpu(side: int) -> bool:
	match config["mode"]:
		"arcade", "cpu", "training", "tutorial", "stranger", "survival", "trials":
			return side == 1
		"watch":
			return true
	return false


func two_players() -> bool:
	return config["mode"] == "versus"


# --- Arcade ---------------------------------------------------------------------------------------

## The ladder a fighter would climb in a tower: ordinary opponents, the rival, the Reflection
## (Master only) and the boss. `seed` fixes the order of the ordinary opponents.
func plan_arcade(fighter: String, tower: int, seed: int) -> Array:
	var t: Dictionary = TOWERS[tower]
	var rival := str(Content.story.get("fighters", {}).get(fighter, {}).get("rival", ""))
	var others := []
	for id in Content.roster():
		if id != fighter and id != rival:
			others.append(id)
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	_shuffle(others, rng)
	var rungs := []
	for i in mini(int(t["fights"]), others.size()):
		rungs.append({"opponent": others[i], "stage": ARENAS[i % ARENAS.size()], "kind": "fight"})
	if rival != "" and Content.fighter(rival):
		rungs.append({"opponent": rival, "stage": str(Content.story["fighters"][rival].get("home", ARENAS[0])), "kind": "rival"})
	if t["mirror"]:
		rungs.append({"opponent": fighter, "stage": "drowned_chapel", "kind": "mirror"})
	if Content.fighter("slopbat"):
		rungs.append({"opponent": "slopbat", "stage": BOSS_ARENA, "kind": "boss"})
	return rungs


func start_arcade(fighter: String, tower: int, rungs: Array) -> void:
	arcade = {"fighter": fighter, "tower": tower, "rungs": rungs, "rung": 0, "continues": 0, "started": Time.get_ticks_msec(),
		"seconds": 0.0, "flawless": 0, "score": 0}
	survival = {}
	stranger_pending = false
	config["mode"] = "arcade"
	config["p1"] = fighter
	config["difficulty"] = tower
	configure_rung()


static func _shuffle(a: Array, rng: RandomNumberGenerator) -> void:
	for i in range(a.size() - 1, 0, -1):
		var j := rng.randi_range(0, i)
		var tmp: Variant = a[i]
		a[i] = a[j]
		a[j] = tmp


func configure_rung() -> void:
	var r: Dictionary = arcade["rungs"][arcade["rung"]]
	config["p2"] = r["opponent"]
	config["stage"] = r["stage"]
	config["mirror_dark"] = r["kind"] == "mirror"
	# The boss fight is a single long round, as the last stand should be.
	config["rounds_to_win"] = 1 if r["kind"] == "boss" else 2
	if r["kind"] == "boss":
		config["p2_health"] = BOSS_HEALTH[clampi(int(arcade.get("tower", 1)), 0, BOSS_HEALTH.size() - 1)]
	else:
		config.erase("p2_health")
	config["mode"] = "arcade"


func current_rung() -> Dictionary:
	if arcade.is_empty():
		return {}
	return arcade["rungs"][arcade["rung"]]


## Points for a won arcade fight, from the match result (health left, flawless, finisher, time).
static func fight_score(result: Dictionary) -> int:
	var s: int = SCORE["win"]
	var health: Array = result.get("health", [0, 0])
	s += int(SCORE["health"]) * maxi(0, int(health[0]))
	if result.get("flawless", false):
		s += int(SCORE["flawless"])
	var fin := str(result.get("finisher", ""))
	if fin != "":
		s += int(SCORE["stage_finisher"]) if fin.begins_with("stage_") else int(SCORE["finisher"])
	s += int(SCORE["second"]) * maxi(0, int(result.get("time_left", 0)))
	return s


## Adds a won fight's points to the arcade run; returns them.
func arcade_score(result: Dictionary) -> int:
	var pts := fight_score(result)
	arcade["score"] = int(arcade.get("score", 0)) + pts
	return pts


## After a won arcade match: advances the ladder; returns false when the run is complete.
func arcade_advance(flawless: bool) -> bool:
	if flawless:
		arcade["flawless"] = int(arcade["flawless"]) + 1
	arcade["rung"] = int(arcade["rung"]) + 1
	if arcade["rung"] >= arcade["rungs"].size():
		arcade["seconds"] = (Time.get_ticks_msec() - int(arcade["started"])) / 1000.0
		return false
	stranger_pending = flawless and current_rung().get("kind", "") != "boss" and Content.fighter("daxion") != null
	configure_rung()
	return true


func arcade_retry() -> void:
	arcade["continues"] = int(arcade["continues"]) + 1
	configure_rung()


## A fighter's combo trials: a training match against their rival on the rival's home ground.
func start_trials(fighter: String) -> void:
	var rival := str(Content.story.get("fighters", {}).get(fighter, {}).get("rival", ""))
	if Content.fighter(rival) == null:
		rival = "gorm" if fighter != "gorm" else "bram"
	arcade = {}
	survival = {}
	config["mode"] = "trials"
	config["p1"] = fighter
	config["p2"] = rival
	config["stage"] = str(Content.story.get("fighters", {}).get(rival, {}).get("home", ARENAS[0]))
	config["rounds_to_win"] = 1
	config["mirror_dark"] = false
	config.erase("p2_health")


# --- Survival ------------------------------------------------------------------------------------

## Begins an endless ladder with `fighter`: one round a fight, health carried over.
func start_survival(fighter: String, seed := -1) -> void:
	if seed < 0:
		seed = int(Time.get_unix_time_from_system()) % 100000
	survival = {"fighter": fighter, "wave": 0, "health": 0, "last": "", "seed": seed, "started": Time.get_ticks_msec()}
	arcade = {}
	stranger_pending = false
	config["mode"] = "survival"
	config["p1"] = fighter
	configure_wave()


## Whether survival wave `wave` (0-based) is the Slopbat's.
static func survival_boss_wave(wave: int) -> bool:
	return (wave + 1) % SURVIVAL_BOSS_EVERY == 0


## The difficulty of survival wave `wave` (0-based): three waves of each level, then Master.
static func survival_difficulty(wave: int) -> int:
	return clampi(wave / SURVIVAL_WAVES_PER_LEVEL, 0, DIFFICULTIES.size() - 1)


## Configures the next survival fight: the opponent (never the same twice running, the Slopbat on
## its waves), the stage, the difficulty, and the health carried in.
func configure_wave() -> void:
	var wave := int(survival["wave"])
	var rng := RandomNumberGenerator.new()
	rng.seed = int(survival["seed"]) * 131 + wave
	var diff := survival_difficulty(wave)
	var boss := survival_boss_wave(wave) and Content.fighter("slopbat") != null
	if boss:
		config["p2"] = "slopbat"
		config["stage"] = BOSS_ARENA
		config["p2_health"] = BOSS_HEALTH[diff] - 200
	else:
		var pool := []
		for id in Content.roster():
			if id != str(survival["last"]):
				pool.append(id)
		config["p2"] = pool[rng.randi_range(0, pool.size() - 1)]
		config["stage"] = ARENAS[rng.randi_range(0, ARENAS.size() - 1)]
		config.erase("p2_health")
	config["p1_health"] = int(survival["health"])
	config["difficulty"] = diff
	config["rounds_to_win"] = 1
	config["mirror_dark"] = false
	config["mode"] = "survival"


## After a won survival fight: the wave is cleared, some health comes back, the next is set up.
func survival_advance(health_left: int, flawless: bool) -> void:
	var full := Content.fighter(str(survival["fighter"])).health
	var back := SURVIVAL_REFILL + (SURVIVAL_FLAWLESS_REFILL if flawless else 0.0)
	survival["health"] = mini(full, maxi(1, health_left) + int(full * back))
	survival["last"] = config["p2"]
	survival["wave"] = int(survival["wave"]) + 1
	configure_wave()


## Waves cleared so far in the run.
func survival_waves() -> int:
	return int(survival.get("wave", 0))


## Sets up the optional encounter with the stranger (from the ladder or from Extras).
func start_stranger(from_arcade: bool) -> void:
	config["mode"] = "stranger"
	config["p2"] = "daxion"
	config["stage"] = "stilt_market"
	config["rounds_to_win"] = 1
	config["mirror_dark"] = false
	config["from_arcade"] = from_arcade
	if not from_arcade and arcade.is_empty():
		config["p1"] = config.get("p1", "ashka")
