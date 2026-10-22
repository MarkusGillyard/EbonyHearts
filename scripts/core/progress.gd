extends Node
## The player's progress: arcade completions and records, unlocks, the endings seen and the
## stranger's memory. Stored as versioned JSON in user://progress.json, written atomically
## (temporary file, then rename). A damaged or unreadable file falls back to a fresh profile
## and is kept aside as progress.json.bad instead of being overwritten unseen.

const VERSION := 1
const PATH := "user://progress.json"

var data: Dictionary = {}
var path := PATH


func _ready() -> void:
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--profile="):
			# Test and development profiles live beside the real one, never in it.
			path = "user://progress_%s.json" % a.trim_prefix("--profile=").validate_filename()
	load_progress()


static func fresh() -> Dictionary:
	return {
		"version": VERSION,
		"arcade": {},
		"endings": [],
		"unlocks": [],
		"stranger": 0,
		"stats": {"matches": 0, "wins": 0, "finishers": 0, "flawless": 0},
		"tutorial_done": false,
	}


func load_progress() -> void:
	data = fresh()
	if not FileAccess.file_exists(path):
		return
	var json := JSON.new()
	var parsed: Variant = json.data if json.parse(FileAccess.get_file_as_string(path)) == OK else null
	if not parsed is Dictionary or int(parsed.get("version", 0)) < 1:
		push_warning("Progress file unreadable; starting fresh and keeping the old one as .bad")
		DirAccess.rename_absolute(ProjectSettings.globalize_path(path), ProjectSettings.globalize_path(path + ".bad"))
		return
	var p: Dictionary = parsed
	for k in data:
		if p.has(k) and typeof(p[k]) == typeof(data[k]):
			data[k] = p[k]
		elif p.has(k) and typeof(data[k]) == TYPE_INT and typeof(p[k]) == TYPE_FLOAT:
			data[k] = int(p[k])
	data["stranger"] = clampi(int(data["stranger"]), 0, 5)


func save() -> bool:
	var tmp := path + ".tmp"
	var f := FileAccess.open(tmp, FileAccess.WRITE)
	if f == null:
		push_error("Cannot write progress: %s" % FileAccess.get_open_error())
		return false
	f.store_string(JSON.stringify(data, "\t"))
	f.close()
	var err := DirAccess.rename_absolute(ProjectSettings.globalize_path(tmp), ProjectSettings.globalize_path(path))
	if err != OK:
		push_error("Cannot replace progress file: %s" % err)
		return false
	return true


func record_arcade(fighter: String, difficulty: int, seconds: float) -> void:
	var a: Dictionary = data["arcade"]
	var key := "%s:%d" % [fighter, difficulty]
	var best: float = float(a.get(key, {}).get("best", 0.0))
	var clears: int = int(a.get(key, {}).get("clears", 0))
	a[key] = {"clears": clears + 1, "best": seconds if best <= 0.0 else minf(best, seconds)}
	if not ("ending:" + fighter) in data["endings"]:
		data["endings"].append("ending:" + fighter)
	unlock("costume:" + fighter)
	unlock("arena:roost")
	save()


func cleared(fighter: String, difficulty := -1) -> bool:
	for key: String in data["arcade"]:
		var parts := key.split(":")
		if parts[0] == fighter and (difficulty < 0 or int(parts[1]) == difficulty):
			return true
	return false


func any_cleared() -> bool:
	return not (data["arcade"] as Dictionary).is_empty()


func unlock(what: String) -> void:
	if not what in data["unlocks"]:
		data["unlocks"].append(what)


func is_unlocked(what: String) -> bool:
	return what in data["unlocks"]


func count(stat: String, n := 1) -> void:
	var s: Dictionary = data["stats"]
	s[stat] = int(s.get(stat, 0)) + n


## The stranger's memory only ever moves forward.
func set_stranger(stage: int) -> void:
	if stage > int(data["stranger"]):
		data["stranger"] = stage
		save()


func reset_all() -> void:
	data = fresh()
	save()
