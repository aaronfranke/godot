extends Control

const WINDOW_SIZE = Vector2(640, 400)
const LAST_FRAME = 30

var _frames = 0
var _failed = false
var _presses = 0
var _button
var _check_box
var _font_label
var _status
var _dynamic_font
var _font_rid
var _glyph_index
var _atlas
var _atlas_image
var _atlas_data
var _updated_atlas_data


func _check(p_condition, p_message):
	if not p_condition:
		_failed = true
		printerr("GUI FAIL: ", p_message)
		get_tree().quit(1)
	return p_condition


func _label(p_parent, p_text, p_position):
	var label = Label.new()
	label.text = p_text
	label.position = p_position
	p_parent.add_child(label)
	return label


func _accept(p_pressed):
	var event = InputEventAction.new()
	event.action = "ui_accept"
	event.pressed = p_pressed
	Input.parse_input_event(event)


func _on_pressed():
	_presses += 1
	_status.text = "Keyboard activation: PASS"


func _check_atlas(p_previous_data):
	var text_server = TextServerManager.get_primary_interface()
	if not _check(text_server.font_get_glyph_texture_rid(_font_rid, Vector2i(20, 0), _glyph_index) == _atlas, "Font atlas lost its logical RID"):
		return false
	var image = RenderingServer.texture_2d_get(_atlas)
	if not _check(image != null and image.get_data() != p_previous_data, "New glyphs did not update the font cache"):
		return false
	if not _check(_atlas_image.get_data() == _atlas_data, "An atlas update changed an earlier CPU snapshot"):
		return false
	_updated_atlas_data = image.get_data()
	return true


func _ready():
	var driver = RenderingServer.get_current_rendering_driver_name()
	print("GUI driver=", driver)
	DisplayServer.window_set_title("Fixed Function GUI - " + driver)
	DisplayServer.window_set_size(Vector2i(WINDOW_SIZE))
	size = WINDOW_SIZE
	get_viewport().disable_3d = true

	var panel = Panel.new()
	panel.position = Vector2(16, 16)
	panel.size = Vector2(288, 176)
	var style = StyleBoxFlat.new()
	style.bg_color = Color(0.12, 0.18, 0.25)
	style.border_color = Color(0.25, 0.65, 1)
	style.set_border_width_all(3)
	style.set_corner_radius_all(12)
	style.anti_aliasing = true
	panel.add_theme_stylebox_override("panel", style)
	add_child(panel)
	_label(panel, "Rounded flat panel / textured controls", Vector2(12, 12))

	_button = Button.new()
	_button.text = "Activate with ui_accept"
	_button.position = Vector2(12, 40)
	_button.size = Vector2(264, 32)
	_button.pressed.connect(_on_pressed)
	panel.add_child(_button)

	var edit = LineEdit.new()
	edit.text = "LineEdit / default font"
	edit.position = Vector2(12, 84)
	edit.size = Vector2(264, 28)
	panel.add_child(edit)

	_check_box = CheckBox.new()
	_check_box.text = "Default theme icons"
	_check_box.position = Vector2(12, 124)
	panel.add_child(_check_box)

	var clip = Control.new()
	clip.position = Vector2(328, 16)
	clip.size = Vector2(280, 176)
	clip.clip_contents = true
	add_child(clip)
	var red = ColorRect.new()
	red.color = Color(0.7, 0.1, 0.1)
	red.position = Vector2(-20, -20)
	red.size = Vector2(340, 230)
	clip.add_child(red)
	_label(clip, "Nested clipping", Vector2(12, 12))
	var inner = Control.new()
	inner.position = Vector2(32, 44)
	inner.size = Vector2(176, 88)
	inner.clip_contents = true
	clip.add_child(inner)
	var green = ColorRect.new()
	green.color = Color(0.1, 0.7, 0.2)
	green.position = Vector2(-16, -16)
	green.size = Vector2(224, 128)
	inner.add_child(green)
	_label(inner, "Clipped text stays inside this green box", Vector2(8, 32))

	var transformed = Panel.new()
	transformed.position = Vector2(28, 232)
	transformed.size = Vector2(268, 80)
	transformed.rotation_degrees = -5
	transformed.scale = Vector2(0.95, 1.1)
	add_child(transformed)
	_label(transformed, "Transformed texture stylebox", Vector2(12, 16))
	_label(transformed, "Geometry remains in Godot coordinates", Vector2(12, 40))

	_font_label = _label(self, "ABC", Vector2(328, 228))
	var font_path = ""
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--gui-font="):
			font_path = argument.substr(11)
	if not font_path.is_empty():
		_dynamic_font = FontFile.new()
		if not _check(_dynamic_font.load_dynamic_font(font_path) == OK, "Font file could not be loaded: " + font_path):
			return
		_dynamic_font.multichannel_signed_distance_field = false
		_font_label.add_theme_font_override("font", _dynamic_font)
		_font_label.add_theme_font_size_override("font_size", 20)
		_dynamic_font.render_range(0, Vector2i(20, 0), 65, 67)
		print("GUI FontFile=", font_path)
	else:
		_label(self, "Optional: --gui-font=<font.ttf>", Vector2(328, 276))
	_status = _label(self, "Waiting for keyboard activation", Vector2(16, 344))
	_button.grab_focus()


func _process(_delta):
	if _failed:
		return
	_frames += 1
	if _frames == 4 and _dynamic_font != null:
		_font_rid = _dynamic_font.get_rids()[0]
		var text_server = TextServerManager.get_primary_interface()
		_glyph_index = text_server.font_get_glyph_index(_font_rid, 20, 65, 0)
		_atlas = text_server.font_get_glyph_texture_rid(_font_rid, Vector2i(20, 0), _glyph_index)
		_atlas_image = RenderingServer.texture_2d_get(_atlas)
		if not _check(_atlas_image != null, "Font atlas has no CPU image"):
			return
		_atlas_data = _atlas_image.get_data()
	if _frames == 6:
		_font_label.text = "ABC / New glyphs 0123456789"
		if _dynamic_font != null:
			_dynamic_font.render_range(0, Vector2i(20, 0), 48, 57)
	if _frames == 8:
		_accept(true)
	if _frames == 9:
		_accept(false)
	if _frames == 10 and _dynamic_font != null:
		if not _check_atlas(_atlas_data):
			return
	if _frames == 12:
		if not _check(_presses == 1, "Focused button did not receive ui_accept"):
			return
		_check_box.button_pressed = true
		_font_label.text += "\nMore glyphs XYZ: Ω / Ж"
		if _dynamic_font != null:
			_dynamic_font.render_range(0, Vector2i(20, 0), 88, 90)
		DisplayServer.window_set_size(Vector2i(Vector2(672, 432)))
	if _frames == 18:
		DisplayServer.window_set_size(Vector2i(WINDOW_SIZE))
	if _frames == 22 and _dynamic_font != null:
		if not _check_atlas(_updated_atlas_data):
			return
		print("GUI atlas PASS: stable texture RID, new glyphs and isolated CPU snapshots")
	if _frames == LAST_FRAME:
		var items = get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_CANVAS, Viewport.RENDER_INFO_OBJECTS_IN_FRAME)
		var draws = get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_CANVAS, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME)
		if not _check(items > 0 and draws > 0, "The GUI viewport did not draw"):
			return
		if not _check(Vector2(DisplayServer.window_get_size()) == WINDOW_SIZE, "Window resize did not settle"):
			return
		print("GUI PASS frames=", _frames, " presses=", _presses, " items=", items, " draws=", draws)
		if not OS.get_cmdline_user_args().has("--smoke-hold"):
			get_tree().quit()
