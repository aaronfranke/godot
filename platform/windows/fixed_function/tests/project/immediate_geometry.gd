extends Node2D

const CLEAR_COLOR = Color(0.125, 0.25, 0.5)

var _target
var _camera
var _geometry
var _failed = false
var _captures = ""


func _check(condition, message):
	if not condition and not _failed:
		_failed = true
		printerr("IMMEDIATE GEOMETRY FAIL: ", message)
		get_tree().quit(1)
	return condition


func _settle():
	for _i in range(4):
		yield(get_tree(), "idle_frame")


func _texture(color):
	var image = Image.new()
	image.create(2, 2, false, Image.FORMAT_RGBA8)
	image.fill(color)
	var texture = ImageTexture.new()
	texture.create_from_image(image, 0)
	return texture


func _batch(primitive, center, corners, color, texture = null):
	_geometry.begin(primitive, texture)
	_geometry.set_color(color)
	_geometry.set_normal(Vector3(0, 0, 1))
	_geometry.set_tangent(Plane(Vector3(1, 0, 0), 1))
	for corner in corners:
		_geometry.set_uv(Vector2(corner.x + 0.5, 0.5 - corner.y))
		_geometry.set_uv2(Vector2(0.75, 0.25))
		_geometry.add_vertex(center + corner)
	_geometry.end()


func _quad():
	return [Vector3(-0.4, 0.35, 0), Vector3(0.4, 0.35, 0), Vector3(0.4, -0.35, 0), Vector3(-0.4, -0.35, 0)]


func _triangles():
	var points = _quad()
	return [points[0], points[1], points[2], points[0], points[2], points[3]]


func _ready():
	OS.low_processor_usage_mode = false
	get_viewport().usage = Viewport.USAGE_2D
	get_viewport().hdr = false
	for argument in OS.get_cmdline_args():
		if argument.begins_with("--shadow-capture="):
			_captures = argument.substr(17)
	_target = Viewport.new()
	_target.size = Vector2(128, 96)
	_target.own_world = true
	_target.usage = Viewport.USAGE_3D
	_target.hdr = false
	_target.render_target_update_mode = Viewport.UPDATE_ALWAYS
	add_child(_target)
	_camera = Camera.new()
	_camera.translation = Vector3(0, 0, 5)
	_camera.projection = Camera.PROJECTION_ORTHOGONAL
	_camera.size = 4
	_target.add_child(_camera)
	_camera.current = true
	_geometry = ImmediateGeometry.new()
	var material = SpatialMaterial.new()
	material.flags_unshaded = true
	material.params_cull_mode = SpatialMaterial.CULL_DISABLED
	material.params_line_width = 3
	material.flags_use_point_size = true
	material.params_point_size = 5
	material.vertex_color_use_as_albedo = true
	if OS.get_video_driver_name(OS.get_current_video_driver()).begins_with("GLES"):
		# GLES2 does not restore its implicit white texture after a textured chunk.
		material.albedo_texture = _texture(Color.white)
	_geometry.material_override = material
	_target.add_child(_geometry)
	var points = _quad()
	_batch(Mesh.PRIMITIVE_TRIANGLES, Vector3(-2, 1.1, 0), _triangles(), Color(1, 0, 0))
	_batch(Mesh.PRIMITIVE_TRIANGLE_STRIP, Vector3(-0.65, 1.1, 0), [points[0], points[1], points[3], points[2]], Color.white, _texture(Color(0, 1, 0)))
	_batch(Mesh.PRIMITIVE_TRIANGLE_FAN, Vector3(0.65, 1.1, 0), points, Color(0, 0, 1))
	var left = Vector3(-0.4, 0, 0)
	var right = Vector3(0.4, 0, 0)
	_batch(Mesh.PRIMITIVE_LINES, Vector3(2, 1.1, 0), [left, right], Color(1, 1, 0))
	_batch(Mesh.PRIMITIVE_LINE_STRIP, Vector3(-2, -0.8, 0), [left, Vector3.ZERO, right], Color(1, 0, 1))
	_batch(Mesh.PRIMITIVE_LINE_LOOP, Vector3(-0.65, -0.8, 0), points, Color(0, 1, 1))
	_batch(Mesh.PRIMITIVE_POINTS, Vector3(0.65, -0.8, 0), [Vector3.ZERO], Color.white)
	var preview = TextureRect.new()
	preview.texture = _target.get_texture()
	preview.expand = true
	preview.rect_position = Vector2(8, 8)
	preview.rect_size = Vector2(384, 288)
	add_child(preview)
	call_deferred("_run")


func _capture(name):
	var image = _target.get_texture().get_data()
	if not _check(image != null and not image.is_empty(), "missing viewport image: " + name):
		return null
	image.flip_y()
	if _captures != "":
		_check(image.save_png(_captures.plus_file(name + ".png")) == OK, "capture save failed")
	return image


func _sample(image, position, expected, radius = 0):
	if image == null:
		return false
	var point = _camera.unproject_position(position).floor()
	var matched = false
	image.lock()
	for y in range(-radius, radius + 1):
		for x in range(-radius, radius + 1):
			var color = image.get_pixelv(point + Vector2(x, y))
			if abs(color.r - expected.r) < 0.08 and abs(color.g - expected.g) < 0.08 and abs(color.b - expected.b) < 0.08:
				matched = true
	image.unlock()
	return _check(matched, "world position %s at %s expected %s" % [position, point, expected])


