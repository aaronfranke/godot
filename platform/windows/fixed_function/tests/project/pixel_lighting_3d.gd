extends Spatial

const SETTING = "rendering/quality/fixed_function/per_pixel_lighting"
var _frame = 0
var _hold = false
var _previous = true


func _ready():
	_hold = OS.get_cmdline_args().has("--smoke-hold")
	_previous = ProjectSettings.get_setting(SETTING) if ProjectSettings.has_setting(SETTING) else true
	Engine.target_fps = 60
	OS.low_processor_usage_mode = false
	OS.window_size = Vector2(400, 300)
	get_viewport().usage = Viewport.USAGE_3D
	$Camera.look_at(Vector3(0, -0.8, 0), Vector3.UP)
	$Spot.look_at(Vector3(1, -1, 0), Vector3.UP)
	for index in range(9):
		var light = OmniLight.new()
		light.translation = Vector3(10 + index * 2, 5, 0)
		light.omni_range = 40
		light.light_energy = 0.01
		add_child(light)
	_set_per_pixel(true)


func _set_per_pixel(enabled):
	ProjectSettings.set_setting(SETTING, enabled)
	$GUI/Label.text = "Per-pixel requested (SM3). Space: toggle" if enabled else "Per-vertex fallback. Space: toggle"


func _input(event):
	if event is InputEventKey and event.pressed and not event.echo and event.scancode == KEY_SPACE:
		_set_per_pixel(not ProjectSettings.get_setting(SETTING))


func _process(_delta):
	_frame += 1
	match _frame:
		12:
			if not _hold:
				_set_per_pixel(false)
		20:
			if not _hold:
				_set_per_pixel(true)
		24:
			$Spot.spot_angle_attenuation = 2
			$Omni.light_color = Color(1, 0.3, 0.1)
		32:
			OS.window_size = Vector2(440, 320)
		40:
			if get_viewport().get_render_info(Viewport.RENDER_INFO_OBJECTS_IN_FRAME) < 2:
				push_error("PIXEL LIGHTING SCENE FAIL: mesh submission")
				get_tree().quit(1)
			else:
				print("PIXEL LIGHTING SCENE PASS frames=", _frame)
				if not _hold:
					get_tree().quit()


func _exit_tree():
	ProjectSettings.set_setting(SETTING, _previous)
