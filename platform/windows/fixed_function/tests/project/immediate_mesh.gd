extends Node3D

const BACKGROUND = Color.BLACK
const TRIANGLE_CENTER = Vector3(-2.5, 1.5, 0)
const STRIP_CENTER = Vector3(1.5, 1.5, 0)
const POINT = Vector3(-2.5, -1.5, 0)
# Center the one-pixel lines on row 88 of the initial orthographic viewport.
const LINE_CENTER = Vector3(-0.5, -1.53125, 0)
const LINE_STRIP_CENTER = Vector3(2.5, -1.53125, 0)

var _target: SubViewport
var _camera: Camera3D
var _instance: MeshInstance3D
var _mesh = ImmediateMesh.new()
var _failed = false
var _captures = ""


func _check(condition, message):
	if not condition and not _failed:
		_failed = true
		printerr("IMMEDIATE MESH FAIL: ", message)
		get_tree().quit(1)
	return condition


func _material(color):
	var material = StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	material.albedo_color = color
	return material


func _surface(primitive, material, vertices):
	_mesh.surface_begin(primitive, material)
	for vertex in vertices:
		_mesh.surface_add_vertex(vertex)
	_mesh.surface_end()


func _triangle(center, material):
	_surface(Mesh.PRIMITIVE_TRIANGLES, material, [
		center + Vector3(-0.7, -0.6, 0),
		center + Vector3(0.7, -0.6, 0),
		center + Vector3(0, 0.8, 0),
	])


func _populate():
	var painted = _material(Color.WHITE)
	painted.vertex_color_use_as_albedo = true
	_mesh.surface_begin(Mesh.PRIMITIVE_TRIANGLES, painted)
	_mesh.surface_set_color(Color.RED)
	_mesh.surface_set_normal(Vector3.BACK)
	_mesh.surface_add_vertex(TRIANGLE_CENTER + Vector3(-0.7, -0.6, 0))
	_mesh.surface_add_vertex(TRIANGLE_CENTER + Vector3(0.7, -0.6, 0))
	_mesh.surface_add_vertex(TRIANGLE_CENTER + Vector3(0, 0.8, 0))
	_mesh.surface_end()

	var texture = Image.create_empty(8, 8, false, Image.FORMAT_RGBA8)
	texture.fill(Color.RED)
	texture.fill_rect(Rect2i(4, 0, 4, 8), Color.GREEN)
	var textured = _material(Color.WHITE)
	textured.albedo_texture = ImageTexture.create_from_image(texture)
	textured.texture_filter = BaseMaterial3D.TEXTURE_FILTER_NEAREST
	_mesh.surface_begin(Mesh.PRIMITIVE_TRIANGLE_STRIP, textured)
	_mesh.surface_set_uv(Vector2(0.75, 0.5))
	_mesh.surface_set_uv2(Vector2(0.25, 0.5))
	for offset in [Vector3(-0.8, -0.8, 0), Vector3(-0.8, 0.8, 0), Vector3(0.8, -0.8, 0), Vector3(0.8, 0.8, 0)]:
		_mesh.surface_add_vertex(STRIP_CENTER + offset)
	_mesh.surface_end()

	_surface(Mesh.PRIMITIVE_POINTS, _material(Color.WHITE), [POINT])
	_surface(Mesh.PRIMITIVE_LINES, _material(Color.BLUE), [
		LINE_CENTER + Vector3(-0.65, 0, 0),
		LINE_CENTER + Vector3(0.65, 0, 0),
	])
	_surface(Mesh.PRIMITIVE_LINE_STRIP, _material(Color.YELLOW), [
		LINE_STRIP_CENTER + Vector3(-0.65, 0, 0),
		LINE_STRIP_CENTER + Vector3(0.2, 0, 0),
		LINE_STRIP_CENTER + Vector3(0.65, 0.5, 0),
	])