func _run():
	yield(_settle(), "completed")
	var image = _capture("primitives")
	var red = Vector3(-2, 1.1, 0)
	if not _sample(image, red, Color(1, 0, 0)) or not _sample(image, Vector3(-0.65, 1.1, 0), Color(0, 1, 0)) or not _sample(image, Vector3(0.65, 1.1, 0), Color(0, 0, 1)):
		return
	if not _sample(image, Vector3(2, 1.1, 0), Color(1, 1, 0), 2) or not _sample(image, Vector3(-2, -0.8, 0), Color(1, 0, 1), 2) or not _sample(image, Vector3(-1.05, -0.8, 0), Color(0, 1, 1), 2) or not _sample(image, Vector3(0.65, -0.8, 0), Color.white, 2):
		return
	_geometry.begin(Mesh.PRIMITIVE_TRIANGLES)
	_geometry.add_vertex(Vector3.ZERO)
	yield(_settle(), "completed")
	if not _sample(_capture("open_batch"), red, CLEAR_COLOR):
		return
	_geometry.end() # An incomplete triangle leaves the older batches intact.
	yield(_settle(), "completed")
	if not _sample(_capture("closed_batch"), red, Color(1, 0, 0)):
		return
	_geometry.clear()
	_batch(Mesh.PRIMITIVE_TRIANGLES, Vector3(100, 0, 0), _triangles(), Color.white)
	yield(_settle(), "completed")
	if not _sample(_capture("culled"), red, CLEAR_COLOR) or not _check(_target.get_render_info(Viewport.RENDER_INFO_OBJECTS_IN_FRAME) == 0, "old bounds survived clear/rebuild"):
		return
	_geometry.clear()
	var material = ShaderMaterial.new()
	material.shader = Shader.new()
	material.shader.code = "shader_type spatial; render_mode unshaded,cull_disabled; uniform sampler2D texture_albedo : hint_albedo; void fragment() { ALBEDO = texture(texture_albedo, UV2).rgb; }"
	material.set_shader_param("texture_albedo", _texture(Color(1, 0, 1)))
	_geometry.material_override = material
	var uv_image = Image.new()
	uv_image.create(2, 2, false, Image.FORMAT_RGBA8)
	uv_image.fill(Color(1, 0, 0))
	uv_image.lock()
	uv_image.set_pixel(1, 0, Color(0, 1, 0))
	uv_image.unlock()
	var texture = ImageTexture.new()
	texture.create_from_image(uv_image, 0)
	_batch(Mesh.PRIMITIVE_TRIANGLES, Vector3.ZERO, _triangles(), Color.white, texture)
	yield(_settle(), "completed")
	if not _sample(_capture("uv2_rebuilt"), Vector3.ZERO, Color(0, 1, 0)):
		return
	var preset = SpatialMaterial.new()
	preset.flags_unshaded = true
	preset.params_cull_mode = SpatialMaterial.CULL_DISABLED
	_geometry.material_override = null
	VisualServer.immediate_set_material(_geometry.get_base(), preset.get_rid())
	if not _check(VisualServer.immediate_get_material(_geometry.get_base()) == preset.get_rid(), "base material was not retained"):
		return
	var driver = OS.get_video_driver_name(OS.get_current_video_driver())
	var native_retained = driver == "D3D7" or driver == "D3D9"
	var green = _texture(Color(0, 1, 0))
	for i in range(6):
		_geometry.clear()
		_batch(Mesh.PRIMITIVE_TRIANGLES, Vector3.ZERO, _triangles(), Color.white, green)
		yield(get_tree(), "idle_frame")
		if native_retained and i >= 2 and not _check(VisualServer.get_render_info(VisualServer.INFO_VERTEX_MEM_USED) == 0, "per-frame rebuild queued retained uploads"):
			return
	yield(_settle(), "completed")
	if not _sample(_capture("rebuilt_preset"), Vector3.ZERO, Color(0, 1, 0)):
		return
	if native_retained:
		if not _check(VisualServer.get_render_info(VisualServer.INFO_VERTEX_MEM_USED) > 0, "retained immediate geometry was not uploaded"):
			return
	var override_material = preset.duplicate()
	override_material.albedo_color = Color(1, 0, 0)
	_geometry.material_override = override_material
	yield(_settle(), "completed")
	if not _sample(_capture("material_override"), Vector3.ZERO, Color.black):
		return
	_geometry.material_override = null
	_target.size = Vector2(160, 120)
	yield(_settle(), "completed")
	if not _sample(_capture("resized"), Vector3.ZERO, Color(0, 1, 0)):
		return
	_geometry.clear()
	if driver.begins_with("GLES"):
		# GLES leaves an exact zero-size point bound, which its scene cull discards.
		_geometry.set_custom_aabb(AABB(Vector3(-0.1, -0.1, -0.1), Vector3(0.2, 0.2, 0.2)))
	_batch(Mesh.PRIMITIVE_POINTS, Vector3.ZERO, [Vector3.ZERO], Color.white)
	yield(_settle(), "completed")
	if not _sample(_capture("single_point"), Vector3.ZERO, Color.white, 2):
		return
	_geometry.begin(Mesh.PRIMITIVE_POINTS)
	_geometry.add_vertex(Vector3.ZERO)
	_geometry.queue_free()
	yield(_settle(), "completed")
	if not _sample(_capture("released"), Vector3.ZERO, CLEAR_COLOR):
		return
	print("IMMEDIATE GEOMETRY PASS: all primitive modes, per-batch textures, vertex colors, open/empty batches, culling, UV2, retained draws, resize and release.")
	get_tree().quit()
