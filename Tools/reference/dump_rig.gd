extends SceneTree
## Run inside the original Godot project (read-only use): poses every pose of every fighter on the
## original rig and writes the solved joints, so the Unreal port can be checked against them.
##   godot --headless --path "<original>" -s <this file> -- <out.json>

func _initialize() -> void:
	var out_path := OS.get_cmdline_user_args()[0]
	var content = root.get_node("Content")
	if content.fighters.is_empty():
		content.reload()
	var result := {}
	for id in content.all_ids():
		var def: FighterDef = content.fighter(id)
		if def == null:
			continue
		var lib := PoseLibrary.new()
		lib.build(def, content.common_poses)
		var rig := Rig.new()
		rig.build(def)
		var poses := {}
		var names: Array = lib.poses.keys()
		names.sort()
		for n in names:
			var p: Pose = lib.get_pose(n)
			var feet: Array = [p.pos["foot_l"] + Vector3(0, rig.ankle_h, 0), p.pos["foot_r"] + Vector3(0, rig.ankle_h, 0)]
			var yaws: Array = [deg_to_rad(p.val["yaw_l"]), deg_to_rad(p.val["yaw_r"])]
			rig.apply(p, feet, yaws, {})
			var joints := {}
			for j in Rig.JOINTS:
				var t: Transform3D = rig.xf[j]
				var q := Quaternion(t.basis.orthonormalized())
				joints[j] = [t.origin.x, t.origin.y, t.origin.z, q.x, q.y, q.z, q.w]
			var ht: Vector3 = rig.hand_tip("r")
			joints["hand_tip_r"] = [ht.x, ht.y, ht.z, 0, 0, 0, 1]
			poses[n] = joints
		result[id] = poses
	var f := FileAccess.open(out_path, FileAccess.WRITE)
	f.store_string(JSON.stringify(result))
	f.close()
	print("wrote ", out_path)
	quit()
