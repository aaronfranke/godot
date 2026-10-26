extends Node3D

var _frames = 0
var _failed = false
var _material
var _cube


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("UNLIT SCENE FAIL: ", message)


func _ready():
	_material = StandardMaterial3D.new()
	_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_material.albedo_color = Color(0.8, 0.25, 0.1)
	var mesh = BoxMesh.new()
	mesh.size = Vector3(2, 2, 2)
	mesh.material = _material
	_cube = MeshInstance3D.new()
	_cube.mesh = mesh
	_cube.position.x = -1.1
	add_child(_cube)

	# A second instance shares geometry but has an independent material override.
	var other = MeshInstance3D.new()
	other.mesh = mesh
	other.material_override = StandardMaterial3D.new()
	other.material_override.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	var image = Image.create(2, 2, false, Image.FORMAT_RGBA8)
	image.set_pixel(0, 0, Color(1, 1, 1))
	image.set_pixel(1, 0, Color(0, 1, 0))
	image.set_pixel(0, 1, Color(0, 0, 1))
	image.set_pixel(1, 1, Color(1, 0, 0))
	var texture = ImageTexture.create_from_image(image)
	other.material_override.texture_filter = BaseMaterial3D.TEXTURE_FILTER_NEAREST
	other.material_override.albedo_texture = texture
	# BoxMesh allocates one sixth of its UV atlas to the front face.
	other.material_override.uv1_scale = Vector3(3, 2, 1)
	other.position.x = 1.1
	other.scale.x = -1
	add_child(other)

	var camera = Camera3D.new()
	camera.position = Vector3(0, 1, 6)
	add_child(camera)
	camera.look_at(Vector3.ZERO, Vector3.UP)
	camera.current = true
	var layer = CanvasLayer.new()
	add_child(layer)
	var label = Label.new()
	label.text = "Opaque unlit 3D + CanvasItem"
	label.position = Vector2(8, 8)
	layer.add_child(label)


func _process(_delta):
	_frames += 1
	if _frames == 12:
		var driver = RenderingServer.get_current_rendering_driver_name()
		if driver == "d3d7" or driver == "d3d9":
			_check(RenderingServer.get_rendering_info(RenderingServer.RENDERING_INFO_BUFFER_MEM_USED) > 0, "retained mesh buffer uploaded")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_OBJECTS_IN_FRAME) == 2, "two mesh instances")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME) == 2, "two spatial draws")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_PRIMITIVES_IN_FRAME) == 24, "two cubes")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_CANVAS, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME) > 0, "canvas overlay")
		_material.albedo_color = Color(0.2, 0.5, 1)
		_cube.rotation.y = 0.3
	if _frames == 18:
		DisplayServer.window_set_size(Vector2i(480, 300))
	if _frames == 25:
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_OBJECTS_IN_FRAME) == 2, "meshes survive resize")
		if not _failed:
			print("UNLIT SCENE PASS driver=", RenderingServer.get_current_rendering_driver_name(), " objects=2 primitives=24")
		if not OS.get_cmdline_user_args().has("--smoke-hold"):
			get_tree().quit(1 if _failed else 0)
