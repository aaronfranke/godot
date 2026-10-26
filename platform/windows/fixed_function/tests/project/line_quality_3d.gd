extends Spatial

var _frames = 0
var _failed = false
var _red_material
var _lines
var _requested_aa


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("LINE QUALITY SCENE FAIL: ", message)


func _quad(position, color, transparent):
	var material = SpatialMaterial.new()
	material.flags_unshaded = true
	material.flags_transparent = transparent
	material.albedo_color = color
	var mesh = QuadMesh.new()
	mesh.size = Vector2(3, 2.5)
	mesh.material = material
	var instance = MeshInstance.new()
	instance.mesh = mesh
	instance.translation = position
	add_child(instance)
	return material


func _ready():
	get_viewport().usage = Viewport.USAGE_3D
	OS.set_window_title("Fixed Function line quality " + OS.get_video_driver_name(OS.get_current_video_driver()))
	_requested_aa = ProjectSettings.get_setting("rendering/quality/fixed_function/antialiased_lines")
	ProjectSettings.set_setting("rendering/quality/fixed_function/antialiased_lines", true)
	# Deliberately submit the nearer transparent surface before the background/far surface.
	_red_material = _quad(Vector3(0.5, 0, 1), Color(1, 0, 0, 0.5), true)
	_quad(Vector3(0, 0, 0), Color(0.2, 0.2, 0.2), false)
	_quad(Vector3(-0.5, 0, 0.5), Color(0, 1, 0, 0.5), true)
	var vertices = PoolVector3Array()
	var colors = PoolColorArray()
	for i in range(6):
		vertices.append(Vector3(-2.5, -2.0 + i * 0.6, 1.5))
		vertices.append(Vector3(2.5, -1.8 + i * 0.5, 1.5))
		colors.append(Color(0.25, 0.5, 1, 0.75))
		colors.append(Color(1, 1, 1, 0.5))
	var arrays = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_COLOR] = colors
	var mesh = ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_LINES, arrays, [], 0)
	var material = SpatialMaterial.new()
	material.flags_unshaded = true
	material.flags_transparent = true
	material.vertex_color_use_as_albedo = true
	material.params_line_width = 3.0
	mesh.surface_set_material(0, material)
	_lines = MeshInstance.new()
	_lines.mesh = mesh
	add_child(_lines)
	var camera = Camera.new()
	camera.projection = Camera.PROJECTION_ORTHOGONAL
	camera.size = 6
	camera.translation.z = 6
	add_child(camera)
	camera.current = true
	var layer = CanvasLayer.new()
	add_child(layer)
	var label = Label.new()
	label.text = "Alpha surfaces + line coverage AA\nAA off at frame 12; on at frame 20"
	label.rect_position = Vector2(8, 8)
	layer.add_child(label)


func _process(_delta):
	_frames += 1
	if _frames == 12 or _frames == 20 or _frames == 40:
		var expected = 54 if _frames == 20 else 198
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_OBJECTS_IN_FRAME) == 4, "four spatial objects")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_VERTICES_IN_FRAME) == expected, "AA changes submitted geometry")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME) == 4, "four bounded draws")
	if _frames == 12:
		ProjectSettings.set_setting("rendering/quality/fixed_function/antialiased_lines", false)
		_red_material.albedo_color.a = 0.25
	if _frames == 20:
		ProjectSettings.set_setting("rendering/quality/fixed_function/antialiased_lines", true)
	if _frames == 26:
		OS.window_size = Vector2(800, 520)
		_lines.scale = Vector3(0.8, 1.1, 1)
	if _frames == 40:
		if not _failed:
			print("LINE QUALITY SCENE PASS driver=", OS.get_video_driver_name(OS.get_current_video_driver()))
		if not OS.get_cmdline_args().has("--smoke-hold"):
			ProjectSettings.set_setting("rendering/quality/fixed_function/antialiased_lines", _requested_aa)
			get_tree().quit(1 if _failed else 0)
