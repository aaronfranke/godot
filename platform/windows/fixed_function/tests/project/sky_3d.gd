extends Node3D

var _environment
var _camera
var _frames = 0
var _failed = false


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("SKY SCENE FAIL: ", message)


func _ready():
	OS.low_processor_usage_mode = false
	get_viewport().disable_3d = false
	var image = Image.create_empty(64, 32, false, Image.FORMAT_RGBA8)
	image.fill(Color(0.2, 0.6, 0.8))
	var texture = ImageTexture.create_from_image(image)
	var panorama = Sky.new()
	panorama.sky_material = PanoramaSkyMaterial.new()
	panorama.sky_material.panorama = texture
	var world = WorldEnvironment.new()
	_environment = Environment.new()
	_environment.background_mode = Environment.BG_SKY
	_environment.sky = panorama
	world.environment = _environment
	add_child(world)
	_camera = Camera3D.new()
	_camera.position.z = 4
	_camera.current = true
	add_child(_camera)


func _process(_delta):
	_frames += 1
	var calls = get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME)
	var objects = get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_OBJECTS_IN_FRAME)
	if _frames == 8:
		_check(calls == 1 and objects == 0, "PanoramaSkyMaterial draws in an empty 3D scene")
		var sky = Sky.new()
		sky.sky_material = ProceduralSkyMaterial.new()
		_environment.sky = sky
	if _frames == 24:
		_check(calls == 1 and objects == 0, "ProceduralSkyMaterial's gradient draws")
		var box = MeshInstance3D.new()
		box.mesh = BoxMesh.new()
		box.material_override = StandardMaterial3D.new()
		add_child(box)
		_camera.set_orthogonal(4.0, 0.05, 50.0)
		_environment.sky_custom_fov = 85.0
		get_window().size = Vector2i(480, 320)
	if _frames == 34:
		_check(calls == 2 and objects == 1, "sky and cube draw together after projection change and resize")
		_environment.background_mode = Environment.BG_COLOR
		_environment.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	if _frames == 44:
		_check(calls == 1 and objects == 1, "explicit sky ambient can use a hidden sky")
		_environment.sky = null
	if _frames == 54:
		_check(calls == 1 and objects == 1, "removing the sky preserves the scene")
		if not _failed:
			print("SKY SCENE PASS: PanoramaSkyMaterial, ProceduralSkyMaterial, empty scene and foreground mesh")
		get_tree().quit(1 if _failed else 0)
