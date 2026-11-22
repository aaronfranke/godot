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
	label.rect_position = p_position
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
	if not _check(_dynamic_font.get_char_texture(65) == _atlas, "Font atlas lost its logical RID"):
		return false
	var image = VisualServer.texture_get_data(_atlas)
	if not _check(image != null and image.get_data() != p_previous_data, "New glyphs did not update the atlas"):
		return false
	if not _check(_atlas_image.get_data() == _atlas_data, "An atlas update changed an earlier CPU snapshot"):
		return false
	_updated_atlas_data = image.get_data()
	return true


func _ready():
	var driver = OS.get_video_driver_name(OS.get_current_video_driver())
	print("GUI driver=", driver)
	OS.set_window_title("Fixed Function GUI - " + driver)
	OS.window_size = WINDOW_SIZE
	rect_size = WINDOW_SIZE
	get_viewport().usage = Viewport.USAGE_2D

	var panel = Panel.new()
	panel.rect_position = Vector2(16, 16)
	panel.rect_size = Vector2(288, 176)
	var style = StyleBoxFlat.new()
	style.bg_color = Color(0.12, 0.18, 0.25)
	style.border_color = Color(0.25, 0.65, 1)
	style.set_border_width_all(3)
	style.set_corner_radius_all(12)
	style.anti_aliasing = true
	panel.add_stylebox_override("panel", style)
	add_child(panel)
	_label(panel, "Rounded flat panel / textured controls", Vector2(12, 12))

	_button = Button.new()
	_button.text = "Activate with ui_accept"
	_button.rect_position = Vector2(12, 40)
	_button.rect_size = Vector2(264, 32)
	_button.connect("pressed", self, "_on_pressed")
	panel.add_child(_button)

	var edit = LineEdit.new()
	edit.text = "LineEdit / default bitmap font"
	edit.rect_position = Vector2(12, 84)
	edit.rect_size = Vector2(264, 28)
	panel.add_child(edit)

	_check_box = CheckBox.new()
	_check_box.text = "Default theme icons"
	_check_box.rect_position = Vector2(12, 124)
	panel.add_child(_check_box)

	var clip = Control.new()
	clip.rect_position = Vector2(328, 16)
	clip.rect_size = Vector2(280, 176)
	clip.rect_clip_content = true
	add_child(clip)
	var red = ColorRect.new()
	red.color = Color(0.7, 0.1, 0.1)
	red.rect_position = Vector2(-20, -20)
	red.rect_size = Vector2(340, 230)
	clip.add_child(red)
	_label(clip, "Nested clipping", Vector2(12, 12))
	var inner = Control.new()
	inner.rect_position = Vector2(32, 44)
	inner.rect_size = Vector2(176, 88)
	inner.rect_clip_content = true
	clip.add_child(inner)
	var green = ColorRect.new()
	green.color = Color(0.1, 0.7, 0.2)
	green.rect_position = Vector2(-16, -16)
	green.rect_size = Vector2(224, 128)
	inner.add_child(green)
	_label(inner, "Clipped text stays inside this green box", Vector2(8, 32))

	var transformed = Panel.new()
	transformed.rect_position = Vector2(28, 232)
	transformed.rect_size = Vector2(268, 80)
	transformed.rect_rotation = -5
	transformed.rect_scale = Vector2(0.95, 1.1)
	add_child(transformed)
	_label(transformed, "Transformed texture stylebox", Vector2(12, 16))
	_label(transformed, "Geometry remains in Godot coordinates", Vector2(12, 40))

	_font_label = _label(self, "ABC", Vector2(328, 228))
	var font_path = ""
	for argument in OS.get_cmdline_args():
		if argument.begins_with("--gui-font="):
			font_path = argument.substr(11)
	if not font_path.empty():
		if not _check(File.new().file_exists(font_path), "Font file does not exist: " + font_path):
			return
		if not _check(ClassDB.can_instance("DynamicFontData") and ClassDB.can_instance("DynamicFont"), "This build lacks DynamicFont support"):
			return
		var data = ClassDB.instance("DynamicFontData")
		data.font_path = font_path
		_dynamic_font = ClassDB.instance("DynamicFont")
		_dynamic_font.font_data = data
		_dynamic_font.size = 20
		_dynamic_font.use_filter = true
		_font_label.add_font_override("font", _dynamic_font)
		if not _check(_dynamic_font.get_string_size("ABC").x > 0, "Font has no usable Latin glyphs"):
			return
		print("GUI DynamicFont=", font_path)
	else:
		_label(self, "Optional: --gui-font=<font.ttf>", Vector2(328, 276))
	_status = _label(self, "Waiting for keyboard activation", Vector2(16, 344))
	_button.grab_focus()


func _process(_delta):
	if _failed:
		return
	_frames += 1
	if _frames == 4 and _dynamic_font != null:
		_atlas = _dynamic_font.get_char_texture(65)
		_atlas_image = VisualServer.texture_get_data(_atlas)
		if not _check(_atlas_image != null, "Font atlas has no CPU image"):
			return
		_atlas_data = _atlas_image.get_data()
	if _frames == 6:
		_font_label.text = "ABC / New glyphs 0123456789"
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
		_check_box.pressed = true
		_font_label.text += "\nMore glyphs XYZ: Ω / Ж"
		OS.window_size = Vector2(672, 432)
	if _frames == 18:
		OS.window_size = WINDOW_SIZE
	if _frames == 22 and _dynamic_font != null:
		if not _check_atlas(_updated_atlas_data):
			return
		print("GUI atlas PASS: stable RID, new glyphs and isolated CPU snapshots")
	if _frames == LAST_FRAME:
		var items = get_viewport().get_render_info(Viewport.RENDER_INFO_2D_ITEMS_IN_FRAME)
		var draws = get_viewport().get_render_info(Viewport.RENDER_INFO_2D_DRAW_CALLS_IN_FRAME)
		if not _check(items > 0 and draws > 0, "The GUI viewport did not draw"):
			return
		if not _check(OS.window_size == WINDOW_SIZE, "Window resize did not settle"):
			return
		print("GUI PASS frames=", _frames, " presses=", _presses, " items=", items, " draws=", draws)
		if not OS.get_cmdline_args().has("--smoke-hold"):
			get_tree().quit()
