extends Node3D

var _frames = 0
var _failed = false
var _sphere
var _texture
var _material
var _normal_setting
var _normal_draw_calls = 0


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("NORMAL MAPPING SCENE FAIL: ", message)


func _ready():
	get_viewport().disable_3d = false
	_normal_setting = ProjectSettings.get_setting("rendering/quality/fixed_function/normal_mapping")
	var image = Image.create_empty(32, 32, false, Image.FORMAT_RGBA8)
	for y in range(32):
		for x in range(32):
			# Positive-Z tangent normals with deliberately uninformative blue.
			image.set_pixel(x, y, Color(0.5 + 0.35 * sin(x * PI / 4), 0.5 + 0.35 * sin(y * PI / 4), 0))
	_texture = ImageTexture.create_from_image(image)
	_material = StandardMaterial3D.new()
	_material.albedo_color = Color(0.65, 0.8, 0.4)
	_material.normal_enabled = true
	_material.normal_texture = _texture
	_sphere = MeshInstance3D.new()
	_sphere.mesh = SphereMesh.new()
	_sphere.material_override = _material
	add_child(_sphere)
	var camera = Camera3D.new()
	camera.position = Vector3(0, 0, 4)
	camera.current = true
	add_child(camera)
	var light = DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-25, -30, 0)
	light.light_energy = 0.8
	add_child(light)
	var world = WorldEnvironment.new()
	world.environment = Environment.new()
	world.environment.background_mode = Environment.BG_COLOR
	world.environment.background_color = Color(0.08, 0.1, 0.15)
	world.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	world.environment.ambient_light_color = Color(1, 1, 1)
	world.environment.ambient_light_energy = 0.1
	add_child(world)
	var layer = CanvasLayer.new()
	add_child(layer)
	var label = Label.new()
	label.text = "D3D9 SM3: tangent-space ripples | D3D7: geometry normals"
	label.position = Vector2(8, 8)
	layer.add_child(label)


func _process(_delta):
	_frames += 1
	if _frames == 12:
		ProjectSettings.set_setting("rendering/quality/fixed_function/normal_mapping", false)
	if _frames == 20:
		ProjectSettings.set_setting("rendering/quality/fixed_function/normal_mapping", true)
		_sphere.scale = Vector3(-1, 0.9, 1)
		_material.normal_scale = 0.8
	if _frames == 28:
		_normal_draw_calls = get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME)
		_material.ao_enabled = true
		_material.ao_texture = _texture
		_material.ao_texture_channel = BaseMaterial3D.TEXTURE_CHANNEL_GREEN
		_material.ao_light_affect = 0.3
		get_window().size = Vector2i(480, 320)
	if _frames == 36:
		_check(_material.normal_enabled and _material.normal_texture == _texture, "CPU material survives quality switch")
		_check(_sphere.mesh.get_mesh_arrays()[Mesh.ARRAY_TANGENT].size() > 0, "authored sphere tangents")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_OBJECTS_IN_FRAME) == 1, "sphere rendered after mirror/resize")
		_check(_normal_draw_calls > 0 and get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME) == _normal_draw_calls, "texture AO adds no draws")
		if not _failed:
			print("NORMAL MAPPING SCENE PASS driver=", RenderingServer.get_current_rendering_driver_name())
		if not OS.get_cmdline_args().has("--smoke-hold"):
			ProjectSettings.set_setting("rendering/quality/fixed_function/normal_mapping", _normal_setting)
			get_tree().quit(1 if _failed else 0)
