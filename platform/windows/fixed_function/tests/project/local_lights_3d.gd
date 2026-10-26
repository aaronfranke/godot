extends Node3D

var _frame = 0
var _hold = false


func _ready():
	Engine.max_fps = 60
	OS.low_processor_usage_mode = false
	_hold = OS.get_cmdline_args().has("--smoke-hold")
	Engine.max_fps = 60
	DisplayServer.window_set_size(Vector2i(320, 240))
	get_viewport().disable_3d = false
	$Camera.look_at(Vector3.ZERO, Vector3.UP)
	$Spot.look_at($Cube.position, Vector3.UP)


func _process(_delta):
	_frame += 1
	match _frame:
		8:
			if get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_OBJECTS_IN_FRAME) < 2:
				push_error("LOCAL LIGHTS SCENE FAIL: mesh submission")
				get_tree().quit(1)
		12:
			$Omni.omni_attenuation = 2.0
			$Spot.spot_angle_attenuation = 2.0
		16:
			$Omni.light_energy = 0.5
			$Spot.light_color = Color(0.1, 1, 0.1)
		20:
			$Spot.rotate_y(0.4)
			$Cube.scale = Vector3(-1.1, 0.8, 1)
		24:
			$Omni.light_cull_mask = 2
		28:
			$Omni.light_cull_mask = 1
			$Camera.projection = Camera3D.PROJECTION_ORTHOGONAL
			$Camera.size = 5
		32:
			DisplayServer.window_set_size(Vector2i(400, 300))
		36:
			if get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_OBJECTS_IN_FRAME) < 2:
				push_error("LOCAL LIGHTS SCENE FAIL: live local-light edits")
				get_tree().quit(1)
			else:
				print("LOCAL LIGHTS SCENE PASS frames=", _frame)
				if not _hold:
					get_tree().quit()