func _ready():
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--shadow-capture="):
			_captures = argument.substr(17)
	_target = SubViewport.new()
	_target.size = Vector2i(160, 128)
	_target.own_world_3d = true
	_target.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(_target)

	_camera = Camera3D.new()
	_camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	_camera.size = 8.0
	_camera.position.z = 5.0
	_target.add_child(_camera)
	_camera.current = true

	var world = WorldEnvironment.new()
	world.environment = Environment.new()
	world.environment.background_mode = Environment.BG_COLOR
	world.environment.background_color = BACKGROUND
	_target.add_child(world)

	_instance = MeshInstance3D.new()
	_instance.mesh = _mesh
	_target.add_child(_instance)
	_populate()
	call_deferred("_run")


func _settle():
	for _i in range(4):
		await get_tree().process_frame


func _capture(name):
	var image = _target.get_texture().get_image()
	if not _check(image != null and not image.is_empty() and image.get_size() == _target.size,
			"missing or incorrectly sized viewport image"):
		return null
	if _captures != "":
		_check(image.save_png(_captures.path_join(name + ".png")) == OK, "capture save failed")
	return image


func _pixel(image, point, expected, radius = 0):
	var position = Vector2i(_camera.unproject_position(point))
	for y in range(position.y - radius, position.y + radius + 1):
		for x in range(position.x - radius, position.x + radius + 1):
			var actual = image.get_pixel(x, y)
			if absf(actual.r - expected.r) < 0.05 and absf(actual.g - expected.g) < 0.05 and absf(actual.b - expected.b) < 0.05:
				return true
	return _check(false, "near %s expected %s, found %s" % [position, expected, image.get_pixelv(position)])


func _run():
	await _settle()
	if not _check(_mesh.get_surface_count() == 5, "five immediate surfaces"):
		return
	if not _check(_mesh.get_aabb().size.x > 6.0, "combined source bounds"):
		return
	var image = _capture("immediate_surfaces")
	if image == null:
		return
	if not _pixel(image, TRIANGLE_CENTER, Color.RED) or not _pixel(image, STRIP_CENTER, Color.GREEN):
		return
	if not _pixel(image, POINT, Color.WHITE, 3) or not _pixel(image, LINE_CENTER, Color.BLUE, 3) or not _pixel(image, LINE_STRIP_CENTER, Color.YELLOW, 3):
		return

	_instance.material_override = _material(Color.MAGENTA)
	await _settle()
	image = _capture("immediate_override")
	if image == null or not _pixel(image, STRIP_CENTER, Color.MAGENTA):
		return
	_instance.material_override = null
	_mesh.clear_surfaces()
	await _settle()
	image = _capture("immediate_cleared")
	if image == null or not _check(_mesh.get_surface_count() == 0, "surface count after clearing") or not _pixel(image, TRIANGLE_CENTER, BACKGROUND):
		return

	_triangle(Vector3(30, 0, 0), _material(Color.CYAN))
	await _settle()
	if not _check(_mesh.get_aabb().position.x > 29, "bounds reset after clearing"):
		return
	if not _check(_target.get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_OBJECTS_IN_FRAME) == 0, "rebuilt mesh culled outside camera"):
		return

	_mesh.clear_surfaces()
	_triangle(Vector3.ZERO, _material(Color.CYAN))
	_target.size = Vector2i(192, 144)
	await _settle()
	image = _capture("immediate_rebuilt")
	if image == null or not _pixel(image, Vector3.ZERO, Color.CYAN) or not _pixel(image, STRIP_CENTER, BACKGROUND):
		return
	_instance.queue_free()
	_mesh = null
	await _settle()
	image = _capture("immediate_released")
	if image == null or not _pixel(image, Vector3.ZERO, BACKGROUND):
		return
	print("IMMEDIATE MESH PASS: all five topologies, vertex color, texture UVs, material override, clear/rebuild, bounds/culling, resize and release.")
	get_tree().quit()
