class_name TestCase
extends RefCounted
## Minimal assertion base. Every method whose name starts with "test_" is run by the runner.

var failures: Array[String] = []
var current := ""
## Scene tree access for integration tests.
var tree: SceneTree
var root: Node


func before_each() -> void:
	pass


func after_each() -> void:
	pass


func fail(msg: String) -> void:
	failures.append("%s: %s" % [current, msg])


func assert_true(cond: bool, msg := "expected true") -> void:
	if not cond:
		fail(msg)


func assert_false(cond: bool, msg := "expected false") -> void:
	if cond:
		fail(msg)


func assert_eq(a: Variant, b: Variant, msg := "") -> void:
	if typeof(a) != typeof(b) and not ((typeof(a) in [TYPE_INT, TYPE_FLOAT]) and (typeof(b) in [TYPE_INT, TYPE_FLOAT])):
		fail("%s expected %s (%s) == %s (%s)" % [msg, a, type_string(typeof(a)), b, type_string(typeof(b))])
	elif a != b:
		fail("%s expected %s == %s" % [msg, a, b])


func assert_ne(a: Variant, b: Variant, msg := "") -> void:
	if a == b:
		fail("%s expected %s != %s" % [msg, a, b])

