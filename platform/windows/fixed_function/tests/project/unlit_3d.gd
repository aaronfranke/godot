extends Spatial

var _frames = 0
var _failed = false
var _material
var _cube


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("UNLIT SCENE FAIL: ", message)


func _ready():
	# The shared smoke project defaults to a 2D-only framebuffer.
	get_viewport().usage = Viewport.USAGE_3D
	_material = SpatialMaterial.new()
	_material.flags_unshaded = true
	_material.albedo_color = Color(0.8, 0.25, 0.1)
	var mesh = CubeMesh.new()
	mesh.material = _material
	_cube = MeshInstance.new()
	_cube.mesh = mesh
	_cube.translation.x = -1.1
	add_child(_cube)

	# A second instance shares geometry but has an independent material override.
	var other = MeshInstance.new()
	other.mesh = mesh
	other.material_override = SpatialMaterial.new()
	other.material_override.flags_unshaded = true
	var image = Image.new()
	image.create(2, 2, false, Image.FORMAT_RGBA8)
	image.lock()
	image.set_pixel(0, 0, Color(1, 1, 1))
	image.set_pixel(1, 0, Color(0, 1, 0))
	image.set_pixel(0, 1, Color(0, 0, 1))
	image.set_pixel(1, 1, Color(1, 0, 0))
	image.unlock()
	var texture = ImageTexture.new()
	texture.create_from_image(image, 0)
	other.material_override.albedo_texture = texture
	# CubeMesh allocates one sixth of its UV atlas to the front face.
	other.material_override.uv1_scale = Vector3(3, 2, 1)
	other.translation.x = 1.1
	other.scale.x = -1
	add_child(other)

	var camera = Camera.new()
	camera.translation = Vector3(0, 1, 6)
	add_child(camera)
	camera.look_at(Vector3.ZERO, Vector3.UP)
	camera.current = true
	var layer = CanvasLayer.new()
	add_child(layer)
	var label = Label.new()
	label.text = "Opaque unlit 3D + CanvasItem"
	label.rect_position = Vector2(8, 8)
	layer.add_child(label)


func _process(_delta):
	_frames += 1
	if _frames == 12:
		var driver = OS.get_video_driver_name(OS.get_current_video_driver())
		if driver == "D3D7" or driver == "D3D9":
			_check(VisualServer.get_render_info(VisualServer.INFO_VERTEX_MEM_USED) > 0, "retained mesh buffer uploaded")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_OBJECTS_IN_FRAME) == 2, "two mesh instances")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME) == 2, "two spatial draws")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_VERTICES_IN_FRAME) == 72, "two cubes")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_2D_DRAW_CALLS_IN_FRAME) > 0, "canvas overlay")
		_material.albedo_color = Color(0.2, 0.5, 1)
		_cube.rotation.y = 0.3
	if _frames == 18:
		OS.window_size = Vector2(480, 300)
	if _frames == 25:
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_OBJECTS_IN_FRAME) == 2, "meshes survive resize")
		if not _failed:
			print("UNLIT SCENE PASS driver=", OS.get_video_driver_name(OS.get_current_video_driver()), " objects=2 vertices=72")
		if not OS.get_cmdline_args().has("--smoke-hold"):
			get_tree().quit(1 if _failed else 0)
