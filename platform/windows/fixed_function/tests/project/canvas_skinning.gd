extends Node2D

var _target
var _rig
var _left
var _right
var _polygon
var _source
var _failed = false
var _captures = ""


func _check(condition, message):
	if not condition and not _failed:
		_failed = true
		printerr("CANVAS SKINNING FAIL: ", message)
		get_tree().quit(1)
	return condition


func _settle():
	for _i in range(4):
		await get_tree().process_frame


func _image(name):
	var image = _target.get_texture().get_image()
	if not _check(image != null and not image.is_empty(), "missing viewport image"):
		return null
	if _captures != "":
		if not _check(image.save_png(_captures.path_join(name + ".png")) == OK, "capture save failed"):
			return null
	return image


func _pixel(image, point, expected):
	if image == null:
		return false
	var actual = image.get_pixelv(point)
	for channel in range(4):
		if not _check(abs(actual[channel] - expected[channel]) < 0.05,
				"pixel %s: %s expected %s" % [point, actual, expected]):
			return false
	return true


func _bone(name, position):
	var bone = Bone2D.new()
	bone.name = name
	bone.set_autocalculate_length_and_angle(false)
	bone.position = position
	bone.rest = Transform2D(0, position)
	_rig.add_child(bone)
	return bone


func _ready():
	OS.low_processor_usage_mode = false
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--shadow-capture="):
			_captures = argument.substr(17)
	_target = SubViewport.new()
	_target.size = Vector2(128, 96)
	_target.disable_3d = true
	_target.world_2d = World2D.new()
	_target.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(_target)
	_target.canvas_transform = Transform2D(Vector2(2, 0), Vector2(0, 1), Vector2(3, 2))
	_rig = Skeleton2D.new()
	_rig.name = "Rig"
	_rig.position = Vector2(8, 8)
	_target.add_child(_rig)
	_left = _bone("Left", Vector2(4, 0))
	_right = _bone("Right", Vector2(0, 4))
	_polygon = Polygon2D.new()
	_polygon.name = "WeightedPolygon"
	_polygon.position = Vector2(12, 8)
	_polygon.polygon = PackedVector2Array([Vector2(0, 0), Vector2(16, 0), Vector2(16, 16), Vector2(0, 16)])
	_source = _polygon.polygon
	_polygon.skeleton = NodePath("../Rig")
	_polygon.add_bone(NodePath("Left"), PackedFloat32Array([1, 1, 1, 1]))
	_polygon.add_bone(NodePath("Right"), PackedFloat32Array([0, 0, 0, 0]))
	_polygon.color = Color(1, 0, 0)
	_target.add_child(_polygon)
	var preview = TextureRect.new()
	preview.texture = _target.get_texture()
	preview.position = Vector2(8, 8)
	preview.size = Vector2(384, 288)
	preview.stretch_mode = TextureRect.STRETCH_SCALE
	add_child(preview)
	call_deferred("_run")


