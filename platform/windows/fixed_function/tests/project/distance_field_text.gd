extends Node2D

class Sample extends Node2D:
	var font
	var atlas

	func _draw():
		VisualServer.canvas_item_set_distance_field_mode(get_canvas_item(), true)
		draw_rect(Rect2(8, 4, 32, 16), Color.blue)
		draw_string(font, Vector2(10, 16), "AB", Color.red)
		draw_texture_rect(atlas, Rect2(44, 8, 8, 8), false)
		draw_rect(Rect2(66, 8, 12, 8), Color.green)


var _viewport
var _atlas
var _label
var _captures = ""
var _outline_font_path = ""
var _failed = false


func _check(condition, message):
	if not condition and not _failed:
		_failed = true
		printerr("DISTANCE FIELD TEXT FAIL: ", message)
		get_tree().quit(1)
	return condition


func _atlas_image(reverse):
	var values = [64, 115, 140, 204]
	if reverse:
		values.invert()
	var image = Image.new()
	image.create(8, 8, false, Image.FORMAT_RGBA8)
	for i in range(4):
		# Atlas RGB is deliberately green. Only the distance in alpha colors the text.
		image.fill_rect(Rect2(i * 2, 0, 2, 8), Color(0, 1, 0, values[i] / 255.0))
	return image


func _ready():
	for argument in OS.get_cmdline_args():
		if argument.begins_with("--shadow-capture="):
			_captures = argument.substr(17)
		elif argument.begins_with("--gui-font="):
			_outline_font_path = argument.substr(11)
	_viewport = Viewport.new()
	_viewport.size = Vector2(96, 64)
	_viewport.usage = Viewport.USAGE_2D
	_viewport.disable_3d = true
	_viewport.hdr = false
	_viewport.render_target_v_flip = true
	_viewport.world_2d = World2D.new()
	_viewport.render_target_update_mode = Viewport.UPDATE_ONCE
	add_child(_viewport)

	_atlas = ImageTexture.new()
	_atlas.create_from_image(_atlas_image(false), 0)
	var font = BitmapFont.new()
	font.set_distance_field_hint(true)
	font.height = 8
	font.ascent = 8
	font.add_texture(_atlas)
	font.add_texture(ImageTexture.new())
	font.add_char(65, 0, Rect2(0, 0, 8, 8), Vector2.ZERO, 8)
	# A page with no backing must omit only B, including in a batched string.
	font.add_char(66, 1, Rect2(0, 0, 8, 8), Vector2.ZERO, 8)
	var sample = Sample.new()
	sample.atlas = _atlas
	sample.font = font
	_viewport.add_child(sample)

	_label = Label.new()
	_label.rect_position = Vector2(8, 28)
	_label.rect_size = Vector2(32, 12)
	_label.add_font_override("font", font)
	_label.add_color_override("font_color", Color.red)
	var background = StyleBoxFlat.new()
	background.bg_color = Color.blue
	_label.add_stylebox_override("normal", background)
	_label.text = "AA"
	_viewport.add_child(_label)
	call_deferred("_run")


func _settle():
	for _i in range(4):
		yield(get_tree(), "idle_frame")


func _pixel(image, point, red, green, blue):
	var pixel = image.get_pixelv(point)
	return _check(abs(pixel.r - red) < 0.10 and abs(pixel.g - green) < 0.10 and abs(pixel.b - blue) < 0.10,
			"pixel %s expected %s, got %s" % [point, Color(red, green, blue), pixel])


func _verify(stage, reversed, binary):
	var image = _viewport.get_texture().get_data()
	if not _check(image != null and not image.is_empty() and image.get_size() == _viewport.size, "missing " + stage + " image"):
		return false
	if _captures != "" and not _check(image.save_png(_captures.plus_file(stage + ".png")) == OK, "save capture"):
		return false
	var coverage = [0.0, 0.22, 0.78, 1.0]
	if binary:
		coverage = [0.0, 0.0, 1.0, 1.0]
	if reversed:
		coverage.invert()
	image.lock()
	var ok = true
	for i in range(4):
		ok = ok and _pixel(image, Vector2(11 + i * 2, 11), coverage[i], 0, 1.0 - coverage[i])
		ok = ok and _pixel(image, Vector2(9 + i * 2, 31), coverage[i], 0, 1.0 - coverage[i])
	ok = ok and _pixel(image, Vector2(21, 11), 0, 0, 1) # Missing glyph page.
	ok = ok and _pixel(image, Vector2(34, 31), 0, 0, 1) # Label's ordinary style box.
	ok = ok and _pixel(image, Vector2(70, 11), 0, 1, 0) # Command after the missing glyph.
	var original_color = image.get_pixel(50 if not reversed else 44, 11)
	ok = ok and _check(original_color.g > 0.75 and original_color.r < 0.12, "regular textured rectangle keeps atlas RGB")
	image.unlock()
	return ok


