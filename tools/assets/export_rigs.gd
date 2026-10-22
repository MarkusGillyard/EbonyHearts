extends SceneTree
## Writes each fighter's skeleton in the bind pose, its look and its cloth rest shapes to
## build/characters/<id>.rig.json, the input of the Python sculptor (tools/assets/sculpt).
##   godot --headless --path . -s tools/assets/export_rigs.gd


func _init() -> void:
	var content: Node = load("res://scripts/core/content.gd").new()
	content.reload()
	DirAccess.make_dir_recursive_absolute("res://build/characters")
	var only := ""
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--only="):
			only = a.trim_prefix("--only=")
	for id: String in content.all_ids():
		var def: FighterDef = content.fighter(id)
		if def == null or (only != "" and not id in only.split(",")) or def.look.has("model"):
			continue
		var rig := Rig.new()
		rig.build(def)
		var binds := CharacterModel.bind_transforms(rig)
		var joints := []
		for j: String in Rig.JOINTS:
			joints.append({"name": j, "parent": Rig.PARENT[j], "bind": _xf(binds[j])})
		var cloth := []
		for spec: Dictionary in def.look.get("cloth", []):
			var shape := ClothSheet.rest_shape(spec, rig.h)
			var pts := []
			for v: Vector3 in shape["points"]:
				pts.append([v.x, v.y, v.z])
			cloth.append({"spec": spec, "joint": str(spec.get("joint", "hips")), "cols": shape["cols"],
				"rows": shape["rows"], "closed": shape["closed"], "points": pts})
		var out := {
			"id": id, "height": rig.h, "look": def.look, "look_hash": CharacterModel.look_hash(def), "joints": joints, "cloth": cloth,
			"lengths": {"upper": rig.upper_len, "fore": rig.fore_len, "thigh": rig.thigh_len, "shin": rig.shin_len, "ankle": rig.ankle_h},
		}
		var f := FileAccess.open("res://build/characters/%s.rig.json" % id, FileAccess.WRITE)
		f.store_string(JSON.stringify(out, "\t"))
		f.close()
		rig.free()
		print("exported ", id)
	content.free()
	quit()


static func _xf(t: Transform3D) -> Array:
	var b := t.basis
	return [b.x.x, b.x.y, b.x.z, b.y.x, b.y.y, b.y.z, b.z.x, b.z.y, b.z.z, t.origin.x, t.origin.y, t.origin.z]
