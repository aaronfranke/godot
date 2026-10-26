extends Node2D

class Sample extends Node2D:
	var texture
	var normal
	var mesh
	var multimesh

	func _triangle(offset):
		return PoolVector2Array([offset, offset + Vector2(16, 0), offset + Vector2(0, 12)])

	func _draw():
		var colors = PoolColorArray([Color.white])
		var uv = PoolVector2Array([Vector2.ZERO, Vector2(1, 0), Vector2(0, 1)])
		draw_rect(Rect2(4, 4, 12, 12), Color.red)
		draw_texture_rect(texture, Rect2(20, 4, 12, 12), false, Color.white, false, normal)
		draw_primitive(_triangle(Vector2(4, 22)), colors, uv, texture, 1.0, normal)
		draw_polygon(_triangle(Vector2(26, 22)), colors, uv, texture, normal)
		draw_mesh(mesh, texture, normal, Transform2D(0, Vector2(4, 42)))
		draw_multimesh(multimesh, texture, normal)
		var normal_rid = RID()
		if normal != null:
			normal_rid = normal.get_rid()
		VisualServer.canvas_item_add_nine_patch(get_canvas_item(), Rect2(40, 4, 20, 12),
				Rect2(0, 0, 8, 8), texture.get_rid(), Vector2(2, 2), Vector2(2, 2),
				VisualServer.NINE_PATCH_TILE, VisualServer.NINE_PATCH_STRETCH, true, Color.white, normal_rid)


var _viewport
var _sample
var _captures = ""
var _failed = false


func _check(condition, message):
	if not condition and not _failed:
		_failed = true
		printerr("CANVAS EFFECT FALLBACK FAIL: ", message)
		get_tree().quit(1)
	return condition


func _texture(color, size):
	var image = Image.new()
	image.create(size, size, false, Image.FORMAT_RGBA8)
	image.fill(color)
	var texture = ImageTexture.new()
	texture.create_from_image(image, 0)
	return texture


func _ready():
	for argument in OS.get_cmdline_args():
		if argument.begins_with("--shadow-capture="):
			_captures = argument.substr(17)
	get_viewport().usage = Viewport.USAGE_2D
	get_viewport().hdr = false
	_viewport = Viewport.new()
	_viewport.size = Vector2(96, 64)
	_viewport.usage = Viewport.USAGE_2D
	_viewport.disable_3d = true
	_viewport.hdr = false
	_viewport.world_2d = World2D.new()
	_viewport.render_target_v_flip = true
	_viewport.render_target_update_mode = Viewport.UPDATE_ALWAYS
	add_child(_viewport)

	_sample = Sample.new()
	_sample.texture = _texture(Color(0, 1, 0), 8)
	_sample.mesh = ArrayMesh.new()
	var arrays = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = _sample._triangle(Vector2.ZERO)
	arrays[Mesh.ARRAY_TEX_UV] = PoolVector2Array([Vector2.ZERO, Vector2(1, 0), Vector2(0, 1)])
	_sample.mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, [], 0)
	_sample.multimesh = MultiMesh.new()
	_sample.multimesh.transform_format = MultiMesh.TRANSFORM_2D
	_sample.multimesh.mesh = _sample.mesh
	_sample.multimesh.instance_count = 1
	_sample.multimesh.set_instance_transform_2d(0, Transform2D(0, Vector2(26, 42)))
	_viewport.add_child(_sample)

	var marker = ColorRect.new()
	marker.rect_position = Vector2(66, 4)
	marker.rect_size = Vector2(16, 12)
	marker.color = Color.blue
	_viewport.add_child(marker)
	VisualServer.canvas_item_set_z_index(marker.get_canvas_item(), 1)

	var preview = TextureRect.new()
	preview.texture = _viewport.get_texture()
	preview.rect_position = Vector2(16, 16)
	add_child(preview)
	call_deferred("_run", marker)


func _settle():
	for _i in range(4):
		yield(get_tree(), "idle_frame")


func _read(viewport, stage):
	var image = viewport.get_texture().get_data()
	if not _check(image != null and not image.is_empty() and image.get_size() == viewport.size, "missing completed image for " + stage):
		return null
	if _captures != "" and not _check(image.save_png(_captures.plus_file(stage + ".png")) == OK, "save capture"):
		return null
	return image


func _verify_stage(stage, expected, expected_root):
	var image = _read(_viewport, stage)
	if image == null or not _check(image.get_data() == expected, "viewport changed with " + stage):
		return false
	var root_image = _read(get_viewport(), stage + "_window")
	return root_image != null and _check(root_image.get_data() == expected_root, "window composition changed with " + stage)


func _run(marker):
	yield(_settle(), "completed")
	var baseline = _read(_viewport, "base")
	var root_image = _read(get_viewport(), "base_window")
	if baseline == null or root_image == null:
		return
	baseline.lock()
	var samples = [Vector2(22, 6), Vector2(42, 6), Vector2(6, 24), Vector2(28, 24), Vector2(6, 44), Vector2(28, 44)]
	var visible = baseline.get_pixel(6, 6).r > 0.9 and baseline.get_pixel(68, 6).b > 0.9
	for point in samples:
		visible = visible and baseline.get_pixelv(point).g > 0.9
	baseline.unlock()
	if not _check(visible, "base canvas commands are visible"):
		return

	_sample.normal = _texture(Color(1, 0.5, 0.5), 8)
	_sample.update()
	var copy = BackBufferCopy.new()
	copy.copy_mode = BackBufferCopy.COPY_MODE_VIEWPORT
	_viewport.add_child(copy)
	# A copy flag on a drawable item must still leave its own commands visible.
	VisualServer.canvas_item_set_copy_to_backbuffer(_sample.get_canvas_item(), true, Rect2(0, 0, 24, 24))
	var light = Light2D.new()
	light.texture = _texture(Color.white, 128)
	light.position = Vector2(48, 32)
	_viewport.add_child(light)
	yield(_settle(), "completed")
	if not _verify_stage("add_light_normal_and_copy", baseline.get_data(), root_image.get_data()):
		return
	light.mode = Light2D.MODE_MASK
	light.range_item_cull_mask = 1
	yield(_settle(), "completed")
	if not _verify_stage("mask_light", baseline.get_data(), root_image.get_data()):
		return
	marker.color = Color.yellow
	yield(_settle(), "completed")
	var updated = _read(_viewport, "live_update")
	var updated_root = _read(get_viewport(), "live_update_window")
	if updated == null or updated_root == null:
		return
	updated.lock()
	var pixel = updated.get_pixel(68, 6)
	updated.unlock()
	if not _check(pixel.r > 0.9 and pixel.g > 0.9 and pixel.b < 0.1, "canvas stays live with effects enabled"):
		return
	if not _check(updated_root.get_data() != root_image.get_data(), "updated viewport is composed into the window"):
		return
	marker.color = Color.blue
	light.queue_free()
	copy.queue_free()
	VisualServer.canvas_item_set_copy_to_backbuffer(_sample.get_canvas_item(), false, Rect2())
	yield(_settle(), "completed")
	if not _verify_stage("normal_map_only", baseline.get_data(), root_image.get_data()):
		return
	print("CANVAS EFFECT FALLBACK PASS: additive and mask lights, normal maps and back-buffer requests preserve unlit viewport and window output.")
	get_tree().quit()
