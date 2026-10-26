extends Node2D

class Sample extends Node2D:
	var atlas: ImageTexture
	var draw_outlines = true

	func _draw():
		var item = get_canvas_item()
		var source = Rect2(0, 0, 8, 8)
		draw_rect(Rect2(6, 4, 110, 24), Color.BLUE)
		draw_texture_rect(atlas, Rect2(8, 20, 8, 8), false)
		RenderingServer.canvas_item_add_msdf_texture_rect_region(item, Rect2(8, 8, 8, 8), atlas.get_rid(), source, Color.RED, 0, 16)
		RenderingServer.canvas_item_add_lcd_texture_rect_region(item, Rect2(32, 8, 8, 8), atlas.get_rid(), source, Color.RED)
		RenderingServer.canvas_item_add_msdf_texture_rect_region(item, Rect2(44, 8, 8, 8), RID(), source, Color.RED, 0, 16)
		draw_texture_rect(atlas, Rect2(56, 8, 8, 8), false)
		draw_rect(Rect2(70, 8, 8, 8), Color.GREEN)
		if draw_outlines:
			RenderingServer.canvas_item_add_msdf_texture_rect_region(item, Rect2(20, 8, 8, 8), atlas.get_rid(), source, Color.RED, 8, 16)
			RenderingServer.canvas_item_add_msdf_texture_rect_region(item, Rect2(82, 8, 8, 8), atlas.get_rid(), source, Color.RED, 20, 16)
			RenderingServer.canvas_item_add_msdf_texture_rect_region(item, Rect2(94, 8, 8, 8), atlas.get_rid(), source, Color.RED, 18, 16)
		RenderingServer.canvas_item_add_msdf_texture_rect_region(item, Rect2(106, 8, 8, 8), atlas.get_rid(), source, Color.RED, 4, 1)


var _target: SubViewport
var _atlas: ImageTexture
var _sample: Sample
var _label: Label
var _captures = ""
var _failed = false


func _check(condition, message):
	if not condition and not _failed:
		_failed = true
		printerr("MSDF TEXT FAIL: ", message)
		get_tree().quit(1)
	return condition


func _image(reversed):
	var distances = [64, 115, 140, 204]
	if reversed:
		distances.reverse()
	var image = Image.create_empty(8, 8, false, Image.FORMAT_RGBA8)
	for i in range(4):
		# Alpha deliberately disagrees with median coverage in the first three bands.
		var alpha = 1.0
		if distances[i] == 115:
			alpha = 0.0
		elif distances[i] == 140:
			alpha = 0.2
		image.fill_rect(Rect2i(i * 2, 0, 2, 8), Color(0.1, distances[i] / 255.0, 0.9, alpha))
	return image


func _ready():
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--shadow-capture="):
			_captures = argument.substr(17)
	_target = SubViewport.new()
	_target.size = Vector2i(128, 64)
	_target.disable_3d = true
	_target.world_2d = World2D.new()
	_target.render_target_update_mode = SubViewport.UPDATE_ONCE
	add_child(_target)

	_atlas = ImageTexture.create_from_image(_image(false))
	_sample = Sample.new()
	_sample.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	_sample.atlas = _atlas
	_target.add_child(_sample)

	var font = SystemFont.new()
	font.font_names = PackedStringArray(["Arial"])
	font.multichannel_signed_distance_field = true
	_label = Label.new()
	_label.position = Vector2(6, 27)
	_label.add_theme_font_override("font", font)
	_label.add_theme_font_size_override("font_size", 22)
	_label.add_theme_color_override("font_outline_color", Color.GREEN)
	_label.add_theme_constant_override("outline_size", 6)
	_label.text = "MSDF"
	_target.add_child(_label)
	call_deferred("_run")


func _settle():
	for _i in range(4):
		await get_tree().process_frame


func _pixel(image, point, expected):
	var actual = image.get_pixelv(point)
	return _check(absf(actual.r - expected.r) < 0.1 and absf(actual.g - expected.g) < 0.1 and absf(actual.b - expected.b) < 0.1,
			"pixel %s expected %s, got %s" % [point, expected, actual])


