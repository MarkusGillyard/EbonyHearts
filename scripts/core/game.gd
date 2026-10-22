extends Node
## Screen flow and the configuration of the next match, including the arcade ladder. Screens
## ask for transitions through goto(); Main owns the actual nodes.

signal screen_requested(screen: String, data: Dictionary)

const DIFFICULTIES := ["Novice", "Warrior", "Master"]
## Arcade towers: how many ordinary opponents precede the rival, and whether the Reflection
## (a mirror match against a slop-dark copy) stands before the boss.
const TOWERS := [{"name": "Novice", "fights": 3, "mirror": false},
	{"name": "Warrior", "fights": 4, "mirror": false},
	{"name": "Master", "fights": 4, "mirror": true}]
const ARENAS := ["rendering_yards", "drowned_chapel", "stilt_market"]
const BOSS_ARENA := "roost"

## mode: "arcade", "versus" (two players), "cpu" (player 1 against the CPU), "training",
## "tutorial", "stranger" (the optional encounter), "watch" (CPU against CPU).
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
		"arcade", "cpu", "training", "tutorial", "stranger":
			return side == 1
		"watch":
			return true
	return false


func two_players() -> bool:
	return config["mode"] == "versus"


# --- Arcade ---------------------------------------------------------------------------------------

func start_arcade(fighter: String, tower: int) -> void:
	var t: Dictionary = TOWERS[tower]
	var rival := str(Content.story.get("fighters", {}).get(fighter, {}).get("rival", ""))
	var others := []
	for id in Content.roster():
		if id != fighter and id != rival:
			others.append(id)
	# A fixed but fighter-dependent order, so each fighter's ladder feels its own.
	var rng := RandomNumberGenerator.new()
	rng.seed = hash(fighter + str(tower)) + int(Time.get_unix_time_from_system()) % 7
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
	arcade = {"fighter": fighter, "tower": tower, "rungs": rungs, "rung": 0, "continues": 0, "started": Time.get_ticks_msec(),
		"seconds": 0.0, "flawless": 0}
	stranger_pending = false
	config["mode"] = "arcade"
	config["p1"] = fighter
	config["difficulty"] = tower
	_configure_rung()


static func _shuffle(a: Array, rng: RandomNumberGenerator) -> void:
	for i in range(a.size() - 1, 0, -1):
		var j := rng.randi_range(0, i)
		var tmp: Variant = a[i]
		a[i] = a[j]
		a[j] = tmp


func _configure_rung() -> void:
	var r: Dictionary = arcade["rungs"][arcade["rung"]]
	config["p2"] = r["opponent"]
	config["stage"] = r["stage"]
	config["mirror_dark"] = r["kind"] == "mirror"
	config["rounds_to_win"] = 2
	config["mode"] = "arcade"


func current_rung() -> Dictionary:
	if arcade.is_empty():
		return {}
	return arcade["rungs"][arcade["rung"]]


## After a won arcade match: advances the ladder; returns false when the run is complete.
func arcade_advance(flawless: bool) -> bool:
	if flawless:
		arcade["flawless"] = int(arcade["flawless"]) + 1
	arcade["rung"] = int(arcade["rung"]) + 1
	if arcade["rung"] >= arcade["rungs"].size():
		arcade["seconds"] = (Time.get_ticks_msec() - int(arcade["started"])) / 1000.0
		return false
	stranger_pending = flawless and current_rung().get("kind", "") != "boss" and Content.fighter("daxion") != null
	_configure_rung()
	return true


func arcade_retry() -> void:
	arcade["continues"] = int(arcade["continues"]) + 1
	_configure_rung()


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
