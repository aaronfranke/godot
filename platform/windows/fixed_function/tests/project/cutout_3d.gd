extends Node3D

var _frames = 0
var _failed = false
var _material


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("CUTOUT SCENE FAIL: ", message)


func _ready():
	var image = Image.create(8, 8, false, Image.FORMAT_RGBA8)
	for y in range(8):
		for x in range(8):
			image.set_pixel(x, y, Color(1, 0.25, 0.1, 1 if (x + y) % 2 else 0))
	var texture = ImageTexture.create_from_image(image)
	_material = StandardMaterial3D.new()
	_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR
	_material.alpha_scissor_threshold = 0.4
	_material.albedo_color = Color(1, 1, 1, 0.8)
	_material.albedo_texture = texture
	_material.texture_filter = BaseMaterial3D.TEXTURE_FILTER_NEAREST
	var mesh = QuadMesh.new()
	mesh.size = Vector2(4, 3)
	var front = MeshInstance3D.new()
	front.mesh = mesh
	front.material_override = _material
	add_child(front)
	var back = MeshInstance3D.new()
	back.mesh = mesh
	back.position.z = -0.5
	back.material_override = StandardMaterial3D.new()
	back.material_override.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	back.material_override.albedo_color = Color(0.1, 0.7, 0.25)
	add_child(back)
	var camera = Camera3D.new()
	camera.position.z = 5
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = 5
	add_child(camera)
	camera.current = true
	var layer = CanvasLayer.new()
	add_child(layer)
	var label = Label.new()
	label.text = "Alpha-cutout unlit 3D: green background through orange checker holes"
	label.position = Vector2(8, 8)
	layer.add_child(label)


func _process(_delta):
	_frames += 1
	if _frames == 12:
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_OBJECTS_IN_FRAME) == 2, "two mesh instances")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME) == 2, "one draw per surface")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_PRIMITIVES_IN_FRAME) == 4, "two quads")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_CANVAS, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME) > 0, "canvas overlay")
		_material.alpha_scissor_threshold = 0.95
		if OS.get_cmdline_user_args().has("--alpha-blend"):
			_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	if _frames == 18:
		_material.alpha_scissor_threshold = 0.4
		DisplayServer.window_set_size(Vector2i(640, 360))
	if _frames == 25:
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_OBJECTS_IN_FRAME) == 2, "meshes survive threshold edits and resize")
		if not _failed:
			print("CUTOUT SCENE PASS driver=", RenderingServer.get_current_rendering_driver_name(), " objects=2 primitives=4")
		if not OS.get_cmdline_user_args().has("--smoke-hold"):
			get_tree().quit(1 if _failed else 0)
