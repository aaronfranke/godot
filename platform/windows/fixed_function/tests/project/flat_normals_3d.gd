extends Node3D

var _frames = 0
var _failed = false
var _mesh
var _cube


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("FLAT NORMALS SCENE FAIL: ", message)


func _ready():
	get_viewport().disable_3d = false
	var arrays = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PackedVector3Array([
		Vector3(-1, -1, -1), Vector3(1, -1, -1), Vector3(1, 1, -1), Vector3(-1, 1, -1),
		Vector3(-1, -1, 1), Vector3(1, -1, 1), Vector3(1, 1, 1), Vector3(-1, 1, 1),
	])
	arrays[Mesh.ARRAY_INDEX] = PackedInt32Array([
		4, 6, 5, 4, 7, 6, 5, 6, 2, 5, 2, 1,
		0, 3, 7, 0, 7, 4, 3, 2, 6, 3, 6, 7,
		0, 1, 2, 0, 2, 3, 0, 4, 5, 0, 5, 1,
	])
	# The eight shared source vertices deliberately have no authored normals.
	_mesh = ArrayMesh.new()
	_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, [], {}, 0)
	var material = StandardMaterial3D.new()
	material.albedo_color = Color(0.65, 0.85, 0.35)
	_cube = MeshInstance3D.new()
	_cube.mesh = _mesh
	_cube.material_override = material
	add_child(_cube)
	var camera = Camera3D.new()
	camera.name = "Camera"
	camera.position = Vector3(3, 2, 5)
	add_child(camera)
	camera.look_at(Vector3.ZERO, Vector3.UP)
	camera.current = true
	var light = DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-35, -25, 0)
	light.light_energy = 0.8
	add_child(light)
	var world = WorldEnvironment.new()
	world.environment = Environment.new()
	world.environment.background_mode = Environment.BG_COLOR
	world.environment.background_color = Color(0.08, 0.1, 0.15)
	world.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	world.environment.ambient_light_color = Color(1, 1, 1)
	world.environment.ambient_light_energy = 0.15
	add_child(world)
	var layer = CanvasLayer.new()
	add_child(layer)
	var label = Label.new()
	label.text = "Eight shared vertices, no normals: flat cube faces"
	label.position = Vector2(8, 8)
	layer.add_child(label)


func _process(_delta):
	_frames += 1
	if _frames == 12:
		_cube.rotation.y = 0.3
		_cube.scale.x = -1
	if _frames == 20:
		get_window().size = Vector2i(480, 320)
	if _frames == 28:
		_check(_mesh.surface_get_array_len(0) == 8, "canonical vertices preserved")
		_check(_mesh.surface_get_array_index_len(0) == 36, "canonical indices preserved")
		_check((_mesh.surface_get_format(0) & Mesh.ARRAY_FORMAT_NORMAL) == 0, "canonical normals remain absent")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_OBJECTS_IN_FRAME) == 1, "cube submitted after mirror/resize")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_PRIMITIVES_IN_FRAME) == 12, "twelve triangles")
		if not _failed:
			print("FLAT NORMALS SCENE PASS driver=", RenderingServer.get_current_rendering_driver_name(), " source_vertices=8 corners=36")
		if not OS.get_cmdline_args().has("--smoke-hold"):
			get_tree().quit(1 if _failed else 0)