func _run():
	await _settle()
	_left.position = Vector2(12, 0)
	await _settle()
	var image = _image("bone_translation")
	if not _pixel(image, Vector2(48, 16), Color(1, 0, 0)) or not _pixel(image, Vector2(32, 16), Color(0.125, 0.25, 0.5)):
		return
	if RenderingServer.get_current_rendering_method() == "fixed_function":
		_polygon.antialiased = true
		await _settle()
		var aliased_fill = _image("antialias_requested")
		if not _check(aliased_fill != null and aliased_fill.get_data() == image.get_data(), "antialiasing request changed the mesh fill"):
			return
	_right.position = Vector2(16, 4)
	_polygon.set_bone_weights(0, PackedFloat32Array([0.5, 0.5, 0.5, 0.5]))
	_polygon.set_bone_weights(1, PackedFloat32Array([0.5, 0.5, 0.5, 0.5]))
	await _settle()
	image = _image("weighted")
	if not _pixel(image, Vector2(64, 16), Color(1, 0, 0)) or not _pixel(image, Vector2(48, 16), Color(0.125, 0.25, 0.5)):
		return
	# UVs/colors stay attached to the original polygon vertices while positions deform.
	var source_image = Image.create(2, 2, false, Image.FORMAT_RGBA8)
	source_image.set_pixel(0, 0, Color(1, 0, 0))
	source_image.set_pixel(1, 0, Color(0, 1, 0))
	source_image.set_pixel(0, 1, Color(0, 0, 1))
	source_image.set_pixel(1, 1, Color(1, 1, 1))
	var texture = ImageTexture.create_from_image(source_image)
	_polygon.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	_polygon.texture = texture
	_polygon.uv = PackedVector2Array([Vector2(0, 0), Vector2(2, 0), Vector2(2, 2), Vector2(0, 2)])
	_polygon.color = Color(1, 1, 1)
	await _settle()
	image = _image("textured")
	if not _pixel(image, Vector2(55, 13), Color(1, 0, 0)) or not _pixel(image, Vector2(79, 13), Color(0, 1, 0)) or not _pixel(image, Vector2(55, 23), Color(0, 0, 1)):
		return
	_polygon.texture = null
	_polygon.color = Color(1, 0, 0)
	# GLES collapses zero-weight vertices; Fixed Function deliberately keeps bind points.
	if RenderingServer.get_current_rendering_method() == "fixed_function":
		_polygon.set_bone_weights(0, PackedFloat32Array([0, 0, 0, 0]))
		_polygon.set_bone_weights(1, PackedFloat32Array([0, 0, 0, 0]))
		await _settle()
		image = _image("unweighted")
		if not _pixel(image, Vector2(32, 16), Color(1, 0, 0)) or not _pixel(image, Vector2(64, 16), Color(0.125, 0.25, 0.5)):
			return
	_polygon.set_bone_weights(0, PackedFloat32Array([1, 1, 1, 1]))
	_polygon.set_bone_weights(1, PackedFloat32Array([0, 0, 0, 0]))
	# The bind shape is outside the viewport. Updated skeleton bounds must keep it visible.
	_polygon.position = Vector2(-100, 8)
	_left.position = Vector2(124, 0)
	await _settle()
	image = _image("posed_bounds")
	if not _pixel(image, Vector2(48, 16), Color(1, 0, 0)):
		return
	_target.render_target_update_mode = SubViewport.UPDATE_ONCE
	await _settle()
	_left.position = Vector2(132, 0)
	await _settle()
	image = _image("frozen")
	if not _pixel(image, Vector2(48, 16), Color(1, 0, 0)):
		return
	_target.render_target_update_mode = SubViewport.UPDATE_ONCE
	await _settle()
	image = _image("explicit_update")
	if not _pixel(image, Vector2(48, 16), Color(0.125, 0.25, 0.5)) or not _pixel(image, Vector2(64, 16), Color(1, 0, 0)):
		return
	_target.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	_target.size = Vector2(160, 120)
	await _settle()
	image = _image("resized")
	if not _check(image.get_size() == Vector2i(160, 120), "wrong resized logical dimensions") or not _pixel(image, Vector2(64, 16), Color(1, 0, 0)):
		return
	_target.size = Vector2(128, 96)
	_polygon.position = Vector2(12, 8)
	_polygon.skeleton = NodePath("")
	await _settle()
	image = _image("detached")
	if not _pixel(image, Vector2(32, 16), Color(1, 0, 0)):
		return
	if not _check(_polygon.polygon == _source, "CPU polygon points changed"):
		return
	# Exercise rig teardown while its polygon remains in the canvas.
	if RenderingServer.get_current_rendering_method() == "fixed_function":
		await _rig_lifetime()
		if _failed:
			return
	print("CANVAS SKINNING PASS: real Skeleton2D/Polygon2D weights, textures, camera transforms, posed culling, frozen/explicit updates, resize, detachment and immutable CPU points.")
	get_tree().quit()


func _rig_lifetime():
	_polygon.skeleton = NodePath("../Rig")
	_left.position = Vector2(12, 0)
	await _settle()
	var image = _image("rig_alive")
	if not _pixel(image, Vector2(48, 16), Color(1, 0, 0)):
		return
	_rig.queue_free()
	await _settle()
	image = _image("rig_freed")
	_pixel(image, Vector2(32, 16), Color(1, 0, 0))
