extends Spatial

var _frame = 0
var _failed = false
var _hold = false


func _ready():
	_hold = OS.get_cmdline_args().has("--smoke-hold")
	get_viewport().usage = Viewport.USAGE_3D
	$Camera.look_at(Vector3(0, 0, 0), Vector3.UP)


func _check(condition, description):
	if not condition and not _failed:
		_failed = true
		push_error("LAMBERT SCENE FAIL: " + description)
		get_tree().quit(1)


func _process(_delta):
	if _failed:
		return
	_frame += 1
	match _frame:
		8:
			_check(get_viewport().get_render_info(Viewport.RENDER_INFO_OBJECTS_IN_FRAME) >= 4, "initial mesh submission")
		12:
			$LambertCube.material_override.albedo_color = Color(0.8, 0.45, 0.2)
			$Sun.light_energy = 0.25
		16:
			$LambertCube.rotation.y = 0.5
		20:
			$WorldEnvironment.environment.ambient_light_color = Color(0.1, 0.2, 0.4)
		24:
			$DefaultWhiteCube.scale = Vector3(-1.25, 0.75, 1)
		28:
			$Sun.light_cull_mask = 2
		32:
			$Sun.light_cull_mask = 1
			$Sun.light_energy = 0.8
			$Camera.projection = Camera.PROJECTION_ORTHOGONAL
			$Camera.size = 12
		36:
			OS.window_size = Vector2(800, 500)
		40:
			_check(get_viewport().get_render_info(Viewport.RENDER_INFO_OBJECTS_IN_FRAME) >= 4, "live material/light/camera edits and resize")
		44:
			$Camera.projection = Camera.PROJECTION_PERSPECTIVE
		48:
			print("LAMBERT SCENE PASS frames=", _frame)
			if not _hold:
				get_tree().quit()
