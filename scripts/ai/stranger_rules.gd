class_name StrangerRules
extends RefCounted
## The rules of the optional encounter with the stranger in white (see docs/DESIGN.md). Placeholder
## until the encounter is built: he stands and does nothing.

var sim: FightSim
var match_node: Node


func _init(s: FightSim, m: Node) -> void:
	sim = s
	match_node = m


func think() -> Vector3i:
	return Vector3i.ZERO


func on_contact(_c: Dictionary) -> String:
	return ""


func on_tick(_s: FightSim) -> void:
	pass


func on_event(_e: Dictionary) -> void:
	pass
