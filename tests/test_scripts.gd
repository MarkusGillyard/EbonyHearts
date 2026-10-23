extends TestCase
## Every script in the game compiles. The other suites only load what they exercise, so a parse
## error in a menu screen would otherwise show up only when a player opened that screen.


func test_every_script_compiles() -> void:
	var paths := []
	_collect("res://scripts", paths)
	assert_true(paths.size() > 40, "found the game's scripts (%d)" % paths.size())
	for p in paths:
		var s: Variant = load(p)
		assert_true(s is GDScript and (s as GDScript).can_instantiate(), "%s compiles" % p)


func _collect(dir: String, out: Array) -> void:
	var d := DirAccess.open(dir)
	if d == null:
		return
	d.list_dir_begin()
	var name := d.get_next()
	while name != "":
		var path := dir.path_join(name)
		if d.current_is_dir():
			_collect(path, out)
		elif name.ends_with(".gd"):
			out.append(path)
		name = d.get_next()
	d.list_dir_end()
