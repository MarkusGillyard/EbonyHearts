extends TestCase
## Input mapping with simulated devices (synthetic events through Godot's input pipeline; no
## physical gamepad was attached when this was written): gamepad buttons and sticks reach the right
## player's actions, device assignment keeps two players apart, presses shorter than a tick are not
## lost, and focus loss clears held input.


func after_each() -> void:
	for a in ["p1_hp", "p1_left", "p2_hp", "p2_left", "p1_bl"]:
		Input.action_release(a)
	# Restore the defaults on disk too, so the next run starts from them.
	Settings.assign = {"kb_left": 0, "kb_right": 1, "pad0": 0, "pad1": 1}
	Settings.solo = true
	Settings.save_settings()
	Settings.apply()


static func _pad_button(device: int, button: int, pressed: bool) -> InputEventJoypadButton:
	var e := InputEventJoypadButton.new()
	e.device = device
	e.button_index = button
	e.pressed = pressed
	return e


static func _pad_axis(device: int, axis: int, value: float) -> InputEventJoypadMotion:
	var e := InputEventJoypadMotion.new()
	e.device = device
	e.axis = axis
	e.axis_value = value
	return e


func test_gamepad_buttons_map_to_the_arcade_layout() -> void:
	Settings.set_solo(false)
	assert_true(_pad_button(0, JOY_BUTTON_Y, true).is_action("p1_hp"), "Y is high punch for pad 1")
	assert_true(_pad_button(0, JOY_BUTTON_X, true).is_action("p1_lp"), "X is low punch")
	assert_true(_pad_button(0, JOY_BUTTON_B, true).is_action("p1_hk"), "B is high kick")
	assert_true(_pad_button(0, JOY_BUTTON_A, true).is_action("p1_lk"), "A is low kick")
	assert_true(_pad_button(0, JOY_BUTTON_RIGHT_SHOULDER, true).is_action("p1_bl"), "right shoulder blocks")
	assert_true(_pad_axis(0, JOY_AXIS_TRIGGER_RIGHT, 1.0).is_action("p1_bl"), "so does the right trigger")
	assert_true(_pad_axis(0, JOY_AXIS_TRIGGER_LEFT, 1.0).is_action("p1_rn"), "the left trigger runs")
	assert_true(_pad_axis(0, JOY_AXIS_LEFT_X, -1.0).is_action("p1_left"), "the stick moves")
	assert_true(_pad_button(1, JOY_BUTTON_Y, true).is_action("p2_hp"), "pad 2 drives player 2")
	assert_false(_pad_button(1, JOY_BUTTON_Y, true).is_action("p1_hp"), "and not player 1")


func test_device_assignment_separates_players() -> void:
	Settings.set_solo(false)
	Settings.assign["pad0"] = 1
	Settings.assign["kb_right"] = -1
	Settings.set_assign("pad1", -1)
	assert_true(_pad_button(0, JOY_BUTTON_Y, true).is_action("p2_hp"), "pad 1 moved to player 2")
	assert_false(_pad_button(0, JOY_BUTTON_Y, true).is_action("p1_hp"))
	var k := InputEventKey.new()
	k.physical_keycode = KEY_KP_7
	k.pressed = true
	assert_false(k.is_action("p2_hp"), "an unassigned keyboard half controls nobody")


func test_pad_reader_latches_short_presses_and_resets() -> void:
	Settings.set_solo(false)
	var r := PadReader.new(0)
	r.feed(_pad_button(0, JOY_BUTTON_Y, true))
	r.feed(_pad_button(0, JOY_BUTTON_Y, false))
	var p := r.poll()
	assert_true(p.z & InputBuffer.HP, "a press released within the same tick still counts")
	assert_eq(r.poll().z, 0, "and only once")
	Input.action_press("p1_bl")
	assert_true(r.poll().y & InputBuffer.BL, "held buttons are read every tick")
	Input.action_release("p1_bl")
	r.feed(_pad_button(0, JOY_BUTTON_X, true))
	r.reset()
	assert_eq(r.poll().z, 0, "a reset (focus loss, pause) forgets latched presses")


func test_menus_answer_to_gamepads() -> void:
	assert_true(_pad_button(0, JOY_BUTTON_A, true).is_action("ui_accept"), "A confirms in menus")
	assert_true(_pad_button(0, JOY_BUTTON_B, true).is_action("ui_cancel"), "B goes back")
	assert_true(_pad_button(0, JOY_BUTTON_DPAD_DOWN, true).is_action("ui_down"), "the d-pad moves the selection")
	assert_true(_pad_button(0, JOY_BUTTON_START, true).is_action("pause"), "Start pauses")


func test_held_button_through_a_pause_is_not_a_new_press() -> void:
	Settings.set_solo(false)
	var r := PadReader.new(0)
	Input.action_press("p1_hk")
	r.reset()
	assert_eq(r.poll().z & InputBuffer.HK, 0, "B held to leave the pause menu does not throw a kick")
	Input.action_release("p1_hk")
	r.poll()
	Input.action_press("p1_hk")
	assert_true(r.poll().z & InputBuffer.HK, "pressing it again does")
	Input.action_release("p1_hk")


func test_devices_set_to_nobody_still_play_solo() -> void:
	Settings.assign["kb_right"] = -1
	Settings.set_solo(false)
	Settings.set_solo(true)
	var k := InputEventKey.new()
	k.physical_keycode = Settings.keys["kb_right"]["hp"]
	k.pressed = true
	assert_true(k.is_action("p1_hp"), "in one-player modes every device drives player 1")
