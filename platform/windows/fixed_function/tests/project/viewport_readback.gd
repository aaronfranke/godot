extends Node2D

var _frames = 0
var _failed = false
var _source
var _scaled
var _source_rects = []
var _scaled_rects = []
var _snapshot
var _preview
var _colors = [Color.red, Color.green, Color.blue, Color.white]


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("VIEWPORT READBACK FAIL: ", message)
		get_tree().quit(1)
	return condition


func _layout(rectangles, size):
	for index in range(4):
		rectangles[index].rect_position = Vector2(size.x * 0.5 * (index % 2), size.y * 0.5 * (index / 2))
		rectangles[index].rect_size = size * 0.5


func _viewport(size, rectangles):
	var viewport = Viewport.new()
	viewport.size = size
	viewport.usage = Viewport.USAGE_2D
	viewport.disable_3d = true
	viewport.hdr = false
	viewport.render_target_update_mode = Viewport.UPDATE_ALWAYS
	add_child(viewport)
	for color in _colors:
		var rectangle = ColorRect.new()
		rectangle.color = color
		viewport.add_child(rectangle)
		rectangles.append(rectangle)
	_layout(rectangles, size)
	return viewport


func _image(image, size, colors, flipped):
	if not _check(image != null and not image.is_empty(), "readback returned no completed image"):
		return false
	if not _check(image.get_size() == size and not image.has_mipmaps(), "wrong logical dimensions or mipmaps"):
		return false
	image.lock()
	var correct = true
	for index in range(4):
		var x = int(size.x * (0.75 if index % 2 else 0.25))
		var y = int(size.y * (0.75 if index >= 2 else 0.25))
		if flipped:
			y = int(size.y) - 1 - y
		var actual = image.get_pixel(x, y)
		for channel in range(4):
			correct = correct and abs(actual[channel] - colors[index][channel]) < 0.04
	image.unlock()
	return _check(correct, "wrong orientation, channel conversion, alpha, or backing extent")


func _ready():
	OS.low_processor_usage_mode = false
	get_viewport().usage = Viewport.USAGE_2D
	get_viewport().hdr = false
	_source = _viewport(Vector2(50, 34), _source_rects)
	_scaled = _viewport(Vector2(400, 160), _scaled_rects)
	_preview = TextureRect.new()
	_preview.texture = _source.get_texture()
	_preview.rect_position = Vector2(8, 8)
	_preview.rect_size = Vector2(100, 68)
	add_child(_preview)


func _process(_delta):
	if _failed:
		return
	_frames += 1
	if _frames == 4:
		_snapshot = _source.get_texture().get_data()
		if not _image(_snapshot, _source.size, _colors, true):
			return
		if not _image(_scaled.get_texture().get_data(), _scaled.size, _colors, true):
			return
		# Mutating an image returned by readback cannot change the GPU target.
		var writable = _source.get_texture().get_data()
		writable.fill(Color.black)
		if not _image(_source.get_texture().get_data(), _source.size, _colors, true):
			return
		_source.render_target_update_mode = Viewport.UPDATE_DISABLED
		_source_rects[0].color = Color.cyan
	if _frames == 8:
		if not _image(_source.get_texture().get_data(), _source.size, _colors, true):
			return
		_source.render_target_v_flip = true
		_source.render_target_update_mode = Viewport.UPDATE_ONCE
	if _frames == 12:
		var changed = _colors.duplicate()
		changed[0] = Color.cyan
		if not _image(_source.get_texture().get_data(), _source.size, changed, false):
			return
		if not _image(_snapshot, Vector2(50, 34), _colors, true):
			return
		_scaled.size = Vector2(70, 42)
		_layout(_scaled_rects, _scaled.size)
	if _frames == 16:
		if not _image(_scaled.get_texture().get_data(), _scaled.size, _colors, true):
			return
		var root_image = get_viewport().get_texture().get_data()
		if not _check(root_image != null and root_image.get_size() == get_viewport().size, "presented root viewport has no image"):
			return
		_preview.texture = null
		_source.queue_free()
	if _frames == 20:
		if not _image(_snapshot, Vector2(50, 34), _colors, true):
			return
		var upright = _snapshot.duplicate()
		upright.flip_y()
		if not _check(upright.save_png("user://viewport_readback.png") == OK, "PNG screenshot export failed"):
			return
		var loaded = Image.new()
		if not _check(loaded.load("user://viewport_readback.png") == OK, "PNG screenshot reload failed"):
			return
		if not _image(loaded, Vector2(50, 34), _colors, false):
			return
		print("VIEWPORT READBACK PASS driver=", OS.get_video_driver_name(OS.get_current_video_driver()))
		get_tree().quit()