func _verify(stage, reversed, binary):
	var image = _target.get_texture().get_image()
	if not _check(image != null and not image.is_empty() and image.get_size() == _target.size, "missing " + stage + " image"):
		return false
	if _captures != "" and not _check(image.save_png(_captures.path_join(stage + ".png")) == OK, "save capture"):
		return false
	var coverage = [0.0, 0.22, 0.78, 1.0]
	if binary:
		coverage = [0.0, 0.0, 1.0, 1.0]
	if reversed:
		coverage.reverse()
	for i in range(4):
		if not _pixel(image, Vector2i(9 + i * 2, 11), Color(coverage[i], 0, 1.0 - coverage[i])):
			return false
	var thin_outline = [0.0, 0.0, 0.0, 1.0]
	var thick_outline = [0.85, 0.0, 0.58, 1.0]
	if binary:
		thick_outline = [1.0, 0.0, 1.0, 1.0]
	if reversed:
		thin_outline.reverse()
		thick_outline.reverse()
	for i in range(4):
		if not _pixel(image, Vector2i(21 + i * 2, 11), Color(thin_outline[i], 0, 1.0 - thin_outline[i])):
			return false
		for x in [83, 95]:
			if not _pixel(image, Vector2i(x + i * 2, 11), Color(thick_outline[i], 0, 1.0 - thick_outline[i])):
				return false
	for x in [35, 47, 109]:
		if not _pixel(image, Vector2i(x, 11), Color.BLUE):
			return false
	if not _pixel(image, Vector2i(73, 11), Color.GREEN):
		return false
	if not _pixel(image, Vector2i(62 if not reversed else 56, 11), Color(0.1, 0.8, 0.9)):
		return false
	for i in range(8):
		if not _check(image.get_pixel(8 + i, 23).is_equal_approx(image.get_pixel(56 + i, 11)), "ordinary atlas draws match before and after glyphs"):
			return false
	var ink = 0
	var outline_ink = 0
	for y in range(28, 56):
		for x in range(6, 90):
			var pixel = image.get_pixel(x, y)
			if pixel.r > 0.7 and pixel.g > 0.7 and pixel.b > 0.7:
				ink += 1
			if pixel.g > 0.6 and pixel.r < 0.4 and pixel.b < 0.4:
				outline_ink += 1
	return _check(ink >= 20 and outline_ink >= 10, "real MSDF Label fill and outline are visible")


func _run():
	var binary = RenderingServer.get_current_rendering_driver_name() == "d3d3_software"
	await _settle()
	if not _verify("first_update_once", false, binary):
		return
	if not _check(_atlas.get_image().get_data() == _image(false).get_data(), "original atlas changed"):
		return
	_atlas.set_image(_image(true))
	await _settle()
	if not _verify("frozen", false, binary):
		return
	_target.render_target_update_mode = SubViewport.UPDATE_ONCE
	await _settle()
	if not _verify("replaced_atlas", true, binary):
		return
	_atlas.update(_image(false))
	_target.render_target_update_mode = SubViewport.UPDATE_ONCE
	await _settle()
	if not _verify("updated_atlas", false, binary):
		return
	var draws_with_outlines = _target.get_render_info(Viewport.RENDER_INFO_TYPE_CANVAS, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME)
	var fill_rect = Rect2i(8, 8, 8, 8)
	var fill_before = _target.get_texture().get_image().get_region(fill_rect).get_data()
	_sample.draw_outlines = false
	_sample.queue_redraw()
	_label.add_theme_constant_override("outline_size", 0)
	_target.render_target_update_mode = SubViewport.UPDATE_ONCE
	await _settle()
	var draws_without_outlines = _target.get_render_info(Viewport.RENDER_INFO_TYPE_CANVAS, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME)
	if not _check(draws_with_outlines - draws_without_outlines == 7, "expected one draw per outline, got %d / %d" % [draws_with_outlines, draws_without_outlines]):
		return
	if not _check(_target.get_texture().get_image().get_region(fill_rect).get_data() == fill_before, "ordinary fill changed"):
		return
	print("MSDF TEXT DRAWS outlined=", draws_with_outlines, " ordinary=", draws_without_outlines)
	print("MSDF TEXT PASS: fill and bounded outline widths, real outlined Label, local omission, frozen viewport and cache replacement.")
	get_tree().quit()
