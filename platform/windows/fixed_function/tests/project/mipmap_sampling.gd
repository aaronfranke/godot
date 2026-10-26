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
		yield(get_tree(), "idle_frame")


func _image(tail):
	var bytes = PoolByteArray()
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
		width = max(width / 2, 1)
		height = max(height / 2, 1)
	var image = Image.new()
	image.create_from_data(32, 8, true, Image.FORMAT_RGBA8, bytes)
	return image


func _add_rect(image, position, size, flags):
	var texture = ImageTexture.new()
	texture.create_from_image(image, flags)
	var rect = TextureRect.new()
	rect.texture = texture
	rect.expand = true
	rect.rect_position = position
	rect.rect_size = size
	_target.add_child(rect)
	_rects.append(rect)
	_textures.append(texture)


func _ready():
	OS.low_processor_usage_mode = false
	get_viewport().usage = Viewport.USAGE_2D
	get_viewport().hdr = false
	for argument in OS.get_cmdline_args():
		if argument.begins_with("--shadow-capture="):
			_captures = argument.substr(17)
	_target = Viewport.new()
	_target.size = Vector2(128, 96)
	_target.usage = Viewport.USAGE_2D
	_target.disable_3d = true
	_target.hdr = false
	_target.world_2d = World2D.new()
	_target.render_target_update_mode = Viewport.UPDATE_ALWAYS
	add_child(_target)
	var image = _image(Color(0, 0, 1))
	_original = image.get_data()
	_add_rect(image, Vector2(16, 16), Vector2(2, 2), Texture.FLAG_MIPMAPS)
	_add_rect(image, Vector2(24, 16), Vector2(2, 2), 0)
	_add_rect(image, Vector2(32, 16), Vector2(2, 2), Texture.FLAG_MIPMAPS | Texture.FLAG_FILTER)
	_add_rect(image, Vector2(16, 32), Vector2(64, 16), Texture.FLAG_MIPMAPS)
	var single = Image.new()
	single.create(32, 8, false, Image.FORMAT_RGBA8)
	single.fill(Color(1, 1, 0))
	_add_rect(single, Vector2(40, 16), Vector2(2, 2), Texture.FLAG_MIPMAPS)
	call_deferred("_run")


func _pixel(image, point, color):
	image.lock()
	var actual = image.get_pixelv(point)
	image.unlock()
	for c in range(4):
		if not _check(abs(actual[c] - color[c]) < 0.05,
				"pixel %s: %s expected %s" % [point, actual, color]):
			return false
	return true


func _capture(name):
	var image = _target.get_texture().get_data()
	if not _check(image != null and not image.is_empty(), "missing viewport image"):
		return null
	image.flip_y()
	if _captures != "":
		_check(image.save_png(_captures.plus_file(name + ".png")) == OK, "capture save failed")
	return image


func _run():
	yield(_settle(), "completed")
	var driver = OS.get_video_driver_name(OS.get_current_video_driver())
	var mipmapped = driver != "D3D3 Software"
	var mip_color = Color(0, 0, 1) if mipmapped else Color(1, 0, 0)
	var image = _capture("initial")
	if image == null:
		return
	if not _pixel(image, Vector2(17, 17), mip_color) or not _pixel(image, Vector2(25, 17), Color(1, 0, 0)) or not _pixel(image, Vector2(33, 17), mip_color) or not _pixel(image, Vector2(41, 17), Color(1, 1, 0)) or not _pixel(image, Vector2(32, 40), Color(1, 0, 0)):
		return
	_textures[0].flags = 0
	_textures[1].flags = Texture.FLAG_MIPMAPS
	yield(_settle(), "completed")
	image = _capture("flags")
	var flag_color = mip_color
	if driver.begins_with("GLES"):
		# GLES discards unrequested sublevels at upload, then regenerates them from
		# the red base when enabling mips later. Fixed Function retains authored levels.
		flag_color = Color(1, 0, 0)
	if image == null or not _pixel(image, Vector2(17, 17), Color(1, 0, 0)) or not _pixel(image, Vector2(25, 17), flag_color):
		return
	_textures[1].set_data(_image(Color(1, 1, 0)))
	yield(_settle(), "completed")
	image = _capture("replacement")
	var replacement = Color(1, 1, 0) if mipmapped else Color(1, 0, 0)
	if image == null or not _pixel(image, Vector2(25, 17), replacement):
		return
	_target.size = Vector2(160, 120)
	yield(_settle(), "completed")
	image = _capture("resized")
	if image == null or not _pixel(image, Vector2(25, 17), replacement):
		return
	if not _check(_textures[0].get_data().get_data() == _original, "canonical authored mip bytes changed"):
		return
	print("MIPMAP SAMPLING PASS: canvas minification, nearest/trilinear mips, opt-out, magnification, base-only texture, live flags/data, viewport resize and canonical CPU data.")
	get_tree().quit()
