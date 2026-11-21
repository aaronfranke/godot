extends Node2D

var _target
var _rects = []
var _textures = []
var _original
var _failed = false
var _captures = ""


func _check(condition, message):
	if not condition and not _failed:
		_failed = true
		printerr("MIPMAP SAMPLING FAIL: ", message)
		get_tree().quit(1)
	return condition


func _settle():
	for _i in range(4):
		await get_tree().process_frame


func _image(tail):
	var bytes = PackedByteArray()
	var width = 32
	var height = 8
	while true:
		var color = Color(0, 1, 0)
		if width == 32:
			color = Color(1, 0, 0)
		# Resizing a POT-backed viewport changes its native pixel footprint and LOD.
		# Keep a coarse-color plateau; this tests filtering, not one exact mip index.
		elif width <= 4:
			color = tail
		var offset = bytes.size()
		bytes.resize(offset + width * height * 4)
		for i in range(width * height):
			for c in range(4):
				bytes[offset + i * 4 + c] = int(color[c] * 255)
		if width == 1 and height == 1:
			break
		width = max(width >> 1, 1)
		height = max(height >> 1, 1)
	return Image.create_from_data(32, 8, true, Image.FORMAT_RGBA8, bytes)


func _add_rect(image, position, size, filter):
	var texture = ImageTexture.create_from_image(image)
	var rect = TextureRect.new()
	rect.texture = texture
	rect.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	rect.position = position
	rect.size = size
	rect.texture_filter = filter
	_target.add_child(rect)
	_rects.append(rect)
	_textures.append(texture)


func _ready():
	OS.low_processor_usage_mode = false
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--shadow-capture="):
			_captures = argument.substr(17)
	_target = SubViewport.new()
	_target.size = Vector2i(128, 96)
	_target.disable_3d = true
	_target.world_2d = World2D.new()
	_target.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(_target)
	var image = _image(Color(0, 0, 1))
	_original = image.get_data()
	_add_rect(image, Vector2(16, 16), Vector2(2, 2), CanvasItem.TEXTURE_FILTER_NEAREST_WITH_MIPMAPS)
	_add_rect(image, Vector2(24, 16), Vector2(2, 2), CanvasItem.TEXTURE_FILTER_NEAREST)
	_add_rect(image, Vector2(32, 16), Vector2(2, 2), CanvasItem.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS)
	_add_rect(image, Vector2(16, 32), Vector2(64, 16), CanvasItem.TEXTURE_FILTER_NEAREST_WITH_MIPMAPS)
	var single = Image.create_empty(32, 8, false, Image.FORMAT_RGBA8)
	single.fill(Color(1, 1, 0))
	_add_rect(single, Vector2(40, 16), Vector2(2, 2), CanvasItem.TEXTURE_FILTER_NEAREST_WITH_MIPMAPS)
	call_deferred("_run")


func _pixel(image, point, color):
	var actual = image.get_pixelv(Vector2i(point))
	for c in range(4):
		if not _check(abs(actual[c] - color[c]) < 0.05,
				"pixel %s: %s expected %s" % [point, actual, color]):
			return false
	return true


func _capture(name):
	var image = _target.get_texture().get_image()
	if not _check(image != null and not image.is_empty(), "missing viewport image"):
		return null
	if _captures != "":
		_check(image.save_png(_captures.path_join(name + ".png")) == OK, "capture save failed")
	return image


func _run():
	await _settle()
	var driver = RenderingServer.get_current_rendering_driver_name()
	var mipmapped = driver != "d3d3_software"
	var mip_color = Color(0, 0, 1) if mipmapped else Color(1, 0, 0)
	var image = _capture("initial")
	if image == null:
		return
	if not _pixel(image, Vector2(17, 17), mip_color) or not _pixel(image, Vector2(25, 17), Color(1, 0, 0)) or not _pixel(image, Vector2(33, 17), mip_color) or not _pixel(image, Vector2(41, 17), Color(1, 1, 0)) or not _pixel(image, Vector2(32, 40), Color(1, 0, 0)):
		return
	_rects[0].texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	_rects[1].texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST_WITH_MIPMAPS
	await _settle()
	image = _capture("flags")
	if image == null or not _pixel(image, Vector2(17, 17), Color(1, 0, 0)) or not _pixel(image, Vector2(25, 17), mip_color):
		return
	_textures[1].update(_image(Color(1, 1, 0)))
	await _settle()
	image = _capture("replacement")
	var replacement = Color(1, 1, 0) if mipmapped else Color(1, 0, 0)
	if image == null or not _pixel(image, Vector2(25, 17), replacement):
		return
	_target.size = Vector2i(160, 120)
	await _settle()
	image = _capture("resized")
	if image == null or not _pixel(image, Vector2(25, 17), replacement):
		return
	if RenderingServer.get_current_rendering_method() == "fixed_function":
		# Fixed Function returns its canonical CPU image. Compatibility reads native storage.
		if not _check(_textures[0].get_image().get_data() == _original, "canonical authored mip bytes changed"):
			return
	print("MIPMAP SAMPLING PASS: canvas minification, nearest/trilinear mips, opt-out, magnification, base-only texture, live filter/data and viewport resize.")
	get_tree().quit()
