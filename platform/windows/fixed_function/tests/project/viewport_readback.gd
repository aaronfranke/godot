extends Node2D

var _frames = 0
var _failed = false
var _source
var _scaled
var _source_rects = []
var _scaled_rects = []
var _snapshot
var _preview
var _colors = [Color.RED, Color.GREEN, Color.BLUE, Color.WHITE]


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("VIEWPORT READBACK FAIL: ", message)
		get_tree().quit(1)
	return condition


func _layout(rectangles, size):
	for index in range(4):
		rectangles[index].position = Vector2(size.x * 0.5 * (index % 2), size.y * 0.5 * (index / 2))
		rectangles[index].size = size * 0.5


func _viewport(size, rectangles):
	var viewport = SubViewport.new()
	viewport.size = size
	viewport.disable_3d = true
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(viewport)
	for color in _colors:
		var rectangle = ColorRect.new()
		rectangle.color = color
		viewport.add_child(rectangle)
		rectangles.append(rectangle)
	_layout(rectangles, size)
	return viewport


func _image(image, size, colors):
	if not _check(image != null and not image.is_empty(), "readback returned no completed image"):
		return false
	if not _check(image.get_size() == size and not image.has_mipmaps(), "wrong logical dimensions or mipmaps"):
		return false
	var correct = true
	for index in range(4):
		var x = int(size.x * (0.75 if index % 2 else 0.25))
		var y = int(size.y * (0.75 if index >= 2 else 0.25))
		var actual = image.get_pixel(x, y)
		for channel in range(4):
			correct = correct and abs(actual[channel] - colors[index][channel]) < 0.04
	return _check(correct, "wrong orientation, channel conversion, alpha, or backing extent")


func _ready():
	OS.low_processor_usage_mode = false
	_source = _viewport(Vector2i(50, 34), _source_rects)
	_scaled = _viewport(Vector2i(400, 160), _scaled_rects)
	_preview = TextureRect.new()
	_preview.texture = _source.get_texture()
	_preview.position = Vector2(8, 8)
	_preview.size = Vector2(100, 68)
	add_child(_preview)


func _process(_delta):
	if _failed:
		return
	_frames += 1
	if _frames == 4:
		_snapshot = _source.get_texture().get_image()
		if not _image(_snapshot, _source.size, _colors):
			return
		if not _image(_scaled.get_texture().get_image(), _scaled.size, _colors):
			return
		# Mutating an image returned by readback cannot change the GPU target.
		var writable = _source.get_texture().get_image()
		writable.fill(Color.BLACK)
		if not _image(_source.get_texture().get_image(), _source.size, _colors):
			return
		_source.render_target_update_mode = SubViewport.UPDATE_DISABLED
		_source_rects[0].color = Color.CYAN
	if _frames == 8:
		if not _image(_source.get_texture().get_image(), _source.size, _colors):
			return
		_source.render_target_update_mode = SubViewport.UPDATE_ONCE
	if _frames == 12:
		var changed = _colors.duplicate()
		changed[0] = Color.CYAN
		if not _image(_source.get_texture().get_image(), _source.size, changed):
			return
		if not _image(_snapshot, Vector2i(50, 34), _colors):
			return
		_scaled.size = Vector2i(70, 42)
		_layout(_scaled_rects, _scaled.size)
	if _frames == 16:
		if not _image(_scaled.get_texture().get_image(), _scaled.size, _colors):
			return
		var root_image = get_viewport().get_texture().get_image()
		if not _check(root_image != null and root_image.get_size() == get_viewport().size, "presented root viewport has no image"):
			return
		_preview.texture = null
		_source.queue_free()
	if _frames == 20:
		if not _image(_snapshot, Vector2i(50, 34), _colors):
			return
		var upright = _snapshot.duplicate()
		if not _check(upright.save_png("user://viewport_readback.png") == OK, "PNG screenshot export failed"):
			return
		var loaded = Image.new()
		if not _check(loaded.load("user://viewport_readback.png") == OK, "PNG screenshot reload failed"):
			return
		if not _image(loaded, Vector2i(50, 34), _colors):
			return
		print("VIEWPORT READBACK PASS driver=", RenderingServer.get_current_rendering_driver_name())
		get_tree().quit()
