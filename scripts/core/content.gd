extends Node
## Loads game data once: the shared basic moves, every fighter, the story text and the shared
## poses. Read-only after loading.

const FIGHTER_DIR := "res://data/fighters/"
## The selectable roster, in select-screen order.
const ROSTER := ["ashka", "gorm", "nix", "mirela", "bram", "hollis"]
## Fighters that exist but are not selectable: the final boss and the stranger.
const SPECIAL := ["slopbat", "daxion"]

var fighters: Dictionary = {}
var basics: Dictionary = {}
var story: Dictionary = {}
var common_poses: Dictionary = {}
var errors: Array[String] = []


func _ready() -> void:
	reload()


func reload() -> void:
	fighters.clear()
	errors.clear()
	basics = load_json("res://data/basics.json")
	common_poses = load_json("res://data/poses_common.json")
	story = load_json("res://data/story.json")
	for id in all_ids():
		var path: String = FIGHTER_DIR + id + ".json"
		if not FileAccess.file_exists(path):
			continue
		var d := load_json(path)
		if d.is_empty():
			errors.append("invalid fighter " + id)
			continue
		fighters[id] = FighterDef.from_dict(d, basics)


func all_ids() -> Array:
	return ROSTER + SPECIAL


func fighter(id: String) -> FighterDef:
	return fighters.get(id)


## Roster fighters whose data is present (all of them in a complete build).
func roster() -> Array:
	var out := []
	for id in ROSTER:
		if fighters.has(id):
			out.append(id)
	return out


static func load_json(path: String) -> Dictionary:
	if not FileAccess.file_exists(path):
		return {}
	var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(path))
	if parsed is Dictionary:
		return parsed
	push_error("Invalid JSON: " + path)
	return {}
