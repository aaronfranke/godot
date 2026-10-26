extends Node2D

var _target: SubViewport
var _sprite: Sprite2D
var _canvas_texture: CanvasTexture
var _marker: ColorRect
var _failed = false


func _check(condition, message):
	if not condition and not _failed:
		_failed = true
		printerr("CANVAS EFFECT FALLBACK FAIL: ", message)
		get_tree().quit(1)
	return condition


func _texture(color, size):
	var image = Image.create_empty(size, size, false, Image.FORMAT_RGBA8)
	image.fill(color)
	return ImageTexture.create_from_image(image)


func _ready():
	_target = SubViewport.new()
	_target.size = Vector2i(96, 64)
	_target.disable_3d = true
	_target.world_2d = World2D.new()
	_target.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(_target)
	_canvas_texture = CanvasTexture.new()
	_canvas_texture.diffuse_texture = _texture(Color.GREEN, 16)
	_sprite = Sprite2D.new()
	_sprite.texture = _canvas_texture
	_sprite.centered = false
	_sprite.position = Vector2(8, 8)
	_target.add_child(_sprite)
	_marker = ColorRect.new()
	_marker.position = Vector2(48, 16)
	_marker.size = Vector2(20, 16)
	_marker.color = Color.BLUE
	_target.add_child(_marker)

	var preview = TextureRect.new()
	preview.texture = _target.get_texture()
	preview.position = Vector2(16, 16)
	add_child(preview)
	call_deferred("_run")


func _settle():
	for _i in range(4):
		await get_tree().process_frame


func _pixels(viewport):
	var image = viewport.get_texture().get_image()
	if not _check(image != null and not image.is_empty() and image.get_size() == viewport.size, "missing viewport image"):
		return PackedByteArray()
	return image.get_data()


func _run():
	await _settle()
	var base = _pixels(_target)
	var window_base = _pixels(get_viewport())
	if _failed or not _check(_target.get_texture().get_image().get_pixel(12, 12).g > 0.9, "diffuse sprite is visible"):
		return

	_canvas_texture.normal_texture = _texture(Color(1, 0.5, 0.5), 16)
	_canvas_texture.specular_texture = _texture(Color.WHITE, 16)
	var point = PointLight2D.new()
	point.texture = _texture(Color.WHITE, 128)
	point.position = Vector2(48, 32)
	point.shadow_enabled = true
	_target.add_child(point)
	var sun = DirectionalLight2D.new()
	sun.shadow_enabled = true
	_target.add_child(sun)
	var occluder = LightOccluder2D.new()
	occluder.occluder = OccluderPolygon2D.new()
	occluder.occluder.polygon = PackedVector2Array([Vector2(3, 3), Vector2(24, 3), Vector2(16, 22)])
	_target.add_child(occluder)
	var copy = BackBufferCopy.new()
	copy.copy_mode = BackBufferCopy.COPY_MODE_VIEWPORT
	_target.add_child(copy)
	RenderingServer.canvas_item_set_copy_to_backbuffer(_sprite.get_canvas_item(), true, Rect2(0, 0, 24, 24))
	await _settle()
	if not _check(_pixels(_target) == base and _pixels(get_viewport()) == window_base, "unsupported effects change the base output"):
		return

	_marker.color = Color.YELLOW
	await _settle()
	var live_image = _target.get_texture().get_image()
	if not _check(live_image != null and live_image.get_pixel(55, 20).is_equal_approx(Color.YELLOW), "canvas stays live"):
		return
	if not _check(_pixels(get_viewport()) != window_base, "window receives live viewport edits"):
		return
	_marker.color = Color.BLUE
	point.queue_free()
	sun.queue_free()
	occluder.queue_free()
	copy.queue_free()
	RenderingServer.canvas_item_set_copy_to_backbuffer(_sprite.get_canvas_item(), false, Rect2())
	await _settle()
	if not _check(_pixels(_target) == base and _pixels(get_viewport()) == window_base, "light and copy teardown"):
		return
	print("CANVAS EFFECT FALLBACK PASS: point and directional lights, shadow hints, normal/specular maps and copy requests preserve live canvas output.")
	get_tree().quit()