func _check_dynamic_outline():
	if not _check(File.new().file_exists(_outline_font_path), "missing outline test font"):
		return false
	if not _check(ClassDB.can_instance("DynamicFontData") and ClassDB.can_instance("DynamicFont"), "missing DynamicFont support"):
		return false
	var data = ClassDB.instance("DynamicFontData")
	data.font_path = _outline_font_path
	var font = ClassDB.instance("DynamicFont")
	font.font_data = data
	font.size = 22
	font.outline_color = Color.green
	_label.add_font_override("font", font)
	_label.add_color_override("font_color", Color.white)
	_label.add_color_override("font_color_shadow", Color(0, 0, 0, 0))
	_label.add_color_override("font_outline_modulate", Color.white)
	_label.text = "Font"

	var plain_pixels
	var plain_draws = 0
	for width in [0, 2, 0]:
		font.outline_size = width
		_label.update()
		_viewport.render_target_update_mode = Viewport.UPDATE_ONCE
		yield(_settle(), "completed")
		var image = _viewport.get_texture().get_data()
		if not _check(image != null and not image.is_empty(), "missing DynamicFont image"):
			return false
		var ink = 0
		var outline = 0
		image.lock()
		for y in range(24, 64):
			for x in range(96):
				var pixel = image.get_pixel(x, y)
				if pixel.r > 0.8 and pixel.g > 0.8 and pixel.b > 0.8:
					ink += 1
				if pixel.r < 0.3 and pixel.g > 0.7 and pixel.b < 0.3:
					outline += 1
		image.unlock()
		if not _check(ink >= 20 and (outline >= 10 if width > 0 else outline == 0), "DynamicFont outline and fill pixels, width=%d, outline=%d, ink=%d" % [width, outline, ink]):
			return false
		var draws = _viewport.get_render_info(Viewport.RENDER_INFO_2D_DRAW_CALLS_IN_FRAME)
		if width == 0:
			if plain_pixels != null and not _check(image.get_data() == plain_pixels and draws == plain_draws, "disabling outline restores ordinary text"):
				return false
			plain_pixels = image.get_data()
			plain_draws = draws
		else:
			if not _check(draws == plain_draws + 4, "one extra draw per DynamicFont outline, got %d / %d" % [draws, plain_draws]):
				return false
			if _captures != "" and not _check(image.save_png(_captures.plus_file("dynamic_outline.png")) == OK, "save DynamicFont outline capture"):
				return false
			print("DYNAMIC FONT DRAWS outlined=", draws, " ordinary=", plain_draws)
	return true


func _run():
	var driver = OS.get_video_driver_name(OS.get_current_video_driver())
	var binary = driver == "D3D3 Software"
	yield(_settle(), "completed")
	if not _verify("first_update_once", false, binary):
		return
	if not _check(_atlas.get_data().get_data() == _atlas_image(false).get_data(), "unchanged original atlas"):
		return
	_atlas.set_data(_atlas_image(true))
	yield(_settle(), "completed")
	if not _verify("frozen", false, binary):
		return
	_viewport.render_target_update_mode = Viewport.UPDATE_ONCE
	yield(_settle(), "completed")
	if not _verify("updated_atlas", true, binary):
		return
	if not _check(_atlas.get_data().get_data() == _atlas_image(true).get_data(), "latest original atlas"):
		return
	var fill_draws = _viewport.get_render_info(Viewport.RENDER_INFO_2D_DRAW_CALLS_IN_FRAME)
	_atlas.set_data(_atlas_image(false))
	_label.add_color_override("font_color_shadow", Color.green)
	_label.add_constant_override("shadow_offset_x", 2)
	_label.add_constant_override("shadow_offset_y", 2)
	_label.add_constant_override("shadow_as_outline", 1)
	_viewport.render_target_update_mode = Viewport.UPDATE_ONCE
	yield(_settle(), "completed")
	var outlined = _viewport.get_texture().get_data()
	if not _check(outlined != null and not outlined.is_empty(), "missing outlined label image"):
		return
	if _captures != "" and not _check(outlined.save_png(_captures.plus_file("shadow_outline.png")) == OK, "save outline capture"):
		return
	outlined.lock()
	var ok = _pixel(outlined, Vector2(23, 31), 1, 0, 0) and _pixel(outlined, Vector2(25, 31), 0, 1, 0)
	ok = ok and _pixel(outlined, Vector2(25, 26), 0, 1, 0) and _pixel(outlined, Vector2(31, 31), 0, 0, 1)
	outlined.unlock()
	if not ok:
		return
	var outline_draws = _viewport.get_render_info(Viewport.RENDER_INFO_2D_DRAW_CALLS_IN_FRAME)
	if not _check(outline_draws == fill_draws + 8, "four shadow offsets per BitmapFont glyph, got %d / %d" % [outline_draws, fill_draws]):
		return
	print("BITMAP FONT DRAWS outlined=", outline_draws, " ordinary=", fill_draws)
	if _outline_font_path != "":
		var result = _check_dynamic_outline()
		if result is GDScriptFunctionState:
			result = yield(result, "completed")
		if not result:
			return
		print("DYNAMIC FONT OUTLINE PASS: rasterized outline, visible fill and restored ordinary text.")
	print("DISTANCE FIELD TEXT PASS: first one-shot frame, tinted fill, Label shadow outline, missing-page isolation and live atlas update.")
	get_tree().quit()
