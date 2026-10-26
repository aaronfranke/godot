extends Spatial

var _frames = 0
var _failed = false
var _material


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("CUTOUT SCENE FAIL: ", message)


func _ready():
	get_viewport().usage = Viewport.USAGE_3D_NO_EFFECTS
	var image = Image.new()
	image.create(8, 8, false, Image.FORMAT_RGBA8)
	image.lock()
	for y in range(8):
		for x in range(8):
			image.set_pixel(x, y, Color(1, 0.25, 0.1, 1 if (x + y) % 2 else 0))
	image.unlock()
	var texture = ImageTexture.new()
	texture.create_from_image(image, 0)
	_material = SpatialMaterial.new()
	_material.flags_unshaded = true
	_material.params_use_alpha_scissor = true
	_material.params_alpha_scissor_threshold = 0.4
	_material.albedo_color = Color(1, 1, 1, 0.8)
	_material.albedo_texture = texture
	var mesh = QuadMesh.new()
	mesh.size = Vector2(4, 3)
	var front = MeshInstance.new()
	front.mesh = mesh
	front.material_override = _material
	add_child(front)
	var back = MeshInstance.new()
	back.mesh = mesh
	back.translation.z = -0.5
	back.material_override = SpatialMaterial.new()
	back.material_override.flags_unshaded = true
	back.material_override.albedo_color = Color(0.1, 0.7, 0.25)
	add_child(back)
	var camera = Camera.new()
	camera.translation.z = 5
	camera.projection = Camera.PROJECTION_ORTHOGONAL
	camera.size = 5
	add_child(camera)
	camera.current = true
	var layer = CanvasLayer.new()
	add_child(layer)
	var label = Label.new()
	label.text = "Alpha-cutout unlit 3D: green background through orange checker holes"
	label.rect_position = Vector2(8, 8)
	layer.add_child(label)


func _process(_delta):
	_frames += 1
	if _frames == 12:
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_OBJECTS_IN_FRAME) == 2, "two mesh instances")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME) == 2, "one draw per surface")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_VERTICES_IN_FRAME) == 12, "two quads")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_2D_DRAW_CALLS_IN_FRAME) > 0, "canvas overlay")
		_material.params_alpha_scissor_threshold = 0.95
	if _frames == 18:
		_material.params_alpha_scissor_threshold = 0.4
		OS.window_size = Vector2(640, 360)
	if _frames == 25:
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_OBJECTS_IN_FRAME) == 2, "meshes survive threshold edits and resize")
		if not _failed:
			print("CUTOUT SCENE PASS driver=", OS.get_video_driver_name(OS.get_current_video_driver()), " objects=2 vertices=12")
		if not OS.get_cmdline_args().has("--smoke-hold"):
			get_tree().quit(1 if _failed else 0)
