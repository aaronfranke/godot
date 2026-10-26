extends Spatial

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
	get_viewport().usage = Viewport.USAGE_3D
	var image = Image.new()
	image.create(64, 32, false, Image.FORMAT_RGBA8)
	image.fill(Color(0.2, 0.6, 0.8))
	var texture = ImageTexture.new()
	texture.create_from_image(image, Texture.FLAG_FILTER)
	var panorama = PanoramaSky.new()
	panorama.panorama = texture
	var world = WorldEnvironment.new()
	_environment = Environment.new()
	_environment.background_mode = Environment.BG_SKY
	_environment.background_sky = panorama
	world.environment = _environment
	add_child(world)
	_camera = Camera.new()
	_camera.translation.z = 4
	_camera.current = true
	add_child(_camera)


func _process(_delta):
	_frames += 1
	var calls = get_viewport().get_render_info(Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME)
	var objects = get_viewport().get_render_info(Viewport.RENDER_INFO_OBJECTS_IN_FRAME)
	if _frames == 8:
		_check(calls == 1 and objects == 0, "PanoramaSky draws in an empty 3D scene")
		var sky = ProceduralSky.new()
		sky.texture_size = ProceduralSky.TEXTURE_SIZE_256
		_environment.background_sky = sky
	if _frames == 24:
		_check(calls == 1 and objects == 0, "ProceduralSky's generated image draws")
		var box = MeshInstance.new()
		box.mesh = CubeMesh.new()
		box.material_override = SpatialMaterial.new()
		box.material_override.flags_unshaded = true
		add_child(box)
		_camera.set_orthogonal(4.0, 0.05, 50.0)
		_environment.background_sky_custom_fov = 85.0
		OS.window_size = Vector2(480, 320)
	if _frames == 34:
		_check(calls == 2 and objects == 1, "sky and cube draw together after projection change and resize")
		_environment.background_sky = null
	if _frames == 44:
		_check(calls == 1 and objects == 1, "removing the sky preserves the scene")
		if not _failed:
			print("SKY SCENE PASS: PanoramaSky, ProceduralSky, empty scene and foreground mesh")
		get_tree().quit(1 if _failed else 0)
