extends Node
## Headless test runner: `godot --headless --path . res://tests/test_runner.tscn`
## Optional user args after `--`: a test file filter, e.g. `-- frames`.

const TEST_DIR := "res://tests/"


func _ready() -> void:
	_run.call_deferred()


func _run() -> void:
	var filter := ""
	for a in OS.get_cmdline_user_args():
		if not a.begins_with("--"):
			filter = a
	var files := []
	for f in DirAccess.get_files_at(TEST_DIR):
		if f.begins_with("test_") and f.ends_with(".gd") and f != "test_case.gd" and f != "test_runner.gd":
			if filter == "" or f.contains(filter):
				files.append(f)
	files.sort()
	var total := 0
	var failed := 0
	var all_failures: Array[String] = []
	var started := Time.get_ticks_msec()
	for f in files:
		var script: GDScript = load(TEST_DIR + f)
		if script == null or not script.can_instantiate():
			# A file that fails to compile must fail the run, not hang it.
			total += 1
			failed += 1
			all_failures.append("%s: does not compile" % f)
			print("FAIL  %s does not compile" % f)
			continue
		var t: TestCase = script.new()
		t.tree = get_tree()
		t.root = self
		var methods := []
		for m in script.get_script_method_list():
			if str(m["name"]).begins_with("test_"):
				methods.append(m["name"])
		methods.sort()
		for m in methods:
			t.current = "%s::%s" % [f.get_basename(), m]
			var before := t.failures.size()
			t.before_each()
			# Awaiting works for plain functions and coroutines alike.
			await t.call(m)
			t.after_each()
			await get_tree().process_frame
			total += 1
			if t.failures.size() > before:
				failed += 1
				print("FAIL  ", t.current)
				for i in range(before, t.failures.size()):
					print("      ", t.failures[i])
			else:
				print("ok    ", t.current)
		all_failures.append_array(t.failures)
	print("\n%d tests, %d failed, %d assertions failed, %.1f s" % [total, failed, all_failures.size(), (Time.get_ticks_msec() - started) / 1000.0])
	get_tree().quit(1 if failed > 0 else 0)
